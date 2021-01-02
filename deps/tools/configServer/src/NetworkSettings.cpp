// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#include "NetworkSettings.h"

#include <unistd.h>

#include <string>
#include <vector>

#include <wpi/FileSystem.h>
#include <wpi/MathExtras.h>
#include <wpi/SmallString.h>
#include <wpi/json.h>
#include <wpi/raw_istream.h>
#include <wpi/raw_ostream.h>
#include <wpi/uv/Process.h>
#include <wpi/uv/util.h>

namespace uv = wpi::uv;

extern bool romi;

#define GEN_MARKER "###### BELOW THIS LINE EDITED BY RPICONFIGSERVER ######"

/*
 Format of generated portion for static:

   interface eth0
   static ip_address=<networkAddress>/<networkMask as CIDR>
   static routers=<networkGateway>
   static domain_name_servers=<networkDNS>

 For static fallback:

   profile static_eth0
   static ip_address=<networkAddress>/<networkMask as CIDR>
   static routers=<networkGateway>
   static domain_name_servers=<networkDNS>
   interface eth0
   fallback static_eth0
 */

wpi::StringRef CidrToNetmask(unsigned int cidr,
                             wpi::SmallVectorImpl<char>& buf) {
  in_addr addr = { htonl(wpi::maskLeadingOnes<uint32_t>(cidr)) };
  wpi::uv::AddrToName(addr, &buf);
  return wpi::StringRef(buf.data(), buf.size());
}

bool NetmaskToCidr(wpi::StringRef netmask, unsigned int* cidr) {
  in_addr addr;
  if (wpi::uv::NameToAddr(netmask, &addr) != 0) return false;
  uint32_t hostAddr = ntohl(addr.s_addr);
  auto leadingOnes = wpi::countLeadingOnes(hostAddr);
  auto trailingZeros = wpi::countTrailingZeros(hostAddr);
  if (leadingOnes + trailingZeros != 32) return false;
  *cidr = leadingOnes;
  return true;
}

std::shared_ptr<NetworkSettings> NetworkSettings::GetInstance() {
  static auto inst = std::make_shared<NetworkSettings>(private_init{});
  return inst;
}

static std::string BuildDhcpcdSetting(
    wpi::StringRef iface, NetworkSettings::Mode mode, wpi::StringRef address,
    wpi::StringRef mask, wpi::StringRef gateway, wpi::StringRef dns,
    std::function<void(wpi::StringRef)> onFail) {
  // validate and sanitize inputs
  wpi::SmallString<32> addressOut;
  unsigned int cidr;
  wpi::SmallString<32> gatewayOut;
  wpi::SmallString<128> dnsOut;

  // address
  in_addr addressAddr;
  if (wpi::uv::NameToAddr(address, &addressAddr) != 0) {
    wpi::SmallString<128> err;
    err += "invalid address '";
    err += address;
    err += "'";
    onFail(err);
    return {};
  }
  wpi::uv::AddrToName(addressAddr, &addressOut);

  // mask
  if (!NetmaskToCidr(mask, &cidr)) {
    wpi::SmallString<128> err;
    err += "invalid netmask '";
    err += mask;
    err += "'";
    onFail(err);
    return {};
  }

  // gateway (may be blank)
  in_addr gatewayAddr;
  if (wpi::uv::NameToAddr(gateway, &gatewayAddr) == 0)
    wpi::uv::AddrToName(gatewayAddr, &gatewayOut);

  // dns
  wpi::SmallVector<wpi::StringRef, 4> dnsStrs;
  wpi::SmallString<32> oneDnsOut;
  bool first = true;
  dns.split(dnsStrs, ' ', -1, false);
  for (auto dnsStr : dnsStrs) {
    in_addr dnsAddr;
    if (wpi::uv::NameToAddr(dnsStr, &dnsAddr) != 0) {
      wpi::SmallString<128> err;
      err += "invalid DNS address '";
      err += dnsStr;
      err += "'";
      onFail(err);
      return {};
    }
    wpi::uv::AddrToName(dnsAddr, &oneDnsOut);
    if (!first) dnsOut += ' ';
    first = false;
    dnsOut += oneDnsOut;
  }

  std::string rv;
  wpi::raw_string_ostream os{rv};
  // write generated config
  switch (mode) {
    case NetworkSettings::kDhcp:
      os << '\n';  // nothing required
      break;
    case NetworkSettings::kStatic:
      os << "interface " << iface << '\n';
      os << "static ip_address=" << addressOut << '/' << cidr << '\n';
      if (!gatewayOut.empty()) os << "static routers=" << gatewayOut << '\n';
      if (!dnsOut.empty())
        os << "static domain_name_servers=" << dnsOut << '\n';
      break;
    case NetworkSettings::kDhcpStatic:
      os << "profile static_" << iface << '\n';
      os << "static ip_address=" << addressOut << '/' << cidr << '\n';
      if (!gatewayOut.empty()) os << "static routers=" << gatewayOut << '\n';
      if (!dnsOut.empty())
        os << "static domain_name_servers=" << dnsOut << '\n';
      os << "interface " << iface << '\n';
      os << "fallback static_" << iface << '\n';
      break;
  }
  return os.str();
}

void NetworkSettings::Set(Mode mode, wpi::StringRef address,
                          wpi::StringRef mask, wpi::StringRef gateway,
                          wpi::StringRef dns, WifiMode wifiAPMode,
                          int wifiChannel, wpi::StringRef wifiSsid,
                          wpi::StringRef wifiWpa2, Mode wifiMode,
                          wpi::StringRef wifiAddress, wpi::StringRef wifiMask,
                          wpi::StringRef wifiGateway, wpi::StringRef wifiDns,
                          std::function<void(wpi::StringRef)> onFail) {
  // sanity check access point settings
  if (wifiAPMode == kAccessPoint) {
    if (wifiSsid.empty()) {
      onFail("must set SSID for access point");
      return;
    }
    if (wifiMode != kStatic || wifiAddress.empty() || wifiMask.empty()) {
      onFail("must set static address and netmask for access point");
      return;
    }
    if (!wifiWpa2.empty() && (wifiWpa2.size() < 8 || wifiWpa2.size() > 63)) {
      onFail("passphrase must be between 8 and 63 characters");
      return;
    }
  }

  //
  // dhcpcd configuration
  //
  {
    std::string eth0 =
        BuildDhcpcdSetting("eth0", mode, address, mask, gateway, dns, onFail);
    if (eth0.empty()) return;
    std::string wlan0;
    if (romi) {
      wlan0 = BuildDhcpcdSetting("wlan0", wifiMode, wifiAddress, wifiMask,
                                 wifiGateway, wifiDns, onFail);
      if (wlan0.empty()) return;
    }

    // read file (up to but not including the marker)
    std::vector<std::string> lines;
    std::error_code ec;
    {
      wpi::raw_fd_istream is(DHCPCD_CONF, ec);
      if (ec) {
        onFail("could not read " DHCPCD_CONF);
        return;
      }

      wpi::SmallString<256> lineBuf;
      while (!is.has_error()) {
        wpi::StringRef line = is.getline(lineBuf, 256).trim();
        if (line == GEN_MARKER) break;
        lines.emplace_back(line);
      }
    }

    // write file
    {
      // write original lines
      wpi::raw_fd_ostream os(DHCPCD_CONF, ec, wpi::sys::fs::F_Text);
      if (ec) {
        onFail("could not write " DHCPCD_CONF);
        return;
      }
      for (auto&& line : lines) os << line << '\n';

      // write marker
      os << GEN_MARKER << '\n';

      // write generated config
      os << eth0;
      os << '\n';
      os << wlan0;
      if (wifiAPMode == kAccessPoint) {
        os << "nohook wpa_supplicant\n";
      }
    }
  }

  //
  // dnsmasq configuration
  //
  if (wifiAPMode == kAccessPoint) {
    // take the 3 parts of IP range
    in_addr addressAddr;
    if (wpi::uv::NameToAddr(wifiAddress, &addressAddr) != 0) {
      wpi::SmallString<128> err;
      err += "invalid address '";
      err += wifiAddress;
      err += "'";
      onFail(err);
      return;
    }
    wpi::SmallString<32> addressOut;
    wpi::uv::AddrToName(addressAddr, &addressOut);
    wpi::StringRef addr3part = wpi::StringRef{addressOut}.rsplit('.').first;

    std::error_code ec;
    wpi::raw_fd_ostream os(DNSMASQ_CONF, ec, wpi::sys::fs::F_Text);
    if (ec) {
      onFail("could not write " DNSMASQ_CONF);
      return;
    }
    os << "interface=wlan0\n";
    os << "dhcp-range=" << addr3part << ".100," << addr3part << ".200,"
       << wifiMask << ",5m\n";
  } else {
    // remove dnsmasq config file
    unlink(DNSMASQ_CONF);
  }

  //
  // hostapd configuration
  //
  if (wifiAPMode == kAccessPoint) {
    std::error_code ec;
    wpi::raw_fd_ostream os(HOSTAPD_CONF, ec, wpi::sys::fs::F_Text);
    if (ec) {
      onFail("could not write " HOSTAPD_CONF);
      return;
    }
    os << "interface=wlan0\n";
    os << "hw_mode=g\n";
    os << "channel=" << wifiChannel << '\n';
    os << "wmm_enabled=0\n";
    os << "macaddr_acl=0\n";
    os << "auth_algs=1\n";
    os << "ignore_broadcast_ssid=0\n";
    os << "ssid=" << wifiSsid << '\n';
    if (!wifiWpa2.empty()) {
      os << "wpa=2\n";
      os << "wpa_key_mgmt=WPA-PSK\n";
      os << "wpa_pairwise=TKIP\n";
      os << "rsn_pairwise=CCMP\n";
      os << "wpa_passphrase=" << wifiWpa2 << '\n';
    }
  } else {
    // remove hostapd config file
    unlink(HOSTAPD_CONF);
  }

  //
  // wpa supplicant configuration
  //
  {
    // read file (up to but not including the marker)
    std::vector<std::string> lines;
    std::error_code ec;
    {
      wpi::raw_fd_istream is(WPA_SUPPLICANT_CONF, ec);
      if (ec) {
        onFail("could not read " WPA_SUPPLICANT_CONF);
        return;
      }

      wpi::SmallString<256> lineBuf;
      while (!is.has_error()) {
        wpi::StringRef line = is.getline(lineBuf, 256).trim();
        if (line == GEN_MARKER) break;
        lines.emplace_back(line);
      }
    }

    // write file
    {
      // write original lines
      wpi::raw_fd_ostream os(WPA_SUPPLICANT_CONF, ec, wpi::sys::fs::F_Text);
      if (ec) {
        onFail("could not write " WPA_SUPPLICANT_CONF);
        return;
      }
      for (auto&& line : lines) os << line << '\n';

      // write marker
      os << GEN_MARKER << '\n';

      // write generated config
      os << "network={\n";
      os << "    ssid=\"" << wifiSsid << "\"\n";
      if (!wifiWpa2.empty()) {
        os << "    psk=\"" << wifiWpa2 << "\"\n";
      } else {
        os << "    key_mgmt=NONE\n";
      }
      os << "}\n";
    }
  }

  // start or stop hostapd
  if (wifiAPMode == kAccessPoint) {
    if (auto proc =
            uv::Process::Spawn(m_loop, "/usr/bin/systemctl",
                               "/usr/bin/systemctl", "unmask", "hostapd")) {
      proc->exited.connect([p = proc.get(), loop = m_loop](int64_t, int) {
        p->Close();
        if (auto proc = uv::Process::Spawn(loop, "/usr/bin/systemctl",
                                           "/usr/bin/systemctl", "restart",
                                           "hostapd")) {
          proc->exited.connect([p = proc.get()](int64_t, int) { p->Close(); });
        }
      });
    }
  } else {
    if (auto proc = uv::Process::Spawn(m_loop, "/usr/bin/systemctl",
                                       "/usr/bin/systemctl", "--now", "mask",
                                       "hostapd")) {
      proc->exited.connect([p = proc.get()](int64_t, int) { p->Close(); });
    }
  }

  // tell dnsmasq to restart
  if (auto proc =
          uv::Process::Spawn(m_loop, "/usr/sbin/service", "/usr/sbin/service",
                             "dnsmasq", "restart")) {
    proc->exited.connect([p = proc.get()](int64_t, int) { p->Close(); });
  }

  // tell dhcpcd to reload config
  if (auto proc =
          uv::Process::Spawn(m_loop, "/sbin/dhcpcd", "/sbin/dhcpcd", "-n")) {
    proc->exited.connect([p = proc.get()](int64_t, int) { p->Close(); });
  }

  // tell wpa to reconfigure
  if (auto proc = uv::Process::Spawn(m_loop, "/sbin/wpa_cli", "/sbin/wpa_cli",
                                     "-i", "wlan0", "reconfigure")) {
    proc->exited.connect([p = proc.get()](int64_t, int) { p->Close(); });
  }

  UpdateStatus();
}

void NetworkSettings::UpdateStatus() { status(GetStatusJson()); }

wpi::json NetworkSettings::GetStatusJson() {
  wpi::json j = {{"type", "networkSettings"},
                 {"networkApproach", "dhcp"},
                 {"wifiNetworkApproach", "dhcp"}};

  //
  // dhcpcd configuration
  //
  {
    std::error_code ec;
    wpi::raw_fd_istream is(DHCPCD_CONF, ec);
    if (ec) {
      wpi::errs() << "could not read " DHCPCD_CONF "\n";
      return wpi::json();
    }

    wpi::SmallString<256> lineBuf;
    bool foundMarker = false;
    bool wlan = false;
    while (!is.has_error()) {
      wpi::StringRef line = is.getline(lineBuf, 256).trim();
      if (line == GEN_MARKER) foundMarker = true;
      if (!foundMarker) continue;
      if (line.empty()) continue;
      if (line.contains("wlan0")) wlan = true;
      if (line.startswith("static ip_address")) {
        j[wlan ? "wifiNetworkApproach" : "networkApproach"] = "static";

        wpi::StringRef value = line.split('=').second.trim();
        wpi::StringRef cidrStr;
        std::tie(j[wlan ? "wifiNetworkAddress" : "networkAddress"], cidrStr) =
            value.split('/');

        unsigned int cidrInt;
        if (!cidrStr.getAsInteger(10, cidrInt)) {
          wpi::SmallString<64> netmaskBuf;
          j[wlan ? "wifiNetworkMask" : "networkMask"] =
              CidrToNetmask(cidrInt, netmaskBuf);
        }
      } else if (line.startswith("static routers")) {
        j[wlan ? "wifiNetworkGateway" : "networkGateway"] =
            line.split('=').second.trim();
      } else if (line.startswith("static domain_name_servers")) {
        j[wlan ? "wifiNetworkDNS" : "networkDNS"] =
            line.split('=').second.trim();
      } else if (line.startswith("fallback")) {
        j[wlan ? "wifiNetworkApproach" : "networkApproach"] = "dhcp-fallback";
      }
    }
  }

  //
  // hostapd configuration
  //
  {
    std::error_code ec;
    wpi::raw_fd_istream is(HOSTAPD_CONF, ec);
    if (ec) {
      j["wifiMode"] = "bridge";
      j["wifiChannel"] = "";
    } else {
      j["wifiMode"] = "access-point";
      wpi::SmallString<256> lineBuf;
      while (!is.has_error()) {
        wpi::StringRef line = is.getline(lineBuf, 256).trim();
        if (line.startswith("channel=")) {
          j["wifiChannel"] = line.drop_front(8);
        }
      }
    }
  }

  //
  // wpa supplicant configuration
  //
  {
    std::error_code ec;
    wpi::raw_fd_istream is(WPA_SUPPLICANT_CONF, ec);
    if (ec) {
      wpi::errs() << "could not read " WPA_SUPPLICANT_CONF "\n";
      return j;
    }

    wpi::SmallString<256> lineBuf;
    bool foundMarker = false;
    while (!is.has_error()) {
      wpi::StringRef line = is.getline(lineBuf, 256).trim();
      if (line == GEN_MARKER) foundMarker = true;
      if (!foundMarker) continue;
      if (line.empty()) continue;
      if (line.contains("ssid")) {
        j["wifiSsid"] = line.split('=').second.trim().drop_front().drop_back();
      } else if (line.contains("psk")) {
        j["wifiWpa2"] = line.split('=').second.trim().drop_front().drop_back();
      } else if (line.contains("key_mgmt")) {
        j["wifiWpa2"] = "";
      }
    }
  }

  return j;
}

// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#ifndef RPICONFIGSERVER_NETWORKSETTINGS_H_
#define RPICONFIGSERVER_NETWORKSETTINGS_H_

#include <functional>
#include <memory>

#include <wpi/Signal.h>
#include <wpi/StringRef.h>
#include <wpi/uv/Loop.h>

namespace wpi {
class json;
}  // namespace wpi

class NetworkSettings {
  struct private_init {};

 public:
  explicit NetworkSettings(const private_init&) {}
  NetworkSettings(const NetworkSettings&) = delete;
  NetworkSettings& operator=(const NetworkSettings&) = delete;

  void SetLoop(std::shared_ptr<wpi::uv::Loop> loop) {
    m_loop = std::move(loop);
  }

  enum WifiMode { kBridge, kAccessPoint };
  enum Mode { kDhcp, kStatic, kDhcpStatic };

  void Set(Mode mode, wpi::StringRef address, wpi::StringRef mask,
           wpi::StringRef gateway, wpi::StringRef dns, WifiMode wifiAPMode,
           int wifiChannel, wpi::StringRef wifiSsid, wpi::StringRef wifiWpa2,
           Mode wifiMode, wpi::StringRef wifiAddress, wpi::StringRef wifiMask,
           wpi::StringRef wifiGateway, wpi::StringRef wifiDns,
           std::function<void(wpi::StringRef)> onFail);

  void UpdateStatus();

  wpi::json GetStatusJson();

  wpi::sig::Signal<const wpi::json&> status;

  static std::shared_ptr<NetworkSettings> GetInstance();

 private:
  std::shared_ptr<wpi::uv::Loop> m_loop;
};

#endif  // RPICONFIGSERVER_NETWORKSETTINGS_H_

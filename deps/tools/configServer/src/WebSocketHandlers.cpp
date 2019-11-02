/*----------------------------------------------------------------------------*/
/* Copyright (c) 2018 FIRST. All Rights Reserved.                             */
/* Open Source Software - may be modified and shared by FRC teams. The code   */
/* must be accompanied by the FIRST BSD license file in the root directory of */
/* the project.                                                               */
/*----------------------------------------------------------------------------*/

#include "WebSocketHandlers.h"

#include <unistd.h>

#include <cstring>
#include <memory>

#include <wpi/SmallVector.h>
#include <wpi/WebSocket.h>
#include <wpi/json.h>
#include <wpi/raw_ostream.h>
#include <wpi/raw_uv_ostream.h>
#include <wpi/uv/Loop.h>
#include <wpi/uv/Pipe.h>
#include <wpi/uv/Process.h>

#include "Application.h"
#include "NetworkSettings.h"
#include "SystemStatus.h"
#include "UploadHelper.h"
#include "VisionSettings.h"
#include "VisionStatus.h"

namespace uv = wpi::uv;

#define SERVICE "/service/camera"

struct WebSocketData {
  bool visionLogEnabled = false;

  UploadHelper upload;

  wpi::sig::ScopedConnection sysStatusConn;
  wpi::sig::ScopedConnection sysWritableConn;
  wpi::sig::ScopedConnection visStatusConn;
  wpi::sig::ScopedConnection visLogConn;
  wpi::sig::ScopedConnection cameraListConn;
  wpi::sig::ScopedConnection netSettingsConn;
  wpi::sig::ScopedConnection visSettingsConn;
  wpi::sig::ScopedConnection appSettingsConn;
};

static void SendWsText(wpi::WebSocket& ws, const wpi::json& j) {
  wpi::SmallVector<uv::Buffer, 4> toSend;
  wpi::raw_uv_ostream os{toSend, 4096};
  os << j;
  ws.SendText(toSend, [](wpi::MutableArrayRef<uv::Buffer> bufs, uv::Error) {
    for (auto&& buf : bufs) buf.Deallocate();
  });
}

template <typename OnSuccessFunc, typename OnFailFunc, typename... Args>
static void RunProcess(wpi::WebSocket& ws, OnSuccessFunc success,
                       OnFailFunc fail, const wpi::Twine& file,
                       const Args&... args) {
  uv::Loop& loop = ws.GetStream().GetLoopRef();

  // create pipe to capture stderr
  auto pipe = uv::Pipe::Create(loop);
  if (auto proc = uv::Process::Spawn(
          loop, file,
          pipe ? uv::Process::StdioCreatePipe(2, *pipe, UV_WRITABLE_PIPE)
               : uv::Process::Option(),
          args...)) {
    // capture stderr output into string
    auto output = std::make_shared<std::string>();
    if (pipe) {
      pipe->StartRead();
      pipe->data.connect([output](uv::Buffer& buf, size_t len) {
        output->append(buf.base, len);
      });
      pipe->end.connect([p = pipe.get()] { p->Close(); });
    }

    // on exit, report
    proc->exited.connect(
        [ p = proc.get(), output, s = ws.shared_from_this(), fail, success ](
            int64_t status, int sig) {
          if (status != EXIT_SUCCESS) {
            SendWsText(
                *s,
                {{"type", "status"}, {"code", status}, {"message", *output}});
            fail(*s);
          } else {
            success(*s);
          }
          p->Close();
        });
  } else {
    SendWsText(ws,
               {{"type", "status"}, {"message", "could not spawn process"}});
    fail(ws);
  }
}

void InitWs(wpi::WebSocket& ws) {
  // set ws data
  auto data = std::make_shared<WebSocketData>();
  ws.SetData(data);

  // send initial system status and hook up system status updates
  auto sysStatus = SystemStatus::GetInstance();

  auto statusFunc = [&ws](const wpi::json& j) { SendWsText(ws, j); };
  statusFunc(sysStatus->GetStatusJson());
  data->sysStatusConn = sysStatus->status.connect_connection(statusFunc);

  auto writableFunc = [&ws](bool writable) {
    if (writable)
      SendWsText(ws, {{"type", "systemWritable"}});
    else
      SendWsText(ws, {{"type", "systemReadOnly"}});
  };
  writableFunc(sysStatus->GetWritable());
  data->sysWritableConn = sysStatus->writable.connect_connection(writableFunc);

  // hook up vision status updates and logging
  auto visStatus = VisionStatus::GetInstance();
  data->visStatusConn = visStatus->update.connect_connection(
      [&ws](const wpi::json& j) { SendWsText(ws, j); });
  data->visLogConn =
      visStatus->log.connect_connection([&ws](const wpi::json& j) {
        auto d = ws.GetData<WebSocketData>();
        if (d->visionLogEnabled) SendWsText(ws, j);
      });
  visStatus->UpdateStatus();
  data->cameraListConn = visStatus->cameraList.connect_connection(
      [&ws](const wpi::json& j) { SendWsText(ws, j); });
  visStatus->UpdateCameraList();

  // send initial network settings
  auto netSettings = NetworkSettings::GetInstance();
  auto netSettingsFunc = [&ws](const wpi::json& j) { SendWsText(ws, j); };
  netSettingsFunc(netSettings->GetStatusJson());
  data->netSettingsConn =
      netSettings->status.connect_connection(netSettingsFunc);

  // send initial vision settings
  auto visSettings = VisionSettings::GetInstance();
  auto visSettingsFunc = [&ws](const wpi::json& j) { SendWsText(ws, j); };
  visSettingsFunc(visSettings->GetStatusJson());
  data->visSettingsConn =
      visSettings->status.connect_connection(visSettingsFunc);

  // send initial application settings
  auto appSettings = Application::GetInstance();
  auto appSettingsFunc = [&ws](const wpi::json& j) { SendWsText(ws, j); };
  appSettingsFunc(appSettings->GetStatusJson());
  data->appSettingsConn =
      appSettings->status.connect_connection(appSettingsFunc);
}

void ProcessWsText(wpi::WebSocket& ws, wpi::StringRef msg) {
  wpi::errs() << "ws: '" << msg << "'\n";

  // parse
  wpi::json j;
  try {
    j = wpi::json::parse(msg, nullptr, false);
  } catch (const wpi::json::parse_error& e) {
    wpi::errs() << "parse error at byte " << e.byte << ": " << e.what() << '\n';
    return;
  }

  // top level must be an object
  if (!j.is_object()) {
    wpi::errs() << "not object\n";
    return;
  }

  // type
  std::string type;
  try {
    type = j.at("type").get<std::string>();
  } catch (const wpi::json::exception& e) {
    wpi::errs() << "could not read type: " << e.what() << '\n';
    return;
  }

  wpi::outs() << "type: " << type << '\n';

  //uv::Loop& loop = ws.GetStream().GetLoopRef();

  wpi::StringRef t(type);
  if (t.startswith("system")) {
    wpi::StringRef subType = t.substr(6);

    auto readOnlyFunc = [](wpi::WebSocket& s) {
      SendWsText(s, {{"type", "systemReadOnly"}});
    };
    auto writableFunc = [](wpi::WebSocket& s) {
      SendWsText(s, {{"type", "systemWritable"}});
    };

    if (subType == "Restart") {
      RunProcess(ws, [](wpi::WebSocket&) {}, [](wpi::WebSocket&) {},
                 "/sbin/reboot", "/sbin/reboot");
    } else if (subType == "ReadOnly") {
      RunProcess(
          ws, readOnlyFunc, writableFunc, "/bin/sh", "/bin/sh", "-c",
          "/bin/mount -o remount,ro / && /bin/mount -o remount,ro /boot");
    } else if (subType == "Writable") {
      RunProcess(
          ws, writableFunc, readOnlyFunc, "/bin/sh", "/bin/sh", "-c",
          "/bin/mount -o remount,rw / && /bin/mount -o remount,rw /boot");
    }
  } else if (t.startswith("vision")) {
    wpi::StringRef subType = t.substr(6);

    auto statusFunc = [s = ws.shared_from_this()](wpi::StringRef msg) {
      SendWsText(*s, {{"type", "status"}, {"message", msg}});
    };

    if (subType == "Up") {
      VisionStatus::GetInstance()->Up(statusFunc);
    } else if (subType == "Down") {
      VisionStatus::GetInstance()->Down(statusFunc);
    } else if (subType == "Term") {
      VisionStatus::GetInstance()->Terminate(statusFunc);
    } else if (subType == "Kill") {
      VisionStatus::GetInstance()->Kill(statusFunc);
    } else if (subType == "LogEnabled") {
      try {
        ws.GetData<WebSocketData>()->visionLogEnabled =
            j.at("value").get<bool>();
      } catch (const wpi::json::exception& e) {
        wpi::errs() << "could not read visionLogEnabled value: " << e.what()
                    << '\n';
        return;
      }
    } else if (subType == "Save") {
      try {
        VisionSettings::GetInstance()->Set(j.at("settings"), statusFunc);
      } catch (const wpi::json::exception& e) {
        wpi::errs() << "could not read visionSave value: " << e.what() << '\n';
        return;
      }
    }
  } else if (t == "networkSave") {
    auto statusFunc = [s = ws.shared_from_this()](wpi::StringRef msg) {
      SendWsText(*s, {{"type", "status"}, {"message", msg}});
    };
    try {
      NetworkSettings::Mode mode;
      auto& approach = j.at("networkApproach").get_ref<const std::string&>();
      if (approach == "dhcp")
        mode = NetworkSettings::kDhcp;
      else if (approach == "static")
        mode = NetworkSettings::kStatic;
      else if (approach == "dhcp-fallback")
        mode = NetworkSettings::kDhcpStatic;
      else {
        wpi::errs() << "could not understand networkApproach value: "
                    << approach << '\n';
        return;
      }
      NetworkSettings::GetInstance()->Set(
          mode, j.at("networkAddress").get_ref<const std::string&>(),
          j.at("networkMask").get_ref<const std::string&>(),
          j.at("networkGateway").get_ref<const std::string&>(),
          j.at("networkDNS").get_ref<const std::string&>(), statusFunc);
    } catch (const wpi::json::exception& e) {
      wpi::errs() << "could not read networkSave value: " << e.what() << '\n';
      return;
    }
  } else if (t.startswith("application")) {
    wpi::StringRef subType = t.substr(11);

    auto statusFunc = [s = ws.shared_from_this()](wpi::StringRef msg) {
      SendWsText(*s, {{"type", "status"}, {"message", msg}});
    };

    std::string appType;
    try {
      appType = j.at("applicationType").get<std::string>();
    } catch (const wpi::json::exception& e) {
      wpi::errs() << "could not read applicationSave value: " << e.what()
                  << '\n';
      return;
    }

    if (subType == "Save") {
      Application::GetInstance()->Set(appType, statusFunc);
    } else if (subType == "StartUpload") {
      auto d = ws.GetData<WebSocketData>();
      d->upload.Open(EXEC_HOME "/appUploadXXXXXX",
                     wpi::StringRef(appType).endswith("python"), statusFunc);
    } else if (subType == "FinishUpload") {
      auto d = ws.GetData<WebSocketData>();
      if (d->upload)
        Application::GetInstance()->FinishUpload(appType, d->upload,
                                                 statusFunc);
      SendWsText(ws, {{"type", "applicationSaveComplete"}});
    }
  } else if (t.startswith("file")) {
    wpi::StringRef subType = t.substr(4);
    if (subType == "StartUpload") {
      auto statusFunc = [s = ws.shared_from_this()](wpi::StringRef msg) {
        SendWsText(*s, {{"type", "status"}, {"message", msg}});
      };
      auto d = ws.GetData<WebSocketData>();
      d->upload.Open(EXEC_HOME "/fileUploadXXXXXX", false, statusFunc);
    } else if (subType == "FinishUpload") {
      auto d = ws.GetData<WebSocketData>();

      // change ownership
      if (fchown(d->upload.GetFD(), APP_UID, APP_GID) == -1) {
        wpi::errs() << "could not change file ownership: "
                    << std::strerror(errno) << '\n';
      }
      d->upload.Close();

      bool extract;
      try {
        extract = j.at("extract").get<bool>();
      } catch (const wpi::json::exception& e) {
        wpi::errs() << "could not read extract value: " << e.what() << '\n';
        unlink(d->upload.GetFilename());
        return;
      }

      std::string filename;
      try {
        filename = j.at("fileName").get<std::string>();
      } catch (const wpi::json::exception& e) {
        wpi::errs() << "could not read fileName value: " << e.what() << '\n';
        unlink(d->upload.GetFilename());
        return;
      }

      auto extractSuccess = [](wpi::WebSocket& s) {
        auto d = s.GetData<WebSocketData>();
        unlink(d->upload.GetFilename());
        SendWsText(s, {{"type", "fileUploadComplete"}, {"success", true}});
      };
      auto extractFailure = [](wpi::WebSocket& s) {
        auto d = s.GetData<WebSocketData>();
        unlink(d->upload.GetFilename());
        wpi::errs() << "could not extract file\n";
        SendWsText(s, {{"type", "fileUploadComplete"}});
      };

      if (extract && wpi::StringRef{filename}.endswith("tar.gz")) {
        RunProcess(ws, extractSuccess, extractFailure, "/bin/tar",
                   uv::Process::Uid(APP_UID), uv::Process::Gid(APP_GID),
                   uv::Process::Cwd(EXEC_HOME), "/bin/tar", "xzf",
                   d->upload.GetFilename());
      } else if (extract && wpi::StringRef{filename}.endswith("zip")) {
        d->upload.Close();
        RunProcess(ws, extractSuccess, extractFailure, "/usr/bin/unzip",
                   uv::Process::Uid(APP_UID), uv::Process::Gid(APP_GID),
                   uv::Process::Cwd(EXEC_HOME), "/usr/bin/unzip",
                   d->upload.GetFilename());
      } else {
        wpi::SmallString<64> pathname;
        pathname = EXEC_HOME;
        pathname += '/';
        pathname += filename;
        if (unlink(pathname.c_str()) == -1) {
          wpi::errs() << "could not remove file: " << std::strerror(errno)
                      << '\n';
        }

        // rename temporary file to new file
        if (rename(d->upload.GetFilename(), pathname.c_str()) == -1) {
          wpi::errs() << "could not rename: " << std::strerror(errno) << '\n';
        }

        SendWsText(ws, {{"type", "fileUploadComplete"}});
      }
    }
  }
}

void ProcessWsBinary(wpi::WebSocket& ws, wpi::ArrayRef<uint8_t> msg) {
  auto d = ws.GetData<WebSocketData>();
  if (d->upload) d->upload.Write(msg);
}

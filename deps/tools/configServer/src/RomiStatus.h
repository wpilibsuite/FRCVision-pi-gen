/*----------------------------------------------------------------------------*/
/* Copyright (c) 2018 FIRST. All Rights Reserved.                             */
/* Open Source Software - may be modified and shared by FRC teams. The code   */
/* must be accompanied by the FIRST BSD license file in the root directory of */
/* the project.                                                               */
/*----------------------------------------------------------------------------*/

#ifndef RPICONFIGSERVER_ROMISTATUS_H_
#define RPICONFIGSERVER_ROMISTATUS_H_

#include <functional>
#include <memory>
#include <vector>

#include <cscore.h>
#include <wpi/Signal.h>
#include <wpi/StringRef.h>
#include <wpi/uv/Loop.h>

namespace wpi {
class json;

namespace uv {
class Buffer;
}  // namespace uv
}  // namespace wpi

class RomiStatus {
  struct private_init {};

 public:
  explicit RomiStatus(const private_init&) {}
  RomiStatus(const RomiStatus&) = delete;
  RomiStatus& operator=(const RomiStatus&) = delete;

  void SetLoop(std::shared_ptr<wpi::uv::Loop> loop);

  void Up(std::function<void(wpi::StringRef)> onFail);
  void Down(std::function<void(wpi::StringRef)> onFail);
  void Terminate(std::function<void(wpi::StringRef)> onFail);
  void Kill(std::function<void(wpi::StringRef)> onFail);

  void UpdateStatus();
  void ConsoleLog(wpi::uv::Buffer& buf, size_t len);

  void FirmwareUpdate(std::function<void(wpi::StringRef)> onFail);

  void UpdateIoConfig(std::function<void(wpi::StringRef)> onFail);

  void SaveConfig(const wpi::json& data, std::function<void(wpi::StringRef)> onFail);

  wpi::sig::Signal<const wpi::json&> update;
  wpi::sig::Signal<const wpi::json&> log;
  wpi::sig::Signal<const wpi::json&> ioConfig;

  static std::shared_ptr<RomiStatus> GetInstance();

 private:
  void RunSvc(const char* cmd, std::function<void(wpi::StringRef)> onFail);
  wpi::json GetIoConfigJson(std::function<void(wpi::StringRef)> onFail);

  std::shared_ptr<wpi::uv::Loop> m_loop;
};

#endif  // RPICONFIGSERVER_VISIONSTATUS_H_

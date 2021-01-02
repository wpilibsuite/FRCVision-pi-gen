// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#ifndef RPICONFIGSERVER_VISIONSTATUS_H_
#define RPICONFIGSERVER_VISIONSTATUS_H_

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

class VisionStatus {
  struct private_init {};

 public:
  explicit VisionStatus(const private_init&) {}
  VisionStatus(const VisionStatus&) = delete;
  VisionStatus& operator=(const VisionStatus&) = delete;

  void SetLoop(std::shared_ptr<wpi::uv::Loop> loop);

  void Up(std::function<void(wpi::StringRef)> onFail);
  void Down(std::function<void(wpi::StringRef)> onFail);
  void Terminate(std::function<void(wpi::StringRef)> onFail);
  void Kill(std::function<void(wpi::StringRef)> onFail);

  void UpdateStatus();
  void ConsoleLog(wpi::uv::Buffer& buf, size_t len);
  void UpdateCameraList();

  wpi::sig::Signal<const wpi::json&> update;
  wpi::sig::Signal<const wpi::json&> log;
  wpi::sig::Signal<const wpi::json&> cameraList;

  static std::shared_ptr<VisionStatus> GetInstance();

 private:
  void RunSvc(const char* cmd, std::function<void(wpi::StringRef)> onFail);
  void RefreshCameraList();

  std::shared_ptr<wpi::uv::Loop> m_loop;

  struct CameraInfo {
    cs::UsbCameraInfo info;
    std::vector<cs::VideoMode> modes;
  };
  std::vector<CameraInfo> m_cameraInfo;
};

#endif  // RPICONFIGSERVER_VISIONSTATUS_H_

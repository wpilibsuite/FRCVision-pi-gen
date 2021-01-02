// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#ifndef RPICONFIGSERVER_VISIONSETTINGS_H_
#define RPICONFIGSERVER_VISIONSETTINGS_H_

#include <functional>
#include <memory>

#include <wpi/Signal.h>
#include <wpi/StringRef.h>

namespace wpi {
class json;
}  // namespace wpi

class VisionSettings {
  struct private_init {};

 public:
  explicit VisionSettings(const private_init&) {}
  VisionSettings(const VisionSettings&) = delete;
  VisionSettings& operator=(const VisionSettings&) = delete;

  void Set(const wpi::json& data, std::function<void(wpi::StringRef)> onFail);

  void UpdateStatus();

  wpi::json GetStatusJson();

  wpi::sig::Signal<const wpi::json&> status;

  static std::shared_ptr<VisionSettings> GetInstance();
};

#endif  // RPICONFIGSERVER_VISIONSETTINGS_H_

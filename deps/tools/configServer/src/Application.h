/*----------------------------------------------------------------------------*/
/* Copyright (c) 2018 FIRST. All Rights Reserved.                             */
/* Open Source Software - may be modified and shared by FRC teams. The code   */
/* must be accompanied by the FIRST BSD license file in the root directory of */
/* the project.                                                               */
/*----------------------------------------------------------------------------*/

#ifndef RPICONFIGSERVER_APPLICATION_H_
#define RPICONFIGSERVER_APPLICATION_H_

#include <functional>
#include <memory>
#include <string>

#include <wpi/ArrayRef.h>
#include <wpi/Signal.h>
#include <wpi/StringRef.h>

namespace wpi {
class json;
}  // namespace wpi

class UploadHelper;

class Application {
  struct private_init {};

 public:
  explicit Application(const private_init&) {}
  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;

  void Set(wpi::StringRef appType, std::function<void(wpi::StringRef)> onFail);

  void FinishUpload(wpi::StringRef appType, UploadHelper& helper,
                    std::function<void(wpi::StringRef)> onFail);

  void UpdateStatus();

  wpi::json GetStatusJson();

  wpi::sig::Signal<const wpi::json&> status;

  static std::shared_ptr<Application> GetInstance();
};

#endif  // RPICONFIGSERVER_APPLICATION_H_

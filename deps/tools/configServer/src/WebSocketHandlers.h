// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#ifndef RPICONFIGSERVER_WEBSOCKETHANDLERS_H_
#define RPICONFIGSERVER_WEBSOCKETHANDLERS_H_

#include <wpi/ArrayRef.h>
#include <wpi/StringRef.h>

namespace wpi {
class WebSocket;
}  // namespace wpi

void InitWs(wpi::WebSocket& ws);
void ProcessWsText(wpi::WebSocket& ws, wpi::StringRef msg);
void ProcessWsBinary(wpi::WebSocket& ws, wpi::ArrayRef<uint8_t> msg);

#endif  // RPICONFIGSERVER_WEBSOCKETHANDLERS_H_

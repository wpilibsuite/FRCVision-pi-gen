// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#ifndef RPICONFIGSERVER_MYHTTPCONNECTION_H_
#define RPICONFIGSERVER_MYHTTPCONNECTION_H_

#include <memory>

#include <wpi/HttpServerConnection.h>
#include <wpi/Twine.h>
#include <wpi/WebSocketServer.h>
#include <wpi/uv/Stream.h>

class MyHttpConnection : public wpi::HttpServerConnection,
                         public std::enable_shared_from_this<MyHttpConnection> {
 public:
  explicit MyHttpConnection(std::shared_ptr<wpi::uv::Stream> stream);

 protected:
  void ProcessRequest() override;
  void SendFileResponse(int code, const wpi::Twine& codeText,
                        const wpi::Twine& contentType,
                        const wpi::Twine& filename,
                        const wpi::Twine& extraHeader = wpi::Twine{});

  wpi::WebSocketServerHelper m_websocketHelper;
};

#endif  // RPICONFIGSERVER_MYHTTPCONNECTION_H_

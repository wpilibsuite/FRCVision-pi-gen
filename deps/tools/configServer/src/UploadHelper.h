/*----------------------------------------------------------------------------*/
/* Copyright (c) 2019 FIRST. All Rights Reserved.                             */
/* Open Source Software - may be modified and shared by FRC teams. The code   */
/* must be accompanied by the FIRST BSD license file in the root directory of */
/* the project.                                                               */
/*----------------------------------------------------------------------------*/

#ifndef RPICONFIGSERVER_UPLOADHELPER_H_
#define RPICONFIGSERVER_UPLOADHELPER_H_

#include <stdint.h>

#include <functional>

#include <wpi/ArrayRef.h>
#include <wpi/SmallString.h>
#include <wpi/StringRef.h>

class UploadHelper {
 public:
  UploadHelper() = default;
  ~UploadHelper() { Close(); }

  UploadHelper(const UploadHelper&) = delete;
  UploadHelper& operator=(const UploadHelper&) = delete;

  UploadHelper(UploadHelper&& oth);
  UploadHelper& operator=(UploadHelper&& oth);

  explicit operator bool() const { return m_fd != -1; }

  const char* GetFilename() { return m_filename.c_str(); }
  int GetFD() const { return m_fd; }

  bool Open(wpi::StringRef filename, bool text,
            std::function<void(wpi::StringRef)> onFail);
  void Write(wpi::ArrayRef<uint8_t> contents);
  void Close();

 private:
  wpi::SmallString<128> m_filename;
  int m_fd = -1;
  bool m_text;
  bool m_hasEol;
};

#endif  // RPICONFIGSERVER_UPLOADHELPER_H_

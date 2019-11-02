/*----------------------------------------------------------------------------*/
/* Copyright (c) 2019 FIRST. All Rights Reserved.                             */
/* Open Source Software - may be modified and shared by FRC teams. The code   */
/* must be accompanied by the FIRST BSD license file in the root directory of */
/* the project.                                                               */
/*----------------------------------------------------------------------------*/

#include "UploadHelper.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include <wpi/raw_ostream.h>

UploadHelper::UploadHelper(UploadHelper&& oth)
    : m_filename{std::move(oth.m_filename)},
      m_fd{oth.m_fd},
      m_text{oth.m_text} {
  oth.m_fd = -1;
}

UploadHelper& UploadHelper::operator=(UploadHelper&& oth) {
  m_filename = std::move(oth.m_filename);
  m_fd = oth.m_fd;
  oth.m_fd = -1;
  m_text = oth.m_text;
  return *this;
}

bool UploadHelper::Open(wpi::StringRef filename, bool text,
                        std::function<void(wpi::StringRef)> onFail) {
  m_text = text;
  m_hasEol = true;
  m_filename = filename;
  // make it a C string
  m_filename.push_back(0);
  m_filename.pop_back();

  m_fd = mkstemp(m_filename.data());
  if (m_fd < 0) {
    wpi::SmallString<64> msg;
    msg = "could not open temporary file: ";
    msg += std::strerror(errno);
    onFail(msg);
  }
  return m_fd >= 0;
}

void UploadHelper::Write(wpi::ArrayRef<uint8_t> contents) {
  if (m_fd < 0) return;
  // write contents
  wpi::raw_fd_ostream out(m_fd, false);
  if (m_text) {
    wpi::StringRef str(reinterpret_cast<const char*>(contents.data()),
                       contents.size());
    // convert any Windows EOL to Unix
    for (;;) {
      size_t idx = str.find("\r\n");
      if (idx == wpi::StringRef::npos) break;
      out << str.slice(0, idx) << '\n';
      str = str.slice(idx + 2, wpi::StringRef::npos);
    }
    out << str;
    m_hasEol == str.empty() || str.back() == '\n';
  } else {
    out << contents;
  }
}

void UploadHelper::Close() {
  if (m_fd < 0) return;
  // ensure text file ends with EOL
  if (m_text && !m_hasEol) {
    wpi::raw_fd_ostream out(m_fd, false);
    out << '\n';
  }
  ::close(m_fd);
  m_fd = -1;
}

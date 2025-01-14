// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_SOCKET_STATE_H_
#define PLATFORM_BASE_SOCKET_STATE_H_

#include <cstdint>
#include <memory>
#include <string>

namespace openscreen {
namespace platform {

// SocketState should be used by TCP and TLS sockets for indicating
// current state. NOTE: socket state transitions should only happen in
// the listed order. New states should be added in appropriate order.
enum class SocketState {
  // Socket is not connected.
  kNotConnected = 0,

  // Socket is currently being connected.
  kConnecting,

  // Socket is actively connected to a remote address.
  kConnected,

  // The socket connection has been terminated, either by Close() or
  // by the remote side.
  kClosed
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_BASE_SOCKET_STATE_H_

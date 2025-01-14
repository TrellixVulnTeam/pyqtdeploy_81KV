// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_TLS_CONNECTION_H_
#define PLATFORM_API_TLS_CONNECTION_H_

#include <cstdint>
#include <vector>

#include "platform/base/error.h"
#include "platform/base/ip_address.h"

namespace openscreen {
namespace platform {

class TlsConnection {
 public:
  // Client callbacks are run via the TaskRunner used by TlsConnectionFactory.
  class Client {
   public:
    // Called when |connection| writing is blocked and unblocked, respectively.
    // Note that implementations should do best effort to buffer packets even in
    // blocked state, and should call OnError if we actually overflow the
    // buffer.
    virtual void OnWriteBlocked(TlsConnection* connection) = 0;
    virtual void OnWriteUnblocked(TlsConnection* connection) = 0;

    // Called when |connection| experiences an error, such as a read error.
    virtual void OnError(TlsConnection* connection, Error error) = 0;

    // Called when a |block| arrives on |connection|.
    virtual void OnRead(TlsConnection* connection,
                        std::vector<uint8_t> block) = 0;

   protected:
    virtual ~Client() = default;
  };

  virtual ~TlsConnection();

  // Sets the Client associated with this instance. This should be called as
  // soon as the factory provides a new TlsConnection instance via
  // TlsConnectionFactory::OnAccepted() or OnConnected(). Pass nullptr to unset
  // the Client.
  virtual void SetClient(Client* client) = 0;

  // Sends a message.
  virtual void Write(const void* data, size_t len) = 0;

  // Get the local address.
  virtual IPEndpoint GetLocalEndpoint() const = 0;

  // Get the connected remote address.
  virtual IPEndpoint GetRemoteEndpoint() const = 0;

 protected:
  TlsConnection();
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_TLS_CONNECTION_H_

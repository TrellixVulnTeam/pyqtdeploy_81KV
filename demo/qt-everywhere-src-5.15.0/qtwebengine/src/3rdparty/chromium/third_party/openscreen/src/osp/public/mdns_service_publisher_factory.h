// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_PUBLIC_MDNS_SERVICE_PUBLISHER_FACTORY_H_
#define OSP_PUBLIC_MDNS_SERVICE_PUBLISHER_FACTORY_H_

#include <memory>

#include "osp/public/service_publisher.h"

namespace openscreen {

namespace platform {
class TaskRunner;
}  // namespace platform

namespace osp {

class MdnsServicePublisherFactory {
 public:
  static std::unique_ptr<ServicePublisher> Create(
      const ServicePublisher::Config& config,
      ServicePublisher::Observer* observer,
      platform::TaskRunner* task_runner);
};

}  // namespace osp
}  // namespace openscreen

#endif  // OSP_PUBLIC_MDNS_SERVICE_PUBLISHER_FACTORY_H_

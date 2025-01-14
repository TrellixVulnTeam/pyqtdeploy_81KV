// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_IMPL_CONVERSION_LAYER_H_
#define DISCOVERY_DNSSD_IMPL_CONVERSION_LAYER_H_

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "discovery/dnssd/impl/constants.h"
#include "discovery/dnssd/public/dns_sd_instance_record.h"
#include "discovery/dnssd/public/dns_sd_txt_record.h"
#include "discovery/mdns/mdns_records.h"
#include "platform/base/error.h"

namespace openscreen {
namespace discovery {

class InstanceKey;
class ServiceKey;

// *** Conversions from DNS entities to DNS-SD Entities ***

// Attempts to create a new TXT record from the provided set of strings,
// returning a TxtRecord on success or an error if the provided strings are
// not valid.
ErrorOr<DnsSdTxtRecord> CreateFromDnsTxt(const TxtRecordRdata& txt);

bool IsPtrRecord(const MdnsRecord& record);

// Checks that the instance, service, and domain ids in this MdnsRecord are
// valid.
bool HasValidDnsRecordAddress(const MdnsRecord& record);

//*** Conversions to DNS entities from DNS-SD Entities ***

// Returns the query required to get all instance information about the service
// instances described by the provided ServiceKey.
DnsQueryInfo GetInstanceQueryInfo(const InstanceKey& key);

// Returns the query required to get all service information that matches the
// provided key.
DnsQueryInfo GetPtrQueryInfo(const ServiceKey& key);

// Returns all MdnsRecord entities generated from this InstanceRecord.
std::vector<MdnsRecord> GetDnsRecords(const DnsSdInstanceRecord& record);

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_IMPL_CONVERSION_LAYER_H_

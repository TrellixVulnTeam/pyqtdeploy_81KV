/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/trace_processor/importers/proto/android_probes_parser.h"

#include "perfetto/base/logging.h"
#include "perfetto/ext/traced/sys_stats_counters.h"
#include "src/trace_processor/args_tracker.h"
#include "src/trace_processor/clock_tracker.h"
#include "src/trace_processor/event_tracker.h"
#include "src/trace_processor/process_tracker.h"
#include "src/trace_processor/syscall_tracker.h"
#include "src/trace_processor/trace_processor_context.h"

#include "protos/perfetto/common/android_log_constants.pbzero.h"
#include "protos/perfetto/config/trace_config.pbzero.h"
#include "protos/perfetto/trace/android/android_log.pbzero.h"
#include "protos/perfetto/trace/android/packages_list.pbzero.h"
#include "protos/perfetto/trace/clock_snapshot.pbzero.h"
#include "protos/perfetto/trace/power/battery_counters.pbzero.h"
#include "protos/perfetto/trace/power/power_rails.pbzero.h"
#include "protos/perfetto/trace/ps/process_stats.pbzero.h"
#include "protos/perfetto/trace/ps/process_tree.pbzero.h"
#include "protos/perfetto/trace/sys_stats/sys_stats.pbzero.h"
#include "protos/perfetto/trace/system_info.pbzero.h"

namespace perfetto {
namespace trace_processor {

AndroidProbesParser::AndroidProbesParser(TraceProcessorContext* context)
    : context_(context),
      batt_charge_id_(context->storage->InternString("batt.charge_uah")),
      batt_capacity_id_(context->storage->InternString("batt.capacity_pct")),
      batt_current_id_(context->storage->InternString("batt.current_ua")),
      batt_current_avg_id_(
          context->storage->InternString("batt.current.avg_ua")) {}

void AndroidProbesParser::ParseBatteryCounters(int64_t ts, ConstBytes blob) {
  protos::pbzero::BatteryCounters::Decoder evt(blob.data, blob.size);
  if (evt.has_charge_counter_uah()) {
    TrackId track =
        context_->track_tracker->InternGlobalCounterTrack(batt_charge_id_);
    context_->event_tracker->PushCounter(ts, evt.charge_counter_uah(), track);
  }
  if (evt.has_capacity_percent()) {
    TrackId track =
        context_->track_tracker->InternGlobalCounterTrack(batt_capacity_id_);
    context_->event_tracker->PushCounter(
        ts, static_cast<double>(evt.capacity_percent()), track);
  }
  if (evt.has_current_ua()) {
    TrackId track =
        context_->track_tracker->InternGlobalCounterTrack(batt_current_id_);
    context_->event_tracker->PushCounter(ts, evt.current_ua(), track);
  }
  if (evt.has_current_avg_ua()) {
    TrackId track =
        context_->track_tracker->InternGlobalCounterTrack(batt_current_avg_id_);
    context_->event_tracker->PushCounter(ts, evt.current_avg_ua(), track);
  }
}

void AndroidProbesParser::ParsePowerRails(int64_t ts, ConstBytes blob) {
  protos::pbzero::PowerRails::Decoder evt(blob.data, blob.size);
  if (evt.has_rail_descriptor()) {
    for (auto it = evt.rail_descriptor(); it; ++it) {
      protos::pbzero::PowerRails::RailDescriptor::Decoder desc(*it);
      uint32_t idx = desc.index();
      if (PERFETTO_UNLIKELY(idx > 256)) {
        PERFETTO_DLOG("Skipping excessively large power_rail index %" PRIu32,
                      idx);
        continue;
      }
      if (power_rails_strs_id_.size() <= idx)
        power_rails_strs_id_.resize(idx + 1);
      char counter_name[255];
      snprintf(counter_name, sizeof(counter_name), "power.%.*s_uws",
               int(desc.rail_name().size), desc.rail_name().data);
      power_rails_strs_id_[idx] = context_->storage->InternString(counter_name);
    }
  }

  if (evt.has_energy_data()) {
    for (auto it = evt.energy_data(); it; ++it) {
      protos::pbzero::PowerRails::EnergyData::Decoder desc(*it);
      if (desc.index() < power_rails_strs_id_.size()) {
        int64_t actual_ts =
            desc.has_timestamp_ms()
                ? static_cast<int64_t>(desc.timestamp_ms()) * 1000000
                : ts;
        TrackId track = context_->track_tracker->InternGlobalCounterTrack(
            power_rails_strs_id_[desc.index()]);
        context_->event_tracker->PushCounter(actual_ts, desc.energy(), track);
      } else {
        context_->storage->IncrementStats(stats::power_rail_unknown_index);
      }
    }
  }
}

void AndroidProbesParser::ParseAndroidLogPacket(ConstBytes blob) {
  protos::pbzero::AndroidLogPacket::Decoder packet(blob.data, blob.size);
  for (auto it = packet.events(); it; ++it)
    ParseAndroidLogEvent(*it);

  if (packet.has_stats())
    ParseAndroidLogStats(packet.stats());
}

void AndroidProbesParser::ParseAndroidLogEvent(ConstBytes blob) {
  // TODO(primiano): Add events and non-stringified fields to the "raw" table.
  protos::pbzero::AndroidLogPacket::LogEvent::Decoder evt(blob.data, blob.size);
  int64_t ts = static_cast<int64_t>(evt.timestamp());
  uint32_t pid = static_cast<uint32_t>(evt.pid());
  uint32_t tid = static_cast<uint32_t>(evt.tid());
  uint8_t prio = static_cast<uint8_t>(evt.prio());
  StringId tag_id = context_->storage->InternString(
      evt.has_tag() ? evt.tag() : base::StringView());
  StringId msg_id = context_->storage->InternString(
      evt.has_message() ? evt.message() : base::StringView());

  char arg_msg[4096];
  char* arg_str = &arg_msg[0];
  *arg_str = '\0';
  auto arg_avail = [&arg_msg, &arg_str]() {
    return sizeof(arg_msg) - static_cast<size_t>(arg_str - arg_msg);
  };
  for (auto it = evt.args(); it; ++it) {
    protos::pbzero::AndroidLogPacket::LogEvent::Arg::Decoder arg(*it);
    if (!arg.has_name())
      continue;
    arg_str +=
        snprintf(arg_str, arg_avail(),
                 " %.*s=", static_cast<int>(arg.name().size), arg.name().data);
    if (arg.has_string_value()) {
      arg_str += snprintf(arg_str, arg_avail(), "\"%.*s\"",
                          static_cast<int>(arg.string_value().size),
                          arg.string_value().data);
    } else if (arg.has_int_value()) {
      arg_str += snprintf(arg_str, arg_avail(), "%" PRId64, arg.int_value());
    } else if (arg.has_float_value()) {
      arg_str += snprintf(arg_str, arg_avail(), "%f",
                          static_cast<double>(arg.float_value()));
    }
  }

  if (prio == 0)
    prio = protos::pbzero::AndroidLogPriority::PRIO_INFO;

  if (arg_str != &arg_msg[0]) {
    PERFETTO_DCHECK(msg_id.is_null());
    // Skip the first space char (" foo=1 bar=2" -> "foo=1 bar=2").
    msg_id = context_->storage->InternString(&arg_msg[1]);
  }
  UniquePid utid = tid ? context_->process_tracker->UpdateThread(tid, pid) : 0;
  base::Optional<int64_t> opt_trace_time = context_->clock_tracker->ToTraceTime(
      protos::pbzero::ClockSnapshot::Clock::REALTIME, ts);
  if (!opt_trace_time)
    return;

  // Log events are NOT required to be sorted by trace_time. The virtual table
  // will take care of sorting on-demand.
  context_->storage->mutable_android_log()->AddLogEvent(
      opt_trace_time.value(), utid, prio, tag_id, msg_id);
}

void AndroidProbesParser::ParseAndroidLogStats(ConstBytes blob) {
  protos::pbzero::AndroidLogPacket::Stats::Decoder evt(blob.data, blob.size);
  if (evt.has_num_failed()) {
    context_->storage->SetStats(stats::android_log_num_failed,
                                static_cast<int64_t>(evt.num_failed()));
  }

  if (evt.has_num_skipped()) {
    context_->storage->SetStats(stats::android_log_num_skipped,
                                static_cast<int64_t>(evt.num_skipped()));
  }

  if (evt.has_num_total()) {
    context_->storage->SetStats(stats::android_log_num_total,
                                static_cast<int64_t>(evt.num_total()));
  }
}

void AndroidProbesParser::ParseStatsdMetadata(ConstBytes blob) {
  protos::pbzero::TraceConfig::StatsdMetadata::Decoder metadata(blob.data,
                                                                blob.size);
  if (metadata.has_triggering_subscription_id()) {
    context_->storage->SetMetadata(
        metadata::statsd_triggering_subscription_id,
        Variadic::Integer(metadata.triggering_subscription_id()));
  }
}

void AndroidProbesParser::ParseAndroidPackagesList(ConstBytes blob) {
  protos::pbzero::PackagesList::Decoder pkg_list(blob.data, blob.size);
  context_->storage->SetStats(stats::packages_list_has_read_errors,
                              pkg_list.read_error());
  context_->storage->SetStats(stats::packages_list_has_parse_errors,
                              pkg_list.parse_error());

  // Insert the package info into arg sets (one set per package), with the arg
  // set ids collected in the Metadata table, under
  // metadata::android_packages_list key type.
  for (auto it = pkg_list.packages(); it; ++it) {
    // Insert a placeholder metadata entry, which will be overwritten by the
    // arg_set_id when the arg tracker is flushed.
    RowId row_id = context_->storage->AppendMetadata(
        metadata::android_packages_list, Variadic::Integer(0));

    auto add_arg = [this, row_id](base::StringView name, Variadic value) {
      StringId key_id = context_->storage->InternString(name);
      context_->args_tracker->AddArg(row_id, key_id, key_id, value);
    };
    protos::pbzero::PackagesList_PackageInfo::Decoder pkg(*it);
    add_arg("name",
            Variadic::String(context_->storage->InternString(pkg.name())));
    add_arg("uid", Variadic::UnsignedInteger(pkg.uid()));
    add_arg("debuggable", Variadic::Boolean(pkg.debuggable()));
    add_arg("profileable_from_shell",
            Variadic::Boolean(pkg.profileable_from_shell()));
    add_arg("version_code", Variadic::Integer(pkg.version_code()));
  }
}

}  // namespace trace_processor
}  // namespace perfetto

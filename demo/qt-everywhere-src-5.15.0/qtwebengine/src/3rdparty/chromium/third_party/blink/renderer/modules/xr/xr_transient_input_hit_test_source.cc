// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_transient_input_hit_test_source.h"

#include "third_party/blink/renderer/modules/xr/xr_input_source_array.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_transient_input_hit_test_result.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

#include "device/vr/public/mojom/vr_service.mojom-blink.h"

namespace {
const char* kCannotCancelHitTestSource2 =
    "Hit test source could not be canceled. Ensure that it was not already "
    "canceled.";
}

namespace blink {

XRTransientInputHitTestSource::XRTransientInputHitTestSource(
    uint64_t id,
    XRSession* xr_session)
    : id_(id), xr_session_(xr_session) {
  DCHECK(xr_session_);
}

uint64_t XRTransientInputHitTestSource::id() const {
  return id_;
}

void XRTransientInputHitTestSource::cancel(ExceptionState& exception_state) {
  if (!xr_session_->RemoveHitTestSource(this)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kCannotCancelHitTestSource2);
  }
}

void XRTransientInputHitTestSource::Update(
    const HashMap<uint32_t, Vector<device::mojom::blink::XRHitResultPtr>>&
        hit_test_results,
    XRInputSourceArray* input_source_array) {
  // TODO: Be smarter about the update - it's possible to add new resulst /
  // remove the ones that were removed & update the ones that are being changed.
  current_frame_results_.clear();

  // If we don't know anything about input sources, we won't be able to
  // construct any results so we are done (and current_frame_results_ should
  // stay empty).
  if (!input_source_array) {
    return;
  }

  for (const auto& source_id_and_results : hit_test_results) {
    XRInputSource* input_source =
        input_source_array->GetWithSourceId(source_id_and_results.key);
    // If the input source with the given ID could not be found, just skip
    // processing results for this input source.
    if (!input_source)
      continue;

    current_frame_results_.push_back(
        MakeGarbageCollected<XRTransientInputHitTestResult>(
            input_source, source_id_and_results.value));
  }
}

HeapVector<Member<XRTransientInputHitTestResult>>
XRTransientInputHitTestSource::Results() {
  return current_frame_results_;
}

void XRTransientInputHitTestSource::Trace(blink::Visitor* visitor) {
  visitor->Trace(current_frame_results_);
  visitor->Trace(xr_session_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

/*
 *
 * Copyright 2017 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef GRPC_CORE_LIB_GPRPP_REF_COUNTED_H
#define GRPC_CORE_LIB_GPRPP_REF_COUNTED_H

#include <grpc/support/port_platform.h>

#include <grpc/support/log.h>
#include <grpc/support/sync.h>

#include <cinttypes>

#include "src/core/lib/debug/trace.h"
#include "src/core/lib/gprpp/abstract.h"
#include "src/core/lib/gprpp/debug_location.h"
#include "src/core/lib/gprpp/memory.h"
#include "src/core/lib/gprpp/ref_counted_ptr.h"
#include "src/core/lib/gprpp/sync.h"

namespace grpc_core {

// A base class for reference-counted objects.
// New objects should be created via New() and start with a refcount of 1.
// When the refcount reaches 0, the object will be deleted via Delete().
//
// This will commonly be used by CRTP (curiously-recurring template pattern)
// e.g., class MyClass : public RefCounted<MyClass>
template <typename Child>
class RefCounted {
 public:
  RefCountedPtr<Child> Ref() GRPC_MUST_USE_RESULT {
    IncrementRefCount();
    return RefCountedPtr<Child>(static_cast<Child*>(this));
  }

  // TODO(roth): Once all of our code is converted to C++ and can use
  // RefCountedPtr<> instead of manual ref-counting, make this method
  // private, since it will only be used by RefCountedPtr<>, which is a
  // friend of this class.
  void Unref() {
    if (grpc_core::Unref(&refs_)) {
      Delete(static_cast<Child*>(this));
    }
  }

  // Not copyable nor movable.
  RefCounted(const RefCounted&) = delete;
  RefCounted& operator=(const RefCounted&) = delete;

  GRPC_ABSTRACT_BASE_CLASS

 protected:
  GPRC_ALLOW_CLASS_TO_USE_NON_PUBLIC_DELETE

  RefCounted() { grpc_core::RefInit(&refs_, 1); }

  virtual ~RefCounted() {}

 private:
  // Allow RefCountedPtr<> to access IncrementRefCount().
  template <typename T>
  friend class RefCountedPtr;

  void IncrementRefCount() { grpc_core::Ref(&refs_); }

  gpr_refcount refs_;
};

// An alternative version of the RefCounted base class that
// supports tracing.  This is intended to be used in cases where the
// object will be handled both by idiomatic C++ code using smart
// pointers and legacy code that is manually calling Ref() and Unref().
// Once all of our code is converted to idiomatic C++, we may be able to
// eliminate this class.
template <typename Child>
class RefCountedWithTracing {
 public:
  RefCountedPtr<Child> Ref() GRPC_MUST_USE_RESULT {
    IncrementRefCount();
    return RefCountedPtr<Child>(static_cast<Child*>(this));
  }

  RefCountedPtr<Child> Ref(const DebugLocation& location,
                           const char* reason) GRPC_MUST_USE_RESULT {
    if (location.Log() && trace_flag_ != nullptr && trace_flag_->enabled()) {
      gpr_atm old_refs = gpr_atm_no_barrier_load(&refs_.count);
      gpr_log(GPR_INFO, "%s:%p %s:%d ref %" PRIdPTR " -> %" PRIdPTR " %s",
              trace_flag_->name(), this, location.file(), location.line(),
              old_refs, old_refs + 1, reason);
    }
    return Ref();
  }

  // TODO(roth): Once all of our code is converted to C++ and can use
  // RefCountedPtr<> instead of manual ref-counting, make the Unref() methods
  // private, since they will only be used by RefCountedPtr<>, which is a
  // friend of this class.

  void Unref() {
    if (grpc_core::Unref(&refs_)) {
      Delete(static_cast<Child*>(this));
    }
  }

  void Unref(const DebugLocation& location, const char* reason) {
    if (location.Log() && trace_flag_ != nullptr && trace_flag_->enabled()) {
      gpr_atm old_refs = gpr_atm_no_barrier_load(&refs_.count);
      gpr_log(GPR_INFO, "%s:%p %s:%d unref %" PRIdPTR " -> %" PRIdPTR " %s",
              trace_flag_->name(), this, location.file(), location.line(),
              old_refs, old_refs - 1, reason);
    }
    Unref();
  }

  // Not copyable nor movable.
  RefCountedWithTracing(const RefCountedWithTracing&) = delete;
  RefCountedWithTracing& operator=(const RefCountedWithTracing&) = delete;

  GRPC_ABSTRACT_BASE_CLASS

 protected:
  GPRC_ALLOW_CLASS_TO_USE_NON_PUBLIC_DELETE

  RefCountedWithTracing()
      : RefCountedWithTracing(static_cast<TraceFlag*>(nullptr)) {}

  explicit RefCountedWithTracing(TraceFlag* trace_flag)
      : trace_flag_(trace_flag) {
    grpc_core::RefInit(&refs_, 1);
  }

#ifdef NDEBUG
  explicit RefCountedWithTracing(DebugOnlyTraceFlag* trace_flag)
      : RefCountedWithTracing() {}
#endif

  virtual ~RefCountedWithTracing() {}

 private:
  // Allow RefCountedPtr<> to access IncrementRefCount().
  template <typename T>
  friend class RefCountedPtr;

  void IncrementRefCount() { grpc_core::Ref(&refs_); }

  TraceFlag* trace_flag_ = nullptr;
  gpr_refcount refs_;
};

}  // namespace grpc_core

#endif /* GRPC_CORE_LIB_GPRPP_REF_COUNTED_H */

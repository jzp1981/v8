// Copyright 2007-2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_COUNTERS_H_
#define V8_COUNTERS_H_

#include <wchar.h>

namespace v8 { namespace internal {

// StatsCounters is an interface for plugging into external
// counters for monitoring.  Counters can be looked up and
// manipulated by name.

class StatsTable : public AllStatic {
 public:
  // Register an application-defined function where
  // counters can be looked up.
  static void SetCounterFunction(CounterLookupCallback f) {
    lookup_function_ = f;
  }

  static bool HasCounterFunction() {
    return lookup_function_ != NULL;
  }

  // Lookup the location of a counter by name.  If the lookup
  // is successful, returns a non-NULL pointer for writing the
  // value of the counter.  Each thread calling this function
  // may receive a different location to store it's counter.
  // The return value must not be cached and re-used across
  // threads, although a single thread is free to cache it.
  static int *FindLocation(const wchar_t* name) {
    if (!lookup_function_) return NULL;
    return lookup_function_(name);
  }

 private:
  static CounterLookupCallback lookup_function_;
};

// StatsCounters are dynamically created values which can be tracked in
// the StatsTable.  They are designed to be lightweight to create and
// easy to use.
//
// Internally, a counter represents a value in a row of a StatsTable.
// The row has a 32bit value for each process/thread in the table and also
// a name (stored in the table metadata).  Since the storage location can be
// thread-specific, this class cannot be shared across threads.
//
// This class is designed to be POD initialized.  It will be registered with
// the counter system on first use.  For example:
//   StatsCounter c = { L"c:myctr", NULL, false };
struct StatsCounter {
  const wchar_t* name_;
  int* ptr_;
  bool lookup_done_;

  // Sets the counter to a specific value.
  void Set(int value) {
    int* loc = GetPtr();
    if (loc) *loc = value;
  }

  // Increments the counter.
  void Increment() {
    int* loc = GetPtr();
    if (loc) (*loc)++;
  }

  void Increment(int value) {
    int* loc = GetPtr();
    if (loc)
      (*loc) += value;
  }

  // Decrements the counter.
  void Decrement() {
    int* loc = GetPtr();
    if (loc) (*loc)--;
  }

  void Decrement(int value) {
    int* loc = GetPtr();
    if (loc) (*loc) -= value;
  }

  // Is this counter enabled?
  // Returns false if table is full.
  bool Enabled() {
    return GetPtr() != NULL;
  }

  // Get the internal pointer to the counter. This is used
  // by the code generator to emit code that manipulates a
  // given counter without calling the runtime system.
  int* GetInternalPointer() {
    int* loc = GetPtr();
    ASSERT(loc != NULL);
    return loc;
  }

 protected:
  // Returns the cached address of this counter location.
  int* GetPtr() {
    if (lookup_done_)
      return ptr_;
    lookup_done_ = true;
    ptr_ = StatsTable::FindLocation(name_);
    return ptr_;
  }
};

// StatsCounterTimer t = { { L"t:foo", NULL, false }, 0, 0 };
struct StatsCounterTimer {
  StatsCounter counter_;

  int64_t start_time_;
  int64_t stop_time_;

  // Start the timer.
  void Start();

  // Stop the timer and record the results.
  void Stop();

  // Returns true if the timer is running.
  bool Running() {
    return counter_.Enabled() && start_time_ != 0 && stop_time_ == 0;
  }
};

// A StatsRate is a combination of both a timer and a counter so that
// several statistics can be produced:
//    min, max, avg, count, total
//
// For example:
//   StatsCounter c = { { { L"t:myrate", NULL, false }, 0, 0 },
//                      { L"c:myrate", NULL, false } };
struct StatsRate {
  StatsCounterTimer timer_;
  StatsCounter counter_;

  // Starts the rate timer.
  void Start() {
    timer_.Start();
  }

  // Stops the rate and records the time.
  void Stop() {
    if (timer_.Running()) {
      timer_.Stop();
      counter_.Increment();
    }
  }
};


// Helper class for scoping a rate.
class StatsRateScope BASE_EMBEDDED {
 public:
  explicit StatsRateScope(StatsRate* rate) :
      rate_(rate) {
    rate_->Start();
  }
  ~StatsRateScope() {
    rate_->Stop();
  }
 private:
  StatsRate* rate_;
};


} }  // namespace v8::internal

#endif  // V8_COUNTERS_H_

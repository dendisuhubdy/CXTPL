﻿// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_default.h"

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"

#if defined(OS_MACOSX)
#include <mach/thread_policy.h>

#include "base/mac/mach_logging.h"
#include "base/mac/scoped_mach_port.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

namespace base {

MessagePumpDefault::MessagePumpDefault()
    : keep_running_(true),
      event_(WaitableEvent::ResetPolicy::AUTOMATIC,
             WaitableEvent::InitialState::NOT_SIGNALED) {
  event_.declare_only_used_while_idle();
}

MessagePumpDefault::~MessagePumpDefault() = default;

void MessagePumpDefault::Run(Delegate* delegate) {
  AutoReset<bool> auto_reset_keep_running(&keep_running_, true);
//P_LOG("MessagePumpDefault::Run 1\n");
#if defined(OS_EMSCRIPTEN)
  DCHECK(delegate);
#endif

#if defined(OS_EMSCRIPTEN) && defined(DISABLE_PTHREADS) && defined(HAVE_ASYNC)
  for (;;) {
#elif defined(OS_EMSCRIPTEN) && defined(DISABLE_PTHREADS) && !defined(HAVE_ASYNC)
  #warning "TODO: port MessagePumpDefault"
  /// \TODO endless loop may hang browser!
  for (int i = 0; i < 2; i++) {
#elif defined(DISABLE_PTHREADS)
  #error "TODO: port MessagePumpDefault"
#else
  for (;;) {
#endif
#if defined(OS_MACOSX)
    mac::ScopedNSAutoreleasePool autorelease_pool;
#endif

//P_LOG("MessagePumpDefault::Run 1.1\n");
    Delegate::NextWorkInfo next_work_info = delegate->DoSomeWork();
    bool has_more_immediate_work = next_work_info.is_immediate();
    if (!keep_running_)
      break;

//P_LOG("MessagePumpDefault::Run 1.2\n");
    if (has_more_immediate_work)
      continue;

//P_LOG("MessagePumpDefault::Run 1.3\n");
    has_more_immediate_work = delegate->DoIdleWork();
    if (!keep_running_)
      break;

    if (has_more_immediate_work)
      continue;

#if !(defined(OS_EMSCRIPTEN) && defined(DISABLE_PTHREADS))
//P_LOG("MessagePumpDefault::Run 1.4\n");
    if (next_work_info.delayed_run_time.is_max()) {
//P_LOG("MessagePumpDefault::Run 1.4.1\n");
      event_.Wait();
    } else {
//P_LOG("MessagePumpDefault::Run 1.4.2\n");
      event_.TimedWait(next_work_info.remaining_delay());
    }
#endif
//P_LOG("MessagePumpDefault::Run 1.5\n");
    // Since event_ is auto-reset, we don't need to do anything special here
    // other than service each delegate method.
  }
//P_LOG("MessagePumpDefault::Run 2\n");
}

void MessagePumpDefault::Quit() {
  keep_running_ = false;
}

void MessagePumpDefault::ScheduleWork() {
  // Since this can be called on any thread, we need to ensure that our Run
  // loop wakes up.
  event_.Signal();
}

void MessagePumpDefault::ScheduleDelayedWork(
    const TimeTicks& delayed_work_time) {
  // Since this is always called from the same thread as Run(), there is nothing
  // to do as the loop is already running. It will wait in Run() with the
  // correct timeout when it's out of immediate tasks.
  // TODO(gab): Consider removing ScheduleDelayedWork() when all pumps function
  // this way (bit.ly/merge-message-pump-do-work).
}

#if defined(OS_MACOSX)
void MessagePumpDefault::SetTimerSlack(TimerSlack timer_slack) {
  thread_latency_qos_policy_data_t policy{};
  policy.thread_latency_qos_tier = timer_slack == TIMER_SLACK_MAXIMUM
                                       ? LATENCY_QOS_TIER_3
                                       : LATENCY_QOS_TIER_UNSPECIFIED;
  mac::ScopedMachSendRight thread_port(mach_thread_self());
  kern_return_t kr =
      thread_policy_set(thread_port.get(), THREAD_LATENCY_QOS_POLICY,
                        reinterpret_cast<thread_policy_t>(&policy),
                        THREAD_LATENCY_QOS_POLICY_COUNT);
  MACH_DVLOG_IF(1, kr != KERN_SUCCESS, kr) << "thread_policy_set";
}
#endif

}  // namespace base
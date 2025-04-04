/*
 * Copyright (c) 2001, 2025, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#include "gc/shared/concurrentGCThread.hpp"
#include "runtime/atomic.hpp"
#include "runtime/init.hpp"
#include "runtime/jniHandles.hpp"
#include "runtime/mutexLocker.hpp"
#include "runtime/os.hpp"

ConcurrentGCThread::ConcurrentGCThread() :
    _should_terminate(false),
    _has_terminated(false) {}

void ConcurrentGCThread::create_and_start(ThreadPriority prio) {
  if (os::create_thread(this, os::gc_thread)) {
    os::set_priority(this, prio);
    os::start_thread(this);
  }
}

void ConcurrentGCThread::run() {
  // Wait for initialization to complete
  wait_init_completed();

  run_service();

  // Signal thread has terminated
  MonitorLocker ml(Terminator_lock);
  Atomic::release_store(&_has_terminated, true);
  ml.notify_all();
}

void ConcurrentGCThread::stop() {
  assert(!should_terminate(), "Invalid state");
  assert(!has_terminated(), "Invalid state");

  // Signal thread to terminate
  Atomic::release_store_fence(&_should_terminate, true);

  stop_service();

  // Wait for thread to terminate
  MonitorLocker ml(Terminator_lock);
  while (!_has_terminated) {
    ml.wait();
  }
}

bool ConcurrentGCThread::should_terminate() const {
  return Atomic::load_acquire(&_should_terminate);
}

bool ConcurrentGCThread::has_terminated() const {
  return Atomic::load_acquire(&_has_terminated);
}

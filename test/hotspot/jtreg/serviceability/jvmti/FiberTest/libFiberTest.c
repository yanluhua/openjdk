/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
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
 */

#include <string.h>
#include "jvmti.h"

#ifdef __cplusplus
extern "C" {
#endif

static jvmtiEnv *jvmti = NULL;
static jrawMonitorID events_monitor = NULL;

static void lock_events() {
  (*jvmti)->RawMonitorEnter(jvmti, events_monitor);
}

static void unlock_events() {
  (*jvmti)->RawMonitorExit(jvmti, events_monitor);
}

static void
print_event_info(jvmtiEnv *jvmti, JNIEnv* jni, jthread thread, jobject fiber, char* event_name) {
  jvmtiThreadInfo thr_info;
  jvmtiError err = (*jvmti)->GetThreadInfo(jvmti, thread, &thr_info);

  if (err != JVMTI_ERROR_NONE) {
    (*jni)->FatalError(jni, "event handler failed during JVMTI GetThreadInfo call");
  }
  char* thr_name = (thr_info.name == NULL) ? "<Unnamed thread>" : thr_info.name;
  printf("\n%s event: event thread: %s, fiber: %p\n", event_name, thr_name, fiber);
}

static void
test_IsFiber(jvmtiEnv *jvmti, JNIEnv* jni, jthread thread, jobject fiber, char* event_name) {
  jboolean is_fiber = JNI_FALSE;
  jvmtiError err;

  printf("\n");

  // #1: Test JVMTI IsFiber function with NULL fiber

  err = (*jvmti)->IsFiber(jvmti, NULL, &is_fiber);
  if (err != JVMTI_ERROR_NONE) {
    (*jni)->FatalError(jni, "event handler: failed during JVMTI IsFiber call");
  }
  if (is_fiber != JNI_FALSE) {
    (*jni)->FatalError(jni, "event handler: JVMTI IsFiber with NULL fiber failed to return JNI_FALSE");
  }
  printf("%s event: JVMTI IsFiber with NULL fiber returned JNI_FALSE as expected\n", event_name);

  // #2: Test JVMTI IsFiber function with a bad fiber

  err = (*jvmti)->IsFiber(jvmti, thread, &is_fiber);
  if (err != JVMTI_ERROR_NONE) {
    (*jni)->FatalError(jni, "event handler: failed during JVMTI IsFiber call");
  }
  if (is_fiber != JNI_FALSE) {
    (*jni)->FatalError(jni, "event handler: JVMTI IsFiber with bad fiber failed to return JNI_FALSE");
  }
  printf("%s event: JVMTI IsFiber with bad fiber returned JNI_FALSE as expected\n", event_name);

  // #3: Test JVMTI IsFiber function with a good fiber

  err = (*jvmti)->IsFiber(jvmti, fiber, &is_fiber);
  if (err != JVMTI_ERROR_NONE) {
    (*jni)->FatalError(jni, "event handler: failed during JVMTI IsFiber call");
  }
  if (is_fiber != JNI_TRUE) {
    (*jni)->FatalError(jni, "event handler: JVMTI IsFiber with good fiber failed to return JNI_TRUE");
  }
  printf("%s event: JVMTI IsFiber with good fiber returned JNI_TRUE as expected\n", event_name);
}

static void
test_GetThreadFiber(jvmtiEnv *jvmti, JNIEnv* jni, jthread thread, jobject fiber, char* event_name) {
  jobject thread_fiber = NULL;
  jvmtiError err;

  printf("\n");

  // #1: Test JVMTI GetThreadFiber function NULL thread (current)

  err = (*jvmti)->GetThreadFiber(jvmti, NULL, &thread_fiber);
  if (err != JVMTI_ERROR_NONE) {
    (*jni)->FatalError(jni, "event handler: JVMTI GetThreadFiber with NULL thread (current) returned error status");
  }
  if (thread_fiber == NULL) {
    (*jni)->FatalError(jni, "event handler: JVMTI GetThreadFiber with NULL thread (current) failed to return non-NULL fiber");
  }
  printf("%s event: JVMTI GetThreadFiber with NULL thread (current) returned non-NULL fiber as expected\n", event_name);

  // #2: Test JVMTI GetThreadFiber function a bad thread

  err = (*jvmti)->GetThreadFiber(jvmti, fiber, &thread_fiber);
  if (err == JVMTI_ERROR_NONE) {
    (*jni)->FatalError(jni, "event handler: JVMTI GetThreadFiber with bad thread failed to return JVMTI_ERROR_INVALID_FIBER");
  }
  printf("%s event: JVMTI GetThreadFiber with bad thread returned JVMTI_ERROR_INVALID_THREAD as expected\n", event_name);

  // #3: Test JVMTI GetThreadFiber function with a good thread

  err = (*jvmti)->GetThreadFiber(jvmti, thread, &thread_fiber);
  if (err != JVMTI_ERROR_NONE) {
    (*jni)->FatalError(jni, "event handler: failed during JVMTI GetThreadFiber call");
  }
  if (thread_fiber == NULL) {
    (*jni)->FatalError(jni, "event handler: JVMTI GetThreadFiber with good thread failed to return non-NULL fiber");
  }
  printf("%s event: JVMTI GetThreadFiber with good thread returned non-NULL fiber as expected\n", event_name);
}

static void
test_GetFiberThread(jvmtiEnv *jvmti, JNIEnv* jni, jthread thread, jobject fiber, char* event_name) {
  jthread fiber_thread = NULL;
  jvmtiError err;

  printf("\n");

  // #1: Test JVMTI GetFiberThread function with NULL fiber

  err = (*jvmti)->GetFiberThread(jvmti, NULL, &fiber_thread);
  if (err == JVMTI_ERROR_NONE) {
    (*jni)->FatalError(jni, "event handler: JVMTI GetFiberThread with NULL fiber failed to return JVMTI_ERROR_INVALID_THREAD");
  }
  printf("%s event: JVMTI GetFiberThread with NULL fiber returned JVMTI_ERROR_INVALID_THREAD as expected\n", event_name);

  // #2: Test JVMTI GetFiberThread function with a bad fiber

  err = (*jvmti)->GetFiberThread(jvmti, thread, &fiber_thread);
  if (err == JVMTI_ERROR_NONE) {
    (*jni)->FatalError(jni, "event handler: JVMTI GetFiberThread with bad fiber failed to return JVMTI_ERROR_INVALID_THREAD");
  }
  printf("%s event: JVMTI GetFiberThread with bad fiber returned JVMTI_ERROR_INVALID_THREAD as expected\n", event_name);

  // #3: Test JVMTI GetFiberThread function with a good fiber

  err = (*jvmti)->GetFiberThread(jvmti, fiber, &fiber_thread);
  if (err != JVMTI_ERROR_NONE) {
    (*jni)->FatalError(jni, "event handler: failed during JVMTI GetFiberThread call");
  }
  if (fiber_thread == NULL) {
    (*jni)->FatalError(jni, "event handler: JVMTI GetFiberThread with good fiber failed to return non-NULL carrier thread");
  }
  printf("%s event: JVMTI GetFiberThread with good fiber returned non-NULL career thread as expected\n", event_name);
}

static void
processFiberEvent(jvmtiEnv *jvmti, JNIEnv* jni, jthread thread, jobject fiber, char* event_name) {
  static int events_cnt = 0;

  if (events_cnt++ > 20 && strcmp(event_name, "FiberTerminated") != 0) {
    return; // No need to test all events
  }

  print_event_info(jvmti, jni, thread, fiber, event_name);
  test_IsFiber(jvmti, jni, thread, fiber, event_name);

  if (strcmp(event_name, "FiberScheduled") == 0 || strcmp(event_name, "FiberTerminated") == 0) {
    return; // skip further testing for FiberScheduled and FiberTerminated events
  }

  test_GetThreadFiber(jvmti, jni, thread, fiber, event_name);
  test_GetFiberThread(jvmti, jni, thread, fiber, event_name);
}

static void JNICALL
FiberScheduled(jvmtiEnv *jvmti, JNIEnv* jni, jthread thread, jobject fiber) {
  jobject mounted_fiber = NULL;
  jvmtiError err;

  lock_events();

  processFiberEvent(jvmti, jni, thread, fiber, "FiberScheduled");

  err = (*jvmti)->GetThreadFiber(jvmti, thread, &mounted_fiber);
  if (err != JVMTI_ERROR_NONE) {
    (*jni)->FatalError(jni, "FiberScheduled event handler: failed during JVMTI GetThreadFiber call");
  }
  if (!(*jni)->IsSameObject(jni, mounted_fiber, fiber)) {
    (*jni)->FatalError(jni, "FiberScheduled event handler: JVMTI GetThreadFiber failed to return proper fiber");
  }

  unlock_events();
}

static void JNICALL
FiberTerminated(jvmtiEnv *jvmti, JNIEnv* jni, jthread thread, jobject fiber) {
  jobject mounted_fiber = NULL;
  jvmtiError err;

  lock_events();

  processFiberEvent(jvmti, jni, thread, fiber, "FiberTerminated");

  err = (*jvmti)->GetThreadFiber(jvmti, thread, &mounted_fiber);
  if (err != JVMTI_ERROR_NONE) {
    (*jni)->FatalError(jni, "FiberTerminated event handler: failed during JVMTI GetThreadFiber call");
  }
  if (mounted_fiber != NULL) {
    (*jni)->FatalError(jni, "FiberTerminated event handler: JVMTI GetThreadFiber failed to return NULL fiber");
  }

  unlock_events();
}

static void JNICALL
FiberMount(jvmtiEnv *jvmti, JNIEnv* jni, jthread thread, jobject fiber) {
  lock_events();
  processFiberEvent(jvmti, jni, thread, fiber, "FiberMount");
  unlock_events();
}

static void JNICALL
FiberUnmount(jvmtiEnv *jvmti, JNIEnv* jni, jthread thread, jobject fiber) {
  lock_events();
  processFiberEvent(jvmti, jni, thread, fiber, "FiberUnmount");
  unlock_events();
}

extern JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *jvm, char *options,
                                           void *reserved) {
  jvmtiEventCallbacks callbacks;
  jvmtiCapabilities caps;
  jvmtiError err;

  printf("Agent_OnLoad started\n");
  if ((*jvm)->GetEnv(jvm, (void **) (&jvmti), JVMTI_VERSION) != JNI_OK) {
    return JNI_ERR;
  }

  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.FiberScheduled  = &FiberScheduled;
  callbacks.FiberTerminated = &FiberTerminated;
  callbacks.FiberMount   = &FiberMount;
  callbacks.FiberUnmount = &FiberUnmount;

  memset(&caps, 0, sizeof(caps));
  caps.can_support_fibers = 1;
  err = (*jvmti)->AddCapabilities(jvmti, &caps);
  if (err != JVMTI_ERROR_NONE) {
    printf("error in JVMTI AddCapabilities: %d\n", err);
  }

  err = (*jvmti)->SetEventCallbacks(jvmti, &callbacks, sizeof(jvmtiEventCallbacks));
  if (err != JVMTI_ERROR_NONE) {
    printf("error in JVMTI SetEventCallbacks: %d\n", err);
  }

  err = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_FIBER_SCHEDULED, NULL);
  if (err != JVMTI_ERROR_NONE) {
    printf("error in JVMTI SetEventNotificationMode: %d\n", err);
  }

  err = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_FIBER_TERMINATED, NULL);
  if (err != JVMTI_ERROR_NONE) {
    printf("error in JVMTI SetEventNotificationMode: %d\n", err);
  }

  err = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_FIBER_MOUNT, NULL);
  if (err != JVMTI_ERROR_NONE) {
    printf("error in JVMTI SetEventNotificationMode: %d\n", err);
  }

  err = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_FIBER_UNMOUNT, NULL);
  if (err != JVMTI_ERROR_NONE) {
    printf("error in JVMTI SetEventNotificationMode: %d\n", err);
  }

  (*jvmti)->CreateRawMonitor(jvmti, "Events Monitor", &events_monitor);
  printf("Agent_OnLoad finished\n");
  return 0;
}

#ifdef __cplusplus
}
#endif

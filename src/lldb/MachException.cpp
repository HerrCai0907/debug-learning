//===-- MachException.cpp ---------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  Created by Greg Clayton on 6/18/07.
//
//===----------------------------------------------------------------------===//

#include "MachException.h"
#include "DNBError.h"
#include "MachProcess.h"
#include "MachTask.h"
#include <cerrno>
#include <sys/ptrace.h>
#include <sys/types.h>

// Routine mach_exception_raise
extern "C" kern_return_t catch_mach_exception_raise(mach_port_t exception_port, mach_port_t thread, mach_port_t task,
                                                    exception_type_t exception, mach_exception_data_t code,
                                                    mach_msg_type_number_t codeCnt);

extern "C" kern_return_t catch_mach_exception_raise_state(mach_port_t exception_port, exception_type_t exception,
                                                          const mach_exception_data_t code,
                                                          mach_msg_type_number_t codeCnt, int *flavor,
                                                          const thread_state_t old_state,
                                                          mach_msg_type_number_t old_stateCnt, thread_state_t new_state,
                                                          mach_msg_type_number_t *new_stateCnt);

// Routine mach_exception_raise_state_identity
extern "C" kern_return_t catch_mach_exception_raise_state_identity(
    mach_port_t exception_port, mach_port_t thread, mach_port_t task, exception_type_t exception,
    mach_exception_data_t code, mach_msg_type_number_t codeCnt, int *flavor, thread_state_t old_state,
    mach_msg_type_number_t old_stateCnt, thread_state_t new_state, mach_msg_type_number_t *new_stateCnt);

extern "C" boolean_t mach_exc_server(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP);

// Note: g_message points to the storage allocated to catch the data from
// catching the current exception raise. It's populated when we catch a raised
// exception which can't immediately be replied to.
//
// If it becomes possible to catch exceptions from multiple threads
// simultaneously, accesses to g_message would need to be mutually exclusive.
static MachException::Data *g_message = NULL;

extern "C" kern_return_t catch_mach_exception_raise_state(mach_port_t exc_port, exception_type_t exc_type,
                                                          const mach_exception_data_t exc_data,
                                                          mach_msg_type_number_t exc_data_count, int *flavor,
                                                          const thread_state_t old_state,
                                                          mach_msg_type_number_t old_stateCnt, thread_state_t new_state,
                                                          mach_msg_type_number_t *new_stateCnt) {
  return KERN_FAILURE;
}

extern "C" kern_return_t catch_mach_exception_raise_state_identity(
    mach_port_t exc_port, mach_port_t thread_port, mach_port_t task_port, exception_type_t exc_type,
    mach_exception_data_t exc_data, mach_msg_type_number_t exc_data_count, int *flavor, thread_state_t old_state,
    mach_msg_type_number_t old_stateCnt, thread_state_t new_state, mach_msg_type_number_t *new_stateCnt) {
  return KERN_FAILURE;
}

extern "C" kern_return_t catch_mach_exception_raise(mach_port_t exc_port, mach_port_t thread_port,
                                                    mach_port_t task_port, exception_type_t exc_type,
                                                    mach_exception_data_t exc_data,
                                                    mach_msg_type_number_t exc_data_count) {
  g_message->exc_type = 0;
  g_message->exc_data.clear();

  if (task_port == g_message->task_port) {
    g_message->task_port = task_port;
    g_message->thread_port = thread_port;
    g_message->exc_type = exc_type;
    g_message->AppendExceptionData(exc_data, exc_data_count);
    return KERN_SUCCESS;
  } else if (!MachTask::IsValid(g_message->task_port)) {
    // Our original exception port isn't valid anymore check for a SIGTRAP
    if (exc_type == EXC_SOFTWARE && exc_data_count == 2 && exc_data[0] == EXC_SOFT_SIGNAL && exc_data[1] == SIGTRAP) {
      // We got a SIGTRAP which indicates we might have exec'ed and possibly
      // lost our old task port during the exec, so we just need to switch over
      // to using this new task port
      g_message->task_port = task_port;
      g_message->thread_port = thread_port;
      g_message->exc_type = exc_type;
      g_message->AppendExceptionData(exc_data, exc_data_count);
      return KERN_SUCCESS;
    }
  }
  return KERN_FAILURE;
}

kern_return_t MachException::Message::Receive(mach_port_t port, mach_msg_option_t options, mach_msg_timeout_t timeout,
                                              mach_port_t notify_port) {
  DNBError err;
  mach_msg_timeout_t mach_msg_timeout = options & MACH_RCV_TIMEOUT ? timeout : 0;
  err = ::mach_msg(&exc_msg.hdr,
                   options,              // options
                   0,                    // Send size
                   sizeof(exc_msg.data), // Receive size
                   port,                 // exception port to watch for exception on
                   mach_msg_timeout,     // timeout in msec (obeyed only if
                                         // MACH_RCV_TIMEOUT is ORed into the
                                         // options parameter)
                   notify_port);
  return err.Status();
}

bool MachException::Message::CatchExceptionRaise(task_t task) {
  bool success = false;
  state.task_port = task;
  g_message = &state;
  // The exc_server function is the MIG generated server handling function
  // to handle messages from the kernel relating to the occurrence of an
  // exception in a thread. Such messages are delivered to the exception port
  // set via thread_set_exception_ports or task_set_exception_ports. When an
  // exception occurs in a thread, the thread sends an exception message to
  // its exception port, blocking in the kernel waiting for the receipt of a
  // reply. The exc_server function performs all necessary argument handling
  // for this kernel message and calls catch_exception_raise,
  // catch_exception_raise_state or catch_exception_raise_state_identity,
  // which should handle the exception. If the called routine returns
  // KERN_SUCCESS, a reply message will be sent, allowing the thread to
  // continue from the point of the exception; otherwise, no reply message
  // is sent and the called routine must have dealt with the exception
  // thread directly.
  if (mach_exc_server(&exc_msg.hdr, &reply_msg.hdr)) { success = true; }
  g_message = NULL;
  return success;
}

kern_return_t MachException::Message::Reply(MachProcess *process, int signal) {
  // Reply to the exception...
  DNBError err;

  // If we had a soft signal, we need to update the thread first so it can
  // continue without signaling
  int soft_signal = state.SoftSignal();
  if (soft_signal) {
    int state_pid = -1;
    if (process->Task().TaskPort() == state.task_port) {
      // This is our task, so we can update the signal to send to it
      state_pid = process->ProcessID();
      soft_signal = signal;
    } else {
      err = ::pid_for_task(state.task_port, &state_pid);
    }

    assert(state_pid != -1);
    if (state_pid != -1) {
      errno = 0;
      if (::ptrace(PT_THUPDATE, state_pid, (caddr_t)((uintptr_t)state.thread_port), soft_signal) != 0)
        err.SetError(errno, DNBError::POSIX);
      else
        err.Clear();
    }
  }

  err = ::mach_msg(&reply_msg.hdr, MACH_SEND_MSG | MACH_SEND_INTERRUPT, reply_msg.hdr.msgh_size, 0, MACH_PORT_NULL,
                   MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  return err.Status();
}

// The EXC_MASK_ALL value hard-coded here so that lldb can be built
// on a new OS with an older deployment target .  The new OS may have
// an addition to its EXC_MASK_ALL that the old OS will not recognize -
// <mach/exception_types.h> doesn't vary the value based on the deployment
// target.  So we need a known set of masks that can be assumed to be
// valid when running on an older OS.  We'll fall back to trying
// PREV_EXC_MASK_ALL if the EXC_MASK_ALL value lldb was compiled with is
// not recognized.

#define PREV_EXC_MASK_ALL                                                                                              \
  (EXC_MASK_BAD_ACCESS | EXC_MASK_BAD_INSTRUCTION | EXC_MASK_ARITHMETIC | EXC_MASK_EMULATION | EXC_MASK_SOFTWARE |     \
   EXC_MASK_BREAKPOINT | EXC_MASK_SYSCALL | EXC_MASK_MACH_SYSCALL | EXC_MASK_RPC_ALERT | EXC_MASK_RESOURCE |           \
   EXC_MASK_GUARD | EXC_MASK_MACHINE)

#define LLDB_EXC_MASK EXC_MASK_ALL

kern_return_t MachException::PortInfo::Save(task_t task) {
  // Be careful to be able to have debugserver built on a newer OS than what
  // it is currently running on by being able to start with all exceptions
  // and back off to just what is supported on the current system
  DNBError err;

  mask = LLDB_EXC_MASK;

  count = (sizeof(ports) / sizeof(ports[0]));
  err = ::task_get_exception_ports(task, mask, masks, &count, ports, behaviors, flavors);

  if (err.Status() == KERN_INVALID_ARGUMENT && mask != PREV_EXC_MASK_ALL) {
    mask = PREV_EXC_MASK_ALL;
    count = (sizeof(ports) / sizeof(ports[0]));
    err = ::task_get_exception_ports(task, mask, masks, &count, ports, behaviors, flavors);
  }
  if (err.Fail()) {
    mask = 0;
    count = 0;
  }
  return err.Status();
}

kern_return_t MachException::PortInfo::Restore(task_t task) {
  uint32_t i = 0;
  DNBError err;
  if (count > 0) {
    for (i = 0; i < count; i++) {
      err = ::task_set_exception_ports(task, masks[i], ports[i], behaviors[i], flavors[i]);
      if (err.Fail()) break;
    }
  }
  count = 0;
  return err.Status();
}

const char *MachException::Name(exception_type_t exc_type) {
  switch (exc_type) {
  case EXC_BAD_ACCESS: return "EXC_BAD_ACCESS";
  case EXC_BAD_INSTRUCTION: return "EXC_BAD_INSTRUCTION";
  case EXC_ARITHMETIC: return "EXC_ARITHMETIC";
  case EXC_EMULATION: return "EXC_EMULATION";
  case EXC_SOFTWARE: return "EXC_SOFTWARE";
  case EXC_BREAKPOINT: return "EXC_BREAKPOINT";
  case EXC_SYSCALL: return "EXC_SYSCALL";
  case EXC_MACH_SYSCALL: return "EXC_MACH_SYSCALL";
  case EXC_RPC_ALERT: return "EXC_RPC_ALERT";
#ifdef EXC_CRASH
  case EXC_CRASH: return "EXC_CRASH";
#endif
  case EXC_RESOURCE: return "EXC_RESOURCE";
#ifdef EXC_GUARD
  case EXC_GUARD: return "EXC_GUARD";
#endif
#ifdef EXC_CORPSE_NOTIFY
  case EXC_CORPSE_NOTIFY: return "EXC_CORPSE_NOTIFY";
#endif
#ifdef EXC_CORPSE_VARIANT_BIT
  case EXC_CORPSE_VARIANT_BIT: return "EXC_CORPSE_VARIANT_BIT";
#endif
  default: break;
  }
  return NULL;
}

// Returns the exception mask for a given exception name.
// 0 is not a legit mask, so we return that in the case of an error.
exception_mask_t MachException::ExceptionMask(const char *name) {
  static const char *exception_prefix = "EXC_";
  static const int prefix_len = strlen(exception_prefix);

  // All mach exceptions start with this prefix:
  if (strstr(name, exception_prefix) != name) return 0;

  name += prefix_len;
  std::string name_str = name;
  if (name_str == "BAD_ACCESS") return EXC_MASK_BAD_ACCESS;
  if (name_str == "BAD_INSTRUCTION") return EXC_MASK_BAD_INSTRUCTION;
  if (name_str == "ARITHMETIC") return EXC_MASK_ARITHMETIC;
  if (name_str == "EMULATION") return EXC_MASK_EMULATION;
  if (name_str == "SOFTWARE") return EXC_MASK_SOFTWARE;
  if (name_str == "BREAKPOINT") return EXC_MASK_BREAKPOINT;
  if (name_str == "SYSCALL") return EXC_MASK_SYSCALL;
  if (name_str == "MACH_SYSCALL") return EXC_MASK_MACH_SYSCALL;
  if (name_str == "RPC_ALERT") return EXC_MASK_RPC_ALERT;
#ifdef EXC_CRASH
  if (name_str == "CRASH") return EXC_MASK_CRASH;
#endif
  if (name_str == "RESOURCE") return EXC_MASK_RESOURCE;
#ifdef EXC_GUARD
  if (name_str == "GUARD") return EXC_MASK_GUARD;
#endif
#ifdef EXC_CORPSE_NOTIFY
  if (name_str == "CORPSE_NOTIFY") return EXC_MASK_CORPSE_NOTIFY;
#endif
  return 0;
}

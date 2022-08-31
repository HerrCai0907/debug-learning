#include "MachProcess.h"
#include "DNBError.h"
#include <cassert>
#include <iostream>
#include <sys/ptrace.h>
#include <unistd.h>

pid_t MachProcess::AttachForDebug(pid_t pid, const IgnoredExceptions &ignored_exceptions, char *err_str,
                                  size_t err_len) {
  // Clear out and clean up from any current state
  // Clear();
  if (pid != 0) {
    DNBError err;
    // Make sure the process exists...
    if (::getpgid(pid) < 0) {
      err.SetErrorToErrno();
      const char *err_cstr = err.AsString();
      ::snprintf(err_str, err_len, "%s", err_cstr ? err_cstr : "No such process");
      return INVALID_NUB_PROCESS;
    }

    // SetState(eStateAttaching);
    m_pid = pid;
    if (!m_task.StartExceptionThread(ignored_exceptions, err)) {
      const char *err_cstr = err.AsString();
      ::snprintf(err_str, err_len, "%s", err_cstr ? err_cstr : "unable to start the exception thread");
      m_pid = INVALID_NUB_PROCESS;
      return INVALID_NUB_PROCESS;
    }

    errno = 0;
    int ptrace_result = ::ptrace(PT_ATTACHEXC, pid, 0, 0);
    int ptrace_errno = errno;
    if (ptrace_result != 0) {
      err.SetError(ptrace_errno);
    } else {
      err.Clear();
    }

    if (err.Success()) {
      // Sleep a bit to let the exception get received and set our process
      // status
      // to stopped.
      ::usleep(250000);
      return m_pid;
    } else {
      ::snprintf(err_str, err_len, "%s", err.AsString());
    }
  }
  return INVALID_NUB_PROCESS;
}

bool MachProcess::Detach() {
  // uint32_t thread_idx = UINT32_MAX;
  // nub_state_t state = DoSIGSTOP(true, true, &thread_idx);
  // ReplyToAllExceptions();
  // if DoSIGSTOP then not reply exceptions. otherwise reply 0
  m_task.ShutDownExcecptionThread();
  // Detach from our process
  errno = 0;
  ::ptrace(PT_DETACH, m_pid, (caddr_t)1, 0);
  DNBError err(errno, DNBError::POSIX);
  if (err.Fail()) {
    std::cout << err.AsString() << "\n";
    assert(false);
  }
  // Resume our task
  err = m_task.Resume();
  assert(err.Success());
  // Clear out any notion of the process we once were
  return true;
}

void MachProcess::ExceptionMessageReceived(const MachException::Message &exceptionMessage) {
  if (m_exception_messages.empty()) m_task.Suspend();
  // Use a locker to automatically unlock our mutex in case of exceptions
  // Add the exception to our internal exception stack
  m_exception_messages.push_back(exceptionMessage);
}

void MachProcess::ReplyToAllExceptions() {
  if (!m_exception_messages.empty()) {
    MachException::Message::iterator pos;
    MachException::Message::iterator begin = m_exception_messages.begin();
    MachException::Message::iterator end = m_exception_messages.end();
    for (pos = begin; pos != end; ++pos) {
      int thread_reply_signal = -1;
      DNBError err(pos->Reply(this, thread_reply_signal));
    }
    // Erase all exception message as we should have used and replied
    // to them all already.
    m_exception_messages.clear();
  }
}
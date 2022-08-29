#include "MachProcess.h"
#include "DNBError.h"
#include <sys/ptrace.h>

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

void MachProcess::ExceptionMessageReceived(const MachException::Message &exceptionMessage) {
  if (m_exception_messages.empty()) m_task.Suspend();
  // Use a locker to automatically unlock our mutex in case of exceptions
  // Add the exception to our internal exception stack
  m_exception_messages.push_back(exceptionMessage);
}
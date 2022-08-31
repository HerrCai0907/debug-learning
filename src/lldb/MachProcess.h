#pragma once

#include "DNBDefs.h"
#include "MachTask.h"
#include "ignoredExceptions.h"
#include <sys/types.h>
#include <unistd.h>

class MachProcess {
public:
  MachProcess() : m_task(this) {}
  pid_t ProcessID() const { return m_pid; }
  bool ProcessIDIsValid() const { return m_pid > 0; }
  pid_t SetProcessID(pid_t pid);
  MachTask &Task() { return m_task; }
  const MachTask &Task() const { return m_task; }

  pid_t AttachForDebug(pid_t pid, const IgnoredExceptions &ignored_exceptions, char *err_str, size_t err_len);
  bool Detach();

  nub_size_t ReadMemory(nub_addr_t addr, nub_size_t size, void *buf);

  void ExceptionMessageReceived(const MachException::Message &exceptionMessage);
  void ReplyToAllExceptions();

private:
  pid_t m_pid = INVALID_NUB_PROCESS;                         // Process ID of child process
  MachTask m_task;                                           // The mach task for this process
  MachException::Message::collection m_exception_messages{}; // A collection of exception messages caught when listening
                                                             // to the exception port
};
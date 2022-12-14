#pragma once

#include "DNBDefs.h"
#include "MachTask.h"
#include "ignoredExceptions.h"
#include <csignal>
#include <mach/mach_types.h>
#include <mach/message.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

class MachProcess {
public:
  enum ProcessStatus {
    DETACH,
    RUNNING,
    STOP,
  };
  MachProcess() : m_task(this) {}
  pid_t ProcessID() const { return m_pid; }
  bool ProcessIDIsValid() const { return m_pid > 0; }
  pid_t SetProcessID(pid_t pid);
  MachTask &Task() { return m_task; }
  const MachTask &Task() const { return m_task; }
  ProcessStatus Status() const { return m_status; }

  void Attach(pid_t pid, const IgnoredExceptions &ignored_exceptions);
  void Detach();
  void Resume();
  void Stop();
  void SingleStep();

  nub_size_t ReadMemory(nub_addr_t addr, nub_size_t size, void *buf);
  nub_size_t WriteMemory(nub_addr_t addr, nub_size_t size, const void *buf);

  std::vector<arm_thread_state64_t> ReadRegister();

  void ExceptionMessageReceived(const MachException::Message &exceptionMessage);

private:
  void Signal(int signal);

  void ReplyToAllExceptions();

private:
  ProcessStatus m_status = ProcessStatus::DETACH;
  pid_t m_pid = INVALID_NUB_PROCESS; // Process ID of child process
  MachTask m_task;                   // The mach task for this process
  MachException::Message::collection
      m_exception_messages{}; // A collection of exception messages caught when listening to the exception port
  std::atomic<bool> m_single_step = false;
};
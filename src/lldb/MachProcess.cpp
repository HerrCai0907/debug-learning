#include "MachProcess.h"
#include "DNBError.h"
#include "MachException.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/errno.h>
#include <sys/ptrace.h>
#include <sys/signal.h>
#include <unistd.h>

void MachProcess::Attach(pid_t pid, const IgnoredExceptions &ignored_exceptions) {
  assert(m_status == ProcessStatus::DETACH);
  if (pid == 0) { throw std::runtime_error("pid == 0"); }
  // Make sure the process exists...
  if (::getpgid(pid) < 0) { throw std::runtime_error("no such process"); }
  // SetState(eStateAttaching);
  m_pid = pid;
  DNBError err;
  if (!m_task.StartExceptionThread(ignored_exceptions, err)) {
    throw std::runtime_error(std::string("unable to start the exception thread due to ") + std::string(err.AsString()));
  }
  errno = 0;
  if (0 != ::ptrace(PT_ATTACHEXC, pid, 0, 0)) { throw std::runtime_error(::strerror(errno)); }
  // Sleep a bit to let the exception get received and set our process status to stopped.
  ::usleep(250000);
  m_status = ProcessStatus::STOP;
}
void MachProcess::Detach() {
  assert(m_status == ProcessStatus::RUNNING || m_status == ProcessStatus::STOP);
  DNBError err;
  if (m_status == ProcessStatus::RUNNING) { Stop(); }
  err = m_task.ShutDownExcecptionThread();
  if (err.Fail()) { throw std::runtime_error(err.AsString()); }
  // Detach from our process
  errno = 0;
  if (0 != ::ptrace(PT_DETACH, m_pid, (caddr_t)1, 0)) { throw std::runtime_error(::strerror(errno)); }
  m_status = ProcessStatus::DETACH;
  // Resume our task
  err = m_task.Resume();
  if (err.Fail()) { throw std::runtime_error(err.AsString()); }
}

void MachProcess::Resume() {
  assert(m_status == ProcessStatus::STOP);
  DNBError err;
  ReplyToAllExceptions();
  err = m_task.Resume();
  if (err.Fail()) { throw std::runtime_error(err.AsString()); }
  m_status = ProcessStatus::RUNNING;
}
void MachProcess::Stop() {
  assert(m_status == ProcessStatus::RUNNING);
  Signal(SIGSTOP);
  m_status = ProcessStatus::STOP;
}

void MachProcess::Signal(int signal) {
  DNBError err;
  errno = 0;
  if (0 != ::kill(ProcessID(), signal)) { throw std::runtime_error(::strerror(errno)); }
}

nub_size_t MachProcess::ReadMemory(nub_addr_t addr, nub_size_t size, void *buf) {
  assert(m_status == ProcessStatus::STOP);
  nub_size_t bytes_read = m_task.ReadMemory(addr, size, buf);
  return bytes_read;
}
nub_size_t MachProcess::WriteMemory(nub_addr_t addr, nub_size_t size, const void *buf) {
  assert(m_status == ProcessStatus::STOP);
  return m_task.WriteMemory(addr, size, buf);
}

void MachProcess::ExceptionMessageReceived(const MachException::Message &exceptionMessage) {
  if (m_exception_messages.empty()) m_task.Suspend();
  // Use a locker to automatically unlock our mutex in case of exceptions
  // Add the exception to our internal exception stack
  m_exception_messages.push_back(exceptionMessage);
}

void MachProcess::ReplyToAllExceptions() {
  assert(m_status == ProcessStatus::STOP);
  if (!m_exception_messages.empty()) {
    MachException::Message::iterator pos;
    MachException::Message::iterator begin = m_exception_messages.begin();
    MachException::Message::iterator end = m_exception_messages.end();
    for (pos = begin; pos != end; ++pos) {
      int thread_reply_signal = 0;
      DNBError err(pos->Reply(this, thread_reply_signal));
    }
    m_exception_messages.clear();
  }
}
#include "MachProcess.h"
#include "DNBError.h"
#include "MachException.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <mach/arm/thread_status.h>
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

void MachProcess::SingleStep() {
  assert(m_status == ProcessStatus::STOP);
  m_single_step = true;
  m_task.EnableSingleStep();
  Resume();
  while (m_single_step) {} // wait until m_single_step change to false
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

std::vector<arm_thread_state64_t> MachProcess::ReadRegister() {
  std::vector<arm_thread_state64_t> registers{};
  DNBError err;
  thread_array_t thread_list = NULL;
  mach_msg_type_number_t thread_list_count = 0;
  err = ::task_threads(m_task.TaskPort(), &thread_list, &thread_list_count);
  if (err.Fail()) { throw std::runtime_error(err.AsString()); }
  for (mach_msg_type_number_t i = 0; i < thread_list_count; i++) {
    thread_t thread = thread_list[i];
    mach_msg_type_number_t count = ARM_THREAD_STATE64_COUNT;
    arm_thread_state64_t gpr;
    err = ::thread_get_state(thread, ARM_THREAD_STATE64, (thread_state_t)&gpr, &count);
    if (err.Fail()) { throw std::runtime_error(err.AsString()); }
    registers.emplace_back(gpr);
  }
  return registers;
}

void MachProcess::ExceptionMessageReceived(const MachException::Message &exceptionMessage) {
  if (m_exception_messages.empty()) m_task.Suspend();
  if (m_single_step) {
    m_task.DisableSingleStep();
    m_single_step = false;
  }
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
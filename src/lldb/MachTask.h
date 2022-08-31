#pragma once

#include "DNBDefs.h"
#include "DNBError.h"
#include "MachException.h"
#include "MachVMMemory.h"
#include "ignoredExceptions.h"
#include <mach/mach.h>
#include <mach/mach_types.h>
#include <string>

class MachProcess;

class MachTask {
public:
  MachTask(MachProcess *process) : m_process(process) { memset(&m_exc_port_info, 0, sizeof(m_exc_port_info)); }

  task_t TaskPort() const { return m_task; }
  MachProcess *Process() { return m_process; }
  const MachProcess *Process() const { return m_process; }
  mach_port_t ExceptionPort() const { return m_exception_port; }

  bool StartExceptionThread(const IgnoredExceptions &ignored_exceptions, DNBError &err);
  kern_return_t ShutDownExcecptionThread();

  kern_return_t SaveExceptionPortInfo();
  kern_return_t RestoreExceptionPortInfo();

  kern_return_t Suspend();
  kern_return_t Resume();

  nub_size_t ReadMemory(nub_addr_t addr, nub_size_t size, void *buf);
  nub_size_t WriteMemory(nub_addr_t addr, nub_size_t size, const void *buf);

  void TaskPortChanged(task_t task);

  bool IsValid() const;
  static bool IsValid(task_t task);
  kern_return_t BasicInfo(struct task_basic_info *info);
  static kern_return_t BasicInfo(task_t task, struct task_basic_info *info);
  bool ExceptionPortIsValid() const;
  task_t TaskPortForProcessID(DNBError &err, bool force = false);
  static task_t TaskPortForProcessID(pid_t pid, DNBError &err, uint32_t num_retries = 10,
                                     uint32_t usec_interval = 10000);

  static void *ExceptionThread(void *arg);

private:
  MachProcess *m_process; // The mach process that owns this MachTask
  task_t m_task = TASK_NULL;
  MachException::PortInfo m_exc_port_info;       // Saved settings for all exception ports
  mach_port_t m_exception_port = MACH_PORT_NULL; // Exception port on which we will receive child exceptions
  pthread_t m_exception_thread = 0;              // Thread ID for the exception thread in case we need it
  MachVMMemory m_vm_memory;                      // Special mach memory reading class that will take
                                                 // care of watching for page and region boundaries
};

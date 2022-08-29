#include "MachTask.h"
#include "MachProcess.h"
#include <cassert>
#include <iostream>

bool MachTask::StartExceptionThread(const IgnoredExceptions &ignored_exceptions, DNBError &err) {
  task_t task = TaskPortForProcessID(err);
  std::cout << task << "\n";
  if (MachTask::IsValid(task)) {
    // Got the mach port for the current process
    mach_port_t task_self = mach_task_self();

    // Allocate an exception port that we will use to track our child process
    err = ::mach_port_allocate(task_self, MACH_PORT_RIGHT_RECEIVE, &m_exception_port);
    assert(err.Success());

    // Add the ability to send messages on the new exception port
    err = ::mach_port_insert_right(task_self, m_exception_port, m_exception_port, MACH_MSG_TYPE_MAKE_SEND);
    assert(err.Success());
    // Save the original state of the exception ports for our child process
    SaveExceptionPortInfo();
    // We weren't able to save the info for our exception ports, we must stop...
    if (m_exc_port_info.mask == 0) {
      err.SetErrorString("failed to get exception port info");
      return false;
    }
    if (!ignored_exceptions.empty()) {
      for (exception_mask_t mask : ignored_exceptions) m_exc_port_info.mask = m_exc_port_info.mask & ~mask;
    }
    // Set the ability to get all exceptions on this port
    err = ::task_set_exception_ports(task, m_exc_port_info.mask, m_exception_port,
                                     EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES, THREAD_STATE_NONE);
    assert(err.Success());
    // Create the exception thread
    err = ::pthread_create(&m_exception_thread, NULL, MachTask::ExceptionThread, this);
    return err.Success();
  }
  return false;
}

kern_return_t MachTask::SaveExceptionPortInfo() { return m_exc_port_info.Save(TaskPort()); }
kern_return_t MachTask::RestoreExceptionPortInfo() { return m_exc_port_info.Restore(TaskPort()); }

kern_return_t MachTask::Suspend() {
  DNBError err;
  task_t task = TaskPort();
  err = ::task_suspend(task);
  return err.Status();
}

kern_return_t MachTask::Resume() {
  struct task_basic_info task_info;
  task_t task = TaskPort();
  if (task == TASK_NULL) return KERN_INVALID_ARGUMENT;

  DNBError err;
  err = BasicInfo(task, &task_info);
  if (err.Success()) {
    // if (m_do_double_resume && task_info.suspend_count == 2) { err = ::task_resume(task); }
    // m_do_double_resume = false;

    // task_resume isn't counted like task_suspend calls are, are, so if the
    // task is not suspended, don't try and resume it since it is already
    // running
    if (task_info.suspend_count > 0) { err = ::task_resume(task); }
  }
  return err.Status();
}

void MachTask::TaskPortChanged(task_t task) { m_task = task; }

task_t MachTask::TaskPortForProcessID(DNBError &err, bool force) {
  if (((m_task == TASK_NULL) || force) && m_process != NULL) {
    m_task = MachTask::TaskPortForProcessID(m_process->ProcessID(), err);
  }
  return m_task;
}
task_t MachTask::TaskPortForProcessID(pid_t pid, DNBError &err, uint32_t num_retries, uint32_t usec_interval) {
  if (pid != INVALID_NUB_PROCESS) {
    DNBError err;
    mach_port_t task_self = mach_task_self();
    task_t task = TASK_NULL;
    for (uint32_t i = 0; i < num_retries; i++) {
      err = ::task_for_pid(task_self, pid, &task);
      if (err.Success()) { return task; }
      // Sleep a bit and try again
      ::usleep(usec_interval);
    }
  }
  return TASK_NULL;
}

bool MachTask::IsValid() const { return MachTask::IsValid(TaskPort()); }
bool MachTask::IsValid(task_t task) {
  if (task != TASK_NULL) {
    struct task_basic_info task_info;
    return BasicInfo(task, &task_info) == KERN_SUCCESS;
  }
  return false;
}

kern_return_t MachTask::BasicInfo(struct task_basic_info *info) { return BasicInfo(TaskPort(), info); }
kern_return_t MachTask::BasicInfo(task_t task, struct task_basic_info *info) {
  if (info == NULL) return KERN_INVALID_ARGUMENT;
  DNBError err;
  mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;
  err = ::task_info(task, TASK_BASIC_INFO, (task_info_t)info, &count);
  return err.Status();
}
bool MachTask::ExceptionPortIsValid() const { return MACH_PORT_VALID(m_exception_port); }

void *MachTask::ExceptionThread(void *arg) {
  if (arg == NULL) return NULL;

  MachTask *mach_task = (MachTask *)arg;
  MachProcess *mach_proc = mach_task->Process();
#if defined(__APPLE__)
  pthread_setname_np("exception monitoring thread");
#if defined(__arm__) || defined(__arm64__) || defined(__aarch64__)
  struct sched_param thread_param;
  int thread_sched_policy;
  if (pthread_getschedparam(pthread_self(), &thread_sched_policy, &thread_param) == 0) {
    thread_param.sched_priority = 47;
    pthread_setschedparam(pthread_self(), thread_sched_policy, &thread_param);
  }
#endif
#endif
  // We keep a count of the number of consecutive exceptions received so
  // we know to grab all exceptions without a timeout. We do this to get a
  // bunch of related exceptions on our exception port so we can process
  // then together. When we have multiple threads, we can get an exception
  // per thread and they will come in consecutively. The main loop in this
  // thread can stop periodically if needed to service things related to this
  // process.
  // flag set in the options, so we will wait forever for an exception on
  // our exception port. After we get one exception, we then will use the
  // MACH_RCV_TIMEOUT option with a zero timeout to grab all other current
  // exceptions for our process. After we have received the last pending
  // exception, we will get a timeout which enables us to then notify
  // our main thread that we have an exception bundle available. We then wait
  // for the main thread to tell this exception thread to start trying to get
  // exceptions messages again and we start again with a mach_msg read with
  // infinite timeout.
  uint32_t num_exceptions_received = 0;
  DNBError err;
  task_t task = mach_task->TaskPort();
  mach_msg_timeout_t periodic_timeout = 0;

  while (mach_task->ExceptionPortIsValid()) {
    ::pthread_testcancel();

    MachException::Message exception_message;

    if (num_exceptions_received > 0) {
      // No timeout, just receive as many exceptions as we can since we already
      // have one and we want
      // to get all currently available exceptions for this task
      std::cout << "receive timeout" << 1 << "\n";
      err = exception_message.Receive(mach_task->ExceptionPort(), MACH_RCV_MSG | MACH_RCV_INTERRUPT | MACH_RCV_TIMEOUT,
                                      1);
    } else if (periodic_timeout > 0) {
      // We need to stop periodically in this loop, so try and get a mach
      // message with a valid timeout (ms)
      std::cout << "receive timeout" << periodic_timeout << "\n";
      err = exception_message.Receive(mach_task->ExceptionPort(), MACH_RCV_MSG | MACH_RCV_INTERRUPT | MACH_RCV_TIMEOUT,
                                      periodic_timeout);
    } else {
      // We don't need to parse all current exceptions or stop periodically,
      // just wait for an exception forever.
      std::cout << "receive forever\n";
      err = exception_message.Receive(mach_task->ExceptionPort(), MACH_RCV_MSG | MACH_RCV_INTERRUPT, 0);
    }

    if (err.Status() == MACH_RCV_INTERRUPTED) {
      std::cout << "MACH_RCV_INTERRUPTED\n";
      // If we have no task port we should exit this thread
      if (!mach_task->ExceptionPortIsValid()) { break; }

      // Make sure our task is still valid
      if (MachTask::IsValid(task)) {
        // Task is still ok
        continue;
      } else {
        // mach_proc->SetState(eStateExited);
        // Our task has died, exit the thread.
        break;
      }
    } else if (err.Status() == MACH_RCV_TIMED_OUT) {
      std::cout << "MACH_RCV_TIMED_OUT\n";
      std::cout << num_exceptions_received << " num_exceptions_received\n";
      if (num_exceptions_received > 0) {
        // We were receiving all current exceptions with a timeout of zero
        // it is time to go back to our normal looping mode
        num_exceptions_received = 0;

        // Notify our main thread we have a complete exception message
        // bundle available and get the possibly updated task port back
        // from the process in case we exec'ed and our task port changed
        // task = mach_proc->ExceptionMessageBundleComplete();

        // in case we use a timeout value when getting exceptions...
        // Make sure our task is still valid
        if (MachTask::IsValid(task)) {
          // Task is still ok
          continue;
        } else {
          // mach_proc->SetState(eStateExited);
          std::cout << "eStateExited\n";
          // Our task has died, exit the thread.
          break;
        }
      }
    } else if (err.Status() != KERN_SUCCESS) {
      // TODO: notify of error?
    } else {
      std::cout << "exception_message.Receive success\n";
      if (exception_message.CatchExceptionRaise(task)) {
        if (exception_message.state.task_port != task) {
          if (exception_message.state.IsValid()) {
            // We exec'ed and our task port changed on us.
            task = exception_message.state.task_port;
            mach_task->TaskPortChanged(exception_message.state.task_port);
          }
        }
        ++num_exceptions_received;
        mach_proc->ExceptionMessageReceived(exception_message);
      }
    }
  }
  return NULL;
}
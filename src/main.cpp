#include "lldb/DNBDefs.h"
#include "lldb/MachProcess.h"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sys/ptrace.h>
#include <thread>

class DebuggerController {
public:
  explicit DebuggerController(pid_t pid) : m_pid(pid) {}

  ~DebuggerController() {}

  void ptrace_attach() {
    pid_t pid = INVALID_NUB_PROCESS;
    std::shared_ptr<MachProcess> processSP = std::make_shared<MachProcess>();
    char err_str[1024];
    IgnoredExceptions ignores{EXC_MASK_BAD_ACCESS, EXC_MASK_BAD_INSTRUCTION, EXC_MASK_ARITHMETIC};
    pid = processSP->AttachForDebug(m_pid, ignores, &err_str[0], 1024);
    if (pid == INVALID_NUB_PROCESS) {
      std::cout << err_str << "\n";
      std::exit(-1);
    }
    printf("attach process pid:%d\n", m_pid);
  }

  void ptrace_detach() {
    if (::ptrace(PT_DETACH, m_pid, NULL, 0) < 0) {
      perror("ptrace dettach error");
      std::exit(-1);
    }
    printf("dettach process pid:%d\n", m_pid);
  }

private:
  pid_t m_pid;
};

int main(int argc, const char *argv[]) {
  assert((argc == 2) && "argument count error");
  pid_t pid = std::atoi(argv[1]);
  DebuggerController controller{pid};
  controller.ptrace_attach();
  std::this_thread::sleep_for(std::chrono::seconds(5));
  controller.ptrace_detach();
}
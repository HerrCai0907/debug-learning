#include "lldb/DNBDefs.h"
#include "lldb/MachProcess.h"
#include "logger.hpp"
#include <_types/_uint64_t.h>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sys/ptrace.h>
#include <thread>
#include <vector>

class DebuggerController {
public:
  explicit DebuggerController(pid_t pid) : m_pid(pid) {}

  ~DebuggerController() {}

  void ptrace_attach() {
    pid_t pid = INVALID_NUB_PROCESS;
    m_processSP = std::make_shared<MachProcess>();
    char err_str[1024];
    IgnoredExceptions ignores{EXC_MASK_BAD_ACCESS, EXC_MASK_BAD_INSTRUCTION, EXC_MASK_ARITHMETIC};
    pid = m_processSP->AttachForDebug(m_pid, ignores, &err_str[0], 1024);
    if (pid == INVALID_NUB_PROCESS) {
      std::cout << err_str << "\n";
      std::exit(-1);
    }
    printf("attach process pid:%d\n", m_pid);
  }

  void ptrace_detach() {
    assert(m_processSP->Detach());
    printf("dettach process pid:%d\n", m_pid);
  }

  std::vector<uint8_t> read_memory(uint64_t addr, uint64_t size) {
    std::vector<uint8_t> memory_data(size);
    m_processSP->ReadMemory(addr, size, memory_data.data());
    return memory_data;
  }
  void write_memory(uint64_t addr, uint8_t const *data, uint64_t size) { m_processSP->WriteMemory(addr, size, data); }

private:
  pid_t m_pid;
  std::shared_ptr<MachProcess> m_processSP = nullptr;
};

constexpr uint64_t ADDR = 0x1002d4000;
int main(int argc, const char *argv[]) {
  assert((argc == 2) && "argument count error");
  pid_t pid = std::atoi(argv[1]);
  DebuggerController controller{pid};
  controller.ptrace_attach();
  std::this_thread::sleep_for(std::chrono::seconds(1));
  auto data = controller.read_memory(ADDR, 12);
  Logger::logDebug(data);
  data[0] = 0;
  data[1] = 0;
  controller.write_memory(ADDR, data.data(), data.size());
  std::this_thread::sleep_for(std::chrono::seconds(1));
  controller.ptrace_detach();
}
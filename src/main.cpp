#include "lldb/DNBDefs.h"
#include "lldb/MachProcess.h"
#include "logger.hpp"
#include <_types/_uint64_t.h>
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sys/ptrace.h>
#include <sys/signal.h>
#include <thread>
#include <vector>

class DebuggerController {
public:
  explicit DebuggerController(pid_t pid) : m_pid(pid) {}

  ~DebuggerController() {}

  void attach() {
    m_processSP = std::make_shared<MachProcess>();
    IgnoredExceptions ignores{EXC_MASK_BAD_ACCESS, EXC_MASK_BAD_INSTRUCTION, EXC_MASK_ARITHMETIC};
    m_processSP->Attach(m_pid, ignores);
    Logger::logInfo("attach process pid", m_pid);
  }
  void detach() {
    m_processSP->Detach();
    Logger::logInfo("detach process pid", m_pid);
  }

  void resume() {
    m_processSP->Resume();
    Logger::logInfo("resume process pid", m_pid);
  }
  void stop() {
    m_processSP->Stop();
    Logger::logInfo("stop process pid", m_pid);
  }
  void single_step() {
    m_processSP->SingleStep();
    Logger::logInfo("single step pid", m_pid);
  }

  std::vector<uint8_t> read_memory(uint64_t addr, uint64_t size) {
    std::vector<uint8_t> memory_data(size);
    m_processSP->ReadMemory(addr, size, memory_data.data());
    return memory_data;
  }
  void write_memory(uint64_t addr, uint8_t const *data, uint64_t size) { m_processSP->WriteMemory(addr, size, data); }

  std::vector<uint64_t> read_pc() {
    std::vector<arm_thread_state64_t> regs = m_processSP->ReadRegister();
    std::vector<uint64_t> pcs(regs.size());
    std::transform(regs.begin(), regs.end(), pcs.begin(),
                   [](arm_thread_state64_t const &it) { return arm_thread_state64_get_pc(it); });
    return pcs;
  }

private:
  pid_t m_pid;
  std::shared_ptr<MachProcess> m_processSP = nullptr;
};

constexpr uint64_t ADDR = 0x104910000;

int main(int argc, const char *argv[]) {
  assert((argc == 2) && "argument count error");
  pid_t pid = std::atoi(argv[1]);
  DebuggerController controller{pid};
  controller.attach();
  std::this_thread::sleep_for(std::chrono::seconds(1));
  auto data = controller.read_memory(ADDR, 12);
  Logger::logDebug(data);
  data[0] = 0;
  data[1] = 0;
  controller.write_memory(ADDR, data.data(), data.size());
  for (int i = 0; i < 3; i++) {
    controller.resume();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    controller.stop();
    std::this_thread::sleep_for(std::chrono::seconds(2));
  }
  for (int i = 0; i < 100; i++) {
    controller.single_step();
    auto pcs = controller.read_pc();
    Logger::logInfo("pc register:", pcs);
  }
  controller.resume();
  controller.detach();
}
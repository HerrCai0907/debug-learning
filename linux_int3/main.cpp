#include <array>
#include <atomic>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ucontext.h>
#include <thread>
#include <unistd.h>

using Int3Func = void (*)();

constexpr std::array<uint8_t, 1> INT3{0xCC};
Int3Func int3 = [] {
  asm("INT3");
  std::cout << "hhhh\n";
};

void initSingalHandler() {
  struct sigaction sa = {};
  sa.sa_flags = SA_SIGINFO | SA_NODEFER;
  sigfillset(&sa.sa_mask);
  sa.sa_sigaction = [](int signum, siginfo_t *info, void *ucontext) {
    std::cout << "Thread " << std::this_thread::get_id() << " Interrupt signal (" << signum << ") received.\n";
    ucontext_t *uc = static_cast<ucontext_t *>(ucontext);
    std::cout << "Trap in " << uc->uc_mcontext.gregs[REG_RIP] << "\n";
  };
  auto ret = sigaction(SIGTRAP, &sa, nullptr);
  if (ret == -1) { throw std::runtime_error(strerror(errno)); }
}

void initIn3() {
  int fd = open("/dev/zero", O_RDWR);
  int3 = reinterpret_cast<Int3Func>(mmap(NULL, INT3.size(), PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED, fd, 0));
  if (int3 == MAP_FAILED) { throw std::runtime_error(strerror(errno)); }
  std::memcpy(reinterpret_cast<void *>(int3), INT3.data(), INT3.size());
}

int main() {
  initIn3();
  std::cout << "INT3 addr " << reinterpret_cast<uint64_t>(int3) << "\n";

  initSingalHandler();
  std::thread int3Thread([] {
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::cout << "Thread " << std::this_thread::get_id() << " throw int3\n";
    int3();
  });

  std::thread workThread([] {
    for (int i = 0; i < 20; i++) {
      std::cout << "Thread " << std::this_thread::get_id() << " is working\n";
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  std::cout << "Thread " << std::this_thread::get_id() << " throw int3\n";
  int3();

  if (int3Thread.joinable()) { int3Thread.join(); }
  if (workThread.joinable()) { workThread.join(); }
}
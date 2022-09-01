#include <_types/_uint64_t.h>
#include <chrono>
#include <iostream>
#include <thread>

int i = 0;

int main() {
  for (;; i++) {
    std::cout << "address " << &i << " is " << i << '\n';
    std::cout << "main address is " << reinterpret_cast<uint64_t>(main) << '\n';
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }
}
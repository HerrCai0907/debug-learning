#include <chrono>
#include <iostream>
#include <thread>

int i = 0;

int main() {
  for (;; i++) {
    std::cout << "address " << &i << " is " << i << '\n';
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }
}
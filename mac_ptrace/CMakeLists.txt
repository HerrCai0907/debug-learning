add_subdirectory(lldb)

add_executable(mac_ptrace main.cpp)

target_link_libraries(mac_ptrace lldb)

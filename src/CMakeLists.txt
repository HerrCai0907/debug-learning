add_subdirectory(lldb)

add_executable(ptrace-learn main.cpp)

target_link_libraries(ptrace-learn lldb_lib)

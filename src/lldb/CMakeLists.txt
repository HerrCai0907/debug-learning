aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} lldb_sources)

set(generated_mach_interfaces
    ${CMAKE_CURRENT_BINARY_DIR}/mach_exc.h
    ${CMAKE_CURRENT_BINARY_DIR}/mach_excServer.c
    ${CMAKE_CURRENT_BINARY_DIR}/mach_excUser.c)
set(MIG_ARCH_FLAGS "")
if(DEFINED MIG_ARCHS)
  foreach(ARCH ${MIG_ARCHS})
    set(MIG_ARCH_FLAGS "${MIG_ARCH_FLAGS} -arch ${ARCH}")
  endforeach()
endif()
separate_arguments(MIG_ARCH_FLAGS_SEPARTED NATIVE_COMMAND "${MIG_ARCH_FLAGS}")
add_custom_command(
  OUTPUT ${generated_mach_interfaces}
  VERBATIM
  COMMAND mig ${MIG_ARCH_FLAGS_SEPARTED} -isysroot ${CMAKE_OSX_SYSROOT}
          ${CMAKE_CURRENT_SOURCE_DIR}/dbgnub-mig.defs
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/dbgnub-mig.defs)

add_library(lldb_lib STATIC ${lldb_sources} ${generated_mach_interfaces})

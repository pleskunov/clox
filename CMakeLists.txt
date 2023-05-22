cmake_minimum_required(VERSION 3.26)
set(CMAKE_BUILD_TYPE Debug)

project(bytecodevm VERSION 1.0 DESCRIPTION "A clox from Crafting Interpreters book.")

add_library(vm_memory)
target_sources(vm_memory
  PUBLIC
    src/common.h
    src/memory.h
  PRIVATE
    src/memory.c
  )

add_library(vm_value)
target_sources(vm_value
  PUBLIC
    src/common.h
    src/value.h
  PRIVATE
    src/value.c
  )
target_link_libraries(vm_value PRIVATE vm_memory)

add_library(vm_chunk)
target_sources(vm_chunk
  PUBLIC
    src/common.h
    src/chunk.h
  PRIVATE
    src/chunk.c
  )
target_link_libraries(vm_chunk PRIVATE vm_memory vm_value)

add_library(vm_debug)
target_sources(vm_debug
  PUBLIC
    src/debug.h
  PRIVATE
    src/debug.c
  )
target_link_libraries(vm_debug PRIVATE vm_chunk)

add_library(vm_core)
target_sources(vm_core
  PUBLIC
    src/vm.h
  PRIVATE
    src/vm.c
  )
target_link_libraries(vm_core PRIVATE vm_chunk)

add_executable(bcvm)
target_sources(bcvm
  PRIVATE
    src/main.c
  )
target_link_libraries(bcvm PRIVATE vm_core vm_chunk vm_debug)

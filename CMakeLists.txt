cmake_minimum_required(VERSION 3.26)
set(CMAKE_BUILD_TYPE Debug)

project(bytecodevm VERSION 1.2 DESCRIPTION "A clox from Crafting Interpreters book.")

add_library(memory)
target_sources(memory
  PUBLIC
    src/common.h
    src/object.h
    src/vm.h
    src/memory.h
  PRIVATE
    src/memory.c
)

add_library(object)
target_sources(object
  PUBLIC
    src/common.h
    src/memory.h
    src/table.h
    src/vm.h
    src/object.h
    src/chunk.h
  PRIVATE
    src/object.c
)

add_library(table)
target_sources(table
  PUBLIC
    src/common.h
    src/memory.h
    src/object.h
    src/table.h
  PRIVATE
    src/table.c
)

add_library(value)
target_sources(value
  PUBLIC
    src/common.h
    src/memory.h
    src/object.h
    src/value.h
  PRIVATE
    src/value.c
)
target_link_libraries(value PRIVATE memory object table)

add_library(chunk)
target_sources(chunk
  PUBLIC
    src/common.h
    src/memory.h
    src/value.h
    src/chunk.h
  PRIVATE
    src/chunk.c
)

add_library(debugger)
target_sources(debugger
  PUBLIC
    src/value.h
    src/chunk.h
    src/debug.h
  PRIVATE
    src/debug.c
  )

add_library(scanner)
target_sources(scanner
  PUBLIC
    src/common.h
    src/scanner.h
  PRIVATE
    src/scanner.c
)

add_library(compiler)
target_sources(compiler
  PUBLIC
    src/common.h
    src/scanner.h
    src/object.h
    src/debug.h
    src/compiler.h
  PRIVATE
    src/compiler.c
)
target_link_libraries(compiler PRIVATE scanner)

add_library(vm)
target_sources(vm
  PUBLIC
    src/common.h
    src/memory.h
    src/object.h
    src/value.h
    src/table.h
    src/chunk.h
    src/debug.h
    src/compiler.h
    src/vm.h
  PRIVATE
    src/vm.c
)
target_link_libraries(vm PRIVATE value chunk debugger compiler)

add_executable(bcvm)
target_sources(bcvm
  PRIVATE
    src/main.c
)
target_link_libraries(bcvm PRIVATE vm)

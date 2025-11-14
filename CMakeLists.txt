cmake_minimum_required(VERSION 3.16)
project(Ardent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ARDENT_BUILD_RELEASE "Build with -O2" ON)
option(ARDENT_ENABLE_LLVM "Enable experimental LLVM-based IR/JIT (Ardent 2.0 work)" OFF)

if(ARDENT_BUILD_RELEASE)
  if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
  endif()
endif()

add_executable(ardent
  src/main.cpp
  src/lexer.cpp
  src/parser.cpp
  src/interpreter.cpp
  src/scroll_loader.cpp)

# Experimental IRGen / Runtime (Ardent 2.0) -- only built if LLVM enabled
if(ARDENT_ENABLE_LLVM)
  # Allow manual override of LLVM config directory
  if(DEFINED LLVM_OVERRIDE_DIR)
    set(LLVM_DIR "${LLVM_OVERRIDE_DIR}" CACHE PATH "Override LLVM config directory" FORCE)
  endif()
  find_package(LLVM REQUIRED CONFIG)
  message(STATUS "LLVM found: ${LLVM_PACKAGE_VERSION}")
  message(STATUS "LLVM include dirs: ${LLVM_INCLUDE_DIRS}")
  add_library(ardent_irgen STATIC src/irgen/ir_builder.cpp)
  target_include_directories(ardent_irgen PRIVATE ${LLVM_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_compile_definitions(ardent_irgen PRIVATE ARDENT_ENABLE_LLVM)
  if(TARGET LLVM::Core)
    target_link_libraries(ardent_irgen PRIVATE LLVM::Core LLVM::Support)
  endif()
  # Optional extended components for future IR work
  if(TARGET LLVM::ExecutionEngine)
    target_link_libraries(ardent_irgen PRIVATE LLVM::ExecutionEngine)
  endif()
  if(TARGET LLVM::OrcJIT)
    target_link_libraries(ardent_irgen PRIVATE LLVM::OrcJIT)
  endif()
  add_library(ardent_runtime STATIC src/runtime/ardent_runtime.cpp)
  target_include_directories(ardent_runtime PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_compile_definitions(ardent_runtime PRIVATE ARDENT_ENABLE_LLVM)
  target_link_libraries(ardent PRIVATE ardent_irgen ardent_runtime)
  add_executable(ardent_ir_demo src/irgen/ir_demo_main.cpp)
  target_include_directories(ardent_ir_demo PRIVATE ${LLVM_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_compile_definitions(ardent_ir_demo PRIVATE ARDENT_ENABLE_LLVM)
  # Link demo with our IR generator and runtime ABI
  target_link_libraries(ardent_ir_demo PRIVATE ardent_irgen ardent_runtime)
  # Prefer imported targets, but fall back to component libs if missing
  set(_linked_imports OFF)
  if(TARGET LLVM::Core)
    target_link_libraries(ardent_ir_demo PRIVATE LLVM::Core LLVM::Support)
    set(_linked_imports ON)
    if(TARGET LLVM::ExecutionEngine)
      target_link_libraries(ardent_ir_demo PRIVATE LLVM::ExecutionEngine)
    endif()
    if(TARGET LLVM::OrcJIT)
      target_link_libraries(ardent_ir_demo PRIVATE LLVM::OrcJIT)
    endif()
    if(TARGET LLVM::Target)
      target_link_libraries(ardent_ir_demo PRIVATE LLVM::Target)
    endif()
    if(TARGET LLVM::MC)
      target_link_libraries(ardent_ir_demo PRIVATE LLVM::MC)
    endif()
    if(TARGET LLVM::RuntimeDyld)
      target_link_libraries(ardent_ir_demo PRIVATE LLVM::RuntimeDyld)
    endif()
  endif()
  if(NOT _linked_imports)
    llvm_map_components_to_libnames(ARDENT_LLVM_LIBS
      core support executionengine orcjit target mc runtimedyld x86codegen x86asmparser x86desc x86info)
    target_link_libraries(ardent_ir_demo PRIVATE ${ARDENT_LLVM_LIBS})
  endif()
endif()

# Header search paths (allow includes from src/ and src/avm/)
target_include_directories(ardent PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src/avm)

# Basic warnings
if (MSVC)
  target_compile_options(ardent PRIVATE /W4)
else()
  target_compile_options(ardent PRIVATE -Wall -Wextra -pedantic)
endif()

# Install layout
# Primary binary: ardent
install(TARGETS ardent RUNTIME DESTINATION bin)

# Duplicate launcher names (ardentc, oracle) using install(CODE)
install(CODE [[
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE FILE FILES "$<TARGET_FILE:ardent>" RENAME "ardentc")
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE FILE FILES "$<TARGET_FILE:ardent>" RENAME "oracle")
]])

# Create lib/ardent/scrolls placeholder
install(DIRECTORY DESTINATION lib/ardent/scrolls)

# Examples
install(DIRECTORY examples/ DESTINATION share/ardent/examples FILES_MATCHING PATTERN "*.ardent")

# Version banner for packages
message(STATUS "Configured Ardent ${CMAKE_PROJECT_VERSION} for install prefix: ${CMAKE_INSTALL_PREFIX}")


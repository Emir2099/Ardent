cmake_minimum_required(VERSION 3.16)
project(Ardent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ARDENT_BUILD_RELEASE "Build with -O2" ON)

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


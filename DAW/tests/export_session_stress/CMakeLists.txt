cmake_minimum_required(VERSION 3.16)
project(IDWExportSessionStress LANGUAGES CXX)

# Standalone so the race check needs no JUCE, freetype or X11: the session is
# JUCE-free, and the top-level project builds juceaide at configure time.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(IDW_SANITIZE_THREAD "Build the stress test with ThreadSanitizer" OFF)

find_package(Threads REQUIRED)

add_executable(IDWDAWExportSessionStressTests ../ExportSessionStressTests.cpp)
target_include_directories(IDWDAWExportSessionStressTests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../..")
target_link_libraries(IDWDAWExportSessionStressTests PRIVATE Threads::Threads)

if(IDW_SANITIZE_THREAD)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "IDW_SANITIZE_THREAD requires GCC or Clang")
    endif()
    target_compile_options(IDWDAWExportSessionStressTests PRIVATE
        -fsanitize=thread -fno-omit-frame-pointer -g -O1)
    target_link_options(IDWDAWExportSessionStressTests PRIVATE -fsanitize=thread)
endif()

enable_testing()
add_test(NAME IDWDAWExportSessionStress COMMAND IDWDAWExportSessionStressTests)
set_tests_properties(IDWDAWExportSessionStress PROPERTIES TIMEOUT 300)

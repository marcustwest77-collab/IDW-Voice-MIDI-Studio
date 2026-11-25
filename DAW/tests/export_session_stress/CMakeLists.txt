cmake_minimum_required(VERSION 3.16)
project(IDWExportSessionStress LANGUAGES CXX)

# Standalone so the race check needs no JUCE, freetype or X11: the session is
# JUCE-free, and the top-level project builds juceaide at configure time.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(IDW_SANITIZE_THREAD "Build the stress test with ThreadSanitizer" OFF)
option(IDW_SANITIZE_ADDRESS "Build the stress test with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

find_package(Threads REQUIRED)

add_executable(IDWDAWExportSessionStressTests ../ExportSessionStressTests.cpp)
target_include_directories(IDWDAWExportSessionStressTests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../..")
target_link_libraries(IDWDAWExportSessionStressTests PRIVATE Threads::Threads)

if(IDW_SANITIZE_THREAD AND IDW_SANITIZE_ADDRESS)
    message(FATAL_ERROR "Choose one sanitizer build, not both")
endif()
if(IDW_SANITIZE_THREAD OR IDW_SANITIZE_ADDRESS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "The sanitizer builds require GCC or Clang")
    endif()
    # Shared by both sanitizers: symbols and usable stacks at a sane speed.
    target_compile_options(IDWDAWExportSessionStressTests PRIVATE
        -fno-omit-frame-pointer -g -O1)
endif()
if(IDW_SANITIZE_THREAD)
    target_compile_options(IDWDAWExportSessionStressTests PRIVATE -fsanitize=thread)
    target_link_options(IDWDAWExportSessionStressTests PRIVATE -fsanitize=thread)
endif()
if(IDW_SANITIZE_ADDRESS)
    # -fno-sanitize-recover=all turns a UB report into a failure instead of a warning.
    target_compile_options(IDWDAWExportSessionStressTests PRIVATE
        -fsanitize=address,undefined -fno-sanitize-recover=all)
    target_link_options(IDWDAWExportSessionStressTests PRIVATE -fsanitize=address,undefined)
endif()

enable_testing()
add_test(NAME IDWDAWExportSessionStress COMMAND IDWDAWExportSessionStressTests)
set_tests_properties(IDWDAWExportSessionStress PROPERTIES TIMEOUT 300)

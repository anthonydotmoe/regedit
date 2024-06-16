cmake_minimum_required(VERSION 3.0.0)

project(regedit LANGUAGES C)

set(EXECUTABLE_NAME regedit)

set(SOURCE_DIR ${CMAKE_SOURCE_DIR}/src)
set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/inc)

set(RESOURCE_FILE ${SOURCE_DIR}/regedit.rc)

# Find all the C source files in the source directory
file(GLOB SOURCE_FILES ${SOURCE_DIR}/*.c)

# Add the executable
add_executable(${EXECUTABLE_NAME} WIN32 ${SOURCE_FILES} ${RESOURCE_FILE})

# Specify the include directories
target_include_directories(${EXECUTABLE_NAME} PRIVATE ${INCLUDE_DIR})

# Add the required Windows libraries
target_link_libraries(${EXECUTABLE_NAME} PRIVATE advapi32 comctl32)

# Enable Unicode
target_compile_definitions(${EXECUTABLE_NAME} PRIVATE UNICODE _UNICODE)
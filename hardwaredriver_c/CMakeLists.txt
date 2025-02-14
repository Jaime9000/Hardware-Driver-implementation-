cmake_minimum_required(VERSION 3.15)

# Add this line to use vcpkg
set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")

# Version information
set(PROJECT_VERSION_MAJOR 1)
set(PROJECT_VERSION_MINOR 0)
set(PROJECT_VERSION_PATCH 0)

project(MyotronicsDriver 
        VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
        DESCRIPTION "Myotronics Hardware Driver"
        LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Force C compiler
enable_language(C)

# Windows-specific settings
if(WIN32)
    # Add Windows-specific compile definitions
    # add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-D_WIN32_WINNT=0x0601)  # Target Windows 7 or later
endif()

# Add near the top with other find_package calls
find_package(OpenCV REQUIRED)

# Define source files
set(CORE_SOURCES
    src/core/main/main.c
    src/core/main/service.c
    src/core/main/config.c
    src/core/serial_interface.c
    src/core/error_codes.c
    src/core/logger.c
)

set(MODE_SOURCES
    src/core/mode_functions/mode_manager.c
    src/core/mode_functions/mode_base.c
    src/core/mode_functions/mode_0.c
    src/core/mode_functions/mode_42.c
    src/core/mode_functions/mode_43.c
    src/core/mode_functions/mode_44.c
    src/core/mode_functions/mode_51.c
    src/core/mode_functions/mode_52.c
    src/core/mode_functions/mode_53.c
    src/core/mode_functions/mode_56.c
    src/core/mode_functions/mode_57.c
    src/core/mode_functions/mode_sweep.c
)

set(GUI_SOURCES
    src/gui/utils/process_manager.c
    src/gui/utils/windows_api.c
    src/gui/utils/image_rotate.c
    src/gui/utils/play_video.c
    src/gui/utils/wait_for_process.c
    src/gui/sweep_data/utils.c
    src/gui/system_options/display_tilt_supplemental_windows.c
)

# Make sure source files are compiled as C
set_source_files_properties(
    ${CORE_SOURCES}
    ${MODE_SOURCES}
    ${GUI_SOURCES}
    PROPERTIES
    LANGUAGE C
)

# Add after setting source files
message(STATUS "Checking source file properties:")
foreach(source ${CORE_SOURCES} ${MODE_SOURCES} ${GUI_SOURCES})
    get_source_file_property(lang ${source} LANGUAGE)
    message(STATUS "${source} - Language: ${lang}")
endforeach()

message(STATUS "Compiler ID: ${CMAKE_C_COMPILER_ID}")
message(STATUS "C Standard: ${CMAKE_C_STANDARD}")
message(STATUS "Compiler flags: ${CMAKE_C_FLAGS}")

# Create library
add_library(myotronics_driver_lib
    ${CORE_SOURCES}
    ${MODE_SOURCES}
    ${GUI_SOURCES}
)

# Set include directories
target_include_directories(myotronics_driver_lib
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}    # This lets compiler find paths starting with "src/"
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/cjson
        ${OpenCV_INCLUDE_DIRS}        # Add OpenCV include directories
)

# Create executable and add version resource
add_executable(myotronics_driver src/core/main/main.c)

if(MSVC)
    target_sources(myotronics_driver PRIVATE version.rc)
endif()

# Link Windows libraries
target_link_libraries(myotronics_driver_lib
    PRIVATE
        winhttp          # HTTP server functionality
        ws2_32          # Windows sockets
        kernel32        # Core Windows functionality
        user32          # Userserial_interface functionality
        advapi32        # Advanced Windows functionality (registry, security)
        setupapi        # Device setup and configuration
        shell32         # Shell functionality
        ${OpenCV_LIBS}  # Add OpenCV libraries
)

target_link_libraries(myotronics_driver
    PRIVATE
        myotronics_driver_lib
)

# Set compiler warnings
if(MSVC)
    target_compile_options(myotronics_driver_lib PRIVATE /W4)
    target_compile_options(myotronics_driver PRIVATE /W4)
else()
    target_compile_options(myotronics_driver_lib PRIVATE -Wall -Wextra)
    target_compile_options(myotronics_driver PRIVATE -Wall -Wextra)
endif()

# Set output directories
set_target_properties(myotronics_driver
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Add post-build command to copy executable
add_custom_command(TARGET myotronics_driver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:myotronics_driver> ${CMAKE_BINARY_DIR}/bin/
)

# Add after the existing post-build commands
# Copy OpenCV DLLs to output directory
add_custom_command(TARGET myotronics_driver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy 
        "${CMAKE_SOURCE_DIR}/lib/opencv/opencv_world440$<$<CONFIG:Debug>:d>.dll"
        "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/"
)
# Myotronics Driver

## Build Requirements

### Required Tools
- CMake (version 3.15 or higher)
- One of the following C compilers:
  - Microsoft Visual Studio 2019 or later with C/C++ development tools
  - MinGW-w64

### Version Information
Current version: 1.0.0

The version information is set in CMakeLists.txt:
```cmake
set(PROJECT_VERSION_MAJOR 1)
set(PROJECT_VERSION_MINOR 0)
set(PROJECT_VERSION_PATCH 0)
```
### Installation Instructions

1. Install CMake:
   - Download from https://cmake.org/download/
   - Or install via winget: `winget install Kitware.CMake`

2. Install Visual Studio:
   - Download from https://visualstudio.microsoft.com/
   - Make sure to select "Desktop development with C++" during installation

### Building the Project

1. Create a build directory:
    - cmd 
    - md build
    - cd build

2. Generate build files:
    - cmd
    - cmake ..

3. Build the project:
    - cmd
    - cmake --build . --config Release

4. Create release package:
    - cmd
    - package.bat   
    
    This will create a Release directory containing:
    - bin/: Contains the executable
    - config/: Contains settings.conf
    - logs/: Directory for log files

<----------------------------------------------------------------->
[Note: You can also use Visual Studio's built-in CMake support by:
1. Opening Visual Studio
2. File -> Open -> Folder
3. Select the project root directory
4. Visual Studio will automatically configure CMake
5. Select "Build All" from the Build menu]
<----------------------------------------------------------------->

### Post-Build
After building, the executable will be automatically copied to the `build/bin` directory.


NOTE: an installer will be added to the final version/release, but we need to verify working order through testing before we implement this. 
cmake_minimum_required(VERSION 3.25.0)

# Set project name
project(vst_scanner)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include VST3 SDK
add_subdirectory(vst3sdk)

# Create executable
add_executable(vst_scanner vst_scanner.cpp)

# Link against VST3 SDK libraries
target_link_libraries(vst_scanner 
    sdk
    sdk_common
    sdk_hosting
    base
    pluginterfaces
)

# Include directories
target_include_directories(vst_scanner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/vst3sdk
)

# Platform-specific settings
if(WIN32)
    # Windows-specific settings
    set_target_properties(vst_scanner PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
elseif(APPLE)
    # macOS-specific settings
    set_target_properties(vst_scanner PROPERTIES
        MACOSX_BUNDLE TRUE
    )
endif()

# Set output directory
set_target_properties(vst_scanner PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
) 
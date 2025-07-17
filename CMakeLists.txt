cmake_minimum_required(VERSION 3.25.0)

# Set project name
project(vst_scanner)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# VST3 SDK options - ensure hosting support is enabled
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES ON CACHE BOOL "Enable VST 3 Hosting Examples" FORCE)
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "Enable VST 3 Plug-in Examples" FORCE)
set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "Enable VSTGUI Support" FORCE)

# Include VST3 SDK
add_subdirectory(vst3sdk)

# Create executable with moduleinfo sources
add_executable(vst_scanner 
    vst_scanner.cpp
    vst3sdk/public.sdk/source/vst/moduleinfo/moduleinfocreator.cpp
    vst3sdk/public.sdk/source/vst/moduleinfo/moduleinfocreator.h
    vst3sdk/public.sdk/source/vst/moduleinfo/moduleinfoparser.cpp
    vst3sdk/public.sdk/source/vst/moduleinfo/moduleinfoparser.h
    vst3sdk/public.sdk/source/vst/moduleinfo/moduleinfo.h
    vst3sdk/public.sdk/source/vst/moduleinfo/json.h
    vst3sdk/public.sdk/source/vst/moduleinfo/jsoncxx.h
)

# Link against VST3 SDK libraries
target_link_libraries(vst_scanner 
    sdk_hosting
    sdk_common
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
    # set_target_properties(vst_scanner PROPERTIES
    #     WIN32_EXECUTABLE TRUE
    # )
    
    # Add Windows-specific module source
    target_sources(vst_scanner PRIVATE vst3sdk/public.sdk/source/vst/hosting/module_win32.cpp)
    # Windows-specific libraries
    target_link_libraries(vst_scanner 
        kernel32
        user32
        shell32
        ole32
        uuid
        advapi32
        oleaut32
    )
elseif(APPLE)
    # macOS-specific settings
    set_target_properties(vst_scanner PROPERTIES
        MACOSX_BUNDLE TRUE
    )
    
    # macOS-specific frameworks
    find_library(COREFOUNDATION_LIBRARY CoreFoundation)
    find_library(CORESERVICES_LIBRARY CoreServices)
    target_link_libraries(vst_scanner 
        ${COREFOUNDATION_LIBRARY}
        ${CORESERVICES_LIBRARY}
    )
elseif(UNIX)
    # Linux-specific libraries
    target_link_libraries(vst_scanner 
        dl
        pthread
    )
endif()

# Set output directory
set_target_properties(vst_scanner PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
) 
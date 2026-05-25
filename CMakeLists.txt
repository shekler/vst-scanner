cmake_minimum_required(VERSION 3.25.0)

project(vst_scanner)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# VST3 SDK options — scanner only needs hosting libs, not sample apps
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "Enable VST 3 Hosting Examples" FORCE)
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "Enable VST 3 Plug-in Examples" FORCE)
set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "Enable VSTGUI Support" FORCE)

add_subdirectory(vst3sdk)

set(VST3_SDK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/vst3sdk)

add_executable(vst_scanner
    vst_scanner.cpp
)

target_compile_features(vst_scanner PUBLIC cxx_std_17)

target_link_libraries(vst_scanner PRIVATE
    sdk_hosting
    sdk_common
    base
    pluginterfaces
)

target_include_directories(vst_scanner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${VST3_SDK_ROOT}
)

# Platform-specific Module::create implementation (required for hosting)
if(APPLE)
    target_sources(vst_scanner PRIVATE
        ${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/module_mac.mm
    )
    set_source_files_properties(
        ${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/module_mac.mm
        PROPERTIES COMPILE_FLAGS "-fobjc-arc"
    )
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
    find_library(CORESERVICES_FRAMEWORK CoreServices)
    target_link_libraries(vst_scanner PRIVATE
        ${COREFOUNDATION_FRAMEWORK}
        ${CORESERVICES_FRAMEWORK}
    )
    if(NOT XCODE)
        find_library(COCOA_FRAMEWORK Cocoa)
        target_link_libraries(vst_scanner PRIVATE ${COCOA_FRAMEWORK})
    endif()
elseif(UNIX AND NOT APPLE)
    target_sources(vst_scanner PRIVATE
        ${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/module_linux.cpp
    )
    target_link_libraries(vst_scanner PRIVATE dl pthread)
elseif(WIN32)
    target_sources(vst_scanner PRIVATE
        ${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/module_win32.cpp
    )
    target_link_libraries(vst_scanner PRIVATE
        kernel32 user32 shell32 ole32 uuid advapi32 oleaut32
    )
endif()

if(MINGW)
    set_target_properties(vst_scanner PROPERTIES LINK_FLAGS "-municode")
endif()

set_target_properties(vst_scanner PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

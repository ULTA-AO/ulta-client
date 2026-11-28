# Configure cmake
cmake_minimum_required(VERSION 3.1.0)
cmake_policy(SET CMP0076 NEW) # silence warning

project(AttorneyOnline)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(CMAKE_VERSION VERSION_LESS "3.7.0")
    set(CMAKE_INCLUDE_CURRENT_DIR ON)
endif()

set(ULTA_VERSION "1.0.0" CACHE STRING "ULTA AO client version")

# AO
add_executable(Attorney_Online resources.qrc)

# WIN32
if(WIN32)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set_property(TARGET Attorney_Online PROPERTY WIN32_EXECUTABLE true)
        set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/resource/logo_ao2.rc")
        target_sources(Attorney_Online PRIVATE ${APP_ICON_RESOURCE_WINDOWS})
    endif()
endif()

# macOS app bundle
if(APPLE)
    set(ULTA_ICNS "${CMAKE_CURRENT_SOURCE_DIR}/resource/logo_ao2.icns")
    target_sources(Attorney_Online PRIVATE ${ULTA_ICNS})
    set_source_files_properties(${ULTA_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(Attorney_Online PROPERTIES
        MACOSX_BUNDLE TRUE
        OUTPUT_NAME "ULTA AO"
        MACOSX_BUNDLE_BUNDLE_NAME "ULTA AO"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.ulta.ao"
        MACOSX_BUNDLE_ICON_FILE "logo_ao2.icns"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${ULTA_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${ULTA_VERSION}"
        MACOSX_BUNDLE_INFO_STRING "ULTA AO")
endif()

# Target Include
target_include_directories(Attorney_Online PRIVATE include)

# Target Lib
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Gui Network Widgets Concurrent WebSockets UiTools REQUIRED)
target_link_directories(Attorney_Online PRIVATE lib)
target_link_libraries(Attorney_Online PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Concurrent
    Qt${QT_VERSION_MAJOR}::WebSockets Qt${QT_VERSION_MAJOR}::UiTools bass bassopus)
if(NOT APPLE)
    target_link_libraries(Attorney_Online PRIVATE bassmidi discord-rpc)
    target_compile_definitions(Attorney_Online PRIVATE DISCORD)
endif()
# Optional Discord presence on macOS, point ULTA_DISCORD_DIR at a built discord-rpc
if(APPLE AND ULTA_DISCORD_DIR)
    target_include_directories(Attorney_Online PRIVATE ${ULTA_DISCORD_DIR}/include)
    target_link_libraries(Attorney_Online PRIVATE ${ULTA_DISCORD_DIR}/libdiscord-rpc.a
        "-framework AppKit" "-framework Foundation")
    target_compile_definitions(Attorney_Online PRIVATE DISCORD)
endif()

target_compile_definitions(Attorney_Online PRIVATE ULTA_CLIENT_VERSION="${ULTA_VERSION}")

# ULTA: locked-server build (see include/ulta_lock.h). Empty = open dev client.
set(ULTA_LOCKED_SERVER "" CACHE STRING "Lock the client to this server, host:port (TCP)")
set(ULTA_LOCKED_NAME "ULTA AO" CACHE STRING "Display name of the locked server")
if(ULTA_LOCKED_SERVER)
    target_compile_definitions(Attorney_Online PRIVATE
        ULTA_LOCKED_SERVER="${ULTA_LOCKED_SERVER}" ULTA_LOCKED_NAME="${ULTA_LOCKED_NAME}")
endif()

# ULTA: self-update. An https address of latest.json turns the update check on.
# The release list is signed (see tools/sign_release.py); the client only trusts
# the public key in include/ulta_update_key.h.
set(ULTA_UPDATE_URL "" CACHE STRING "Address of latest.json for the update check (empty = no update check)")
option(ULTA_UPDATE_ALLOW_HTTP "Allow an http update address (local tests only)" OFF)
target_sources(Attorney_Online PRIVATE src/ulta_updater.cpp third_party/tweetnacl/tweetnacl.c)
if(ULTA_UPDATE_URL)
    target_compile_definitions(Attorney_Online PRIVATE ULTA_UPDATE_URL="${ULTA_UPDATE_URL}")
endif()
if(ULTA_UPDATE_ALLOW_HTTP)
    target_compile_definitions(Attorney_Online PRIVATE ULTA_UPDATE_ALLOW_HTTP)
endif()
if(WIN32)
    # The updater program: no Qt, fully static, so it runs from any folder.
    add_executable(ulta-updater updater/ulta_updater_main.cpp third_party/miniz/miniz.c)
    set_target_properties(ulta-updater PROPERTIES AUTOMOC OFF AUTORCC OFF AUTOUIC OFF)
    target_include_directories(ulta-updater PRIVATE third_party/miniz)
    target_link_options(ulta-updater PRIVATE -municode -static -static-libgcc -static-libstdc++)
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(Attorney_Online PRIVATE "-framework CoreFoundation" "-framework Foundation" "-framework CoreServices")
endif()

# Subdirectories
add_subdirectory(src)
add_subdirectory(include)

# Pick up the widget sources and all headers
file(GLOB_RECURSE AO_WIDGET_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/*.cpp)
file(GLOB_RECURSE AO_ALL_HEADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)
target_sources(Attorney_Online PRIVATE ${AO_WIDGET_SOURCES} ${AO_ALL_HEADERS})

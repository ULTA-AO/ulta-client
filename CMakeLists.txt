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

# Target Include
target_include_directories(Attorney_Online PRIVATE include)

# Target Lib
find_package(Qt5 COMPONENTS Core Gui Network Widgets Concurrent WebSockets UiTools REQUIRED)
target_link_directories(Attorney_Online PRIVATE lib)
target_link_libraries(Attorney_Online PRIVATE Qt5::Core Qt5::Gui Qt5::Network Qt5::Widgets Qt5::Concurrent
    Qt5::WebSockets Qt5::UiTools bass bassmidi bassopus discord-rpc)
target_compile_definitions(Attorney_Online PRIVATE DISCORD)

# ULTA: client version shown in the lobby and the About dialog.
set(ULTA_VERSION "1.0.0" CACHE STRING "ULTA AO client version")
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

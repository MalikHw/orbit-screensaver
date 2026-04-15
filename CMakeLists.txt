cmake_minimum_required(VERSION 3.16)
project(orbit_screensaver)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -s")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

if(NOT DEFINED APP_VERSION)
    set(APP_VERSION "dev")
endif()
add_compile_definitions(APP_VERSION="${APP_VERSION}")

find_package(SDL2 REQUIRED)
find_package(SDL2_image QUIET)

set(BOX2D_BUILD_TESTBED OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(extern/box2d)

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/extern/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl2.cpp
)

# ${CMAKE_BINARY_DIR}/generated/  e.g. orb1_data.h, cube_data.h, logo_data.h
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GENERATED_DIR}")

set(ASSET_HEADERS "")

# orb1..orb10, cube, logo
set(ASSET_FILES "")
foreach(IDX RANGE 1 10)
    list(APPEND ASSET_FILES "orb${IDX}.png")
endforeach()
list(APPEND ASSET_FILES "cube.png" "logo.png")

foreach(ASSET_FILE ${ASSET_FILES})
    set(ASSET_PATH "${CMAKE_SOURCE_DIR}/${ASSET_FILE}")
    # derive variable name: orb1.png -> orb1_data
    string(REGEX REPLACE "\\.png$" "" VARNAME "${ASSET_FILE}")
    string(APPEND VARNAME "_data")
    set(HEADER_PATH "${GENERATED_DIR}/${VARNAME}.h")

    add_custom_command(
        OUTPUT  "${HEADER_PATH}"
        COMMAND ${CMAKE_COMMAND} -E echo "Embedding ${ASSET_FILE}..."
        COMMAND python3 "${CMAKE_SOURCE_DIR}/tools/bin2h.py"
                        "${ASSET_PATH}"
                        "${HEADER_PATH}"
                        "${VARNAME}"
        DEPENDS "${ASSET_PATH}" "${CMAKE_SOURCE_DIR}/tools/bin2h.py"
        COMMENT "Generating header for ${ASSET_FILE}"
    )
    list(APPEND ASSET_HEADERS "${HEADER_PATH}")
endforeach()

add_custom_target(generate_asset_headers ALL DEPENDS ${ASSET_HEADERS})

# main screensaver
add_executable(orbit_screensaver WIN32 main.cpp ${IMGUI_SOURCES})
add_dependencies(orbit_screensaver generate_asset_headers)

if(EXISTS "${CMAKE_SOURCE_DIR}/icon.rc")
    target_sources(orbit_screensaver PRIVATE icon.rc)
endif()

target_include_directories(orbit_screensaver PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${GENERATED_DIR}
)
target_link_libraries(orbit_screensaver PRIVATE
    ${SDL2_LIBRARIES} SDL2_image::SDL2_image box2d
    opengl32 shell32 comdlg32 gdi32 winmm advapi32 winhttp urlmon)
set_target_properties(orbit_screensaver PROPERTIES OUTPUT_NAME "orbit_screensaver" SUFFIX ".scr")

# updater
add_executable(updater WIN32 updater.cpp)
if(EXISTS "${CMAKE_SOURCE_DIR}/icon.rc")
    target_sources(updater PRIVATE icon.rc)
endif()
target_link_libraries(updater PRIVATE shell32 winhttp advapi32 ole32 oleaut32 urlmon)
set_target_properties(updater PROPERTIES OUTPUT_NAME "updater")

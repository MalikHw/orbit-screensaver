cmake_minimum_required(VERSION 3.16)
project(orbit_screensaver)

set(CMAKE_CXX_STANDARD 17)

# release flags - small and fast
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /GL /GS-")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -s")
endif()

find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)

# box2d - try find_package first, fall back to subdir
find_package(box2d QUIET)
if(NOT box2d_FOUND)
    if(EXISTS "${CMAKE_SOURCE_DIR}/extern/box2d/CMakeLists.txt")
        set(BOX2D_BUILD_TESTBED OFF CACHE BOOL "" FORCE)
        set(BOX2D_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
        add_subdirectory(extern/box2d)
    else()
        message(FATAL_ERROR "box2d not found. Install it or clone into extern/box2d")
    endif()
endif()

add_executable(orbit_screensaver main.cpp)

target_include_directories(orbit_screensaver PRIVATE
    ${SDL2_INCLUDE_DIRS}
)

if(WIN32)
    # windows: no console window, link opengl32
    set_target_properties(orbit_screensaver PROPERTIES
        WIN32_EXECUTABLE TRUE
        OUTPUT_NAME "orbit_screensaver"
    )
    target_link_libraries(orbit_screensaver PRIVATE
        ${SDL2_LIBRARIES}
        SDL2_image::SDL2_image
        box2d
        opengl32
        shell32
        comdlg32
        comctl32
        gdi32
        winmm
        dwmapi
    )
    # icon
    if(EXISTS "${CMAKE_SOURCE_DIR}/icon.rc")
        target_sources(orbit_screensaver PRIVATE icon.rc)
    endif()
else()
    # linux: link GL and dl
    target_link_libraries(orbit_screensaver PRIVATE
        ${SDL2_LIBRARIES}
        SDL2_image
        box2d
        GL
        dl
        m
    )
endif()

# copy assets next to binary
set(ASSETS
    orb1.png orb2.png orb3.png orb4.png orb5.png
    orb6.png orb7.png orb8.png orb9.png orb10.png
    cube.png
)
foreach(ASSET ${ASSETS})
    if(EXISTS "${CMAKE_SOURCE_DIR}/${ASSET}")
        add_custom_command(TARGET orbit_screensaver POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/${ASSET}"
                "$<TARGET_FILE_DIR:orbit_screensaver>/${ASSET}"
        )
    endif()
endforeach()

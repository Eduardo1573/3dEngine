cmake_minimum_required(VERSION 3.16)

project(Polygon3DEngineCPP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(gpu_polygon_engine main.cpp)

target_compile_definitions(gpu_polygon_engine PRIVATE
    PROJECT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

if(APPLE)
    target_link_libraries(gpu_polygon_engine PRIVATE
        "-framework OpenGL"
        "-framework GLUT"
        "-framework ApplicationServices"
    )
else()
    find_package(OpenGL REQUIRED)
    find_package(GLUT REQUIRED)
    target_include_directories(gpu_polygon_engine PRIVATE ${OPENGL_INCLUDE_DIRS} ${GLUT_INCLUDE_DIRS})

    if(TARGET OpenGL::GL)
        target_link_libraries(gpu_polygon_engine PRIVATE OpenGL::GL)
    else()
        target_link_libraries(gpu_polygon_engine PRIVATE ${OPENGL_LIBRARIES})
    endif()

    if(TARGET GLUT::GLUT)
        target_link_libraries(gpu_polygon_engine PRIVATE GLUT::GLUT)
    else()
        target_link_libraries(gpu_polygon_engine PRIVATE ${GLUT_LIBRARIES})
    endif()

    if(TARGET OpenGL::GLU)
        target_link_libraries(gpu_polygon_engine PRIVATE OpenGL::GLU)
    elseif(WIN32)
        target_link_libraries(gpu_polygon_engine PRIVATE glu32)
    endif()

    if(WIN32)
        target_compile_definitions(gpu_polygon_engine PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
        target_link_libraries(gpu_polygon_engine PRIVATE ws2_32)
    endif()
endif()

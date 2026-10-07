# ============================================================================
#  HandBoard - draw on a whiteboard with your hand (OpenCV DNN + MediaPipe)
#
#    main.cpp        the program (WinMain)
#    lib/            headers (.hpp)          -> lib/CMakeLists.txt
#    src/            implementations (.cpp)  -> src/CMakeLists.txt
#    models/         hand models (.onnx)
#
#  rregullo.bat installs everything; or manually:
#    cmake -B build -DOpenCV_DIR=C:/opencv-4.14.0/opencv/build
#    cmake --build build --config Release
# ============================================================================
cmake_minimum_required(VERSION 3.15)
project(HandBoard CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()
# the exe always ends up in build/Release (or build/Debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>)

# --- OpenCV (>= 4.7 for the MediaPipe models) ---
if(NOT OpenCV_DIR AND EXISTS "C:/opencv-4.14.0/opencv/build/OpenCVConfig.cmake")
    set(OpenCV_DIR "C:/opencv-4.14.0/opencv/build")
endif()
find_package(OpenCV 4.7 REQUIRED COMPONENTS core imgproc dnn)

add_subdirectory(lib)   # handboard_headers
add_subdirectory(src)   # handboard_core

# --- the program ---
add_executable(HandBoard WIN32 main.cpp)
target_link_libraries(HandBoard PRIVATE handboard_core)
if(MSVC)
    target_compile_options(HandBoard PRIVATE /W4 /utf-8)
endif()

# --- after build: OpenCV DLL and models next to the exe ---
if(TARGET opencv_world)
    add_custom_command(TARGET HandBoard POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:opencv_world> $<TARGET_FILE_DIR:HandBoard>)
endif()
add_custom_command(TARGET HandBoard POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/models $<TARGET_FILE_DIR:HandBoard>/models)

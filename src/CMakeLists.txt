# src/ - HandBoard implementations (.cpp), as a static library.
add_library(handboard_core STATIC
    Application.cpp
    Camera.cpp
    Gestures.cpp
    HandDetector.cpp
    InputSender.cpp
    MainWindow.cpp
    Preview.cpp
    SharedState.cpp
    Utils.cpp
    Worker.cpp)
target_link_libraries(handboard_core PUBLIC handboard_headers mfplat mfreadwrite mf mfuuid ole32 user32 gdi32)

if(MSVC)
    # /MD (DLL) runtime, same as OpenCV - mixing with /MT causes crashes
    target_compile_options(handboard_core PRIVATE /W4 /utf-8)
    target_compile_definitions(handboard_core PUBLIC _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(handboard_core PRIVATE -Wall -Wextra)
endif()

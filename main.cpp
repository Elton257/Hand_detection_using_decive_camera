// ============================================================================
//  HandBoard - draw on a whiteboard (Teams / Slack / Meet) with your hand
//
//  Hands are tracked with OpenCV DNN and the MediaPipe models (OpenCV Zoo, Apache 2.0).
//
//  Layout:
//    main.cpp          this file: creates the Application and runs it
//    lib/*.hpp         class declarations (headers)
//    src/*.cpp         class implementations
//
//  Classes:
//    Application         owns everything below and runs the message loop
//    SharedState         state shared between the window and the worker thread
//    MainWindow          preview window and the Ctrl+Alt+F7..F11 shortcuts
//    Worker              loop: camera -> hands -> gestures -> mouse
//    Camera              webcam (Media Foundation)
//    HandDetector        palm + 21 hand landmarks (OpenCV DNN)
//    GestureClassifier   from landmarks to gestures
//    OneEuro, Debounce   cursor smoothing, flicker-free gestures
//    PreviewRenderer     small preview with the hand skeleton
//    InputSender         mouse/keyboard (SendInput)
//    Logger, Clock, Paths  HandBoard.log, timing, file paths
// ============================================================================
#include "Application.hpp"

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    Application app(hInst);
    return app.run();
}

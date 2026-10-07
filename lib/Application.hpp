// ============================================================================
//  Application.hpp - owns every part of HandBoard and runs it
//
//      SharedState  <- the window and the worker only talk through this
//      MainWindow   (UI thread)
//      Worker       (own thread: camera -> hands -> gestures -> mouse)
// ============================================================================
#pragma once
#include "MainWindow.hpp"
#include "SharedState.hpp"
#include "WinCommon.hpp"
#include "Worker.hpp"

#include <thread>

class Application {
public:
    explicit Application(HINSTANCE hInst) : hInst_(hInst) {}
    ~Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // Runs until the window is closed. Returns the process exit code.
    int run();

private:
    bool acquireSingleInstance();        // only one copy (two would fight over the mouse)
    void stopWorker();

    HINSTANCE hInst_;
    HANDLE instanceMutex_ = nullptr;
    // Order matters: state_ is created first and destroyed last.
    SharedState state_;
    MainWindow window_{state_};
    Worker worker_{state_};
    std::thread workerThread_;
};

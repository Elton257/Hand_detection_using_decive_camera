#include "Application.hpp"
#include "Utils.hpp"

namespace {
// If the program crashes, write the code to HandBoard.log before it exits.
LONG WINAPI OnCrash(EXCEPTION_POINTERS* info) {
    Logger::write(L"CRASH: kodi 0x%08lX ne adresen %p",
                  (unsigned long)info->ExceptionRecord->ExceptionCode, info->ExceptionRecord->ExceptionAddress);
    return EXCEPTION_CONTINUE_SEARCH;
}
}  // namespace

Application::~Application() {
    stopWorker();
    if (instanceMutex_) {
        ReleaseMutex(instanceMutex_);
        CloseHandle(instanceMutex_);
    }
}

bool Application::acquireSingleInstance() {
    instanceMutex_ = CreateMutexW(nullptr, TRUE, L"HandBoard_SingleInstance_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        Logger::write(L"Nje kopje tjeter e HandBoard eshte e hapur - kjo mbyllet");
        MessageBoxW(nullptr, L"HandBoard është tashmë i hapur.", L"HandBoard", MB_ICONINFORMATION);
        if (instanceMutex_) CloseHandle(instanceMutex_);
        instanceMutex_ = nullptr;
        return false;
    }
    return true;
}

int Application::run() {
    if (!acquireSingleInstance()) return 0;
    SetUnhandledExceptionFilter(OnCrash);
    Logger::write(L"Nisja e HandBoard");
    SetProcessDPIAware();   // real screen coordinates

    if (!window_.create(hInst_)) return 1;
    workerThread_ = std::thread([this] { worker_.run(); });

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    stopWorker();
    Logger::write(L"HandBoard u mbyll normalisht");
    return 0;
}

void Application::stopWorker() {
    state_.running = false;
    if (workerThread_.joinable()) workerThread_.join();
}

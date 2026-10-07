#include "Worker.hpp"

#include "Config.hpp"
#include "Utils.hpp"

#include <mfapi.h>
#include <opencv2/core/utility.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <exception>
#include <string>

void Worker::run() {
    const HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const HRESULT hrMf = MFStartup(MF_VERSION, MFSTARTUP_LITE);
    cv::setNumThreads(cfg::DNN_THREADS);
    mouse_.refresh();
    lastTime_ = fpsTime_ = lastScreenCheck_ = Clock::seconds();

    if (FAILED(hrMf)) {
        state_.setStatus(L"Media Foundation nuk u nis (Windows N? instalo Media Feature Pack).");
        state_.post(WM_APP_FRAME);
    } else if (loadModels()) {
        while (state_.running.load()) {
            try {
                if (!cameraOk_ && !openCamera()) continue;
                processFrame();
            } catch (const std::exception& e) {
                // no unexpected error should crash the program
                mouse_.release();
                Logger::write(L"Gabim i papritur ne ciklin e punes: %.300hs", e.what());
                Sleep(200);
            }
        }
    }

    mouse_.release();
    camera_.close();
    if (SUCCEEDED(hrMf)) MFShutdown();
    if (SUCCEEDED(hrCo)) CoUninitialize();
}

bool Worker::loadModels() {
    detector_.palmScoreThreshold = cfg::PALM_SCORE;
    detector_.handConfThreshold = cfg::HAND_CONF;
    const std::wstring dir = Paths::exeDir();
    std::string err;
    bool ok = false;
    try {
        ok = detector_.load(Paths::toUtf8(dir + cfg::PALM_MODEL), Paths::toUtf8(dir + cfg::HAND_MODEL), err);
    } catch (...) {
        ok = false;
    }
    if (!ok) {
        Logger::write(L"Modelet nuk u ngarkuan: %.300hs", err.c_str());
        state_.setStatus(L"Modelet mungojnë: dosja \"models\" duhet të jetë pranë HandBoard.exe (hap rregullo.bat).");
        state_.post(WM_APP_FRAME);
    }
    return ok;
}

bool Worker::openCamera() {
    if (!camera_.open()) {
        if (!cameraFailLogged_) {
            Logger::write(L"Kamera nuk u gjet ose eshte e zene. Po provoj prape...");
            cameraFailLogged_ = true;
        }
        state_.setStatus(L"Kamera nuk u gjet ose e zënë nga një program tjetër. Po provoj prapë...");
        state_.post(WM_APP_FRAME);
        for (int i = 0; i < 20 && state_.running.load(); ++i) Sleep(100);
        return false;
    }
    // ---------- allocations happen only here ----------
    frame_.assign((size_t)camera_.width * camera_.height, 0);
    previewW_ = cfg::PREVIEW_W;
    previewH_ = std::max(1, (int)std::lround((double)previewW_ * camera_.height / camera_.width));
    preview_.assign((size_t)previewW_ * previewH_, 0);
    state_.resizePreview(previewW_, previewH_);
    state_.post(WM_APP_SIZE, (WPARAM)previewW_, (LPARAM)previewH_);
    cameraOk_ = true;
    cameraFailLogged_ = false;
    Logger::write(L"Kamera u hap: %dx%d", camera_.width, camera_.height);
    return true;
}

void Worker::processFrame() {
    const int rr = camera_.read(frame_.data(), frame_.size());
    if (rr < 0) {
        Logger::write(L"Kamera humbi lidhjen (u fik, u hoq, ose e mori nje program tjeter). Po e rihap...");
        mouse_.release();
        camera_.close();
        cameraOk_ = false;
        Sleep(300);
        return;
    }
    if (rr == 0) return;

    const double t = Clock::seconds();
    const float dt = (float)std::min(0.2, std::max(1.0 / 120.0, t - lastTime_));
    lastTime_ = t;
    ++fpsFrames_;
    if (t - fpsTime_ >= 1.0) { fps_ = (float)(fpsFrames_ / (t - fpsTime_)); fpsFrames_ = 0; fpsTime_ = t; }
    if (t - lastScreenCheck_ > 2.0) { mouse_.refresh(); lastScreenCheck_ = t; }

    handleSnapshot();

    FrameResult r;
    detectHands(r);
    assignHands(r);
    readGestures(r, dt);
    applyActions(r);
    publish(r);
}

// ---------- diagnostic snapshot (Ctrl+Alt+F11) ----------
void Worker::handleSnapshot() {
    if (!state_.snapshotRequest.exchange(false)) return;
    wchar_t name[64], b[160];
    if (saveSnapshot(name, 64)) swprintf(b, 160, L"Foto u ruajt: %ls", name);
    else wcscpy(b, L"Foto nuk u ruajt dot");
    state_.setMessage(b, 4000);
}

// ---------- hands (OpenCV DNN) ----------
void Worker::detectHands(FrameResult& r) {
    const cv::Mat bgra(camera_.height, camera_.width, CV_8UC4, frame_.data());
    cv::cvtColor(bgra, bgr_, cv::COLOR_BGRA2BGR);
    cv::flip(bgr_, mirrored_, 1);                     // mirror: like looking into a mirror
    try {
        r.nHands = detector_.detect(mirrored_, hands_);
    } catch (const cv::Exception& e) {
        r.nHands = 0;
        const ULONGLONG now = GetTickCount64();
        if (now - lastDnnErrorLog_ > 60000) {
            Logger::write(L"Gabim OpenCV: %.300hs", e.what());
            lastDnnErrorLog_ = now;
        }
    }
}

// Which hand is which: the model says so; if both come out the same, use their position.
void Worker::assignHands(FrameResult& r) const {
    const bool swp = state_.swapHands.load();
    for (int k = 0; k < r.nHands; ++k) {
        const bool right = hands_[k].isRight() != swp;
        if (right && r.rightIdx < 0) r.rightIdx = k;
        else if (!right && r.leftIdx < 0) r.leftIdx = k;
        else if (r.rightIdx < 0) r.rightIdx = k;
        else if (r.leftIdx < 0) r.leftIdx = k;
    }
    if (r.nHands == 2 && hands_[0].isRight() == hands_[1].isRight()) {
        const bool zeroIsRight = (hands_[0].pts[WRIST].x > hands_[1].pts[WRIST].x) != swp;
        r.rightIdx = zeroIsRight ? 0 : 1;
        r.leftIdx = zeroIsRight ? 1 : 0;
    }
}

// ---------- gestures + fingertip -> screen point ----------
void Worker::readGestures(FrameResult& r, float dt) {
    const RState rawR = (r.rightIdx >= 0) ? GestureClassifier::right(hands_[r.rightIdx], r.tipX, r.tipY) : RState::None;
    r.hasTip = (rawR == RState::Point || rawR == RState::Draw || rawR == RState::Erase);
    const bool pressing = (rawR == RState::Draw || rawR == RState::Erase);
    r.right = rightDeb_.update(rawR, pressing ? cfg::FRAMES_TO_PRESS : cfg::FRAMES_TO_RELEASE);
    const LState rawL = (r.leftIdx >= 0) ? GestureClassifier::left(hands_[r.leftIdx]) : LState::None;
    r.left = leftDeb_.update(rawL, rawL == LState::None ? 1 : cfg::FRAMES_ZOOM);

    if (r.hasTip) lostFrames_ = 0; else if (lostFrames_ < 999) ++lostFrames_;
    if (lostFrames_ >= 4) { filterX_.reset(); filterY_.reset(); }

    if (r.hasTip) {
        float u = (r.tipX / mirrored_.cols - cfg::AREA_X0) / (cfg::AREA_X1 - cfg::AREA_X0);
        float v = (r.tipY / mirrored_.rows - cfg::AREA_Y0) / (cfg::AREA_Y1 - cfg::AREA_Y0);
        u = std::min(1.f, std::max(0.f, u));
        v = std::min(1.f, std::max(0.f, v));
        r.screenX = (int)std::lround(filterX_.filter(u * (mouse_.sw - 1), dt));
        r.screenY = (int)std::lround(filterY_.filter(v * (mouse_.sh - 1), dt));
    }
}

// ---------- actions ----------
void Worker::applyActions(FrameResult& r) {
    if (!state_.active.load()) {
        mouse_.release();
        return;
    }
    const RState st = r.right;
    if (st == RState::Erase && !eraserTool_ && state_.hasEraser()) { switchTool(true, r.screenX, r.screenY);  eraserTool_ = true; }
    if (st == RState::Draw  && eraserTool_  && state_.hasPen())    { switchTool(false, r.screenX, r.screenY); eraserTool_ = false; }
    bool wantDown = (st == RState::Draw && !eraserTool_) || (st == RState::Erase && eraserTool_);

    // safety: a big jump while drawing is a detection error
    if (r.hasTip && mouse_.down && sentX_ >= 0) {
        const float jx = (float)(r.screenX - sentX_) / mouse_.sw, jy = (float)(r.screenY - sentY_) / mouse_.sh;
        if (std::sqrt(jx * jx + jy * jy) > cfg::MAX_JUMP) {
            r.hasTip = false;
            if (++glitches_ >= 3) wantDown = false;
        } else {
            glitches_ = 0;
        }
    }
    const bool moving = (st == RState::Point || st == RState::Draw || st == RState::Erase);
    if (r.hasTip && moving) { mouse_.move(r.screenX, r.screenY); sentX_ = r.screenX; sentY_ = r.screenY; }
    if (wantDown && !mouse_.down && r.hasTip) mouse_.press();
    if (!wantDown && mouse_.down) mouse_.release();
    if (!mouse_.down) glitches_ = 0;

    if (!mouse_.down && (r.left == LState::ZoomIn || r.left == LState::ZoomOut)) {
        const ULONGLONG now = GetTickCount64();
        if (now - lastZoom_ >= cfg::ZOOM_INTERVAL_MS) {
            mouse_.ctrlWheel(r.left == LState::ZoomIn ? WHEEL_DELTA : -WHEEL_DELTA);
            lastZoom_ = now;
        }
    }
}

// ---------- preview + status ----------
void Worker::publish(const FrameResult& r) {
    PreviewInfo pi;
    pi.nHands = r.nHands;
    pi.hands = hands_;
    pi.rightIdx = r.rightIdx;
    pi.leftIdx = r.leftIdx;
    pi.tipOk = r.hasTip;
    pi.tipX = r.tipX;
    pi.tipY = r.tipY;
    pi.state = r.right;
    renderer_.render(mirrored_, pi, previewW_, previewH_, preview_.data());

    const wchar_t* lsTxt = (r.left == LState::ZoomIn) ? L"ZOOM +" : (r.left == LState::ZoomOut) ? L"ZOOM −" : L"—";
    wchar_t l0[160], l1[160], l2[160];
    swprintf(l0, 160, L"%ls   %.0f fps", state_.active.load() ? L"● AKTIV" : L"❚❚ PAUZË  (Ctrl+Alt+F8)", fps_);
    swprintf(l1, 160, L"Djathtas: %ls", rightText(r.right, eraserTool_));
    swprintf(l2, 160, L"Majtas: %ls%ls", lsTxt, state_.swapHands.load() ? L"   [duart të ndërruara]" : L"");
    state_.publish(preview_.data(), preview_.size(), l0, l1, l2);
    state_.post(WM_APP_FRAME);
}

// Clicks the pen or eraser icon and moves the cursor back where it was.
void Worker::switchTool(bool toEraser, int retX, int retY) {
    const POINT target = toEraser ? state_.eraser() : state_.pen();
    if (retX < 0) { POINT p; GetCursorPos(&p); retX = p.x; retY = p.y; }
    mouse_.release();
    mouse_.move(target.x, target.y); Sleep(25);
    mouse_.click();                  Sleep(45);
    mouse_.move(retX, retY);         Sleep(15);
}

// Saves the camera frame as a BMP next to HandBoard.exe (for diagnostics).
bool Worker::saveSnapshot(wchar_t* outName, size_t outLen) const {
    const std::wstring dir = Paths::exeDir();
    if (dir.empty()) return false;
    const int w = camera_.width, h = camera_.height;
    for (int k = 1; k < 1000; ++k) {
        wchar_t name[64];
        swprintf(name, 64, L"HandBoard_foto_%d.bmp", k);
        const std::wstring path = dir + name;
        // CREATE_NEW: never overwrites an existing photo
        HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f == INVALID_HANDLE_VALUE) {
            if (GetLastError() == ERROR_FILE_EXISTS) continue;
            return false;
        }
        const DWORD imgBytes = (DWORD)w * (DWORD)h * 4;
        BITMAPFILEHEADER fh = {};
        BITMAPINFOHEADER ih = {};
        fh.bfType = 0x4D42;
        fh.bfOffBits = sizeof(fh) + sizeof(ih);
        fh.bfSize = fh.bfOffBits + imgBytes;
        ih.biSize = sizeof(ih);
        ih.biWidth = w;
        ih.biHeight = -h;            // top-down
        ih.biPlanes = 1;
        ih.biBitCount = 32;
        ih.biCompression = BI_RGB;
        DWORD w1 = 0, w2 = 0, w3 = 0;
        const bool ok = WriteFile(f, &fh, sizeof(fh), &w1, nullptr) && WriteFile(f, &ih, sizeof(ih), &w2, nullptr) &&
                        WriteFile(f, frame_.data(), imgBytes, &w3, nullptr) && w3 == imgBytes;
        CloseHandle(f);
        wcsncpy(outName, name, outLen - 1);
        outName[outLen - 1] = 0;
        return ok;
    }
    return false;
}

const wchar_t* Worker::rightText(RState st, bool eraserTool) {
    switch (st) {
        case RState::Point: return L"LËVIZ (pa shkruar)";
        case RState::Draw:  return eraserTool ? L"SHKRUAN (ruaj lapsin: Ctrl+Alt+F9)" : L"SHKRUAN";
        case RState::Erase: return eraserTool ? L"FSHIN" : L"FSHIN (ruaj gomën: Ctrl+Alt+F10)";
        case RState::Idle:  return L"pret";
        default:            return L"nuk shihet";
    }
}

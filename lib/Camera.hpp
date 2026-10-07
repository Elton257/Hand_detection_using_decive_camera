// ============================================================================
//  Camera.hpp - camera capture with Media Foundation (part of Windows)
// ============================================================================
#pragma once
#include "WinCommon.hpp"
#include "ComPtr.hpp"

#include <mfidl.h>
#include <mfreadwrite.h>

#include <cstddef>
#include <cstdint>

class Camera {
public:
    int width = 0, height = 0;

    ~Camera() { close(); }

    // Opens the first camera, picks a ~640px, >= 25 fps format, outputs RGB32.
    bool open();

    // 1 = frame read into dst, 0 = no frame yet (try again), -1 = error (reopen the camera)
    int read(uint32_t* dst, size_t cap);

    void close();

private:
    ComPtr<IMFMediaSource> source_;
    ComPtr<IMFSourceReader> reader_;
    LONG stride_ = 0;
};

#include "SharedState.hpp"

#include <cstring>
#include <cwchar>

namespace {
void Copy(wchar_t* dst, const wchar_t* src) {
    wcsncpy(dst, src ? src : L"", 159);
    dst[159] = 0;
}
}  // namespace

void SharedState::resizePreview(int w, int h) {
    std::lock_guard<std::mutex> lk(m_);
    img_.assign((size_t)w * h, 0xFF000000u);
    w_ = w;
    h_ = h;
}

void SharedState::publish(const uint32_t* bgra, size_t count,
                          const wchar_t* l0, const wchar_t* l1, const wchar_t* l2) {
    std::lock_guard<std::mutex> lk(m_);
    if (img_.size() == count) std::memcpy(img_.data(), bgra, count * sizeof(uint32_t));
    Copy(lines_[0], l0);
    Copy(lines_[1], l1);
    Copy(lines_[2], l2);
}

void SharedState::setStatus(const wchar_t* l0) {
    std::lock_guard<std::mutex> lk(m_);
    Copy(lines_[0], l0);
    lines_[1][0] = lines_[2][0] = 0;
}

void SharedState::setMessage(const wchar_t* text, DWORD ms) {
    std::lock_guard<std::mutex> lk(m_);
    Copy(msg_, text);
    msgUntil_ = GetTickCount64() + ms;
}

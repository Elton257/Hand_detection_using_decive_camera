// ============================================================================
//  ComPtr.hpp - tiny RAII wrapper for COM objects (released automatically)
// ============================================================================
#pragma once

template <class T>
class ComPtr {
public:
    ComPtr() = default;
    ~ComPtr() { reset(); }
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    T* get() const { return p_; }
    T* operator->() const { return p_; }
    T** put() { reset(); return &p_; }          // for output parameters
    void reset() { if (p_) { p_->Release(); p_ = nullptr; } }
    explicit operator bool() const { return p_ != nullptr; }

private:
    T* p_ = nullptr;
};

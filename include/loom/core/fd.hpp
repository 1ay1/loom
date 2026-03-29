#pragma once

// ═══════════════════════════════════════════════════════════════════════
//  fd.hpp — RAII file descriptor wrapper
//
//  An affine type: move-only, auto-closing. Substructural discipline
//  prevents double-close and leak. The deleted copy constructor removes
//  the contraction rule; the destructor handles weakening (cleanup on
//  discard). See the type-theoretic C++ series, Part 8.
// ═══════════════════════════════════════════════════════════════════════

#include <unistd.h>
#include <utility>

namespace loom
{

class FileDescriptor
{
public:
    FileDescriptor() = default;
    explicit FileDescriptor(int fd) noexcept : fd_(fd) {}

    ~FileDescriptor()
    {
        if (fd_ >= 0) ::close(fd_);
    }

    // Affine: no contraction (copying)
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;

    // Transfer of ownership (linear implication: A ⊸ A)
    FileDescriptor(FileDescriptor&& other) noexcept
        : fd_(other.fd_)
    {
        other.fd_ = -1;
    }

    FileDescriptor& operator=(FileDescriptor&& other) noexcept
    {
        if (this != &other)
        {
            if (fd_ >= 0) ::close(fd_);
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    int get() const noexcept { return fd_; }

    // Consume: release ownership (the caller takes responsibility)
    [[nodiscard]] int release() noexcept
    {
        int fd = fd_;
        fd_ = -1;
        return fd;
    }

    explicit operator bool() const noexcept { return fd_ >= 0; }

private:
    int fd_ = -1;
};

} // namespace loom

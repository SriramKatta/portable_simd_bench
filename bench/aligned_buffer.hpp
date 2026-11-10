#pragma once

#include <cstddef>
#include <new>
#include <type_traits>

// Minimal RAII aligned array of trivially-copyable T. Deliberately independent
// of xsimd and Highway: the harness must not depend on a library under test.
template <typename T>
class aligned_buffer
{
    static_assert(std::is_trivially_copyable_v<T>);

public:
    aligned_buffer(std::size_t count, std::size_t alignment)
        : data_(static_cast<T *>(::operator new[](count * sizeof(T), std::align_val_t(alignment)))),
          size_(count),
          alignment_(alignment)
    {
    }

    ~aligned_buffer()
    {
        ::operator delete[](data_, std::align_val_t(alignment_));
    }

    aligned_buffer(const aligned_buffer &) = delete;
    aligned_buffer &operator=(const aligned_buffer &) = delete;

    T *data() { return data_; }
    const T *data() const { return data_; }
    std::size_t size() const { return size_; }
    T &operator[](std::size_t i) { return data_[i]; }
    const T &operator[](std::size_t i) const { return data_[i]; }

private:
    T *data_;
    std::size_t size_;
    std::size_t alignment_;
};

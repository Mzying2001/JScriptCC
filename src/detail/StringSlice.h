#pragma once
#include <cstddef>
#include <cstring>
#include <string>
#include <algorithm>

namespace jscriptcc {

/// Non-owning view into a UTF-8 string, analogous to std::string_view (C++17).
/// All positions are byte offsets. Caller must ensure the underlying string
/// outlives every StringSlice that references it.
class StringSlice {
public:
    StringSlice() : data_(nullptr), size_(0) {}
    StringSlice(const char* data, std::size_t size) : data_(data), size_(size) {}
    StringSlice(const char* begin, const char* end) : data_(begin), size_(static_cast<std::size_t>(end - begin)) {}
    StringSlice(const std::string& s) : data_(s.data()), size_(s.size()) {}

    const char* data() const { return data_; }
    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    const char* begin() const { return data_; }
    const char* end() const { return data_ + size_; }

    char operator[](std::size_t i) const { return data_[i]; }
    char front() const { return data_[0]; }
    char back() const { return data_[size_ - 1]; }

    StringSlice sub(std::size_t pos, std::size_t count = std::string::npos) const {
        if (pos >= size_) return StringSlice();
        std::size_t n = (count == std::string::npos || pos + count > size_) ? (size_ - pos) : count;
        return StringSlice(data_ + pos, n);
    }

    std::size_t find(char c, std::size_t pos = 0) const {
        for (std::size_t i = pos; i < size_; ++i) {
            if (data_[i] == c) return i;
        }
        return std::string::npos;
    }

    std::size_t find(const char* s, std::size_t pos = 0) const {
        std::size_t slen = std::strlen(s);
        if (slen == 0) return pos;
        if (pos + slen > size_) return std::string::npos;
        for (std::size_t i = pos; i <= size_ - slen; ++i) {
            if (std::memcmp(data_ + i, s, slen) == 0) return i;
        }
        return std::string::npos;
    }

    int compare(const StringSlice& other) const {
        std::size_t n = std::min(size_, other.size_);
        // memcmp with n==0 is a no-op, but passing nullptr is technically UB
        // per C11 §7.24.1 even when n is zero. Guard against it.
        int r = (n == 0) ? 0 : std::memcmp(data_, other.data_, n);
        if (r != 0) return r;
        if (size_ < other.size_) return -1;
        if (size_ > other.size_) return 1;
        return 0;
    }

    bool operator==(const StringSlice& other) const { return compare(other) == 0; }
    bool operator!=(const StringSlice& other) const { return compare(other) != 0; }
    bool operator<(const StringSlice& other) const { return compare(other) < 0; }
    bool operator==(const char* s) const { return compare(StringSlice(s)) == 0; }
    bool operator!=(const char* s) const { return compare(StringSlice(s)) != 0; }

    std::string toString() const { return std::string(data_, size_); }

private:
    const char* data_;
    std::size_t size_;
};

} // namespace jscriptcc

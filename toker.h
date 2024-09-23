#pragma once

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cassert>
#include <cctype>
#include <cstdint>
#include <string>

namespace hyperlink {

class MappedFileToker {
   public:
    explicit MappedFileToker(const std::string &filename) {
        _fd = open(filename.c_str(), O_RDONLY);
        assert(_fd != -1 && "open() failed");

        struct stat file_info {};
        const int ans = fstat(_fd, &file_info);
        assert(ans != -1 && "fstat() failed");

        _position = 0;
        _length = static_cast<std::size_t>(file_info.st_size);

        _contents = static_cast<char *>(
            mmap(nullptr, _length, PROT_READ, MAP_PRIVATE, _fd, 0));
        assert(_contents != MAP_FAILED && "mmap() failed");
    }

    ~MappedFileToker() {
        munmap(_contents, _length);
        close(_fd);
    }

    inline void SkipSpaces() {
        while (ValidPosition() && std::isspace(Current())) {
            Advance();
        }
    }

    inline void SkipLine() {
        while (ValidPosition() && Current() != '\n') {
            Advance();
        }
        if (ValidPosition()) {
            Advance();
        }
    }

    inline std::uint64_t ScanUInt() {
        std::uint64_t number = 0;
        while (ValidPosition() && std::isdigit(Current())) {
            const int digit = Current() - '0';
            number = number * 10 + digit;
            Advance();
        }

        SkipSpaces();
        return number;
    }

    inline void SkipUInt() {
        while (ValidPosition() && std::isdigit(Current())) {
            Advance();
        }
        SkipSpaces();
    }

    [[nodiscard]] inline bool ValidPosition() const {
        return _position < _length;
    }

    [[nodiscard]] inline char Current() const { return _contents[_position]; }

    inline void Advance() { ++_position; }

    [[nodiscard]] inline std::size_t Position() const { return _position; }

    [[nodiscard]] inline std::size_t Length() const { return _length; }

   private:
    int _fd = 0;
    std::size_t _position = 0;
    std::size_t _length = 0;
    char *_contents = nullptr;
};

}  // namespace hyperlink

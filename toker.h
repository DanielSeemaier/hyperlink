#pragma once

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cctype>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace hyperlink {

class MappedFileToker {
   public:
    explicit MappedFileToker(const std::string &filename) {
        _fd = open(filename.c_str(), O_RDONLY);
        if (_fd == -1) {
            throw std::runtime_error("open() failed");
        }

        struct stat file_info {};
        if (fstat(_fd, &file_info) == -1) {
            close(_fd);
            throw std::runtime_error("fstat() failed");
        }

        _position = 0;
        _length = static_cast<std::size_t>(file_info.st_size);

        _contents = static_cast<char *>(
            mmap(nullptr, _length, PROT_READ, MAP_PRIVATE, _fd, 0));
        if (_contents == MAP_FAILED) {
            close(_fd);
            throw std::runtime_error("mmap() failed");
        }
    }

    ~MappedFileToker() {
        munmap(_contents, _length);
        close(_fd);
    }

    inline void skip_spaces() {
        while (valid_position() && std::isspace(current())) {
            advance();
        }
    }

    inline void skip_line() {
        while (valid_position() && current() != '\n') {
            advance();
        }
        if (valid_position()) {
            advance();
        }
    }

    inline std::uint64_t scan_uint() {
        std::uint64_t number = 0;
        while (valid_position() && std::isdigit(current())) {
            const int digit = current() - '0';
            number = number * 10 + digit;
            advance();
        }

        skip_spaces();
        return number;
    }

    inline void skip_uint() {
        while (valid_position() && std::isdigit(current())) {
            advance();
        }
        skip_spaces();
    }

    [[nodiscard]] inline bool valid_position() const {
        return _position < _length;
    }

    [[nodiscard]] inline char current() const { return _contents[_position]; }

    inline void advance() { ++_position; }

    [[nodiscard]] inline std::size_t position() const { return _position; }

    [[nodiscard]] inline std::size_t length() const { return _length; }

   private:
    int _fd;
    std::size_t _position;
    std::size_t _length;
    char *_contents;
};

}  // namespace hyperlink

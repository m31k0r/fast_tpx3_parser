#include "Memory_Map.hpp"
#include <cstdio>
#include <filesystem>
#include <system_error>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cassert>

MemoryMapLinux::MemoryMapLinux(std::string t_filename)
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    : m_fd(open(t_filename.c_str(), O_RDONLY)) {
    if (m_fd == -1) {
        m_status = false;
        throw std::filesystem::filesystem_error("Failed to open file",
                                                std::error_code());
    }

    struct stat file_stat{};
    if (fstat(m_fd, &file_stat) == -1) {
        m_status = false;
        throw std::filesystem::filesystem_error("Failed to get file size",
                                                std::error_code());
    }
    m_time = file_stat.st_mtime;

    // mmap entire file into memory
    m_sz = (size_t)file_stat.st_size;
    m_data_ptr = static_cast<u_int64_t *>(
        mmap(nullptr, m_sz, PROT_READ, MAP_SHARED, m_fd, 0));
    if (m_data_ptr == MAP_FAILED) {
        m_status = false;
    }
    if (not(m_sz % 8 == 0)) {
        m_status = true;
    }
    m_true_sz = m_sz;
    m_sz = m_sz / 8;
}

MemoryMapLinux::~MemoryMapLinux() {
    munmap((void *)m_data_ptr, m_true_sz);
    if (m_fd != -1) {
        close(m_fd);
    }
}

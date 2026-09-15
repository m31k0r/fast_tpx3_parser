#pragma once
#include <sys/types.h>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <utility>

#ifndef _WIN32

class MemoryMapLinux {
    int m_fd;
    size_t m_sz;
    size_t m_true_sz;
    uint64_t* m_data_ptr;
    time_t m_time;
    bool m_status = true;

   public:
    bool get_status() const { return m_status; }
    explicit MemoryMapLinux(std::string t_filename);
    ~MemoryMapLinux();

    MemoryMapLinux(const MemoryMapLinux&) = delete;
    MemoryMapLinux& operator=(const MemoryMapLinux&) = delete;

    MemoryMapLinux(MemoryMapLinux&& t_lhs) noexcept
        : m_fd(std::exchange(t_lhs.m_fd, -1)),
          m_sz(std::exchange(t_lhs.m_sz, 0)),
          m_true_sz(std::exchange(t_lhs.m_true_sz, 0)),
          m_data_ptr(std::exchange(t_lhs.m_data_ptr, nullptr)),
          m_time(std::exchange(t_lhs.m_time, 0)),
          m_status(std::exchange(t_lhs.m_status, false)) {};
    MemoryMapLinux& operator=(MemoryMapLinux&& t_lhs) noexcept {
        using std::swap;
        swap(m_fd, t_lhs.m_fd);
        swap(m_sz, t_lhs.m_sz);
        swap(m_true_sz, t_lhs.m_true_sz);
        swap(m_data_ptr, t_lhs.m_data_ptr);
        swap(m_time, t_lhs.m_time);
        swap(m_status, t_lhs.m_status);
        return *this;
    };

    uint64_t* begin() { return m_data_ptr; };
    uint64_t* data() { return m_data_ptr; };
    uint64_t* end() { return std::next(m_data_ptr, static_cast<long>(m_sz)); }
};
using MemoryMap = MemoryMapLinux;
#else
#include <windows.h>
#include "TOOLS/logging.hpp"

class MemoryMap_Windows {
    HANDLE hMapFile;
    HANDLE hFile;
    size_t sz;
    size_t true_sz;
    uint64_t* data_ptr;
    time_t time;
    bool m_status;

   public:
    bool get_status() const { return m_status; }
    MemoryMap_Windows(std::string filename) : m_status(false) {
        hFile = CreateFile(filename.c_str(),
                           GENERIC_READ,           // dwDesiredAccess
                           0,                      // dwShareMode
                           NULL,                   // lpSecurityAttributes
                           OPEN_EXISTING,          // dwCreationDisposition
                           FILE_ATTRIBUTE_NORMAL,  // dwFlagsAndAttributes
                           0);
        if (hFile == INVALID_HANDLE_VALUE) {
            SPDLOG_ERROR("Create File failed with error {} {}", GetLastError(),
                         filename);
            return;
        }
        LARGE_INTEGER liFileSize;
        if (!GetFileSizeEx(hFile, &liFileSize)) {
            fprintf(stderr, "GetFileSize failed with error %d\n",
                    GetLastError());
            CloseHandle(hFile);
            return;
        }

        if (liFileSize.QuadPart == 0) {
            fprintf(stderr, "File is empty\n");
            CloseHandle(hFile);
            return;
        }
        true_sz = liFileSize.QuadPart;
        sz = true_sz / 8;

        hMapFile = CreateFileMapping(hFile,
                                     NULL,           // Mapping attributes
                                     PAGE_READONLY,  // Protection flags
                                     0,              // MaximumSizeHigh
                                     0,              // MaximumSizeLow
                                     NULL);          // Name

        if (hMapFile == NULL) {
            SPDLOG_ERROR("ERROR {} {}", GetLastError(), filename);
            printf(TEXT("Could not create file mapping object (%d).\n"),
                   GetLastError());
            CloseHandle(hFile);
            return;
        }
        LPVOID lpBasePtr = MapViewOfFile(hMapFile,
                                         FILE_MAP_READ,  // dwDesiredAccess
                                         0,              // dwFileOffsetHigh
                                         0,              // dwFileOffsetLow
                                         0);             // dwNumberOfBytesToMap
        if (lpBasePtr == NULL) {
            fprintf(stderr, "MapViewOfFile failed with error %d\n",
                    GetLastError());
            CloseHandle(hMapFile);
            CloseHandle(hFile);
            return;
        }
        data_ptr = static_cast<uint64_t*>(lpBasePtr);

        m_status = true;
    };
    ~MemoryMap_Windows() {
        if (m_status) {
            UnmapViewOfFile(data_ptr);
            CloseHandle(hMapFile);
            CloseHandle(hFile);
        }
    };
    MemoryMap_Windows(MemoryMap_Windows&&) = default;
    MemoryMap_Windows& operator=(MemoryMap_Windows&&) = default;
    MemoryMap_Windows(const MemoryMap_Windows&) = delete;
    MemoryMap_Windows& operator=(const MemoryMap_Windows&) = delete;
    MemoryMap_Windows(MemoryMap_Windows&) = delete;
    MemoryMap_Windows& operator=(MemoryMap_Windows&) = delete;

    uint64_t* begin() { return data_ptr; };
    uint64_t* data() { return data_ptr; };
    uint64_t* end() { return data_ptr + (sz); }
};

using MemoryMap = MemoryMap_Windows;
#endif

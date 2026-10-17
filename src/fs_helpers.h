#pragma once
// Small portable helpers for the two places that need a temporary file: the
// settings store (atomic replace) and the workflow layer (patch/editor content).
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace eg {
// Win32 forbids control characters in file names, so fixtures that exercise
// unusual names substitute a space there and keep the newline on POSIX.
#ifdef _WIN32
inline constexpr const char* unusual_name_separator = " ";
#else
inline constexpr const char* unusual_name_separator = "\n";
#endif

// A directory that is proven writable, for scratch files. The system temporary
// directory can be redirected, remapped by a sandbox, or locked down by policy,
// so every candidate is probed with a real file before it is used. An empty
// result means no candidate accepted a write.
inline std::filesystem::path scratch_directory() {
    static const std::filesystem::path chosen = [] {
        std::vector<std::filesystem::path> candidates;
        std::error_code temp_error;
        auto system_temp = std::filesystem::temp_directory_path(temp_error);
        if (!temp_error) candidates.push_back(system_temp);
#ifdef _WIN32
        wchar_t buffer[32768] = {};
        if (DWORD size = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, 32768))
            candidates.push_back(std::filesystem::path(std::wstring(buffer, size)) / "easy_git" / "tmp");
        if (DWORD size = GetModuleFileNameW(nullptr, buffer, 32768))
            candidates.push_back(std::filesystem::path(std::wstring(buffer, size)).parent_path());
#else
        if (const char* home = std::getenv("HOME"))
            candidates.push_back(std::filesystem::path(home) / ".cache" / "easy_git");
#endif
        std::error_code cwd_error;
        auto working = std::filesystem::current_path(cwd_error);
        if (!cwd_error) candidates.push_back(working);
        for (const auto& candidate : candidates) {
            if (candidate.empty()) continue;
            std::error_code create_error;
            std::filesystem::create_directories(candidate, create_error);
            auto probe = candidate / ("easy-git-write-probe-" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
            std::ofstream test(probe, std::ios::binary | std::ios::trunc);
            if (!test) continue;
            test.put('\n');
            test.close();
            std::error_code remove_error;
            std::filesystem::remove(probe, remove_error);
            if (test.good() || std::filesystem::exists(candidate)) return candidate;
        }
        return std::filesystem::path{};
    }();
    return chosen;
}

// Write `text` to a newly named file inside `directory` and return its path.
inline std::string write_temporary_file(const std::filesystem::path& directory, const std::string& prefix,
                                        const std::string& text) {
    static std::atomic<unsigned long long> counter{0};
    for (int attempt = 0; attempt < 64; ++attempt) {
        auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        auto path = directory /
            (prefix + std::to_string(stamp) + "-" + std::to_string(counter.fetch_add(1)));
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("Cannot create temporary file.");
        out.write(text.data(), std::streamsize(text.size()));
        out.flush();
        if (!out) {
            out.close();
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
            continue;
        }
        return path.string();
    }
    throw std::runtime_error("Cannot write a temporary file in " + directory.string() + ".");
}

// Replace `target` with `source`. Windows refuses an in-place overwrite, so the
// rename needs MOVEFILE_REPLACE_EXISTING there.
inline bool replace_file(const std::filesystem::path& source, const std::filesystem::path& target) {
#ifdef _WIN32
    auto from = source.wstring();
    auto to = target.wstring();
    return MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
#else
    return std::rename(source.string().c_str(), target.string().c_str()) == 0;
#endif
}

// Git marks its object files read-only on Windows, which blocks removal until
// the attribute is cleared; tests use this for their fixtures.
inline void remove_tree(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
    if (!ec) return;
    for (auto it = std::filesystem::recursive_directory_iterator(
             path, std::filesystem::directory_options::skip_permission_denied, ec);
         !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
        std::error_code ignored;
        std::filesystem::permissions(*it, std::filesystem::perms::owner_write,
                                     std::filesystem::perm_options::add, ignored);
    }
    std::filesystem::remove_all(path, ec);
}

// Create a fresh directory for a test fixture; the portable stand-in for mkdtemp.
inline std::filesystem::path make_test_directory(const std::string& prefix) {
    static std::atomic<unsigned long long> counter{0};
    auto base = scratch_directory();
    if (base.empty()) throw std::runtime_error("Cannot create a temporary directory: no writable location.");
    for (int attempt = 0; attempt < 64; ++attempt) {
        auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        auto path = base / (prefix + std::to_string(stamp) + "-" + std::to_string(counter.fetch_add(1)));
        std::error_code ec;
        if (std::filesystem::create_directory(path, ec)) return path;
        if (ec) throw std::runtime_error("Cannot create a temporary directory: " + ec.message());
    }
    throw std::runtime_error("Cannot create a temporary directory.");
}
}

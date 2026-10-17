// Windows process backend: equivalent of the POSIX runner in git.cpp.
// Child output goes to unlinked temporary files so a chatty Git command cannot
// block on a full pipe, and a job object guarantees the whole child tree dies
// when the operation is cancelled or times out.
#ifdef _WIN32
#include "git.h"
#include "fs_helpers.h"
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace eg {
namespace {
constexpr size_t output_limit = 8 * 1024 * 1024;

std::wstring widen(const std::string& text) {
    if (text.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), int(text.size()), nullptr, 0);
    if (size <= 0) throw std::runtime_error("Invalid UTF-8 in process argument");
    std::wstring out(size_t(size), L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), int(text.size()), out.data(), size))
        throw std::runtime_error("Invalid UTF-8 in process argument");
    return out;
}

std::string describe(DWORD error) {
    wchar_t* buffer = nullptr;
    auto size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                                   FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error, 0,
                               reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
    std::string text;
    if (size && buffer) {
        int need = WideCharToMultiByte(CP_UTF8, 0, buffer, int(size), nullptr, 0, nullptr, nullptr);
        text.resize(size_t(std::max(need, 0)));
        if (need > 0) WideCharToMultiByte(CP_UTF8, 0, buffer, int(size), text.data(), need, nullptr, nullptr);
        while (!text.empty() && (text.back() == '\r' || text.back() == '\n' || text.back() == ' ')) text.pop_back();
    }
    if (buffer) LocalFree(buffer);
    return text.empty() ? "Windows error " + std::to_string(error) : text;
}

// Quote a single argument exactly as CommandLineToArgvW and the VC++ runtime
// split the command line back into argv.
void append_quoted(std::wstring& line, const std::wstring& word) {
    if (!word.empty() && word.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        line += word;
        return;
    }
    line.push_back(L'"');
    for (auto it = word.begin();; ++it) {
        size_t backslashes = 0;
        while (it != word.end() && *it == L'\\') { ++it; ++backslashes; }
        if (it == word.end()) {
            line.append(backslashes * 2, L'\\');
            break;
        }
        if (*it == L'"') {
            line.append(backslashes * 2 + 1, L'\\');
            line.push_back(L'"');
        } else {
            line.append(backslashes, L'\\');
            line.push_back(*it);
        }
    }
    line.push_back(L'"');
}

std::wstring command_line(const std::vector<std::string>& words) {
    std::wstring line;
    for (size_t i = 0; i < words.size(); ++i) {
        if (i) line.push_back(L' ');
        append_quoted(line, widen(words[i]));
    }
    return line;
}

std::wstring environment_block(char* const* environment) {
    std::vector<std::string> entries;
    if (environment) {
        for (char* const* item = environment; *item; ++item) entries.emplace_back(*item);
    } else {
        for (wchar_t* item = GetEnvironmentStringsW(); *item; item += wcslen(item) + 1) {
            int need = WideCharToMultiByte(CP_UTF8, 0, item, int(wcslen(item)), nullptr, 0, nullptr, nullptr);
            std::string entry(size_t(std::max(need, 0)), '\0');
            if (need > 0) WideCharToMultiByte(CP_UTF8, 0, item, int(wcslen(item)), entry.data(), need, nullptr, nullptr);
            entries.push_back(std::move(entry));
        }
        FreeEnvironmentStringsW(GetEnvironmentStringsW());
    }
    std::wstring block;
    for (const auto& entry : entries) {
        if (entry.empty()) continue;
        block += widen(entry);
        block.push_back(L'\0');
    }
    block.push_back(L'\0');
    return block;
}

HANDLE open_capture_file() {
    // The system temporary directory can be redirected or locked down; the shared
    // helper probes candidates so a sandboxed or policy-restricted launch still
    // has somewhere to capture the child's output.
    auto scratch = eg::scratch_directory();
    if (scratch.empty()) throw std::runtime_error("Cannot create a temporary file: no writable directory was found");
    auto wide = scratch.wstring();
    if (wide.size() + 1 > MAX_PATH)
        throw std::runtime_error("Cannot create a temporary file: the scratch path is too long");
    wchar_t name[MAX_PATH + 1] = {};
    if (!GetTempFileNameW(wide.c_str(), L"egt", 0, name))
        throw std::runtime_error("Cannot create a temporary file in " + scratch.string() + ": " + describe(GetLastError()));
    SECURITY_ATTRIBUTES inherit{};
    inherit.nLength = sizeof(inherit);
    inherit.bInheritHandle = TRUE;
    HANDLE file = CreateFileW(name, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              &inherit, OPEN_EXISTING,
                              FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        DeleteFileW(name);
        throw std::runtime_error("Cannot open the temporary output file");
    }
    return file;
}

std::string read_capture(HANDLE file, bool& truncated) {
    std::string out;
    LARGE_INTEGER start{};
    if (!SetFilePointerEx(file, start, nullptr, FILE_BEGIN)) return out;
    char buffer[8192];
    DWORD got = 0;
    while (ReadFile(file, buffer, sizeof(buffer), &got, nullptr) && got) {
        if (out.size() + got > output_limit) {
            out.append(buffer, output_limit - out.size());
            truncated = true;
            break;
        }
        out.append(buffer, got);
    }
    return out;
}
}  // namespace

Result run_process(std::vector<std::string> words, const std::shared_ptr<std::atomic_bool>& cancel,
                   int timeout_seconds, char* const* environment) {
    if (words.empty() || words.front().empty()) throw std::runtime_error("Missing executable");
    for (const auto& word : words)
        if (word.find('\0') != std::string::npos) throw std::runtime_error("NUL in process argument");

    HANDLE out_file = open_capture_file();
    HANDLE err_file = open_capture_file();
    SECURITY_ATTRIBUTES inherit{};
    inherit.nLength = sizeof(inherit);
    inherit.bInheritHandle = TRUE;
    HANDLE null_input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit,
                                    OPEN_EXISTING, 0, nullptr);
    auto line = command_line(words);
    auto block = environment_block(environment);

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = null_input == INVALID_HANDLE_VALUE ? nullptr : null_input;
    startup.hStdOutput = out_file;
    startup.hStdError = err_file;
    PROCESS_INFORMATION info{};
    std::wstring buffer = line;  // CreateProcessW may modify the command line in place.
    DWORD creation = CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT | CREATE_SUSPENDED;
    bool started = CreateProcessW(nullptr, buffer.data(), nullptr, nullptr, TRUE, creation, block.data(), nullptr,
                                  &startup, &info) != 0;
    DWORD error = started ? 0 : GetLastError();
    if (null_input != INVALID_HANDLE_VALUE) CloseHandle(null_input);
    if (!started) {
        CloseHandle(out_file);
        CloseHandle(err_file);
        throw std::runtime_error("Cannot start " + words.front() + ": " + describe(error));
    }
    // Suspended start lets the job object own the child from its very first instruction.
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
        AssignProcessToJobObject(job, info.hProcess);
    }
    ResumeThread(info.hThread);
    CloseHandle(info.hThread);

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_seconds);
    bool stopped = false, too_large = false;
    while (true) {
        if (WaitForSingleObject(info.hProcess, 10) == WAIT_OBJECT_0) break;
        LARGE_INTEGER out_size{}, err_size{};
        GetFileSizeEx(out_file, &out_size);
        GetFileSizeEx(err_file, &err_size);
        too_large = out_size.QuadPart > LONGLONG(output_limit) || err_size.QuadPart > LONGLONG(output_limit);
        if (too_large || (cancel && *cancel) || (timeout_seconds > 0 && std::chrono::steady_clock::now() > deadline)) {
            if (job) TerminateJobObject(job, 1);
            TerminateProcess(info.hProcess, 1);
            WaitForSingleObject(info.hProcess, 5000);
            stopped = true;
            break;
        }
    }
    WaitForSingleObject(info.hProcess, 5000);
    DWORD code = 0;
    GetExitCodeProcess(info.hProcess, &code);
    CloseHandle(info.hProcess);
    if (job) CloseHandle(job);

    Result result;
    result.code = int(code);
    result.out = read_capture(out_file, result.truncated);
    result.error = read_capture(err_file, result.truncated);
    CloseHandle(out_file);
    CloseHandle(err_file);
    if (stopped) {
        result.code = 124;
        result.error += too_large ? "\nProcess output exceeds 8 MiB. Narrow the query." :
            "\nProcess stopped (cancelled or timed out). Refresh to inspect repository state.";
    }
    return result;
}
}  // namespace eg
#endif

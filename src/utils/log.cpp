#include "utils/log.hpp"

#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>

namespace fs = std::filesystem;

LogLevel parse_log_level(const std::string& s) {
    if (s == "debug") return LogLevel::Debug;
    if (s == "info")  return LogLevel::Info;
    if (s == "warn")  return LogLevel::Warn;
    if (s == "error") return LogLevel::Error;
    throw std::runtime_error("log_level harus debug, info, warn, atau error");
}

namespace {

struct State {
    std::mutex mu;
    std::ofstream file;
    std::string path;
    LogLevel min_level = LogLevel::Info;
    std::size_t max_bytes = 1024 * 1024;
    std::size_t written = 0;
    bool enabled = false;
};

State& state() {
    static State s;
    return s;
}

const char* label(LogLevel l) {
    switch (l) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
    }
    return "?????";
}

std::string timestamp() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

// Satu baris saja, tanpa karakter kontrol, dipotong tanpa merusak UTF-8.
std::string sanitize(const std::string& in, std::size_t max) {
    std::string s = in;
    bool cut = false;
    if (s.size() > max) {
        std::size_t n = max;
        while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80) --n;
        s.resize(n);
        cut = true;
    }
    std::string out;
    out.reserve(s.size() + 3);
    for (unsigned char c : s) {
        if (c == '\n' || c == '\r' || c == '\t') out += ' ';
        else if (c < 0x20 || c == 0x7f) out += '?';
        else out += static_cast<char>(c);
    }
    if (cut) out += "...";
    return out;
}

void open_file(State& st) {
    st.file.close();
    st.file.clear();
    st.file.open(st.path, std::ios::app);
    st.written = 0;
    if (!st.file) return;
    std::error_code ec;
    fs::permissions(st.path, fs::perms::owner_read | fs::perms::owner_write,
                    fs::perm_options::replace, ec);
    const auto sz = fs::file_size(st.path, ec);
    if (!ec) st.written = static_cast<std::size_t>(sz);
}

void rotate(State& st) {
    st.file.close();
    std::error_code ec;
    fs::rename(st.path, st.path + ".1", ec);   // menimpa cadangan lama
    if (ec) fs::remove(st.path, ec);           // rename gagal: buang saja, jangan tumbuh tanpa batas
    open_file(st);
}

}  // namespace

namespace logger {

void init(const std::string& path, LogLevel min_level, std::size_t max_bytes) {
    State& st = state();
    std::lock_guard<std::mutex> lock(st.mu);
    st.min_level = min_level;
    st.max_bytes = max_bytes;
    st.enabled = false;
    if (path.empty()) return;

    st.path = path;
    open_file(st);
    if (!st.file) {
        std::cerr << "Peringatan: tidak bisa membuka file log (" << path
                  << "), log ke file dimatikan\n";
        return;
    }
    st.enabled = true;
}

void write(LogLevel level, const std::string& component, const std::string& message) {
    State& st = state();
    std::lock_guard<std::mutex> lock(st.mu);

    const std::string line = timestamp() + " " + label(level) + " [" +
                             sanitize(component, 20) + "] " + sanitize(message, 500);

    if (level >= LogLevel::Warn) std::cerr << line << '\n';
    if (!st.enabled || level < st.min_level) return;

    if (st.written > 0 && st.written + line.size() + 1 > st.max_bytes) rotate(st);
    if (!st.file) return;

    st.file << line << '\n';
    st.file.flush();
    st.written += line.size() + 1;
}

}  // namespace logger
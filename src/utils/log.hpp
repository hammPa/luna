#pragma once
#include <cstddef>
#include <string>

enum class LogLevel { Debug = 0, Info, Warn, Error };

// Melempar std::runtime_error jika nama level tidak dikenal.
LogLevel parse_log_level(const std::string& s);

namespace logger {

// path kosong = tanpa file (WARN dan ERROR tetap ke stderr).
void init(const std::string& path, LogLevel min_level, std::size_t max_bytes);
void write(LogLevel level, const std::string& component, const std::string& message);

inline void debug(const std::string& c, const std::string& m) { write(LogLevel::Debug, c, m); }
inline void info(const std::string& c, const std::string& m)  { write(LogLevel::Info, c, m); }
inline void warn(const std::string& c, const std::string& m)  { write(LogLevel::Warn, c, m); }
inline void error(const std::string& c, const std::string& m) { write(LogLevel::Error, c, m); }

}  // namespace logger
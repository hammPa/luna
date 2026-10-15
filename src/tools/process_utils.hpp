#pragma once

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

// Jalankan program TANPA shell dan tanpa menunggu.
// argv[0] = nama program (dicari di PATH oleh execvp).
// Double-fork: anak langsung segera keluar, cucu diwariskan ke init,
// jadi tidak ada proses zombie.
inline void spawn_detached(const std::vector<std::string>& argv) {
    if (argv.empty() || argv[0].empty()) {
        throw std::runtime_error("Perintah kosong.");
    }

    // Siapkan array char* SEBELUM fork (alokasi memori tidak aman setelah fork).
    std::vector<char*> args;
    args.reserve(argv.size() + 1);
    for (const auto& a : argv) args.push_back(const_cast<char*>(a.c_str()));
    args.push_back(nullptr);

    pid_t pid = fork();
    if (pid < 0) throw std::runtime_error("fork gagal.");

    if (pid == 0) {
        // Anak: buat sesi baru, fork lagi, lalu keluar.
        setsid();
        pid_t pid2 = fork();
        if (pid2 != 0) _exit(pid2 < 0 ? 1 : 0);

        // Cucu: arahkan stdio ke /dev/null lalu exec.
        int fd = open("/dev/null", O_RDWR);
        if (fd >= 0) {
            dup2(fd, STDIN_FILENO);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            if (fd > STDERR_FILENO) close(fd);
        }
        execvp(args[0], args.data());
        _exit(127);  // exec gagal (program tidak ada)
    }

    // Induk: tunggu anak pertama (cepat selesai), bersihkan.
    int status = 0;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        throw std::runtime_error("Gagal memulai proses.");
    }
}

// Cari program di PATH (tanpa shell). Mengembalikan true kalau bisa dieksekusi.
inline bool program_exists(const std::string& name) {
    if (name.find('/') != std::string::npos) return access(name.c_str(), X_OK) == 0;
    const char* path = std::getenv("PATH");
    if (!path) return false;
    std::string p = path;
    std::size_t start = 0;
    while (start <= p.size()) {
        std::size_t end = p.find(':', start);
        if (end == std::string::npos) end = p.size();
        std::string dir = p.substr(start, end - start);
        if (!dir.empty() && access((dir + "/" + name).c_str(), X_OK) == 0) return true;
        start = end + 1;
    }
    return false;
}

// Jalankan tanpa shell, tunggu selesai, kembalikan exit code.
// stdout/stderr dibuang. Lempar exception kalau fork/exec gagal.
inline int run_and_wait(const std::vector<std::string>& argv) {
    if (argv.empty() || argv[0].empty()) throw std::runtime_error("Perintah kosong.");

    std::vector<char*> args;
    for (const auto& a : argv) args.push_back(const_cast<char*>(a.c_str()));
    args.push_back(nullptr);

    pid_t pid = fork();
    if (pid < 0) throw std::runtime_error("fork gagal.");
    if (pid == 0) {
        int fd = open("/dev/null", O_RDWR);
        if (fd >= 0) {
            dup2(fd, STDIN_FILENO);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            if (fd > STDERR_FILENO) close(fd);
        }
        execvp(args[0], args.data());
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) throw std::runtime_error("waitpid gagal.");
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}
#pragma once

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>
#include <csignal>

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


struct CaptureResult {
    int exit_code = -1;
    std::string out;
};

// Jalankan tanpa shell. stdin_data ditulis ke stdin program, stdout ditangkap.
// Hanya untuk data kecil (stdin_data dan output dibatasi).
inline CaptureResult run_capture(const std::vector<std::string>& argv,
                                 const std::string& stdin_data = "",
                                 std::size_t max_out = 64 * 1024) {
    if (argv.empty() || argv[0].empty()) throw std::runtime_error("Perintah kosong.");
    if (stdin_data.size() > 64 * 1024) throw std::runtime_error("Input terlalu besar.");

    std::vector<char*> args;
    for (const auto& a : argv) args.push_back(const_cast<char*>(a.c_str()));
    args.push_back(nullptr);

    int in_pipe[2];   // Luna menulis [1], anak membaca [0]
    int out_pipe[2];  // anak menulis [1], Luna membaca [0]
    if (pipe(in_pipe) != 0) throw std::runtime_error("pipe gagal.");
    if (pipe(out_pipe) != 0) {
        close(in_pipe[0]); close(in_pipe[1]);
        throw std::runtime_error("pipe gagal.");
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(in_pipe[0]); close(in_pipe[1]);
        close(out_pipe[0]); close(out_pipe[1]);
        throw std::runtime_error("fork gagal.");
    }

    if (pid == 0) {
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        int fd = open("/dev/null", O_WRONLY);
        if (fd >= 0) { dup2(fd, STDERR_FILENO); close(fd); }
        close(in_pipe[0]); close(in_pipe[1]);
        close(out_pipe[0]); close(out_pipe[1]);
        execvp(args[0], args.data());
        _exit(127);
    }

    // Induk: tutup ujung yang bukan miliknya.
    close(in_pipe[0]);
    close(out_pipe[1]);

    // Cegah SIGPIPE membunuh Luna kalau anak keluar lebih dulu.
    auto old_handler = std::signal(SIGPIPE, SIG_IGN);

    std::size_t written = 0;
    while (written < stdin_data.size()) {
        ssize_t n = write(in_pipe[1], stdin_data.data() + written, stdin_data.size() - written);
        if (n < 0) break;  // anak sudah menutup stdin; lanjut baca outputnya
        written += static_cast<std::size_t>(n);
    }
    close(in_pipe[1]);  // EOF untuk anak

    CaptureResult res;
    char buf[1024];
    while (true) {
        ssize_t n = read(out_pipe[0], buf, sizeof(buf));
        if (n <= 0) break;
        if (res.out.size() + static_cast<std::size_t>(n) > max_out) break;
        res.out.append(buf, static_cast<std::size_t>(n));
    }
    close(out_pipe[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    std::signal(SIGPIPE, old_handler);

    res.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return res;
}
#pragma once

#include "utils/format_utils.hpp"

#include <filesystem>
#include <string>
#include <system_error>

// Tujuan pengiriman file. Tool send_file tidak tahu sedang di terminal atau Telegram,
// sama seperti Prompter: tiap kanal punya implementasinya sendiri.
class FileSink {
public:
    virtual ~FileSink() = default;

    // true jika file benar-benar meninggalkan komputer ini (diunggah ke pihak ketiga).
    virtual bool uploads() const = 0;

    // Kirim satu file. Lempar std::runtime_error jika gagal.
    // Mengembalikan deskripsi singkat untuk dilaporkan ke model.
    virtual std::string deliver(const std::filesystem::path& file, const std::string& caption) = 0;
};

// Di terminal: tidak mengunggah apa pun, hanya menunjukkan lokasi file.
class TerminalFileSink : public FileSink {
public:
    bool uploads() const override { return false; }

    std::string deliver(const std::filesystem::path& file, const std::string& /*caption*/) override {
        std::error_code ec;
        const auto size = std::filesystem::file_size(file, ec);
        return file.string() + (ec ? std::string() : " (" + human_size(size) + ")");
    }
};

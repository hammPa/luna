#include "telegram/telegram_loop.hpp"

#include "app/commands.hpp"
#include "telegram/telegram_client.hpp"

#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "auth/totp.hpp"
#include <memory>

namespace {

constexpr std::size_t kMaxInputChars = 2000;

bool is_allowed(const Config& cfg, long long user_id) {
    return std::find(cfg.telegram_allowed_users.begin(),
                     cfg.telegram_allowed_users.end(),
                     user_id) != cfg.telegram_allowed_users.end();
}

}  // namespace

void run_telegram(const Config& cfg, ChatSession& session, Memory& memory,
                  TelegramPrompter& prompter) {
    // Fail closed: tanpa allowlist, mode ini tidak boleh jalan.
    if (cfg.telegram_allowed_users.empty()) {
        throw std::runtime_error(
            "telegram_allowed_users kosong. Isi dengan user ID-mu "
            "(mode Telegram tidak mau jalan tanpa allowlist).");
    }
    const char* token = std::getenv(cfg.telegram_token_env.c_str());
    if (token == nullptr || *token == '\0') {
        throw std::runtime_error("Environment variable " + cfg.telegram_token_env +
                                 " belum di-set");
    }

    TelegramClient tg(token);
    long long offset = 0;

    // Prompter memakai client dan offset yang sama dengan loop ini.
    prompter.bind(&tg, &offset);

    std::unique_ptr<TotpVerifier> verifier;
    if (const char* secret = std::getenv("LUNA_TOTP_SECRET"); secret != nullptr && *secret != '\0') {
        verifier = std::make_unique<TotpVerifier>(make_oathtool_generator(secret));
        prompter.set_verifier(verifier.get());
        std::cout << "TOTP aktif: aksi berbahaya bisa disetujui dengan kode authenticator.\n";
    } else {
        std::cout << "TOTP tidak aktif: aksi Destructive/Privileged ditolak dari Telegram.\n";
    }

    // Buang pesan yang menumpuk saat Luna mati, supaya tidak dieksekusi mendadak.
    try {
        tg.skip_backlog(offset);
    } catch (const std::exception& e) {
        std::cerr << "Peringatan: gagal membuang pesan lama: " << e.what() << '\n';
    }

    std::cout << "Mode Telegram aktif (" << cfg.telegram_allowed_users.size()
              << " user diizinkan). Ctrl+C untuk berhenti.\n";

    while (true) {
        std::vector<TelegramUpdate> updates;
        try {
            updates = tg.get_updates(offset);
        } catch (const std::exception& e) {
            std::cerr << "Polling gagal: " << e.what() << " (coba lagi 5 detik)\n";
            sleep(5);
            continue;
        }

        for (const auto& u : updates) {
            // Orang asing diabaikan diam-diam (tidak dibalas).
            if (!is_allowed(cfg, u.user_id)) {
                std::cerr << "[telegram] diabaikan: user " << u.user_id << '\n';
                continue;
            }

            if (u.text.size() > kMaxInputChars) {
                try {
                    tg.send_message(u.chat_id,
                                    "Pesan terlalu panjang (maks " +
                                        std::to_string(kMaxInputChars) + " karakter).");
                } catch (const std::exception&) {
                }
                continue;
            }

            // Log hanya panjang, bukan isi, supaya terminal tidak menyimpan chat pribadi.
            std::cerr << "[telegram] pesan dari " << u.user_id << " ("
                      << u.text.size() << " karakter)\n";

            // Indikator mengetik; gagal di sini tidak penting.
            try {
                tg.send_chat_action(u.chat_id);
            } catch (const std::exception&) {
            }

            std::string reply;
            try {
                bool handled = false;
                if (!u.text.empty() && u.text[0] == '/') {
                    std::ostringstream out;
                    handled = handle_command(u.text, memory, out);
                    if (handled) reply = out.str();
                }
                if (!handled) {
                    // Izinkan prompter tahu siapa yang sedang dilayani.
                    prompter.set_context(u.chat_id, u.user_id);
                    try {
                        reply = session.send(u.text);
                    } catch (...) {
                        prompter.clear_context();
                        throw;  // ditangkap catch di bawah, jadi "Error: ..."
                    }
                    prompter.clear_context();
                }
            } catch (const std::exception& e) {
                reply = std::string("Error: ") + e.what();
            }

            try {
                tg.send_message(u.chat_id, reply);
            } catch (const std::exception& e) {
                std::cerr << "Gagal kirim balasan: " << e.what() << '\n';
            }
        }
    }
}
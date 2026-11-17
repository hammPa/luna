#pragma once

#include "tools/prompter.hpp"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>

// Prompter untuk TUI. ask() dipanggil dari thread kerja dan menunggu;
// UI menampilkan pertanyaan lalu memanggil answer().
class TuiPrompter : public Prompter {
public:
    struct Request {
        std::string question;
        bool can_always = false;
    };

    Decision ask(const std::string& question, bool can_always, Risk risk) override;

    // Sisi UI.
    bool pending(Request* out = nullptr) const;
    void answer(Decision d);
    void close();                                   // semua yang menunggu ditolak
    void set_notifier(std::function<void()> fn);    // dipanggil saat ada pertanyaan baru

private:
    mutable std::mutex mu_;
    std::condition_variable cv_;
    std::function<void()> notify_;
    Request req_;
    bool pending_ = false;
    bool answered_ = false;
    bool closed_ = false;
    Decision decision_ = Decision::Deny;
};
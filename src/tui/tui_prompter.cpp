#include "tui/tui_prompter.hpp"

#include <utility>

Decision TuiPrompter::ask(const std::string& question, bool can_always, Risk /*risk*/) {
    std::function<void()> notify;
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (closed_) return Decision::Deny;
        req_.question = question;
        req_.can_always = can_always;
        pending_ = true;
        answered_ = false;
        decision_ = Decision::Deny;
        notify = notify_;
    }
    if (notify) notify();

    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk, [&] { return answered_ || closed_; });
    pending_ = false;
    // Ditutup tanpa jawaban = tolak. Jangan pernah default ke izin.
    const Decision d = (answered_ && !closed_) ? decision_ : Decision::Deny;
    answered_ = false;
    return d;
}

bool TuiPrompter::pending(Request* out) const {
    std::lock_guard<std::mutex> lk(mu_);
    if (!pending_ || answered_) return false;
    if (out) *out = req_;
    return true;
}

void TuiPrompter::answer(Decision d) {
    std::lock_guard<std::mutex> lk(mu_);
    if (!pending_ || answered_) return;
    if (d == Decision::AllowAlways && !req_.can_always) return;
    decision_ = d;
    answered_ = true;
    cv_.notify_all();
}

void TuiPrompter::close() {
    std::lock_guard<std::mutex> lk(mu_);
    closed_ = true;
    cv_.notify_all();
}

void TuiPrompter::set_notifier(std::function<void()> fn) {
    std::lock_guard<std::mutex> lk(mu_);
    notify_ = std::move(fn);
}
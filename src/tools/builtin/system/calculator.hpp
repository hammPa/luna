#pragma once

#include "tools/tool.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

class CalculatorTool : public Tool {
public:
    std::string name() const override { return "calculator"; }

    std::string description() const override {
        return "Hitung ekspresi matematika: + - * / % ^, kurung, pi, e, sqrt, abs, "
               "sin, cos, tan (radian), log (basis 10), ln, floor, ceil, round, min, max.";
    }

    nlohmann::json parameters() const override {
        return {
            {"type", "object"},
            {"properties", {
                {"expression", {{"type", "string"},
                                {"description", "Ekspresi, mis. (12.5 + 3) * 2^3"}}}
            }},
            {"required", nlohmann::json::array({"expression"})}
        };
    }

    Risk risk(const nlohmann::json&) const override { return Risk::Safe; }

    std::string describe(const nlohmann::json& args) const override {
        return "Hitung: " + expr_from(args).substr(0, 200);
    }

    std::string execute(const nlohmann::json& args) override {
        const std::string expr = expr_from(args);
        if (expr.empty()) return "Error: 'expression' wajib berupa teks tidak kosong.";
        if (expr.size() > 200) return "Error: ekspresi terlalu panjang (maks 200 karakter).";
        try {
            Parser p(expr);
            const double v = p.parse();
            char buf[64];
            std::snprintf(buf, sizeof buf, "%.12g", v);
            return buf;
        } catch (const std::exception& e) {
            return std::string("Error: ") + e.what();
        }
    }

private:
    static std::string expr_from(const nlohmann::json& args) {
        if (args.is_object()) {
            auto it = args.find("expression");
            if (it != args.end() && it->is_string()) return it->get<std::string>();
        }
        return "";
    }

    // Parser rekursif: tanpa eval dan tanpa shell.
    // expr = term (('+'|'-') term)*   term = unary (('*'|'/'|'%') unary)*
    // unary = ('-'|'+') unary | power  power = primary ['^' unary]
    class Parser {
    public:
        explicit Parser(const std::string& s) : s_(s) {}

        double parse() {
            const double v = expr();
            skip();
            if (i_ < s_.size()) fail("karakter tidak terduga");
            return check(v);
        }

    private:
        static constexpr int kMaxDepth = 50;

        const std::string& s_;
        std::size_t i_ = 0;
        int depth_ = 0;

        struct Depth {
            int& d;
            explicit Depth(int& x) : d(x) {
                if (++d > kMaxDepth) throw std::runtime_error("ekspresi terlalu bersarang");
            }
            ~Depth() { --d; }
        };

        [[noreturn]] void fail(const std::string& m) const {
            throw std::runtime_error(m + " (posisi " + std::to_string(i_ + 1) + ")");
        }

        static double check(double v) {
            if (!std::isfinite(v)) throw std::runtime_error("hasil tidak terhingga atau bukan angka");
            return v;
        }

        void skip() {
            while (i_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[i_]))) ++i_;
        }

        bool eat(char c) {
            skip();
            if (i_ < s_.size() && s_[i_] == c) { ++i_; return true; }
            return false;
        }

        double expr() {
            Depth g(depth_);
            double v = term();
            while (true) {
                if (eat('+')) v += term();
                else if (eat('-')) v -= term();
                else return v;
            }
        }

        double term() {
            double v = unary();
            while (true) {
                if (eat('*')) {
                    v *= unary();
                } else if (eat('/')) {
                    const double d = unary();
                    if (d == 0) fail("pembagian dengan nol");
                    v /= d;
                } else if (eat('%')) {
                    const double d = unary();
                    if (d == 0) fail("modulo dengan nol");
                    v = std::fmod(v, d);
                } else {
                    return v;
                }
            }
        }

        double unary() {
            Depth g(depth_);
            if (eat('-')) return -unary();
            if (eat('+')) return unary();
            return power();
        }

        double power() {
            const double base = primary();
            if (eat('^')) return check(std::pow(base, unary()));
            return base;
        }

        double primary() {
            skip();
            if (i_ >= s_.size()) fail("ekspresi berakhir terlalu dini");
            const char c = s_[i_];
            if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') return number();
            if (std::isalpha(static_cast<unsigned char>(c))) return word();
            if (c == '(') {
                ++i_;
                const double v = expr();
                if (!eat(')')) fail("kurung tutup hilang");
                return v;
            }
            fail(std::string("karakter tidak dikenal '") + c + "'");
        }

        double number() {
            std::size_t j = i_;
            bool dot = false;
            while (j < s_.size() &&
                   (std::isdigit(static_cast<unsigned char>(s_[j])) || (s_[j] == '.' && !dot))) {
                if (s_[j] == '.') dot = true;
                ++j;
            }
            const std::string t = s_.substr(i_, j - i_);
            if (t == ".") fail("angka tidak valid");
            i_ = j;
            return std::strtod(t.c_str(), nullptr);
        }

        double word() {
            std::size_t j = i_;
            while (j < s_.size() && std::isalpha(static_cast<unsigned char>(s_[j]))) ++j;
            std::string name = s_.substr(i_, j - i_);
            for (auto& ch : name) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            i_ = j;

            skip();
            const bool has_call = (i_ < s_.size() && s_[i_] == '(');
            if (!has_call) {
                if (name == "pi") return 3.14159265358979323846;
                if (name == "e") return 2.71828182845904523536;
                fail("konstanta atau fungsi tidak dikenal '" + name + "'");
            }
            ++i_;   // '('
            std::vector<double> a;
            if (!eat(')')) {
                a.push_back(expr());
                while (eat(',')) a.push_back(expr());
                if (!eat(')')) fail("kurung tutup hilang");
            }
            return call(name, a);
        }

        double call(const std::string& n, const std::vector<double>& a) {
            auto one = [&]() -> double {
                if (a.size() != 1) fail(n + " butuh tepat 1 argumen");
                return a[0];
            };
            if (n == "sqrt") {
                const double x = one();
                if (x < 0) fail("sqrt dari angka negatif");
                return std::sqrt(x);
            }
            if (n == "abs")   return std::fabs(one());
            if (n == "sin")   return std::sin(one());
            if (n == "cos")   return std::cos(one());
            if (n == "tan")   return std::tan(one());
            if (n == "floor") return std::floor(one());
            if (n == "ceil")  return std::ceil(one());
            if (n == "round") return std::round(one());
            if (n == "log" || n == "ln") {
                const double x = one();
                if (x <= 0) fail(n + " butuh angka positif");
                return n == "log" ? std::log10(x) : std::log(x);
            }
            if (n == "min" || n == "max") {
                if (a.empty()) fail(n + " butuh minimal 1 argumen");
                double r = a[0];
                for (double x : a) r = (n == "min") ? std::min(r, x) : std::max(r, x);
                return r;
            }
            fail("fungsi tidak dikenal '" + n + "'");
        }
    };
};
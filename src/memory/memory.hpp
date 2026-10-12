#pragma once

#include "llm/llm_types.hpp"

#include <SQLiteCpp/SQLiteCpp.h>

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

class Memory {
public:
    // Buka (atau buat) database dan pastikan tabel ada.
    // Lempar SQLite::Exception (turunan std::runtime_error) jika gagal.
    explicit Memory(const std::string& db_path);

    Memory(const Memory&) = delete;
    Memory& operator=(const Memory&) = delete;

    void add_message(const Message& m);

    // N pesan terakhir, urut dari yang lama ke yang baru.
    std::vector<Message> load_recent(std::size_t limit);

    // Simpan fakta. Kunci yang sama menimpa nilai lama.
    void set_fact(const std::string& key, const std::string& value);

    std::vector<std::pair<std::string, std::string>> all_facts();

private:
    SQLite::Database db_;
};
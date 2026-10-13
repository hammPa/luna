#include "memory/memory.hpp"

Memory::Memory(const std::string& db_path)
    : db_(db_path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE) {
    db_.exec(R"(
        CREATE TABLE IF NOT EXISTS messages (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            role       TEXT NOT NULL,
            content    TEXT NOT NULL,
            name       TEXT NOT NULL DEFAULT '',
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );
        CREATE TABLE IF NOT EXISTS facts (
            key        TEXT PRIMARY KEY,
            value      TEXT NOT NULL,
            updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );
    )");
}

void Memory::add_message(const Message& m) {
    SQLite::Statement st(
        db_, "INSERT INTO messages (role, content, name) VALUES (?, ?, ?)");
    st.bind(1, m.role);  // indeks bind mulai dari 1
    st.bind(2, m.content);
    st.bind(3, m.name);
    st.exec();
}

std::vector<Message> Memory::load_recent(std::size_t limit) {
    // Subquery: ambil N terbaru (DESC), query luar membaliknya jadi kronologis.
    SQLite::Statement st(db_, R"(
        SELECT role, content, name FROM (
            SELECT id, role, content, name FROM messages
            ORDER BY id DESC LIMIT ?
        ) ORDER BY id ASC
    )");
    st.bind(1, static_cast<int>(limit));

    std::vector<Message> out;
    while (st.executeStep()) {
        std::string role = st.getColumn(0);  // indeks kolom mulai dari 0
        std::string content = st.getColumn(1);
        std::string name = st.getColumn(2);
        
        Message m;
        m.role = std::move(role);
        m.content = std::move(content);
        m.name = std::move(name);
        out.push_back(std::move(m));
    }
    return out;
}

void Memory::set_fact(const std::string& key, const std::string& value) {
    SQLite::Statement st(db_, R"(
        INSERT INTO facts (key, value) VALUES (?, ?)
        ON CONFLICT(key) DO UPDATE SET
            value = excluded.value,
            updated_at = CURRENT_TIMESTAMP
    )");
    st.bind(1, key);
    st.bind(2, value);
    st.exec();
}

std::vector<std::pair<std::string, std::string>> Memory::all_facts() {
    SQLite::Statement st(db_, "SELECT key, value FROM facts ORDER BY key");

    std::vector<std::pair<std::string, std::string>> out;
    while (st.executeStep()) {
        std::string k = st.getColumn(0);
        std::string v = st.getColumn(1);
        out.emplace_back(std::move(k), std::move(v));
    }
    return out;
}
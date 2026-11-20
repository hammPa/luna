# Arsitektur

## Struktur proyek

```
luna/
├── CMakeLists.txt
├── config.example.json
├── third_party/            nlohmann/json.hpp, SQLiteCpp/
└── src/
    ├── main.cpp            pemilihan mode, wiring objek
    ├── app/                ChatSession (loop chat + tool), commands, run_cli, register_tools
    ├── llm/                tipe, interface LLMBackend, OpenAIBackend, factory
    ├── memory/             Memory (SQLite: messages, facts)
    ├── search/             SearchProvider, TavilyProvider, search_factory
    ├── tools/              Tool, ToolRegistry, PermissionGate, PathGuard, Prompter, FileSink, FileBlocklist
    │   └── builtin/
    │       ├── files/    list_dir, read_file, create_file, patch_file, write_file,
    │       │             delete_file, search_files, send_file
    │       ├── system/   get_time, open_url, open_app, close_app
    │       ├── window/   switch_workspace, list_windows, focus_window, close_window
    │       └── web/      weather, search_web
    ├── telegram/           client, prompter, loop, file_sink, telegram_format
    ├── auth/               TotpVerifier
    ├── tui/                tui_app, tui_view, tui_events, tui_state, tui_render, tui_text,
│               tui_prompter, theme, slash_menu, tui_markdown, clipboard
    └── utils/              http (post, post_multipart, get), config, log, string_utils, process_utils, format_utils, ctx_format, markdown, markdown_ansi, cancel
```

Aturan dependensi: `llm/` tidak mengenal `memory/`, `tools/`, atau `telegram/`. Format khusus provider hanya ada di `openai_backend.cpp`. Detail provider tidak boleh bocor ke `main.cpp`.
`search/` tidak mengenal `tools/`, `memory/`, atau `telegram/`. Format khusus provider pencarian hanya ada di file provider-nya.

## Menambah tool baru

1. Buat class turunan `Tool` dan letakkan di `src/tools/builtin/<kategori>/` dengan `name`, `description`, `parameters` (JSON schema), `risk(args)`, `describe(args)`, dan `execute(args)`.
2. Include di `builtin_tools.hpp`
3. Daftarkan di `app/register_tools.cpp`, pada fungsi `register_*_tools` yang sesuai: `registry.add(std::make_unique<NamaTool>(...))`. Untuk tool yang butuh program luar, daftarkan hanya jika programnya ada.

Daftar tool di system prompt dan schema untuk model dibuat otomatis dari registry. Gate permission otomatis berlaku. Argumen dari model tidak tepercaya: validasi dengan `arg_string`, dan pakai `PathGuard` untuk semua path.

## Menambah backend LLM

Buat class turunan `LLMBackend`, lalu tambahkan satu cabang di `backend_factory.cpp`. `main` tidak berubah.
# Arsitektur

## Struktur proyek

```
luna/
├── CMakeLists.txt
├── config.example.json
├── third_party/            nlohmann/json.hpp, SQLiteCpp/
└── src/
    ├── main.cpp            pemilihan mode, wiring objek
    ├── app/                ChatSession (loop chat + tool), commands, register_tools, model_switcher
    ├── cli/                run_cli (mode --cli)
    ├── llm/                tipe, interface LLMBackend, factory
    │   └── openai/       openai_backend, openai_http (error HTTP, retry), openai_format (konversi pesan dan tool)
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
    ├── tui/                tui_app, tui_prompter
    │   ├── core/         tui_state, tui_events, tui_view
    │   ├── render/       tui_render, tui_markdown, tui_wrap, tui_utf8, tui_text, theme
    │   └── input/        slash_menu, clipboard, input_history, copy_command
    └── utils/              http (post, post_multipart, get), config, log, string_utils, process_utils, format_utils, ctx_format, markdown, markdown_ansi, cancel
        └── config/       reject_old, apps, profiles, telegram, providers, search, misc (header per bagian load_config)
```

Aturan dependensi: `llm/` tidak mengenal `memory/`, `tools/`, atau `telegram/`. Format khusus provider hanya ada di folder provider-nya (`llm/openai/`). Detail provider tidak boleh bocor ke `main.cpp`.
`search/` tidak mengenal `tools/`, `memory/`, atau `telegram/`. Format khusus provider pencarian hanya ada di file provider-nya.

## Menambah tool baru

1. Buat class turunan `Tool` dan letakkan di `src/tools/builtin/<kategori>/` dengan `name`, `description`, `parameters` (JSON schema), `risk(args)`, `describe(args)`, dan `execute(args)`.
2. Include di `builtin_tools.hpp`
3. Daftarkan di `app/register_tools.cpp`, pada fungsi `register_*_tools` yang sesuai: `registry.add(std::make_unique<NamaTool>(...))`. Untuk tool yang butuh program luar, daftarkan hanya jika programnya ada.

Daftar tool di system prompt dan schema untuk model dibuat otomatis dari registry. Gate permission otomatis berlaku. Argumen dari model tidak tepercaya: validasi dengan `arg_string`, dan pakai `PathGuard` untuk semua path.

## Menambah backend LLM

Buat folder `llm/<nama>/` berisi class turunan `LLMBackend` dan file bantunya (ikuti pola `llm/openai/`: backend, bantu HTTP, konversi format), lalu tambahkan satu cabang di `backend_factory.cpp`. `main` tidak berubah. Provider dan model baru cukup didaftarkan di `providers` pada `config.json`; `/model` memakai daftar itu lewat antarmuka `ModelSwitcher` (diimplementasikan `App` di `main.cpp`), jadi `commands.cpp` tidak mengenal `backend_factory`.

## Retry rate limit dan kabar status

Retry hidup di `OpenAIBackend::chat`, jadi berlaku sama di TUI, CLI, dan Telegram. Respons 429, atau 503 dari router yang membungkus 429, diulang maksimal 2 kali. Waktu tunggu dibaca dari pesan server ("try again in Ns"); tanpa petunjuk dipakai 2 detik lalu 4 detik. Tunggu lebih dari 20 detik berarti menyerah dan error biasa dilempar. Tunggu dipecah per 100 ms dan memeriksa cancel flag, sehingga Esc atau Ctrl+C membatalkannya dengan `CancelledError`. Request yang kena 429 ditolak sebelum diproses, jadi mengirim ulang tidak punya efek samping dan tidak perlu konfirmasi.

Backend tidak boleh mengenal UI, jadi kabar status lewat callback:

- `LLMBackend::set_notice_callback(f)`: backend memanggil `f("kena rate limit, coba ulang 1/2 dalam N dtk...")` sebelum menunggu, dan `f("")` setelah selesai. Bawaannya kosong (backend lain boleh mengabaikan).
- `ChatSession::set_notice(f)`: UI memasang `f` di session, bukan langsung ke backend. Session menyimpan salinannya dan meneruskannya ke backend aktif. Saat `/model` mengganti backend, `set_backend` memasang salinan itu ke backend baru, sama seperti cancel flag. Tanpa salinan, callback hilang setelah ganti model.
- Callback dipanggil dari thread kerja. TUI menyimpan teksnya di `TuiState` (dengan mutex sendiri) lalu meminta gambar ulang; CLI mencetak satu baris redup; Telegram tidak memasang callback.
- Callback di TUI merujuk state lokal `run_tui`, jadi dilepas (`set_notice(nullptr)`) sebelum `run_tui` selesai.

Selain itu, `dump()` JSON yang dikirim ke LLM memakai `error_handler_t::replace`, sehingga byte UTF-8 rusak menjadi karakter pengganti dan tidak melempar `type_error.316`.

## Jejak aktivitas tool

`ChatSession` melaporkan tiap pemanggilan tool lewat dua callback yang dipasang UI, supaya `tools/` tetap tidak mengenal `tui/`:

- `set_notice` (yang sama dengan kabar retry): dipanggil dengan "menjalankan nama..." sebelum `ToolRegistry::run` dan dengan string kosong sesudahnya. `busy_row` menampilkannya.
- `set_tool_event`: dipanggil sekali per tool dengan `ToolEvent{name, status, ms}`. `format_tool_event` (di `chat_session.hpp`) membuat teksnya. TUI menaruhnya sebagai baris `info`, CLI mencetak baris redup, Telegram tidak memasang callback.

Status diturunkan dari teks hasil tool karena `ToolRegistry::run` hanya mengembalikan teks: awalan `Error` = `gagal`; awalan `User menolak` atau `Ditolak oleh kebijakan` (teks dari `PermissionGate`) = `ditolak`; selain itu `ok`. Kalau teks penolakan di `permission.cpp` diubah, `classify_tool_result` di `chat_session.cpp` harus ikut diubah. Seperti kabar retry, `set_tool_event` tidak ikut `set_backend` karena tidak bergantung pada backend, dan harus dilepas (`nullptr`) sebelum `run_tui` selesai karena merujuk state lokal.
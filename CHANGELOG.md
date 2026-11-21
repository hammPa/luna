# Riwayat versi

## 1.4.1
Profil tool: `profiles` dan `active_profile` di config, `/profile` untuk melihat dan mengganti profil (TUI dan CLI); header TUI menampilkan jumlah tool aktif dan nama profil secara langsung

## 1.4.0
Hidup/matikan tool saat berjalan: `/tools` dan `/tool on|off nama` (TUI dan CLI); tool nonaktif tidak dikirim ke model sehingga hemat token

## 1.3.2
Refactor `main.cpp` tanpa perubahan perilaku: `Mode` dipakai di semua tempat, argumen dipisah ke `parse_mode()`, objek dikumpulkan di `struct App` dengan `start_app()`, mode CLI pindah ke `app/run_cli.cpp`, dan `register_*_tools` pindah ke `app/register_tools.cpp`

## 1.3.1
Gulir TUI kembali mulus per baris layar, blok kode dibungkus; daftar tool tidak lagi dikirim dua kali ke model (hemat sekitar 700-800 token per panggilan)

## 1.3.0
Render markdown di TUI, CLI, dan Telegram (tebal, miring, kode, daftar, judul, kutipan, tautan, blok kode berbahasa); tombol batal giliran (Esc di TUI, Ctrl+C di CLI); `/copy` dan `/copy code` di TUI

## 1.2
Indikator konteks (TUI/CLI), `/ctx`, modularisasi TUI

## 1.1
TUI: tema, gulir riwayat, menu perintah `/`; struktur `tools/builtin/` per kategori

## 1.0
Antarmuka TUI (FTXUI) menggantikan CLI sebagai mode bawaan; CLI tetap ada lewat `--cli`

## 0.9
Log ke file (rotasi, tanpa isi pesan) dan dokumentasi model lokal lewat Ollama

## 0.8
Akses web: `HttpClient::get`, `weather` (Open-Meteo), `search_web` dengan provider pencarian yang bisa diganti (`SearchProvider`, saat ini Tavily)

## 0.7
`search_files`, `send_file` (unggah file ke Telegram dengan persetujuan TOTP), tingkat risiko `Exfiltrate`, upload multipart

## 0.6
Kontrol jendela Hyprland (`list_windows`, `focus_window`, `close_window`), `close_app` melaporkan hasil sebenarnya

## 0.5
`delete_file` (ke `.trash`), `switch_workspace` (Hyprland), `/clear`, base URL Telegram yang bisa diatur (reverse proxy)

## 0.4
Mode Telegram, persetujuan dari HP, TOTP untuk aksi berbahaya

## 0.3
Tool calling, permission gate, tool file dan aplikasi

## 0.2
Memori SQLite (`messages`, `facts`)

## 0.1
Chat CLI, `HttpClient`, backend OpenAI-compatible, config
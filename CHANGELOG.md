# Riwayat versi

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
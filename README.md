# Luna

Asisten AI pribadi berbasis C++17 yang berjalan di terminal Linux dengan antarmuka TUI. Luna bisa ngobrol, mengingat hal tentangmu, menjalankan tool (file, aplikasi, URL, workspace dan jendela desktop, kirim file), dan bisa dikendalikan dari HP lewat bot Telegram, dengan sistem izin berlapis supaya aksi berbahaya tidak jalan sembarangan.

**Versi saat ini: 1.4.1**

[Riwayat versi](CHANGELOG.md)

## Fitur

- **Antarmuka TUI** (FTXUI): layar chat satu halaman, kotak izin interaktif, indikator saat menunggu jawaban. Mode `--cli` tetap tersedia untuk SSH dan pipe.
- **Backend AI agnostik**: semua provider yang kompatibel dengan OpenAI API (OpenAI, Mistral, Groq, DeepSeek, OpenRouter, router seperti 9router) dan model lokal lewat Ollama. Ganti model/provider cukup edit `config.json`, tanpa rebuild.
- **Memori jangka panjang** (SQLite): riwayat chat dan fakta permanen tentangmu.
- **Tool calling**: model memanggil tool yang kamu daftarkan.
- **Permission gate**: setiap tool punya tingkat risiko; yang berisiko ditanyakan dulu.
- **Hidup/matikan tool**: `/tools` dan `/tool on|off nama` di TUI dan CLI. Tool yang dimatikan tidak dikirim ke model, jadi hemat token.
- **Profil tool**: kumpulan tool aktif yang diberi nama di `config.json` (mis. `file`, `jendela`), diganti dengan `/profile nama`. Hemat token karena hanya tool yang perlu yang dikirim ke model.
- **Kontrol desktop Hyprland**: pindah workspace, lihat jendela yang terbuka, pindah fokus, dan tutup satu jendela atau seluruh aplikasi.
- **Mode Telegram**: kendalikan Luna dari HP dengan allowlist user, persetujuan y/n, dan kode TOTP untuk aksi berbahaya. Mendukung reverse proxy.
- **Cuaca dan pencarian web**: `weather` (tanpa API key) dan `search_web` lewat provider yang bisa diganti. Hasil web ditandai sebagai data tak tepercaya.
- **Log ke file**: riwayat aktivitas (panggilan LLM, aksi tool, aktivitas Telegram) tersimpan sebagai metadata, tanpa isi pesan.
- Indikator konteks: seberapa penuh percakapan terhadap `max_context`, di header TUI dan setelah tiap balasan di CLI
- **Render markdown**: tebal, miring, kode, daftar, judul, dan blok kode berbahasa di TUI, CLI, dan Telegram; tombol batal giliran dan /copy.

## Prasyarat

Sistem: **Linux** (memakai `fork`, `execvp`, `xdg-open`, `pkill`, `localtime_r`).

| Kebutuhan | Keterangan |
|---|---|
| Compiler C++17 | `g++` 9+ atau `clang++` |
| CMake | 3.15 atau lebih baru |
| libcurl (dev) | `sudo apt install libcurl4-openssl-dev` |
| nlohmann/json | satu file header di `third_party/nlohmann/json.hpp` |
| SQLiteCpp | di `third_party/SQLiteCpp` |
| FTXUI v6.1.9 | di `third_party/FTXUI` (di-vendor, tidak perlu dipasang sistem) |

Program pendukung (opsional; tool terkait hanya aktif jika programnya ada):

| Program | Untuk |
|---|---|
| `xdg-open` | `open_url` |
| `pkill`, `pgrep` (paket `procps`) | `close_app` |
| `hyprctl` | `switch_workspace`, `list_windows`, `focus_window`, `close_window` (hanya di dalam sesi Hyprland) |
| `oathtool` | TOTP (`sudo apt install oathtool`) |

Opsional: akun penyedia pencarian (saat ini Tavily) agar `search_web` aktif. `weather` tidak butuh akun.

## Build

```bash
git clone https://github.com/hammPa/luna
cd luna

# dependensi di third_party
mkdir -p third_party/nlohmann
# letakkan json.hpp dari rilis nlohmann/json di third_party/nlohmann/json.hpp
git clone https://github.com/SRombauts/SQLiteCpp.git third_party/SQLiteCpp

git clone --depth 1 --branch v6.1.9 https://github.com/ArthurSonzogni/FTXUI.git third_party/FTXUI

cp config.example.json config.json   # lalu sesuaikan

mkdir build && cd build
cmake ..
make
```

Binary: `build/luna`. Peringatan kompiler penuh aktif (`-Wall -Wextra -Wpedantic`); target kualitas: build tanpa warning.

## Menjalankan

Dari folder `build/`:

```bash
# TUI (bawaan)
./luna ../config.json

# Mode Cli
./luna ../config.json --cli

# Mode Telegram
./luna ../config.json --telegram
```

Argumen pertama adalah path config (default `config.json`). Argumen kedua opsional: `--cli` atau `--telegram`; argumen lain ditolak. TUI butuh terminal interaktif; untuk pipe pakai `--cli`, mis. `echo "halo" | ./luna ../config.json --cli`.

## Dokumentasi

- [Konfigurasi](docs/config.md) dan [Ollama (model lokal)](docs/ollama.md)
- [TUI dan indikator konteks](docs/tui.md)
- [Tool, sistem izin, pencarian web](docs/tools.md)
- [Telegram dan TOTP](docs/telegram.md)
- [Log](docs/log.md)
- [Arsitektur dan cara menambah tool/backend](docs/architecture.md)
- [Keamanan, privasi, keterbatasan](docs/security.md)
- [Troubleshooting](docs/troubleshooting.md)

## Roadmap

- [x] `delete_file` (ke `.trash/`), `/clear`
- [x] `switch_workspace` (Hyprland)
- [x] Base URL Telegram yang bisa diatur
- [x] Kontrol jendela: `list_windows`, `focus_window`, `close_window`
- [x] `search_files` dan `send_file` (kirim file ke Telegram)
- [x] `HttpClient::get`, lalu `search_web`, `fetch_url`, `weather`
- [x] Backend lokal (Ollama)
- [x] Log ke file
- [x] Antarmuka TUI (FTXUI), CLI tetap tersedia lewat `--cli`
- [x] TUI: gulir riwayat, markdown, blok kode, tombol batal, `/copy`
- [x] Hidup/matikan tool saat berjalan
- [x] Profil tool (kumpulan tool aktif yang bisa diganti)
- [ ] TUI: riwayat input, aktivitas tool di layar, seleksi mouse otomatis.
- [ ] Streaming balasan
- [ ] Tool tambahan: `calculator`, `save_fact`, info sistem, kontrol volume dan media, `move_file`, `create_dir`, `append_file`
- [ ] `open_url` dengan pilihan browser dan jendela baru
- [ ] Tes otomatis (target CMake) dan CI
- [ ] Service systemd, pengingat, dan tugas terjadwal
- [ ] Sudo dengan allowlist perintah
- [ ] Voice note Telegram dan input suara (STT/TTS di luar inti Luna)
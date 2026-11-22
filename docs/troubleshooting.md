# Troubleshooting

## Troubleshooting

| Gejala | Penyebab umum |
|---|---|
| `Gagal start: ... belum di-set` | Env var belum di-export di terminal ini |
| Tool file tidak muncul | `allowed_dirs` kosong atau tidak terbaca dari config |
| Folder workspace kosong | `allowed_dirs[0]` menunjuk folder lain dari yang kamu kira |
| Tool Hyprland tidak muncul | Luna tidak berjalan di dalam sesi Hyprland (`HYPRLAND_INSTANCE_SIGNATURE` kosong), atau `hyprctl` tidak ada |
| Model tidak memanggil tool | Riwayat lama memengaruhi model; coba `/clear`, atau ganti model |
| Jawaban aneh/patah-patah | Router dapat menyisipkan prompt tersembunyi; bandingkan langsung ke provider |
| `close_app` melapor "masih berjalan" | Aplikasi menunggu konfirmasi (dialog tutup semua tab). Pakai `close_window` per jendela |
| `close_app` melapor "tidak sedang berjalan" padahal terbuka | Nilai `process` salah; cek `ps -eo comm | grep -i nama` |
| `close_window` "jendela tidak ditemukan" | Alamat basi; panggil `list_windows` lagi |
| `Timeout was reached` ke Telegram | Jaringan memblokir `api.telegram.org`; pakai `telegram_api_base_url` |
| Telegram tidak membalas | Cek token (`getMe`), ID di allowlist, dan hanya satu instance berjalan (error 409) |
| `telegram_api_base_url harus diawali https://` | Base URL memakai `http://` |
| Kode TOTP selalu salah | Jam tidak sinkron (`timedatectl`), atau secret berbeda dengan di app |
| Riwayat "hilang" setelah pindah folder | `memory_db` relatif; pakai path absolut |
| `search_web` tidak muncul | Blok `search` kosong, atau env API key belum di-export (cari log `[search] ... dimatikan`) |
| `Gagal start: search.provider tidak dikenal` | Nama provider salah ketik; saat ini hanya `tavily` |
| `pencarian gagal: Tavily HTTP 401` | API key salah atau kedaluwarsa |
| `pencarian gagal: Tavily HTTP 429` | Kuota atau batas laju provider habis |
| `weather`: "Kota ... tidak ditemukan" | Nama kota tidak dikenali geocoding; coba nama lain atau tambah negara |
| `weather`: "kota tidak diberikan" | Tidak menyebut kota dan `default_location` kosong |
| File log tidak terbentuk | `log_file` kosong, atau foldernya tidak ada / tidak bisa ditulis (cari peringatan di stderr) |
| `Gagal start: log_level harus ...` | Nilai `log_level` salah ketik |
| Luna gagal terhubung ke Ollama | Pastikan servis Ollama hidup: `curl http://localhost:11434/api/tags` |
| Model lokal lambat dan CPU panas | Cek `ollama ps`; jika bukan `100% GPU`, model terlalu besar untuk VRAM atau driver CUDA tidak terbaca |
| `TUI butuh terminal interaktif` | Input atau output bukan terminal (pipe, service). Pakai `--cli` |
| Terminal berantakan setelah Luna ditutup paksa | Jalankan `reset` |
| Tidak ada peringatan atau error di layar | Di TUI `stderr` disembunyikan; lihat `luna.log` |
| Enter tidak berfungsi | Luna sedang menjawab, atau kotak izin sedang terbuka |
| Karakter aneh atau kotak-kotak | Terminal atau font tidak mendukung UTF-8 |
| `Argumen tidak dikenal` | Argumen kedua bukan `--cli` atau `--telegram` |
| Angka konteks berawalan `~` | Provider tidak mengirim `usage`, jadi angkanya perkiraan |
| Persen konteks tidak masuk akal | `max_context` di config harus sama dengan konteks
nyata model. Di Ollama, cek `ollama ps` atau `num_ctx` |
| `/copy` tidak menempel | Terminal belum mengizinkan OSC 52 (di kitty cek `clipboard_control`; di tmux butuh `set-clipboard on`) |
| `Gagal start: field '...' tingkat atas tidak dipakai lagi` | Config format lama; pindahkan ke `providers` dan `active` (lihat `config.example.json`) |
| `Gagal start: model '...' tidak terdaftar` | Model di `active` belum ada di `providers.<nama>.models` |
| `Gagal start: model '...' wajib punya 'max_context'` | Isi `max_context` dan `history_limit` di entri `models` |
| `Gagal ganti model: Environment variable ... belum di-set` | API key provider tujuan belum di-export di terminal ini; model lama tetap aktif |
| `/model` tidak dikenali di Telegram | Memang tidak tersedia di Telegram; ganti `active` di config lalu restart |
| Layar menampilkan "kena rate limit, coba ulang N/2 dalam X dtk..." | Provider membatasi token per menit (mis. Groq 8000 TPM). Luna menunggu lalu mengirim ulang otomatis; Esc membatalkan. Normal, tidak perlu tindakan |
| `HTTP 503 ... [429] ... Please try again in 29s` (log: `rate limit, tunggu ... melebihi batas, menyerah`) | Server meminta tunggu lebih dari 20 detik, jadi Luna tidak mengulang. Tunggu sebentar lalu kirim lagi. Untuk menghindarinya, perkecil request: `/profile nama` atau `/tool off nama` supaya lebih sedikit tool dikirim, dan `/clear` bila riwayat panjang |
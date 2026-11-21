# Keamanan, privasi, dan keterbatasan

## Keamanan dan privasi

- Jangan commit `config.json` yang berisi ID pribadi, `memory.db`, atau secret apa pun. Commit `config.example.json` saja.
- Pesan, isi file yang dibaca tool, daftar jendela, hasil pencarian, dan hasil cuaca dikirim ke provider LLM yang kamu pilih, dan lewat server Telegram (atau proxy-mu) jika memakai mode Telegram.
- Kata kunci pencarian dikirim ke provider pencarian (Tavily); nama kota untuk `weather` dikirim ke Open-Meteo. Hindari menaruh data pribadi di kata kunci.
- Perlakukan token bot dan secret TOTP seperti password. Jika bocor: `/revoke` di BotFather, dan buat secret TOTP baru.
- File yang dikirim lewat Telegram tersimpan di server Telegram (bot tidak end-to-end encrypted). Jangan kirim dokumen yang sangat sensitif.
- Log hanya memuat metadata (ID pengirim, panjang pesan, nama tool, durasi), bukan isi percakapan. Tetap anggap `luna.log` sebagai data pribadi: jangan di-commit (`*.log` ada di `.gitignore`).
- Dengan Ollama, pesan diproses di laptopmu dan tidak dikirim ke provider LLM cloud (Telegram, `search_web`, dan `weather` tetap memakai layanan masing-masing).
- Teks dari model (termasuk hasil pencarian web) dibersihkan dari karakter kontrol sebelum ditampilkan di TUI, sehingga tidak bisa mengirim escape sequence ke terminal.

## Keterbatasan saat ini

- Hanya Linux; tool jendela hanya untuk Hyprland.
- Backend lokal hanya lewat Ollama (HTTP).
- Luna **reaktif**: hanya bertindak saat ada perintah. Belum ada tugas terjadwal, pengingat, atau pemicu otomatis, dan belum bisa menghubungimu duluan.
- Belum berjalan sebagai service latar belakang.
- `list_windows` mengirim judul jendela (termasuk judul tab browser) ke provider LLM. Pertimbangkan itu jika judulnya sensitif.
- Pesan perantara tool tidak disimpan ke database (hanya pesan user dan jawaban akhir).
- Selama menunggu persetujuan di Telegram, pesan lain tidak diproses.
- Indikator "mengetik" Telegram hanya bertahan sekitar 5 detik.
- Akses web baru sebatas `weather` dan `search_web` (cuplikan hasil). Belum ada `fetch_url` untuk membaca isi halaman.
- Hanya satu provider pencarian (Tavily) yang tersedia.
- Cuaca hanya dalam satuan metrik (°C, km/j).
- Belum ada tes otomatis.
- Log belum mencatat permintaan izin dan percobaan kode TOTP.
- TUI belum punya streaming, dan salin otomatis lewat seleksi mouse (hanya `/copy`).
- Aktivitas tool (nama, durasi) belum tampil di layar chat; lihat `luna.log`.
- Menutup TUI saat panggilan LLM berjalan menunggu panggilan itu selesai.
- `max_context` hanya untuk tampilan; riwayat dipangkas berdasarkan jumlah pesan.
- Status hidup/mati tool tidak disimpan ke config; saat restart semua tool kembali aktif.
- Pembatalan giliran (Esc/Ctrl+C) belum tersedia di Telegram.
- `/copy` hanya ada di TUI.
- Blok kode di TUI tidak dibungkus; baris yang lebih lebar dari layar terpotong.
- Ganti model lewat `/model` tidak disimpan ke config dan tidak tersedia di Telegram.
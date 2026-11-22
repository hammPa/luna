# Telegram

## Mode Telegram

### Menyiapkan bot

1. Di Telegram, buka **@BotFather**, kirim `/newbot`, ikuti langkahnya, simpan token.
2. `export LUNA_ASSISTANT_BOT_TOKEN='...'`
3. Kirim satu pesan ke botmu, lalu ambil ID-mu:
   ```bash
   curl "https://api.telegram.org/bot$LUNA_ASSISTANT_BOT_TOKEN/getUpdates"
   ```
   Catat `message.from.id` dan isi ke `telegram_allowed_users`.
4. Jalankan `./luna ../config.json --telegram`.

### Cara kerja

Luna melakukan long polling ke Telegram (`getUpdates`), jadi tidak butuh IP publik. Komputer harus menyala dan Luna harus berjalan.

Jika provider LLM kena rate limit (429), Luna mencoba ulang otomatis (maks 2 kali, tunggu sampai 20 detik per percobaan), sama seperti di TUI dan CLI. Di Telegram tidak ada indikator untuk itu, dan menunggunya belum bisa dibatalkan karena tombol batal belum ada.

### Reverse proxy

Jika jaringan tempat Luna berjalan tidak bisa menjangkau `api.telegram.org` (beberapa hosting memblokirnya), isi `telegram_api_base_url` dengan URL proxy milikmu sendiri. Aturan:

- Harus `https://` (token bot ada di path URL).
- Trailing slash dibuang otomatis.
- Proxy dapat membaca semua pesan dan token, jadi **hanya pakai proxy yang kamu kontrol**, dan batasi agar hanya meneruskan path bot-mu.
- Tes dulu: `curl "<base_url>/bot$LUNA_ASSISTANT_BOT_TOKEN/getMe"` harus mengembalikan `"ok":true`.

### Keamanan Telegram

| Hal | Perilaku |
|---|---|
| Allowlist | Hanya user ID di config yang dilayani; lainnya diabaikan diam-diam |
| Pesan lama | Dibuang saat start, tidak dieksekusi |
| Batas input | 2000 karakter |
| Log | Hanya panjang pesan, bukan isinya |
| Safe | Langsung jalan |
| Write, Execute | Balas `y` atau `n` (timeout 60 detik, tanpa "always") |
| Destructive, Privileged, Exfiltrate | Butuh kode TOTP; tanpa `LUNA_TOTP_SECRET` otomatis ditolak |
| `send_file` | Satu kode untuk seluruh pengiriman (maks 5 file). File rahasia (`.env`, `*.key`, `*.db`, `config.json`, folder `.ssh`/`.git`) selalu ditolak |
| `close_window` | Wajib memakai alamat eksplisit dari `list_windows` |
| Batas laju | Maks 5 permintaan izin per 10 menit |
| Jawaban izin/kode | Dicegat sebelum model; tidak masuk riwayat atau database |

### Menyiapkan TOTP (opsional, untuk aksi berbahaya)

```bash
sudo apt install oathtool
openssl rand 20 | base32          # buat secret, buang '=' di ujung
export LUNA_TOTP_SECRET='SECRET_BASE32'
```

Daftarkan secret ke app authenticator (input manual, TOTP, SHA1, 6 digit, 30 detik), lalu pastikan kodenya sama dengan `oathtool --totp -b "$LUNA_TOTP_SECRET"`. Jam komputer dan HP harus akurat.

Kode berlaku sekali pakai, 3 kali salah mengunci 10 menit, dan secret dikirim ke `oathtool` lewat stdin (tidak terlihat di `ps`).

## Deploy di cloud (catatan)

Luna bisa dijalankan di server (Docker, Railway, VPS) hanya sebagai bot chat. Hal yang perlu diperhatikan:

- Simpan token, API key, dan secret TOTP di **secrets/environment** layanan, jangan di Dockerfile atau `config.json`.
- Jangan menyajikan folder kerja lewat HTTP. Jika layanan butuh port untuk health check, sajikan folder kosong, karena `config.json`, binary, dan `memory.db` tidak boleh bisa diunduh.
- Penyimpanan gratis biasanya tidak permanen, jadi `memory.db` hilang saat restart.
- Tool desktop tidak berguna di server. Untuk mengontrol laptopmu dari HP, Luna harus berjalan di laptop itu.
- Sebagian layanan hosting tidak bisa menjangkau Telegram; gunakan `telegram_api_base_url`.
# Luna

Asisten AI pribadi berbasis C++17 yang berjalan di terminal Linux. Luna bisa ngobrol, mengingat hal tentangmu, menjalankan tool (file, buka/tutup aplikasi, buka URL), dan bisa dikendalikan dari HP lewat bot Telegram, dengan sistem izin berlapis supaya aksi berbahaya tidak jalan sembarangan.

**Versi saat ini: 0.4.0 (pra-rilis, status: dalam pengembangan)**

| Versi | Isi |
|---|---|
| 0.1 | Chat CLI, `HttpClient`, backend OpenAI-compatible, config |
| 0.2 | Memori SQLite (`messages`, `facts`) |
| 0.3 | Tool calling, permission gate, tool file/aplikasi |
| 0.4 | Mode Telegram, persetujuan dari HP, TOTP untuk aksi berbahaya |

---

## Fitur

- **Chat teks** di terminal, dengan riwayat percakapan.
- **Backend AI agnostik**: semua provider yang kompatibel dengan OpenAI API (OpenAI, Mistral, Groq, DeepSeek, OpenRouter, atau router seperti 9router). Ganti model/provider cukup edit `config.json`, tanpa rebuild.
- **Memori jangka panjang** (SQLite): riwayat chat dan fakta permanen tentangmu.
- **Tool calling**: model bisa memanggil tool yang kamu daftarkan.
- **Permission gate**: setiap tool punya tingkat risiko; yang berisiko ditanyakan dulu ke kamu.
- **Mode Telegram**: kendalikan Luna dari HP, dengan allowlist user, persetujuan y/n, dan kode TOTP untuk aksi berbahaya.

---

## Prasyarat

Sistem: **Linux** (memakai `fork`, `execvp`, `xdg-open`, `pkill`, `localtime_r`).

| Kebutuhan | Keterangan |
|---|---|
| Compiler C++17 | `g++` 9+ atau `clang++` |
| CMake | 3.15 atau lebih baru |
| libcurl (dev) | `sudo apt install libcurl4-openssl-dev` |
| nlohmann/json | satu file header di `third_party/nlohmann/json.hpp` |
| SQLiteCpp | di `third_party/SQLiteCpp` (git clone atau submodule) |
| `xdg-open` | untuk tool `open_url` (biasanya sudah ada) |
| `pkill` | untuk tool `close_app` (paket `procps`) |
| `oathtool` | untuk TOTP: `sudo apt install oathtool` (hanya jika memakai TOTP) |

---

## Build

```bash
git clone https://github.com/hammPa/luna
cd luna

# dependensi di third_party
mkdir -p third_party/nlohmann
# letakkan json.hpp dari rilis nlohmann/json di third_party/nlohmann/json.hpp
git clone https://github.com/SRombauts/SQLiteCpp.git third_party/SQLiteCpp

mkdir build && cd build
cmake ..
make
```

Binary: `build/luna`.

Build debug dengan peringatan penuh sudah aktif (`-Wall -Wextra -Wpedantic`). Target kualitas: build tanpa warning.

---

## Konfigurasi

### Environment variable (rahasia, jangan di-commit)

| Variable | Fungsi |
|---|---|
| `NINE_ROUTER_API_KEY` | API key provider LLM (nama variabel bisa diubah lewat `api_key_env`) |
| `LUNA_ASSISTANT_BOT_TOKEN` | Token bot dari @BotFather (nama bisa diubah lewat `telegram_token_env`) |
| `LUNA_TOTP_SECRET` | Secret TOTP base32 (opsional, untuk aksi berbahaya dari Telegram) |

Tambahkan ke `~/.zshrc` atau `~/.bashrc`, lalu `source`.

### `config.json`

```json
{
    "backend": "openai",
    "model": "mistral/ministral-3b-latest",
    "base_url": "https://api.mistral.ai/v1",
    "api_key_env": "MISTRAL_API_KEY",
    "system_prompt": "Kamu Luna, asisten pribadi yang ramah dan to the point.",
    "history_limit": 20,
    "max_context": 4096,
    "timeout_sec": 30,
    "memory_db": "memory.db",

    "allowed_dirs": ["/home/USER/luna-workspace"],

    "permissions": {
        "safe": "allow",
        "write": "ask",
        "destructive": "ask",
        "execute": "ask",
        "privileged": "ask"
    },

    "apps": {
        "browser":  {"run": "firefox", "process": "firefox"},
        "terminal": {"run": "gnome-terminal"},
        "files":    {"run": "nautilus", "process": "nautilus"}
    },

    "telegram_allowed_users": [123456789],
    "telegram_token_env": "LUNA_ASSISTANT_BOT_TOKEN",
    "telegram_api_base_url": ""
}
```

| Field | Arti |
|---|---|
| `backend` | Saat ini hanya `openai` (format OpenAI-compatible) |
| `model` | Nama model sesuai provider |
| `base_url` | URL dasar API, tanpa `/chat/completions` |
| `api_key_env` | **Nama** env var yang berisi API key (bukan key-nya) |
| `system_prompt` | Aturan umum Luna. Daftar tool dan fakta ditambahkan otomatis |
| `history_limit` | Jumlah pesan riwayat yang dikirim (tanpa system prompt) |
| `max_context` | Disiapkan untuk backend lokal, belum dipakai |
| `timeout_sec` | Batas waktu request HTTP |
| `memory_db` | Path file SQLite (relatif terhadap folder tempat Luna dijalankan; pakai path absolut agar konsisten) |
| `allowed_dirs` | Folder yang boleh disentuh tool file. Entri pertama = workspace utama. **Kosong = tool file tidak aktif.** Pakai path lengkap, `~` tidak diperluas |
| `permissions` | Kebijakan per tingkat risiko: `allow`, `ask`, `deny` |
| `apps` | Allowlist aplikasi. `run` = program untuk membuka, `process` = nama proses untuk menutup (tanpa `process` = tidak bisa ditutup). Nilai tanpa spasi/argumen |
| `telegram_allowed_users` | Daftar user ID numerik yang boleh memakai bot. **Kosong = mode Telegram menolak start** |
| `telegram_token_env` | Nama env var token bot |
| `telegram_api_base_url` | Base URL |

Contoh provider:

| Provider | `base_url` |
|---|---|
| Mistral | `https://api.mistral.ai/v1` |
| OpenAI | `https://api.openai.com/v1` |
| Groq | `https://api.groq.com/openai/v1` |
| OpenRouter | `https://openrouter.ai/api/v1` |

---

## Menjalankan

Dari folder `build/`:

```bash
# Mode terminal
./luna ../config.json

# Mode Telegram
./luna ../config.json --telegram
```

Argumen pertama adalah path config (default `config.json` di folder aktif). `--telegram` harus argumen kedua.

### Perintah di chat

| Perintah | Fungsi |
|---|---|
| `/remember kunci=nilai` | Simpan fakta permanen, mis. `/remember nama=Budi` |
| `/facts` | Tampilkan semua fakta |
| `/start`, `/help` | Bantuan singkat (Telegram) |
| `exit` / `quit` / Ctrl+D | Keluar (mode terminal) |

Fakta otomatis disisipkan ke system prompt di setiap giliran, jadi tidak terpotong oleh `history_limit`.

---

## Tool yang tersedia

| Tool | Fungsi | Risiko | Aktif jika |
|---|---|---|---|
| `get_time` | Tanggal dan jam sekarang | Safe | selalu |
| `list_dir` | Isi folder workspace | Safe | `allowed_dirs` terisi |
| `read_file` | Baca file teks (maks 64 KB, tolak file biner) | Safe | `allowed_dirs` terisi |
| `create_file` | Buat file baru (gagal jika sudah ada) | Write | `allowed_dirs` terisi |
| `patch_file` | Ganti satu potongan teks (harus muncul tepat sekali) | Write | `allowed_dirs` terisi |
| `write_file` | Tulis file; menimpa = Destructive | Write / Destructive | `allowed_dirs` terisi |
| `open_url` | Buka URL http/https di browser default | Execute | selalu |
| `open_app` | Buka aplikasi dari allowlist `apps` | Execute | `apps` terisi |
| `close_app` | Tutup aplikasi (SIGTERM) yang punya `process` | Destructive | ada app dengan `process` |

Contoh: "buka youtube cari lofi", "buat file catatan.txt isinya ...", "ganti 'Halo' jadi 'Hai' di catatan.txt", "tutup browser".

Batasan: `open_url` hanya membuka halaman (Luna tidak membaca isinya) dan selalu memakai browser default.

---

## Sistem izin (permission)

Semua tool lewat satu gerbang di `ToolRegistry::run`; tool tidak bisa melewatinya.

| Risiko | Contoh | Perilaku default |
|---|---|---|
| Safe | `read_file`, `list_dir` | Langsung jalan |
| Write | `create_file`, `patch_file` | Tanya; boleh "always" untuk sesi ini (hanya di terminal) |
| Destructive | menimpa file, `close_app` | Selalu tanya, **tanpa "always"** |
| Execute | `open_url`, `open_app` | Selalu tanya, **tanpa "always"** |
| Privileged | (belum ada tool) | Selalu tanya, tanpa "always" |

Aturan keras: kebijakan `allow` untuk Destructive/Execute/Privileged dipaksa menjadi `ask`. Tanpa jawaban, EOF, atau error berarti **ditolak**.

Lapis keamanan lain:

- **PathGuard**: path dinormalisasi (`..`, symlink diselesaikan) dan harus di dalam `allowed_dirs`; di luar itu ditolak tanpa bertanya.
- **Tanpa shell**: aplikasi dijalankan lewat `fork` + `execvp` dengan argumen terpisah; tidak ada `system()`.
- **Allowlist aplikasi**: model hanya memilih nama dari daftar, tidak bisa memberi perintah atau nama proses bebas.
- **Penulisan atomik** file (file sementara lalu `rename`), batas konten 256 KB.

---

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

Luna melakukan long polling ke Telegram (`getUpdates`), jadi laptop tidak butuh IP publik. Laptop harus menyala dan Luna harus berjalan.

### Keamanan Telegram

| Hal | Perilaku |
|---|---|
| Allowlist | Hanya user ID di config yang dilayani; lainnya diabaikan diam-diam |
| Pesan lama | Dibuang saat start, tidak dieksekusi |
| Batas input | 2000 karakter |
| Log | Hanya panjang pesan, bukan isinya |
| Safe | Langsung jalan |
| Write, Execute | Balas `y` atau `n` (timeout 60 detik, tanpa "always") |
| Destructive, Privileged | Butuh kode TOTP; tanpa `LUNA_TOTP_SECRET` otomatis ditolak |
| Batas laju | Maks 5 permintaan izin per 10 menit |
| Jawaban izin/kode | Dicegat sebelum model; tidak masuk riwayat atau database |

### Menyiapkan TOTP (opsional, untuk aksi berbahaya)

```bash
sudo apt install oathtool
openssl rand 20 | base32          # buat secret, buang '=' di ujung
export LUNA_TOTP_SECRET='SECRET_BASE32'
```

Daftarkan secret ke app authenticator (input manual, TOTP, SHA1, 6 digit, 30 detik), lalu pastikan kodenya sama dengan `oathtool --totp -b "$LUNA_TOTP_SECRET"`. Jam laptop dan HP harus akurat.

Kode berlaku sekali pakai, 3 kali salah mengunci 10 menit, dan secret dikirim ke `oathtool` lewat stdin (tidak terlihat di `ps`).

---

## Struktur proyek

```
luna/
├── CMakeLists.txt
├── config.json
├── third_party/            nlohmann/json.hpp, SQLiteCpp/
└── src/
    ├── main.cpp            pemilihan mode, wiring objek
    ├── app/                ChatSession (loop chat + tool), commands
    ├── llm/                tipe, interface LLMBackend, OpenAIBackend, factory
    ├── memory/             Memory (SQLite: messages, facts)
    ├── tools/              Tool, ToolRegistry, PermissionGate, PathGuard, Prompter
    │   └── builtin/        satu file per tool
    ├── telegram/           client, prompter, loop
    ├── auth/               TotpVerifier
    └── utils/              http (libcurl), config, string, process
```

Aturan dependensi: `llm/` tidak mengenal `memory/`, `tools/`, atau `telegram/`. Format khusus provider hanya ada di `openai_backend.cpp`. Detail provider tidak boleh bocor ke `main.cpp`.

---

## Menambah tool baru

1. Buat class turunan `Tool` di `src/tools/builtin/` dengan `name`, `description`, `parameters` (JSON schema), `risk(args)`, `describe(args)`, dan `execute(args)`.
2. Daftarkan di `main.cpp`: `registry->add(std::make_unique<NamaTool>(...))`.

Daftar tool di system prompt dan schema untuk model dibuat otomatis dari registry. Gate permission otomatis berlaku. Argumen dari model tidak tepercaya: validasi tipe dengan `arg_string`, dan pakai `PathGuard` untuk semua path.

## Menambah backend LLM

Buat class turunan `LLMBackend`, lalu tambahkan satu cabang di `backend_factory.cpp`. `main` tidak berubah.

---

## Troubleshooting

| Gejala | Penyebab umum |
|---|---|
| `Gagal start: ... belum di-set` | Env var API key belum di-export di terminal ini |
| Tool file tidak muncul | `allowed_dirs` kosong atau tidak terbaca dari config |
| Folder workspace kosong | `allowed_dirs[0]` menunjuk folder lain dari yang kamu kira |
| Model tidak memanggil tool | Riwayat lama memengaruhi model; coba hapus `memory.db`, atau ganti model |
| Jawaban aneh/patah-patah | Router dapat menyisipkan prompt tersembunyi; bandingkan langsung ke provider |
| `close_app` tidak menutup | Nilai `process` salah; cek `ps -eo comm | grep -i nama` |
| Telegram tidak membalas | Cek token (`getMe`), ID di allowlist, dan hanya satu instance berjalan (error 409) |
| Kode TOTP selalu salah | Jam tidak sinkron (`timedatectl`), atau secret berbeda dengan di app |

---

## Keterbatasan saat ini

- Hanya Linux.
- Backend lokal (Ollama/llama.cpp) belum ada; semua lewat API.
- Pesan perantara tool tidak disimpan ke database (hanya pesan user dan jawaban akhir).
- Selama menunggu persetujuan di Telegram, pesan lain tidak diproses.
- Indikator "mengetik" Telegram hanya bertahan sekitar 5 detik.
- Belum ada perintah `/clear`.

## Roadmap

- [ ] `delete_file` (ke `.trash/`), `/clear`
- [ ] `open_url` dengan pilihan browser dan jendela baru
- [ ] `search_web` (API resmi)
- [ ] Sudo dengan allowlist perintah
- [ ] Backend lokal (Ollama, llama.cpp)
- [ ] Fase 4: suara (TTS piper, STT whisper.cpp, push-to-talk)

## Keamanan dan privasi

- Jangan commit `config.json` yang berisi ID pribadi, `memory.db`, atau secret apa pun.
- Pesan dan isi file yang dibaca tool dikirim ke provider LLM yang kamu pilih, dan lewat server Telegram jika memakai mode Telegram.
- Perlakukan token bot dan secret TOTP seperti password. Jika bocor: `/revoke` di BotFather, dan buat secret TOTP baru.
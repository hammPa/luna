# Luna

Asisten AI pribadi berbasis C++17 yang berjalan di terminal Linux. Luna bisa ngobrol, mengingat hal tentangmu, menjalankan tool (file, aplikasi, URL, workspace dan jendela desktop), dan bisa dikendalikan dari HP lewat bot Telegram, dengan sistem izin berlapis supaya aksi berbahaya tidak jalan sembarangan.

**Versi saat ini: 0.6.0 (pra-rilis, dalam pengembangan)**

| Versi | Isi |
|---|---|
| 0.1 | Chat CLI, `HttpClient`, backend OpenAI-compatible, config |
| 0.2 | Memori SQLite (`messages`, `facts`) |
| 0.3 | Tool calling, permission gate, tool file dan aplikasi |
| 0.4 | Mode Telegram, persetujuan dari HP, TOTP untuk aksi berbahaya |
| 0.5 | `delete_file` (ke `.trash`), `switch_workspace` (Hyprland), `/clear`, base URL Telegram yang bisa diatur (reverse proxy) |
| 0.6 | Kontrol jendela Hyprland (`list_windows`, `focus_window`, `close_window`), `close_app` melaporkan hasil sebenarnya |

---

## Fitur

- **Chat teks** di terminal, dengan riwayat percakapan.
- **Backend AI agnostik**: semua provider yang kompatibel dengan OpenAI API (OpenAI, Mistral, Groq, DeepSeek, OpenRouter, atau router seperti 9router). Ganti model/provider cukup edit `config.json`, tanpa rebuild.
- **Memori jangka panjang** (SQLite): riwayat chat dan fakta permanen tentangmu.
- **Tool calling**: model memanggil tool yang kamu daftarkan.
- **Permission gate**: setiap tool punya tingkat risiko; yang berisiko ditanyakan dulu.
- **Kontrol desktop Hyprland**: pindah workspace, lihat jendela yang terbuka, pindah fokus, dan tutup satu jendela atau seluruh aplikasi.
- **Mode Telegram**: kendalikan Luna dari HP dengan allowlist user, persetujuan y/n, dan kode TOTP untuk aksi berbahaya. Mendukung reverse proxy.

---

## Prasyarat

Sistem: **Linux** (memakai `fork`, `execvp`, `xdg-open`, `pkill`, `localtime_r`).

| Kebutuhan | Keterangan |
|---|---|
| Compiler C++17 | `g++` 9+ atau `clang++` |
| CMake | 3.15 atau lebih baru |
| libcurl (dev) | `sudo apt install libcurl4-openssl-dev` |
| nlohmann/json | satu file header di `third_party/nlohmann/json.hpp` |
| SQLiteCpp | di `third_party/SQLiteCpp` |

Program pendukung (opsional; tool terkait hanya aktif jika programnya ada):

| Program | Untuk |
|---|---|
| `xdg-open` | `open_url` |
| `pkill`, `pgrep` (paket `procps`) | `close_app` |
| `hyprctl` | `switch_workspace`, `list_windows`, `focus_window`, `close_window` (hanya di dalam sesi Hyprland) |
| `oathtool` | TOTP (`sudo apt install oathtool`) |

---

## Build

```bash
git clone https://github.com/hammPa/luna
cd luna

# dependensi di third_party
mkdir -p third_party/nlohmann
# letakkan json.hpp dari rilis nlohmann/json di third_party/nlohmann/json.hpp
git clone https://github.com/SRombauts/SQLiteCpp.git third_party/SQLiteCpp

cp config.example.json config.json   # lalu sesuaikan

mkdir build && cd build
cmake ..
make
```

Binary: `build/luna`. Peringatan kompiler penuh aktif (`-Wall -Wextra -Wpedantic`); target kualitas: build tanpa warning.

---

## Konfigurasi

### Environment variable (rahasia, jangan di-commit)

| Variable | Fungsi |
|---|---|
| `NINE_ROUTER_API_KEY` | API key provider LLM (nama bisa diubah lewat `api_key_env`) |
| `LUNA_ASSISTANT_BOT_TOKEN` | Token bot dari @BotFather (nama bisa diubah lewat `telegram_token_env`) |
| `LUNA_TOTP_SECRET` | Secret TOTP base32 (opsional, untuk aksi berbahaya dari Telegram) |

Tambahkan ke `~/.zshrc` atau `~/.bashrc`, lalu `source`.

### `config.json`

```json
{
    "backend": "openai",
    "model": "mistral/ministral-3b-latest",
    "base_url": "https://hamm.up.railway.app/v1",
    "api_key_env": "NINE_ROUTER_API_KEY",
    "system_prompt": "Kamu Luna, asisten pribadi yang ramah dan to the point.",
    "history_limit": 20,
    "max_context": 4096,
    "timeout_sec": 30,
    "memory_db": "/home/USER/luna-data/memory.db",

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
| `memory_db` | Path file SQLite. Relatif terhadap folder tempat Luna dijalankan, jadi pakai path absolut agar konsisten |
| `allowed_dirs` | Folder yang boleh disentuh tool file. Entri pertama = workspace utama. **Kosong = tool file tidak aktif.** Pakai path lengkap (`~` tidak diperluas) |
| `permissions` | Kebijakan per tingkat risiko: `allow`, `ask`, `deny` |
| `apps` | Allowlist aplikasi untuk `open_app` dan `close_app`. `run` = program untuk membuka, `process` = nama proses untuk menutup (tanpa `process` = tidak bisa ditutup). Tanpa spasi/argumen |
| `telegram_allowed_users` | User ID numerik yang boleh memakai bot. **Kosong = mode Telegram menolak start** |
| `telegram_token_env` | Nama env var token bot |
| `telegram_api_base_url` | Base URL API Telegram. **Kosong = `https://api.telegram.org`.** Isi dengan URL reverse proxy bila jaringanmu tidak bisa menjangkau Telegram. Harus `https://` |

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
| `/clear` | Hapus riwayat percakapan (RAM dan database). **Fakta tidak ikut terhapus** |
| `/start`, `/help` | Bantuan singkat (Telegram) |
| `exit` / `quit` / Ctrl+D | Keluar (mode terminal) |

Fakta otomatis disisipkan ke system prompt di setiap giliran, jadi tidak terpotong oleh `history_limit`. `/clear` berguna kalau model mulai meniru jawaban lama yang sudah basi.

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
| `delete_file` | Hapus file dengan **memindahkannya ke `.trash/`** (bisa dipulihkan) | Destructive | `allowed_dirs` terisi |
| `open_url` | Buka URL http/https di browser default | Execute | selalu |
| `open_app` | Buka aplikasi dari allowlist `apps` | Execute | `apps` terisi |
| `close_app` | Tutup **semua jendela satu aplikasi** (SIGTERM), lalu memverifikasi prosesnya benar-benar hilang | Destructive | ada app dengan `process` |
| `switch_workspace` | Pindah workspace Hyprland (1 sampai 10) | Safe | sesi Hyprland dan `hyprctl` ada |
| `list_windows` | Daftar jendela terbuka. `scope`: `all`, `workspace` (yang sedang fokus), atau `active`. Menandai jendela yang fokus | Safe | sesi Hyprland dan `hyprctl` ada |
| `focus_window` | Pindah fokus: arah (`left`, `right`, `up`, `down`) atau alamat jendela dari `list_windows` | Safe | sesi Hyprland dan `hyprctl` ada |
| `close_window` | Tutup **satu jendela** (yang fokus atau lewat alamat), bukan seluruh aplikasi | Destructive | sesi Hyprland dan `hyprctl` ada |

Contoh: "buka youtube cari lofi", "buat file catatan.txt isinya ...", "ganti 'Halo' jadi 'Hai' di catatan.txt", "hapus file catatan.txt", "pindah ke workspace 3", "app apa saja yang terbuka", "fokus ke kanan", "tutup jendela ini", "tutup browser".

### `close_app` atau `close_window`?

| Kebutuhan | Tool |
|---|---|
| Tutup semua jendela satu aplikasi (mis. seluruh Firefox) | `close_app` |
| Tutup satu jendela saja, aplikasi lain tetap hidup | `close_window` |

`close_app` mengirim SIGTERM ke prosesnya, jadi aplikasi dengan banyak jendela atau tab (seperti Firefox) bisa menampilkan dialog konfirmasi dan belum benar-benar keluar. Luna melaporkan hal itu ("masih berjalan, mungkin menunggu konfirmasi") dan menyarankan `close_window`, yang menutup satu jendela dan umumnya tidak memicu dialog keluar.

Batasan:

- `open_url` hanya membuka halaman di browser default; Luna tidak membaca isinya.
- `delete_file` hanya untuk file biasa (bukan folder). Isi `.trash/` tidak bisa dihapus lewat Luna. Pulihkan dengan memindahkan file kembali (`mv`).
- `close_window` menampilkan kelas dan judul jendela target di prompt izin. Alamat jendela harus ada di daftar `hyprctl` saat itu; alamat sembarang ditolak.
- Dari Telegram, `close_window` **wajib memakai alamat** dari `list_windows` (tidak boleh "jendela yang sedang fokus"), karena jendela fokus ada di layar laptop yang tidak sedang kamu lihat.
- Tool desktop hanya bermakna di komputer yang punya desktop. Di server atau container tidak berguna.
- Tool jendela khusus Hyprland. Di desktop lain tool-nya tidak terdaftar.

---

## Sistem izin (permission)

Semua tool lewat satu gerbang di `ToolRegistry::run`; tool tidak bisa melewatinya.

| Risiko | Contoh | Perilaku default |
|---|---|---|
| Safe | `read_file`, `list_dir`, `switch_workspace`, `list_windows`, `focus_window` | Langsung jalan |
| Write | `create_file`, `patch_file` | Tanya; boleh "always" untuk sesi ini (hanya di terminal) |
| Destructive | menimpa file, `delete_file`, `close_app`, `close_window` | Selalu tanya, **tanpa "always"** |
| Execute | `open_url`, `open_app` | Selalu tanya, **tanpa "always"** |
| Privileged | (belum ada tool) | Selalu tanya, tanpa "always" |

Aturan keras: kebijakan `allow` untuk Destructive/Execute/Privileged dipaksa menjadi `ask`. Tanpa jawaban, EOF, atau error berarti **ditolak**.

Lapis keamanan lain:

- **PathGuard**: path dinormalisasi (`..`, symlink diselesaikan) dan harus di dalam `allowed_dirs`; di luar itu ditolak tanpa bertanya.
- **Tanpa shell**: program dijalankan lewat `fork` + `execvp` dengan argumen terpisah; tidak ada `system()`.
- **Allowlist aplikasi**: model hanya memilih nama dari daftar, tidak bisa memberi perintah atau nama proses bebas.
- **Alamat jendela divalidasi**: hanya format `0x` + heksadesimal, dan harus cocok dengan jendela yang benar-benar ada.
- **Penulisan atomik** (file sementara lalu `rename`), batas konten 256 KB.
- **Argumen tidak tepercaya**: semua argumen dari model divalidasi tipe dan rentangnya.

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

Luna melakukan long polling ke Telegram (`getUpdates`), jadi tidak butuh IP publik. Komputer harus menyala dan Luna harus berjalan.

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
| Destructive, Privileged | Butuh kode TOTP; tanpa `LUNA_TOTP_SECRET` otomatis ditolak |
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

---

## Deploy di cloud (catatan)

Luna bisa dijalankan di server (Docker, Railway, VPS) hanya sebagai bot chat. Hal yang perlu diperhatikan:

- Simpan token, API key, dan secret TOTP di **secrets/environment** layanan, jangan di Dockerfile atau `config.json`.
- Jangan menyajikan folder kerja lewat HTTP. Jika layanan butuh port untuk health check, sajikan folder kosong, karena `config.json`, binary, dan `memory.db` tidak boleh bisa diunduh.
- Penyimpanan gratis biasanya tidak permanen, jadi `memory.db` hilang saat restart.
- Tool desktop tidak berguna di server. Untuk mengontrol laptopmu dari HP, Luna harus berjalan di laptop itu.
- Sebagian layanan hosting tidak bisa menjangkau Telegram; gunakan `telegram_api_base_url`.

---

## Struktur proyek

```
luna/
├── CMakeLists.txt
├── config.example.json
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
2. Daftarkan di `main.cpp`: `registry->add(std::make_unique<NamaTool>(...))`. Untuk tool yang butuh program luar, daftarkan hanya jika programnya ada.

Daftar tool di system prompt dan schema untuk model dibuat otomatis dari registry. Gate permission otomatis berlaku. Argumen dari model tidak tepercaya: validasi dengan `arg_string`, dan pakai `PathGuard` untuk semua path.

## Menambah backend LLM

Buat class turunan `LLMBackend`, lalu tambahkan satu cabang di `backend_factory.cpp`. `main` tidak berubah.

---

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

---

## Keterbatasan saat ini

- Hanya Linux; tool jendela hanya untuk Hyprland.
- Backend lokal (Ollama/llama.cpp) belum ada; semua lewat API.
- Luna **reaktif**: hanya bertindak saat ada perintah. Belum ada tugas terjadwal, pengingat, atau pemicu otomatis, dan belum bisa menghubungimu duluan.
- Belum berjalan sebagai service latar belakang.
- `list_windows` mengirim judul jendela (termasuk judul tab browser) ke provider LLM. Pertimbangkan itu jika judulnya sensitif.
- Pesan perantara tool tidak disimpan ke database (hanya pesan user dan jawaban akhir).
- Selama menunggu persetujuan di Telegram, pesan lain tidak diproses.
- Indikator "mengetik" Telegram hanya bertahan sekitar 5 detik.
- Belum ada akses web (`search_web`, `fetch_url`); `HttpClient` baru mendukung POST.
- Belum ada tes otomatis.

## Roadmap

- [x] `delete_file` (ke `.trash/`), `/clear`
- [x] `switch_workspace` (Hyprland)
- [x] Base URL Telegram yang bisa diatur
- [x] Kontrol jendela: `list_windows`, `focus_window`, `close_window`
- [ ] `HttpClient::get`, lalu `search_web`, `fetch_url`, `weather`
- [ ] Tool tambahan: `calculator`, `save_fact`, info sistem, kontrol volume dan media, file lanjutan
- [ ] `open_url` dengan pilihan browser dan jendela baru
- [ ] Tes otomatis (target CMake) dan CI
- [ ] Service systemd, pengingat, dan tugas terjadwal
- [ ] Sudo dengan allowlist perintah
- [ ] Voice note Telegram dan input suara (STT/TTS di luar inti Luna)
- [ ] Backend lokal (Ollama, llama.cpp)

## Keamanan dan privasi

- Jangan commit `config.json` yang berisi ID pribadi, `memory.db`, atau secret apa pun. Commit `config.example.json` saja.
- Pesan, isi file yang dibaca tool, dan daftar jendela dikirim ke provider LLM yang kamu pilih, dan lewat server Telegram (atau proxy-mu) jika memakai mode Telegram.
- Perlakukan token bot dan secret TOTP seperti password. Jika bocor: `/revoke` di BotFather, dan buat secret TOTP baru.
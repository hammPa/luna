# Konfigurasi

## Konfigurasi

### Environment variable (rahasia, jangan di-commit)

| Variable | Fungsi |
|---|---|
| `NINE_ROUTER_API_KEY` | API key provider LLM (nama bisa diubah lewat `api_key_env`) |
| `LUNA_ASSISTANT_BOT_TOKEN` | Token bot dari @BotFather (nama bisa diubah lewat `telegram_token_env`) |
| `LUNA_TOTP_SECRET` | Secret TOTP base32 (opsional, untuk aksi berbahaya dari Telegram) |
| `TAVILY_API_KEY` | API key pencarian web (opsional; nama bisa diubah lewat `search.api_key_env`) |

Tambahkan ke `~/.zshrc` atau `~/.bashrc`, lalu `source`.

### `config.json`

```json
{
    "active": {"provider": "9router", "model": "groq/openai/gpt-oss-120b"},
    "providers": {
        "9router": {
            "backend": "openai",
            "base_url": "https://hamm.up.railway.app/v1",
            "api_key_env": "NINE_ROUTER_API_KEY",
            "timeout_sec": 30,
            "models": {
                "groq/openai/gpt-oss-120b": {"max_context": 131072, "history_limit": 20}
            }
        },
        "ollama": {
            "backend": "openai",
            "base_url": "http://localhost:11434/v1",
            "api_key_env": "LUNA_DUMMY_KEY",
            "timeout_sec": 180,
            "models": {
                "qwen2.5:1.5b": {"max_context": 4096, "history_limit": 8},
                "qwen3-coder:3b":   {"max_context": 8192, "history_limit": 8}
            }
        }
    },
    "system_prompt": "Kamu Luna, asisten pribadi yang ramah dan to the point.",
    "memory_db": "/home/USER/luna-data/memory.db",

    "allowed_dirs": ["/home/USER/luna-workspace"],

    "permissions": {
        "safe": "allow",
        "write": "ask",
        "destructive": "ask",
        "execute": "ask",
        "privileged": "ask",
        "exfiltrate": "ask",
    },

    "apps": {
        "browser":  {"run": "firefox", "process": "firefox"},
        "terminal": {"run": "gnome-terminal"},
        "files":    {"run": "nautilus", "process": "nautilus"}
    },

    "active_profile": "umum",
    "profiles": {
        "umum":    ["*"],
        "file":    ["get_time", "list_dir", "read_file", "create_file", "patch_file", "send_file"],
        "jendela": ["list_windows", "focus_window", "switch_workspace", "close_window"]
    },

    "telegram_allowed_users": [123456789],
    "telegram_token_env": "LUNA_ASSISTANT_BOT_TOKEN",
    "telegram_api_base_url": "",

    "max_send_mb": 20,

    "default_location": "Pekanbaru",
    "search": {
        "provider": "tavily",
        "api_key_env": "TAVILY_API_KEY",
        "base_url": "",
        "max_results": 5
    },

    "log_file": "/home/USER/luna-data/luna.log",
    "log_level": "info",
    "log_max_kb": 1024,
    "input_history_max": 100,
}
```

| Field | Arti |
|---|---|
| `active` | Pasangan `{"provider": "...", "model": "..."}` yang dipakai. Wajib. Harus terdaftar di `providers` |
| `providers` | Nama provider → `backend` (saat ini hanya `openai`), `base_url` (tanpa `/chat/completions`), `api_key_env` (**nama** env var, bukan key-nya), `timeout_sec` (batas waktu request HTTP), dan `models`. Semua wajib |
| `providers.X.models` | Nama model → `max_context` (batas konteks untuk indikator; samakan dengan konteks nyata model, belum dipakai memangkas riwayat) dan `history_limit` (jumlah pesan riwayat yang dikirim). Keduanya wajib. Nama model string bebas (boleh memuat `/` atau `:`), dikirim ke provider apa adanya |
| `system_prompt` | Aturan umum Luna. Daftar tool dan fakta ditambahkan otomatis |
| `memory_db` | Path file SQLite. Relatif terhadap folder tempat Luna dijalankan, jadi pakai path absolut agar konsisten |
| `allowed_dirs` | Folder yang boleh disentuh tool file. Entri pertama = workspace utama. **Kosong = tool file tidak aktif.** Pakai path lengkap (`~` tidak diperluas) |
| `permissions` | Kebijakan per tingkat risiko: `allow`, `ask`, `deny` |
| `apps` | Allowlist aplikasi untuk `open_app` dan `close_app`. `run` = program untuk membuka, `process` = nama proses untuk menutup (tanpa `process` = tidak bisa ditutup). Tanpa spasi/argumen |
| `active_profile` | Profil tool yang aktif saat start. Kosong = semua tool aktif. Harus ada di `profiles`, kalau tidak Luna gagal start |
| `profiles` | Nama profil → daftar nama tool yang aktif. `"*"` = semua tool. Nama yang tidak terdaftar (mis. tool Hyprland di luar Hyprland) dilewati dan dicatat WARN di log. Tanpa blok ini semua tool aktif |
| `telegram_allowed_users` | User ID numerik yang boleh memakai bot. **Kosong = mode Telegram menolak start** |
| `telegram_token_env` | Nama env var token bot |
| `telegram_api_base_url` | Base URL API Telegram. **Kosong = `https://api.telegram.org`.** Isi dengan URL reverse proxy bila jaringanmu tidak bisa menjangkau Telegram. Harus `https://` |
| `max_send_mb` | Batas total ukuran file per pengiriman lewat `send_file`, 1 sampai 50 (batas bot Telegram 50 MB). Default 20 |
| `default_location` | Kota default untuk `weather` jika model tidak menyebut kota. Kosong = kota wajib disebut |
| `search.provider` | Provider pencarian. Saat ini `tavily`. **Kosong atau blok `search` dihapus = `search_web` tidak aktif.** Nama yang tidak dikenal membuat program gagal start |
| `search.api_key_env` | **Nama** env var berisi API key pencarian. Jika env-nya kosong, `search_web` dimatikan (log `[search] ... dimatikan`) |
| `search.base_url` | Kosong = alamat bawaan provider. Jika diisi harus `https://` |
| `search.max_results` | Jumlah hasil pencarian, 1 sampai 10. Default 5 |
| `log_file` | Path file log (pakai path absolut). **Kosong = tidak ada file log**; WARN dan ERROR tetap tampil di stderr |
| `log_level` | `debug`, `info`, `warn`, atau `error`. Default `info`. Menentukan level minimum yang ditulis ke file |
| `log_max_kb` | Ukuran maksimum file log (64 sampai 102400, default 1024). Saat penuh, file diganti nama menjadi `<log_file>.1` (satu cadangan, cadangan lama ditimpa) |
| `input_history_max` | Jumlah pesan yang diingat untuk panah atas/bawah di TUI, 0 sampai 10000. Default 100, `0` = mati. Hanya di memori, hilang saat program ditutup |

Contoh provider:

| Provider | `base_url` |
|---|---|
| Mistral | `https://api.mistral.ai/v1` |
| OpenAI | `https://api.openai.com/v1` |
| Groq | `https://api.groq.com/openai/v1` |
| OpenRouter | `https://openrouter.ai/api/v1` |
| Ollama (lokal) | `http://localhost:11434/v1` |
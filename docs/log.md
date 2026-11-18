# Log

## Log

Jika `log_file` diisi, Luna menulis satu baris per kejadian:

```
2026-11-17 08:30:01 INFO  [app] start mode=tui backend=openai model=... history=12
2026-11-17 08:34:53 INFO  [llm] chat round=0 ms=812 tool_calls=1
2026-11-17 08:34:54 INFO  [tool] get_time selesai wait_ms=0 ms=0 hasil_chars=19
2026-11-17 08:35:10 WARN  [tool] open_url ditolak wait_ms=4210
```

Yang dicatat: mulai dan berhenti, daftar tool aktif, panggilan LLM (durasi, jumlah tool call), aksi tool (nama, selesai/gagal/ditolak, durasi, panjang hasil), dan aktivitas Telegram (ID pengirim dan panjang pesan, pengguna asing, polling gagal dan pulih).

Yang **tidak pernah** dicatat: isi pesan, argumen dan isi hasil tool, API key, token, dan kode TOTP.

Pesan dibersihkan sebelum ditulis (satu baris, tanpa karakter kontrol, maksimal 500 karakter), sehingga teks dari model atau pengguna tidak bisa memalsukan baris log. File dibuat dengan izin `0600`. Jika file log tidak bisa dibuka, Luna tetap jalan dan hanya mengeluarkan peringatan.

Memantau: `tail -f /path/ke/luna.log`
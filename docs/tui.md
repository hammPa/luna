# TUI

## TUI

Layar terdiri dari header (backend, model, jumlah tool), area chat yang otomatis mengikuti pesan terbaru, kotak input, dan bilah petunjuk.

### Kontrol

| Tombol | Fungsi |
|---|---|
| Enter | kirim pesan |
| `/` | buka menu perintah (↑/↓ pilih, Tab atau Enter melengkapi, Esc batal) |
| PageUp / PageDown | gulir riwayat chat |
| Roda mouse | gulir riwayat chat |
| ↑ / ↓ | telusuri pesan yang pernah dikirim (saat menu `/` tertutup) |
| Ctrl+C, `exit`, `/exit` | keluar |
| Esc (saat Luna bekerja) | batalkan giliran yang sedang berjalan |

Saat menggulir ke atas, balasan baru tidak menarik tampilan ke bawah; bilah bawah
menunjukkan jumlah baris di bawah. Mengirim pesan kembali ke bagian terbawah.

Saat sebuah tool butuh izin, muncul kotak di tengah layar:

| Tombol | Arti |
|---|---|
| `y` | Izinkan sekali |
| `a` | Selalu untuk sesi ini (hanya ditawarkan untuk risiko Write) |
| `n` atau Esc | Tolak |

Selama kotak izin terbuka, tombol lain tidak berfungsi. Menutup TUI saat izin menunggu berarti **ditolak**.

Hal yang perlu diketahui:
- Panggilan LLM berjalan di thread terpisah, jadi layar tidak membeku. Satu giliran diproses pada satu waktu; Enter diabaikan selagi Luna menjawab.
- Jawaban muncul sekaligus (belum ada streaming). Balasan Luna dirender dengan markdown: tebal, miring, kode inline, daftar, judul, kutipan, tautan, dan blok kode berlabel bahasa. Tabel dan format bersarang belum didukung.
- `Esc` saat Luna bekerja membatalkan giliran (jeda sekitar 1 detik). Tool yang sedang berjalan selesai dulu.
- Jika provider kena rate limit (429), Luna mencoba ulang otomatis (maks 2 kali, tunggu sampai 20 detik). Selama menunggu, baris `luna` di bawah pesan berubah dari "berpikir..." menjadi "kena rate limit, coba ulang 1/2 dalam N dtk...", dan `Esc` tetap membatalkan giliran. Jika server meminta tunggu lebih dari 20 detik, giliran gagal dengan pesan error.
- Setiap pemanggilan tool meninggalkan satu baris `info` di chat: `✓ list_dir · 1 ms` (berhasil), `✗ read_file · gagal`, atau `✗ create_file · ditolak` (izin ditolak atau diblokir kebijakan). Selama tool berjalan, baris `luna` menampilkan "menjalankan list_dir...". Durasi termasuk waktu menunggu persetujuan izin. Argumen tool (mis. path file) tidak ditampilkan.
- Selama TUI aktif, `stderr` dialihkan ke `/dev/null` agar peringatan tidak merusak layar. Isi `log_file` untuk tetap melihat WARN dan ERROR.
- Teks dari model dibersihkan dari karakter kontrol sebelum ditampilkan.
- Menutup TUI saat Luna masih menunggu LLM akan menunggu permintaan itu selesai (maksimal `timeout_sec`).

### Perintah di chat

| Perintah | Fungsi |
|---|---|
| `/remember kunci=nilai` | Simpan fakta permanen, mis. `/remember nama=Budi` |
| `/facts` | Tampilkan semua fakta |
| `/clear` | Hapus riwayat percakapan (RAM dan database). **Fakta tidak ikut terhapus** |
| `/start`, `/help` | Bantuan singkat (Telegram) |
| `/ctx` | tampilkan ukuran konteks yang terpakai |
| `/tools` | Daftar semua tool dan statusnya (`[on ]`/`[off]`) |
| `/tool on\|off nama` | Hidupkan atau matikan satu tool. Tool nonaktif tidak dikirim ke model. Hanya berlaku selama program jalan; saat restart semua tool kembali aktif |
| `/profile` | Daftar profil tool; yang aktif ditandai `*` |
| `/profile nama` | Ganti profil: hanya tool di profil itu yang aktif (menghapus perubahan `/tool` sebelumnya). Header menampilkan jumlah tool dan nama profil |
| `/model` | Daftar semua pasangan provider dan model (bernomor, urut abjad provider lalu model); yang aktif ditandai `*` |
| `/model nomor` | Pindah ke model itu tanpa restart. Riwayat tetap dipakai, `max_context` dan `history_limit` mengikuti model baru, angka konteks diulang dari awal. Jika gagal dibuat, model lama tetap aktif. Hanya berlaku selama program jalan |
| `/copy` | Salin balasan Luna terakhir ke clipboard (OSC 52) |
| `/copy code` | Salin hanya blok kode terakhir dari balasan Luna |
| `exit` / `quit` | Keluar (TUI dan CLI). Di CLI juga Ctrl+D; di TUI juga Ctrl+C |

Fakta otomatis disisipkan ke system prompt di setiap giliran, jadi tidak terpotong oleh `history_limit`. `/clear` berguna kalau model mulai meniru jawaban lama yang sudah basi.

## Indikator konteks

Angka menunjukkan ukuran konteks pada panggilan LLM **terakhir**: system prompt, daftar
tool, fakta, riwayat, dan hasil tool. Angka naik seiring percakapan, dan turun setelah
`/clear` atau saat riwayat lama terpangkas oleh `history_limit`. Ini bukan total token
yang ditagih provider.

    ctx 1.2k / 4096 ▰▰▱▱▱

| Warna | Terpakai |
|---|---|
| hijau | < 60% |
| kuning | 60-85% |
| merah | > 85% |

Angka berawalan `~` adalah perkiraan (sekitar 3 byte per token), dipakai bila provider
tidak mengirim `usage`.

Luna belum memangkas riwayat berdasarkan token. Pemangkasan masih lewat `history_limit`,
jadi hasil tool besar (mis. `read_file`) bisa menaikkan konteks tiba-tiba.


## Format balasan

Balasan Luna dirender sesuai markdown di semua antarmuka:

| Antarmuka | Tampilan |
|---|---|
| TUI | tebal, miring, kode inline berlatar, daftar, judul, kutipan, tautan, blok kode berlatar dengan label bahasa |
| CLI | escape ANSI di terminal; teks asli bila output di-pipe |
| Telegram | HTML (`<b>`, `<i>`, `<code>`, `<pre>`); bila Telegram menolak, dikirim sebagai teks polos |

Belum didukung: tabel, format bersarang (`**tebal `kode`**`), dan `_miring_`.
Pesan Telegram dipecah di batas blok sehingga blok kode tidak terbelah.
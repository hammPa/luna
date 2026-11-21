# Ollama (model lokal)

### Model lokal dengan Ollama

Ollama menyediakan endpoint OpenAI-compatible, jadi tidak butuh kode tambahan. Daftarkan sebagai provider:

```json
"active": {"provider": "ollama", "model": "qwen2.5:3b"},
"providers": {
    "ollama": {
        "backend": "openai",
        "base_url": "http://localhost:11434/v1",
        "api_key_env": "LUNA_DUMMY_KEY",
        "timeout_sec": 180,
        "models": {
            "qwen2.5:3b": {"max_context": 4096, "history_limit": 8}
        }
    }
}
```

```bash
ollama pull qwen2.5:3b
export LUNA_DUMMY_KEY=ollama     # nilainya bebas, tetapi env var harus terisi
./luna ../config.json
```

- `ollama ps` menunjukkan apakah model berjalan di GPU (kolom `PROCESSOR`).
- Model kecil lebih lambat dan kurang andal memanggil tool dibanding model cloud.
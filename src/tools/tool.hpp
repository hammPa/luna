#pragma once

#include <nlohmann/json.hpp>

#include <string>

// Seberapa berbahaya sebuah pemanggilan tool.
// Dipakai PermissionGate di Tahap B.
enum class Risk {
    Safe,         // hanya membaca / tanpa efek samping
    Write,        // membuat atau mengubah file
    Destructive,  // menghapus atau menimpa
    Execute,      // menjalankan program lain
    Privileged    // butuh sudo
};

class Tool {
public:
    virtual ~Tool() = default;

    // Nama unik yang dipakai model, mis. "get_time". Hanya huruf, angka, underscore.
    virtual std::string name() const = 0;

    // Dibaca MODEL untuk memutuskan kapan tool ini dipakai. Tulis jelas.
    virtual std::string description() const = 0;

    // JSON schema argumen: {"type":"object","properties":{...},"required":[...]}
    virtual nlohmann::json parameters() const = 0;

    // Risiko bergantung isi argumen (menulis ke file baru vs menimpa).
    virtual Risk risk(const nlohmann::json& args) const = 0;

    // Teks untuk prompt izin ke user. Dibuat dari args sebenarnya.
    virtual std::string describe(const nlohmann::json& args) const = 0;

    // Jalankan. Kembalikan teks hasil untuk dikirim balik ke model.
    // Boleh melempar exception; registry yang menangkapnya.
    virtual std::string execute(const nlohmann::json& args) = 0;
};
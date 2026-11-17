#pragma once
#include <string>
#include <vector>
#include <filesystem>

struct HttpResponse {
    long statusCode;
    std::string body;
    bool truncated = false;   // respons melebihi max_bytes (hanya diisi oleh get)

    bool ok() const {
        return statusCode >= 200 && statusCode < 300;
    }
};

struct MultipartField {
    std::string name;
    std::string value;
};

struct MultipartFile {
    std::string field;             // nama field, misal "document"
    std::filesystem::path path;    // dibaca langsung dari disk saat upload
    std::string filename;          // nama yang dilihat penerima
    std::string mime;              // kosong = libcurl menebak dari ekstensi
};

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    HttpResponse post(const std::string& url,
                      const std::vector<std::string>& headers,
                      const std::string& body,
                      long timeoutSec = 30);

    // Upload multipart/form-data. File dibaca dari disk saat dikirim (tidak dimuat ke RAM).
    // Jangan isi header Content-Type: libcurl menambahkan boundary sendiri.
    HttpResponse post_multipart(const std::string& url,
                                const std::vector<std::string>& headers,
                                const std::vector<MultipartField>& fields,
                                const MultipartFile& file,
                                long timeout_sec = 120);
    // GET khusus https, tanpa redirect, dengan batas ukuran respons.
    HttpResponse get(const std::string& url,
                     const std::vector<std::string>& headers,
                     long timeout_sec = 15,
                     std::size_t max_bytes = 256 * 1024) const;

    static std::string url_encode(const std::string& s);
};
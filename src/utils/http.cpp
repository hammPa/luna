#include <utils/http.hpp>

#include <curl/curl.h>

#include <memory>
#include <stdexcept>
#include <filesystem>

namespace {

    static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userData) {
        std::string* response = static_cast<std::string*>(userData);
        response->append(ptr, size * nmemb);
        return size * nmemb;
    }

    struct CurlDeleter {
        void operator()(CURL* curl) const {
            if (curl) {
                curl_easy_cleanup(curl);
            }
        }
    };

    struct SlistDeleter {
        void operator()(curl_slist* slist) const {
            if (slist) {
                curl_slist_free_all(slist);
            }
        }
    };

    struct MimeDeleter {
        void operator()(curl_mime* m) const { curl_mime_free(m); }
    };
    
    // tipe data dan custom deleter untuk smart pointer
    using CurlHandle = std::unique_ptr<CURL, CurlDeleter>;
    using SlistHandle = std::unique_ptr<curl_slist, SlistDeleter>;
    using MimeHandle = std::unique_ptr<curl_mime, MimeDeleter>;

    SlistHandle build_headers(const std::vector<std::string>& headers){
        curl_slist* raw = nullptr;
        for (const auto& h : headers) {
            curl_slist* tmp = curl_slist_append(raw, h.c_str());
            if (tmp == nullptr) {
                curl_slist_free_all(raw);  // jangan bocor
                throw std::runtime_error("Gagal alokasi header");
            }
            raw = tmp;
        }
        return SlistHandle(raw);
    }

    struct LimitedSink {
        std::string* out;
        std::size_t max;
        bool truncated;
    };

    static size_t limitedWriteCallback(char* ptr, size_t size, size_t nmemb, void* userData) {
        auto* s = static_cast<LimitedSink*>(userData);
        const size_t total = size * nmemb;
        if (s->out->size() + total > s->max) {
            s->truncated = true;
            return 0;   // hentikan unduhan
        }
        s->out->append(ptr, total);
        return total;
    }
} // namespace

HttpClient::HttpClient() { curl_global_init(CURL_GLOBAL_DEFAULT); }

HttpClient::~HttpClient() { curl_global_cleanup(); }


HttpResponse HttpClient::post(const std::string& url,
                              const std::vector<std::string>& headers,
                              const std::string& body,
                              long timeout_sec) {
    // Dibuat sebelum handle curl: destruktor C++ berjalan terbalik,
    // jadi handle curl dibersihkan lebih dulu, baru daftar header.
    SlistHandle slist = build_headers(headers);

    CurlHandle curl(curl_easy_init());
    if (!curl) {
        throw std::runtime_error("curl_easy_init gagal");
    }

    std::string response_body;
    CURL* c = curl.get();

    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, slist.get());
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, timeout_sec);

    CURLcode rc = curl_easy_perform(c);
    if (rc != CURLE_OK) {
        throw std::runtime_error(std::string("Request gagal: ") +
                                 curl_easy_strerror(rc));
    }

    HttpResponse res;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &res.statusCode);
    res.body = std::move(response_body);
    return res;
}

HttpResponse HttpClient::post_multipart(const std::string& url,
                                        const std::vector<std::string>& headers,
                                        const std::vector<MultipartField>& fields,
                                        const MultipartFile& file,
                                        long timeout_sec) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(file.path, ec)) {
        throw std::runtime_error("File tidak ditemukan untuk diunggah.");
    }

    SlistHandle slist = build_headers(headers);
    CurlHandle curl(curl_easy_init());
    if (!curl) throw std::runtime_error("curl_easy_init gagal");
    CURL* c = curl.get();

    MimeHandle mime(curl_mime_init(c));
    if (!mime) throw std::runtime_error("curl_mime_init gagal");

    for (const auto& f : fields) {
        curl_mimepart* part = curl_mime_addpart(mime.get());
        if (part == nullptr || curl_mime_name(part, f.name.c_str()) != CURLE_OK ||
            curl_mime_data(part, f.value.c_str(), CURL_ZERO_TERMINATED) != CURLE_OK) {
            throw std::runtime_error("Gagal menyusun field multipart.");
        }
    }

    const std::string path = file.path.string();
    curl_mimepart* part = curl_mime_addpart(mime.get());
    if (part == nullptr || curl_mime_name(part, file.field.c_str()) != CURLE_OK ||
        curl_mime_filedata(part, path.c_str()) != CURLE_OK ||
        curl_mime_filename(part, file.filename.c_str()) != CURLE_OK) {
        throw std::runtime_error("Gagal menyusun bagian file multipart.");
    }
    if (!file.mime.empty() && curl_mime_type(part, file.mime.c_str()) != CURLE_OK) {
        throw std::runtime_error("Gagal mengatur tipe file.");
    }

    std::string response_body;
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    if (slist) curl_easy_setopt(c, CURLOPT_HTTPHEADER, slist.get());
    curl_easy_setopt(c, CURLOPT_MIMEPOST, mime.get());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, timeout_sec);

    CURLcode rc = curl_easy_perform(c);
    if (rc != CURLE_OK) {
        // Jangan sertakan URL (berisi token bot).
        throw std::runtime_error(std::string("Upload gagal: ") + curl_easy_strerror(rc));
    }

    HttpResponse res;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &res.statusCode);
    res.body = std::move(response_body);
    return res;
}

HttpResponse HttpClient::get(const std::string& url,
                             const std::vector<std::string>& headers,
                             long timeout_sec,
                             std::size_t max_bytes) const {
    // Redirect tidak diikuti (default libcurl), jadi host tetap yang kita tentukan.
    if (url.rfind("https://", 0) != 0) {
        throw std::runtime_error("Hanya URL https yang diizinkan.");
    }

    SlistHandle slist = build_headers(headers);
    CurlHandle curl(curl_easy_init());
    if (!curl) throw std::runtime_error("curl_easy_init gagal");
    CURL* c = curl.get();

    HttpResponse res;
    LimitedSink sink{&res.body, max_bytes, false};

    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    if (slist) curl_easy_setopt(c, CURLOPT_HTTPHEADER, slist.get());
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(c, CURLOPT_USERAGENT, "luna/0.8");
    curl_easy_setopt(c, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, limitedWriteCallback);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &sink);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, timeout_sec);

    CURLcode rc = curl_easy_perform(c);
    // CURLE_WRITE_ERROR akibat kita sendiri menghentikan unduhan = bukan kegagalan.
    if (rc != CURLE_OK && !(rc == CURLE_WRITE_ERROR && sink.truncated)) {
        throw std::runtime_error(std::string("GET gagal: ") + curl_easy_strerror(rc));
    }

    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &res.statusCode);
    res.truncated = sink.truncated;
    return res;
}

std::string HttpClient::url_encode(const std::string& s) {
    CurlHandle curl(curl_easy_init());
    if (!curl) throw std::runtime_error("curl_easy_init gagal");
    char* e = curl_easy_escape(curl.get(), s.c_str(), static_cast<int>(s.size()));
    if (!e) throw std::runtime_error("url_encode gagal");
    std::string out(e);
    curl_free(e);
    return out;
}
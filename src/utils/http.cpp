#include <utils/http.hpp>

#include <curl/curl.h>

#include <memory>
#include <stdexcept>

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

    // tipe data dan custom deleter untuk smart pointer
    using CurlHandle = std::unique_ptr<CURL, CurlDeleter>;
    using SlistHandle = std::unique_ptr<curl_slist, SlistDeleter>;

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
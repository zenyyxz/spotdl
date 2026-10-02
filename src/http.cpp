#include "spotdl/http.hpp"
#include "spotdl/ui.hpp"

#include <curl/curl.h>
#include <iostream>
#include <mutex>

namespace spotdl {

    namespace {

        std::once_flag curl_init_flag;

        void ensure_curl_init() {
            std::call_once(curl_init_flag, [](){
                curl_global_init(CURL_GLOBAL_ALL);
            });
        }

        size_t write_string_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
            size_t total_size = size * nmemb;
            auto* str = static_cast<std::string*>(userdata);
            str->append(ptr, total_size);
            return total_size;
        }

        size_t write_bytes_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
            size_t total_size = size * nmemb;
            auto* vec = static_cast<std::vector<uint8_t>*>(userdata);
            vec->insert(vec->end(), reinterpret_cast<uint8_t*>(ptr), reinterpret_cast<uint8_t*>(ptr) + total_size);
            return total_size;
        }

    } // namespace

    HttpClient::HttpClient(bool force_ipv4, long timeout_seconds) : m_force_ipv4(force_ipv4), m_timeout_seconds(timeout_seconds) {
        ensure_curl_init();
    }

    HttpClient::~HttpClient() = default;

    HttpClient::HttpClient(HttpClient&&) noexcept = default;
    HttpClient& HttpClient::operator=(HttpClient&&) noexcept = default;

    void HttpClient::set_force_ipv4(bool force) {
        m_force_ipv4 = force;
    }

    void HttpClient::set_timeout(long timeout_seconds) {
        m_timeout_seconds = timeout_seconds;
    }

    std::optional<std::string> HttpClient::get(const std::string& url, const std::map<std::string, std::string>& headers) {
        ConsoleUI::print_debug("HTTP GET: " + url + (m_force_ipv4 ? " (IPv4 enforced)" : ""));
        CURL* curl = curl_easy_init();
        if (!curl) return std::nullopt;

        std::string response_data;
        struct curl_slist* chunk = nullptr;

        for (const auto& [k, v] : headers) {
            std::string header_line = k + ": " + v;
            chunk = curl_slist_append(chunk, header_line.c_str());
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_string_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, m_timeout_seconds);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");

        if (m_force_ipv4) {
            curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
        }

        if (chunk) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
        }

        CURLcode res = curl_easy_perform(curl);
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        if (chunk) {
            curl_slist_free_all(chunk);
        }
        curl_easy_cleanup(curl);

        ConsoleUI::print_debug("HTTP GET Response Code: " + std::to_string(http_code));

        if (res != CURLE_OK || http_code < 200 || http_code >= 300) {
            return std::nullopt;
        }

        return response_data;
    }

    std::optional<std::vector<uint8_t>> HttpClient::get_bytes(const std::string& url, const std::map<std::string, std::string>& headers) {
        ConsoleUI::print_debug("HTTP GET Bytes: " + url);
        CURL* curl = curl_easy_init();
        if (!curl) return std::nullopt;

        std::vector<uint8_t> response_data;
        struct curl_slist* chunk = nullptr;

        for (const auto& [k, v] : headers) {
            std::string header_line = k + ": " + v;
            chunk = curl_slist_append(chunk, header_line.c_str());
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_bytes_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, m_timeout_seconds);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");

        if (m_force_ipv4) {
            curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
        }

        if (chunk) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
        }

        CURLcode res = curl_easy_perform(curl);
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        if (chunk) {
            curl_slist_free_all(chunk);
        }

        curl_easy_cleanup(curl);

        ConsoleUI::print_debug("HTTP GET Bytes Response Code: " + std::to_string(http_code));

        if (res != CURLE_OK || http_code < 200 || http_code >= 300) {
            return std::nullopt;
        }

        return response_data;
    }

    std::optional<std::string> HttpClient::post(const std::string& url, const std::string& post_fields, const std::map<std::string, std::string>& headers) {
        ConsoleUI::print_debug("HTTP POST: " + url);
        CURL* curl = curl_easy_init();
        if (!curl) return std::nullopt;

        std::string response_data;
        struct curl_slist* chunk = nullptr;

        for (const auto& [k, v] : headers) {
            std::string header_line = k + ": " + v;
            chunk = curl_slist_append(chunk, header_line.c_str());
        }

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_string_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, m_timeout_seconds);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");

        if (m_force_ipv4) {
            curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
        }

        if (chunk) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
        }

        CURLcode res = curl_easy_perform(curl);
        long http_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        if (chunk) {
            curl_slist_free_all(chunk);
        }
        curl_easy_cleanup(curl);

        ConsoleUI::print_debug("HTTP POST Response Code: " + std::to_string(http_code));

        if (res != CURLE_OK || http_code < 200 || http_code >= 300) {
            return std::nullopt;
        }

        return response_data;
    }

    std::string HttpClient::url_encode(const std::string& input) {
        ensure_curl_init();
        CURL* curl = curl_easy_init();
        if (!curl) return input;

        char* output = curl_easy_escape(curl, input.c_str(), static_cast<int>(input.length()));
        if (!output) {
            curl_easy_cleanup(curl);
            return input;
        }

        std::string result(output);
        curl_free(output);
        curl_easy_cleanup(curl);
        return result;
    }

} // namespace spotdl
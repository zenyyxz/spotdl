#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <map>

namespace spotdl {

class HttpClient {
public:
    explicit HttpClient(bool force_ipv4 = false, long timeout_seconds = 15);
    ~HttpClient();

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    HttpClient(HttpClient&&) noexcept;
    HttpClient& operator=(HttpClient&&) noexcept;

    void set_force_ipv4(bool force);
    void set_timeout(long timeout_seconds);

    [[nodiscard]] std::optional<std::string> get(const std::string& url, const std::map<std::string, std::string>& headers = {});

    [[nodiscard]] std::optional<std::vector<uint8_t>> get_bytes(const std::string& url, const std::map<std::string, std::string>& headers = {});

    [[nodiscard]] std::optional<std::string> post(const std::string& url, const std::string& post_fields, const std::map<std::string, std::string>& headers = {});

    [[nodiscard]] static std::string url_encode(const std::string& input);

private:
    bool m_force_ipv4;
    long m_timeout_seconds;
};

} // namespace spotdl
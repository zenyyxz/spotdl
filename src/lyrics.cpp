#include "spotdl/lyrics.hpp"
#include "spotdl/http.hpp"

#include <nlohmann/json.hpp>
#include <iostream>

using nj = nlohmann::json;

namespace spotdl {

std::optional<std::string> LyricsFetcher::fetch_lyrics(const TrackMetaData& track, bool force_ipv4) {
    HttpClient http(force_ipv4, 10);

    // direct get attempt with track details
    int duration_sec = track.duration_ms / 1000;
    std::string get_url = "https://lrclib.net/api/get?"
                          "track_name=" + HttpClient::url_encode(track.title) + 
                          "&artist_name=" + HttpClient::url_encode(track.primary_artist());
    if (!track.album_name.empty()) {
        get_url += "&album_name=" + HttpClient::url_encode(track.album_name);
    }
    if (duration_sec > 0) {
        get_url += "&duration=" + std::to_string(duration_sec);
    }
    
    std::map<std::string, std::string> headers = {
        {"User-Agent", "spotdl-reborn/0.1.0"}
    };

    auto resp = http.get(get_url, headers);
    if (resp) {
        auto j = nj::parse(*resp, nullptr, false);
        if (!j.is_discarded()) {
            if (j.contains("syncedLyrics") && j["syncedLyrics"].is_string()) {
                std::string synced = j["syncedLyrics"].get<std::string>();
                if (!synced.empty()) return synced;
            }
            if (j.contains("plainLyrics") && j["plainLyrics"].is_string()) {
                std::string plain = j["plainLyrics"].get<std::string>();
                if (!plain.empty()) return plain;
            }
        }
    }

    // search fallback
    std::string search_query = track.primary_artist() + " " + track.title;
    std::string search_url = "https://lrclib.net/api/search?q=" + HttpClient::url_encode(search_query);

    auto search_resp = http.get(search_url, headers);
    if (search_resp) {
        auto j = nj::parse(*search_resp, nullptr, false);
        if (!j.is_discarded() && j.is_array() && !j.empty()) {
            for (const auto& item : j) {
                if (item.contains("syncedLyrics") && item["syncedLyrics"].is_string()) {
                    std::string synced = item["syncedLyrics"].get<std::string>();
                    if (!synced.empty()) return synced;
                }
            }
            for (const auto& item : j) {
                if (item.contains("plainLyrics") && item["plainLyrics"].is_string()) {
                    std::string plain = item["plainLyrics"].get<std::string>();
                    if (!plain.empty()) return plain;
                }
            }
        }
    }

    return std::nullopt;
}

} // namespace spotdl
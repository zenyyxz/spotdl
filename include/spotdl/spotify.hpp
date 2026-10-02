#pragma once

#include "spotdl/types.hpp"
#include "spotdl/http.hpp"

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace spotdl {

class SpotifyFetcher {
public:
    explicit SpotifyFetcher(std::string client_id = "", std::string client_secret = "", bool force_ipv4 = false);

    std::vector<TrackMetaData> fetch(const std::string& url_or_query);

    std::optional<TrackMetaData> get_track(const std::string& track_id);
    std::vector<TrackMetaData> get_playlist_tracks(const std::string& playlist_id);
    std::vector<TrackMetaData> get_album_tracks(const std::string& album_id);
    std::vector<TrackMetaData> get_artist_top_tracks(const std::string& artist_id);
    std::optional<TrackMetaData> search_track(const std::string& query);

    static std::tuple<std::string, std::string> parse_url(const std::string& url_or_query);
    static std::string upgrade_cover_url(std::string url);

private:
    std::string m_client_id;
    std::string m_client_secret;
    std::string m_access_token;
    HttpClient m_http;

    void ensure_access_token();
    std::optional<nlohmann::json> extract_next_data(const std::string& html);
    std::string extract_cover_url_from_json(const nlohmann::json& entity);
    TrackMetaData parse_track_json(const nlohmann::json& track_data, int track_num = 1, int total_tracks = 1);
};

} // namespace spotdl
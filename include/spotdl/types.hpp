#pragma once

#include <string>
#include <vector>
#include <sstream>

namespace spotdl {

struct TrackMetaData {
    std::string title;
    std::vector<std::string> artists;
    std::string album_name;
    std::string album_artist;
    std::string release_date;
    int track_number{1};
    int total_tracks{1};
    int disc_number{1};
    std::string cover_url;
    int duration_ms{0};
    bool explicit_content{false};
    std::string spotify_url;
    std::string isrc;
    std::string publisher;
    std::string copyright;
    std::vector<std::string> genres;

    [[nodiscard]] std::string primary_artist() const {
        return artists.empty() ? "Unknown Artist" : artists.front();
    }

    [[nodiscard]] std::string artist_str() const {
        if (artists.empty()) return "Unknown Artist";
        std::ostringstream oss;
        for (size_t i{0}; i < artists.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << artists[1];
        }
        return oss.str();
    }
};

struct DownloadOptions {
    std::vector<std::string> queries;
    std::string output_template = "{artist} - {title}.{ext}";
    std::string output_dir = ".";
    std::string audio_format = "mp3";
    std::string bitrate = "320k";
    int threads{4};
    int concurrent_fragments{4};
    int retries{3};
    double retry_delay{1.5};
    bool force_ipv4{false};
    std::string cookies_from_browser = "firefox";
    std::string cookie_file;
    bool embed_lyrics{false};
    std::string client_id;
    std::string client_secret;
    bool overwrite{false};
    bool verbose{false};
};

} // namespace spotdl
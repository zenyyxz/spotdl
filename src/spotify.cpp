#include "spotdl/spotify.hpp"
#include "spotdl/ui.hpp"

#include <iostream>
#include <regex>
#include <algorithm>

namespace spotdl {

SpotifyFetcher::SpotifyFetcher(std::string client_id, std::string client_secret, bool force_ipv4) : m_client_id(std::move(client_id)), m_client_secret(std::move(client_secret)), m_http(force_ipv4, 15) {}

std::tuple<std::string, std::string> SpotifyFetcher::parse_url(const std::string& url_or_query) {
    std::string trimmed = url_or_query;
    // trim whitespace
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

    static const std::regex uri_re(R"(spotify:(track|playlist|album|artist):([a-zA-Z0-9]+))");
    std::smatch match;
    if (std::regex_search(trimmed, match, uri_re)) {
        return {match[1].str(), match[2].str()};
    }

    static const std::regex url_re(R"(open\.spotify\.com/(?:intl-[a-z]+/)?(track|playlist|album|artist)/([a-zA-Z0-9]+))");
    if (std::regex_search(trimmed, match, url_re)) {
        return {match[1].str(), match[2].str()};
    }

    return {"query", trimmed};
}

std::string SpotifyFetcher::upgrade_cover_url(std::string url) {
    if (url.empty()) return url;
    
    // replace standard/small image hashes with 640x640 high-res hash
    size_t pos = 0;
    while ((pos = url.find("ab67616d00001e02", pos)) != std::string::npos) {
        url.replace(pos, 16, "ab67616d0000b273");
        pos += 16;
    }
    pos = 0;
    while ((pos = url.find("ab67616d00004851", pos)) != std::string::npos) {
        url.replace(pos, 16, "ab67616d0000b273");
        pos += 16;
    }
    return url;
}

std::optional<nlohmann::json> SpotifyFetcher::extract_next_data(const std::string& html) {
    static const std::regex script_re(R"(<script\s+id="__NEXT_DATA__"\s+type="application/json">([\s\S]*?)</script>)");
    std::smatch match;
    if (std::regex_search(html, match, script_re)) {
        std::string json_str = match[1].str();
        auto parsed = nlohmann::json::parse(json_str, nullptr, false);
        if (!parsed.is_discarded()) {
            return parsed;
        }
    }
    return std::nullopt;
}

std::string SpotifyFetcher::extract_cover_url_from_json(const nlohmann::json& entity) {
    if (!entity.is_object()) return "";

    if (entity.contains("visualIdentity") && entity["visualIdentity"].contains("image")) {
        const auto& images = entity["visualIdentity"]["image"];
        if (images.is_array() && !images.empty()) {
            std::string best_url;
            int max_w = 0;
            for (const auto& img : images) {
                int w = img.value("maxWidth", 0);
                if (w >= max_w && img.contains("url")) {
                    max_w = w;
                    best_url = img["url"].get<std::string>();
                }
            }
            if (!best_url.empty()) return upgrade_cover_url(best_url);
        }
    }

    if (entity.contains("coverArt") && entity["coverArt"].contains("sources")) {
        const auto& sources = entity["coverArt"]["sources"];
        if (sources.is_array() && !sources.empty() && sources[0].contains("url")) {
            return upgrade_cover_url(sources[0]["url"].get<std::string>());
        }
    }

    if (entity.contains("album") && entity["album"].contains("images")) {
        const auto& imgs = entity["album"]["images"];
        if (imgs.is_array() && !imgs.empty() && imgs[0].contains("url")) {
            return upgrade_cover_url(imgs[0]["url"].get<std::string>());
        }
    }

    return "";
}

void SpotifyFetcher::ensure_access_token() {
    if (!m_access_token.empty() || m_client_id.empty() || m_client_secret.empty()) {
        return;
    }

    ConsoleUI::print_debug("Requesting Spotify OAuth access token...");
    std::string auth_url = "https://accounts.spotify.com/api/token";
    std::string post_data = "grant_type=client_credentials&client_id=" +
                            HttpClient::url_encode(m_client_id) +
                            "&client_secret=" + HttpClient::url_encode(m_client_secret);
    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/x-www-form-urlencoded"}
    };

    auto resp = m_http.post(auth_url, post_data, headers);
    if (resp) {
        auto j = nlohmann::json::parse(*resp, nullptr, false);
        if (!j.is_discarded() && j.contains("access_token")) {
            m_access_token = j["access_token"].get<std::string>();
            ConsoleUI::print_debug("Successfully retrieved Spotify access token.");
        }
    }
}

std::vector<TrackMetaData> SpotifyFetcher::fetch(const std::string& url_or_query) {
    auto [item_type, item_id] = parse_url(url_or_query);
    ConsoleUI::print_debug("Parsing query: '" + url_or_query + "' -> type: " + item_type + ", id: " + item_id);

    if (item_type == "track") {
        auto tr = get_track(item_id);
        if (tr) return {*tr};
    } else if (item_type == "playlist") {
        return get_playlist_tracks(item_id);
    } else if (item_type == "album") {
        return get_album_tracks(item_id);
    } else if (item_type == "artist") {
        return get_artist_top_tracks(item_id);
    } else {
        auto tr = search_track(url_or_query);
        if (tr) return {*tr};
    }

    return {};
}

std::optional<TrackMetaData> SpotifyFetcher::get_track(const std::string& track_id) {
    ensure_access_token();
    if (!m_access_token.empty()) {
        std::string api_url = "https://api.spotify.com/v1/tracks/" + track_id;
        std::map<std::string, std::string> headers = {
            {"Authorization", "Bearer " + m_access_token}
        };
        auto resp = m_http.get(api_url, headers);
        if (resp) {
            auto j = nlohmann::json::parse(*resp, nullptr, false);
            if (!j.is_discarded() && j.contains("name")) {
                TrackMetaData t;
                t.title = j.value("name", "Unknown Track");
                if (j.contains("artists") && j["artists"].is_array()) {
                    for (const auto& a : j["artists"]) {
                        t.artists.push_back(a.value("name", ""));
                    }
                }
                if (j.contains("album") && j["album"].is_object()) {
                    t.album_name = j["album"].value("name", "Unknown Album");
                    t.release_date = j["album"].value("release_date", "");
                    t.total_tracks = j["album"].value("total_tracks", 1);
                    if (j["album"].contains("artists") && j["album"]["artists"].is_array() && !j["album"]["artists"].empty()) {
                        t.album_artist = j["album"]["artists"][0].value("name", t.primary_artist());
                    }
                    if (j["album"].contains("images") && j["album"]["images"].is_array() && !j["album"]["images"].empty()) {
                        t.cover_url = upgrade_cover_url(j["album"]["images"][0].value("url", ""));
                    }
                }
                t.track_number = j.value("track_number", 1);
                t.disc_number = j.value("disc_number", 1);
                t.duration_ms = j.value("duration_ms", 0);
                t.explicit_content = j.value("explicit", false);
                t.spotify_url = "https://open.spotify.com/track/" + track_id;
                if (j.contains("external_ids") && j["external_ids"].contains("isrc")) {
                    t.isrc = j["external_ids"]["isrc"].get<std::string>();
                }
                return t;
            }
        }
    }

    // embed scrape fallback (no API keys needed)
    std::string embed_url = "https://open.spotify.com/embed/track/" + track_id;
    auto html = m_http.get(embed_url);
    if (!html) return std::nullopt;

    auto data = extract_next_data(*html);
    if (!data) return std::nullopt;

    try {
        const auto& entity = (*data)["props"]["pageProps"]["state"]["data"]["entity"];
        TrackMetaData t;
        t.title = entity.contains("title") ? entity["title"].get<std::string>() : entity.value("name", "Unknown Track");
        
        if (entity.contains("artists") && entity["artists"].is_array()) {
            for (const auto& a : entity["artists"]) {
                if (a.contains("name")) t.artists.push_back(a["name"].get<std::string>());
            }
        }
        if (t.artists.empty()) t.artists.push_back("Unknown Artist");

        if (entity.contains("album") && entity["album"].contains("name")) {
            t.album_name = entity["album"]["name"].get<std::string>();
        } else {
            t.album_name = t.title;
        }
        t.album_artist = t.primary_artist();

        if (entity.contains("releaseDate")) {
            if (entity["releaseDate"].is_object() && entity["releaseDate"].contains("isoString")) {
                t.release_date = entity["releaseDate"]["isoString"].get<std::string>().substr(0, 10);
            }
        }

        t.cover_url = extract_cover_url_from_json(entity);
        t.track_number = 1;
        t.total_tracks = 1;
        t.disc_number = 1;
        t.duration_ms = entity.value("duration", 0);
        t.explicit_content = entity.value("isExplicit", false);
        t.spotify_url = "https://open.spotify.com/track/" + track_id;

        return t;
    } catch (const std::exception& e) {
        return std::nullopt;
    }
}

std::vector<TrackMetaData> SpotifyFetcher::get_playlist_tracks(const std::string& playlist_id) {
    ensure_access_token();
    if (!m_access_token.empty()) {
        std::vector<TrackMetaData> all_tracks;
        std::string next_url = "https://api.spotify.com/v1/playlists/" + playlist_id + "/tracks?limit=100";
        std::map<std::string, std::string> headers = {
            {"Authorization", "Bearer " + m_access_token}
        };

        while (!next_url.empty()) {
            auto resp = m_http.get(next_url, headers);
            if (!resp) break;
            auto j = nlohmann::json::parse(*resp, nullptr, false);
            if (j.is_discarded() || !j.contains("items")) break;

            for (const auto& item : j["items"]) {
                if (item.contains("track") && item["track"].is_object()) {
                    const auto& tr = item["track"];
                    TrackMetaData t;
                    t.title = tr.value("name", "Unknown Track");
                    if (tr.contains("artists") && tr["artists"].is_array()) {
                        for (const auto& a : tr["artists"]) {
                            t.artists.push_back(a.value("name", ""));
                        }
                    }
                    if (tr.contains("album") && tr["album"].is_object()) {
                        t.album_name = tr["album"].value("name", "Unknown Album");
                        t.release_date = tr["album"].value("release_date", "");
                        t.total_tracks = tr["album"].value("total_tracks", 1);
                        if (tr["album"].contains("artists") && tr["album"]["artists"].is_array() && !tr["album"]["artists"].empty()) {
                            t.album_artist = tr["album"]["artists"][0].value("name", t.primary_artist());
                        }
                        if (tr["album"].contains("images") && tr["album"]["images"].is_array() && !tr["album"]["images"].empty()) {
                            t.cover_url = upgrade_cover_url(tr["album"]["images"][0].value("url", ""));
                        }
                    }
                    t.track_number = tr.value("track_number", static_cast<int>(all_tracks.size() + 1));
                    t.disc_number = tr.value("disc_number", 1);
                    t.duration_ms = tr.value("duration_ms", 0);
                    t.explicit_content = tr.value("explicit", false);
                    if (tr.contains("id") && tr["id"].is_string()) {
                        t.spotify_url = "https://open.spotify.com/track/" + tr["id"].get<std::string>();
                    }
                    if (tr.contains("external_ids") && tr["external_ids"].contains("isrc")) {
                        t.isrc = tr["external_ids"]["isrc"].get<std::string>();
                    }
                    all_tracks.push_back(t);
                }
            }

            if (j.contains("next") && j["next"].is_string()) {
                next_url = j["next"].get<std::string>();
            } else {
                next_url.clear();
            }
        }
        if (!all_tracks.empty()) return all_tracks;
    }

    // scrape embed playlist
    std::string embed_url = "https://open.spotify.com/embed/playlist/" + playlist_id;
    auto html = m_http.get(embed_url);
    if (!html) return {};

    auto data = extract_next_data(*html);
    if (!data) return {};

    std::vector<TrackMetaData> tracks;
    try {
        const auto& entity = (*data)["props"]["pageProps"]["state"]["data"]["entity"];
        std::string playlist_name = entity.value("title", "Spotify Playlist");
        std::string cover_url = extract_cover_url_from_json(entity);
        const auto& track_list = entity.value("trackList", nlohmann::json::array());

        int total = static_cast<int>(track_list.size());
        for (int idx = 0; idx < total; ++idx) {
            const auto& item = track_list[idx];
            TrackMetaData t;
            t.title = item.value("title", "Unknown Track");
            std::string subtitle = item.value("subtitle", "");
            if (!subtitle.empty()) {
                t.artists = {subtitle};
            } else {
                t.artists = {"Various Artists"};
            }
            t.album_name = playlist_name;
            t.album_artist = t.primary_artist();
            t.track_number = idx + 1;
            t.total_tracks = total;
            t.disc_number = 1;
            t.cover_url = cover_url;
            t.duration_ms = item.value("duration", 0);
            t.explicit_content = item.value("isExplicit", false);
            std::string uri = item.value("uri", "");
            if (uri.rfind("spotify:track:", 0) == 0) {
                t.spotify_url = "https://open.spotify.com/track/" + uri.substr(14);
            }
            tracks.push_back(t);
        }
    } catch (...) {}

    return tracks;
}

/*
Hi fellow programmer. I'm Lahiru and nice to see you here. :)
Guess why I'm writing shit here? bcoz I have no peope with me to spend my time
and I'm too socially awkward to interact with others. So I had to
learn cxx at 16, I'm still socially awkward and alone, but guess what. I'm now 18
and I can code in cxx easier than finding people to spend my time with.

Maybe we can be friends??? can we? since we both have the same programming taste?
if not you aren't here right. so. what do you think? thanks for reading this tho.
*/

std::vector<TrackMetaData> SpotifyFetcher::get_album_tracks(const std::string& album_id) {
    ensure_access_token();
    if (!m_access_token.empty()) {
        std::string api_url = "https://api.spotify.com/v1/albums/" + album_id;
        std::map<std::string, std::string> headers = {
            {"Authorization", "Bearer " + m_access_token}
        };
        auto resp = m_http.get(api_url, headers);
        if (resp) {
            auto j = nlohmann::json::parse(*resp, nullptr, false);
            if (!j.is_discarded() && j.contains("tracks")) {
                std::string album_name = j.value("name", "Unknown Album");
                std::string release_date = j.value("release_date", "");
                std::string album_artist = "Unknown Artist";
                if (j.contains("artists") && j["artists"].is_array() && !j["artists"].empty()) {
                    album_artist = j["artists"][0].value("name", "Unknown Artist");
                }
                std::string cover_url;
                if (j.contains("images") && j["images"].is_array() && !j["images"].empty()) {
                    cover_url = upgrade_cover_url(j["images"][0].value("url", ""));
                }
                int total_tracks = j.value("total_tracks", 1);

                std::vector<TrackMetaData> tracks;
                for (const auto& tr : j["tracks"].value("items", nlohmann::json::array())) {
                    TrackMetaData t;
                    t.title = tr.value("name", "Unknown Track");
                    if (tr.contains("artists") && tr["artists"].is_array()) {
                        for (const auto& a : tr["artists"]) {
                            t.artists.push_back(a.value("name", ""));
                        }
                    }
                    t.album_name = album_name;
                    t.album_artist = album_artist;
                    t.release_date = release_date;
                    t.track_number = tr.value("track_number", 1);
                    t.total_tracks = total_tracks;
                    t.disc_number = tr.value("disc_number", 1);
                    t.duration_ms = tr.value("duration_ms", 0);
                    t.cover_url = cover_url;
                    t.explicit_content = tr.value("explicit", false);
                    if (tr.contains("id") && tr["id"].is_string()) {
                        t.spotify_url = "https://open.spotify.com/track/" + tr["id"].get<std::string>();
                    }
                    tracks.push_back(t);
                }
                return tracks;
            }
        }
    }

    // scrape embed album
    std::string embed_url = "https://open.spotify.com/embed/album/" + album_id;
    auto html = m_http.get(embed_url);
    if (!html) return {};

    auto data = extract_next_data(*html);
    if (!data) return {};

    std::vector<TrackMetaData> tracks;
    try {
        const auto& entity = (*data)["props"]["pageProps"]["state"]["data"]["entity"];
        std::string album_name = entity.value("title", "Unknown Album");
        std::string album_artist = entity.value("subtitle", "Unknown Artist");
        std::string cover_url = extract_cover_url_from_json(entity);
        const auto& track_list = entity.value("trackList", nlohmann::json::array());

        int total = static_cast<int>(track_list.size());
        for (int idx = 0; idx < total; ++idx) {
            const auto& item = track_list[idx];
            TrackMetaData t;
            t.title = item.value("title", "Unknown Track");
            std::string subtitle = item.value("subtitle", album_artist);
            t.artists = {subtitle};
            t.album_name = album_name;
            t.album_artist = album_artist;
            t.track_number = idx + 1;
            t.total_tracks = total;
            t.disc_number = 1;
            t.cover_url = cover_url;
            t.duration_ms = item.value("duration", 0);
            t.explicit_content = item.value("isExplicit", false);
            std::string uri = item.value("uri", "");
            if (uri.rfind("spotify:track:", 0) == 0) {
                t.spotify_url = "https://open.spotify.com/track/" + uri.substr(14);
            }
            tracks.push_back(t);
        }
    } catch (...) {}

    return tracks;
}

std::vector<TrackMetaData> SpotifyFetcher::get_artist_top_tracks(const std::string& artist_id) {
    ensure_access_token();
    if (m_access_token.empty()) return {};

    std::string api_url = "https://api.spotify.com/v1/artists/" + artist_id + "/top-tracks?market=US";
    std::map<std::string, std::string> headers = {
        {"Authorization", "Bearer " + m_access_token}
    };
    auto resp = m_http.get(api_url, headers);
    if (!resp) return {};

    auto j = nlohmann::json::parse(*resp, nullptr, false);
    if (j.is_discarded() || !j.contains("tracks")) return {};

    std::vector<TrackMetaData> tracks;
    for (const auto& tr : j["tracks"]) {
        TrackMetaData t;
        t.title = tr.value("name", "Unknown Track");
        if (tr.contains("artists") && tr["artists"].is_array()) {
            for (const auto& a : tr["artists"]) {
                t.artists.push_back(a.value("name", ""));
            }
        }
        if (tr.contains("album") && tr["album"].is_object()) {
            t.album_name = tr["album"].value("name", "Unknown Album");
            t.release_date = tr["album"].value("release_date", "");
            t.total_tracks = tr["album"].value("total_tracks", 1);
            if (tr["album"].contains("images") && tr["album"]["images"].is_array() && !tr["album"]["images"].empty()) {
                t.cover_url = upgrade_cover_url(tr["album"]["images"][0].value("url", ""));
            }
        }
        t.track_number = tr.value("track_number", 1);
        t.disc_number = tr.value("disc_number", 1);
        t.duration_ms = tr.value("duration_ms", 0);
        t.explicit_content = tr.value("explicit", false);
        if (tr.contains("id") && tr["id"].is_string()) {
            t.spotify_url = "https://open.spotify.com/track/" + tr["id"].get<std::string>();
        }
        tracks.push_back(t);
    }
    return tracks;
}

std::optional<TrackMetaData> SpotifyFetcher::search_track(const std::string& query) {
    ensure_access_token();
    if (m_access_token.empty()) {
        // construct a basic synthetic track metadata from query
        TrackMetaData t;
        t.title = query;
        t.artists = {"Unknown Artist"};
        t.album_name = "Single";
        t.album_artist = "Unknown Artist";
        t.track_number = 1;
        t.total_tracks = 1;
        t.disc_number = 1;
        return t;
    }

    std::string api_url = "https://api.spotify.com/v1/search?q=" + HttpClient::url_encode(query) + "&type=track&limit=1";
    std::map<std::string, std::string> headers = {
        {"Authorization", "Bearer " + m_access_token}
    };
    auto resp = m_http.get(api_url, headers);
    if (!resp) return std::nullopt;

    auto j = nlohmann::json::parse(*resp, nullptr, false);
    if (j.is_discarded() || !j.contains("tracks") || !j["tracks"].contains("items")) {
        return std::nullopt;
    }

    const auto& items = j["tracks"]["items"];
    if (items.empty()) return std::nullopt;

    const auto& tr = items[0];
    TrackMetaData t;
    t.title = tr.value("name", "Unknown Track");
    if (tr.contains("artists") && tr["artists"].is_array()) {
        for (const auto& a : tr["artists"]) {
            t.artists.push_back(a.value("name", ""));
        }
    }
    if (tr.contains("album") && tr["album"].is_object()) {
        t.album_name = tr["album"].value("name", "Unknown Album");
        t.release_date = tr["album"].value("release_date", "");
        t.total_tracks = tr["album"].value("total_tracks", 1);
        if (tr["album"].contains("images") && tr["album"]["images"].is_array() && !tr["album"]["images"].empty()) {
            t.cover_url = upgrade_cover_url(tr["album"]["images"][0].value("url", ""));
        }
    }
    t.track_number = tr.value("track_number", 1);
    t.disc_number = tr.value("disc_number", 1);
    t.duration_ms = tr.value("duration_ms", 0);
    t.explicit_content = tr.value("explicit", false);
    if (tr.contains("id") && tr["id"].is_string()) {
        t.spotify_url = "https://open.spotify.com/track/" + tr["id"].get<std::string>();
    }
    return t;
}

} // namespace spotdl

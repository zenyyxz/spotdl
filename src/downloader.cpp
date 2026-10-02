#include "spotdl/downloader.hpp"
#include "spotdl/http.hpp"
#include "spotdl/lyrics.hpp"
#include "spotdl/tagger.hpp"
#include "spotdl/ui.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <regex>
#include <chrono>
#include <thread>
#include <cstdio>

namespace spotdl {

namespace fs = std::filesystem;

TrackDownloader::TrackDownloader(const DownloadOptions& options) : m_options(options) {}

std::string TrackDownloader::sanitize_filename(const std::string& name) {
    static const std::regex invalid_chars(R"([\\/:*?"<>|])");
    std::string clean = std::regex_replace(name, invalid_chars, "");
    // trim whitespaces
    clean.erase(0, clean.find_first_not_of(" \t\n\r"));
    clean.erase(clean.find_last_not_of(" \t\n\r") + 1);
    return clean;
}

std::string TrackDownloader::build_file_path(const TrackMetaData& track, const std::string& output_dir) const {
    std::string result = m_options.output_template;

    std::ostringstream tr_oss;
    tr_oss << std::setw(2) << std::setfill('0') << track.track_number;

    auto replace_all = [](std::string& str, const std::string& from, const std::string& to) {
        if (from.empty()) return;
        size_t start_pos = 0;
        while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }
    };

    replace_all(result, "{artist}", sanitize_filename(track.primary_artist()));
    replace_all(result, "{artists}", sanitize_filename(track.artist_str()));
    replace_all(result, "{title}", sanitize_filename(track.title));
    replace_all(result, "{album}", sanitize_filename(track.album_name));
    replace_all(result, "{track_number}", tr_oss.str());
    replace_all(result, "{disc_number}", std::to_string(track.disc_number));
    replace_all(result, "{ext}", m_options.audio_format);

    fs::path full_path = fs::path(output_dir) / result;
    fs::create_directories(full_path.parent_path());
    return full_path.lexically_normal().string();
}

std::string TrackDownloader::find_youtube_url(const TrackMetaData& track, int attempt) const {
    std::string q = track.artist_str() + " - " + track.title;
    if (attempt == 2) q += " audio";
    else if (attempt >= 3) q = track.primary_artist() + " " + track.title + " official video";
    else q += " official audio";

    // fast HTTP lookup directly on yt search to extract videoId in ~200ms
    std::string search_url = "https://www.youtube.com/results?search_query=" + HttpClient::url_encode(q);
    ConsoleUI::print_debug("Fast YouTube search lookup via HTTP: " + search_url);

    HttpClient http(m_options.force_ipv4, 10);
    auto html = http.get(search_url);
    if (html) {
        static const std::regex video_id_re("\"videoId\":\"([a-zA-Z0-9_-]{11})\"");
        std::smatch match;
        if (std::regex_search(*html, match, video_id_re)) {
            std::string video_id = match[1].str();
            std::string direct_url = "https://www.youtube.com/watch?v=" + video_id;
            ConsoleUI::print_debug("Fast YouTube lookup resolved videoId: " + video_id + " (" + direct_url + ")");
            return direct_url;
        }
    }

    ConsoleUI::print_debug("Fast lookup fallback to ytsearch1: " + q);
    return "ytsearch1:" + q;
}

bool TrackDownloader::run_ytdlp(const std::string& yt_url, const std::string& temp_output_base, ProgressCallback progress_callback) {
    std::ostringstream cmd;
    cmd << "yt-dlp --newline ";
    cmd << "--no-playlist ";
    cmd << "--no-warnings ";
    cmd << "--format \"bestaudio/best\" ";
    cmd << "--extract-audio ";
    cmd << "--audio-format " << m_options.audio_format << " ";

    std::string quality = m_options.bitrate;
    if (!quality.empty() && (quality.back() == 'k' || quality.back() == 'K')) {
        quality.pop_back();
    }
    cmd << "--audio-quality " << quality << " ";
    cmd << "--output \"" << temp_output_base << ".%(ext)s\" ";
    cmd << "--socket-timeout 30 --retries 3 --fragment-retries 3 ";
    cmd << "--concurrent-fragments " << m_options.concurrent_fragments << " ";

    if (m_options.force_ipv4) {
        cmd << "--force-ipv4 ";
    }

    if (!m_options.cookies_from_browser.empty()) {
        cmd << "--cookies-from-browser " << m_options.cookies_from_browser << " ";
    } else if (!m_options.cookie_file.empty()) {
        cmd << "--cookies \"" << m_options.cookie_file << "\" ";
    }

    cmd << "\"" << yt_url << "\" 2>&1";

    std::string full_cmd = cmd.str();
    ConsoleUI::print_debug("Executing command: " + full_cmd);

    FILE* pipe = popen(full_cmd.c_str(), "r");
    if (!pipe) {
        ConsoleUI::print_error("Failed to execute yt-dlp process");
        return false;
    }

    char buffer[1024];
    static const std::regex pct_regex(R"(\b([0-9]{1,3}\.[0-9]+)%|\b([0-9]{1,3})%)");
    static const std::regex spd_regex(R"(at\s+([0-9.]+\s*[A-Za-z/]+))");

    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string line(buffer);
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
            line.pop_back();
        }

        if (line.empty()) continue;

        ConsoleUI::print_debug("[yt-dlp] " + line);

        if (progress_callback && line.find("[download]") != std::string::npos) {
            std::smatch pct_match, spd_match;
            if (std::regex_search(line, pct_match, pct_regex)) {
                try {
                    std::string pct_str = pct_match[1].matched ? pct_match[1].str() : pct_match[2].str();
                    double pct = std::stod(pct_str);
                    std::string spd = "";
                    if (std::regex_search(line, spd_match, spd_regex)) {
                        spd = spd_match[1].str();
                    }
                    progress_callback(pct, spd);
                } catch (...) {}
            }
        }
    }

    int ret = pclose(pipe);
    ConsoleUI::print_debug("yt-dlp process exited with code: " + std::to_string(ret));
    return (ret == 0);
}

std::optional<std::string> TrackDownloader::download_track(const TrackMetaData& track, const std::string& output_dir, ProgressCallback progress_callback) {
    std::string file_path = build_file_path(track, output_dir);

    if (!m_options.overwrite && fs::exists(file_path) && fs::file_size(file_path) > 10000) {
        ConsoleUI::print_debug("File already exists, skipping: " + file_path);
        if (progress_callback) {
            progress_callback(100.0, "Skipped (Exists)");
        }
        return file_path;
    }

    std::string temp_output_base = file_path;
    size_t last_dot = temp_output_base.rfind('.');
    if (last_dot != std::string::npos) {
        temp_output_base = temp_output_base.substr(0, last_dot);
    }
    temp_output_base += ".temp";
    std::string expected_temp_file = temp_output_base + "." + m_options.audio_format;

    for (int attempt = 1; attempt <= m_options.retries; ++attempt) {
        std::string yt_url = find_youtube_url(track, attempt);

        if (progress_callback) {
            progress_callback(0.0, "Starting...");
        }

        run_ytdlp(yt_url, temp_output_base, progress_callback);

        if (fs::exists(expected_temp_file)) {
            std::error_code ec;
            if (fs::exists(file_path)) {
                fs::remove(file_path, ec);
            }
            fs::rename(expected_temp_file, file_path, ec);

            std::optional<std::string> lyrics = std::nullopt;
            if (m_options.embed_lyrics) {
                if (progress_callback) {
                    progress_callback(95.0, "Fetching lyrics...");
                }
                ConsoleUI::print_debug("Fetching lyrics for: " + track.title);
                lyrics = LyricsFetcher::fetch_lyrics(track, m_options.force_ipv4);
            }

            if (progress_callback) {
                progress_callback(98.0, "Tagging metadata...");
            }

            ConsoleUI::print_debug("Applying TagLib metadata to: " + file_path);
            AudioTagger::apply_metadata(file_path, track, lyrics, m_options.force_ipv4);

            if (progress_callback) {
                progress_callback(100.0, "Done");
            }
            return file_path;
        }

        // cleanup temp file if half-written
        if (fs::exists(expected_temp_file)) {
            std::error_code ec;
            fs::remove(expected_temp_file, ec);
        }

        if (attempt < m_options.retries) {
            double wait_sec = m_options.retry_delay * attempt;
            ConsoleUI::print_warning("Attempt " + std::to_string(attempt) + " failed for '" + track.title + "'. Retrying in " + std::to_string(wait_sec) + "s...");
            if (progress_callback) {
                std::ostringstream oss;
                oss << "Retry " << attempt << "/" << m_options.retries;
                progress_callback(0.0, oss.str());
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(wait_sec * 1000)));
        } else {
            ConsoleUI::print_error("All " + std::to_string(m_options.retries) + " attempts failed for '" + track.title + "'");
            if (progress_callback) {
                progress_callback(0.0, "Failed");
            }
        }
    }
    
    return std::nullopt;
}

} // namespace spotdl
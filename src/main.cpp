#include "spotdl/types.hpp"
#include "spotdl/http.hpp"
#include "spotdl/spotify.hpp"
#include "spotdl/downloader.hpp"
#include "spotdl/thread_pool.hpp"
#include "spotdl/ui.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <atomic>
#include <cstdlib>

namespace fs = std::filesystem;
using namespace spotdl;

#ifndef SPOTDL_VERSION
#define SPOTDL_VERSION "0.1.0"
#endif

void print_help(const char* prog) {
    std::cout << "Usage: " << prog << " [OPTIONS] QUERIES...\n\n"
              << "SpotDL: Download Spotify playlists, albums, and tracks with full metadata (C++ Rewrite).\n\n"
              << "Arguments:\n"
              << "  QUERIES...                  Spotify playlist/album/track URLs, URIs, or search terms.\n\n"
              << "Options:\n"
              << "  -o, --output TEMPLATE       Output format template (default: '{artist} - {title}.{ext}') or dir.\n"
              << "  -f, --format FORMAT         Target audio format: mp3, m4a, flac, opus, wav (default: mp3).\n"
              << "  -b, --bitrate BITRATE       Target audio bitrate/quality e.g. 320k, 256k, 192k (default: 320k).\n"
              << "  -t, --threads NUM           Number of concurrent song download threads (default: 4).\n"
              << "      --concurrent-fragments N Number of parallel connections per song stream (default: 4).\n"
              << "  -r, --retries NUM           Number of download retries on network error per song (default: 3).\n"
              << "      --retry-delay SEC       Base delay in seconds between retries (default: 1.5).\n"
              << "      --force-ipv4            Strictly force IPv4 for all network requests (disabled by default).\n"
              << "      --cookies-from-browser B Browser name to extract YouTube cookies from e.g. firefox, chrome (default: firefox).\n"
              << "      --cookie-file PATH      Path to cookies.txt file for YouTube extraction.\n"
              << "      --embed-lyrics          Fetch and embed lyrics into audio file.\n"
              << "      --client-id ID          Optional Spotify Client ID.\n"
              << "      --client-secret SECRET  Optional Spotify Client Secret.\n"
              << "      --overwrite             Overwrite existing files.\n"
              << "  -v, --verbose               Enable verbose debug logging.\n"
              << "  -V, --version               Show version and exit.\n"
              << "  -h, --help                  Show this help message and exit.\n";
}

int main(int argc, char* argv[]) {
    DownloadOptions options;

    const char* env_client_id = std::getenv("SPOTIPY_CLIENT_ID");
    if (env_client_id) options.client_id = env_client_id;

    const char* env_client_secret = std::getenv("SPOTIPY_CLIENT_SECRET");
    if (env_client_secret) options.client_secret = env_client_secret;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            return 0;
        } else if (arg == "-V" || arg == "--version") {
            std::cout << "spotdl-reborn " << SPOTDL_VERSION << "\n";
            return 0;
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            options.output_template = argv[++i];
        } else if ((arg == "-f" || arg == "--format") && i + 1 < argc) {
            options.audio_format = argv[++i];
        } else if ((arg == "-b" || arg == "--bitrate") && i + 1 < argc) {
            options.bitrate = argv[++i];
        } else if ((arg == "-t" || arg == "--threads") && i + 1 < argc) {
            options.threads = std::max(1, std::stoi(argv[++i]));
        } else if (arg == "--concurrent-fragments" && i + 1 < argc) {
            options.concurrent_fragments = std::max(1, std::stoi(argv[++i]));
        } else if ((arg == "-r" || arg == "--retries") && i + 1 < argc) {
            options.retries = std::max(1, std::stoi(argv[++i]));
        } else if (arg == "--retry-delay" && i + 1 < argc) {
            options.retry_delay = std::max(0.5, std::stod(argv[++i]));
        } else if (arg == "--force-ipv4") {
            options.force_ipv4 = true;
        } else if (arg == "--cookies-from-browser" && i + 1 < argc) {
            options.cookies_from_browser = argv[++i];
        } else if (arg == "--cookie-file" && i + 1 < argc) {
            options.cookie_file = argv[++i];
        } else if (arg == "--embed-lyrics") {
            options.embed_lyrics = true;
        } else if (arg == "--client-id" && i + 1 < argc) {
            options.client_id = argv[++i];
        } else if (arg == "--client-secret" && i + 1 < argc) {
            options.client_secret = argv[++i];
        } else if (arg == "--overwrite") {
            options.overwrite = true;
        } else if (arg == "-v" || arg == "--verbose") {
            options.verbose = true;
        } else if (!arg.empty() && arg[0] != '-') {
            options.queries.push_back(arg);
        }
    }

    if (options.queries.empty()) {
        print_help(argv[0]);
        return 1;
    }

    ConsoleUI::set_verbose(options.verbose);
    ConsoleUI::print_banner(SPOTDL_VERSION, options);

    // normalize output dir and template
    if (fs::is_directory(options.output_template)) {
        options.output_dir = options.output_template;
        options.output_template = "{artist} - {title}.{ext}";
    } else if (options.output_template.find('/') != std::string::npos || options.output_template.find('\\') != std::string::npos) {
        fs::path p(options.output_template);
        if (p.has_parent_path()) {
            options.output_dir = p.parent_path().string();
            options.output_template = p.filename().string();
        }
    }

    // 1. fetch Spotify Metadata
    ConsoleUI::print_status("Fetching Spotify metadata (with full pagination)...");
    SpotifyFetcher fetcher(options.client_id, options.client_secret, options.force_ipv4);
    std::vector<TrackMetaData> all_tracks;

    for (const auto& q : options.queries) {
        auto fetched = fetcher.fetch(q);
        all_tracks.insert(all_tracks.end(), fetched.begin(), fetched.end());
    }

    if (all_tracks.empty()) {
        ConsoleUI::print_error("No tracks found for the provided inputs.");
        return 1;
    }

    ConsoleUI::print_info("Found " + std::to_string(all_tracks.size()) + " track(s) to process.\n");

    // 2. Download Tracks
    TrackDownloader downloader(options);
    std::vector<std::string> downloaded_files;
    std::vector<TrackMetaData> failed_tracks;

    std::atomic<int> completed_count(0);
    int total_tracks = static_cast<int>(all_tracks.size());

    auto download_worker = [&](const TrackMetaData& track) -> std::pair<TrackMetaData, std::optional<std::string>> {
        auto res = downloader.download_track(
            track,
            options.output_dir,
            [&](double pct, const std::string& speed) {
                ConsoleUI::print_progress(completed_count.load(), total_tracks, track.title, pct, speed);
            }
        );
        completed_count.fetch_add(1);
        ConsoleUI::print_progress(completed_count.load(), total_tracks, track.title, 100.0, "Done");
        return {track, res};
    };

    ConsoleUI::print_progress(0, total_tracks, "Starting downloads...");

    {
        ThreadPool pool(options.threads);
        std::vector<std::future<std::pair<TrackMetaData, std::optional<std::string>>>> futures;

        for (const auto& track : all_tracks) {
            futures.push_back(pool.enqueue(download_worker, track));
        }

        for (auto& f : futures) {
            auto [track, result] = f.get();
            if (result) {
                downloaded_files.push_back(*result);
            } else {
                failed_tracks.push_back(track);
            }
        }
    }

    // automated post download retry passes for failed tracks
    int max_post_retries = 2;
    int pass_num = 1;

    while (!failed_tracks.empty() && pass_num <= max_post_retries) {
        auto retry_targets = failed_tracks;
        failed_tracks.clear();

        ConsoleUI::print_warning("Retrying " + std::to_string(retry_targets.size()) +
                                " failed track(s)... (Post-Download Retry Pass " +
                                std::to_string(pass_num) + "/" + std::to_string(max_post_retries) + ")");

        completed_count = 0;
        int retry_total = static_cast<int>(retry_targets.size());
        ConsoleUI::print_progress(0, retry_total, "Starting retries...");

        {
            ThreadPool pool(options.threads);
            std::vector<std::future<std::pair<TrackMetaData, std::optional<std::string>>>> futures;

            for (const auto& track : retry_targets) {
                futures.push_back(pool.enqueue([&](const TrackMetaData& t) {
                    auto res = downloader.download_track(
                        t,
                        options.output_dir,
                        [&](double pct, const std::string& speed) {
                            ConsoleUI::print_progress(completed_count.load(), retry_total, t.title, pct, speed);
                        }
                    );
                    completed_count.fetch_add(1);
                    ConsoleUI::print_progress(completed_count.load(), retry_total, t.title, 100.0, "Done");
                    return std::make_pair(t, res);
                }, track));
            }

            for (auto& f : futures) {
                auto [track, result] = f.get();
                if (result) {
                    downloaded_files.push_back(*result);
                } else {
                    failed_tracks.push_back(track);
                }
            }
        }

        pass_num++;
    }

    // 3. Print Summary Table
    ConsoleUI::print_summary_table(all_tracks, failed_tracks);

    return failed_tracks.empty() ? 0 : 1;
}

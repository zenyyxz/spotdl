#include "spotdl/ui.hpp"

#include <iostream>
#include <cmath>
#include <iomanip>

namespace spotdl {

    std::mutex ConsoleUI::s_console_mutex;
    bool ConsoleUI::s_verbose = false;

    namespace {

        // ANSI escape codes
        constexpr const char* RESET = "\033[0m";
        constexpr const char* BOLD = "\033[1m";
        constexpr const char* DIM = "\033[2m";
        constexpr const char* RED = "\033[31m";
        constexpr const char* GREEN = "\033[32m";
        constexpr const char* YELLOW = "\033[33m";
        constexpr const char* BLUE = "\033[34m";
        constexpr const char* MAGENTA = "\033[35m";
        constexpr const char* CYAN = "\033[36m";

    } // namespace

    void ConsoleUI::set_verbose(bool verbose) {
        s_verbose = verbose;
    }

    bool ConsoleUI::is_verbose() {
        return s_verbose;
    }

    void ConsoleUI::print_banner(const std::string& version, const DownloadOptions& options) {
        std::lock_guard<std::mutex> lock(s_console_mutex);
        std::cout << BOLD << GREEN << "spotdl-reborn v" << version << RESET
                  << " - Spotify Music Downloader (C++ Rewrite)\n"
                  << DIM
                  << "Cookies browser: " << options.cookies_from_browser
                  << " | Format: " << options.audio_format
                  << " | Bitrate: " << options.bitrate
                  << " | Threads: " << options.threads
                  << " | Fragments: " << options.concurrent_fragments
                  << " | IPv4 Enforced: " << (options.force_ipv4 ? "Yes" : "No (Default)")
                  << " | Verbose: " << (s_verbose ? "Yes" : "No")
                  << RESET << "\n\n";
    }

    void ConsoleUI::print_status(const std::string& message) {
        std::lock_guard<std::mutex> lock(s_console_mutex);
        std::cout << CYAN << ":: " << RESET << message << "\n";
    }

    void ConsoleUI::print_info(const std::string& message) {
        std::lock_guard<std::mutex> lock(s_console_mutex);
        std::cout << BOLD << CYAN << "[INFO] " << RESET << message << "\n";
    }

    void ConsoleUI::print_success(const std::string& message) {
        std::lock_guard<std::mutex> lock(s_console_mutex);
        std::cout << BOLD << GREEN << "[SUCCESS] " << RESET << message << "\n";
    }

    void ConsoleUI::print_warning(const std::string& message) {
        std::lock_guard<std::mutex> lock(s_console_mutex);
        std::cout << BOLD << YELLOW << "[WARNING] " << RESET << message << "\n";
    }

    void ConsoleUI::print_error(const std::string& message) {
        std::lock_guard<std::mutex> lock(s_console_mutex);
        std::cout << BOLD << RED << "[ERROR] " << RESET << message << "\n";
    }

    void ConsoleUI::print_debug(const std::string& message) {
        if (!s_verbose) return;
        std::lock_guard<std::mutex> lock(s_console_mutex);
        std::cout << DIM << MAGENTA << "[DEBUG] " << RESET << DIM << message << RESET << "\n";
    }

    void ConsoleUI::print_progress(int completed, int total, const std::string& current_track, double track_pct, const std::string& track_speed) {
        // in verbose mode, don't overwrite terminal line with \r so debug logs remain visible
        if (s_verbose) return;

        std::lock_guard<std::mutex> lock(s_console_mutex);
        int bar_width = 25;
        float overall_progress = total > 0 ? static_cast<float>(completed) / total : 0.0f;
        int pos = static_cast<int>(bar_width * overall_progress);

        std::cout << "\r" << BOLD << "[TOTAL] " << RESET << "[";
        for (int i{0}; i < bar_width; ++i) {
            if (i < pos) std::cout << GREEN << "=" << RESET;
            else if (i == pos) std::cout << GREEN << ">" << RESET;
            else std::cout << " ";
        }
        std::cout << "] " << BOLD << static_cast<int>(overall_progress * 100.0) << "%" << RESET << " (" << completed << "/" << total << ")";
        
        if (!current_track.empty()) {
            std::string name = current_track.length() > 25 ? current_track.substr(0, 22) + "..." : current_track;
            std::cout << " - " << BOLD << CYAN << name << RESET;
            if (track_pct > 0.0) {
                std::cout << " [" << BOLD << YELLOW << std::fixed << std::setprecision(1) << track_pct << "%" << RESET;
                if (!track_speed.empty()) {
                    std::cout << " @ " << track_speed; 
                }
                std::cout << "]";
            }
        }
        std::cout << "                                           " << std::flush;

        if (completed == total && total > 0) {
            std::cout << "\n";
        }
    }

    void ConsoleUI::print_summary_table(const std::vector<TrackMetaData>& all_tracks, const std::vector<TrackMetaData>& failed_tracks) {
        std::lock_guard<std::mutex> lock(s_console_mutex);

        std::cout << "\n" << BOLD << MAGENTA << "━━━ Download Summary ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << RESET << "\n";
        std::cout << BOLD << std::left
                  << std::setw(12) << "Status"
                  << std::setw(35) << "Track"
                  << std::setw(30) << "Artist"
                  << RESET << "\n";
        std::cout << "────────────────────────────────────────────────────────────────────────\n";

        for (const auto& track : all_tracks) {
            bool failed = false;
            for (const auto& ft : failed_tracks) {
                if (ft.title == track.title && ft.artist_str() == track.artist_str()) {
                    failed = true;
                    break;
                }
            }

            std::string track_name = track.title.length() > 32 ? track.title.substr(0, 29) + "..." : track.title;
            std::string artist_name = track.artist_str().length() > 28 ? track.artist_str().substr(0, 25) + "..." : track.artist_str();

            if (failed) {
                std::cout << BOLD << RED << std::left << std::setw(12) << "FAILED" << RESET;
            } else {
                std::cout << BOLD << GREEN << std::left << std::setw(12) << "SUCCESS" << RESET;
            }

            std::cout << CYAN << std::left << std::setw(35) << track_name << RESET << GREEN << std::left << std::setw(30) << artist_name << RESET << "\n";
        }

        std::cout << "────────────────────────────────────────────────────────────────────────\n";

        size_t successful = all_tracks.size() - failed_tracks.size();
        std::cout << BOLD << GREEN << "Finished! " << RESET << "Successfully processed " << successful << " / " << all_tracks.size() << " track(s).\n\n";
    }

} // namespace spotdl

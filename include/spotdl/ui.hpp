#pragma once

#include "spotdl/types.hpp"
#include <string>
#include <vector>
#include <mutex>

namespace spotdl {

class ConsoleUI {
public:
    static void set_verbose(bool verbose);
    static bool is_verbose();

    static void print_banner(const std::string& version, const DownloadOptions& options);

    static void print_status(const std::string& message);
    static void print_info(const std::string& message);
    static void print_success(const std::string& message);
    static void print_warning(const std::string& message);
    static void print_error(const std::string& message);
    static void print_debug(const std::string& message);

    static void print_progress(int completed, int total, const std::string& current_track = "", double track_pct = 0.0, const std::string& track_speed = "");

    static void print_summary_table(const std::vector<TrackMetaData>& all_tracks, const std::vector<TrackMetaData>& failed_tracks);

private:
    static std::mutex s_console_mutex;
    static bool s_verbose;
};

} // namespace spotdl
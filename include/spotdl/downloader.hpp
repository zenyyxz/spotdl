#pragma once

#include "spotdl/types.hpp"
#include <string>
#include <optional>
#include <functional>

namespace spotdl {

using ProgressCallback = std::function<void(double percent, const std::string& speed)>;

class TrackDownloader {
public:
    explicit TrackDownloader(const DownloadOptions& options);

    [[nodiscard]] std::string build_file_path(const TrackMetaData& track, const std::string& output_dir = ".") const;

    [[nodiscard]] static std::string sanitize_filename(const std::string& name);

    [[nodiscard]] std::string find_youtube_url(const TrackMetaData& track, int attempt = 1) const;

    [[nodiscard]] std::optional<std::string> download_track(const TrackMetaData& track, const std::string& output_dir = ".", ProgressCallback progress_callback = nullptr);

private:
    DownloadOptions m_options;

    bool run_ytdlp(const std::string& yt_url, const std::string& temp_output_base, ProgressCallback progress_callback);
};

} // namespace spotdl
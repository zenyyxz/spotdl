#pragma once

#include "spotdl/types.hpp"
#include <string>
#include <optional>
#include <vector>
#include <cstdint>

namespace spotdl {

class AudioTagger {
public:
    static bool apply_metadata(const std::string& file_path, const TrackMetaData& track, const std::optional<std::string>& lyrics = std::nullopt, bool force_ipv4 = false);

    static std::optional<std::vector<uint8_t>> fetch_image_data(const std::string& cover_url, bool force_ipv4 = false);

private:
    static bool tag_id3(const std::string& file_path, const TrackMetaData& track, const std::optional<std::vector<uint8_t>>& image_data, const std::optional<std::string>& lyrics);

    static bool tag_mp4(const std::string& file_path, const TrackMetaData& track, const std::optional<std::vector<uint8_t>>& image_data, const std::optional<std::string>& lyrics);

    static bool tag_flac(const std::string& file_path, const TrackMetaData& track, const std::optional<std::vector<uint8_t>>& image_data, const std::optional<std::string>& lyrics);
};

} // namspace spotdl
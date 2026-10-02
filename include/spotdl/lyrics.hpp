#pragma once

#include "spotdl/types.hpp"
#include <string>
#include <optional>

namespace spotdl {

class LyricsFetcher {
public:
    static std::optional<std::string> fetch_lyrics(const TrackMetaData& track, bool force_ipv4 = false);
};

} // namespace spotdl
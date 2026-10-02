# Developer Guide & Project Architecture

This document provides a comprehensive overview of the `spotdl-reborn` C++ rewrite (`cxx_rewrite/`) for developers, maintainers, and contributors.

---

## 1. Directory & File Structure

```
cxx_rewrite/
├── CMakeLists.txt                 # Master CMake build configuration
├── README.md                      # End-user installation & usage guide
├── DEVELOPMENT.md                 # Developer documentation & architecture reference
├── include/
│   ├── nlohmann/
│   │   └── json.hpp               # Single-header JSON parser library (nlohmann::json v3.11.3)
│   └── spotdl/
│       ├── types.hpp              # Data structures (TrackMetadata, DownloadOptions)
│       ├── http.hpp               # libcurl wrapper class declaration (HttpClient)
│       ├── spotify.hpp            # Spotify URL parser & embed metadata fetcher
│       ├── lyrics.hpp             # LRCLIB API lyrics fetcher
│       ├── tagger.hpp             # TagLib audio metadata tagger for MP3, MP4/M4A, FLAC
│       ├── downloader.hpp         # YouTube fast lookup & yt-dlp downloader manager
│       ├── thread_pool.hpp        # C++20 multi-threaded worker pool
│       └── ui.hpp                 # ANSI terminal progress bars & summary tables
├── src/
│   ├── http.cpp                   # HttpClient implementation (curl_easy, IPv4 enforcement)
│   ├── spotify.cpp                # SpotifyFetcher implementation (embed HTML scraping)
│   ├── lyrics.cpp                 # LyricsFetcher implementation (LRCLIB REST API)
│   ├── tagger.cpp                 # AudioTagger implementation (TagLib ID3v2, MP4, FLAC)
│   ├── downloader.cpp             # TrackDownloader implementation (fast lookup, yt-dlp pipe)
│   ├── ui.cpp                     # ConsoleUI implementation (rendering ANSI UI & table)
│   └── main.cpp                   # CLI application entry point & option parser
└── tests/
    └── test_main.cpp              # C++ unit test suite
```

---

## 2. File-by-File Breakdown & Responsibilities

### Build System & Configuration
- **[CMakeLists.txt](CMakeLists.txt)**
  - Defines project metadata (`spotdl-reborn` v0.1.0, C++20).
  - Finds system packages: `Threads`, `CURL::libcurl`, `PkgConfig` (`taglib`).
  - Configures `spotdl_core` static library, `spotdl` CLI executable, and `spotdl_tests` test binary.

---

### Header Declarations (`include/spotdl/`)

- **[include/spotdl/types.hpp](include/spotdl/types.hpp)**
  - `TrackMetadata`: Core data structure containing track properties (`title`, `artists`, `album_name`, `album_artist`, `release_date`, `track_number`, `total_tracks`, `disc_number`, `cover_url`, `duration_ms`, `explicit_content`, `spotify_url`, `isrc`, `publisher`, `genres`). Includes helper methods `primary_artist()` and `artist_str()`.
  - `DownloadOptions`: Struct holding user CLI settings (`queries`, `output_template`, `output_dir`, `audio_format`, `bitrate`, `threads`, `concurrent_fragments`, `retries`, `retry_delay`, `force_ipv4`, `cookies_from_browser`, `cookie_file`, `embed_lyrics`, `client_id`, `client_secret`, `overwrite`, `verbose`).

- **[include/spotdl/http.hpp](include/spotdl/http.hpp)**
  - `HttpClient`: Thread-safe RAII wrapper around `libcurl`.
  - Provides `get()`, `get_bytes()` (for binary images), `post()`, and `url_encode()`.
  - Supports strict IPv4 enforcement (`CURLOPT_IPRESOLVE = CURL_IPRESOLVE_V4`) and custom user-agent headers.

- **[include/spotdl/spotify.hpp](include/spotdl/spotify.hpp)**
  - `SpotifyFetcher`: Class responsible for fetching track metadata without requiring API credentials.
  - Implements `parse_url()` to detect track, album, playlist, artist top-tracks, or search queries.
  - Implements embed page HTML scraping (`__NEXT_DATA__` JSON extraction) and high-res cover art URL upgrading (`ab67616d0000b273` 640x640 upgrade).
  - Supports optional Spotify OAuth client credential API requests if `--client-id` and `--client-secret` are supplied.

- **[include/spotdl/lyrics.hpp](include/spotdl/lyrics.hpp)**
  - `LyricsFetcher`: Static service fetching synchronized (.lrc) or unsynchronized lyrics from the LRCLIB public API (`https://lrclib.net/api/get` with fallback to `search`).

- **[include/spotdl/tagger.hpp](include/spotdl/tagger.hpp)**
  - `AudioTagger`: Audio file tagging service using `TagLib`.
  - Supports ID3v2 for MP3 and WAV files (Title, Artists, Album, AlbumArtist, Track/Total, Disc, Date, Genre, ISRC, Publisher, Comment, USLT lyrics, APIC cover picture).
  - Supports MP4/M4A atoms (`©nam`, `©ART`, `©alb`, `aART`, `trkn`, `disk`, `©day`, `©gen`, `©lyr`, `covr`).
  - Supports FLAC metadata blocks (Vorbis comments + FLAC Picture block).

- **[include/spotdl/downloader.hpp](include/spotdl/downloader.hpp)**
  - `TrackDownloader`: Manages audio downloading and tagging workflow.
  - `build_file_path()`: Renders path template (e.g. `{artist} - {title}.{ext}`).
  - `sanitize_filename()`: Removes illegal characters (`\ / : * ? " < > |`).
  - `find_youtube_url()`: Performs fast HTTP search on YouTube to extract `"videoId":"..."` in ~200ms.
  - `run_ytdlp()`: Executes `yt-dlp` via pipe (`popen`), parses progress output, and handles callback notifications.

- **[include/spotdl/thread_pool.hpp](include/spotdl/thread_pool.hpp)**
  - `ThreadPool`: Lightweight C++ thread pool managing worker threads, a synchronized task queue, and futures for concurrent downloads.

- **[include/spotdl/ui.hpp](include/spotdl/ui.hpp)**
  - `ConsoleUI`: Terminal UI module with ANSI color formatting, banner printing, thread-safe live progress bar rendering, summary table printing, and verbose debug logging (`print_debug`).

---

### Source Implementations (`src/`)

- **[src/http.cpp](src/http.cpp)**: Implements libcurl initialization, callback handlers, IPv4 resolution rules, and verbose debug output.
- **[src/spotify.cpp](src/spotify.cpp)**: Implements regex URL parsing, HTML script tag parsing (`__NEXT_DATA__`), JSON traversal, and Spotify Web API OAuth fallback.
- **[src/lyrics.cpp](src/lyrics.cpp)**: Implements LRCLIB API GET and search requests.
- **[src/tagger.cpp](src/tagger.cpp)**: Implements detailed TagLib frame creation for ID3v2, MP4, and FLAC, including APIC / CoverArt and USLT / Vorbis lyrics.
- **[src/downloader.cpp](src/downloader.cpp)**: Implements fast HTTP search lookup, `yt-dlp` popen execution, regex progress output parsing, retry delay logic, and metadata tagging pipeline.
- **[src/ui.cpp](src/ui.cpp)**: Implements ANSI color formatting, live carriage-return progress updating (`[45.3% @ 2.25MiB/s]`), debug printing when `-v` is active, and the summary table.
- **[src/main.cpp](src/main.cpp)**: Entry point parsing command-line options, orchestrating metadata fetching, dispatching tasks to `ThreadPool`, running post-download retry passes, and displaying summary results.

---

### Test Suite (`tests/`)

- **[tests/test_main.cpp](tests/test_main.cpp)**: Unit tests for URL parsing, filename sanitization, output path templates, HTTP networking with IPv4, live Spotify embed metadata scraping, and TagLib MP3 tag creation.

---

## 3. Data & Control Flow Architecture

```
User CLI Invocation (spotdl [queries...] -v --force-ipv4)
                      │
                      ▼
               [ src/main.cpp ]
                      │
   1. Fetch Metadata  │
                      ▼
            [ SpotifyFetcher ] ─────► HTTP GET (open.spotify.com/embed)
                      │               Scrapes __NEXT_DATA__ JSON
                      ▼
           std::vector<TrackMetadata>
                      │
   2. Parallel Queue  │
                      ▼
             [ ThreadPool ]
                      │
         ┌────────────┴────────────┐
         ▼                         ▼
   Worker Thread 1           Worker Thread 2
         │                         │
  [TrackDownloader]         [TrackDownloader]
         │                         │
         ├─► Fast HTTP Lookup ──► GET youtube.com/results ("videoId":"...")
         │
         ├─► run_ytdlp() ────────► exec "yt-dlp" pipe
         │                            └─► progress_callback -> ConsoleUI
         │
         ├─► LyricsFetcher ──────► GET lrclib.net/api
         │
         └─► AudioTagger ────────► TagLib (ID3v2 / MP4 / FLAC)
                      │
                      ▼
             [ Download Summary ]
```

---

## 4. How to Build & Run Tests

```bash
cd cxx_rewrite
mkdir -p build && cd build
cmake ..
cmake --build . -j$(nproc)

# Run test suite
./bin/spotdl_tests

# Run application with verbose debug logging
./bin/spotdl "https://open.spotify.com/track/..." -v --force-ipv4
```

>[!disclaimer] disclaimer
>"This DEVELOPMENT.md is AI written. I was just lazy to write this.


# spotdl-reborn (C++ Rewrite)

A high-performance C++20 implementation of `spotdl-reborn` powered by CMake, TagLib, libcurl, and yt-dlp.

## Features

- **C++20 Architecture**: Ultra-fast and memory-efficient native binary.
- **TagLib Metadata Tagging**: Complete ID3v2.4 (MP3, WAV), MP4/M4A, and FLAC tagging (Title, Artists, Album, Track & Disc numbers, Year, ISRC, Genre, synchronized lyrics, and high-res cover art).
- **Strict IPv4 Resolution**: Enforces IPv4 for all network requests via libcurl and yt-dlp.
- **No Spotify API Keys Required**: Embed scraper extracts metadata for tracks, playlists, and albums automatically.
- **Synchronized Lyrics**: Embeds LRC or plain lyrics directly into audio tags via LRCLIB.
- **Multi-threaded Downloads**: Concurrent worker thread pool with live progress bars and automatic retry passes.

## Prerequisites

- CMake 3.20+
- C++20 compiler (`g++` 10+ or `clang++` 12+)
- `libcurl`
- `taglib` (v2.x or v1.x)
- `ffmpeg`
- `yt-dlp`

On Ubuntu/Debian:
```bash
sudo apt-get install cmake g++ libcurl4-openssl-dev libtag1-dev ffmpeg
```

On Arch Linux:
```bash
sudo pacman -S cmake gcc curl taglib ffmpeg yt-dlp
```

## Building

```bash
cd cxx_rewrite
mkdir -p build && cd build
cmake ..
cmake --build . -j$(nproc)
```

The compiled binaries will be placed in `cxx_rewrite/build/bin/`:
- `spotdl`: Main CLI downloader
- `spotdl_tests`: Unit test suite

## Running Tests

```bash
cd cxx_rewrite/build
ctest --output-on-failure
# Or run directly:
./bin/spotdl_tests
```

## Usage

```bash
# Download a Spotify track
./bin/spotdl "https://open.spotify.com/track/3ouNEk0tv5TTi8VWMe1xbX"

# Download a playlist with custom format and bitrate
./bin/spotdl "https://open.spotify.com/playlist/37i9dQZF1DXcBWIGoYBM5M" --format mp3 --bitrate 320k --threads 4

# Force IPv4 and extract cookies from browser
./bin/spotdl "https://open.spotify.com/track/..." --force-ipv4 --cookies-from-browser firefox

# Embed synchronized lyrics into audio file
./bin/spotdl "https://open.spotify.com/track/..." --embed-lyrics
```

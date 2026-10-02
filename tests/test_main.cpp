#include "spotdl/types.hpp"
#include "spotdl/http.hpp"
#include "spotdl/spotify.hpp"
#include "spotdl/downloader.hpp"
#include "spotdl/tagger.hpp"
#include "spotdl/lyrics.hpp"

#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>

namespace fs = std::filesystem;
using namespace spotdl;

void test_url_parsing() {
    std::cout << "[TEST] Running URL parsing tests..." << std::endl;

    auto [t1, id1] = SpotifyFetcher::parse_url("spotify:track:3ouNEk0tv5TTi8VWMe1xbX");
    assert(t1 == "track");
    assert(id1 == "3ouNEk0tv5TTi8VWMe1xbX");

    auto [t2, id2] = SpotifyFetcher::parse_url("https://open.spotify.com/track/3ouNEk0tv5TTi8VWMe1xbX?si=abc");
    assert(t2 == "track");
    assert(id2 == "3ouNEk0tv5TTi8VWMe1xbX");

    auto [t3, id3] = SpotifyFetcher::parse_url("https://open.spotify.com/intl-de/playlist/37i9dQZF1DXcBWIGoYBM5M");
    assert(t3 == "playlist");
    assert(id3 == "37i9dQZF1DXcBWIGoYBM5M");

    auto [t4, id4] = SpotifyFetcher::parse_url("https://open.spotify.com/album/4m2880jivSbbyEGAKfITCa");
    assert(t4 == "album");
    assert(id4 == "4m2880jivSbbyEGAKfITCa");

    auto [t5, id5] = SpotifyFetcher::parse_url("Coldplay - Yellow");
    assert(t5 == "query");
    assert(id5 == "Coldplay - Yellow");

    std::cout << "[+] URL parsing tests passed!" << std::endl;
}

void test_filename_and_path_building() {
    std::cout << "[TEST] Running filename sanitization and template tests..." << std::endl;

    std::string sanitized = TrackDownloader::sanitize_filename(R"(AC/DC: "Back in Black" <Remastered>? *|)");
    assert(sanitized.find('/') == std::string::npos);
    assert(sanitized.find('\\') == std::string::npos);
    assert(sanitized.find(':') == std::string::npos);
    assert(sanitized.find('*') == std::string::npos);
    assert(sanitized.find('?') == std::string::npos);
    assert(sanitized.find('"') == std::string::npos);
    assert(sanitized.find('<') == std::string::npos);
    assert(sanitized.find('>') == std::string::npos);
    assert(sanitized.find('|') == std::string::npos);

    DownloadOptions opts;
    opts.output_template = "{artist} - {title}.{ext}";
    opts.audio_format = "mp3";

    TrackDownloader downloader(opts);
    TrackMetaData t;
    t.title = "Animal";
    t.artists = {"KATSEYE"};
    t.album_name = "SIS (Soft Is Strong)";
    t.track_number = 2;
    t.disc_number = 1;

    std::string path = downloader.build_file_path(t, "test_output");
    assert(path.find("KATSEYE - Animal.mp3") != std::string::npos);

    std::cout << "[+] Filename & template tests passed!" << std::endl;
}

void test_http_and_ipv4() {
    std::cout << "[TEST] Running HTTP client and IPv4 tests..." << std::endl;

    HttpClient client(true, 10);
    auto resp = client.get("https://open.spotify.com/embed/track/3ouNEk0tv5TTi8VWMe1xbX");
    assert(resp.has_value());
    assert(resp->find("__NEXT_DATA__") != std::string::npos);

    std::cout << "[+] HTTP client and IPv4 tests passed!" << std::endl;
}

void test_spotify_embed_scraping() {
    std::cout << "[TEST] Running Spotify metadata fetch test (without API keys)..." << std::endl;

    SpotifyFetcher fetcher("", "", true);
    auto tracks = fetcher.fetch("https://open.spotify.com/track/3ouNEk0tv5TTi8VWMe1xbX");

    assert(!tracks.empty());
    assert(tracks[0].title.find("Animal") != std::string::npos);
    assert(tracks[0].artist_str().find("KATSEYE") != std::string::npos);
    assert(!tracks[0].cover_url.empty());

    std::cout << "[+] Spotify metadata fetch passed! Track: "
              << tracks[0].artist_str() << " - " << tracks[0].title << std::endl;
}

void test_audio_tagging() {
    std::cout << "[TEST] Running AudioTagger tests..." << std::endl;

    // Create a minimal valid empty MP3 frame structure for testing ID3v2 tag writing
    fs::path temp_mp3 = "test_scratch_tag.mp3";
    {
        std::ofstream ofs(temp_mp3, std::ios::binary);
        // Write standard ID3v2 empty header + minimal MPEG sync word header
        unsigned char mp3_bytes[] = {
            0xFF, 0xFB, 0x90, 0x64, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };
        for (int i = 0; i < 100; ++i) {
            ofs.write(reinterpret_cast<char*>(mp3_bytes), sizeof(mp3_bytes));
        }
    }

    TrackMetaData track;
    track.title = "Test Song Title";
    track.artists = {"First Artist", "Second Artist"};
    track.album_name = "Test Album";
    track.album_artist = "First Artist";
    track.track_number = 3;
    track.total_tracks = 10;
    track.disc_number = 1;
    track.release_date = "2008-01-14";

    bool tagged = AudioTagger::apply_metadata(temp_mp3.string(), track, "Test lyrics here");
    assert(tagged);

    // read back tags with TagLib to verify
    TagLib::MPEG::File f(temp_mp3.string().c_str());
    assert(f.isValid());
    auto* tag = f.ID3v2Tag();
    assert(tag != nullptr);
    assert(std::string(tag->title().toCString(true)) == "Test Song Title");
    assert(std::string(tag->album().toCString(true)) == "Test Album");

    fs::remove(temp_mp3);
    std::cout << "[+] Audio tagging test passed!" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "spotdl-reborn C++ Unit Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        test_url_parsing();
        test_filename_and_path_building();
        test_http_and_ipv4();
        test_spotify_embed_scraping();
        test_audio_tagging();

        std::cout << "\n========================================" << std::endl;
        std::cout << "All C++ Unit Tests Passed Successfully!" << std::endl;
        std::cout << "========================================" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}

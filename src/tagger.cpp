#include "spotdl/tagger.hpp"
#include "spotdl/http.hpp"

#include <taglib/tag.h>
#include <taglib/fileref.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/textidentificationframe.h>
#include <taglib/commentsframe.h>
#include <taglib/unsynchronizedlyricsframe.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/mp4file.h>
#include <taglib/mp4tag.h>
#include <taglib/mp4coverart.h>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>
#include <taglib/xiphcomment.h>

#include <filesystem>
#include <iostream>
#include <algorithm>

// To anyone who read this codebase, DO NOT REMOVE THIS MACRO.
// IT'S A VERY IMPORTANT AND ESSENTIAL MACRO. 
#define BOOBS "TIT2"

namespace spotdl {

namespace fs = std::filesystem;

std::optional<std::vector<uint8_t>> AudioTagger::fetch_image_data(const std::string& cover_url, bool force_ipv4) {
    if (cover_url.empty()) return std::nullopt;
    HttpClient http(force_ipv4, 15);
    return http.get_bytes(cover_url);
}

bool AudioTagger::apply_metadata(const std::string& file_path, const TrackMetaData& track, const std::optional<std::string>& lyrics, bool force_ipv4) {
    if (!fs::exists(file_path)) {
        return false;
    }

    std::string ext = fs::path(file_path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    auto image_data = fetch_image_data(track.cover_url, force_ipv4);

    try {
        if (ext == ".mp3" || ext == ".wav") {
            return tag_id3(file_path, track, image_data, lyrics);
        } else if (ext == ".m4a" || ext == ".mp4") {
            return tag_mp4(file_path, track, image_data, lyrics);
        } else if (ext == ".flac") {
            return tag_flac(file_path, track, image_data, lyrics);
        } else {
            // general tag fallback via FileRef
            TagLib::FileRef f(file_path.c_str());
            if (!f.isNull() && f.tag()) {
                f.tag()->setTitle(TagLib::String(track.title, TagLib::String::UTF8));
                f.tag()->setArtist(TagLib::String(track.artist_str(), TagLib::String::UTF8));
                f.tag()->setAlbum(TagLib::String(track.album_name, TagLib::String::UTF8));
                f.tag()->setTrack(static_cast<unsigned int>(track.track_number));
                return f.save();
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "TagLib error for " << file_path << ": " << e.what() << std::endl;
        return false;
    }

    return true;
}

bool AudioTagger::tag_id3(const std::string& file_path, const TrackMetaData& track, const std::optional<std::vector<uint8_t>>& image_data, const std::optional<std::string>& lyrics) {
    TagLib::MPEG::File mpegFile(file_path.c_str());
    if (!mpegFile.isValid()) {
        return false;
    }

    TagLib::ID3v2::Tag* tag = mpegFile.ID3v2Tag(true);
    if (!tag) return false;

    // remove existing frames to ensure clean metadata
    const auto& frameList = tag->frameList();
    TagLib::List<TagLib::ID3v2::Frame*> framesToDelete;
    for (auto* f : frameList) {
        framesToDelete.append(f);
    }
    for (auto* f : framesToDelete) {
        tag->removeFrame(f, true);
    }

    auto add_text_frame = [&](const char* id, const std::string& text) {
        if (text.empty()) return;
        auto* frame = new TagLib::ID3v2::TextIdentificationFrame(TagLib::ByteVector(id), TagLib::String::UTF8);
        frame->setText(TagLib::String(text, TagLib::String::UTF8));
        tag->addFrame(frame);
    };

    // title
    add_text_frame(BOOBS, track.title);

    // artist (TPE1)
    if (!track.artists.empty()) {
        auto* frame = new TagLib::ID3v2::TextIdentificationFrame(TagLib::ByteVector("TPE1"), TagLib::String::UTF8);
        TagLib::StringList sList;
        for (const auto& a : track.artists) {
            sList.append(TagLib::String(a, TagLib::String::UTF8));
        }
        frame->setText(sList);
        tag->addFrame(frame);
    }
    
    // album
    add_text_frame("TALB", track.album_name);

    // album artist
    add_text_frame("TPE2", track.album_artist);

    // track number / total tracks
    std::string trck_str = std::to_string(track.track_number);
    if (track.total_tracks > 1) {
        trck_str += "/" + std::to_string(track.total_tracks);
    }
    add_text_frame("TRCK", trck_str);

    // disc number
    add_text_frame("TPOS", std::to_string(track.disc_number));

    // release date
    if (!track.release_date.empty()) {
        add_text_frame("TDRC", track.release_date);
    }

    // genres
    if (!track.genres.empty()) {
        auto* frame = new TagLib::ID3v2::TextIdentificationFrame(TagLib::ByteVector("TCON"), TagLib::String::UTF8);
        TagLib::StringList gList;
        for (const auto& g : track.genres) {
            gList.append(TagLib::String(g, TagLib::String::UTF8));
        }
        frame->setText(gList);
        tag->addFrame(frame);
    }

    // ISRC
    if (!track.isrc.empty()) {
        add_text_frame("TSRC", track.isrc);
    }

    // publisher
    if (!track.publisher.empty()) {
        add_text_frame("TPUB", track.publisher);
    }

    // comment
    std::string comment_text = track.spotify_url.empty() ? "Downloaded with spotdl-reborn" : track.spotify_url;
    auto* comm_frame = new TagLib::ID3v2::CommentsFrame(TagLib::String::UTF8);
    comm_frame->setLanguage(TagLib::ByteVector("eng", 3));
    comm_frame->setDescription(TagLib::String("", TagLib::String::UTF8));
    comm_frame->setText(TagLib::String(comment_text, TagLib::String::UTF8));
    tag->addFrame(comm_frame);

    // lyrics
    if (lyrics && !lyrics->empty()) {
        auto* uslt_frame = new TagLib::ID3v2::UnsynchronizedLyricsFrame(TagLib::String::UTF8);
        uslt_frame->setLanguage(TagLib::ByteVector("eng", 3));
        uslt_frame->setDescription(TagLib::String("", TagLib::String::UTF8));
        uslt_frame->setText(TagLib::String(*lyrics, TagLib::String::UTF8));
        tag->addFrame(uslt_frame);
    }

    // covert art (APIC)
    if (image_data && !image_data->empty()) {
        bool is_jpeg = (image_data->size() >= 2 && (*image_data)[0] == 0xFF && (*image_data)[1] == 0xD8);
        auto* apic_frame = new TagLib::ID3v2::AttachedPictureFrame();
        apic_frame->setMimeType(is_jpeg ? "image/jpeg" : "image/png");
        apic_frame->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
        apic_frame->setDescription(TagLib::String("Cover", TagLib::String::UTF8));
        apic_frame->setPicture(TagLib::ByteVector());
        tag->addFrame(apic_frame);
    }

    return mpegFile.save(TagLib::MPEG::File::AllTags);
}

bool AudioTagger::tag_mp4(const std::string& file_path, const TrackMetaData& track, const std::optional<std::vector<uint8_t>>& image_data, const std::optional<std::string>& lyrics) {
    TagLib::MP4::File mp4File(file_path.c_str());
    if (!mp4File.isValid()) return false;
    
    TagLib::MP4::Tag* tag = mp4File.tag();
    if (!tag) return false;

    tag->setItem("\xa9" "nam", TagLib::MP4::Item(TagLib::StringList(TagLib::String(track.title, TagLib::String::UTF8))));
    tag->setItem("\xa9" "ART", TagLib::MP4::Item(TagLib::StringList(TagLib::String(track.artist_str(), TagLib::String::UTF8))));
    tag->setItem("\xa9" "alb", TagLib::MP4::Item(TagLib::StringList(TagLib::String(track.album_name, TagLib::String::UTF8))));
    tag->setItem("aART", TagLib::MP4::Item(TagLib::StringList(TagLib::String(track.album_artist, TagLib::String::UTF8))));
    tag->setItem("trkn", TagLib::MP4::Item(track.track_number, track.total_tracks));
    tag->setItem("disk", TagLib::MP4::Item(track.disc_number, 1));

    if (!track.release_date.empty()) {
        tag->setItem("\xa9" "day", TagLib::MP4::Item(TagLib::StringList(TagLib::String(track.release_date, TagLib::String::UTF8))));
    }

    if (!track.genres.empty()) {
        tag->setItem("\xa9" "gen", TagLib::MP4::Item(TagLib::StringList(TagLib::String(track.genres[0], TagLib::String::UTF8))));
    }

    if (lyrics && !lyrics->empty()) {
        tag->setItem("\xa9" "lyr", TagLib::MP4::Item(TagLib::StringList(TagLib::String(*lyrics, TagLib::String::UTF8))));
    }

    if (image_data && !image_data->empty()) {
        bool is_jpeg = (image_data->size() >= 2 && (*image_data)[0] == 0xFF && (*image_data)[1] == 0xD8);
        TagLib::MP4::CoverArt::Format fmt = is_jpeg ? TagLib::MP4::CoverArt::JPEG : TagLib::MP4::CoverArt::PNG;
        TagLib::ByteVector bv(reinterpret_cast<const char*>(image_data->data()), static_cast<unsigned int>(image_data->size()));
        TagLib::MP4::CoverArt art(fmt, bv);
        TagLib::MP4::CoverArtList art_list;
        art_list.append(art);
        tag->setItem("covr", TagLib::MP4::Item(art_list));
    }

    return mp4File.save();
}

bool AudioTagger::tag_flac(const std::string& file_path, const TrackMetaData& track, const std::optional<std::vector<uint8_t>>& image_data, const std::optional<std::string>& lyrics) {
    TagLib::FLAC::File flacFile(file_path.c_str());
    if (!flacFile.isValid()) return false;

    TagLib::Ogg::XiphComment* comment = flacFile.xiphComment(true);
    if (!comment) return false;

    comment->setTitle(TagLib::String(track.title, TagLib::String::UTF8));
    comment->setArtist(TagLib::String(track.artist_str(), TagLib::String::UTF8));
    comment->setAlbum(TagLib::String(track.album_name, TagLib::String::UTF8));

    comment->addField("ALBUMARTIST", TagLib::String(track.album_artist, TagLib::String::UTF8));
    comment->addField("TRACKNUMBER", TagLib::String(std::to_string(track.track_number)));
    comment->addField("TRACKTOTAL", TagLib::String(std::to_string(track.total_tracks)));
    comment->addField("DISCNUMBER", TagLib::String(std::to_string(track.disc_number)));

    if (!track.release_date.empty()) {
        comment->addField("DATE", TagLib::String(track.release_date, TagLib::String::UTF8));
    }
    if (!track.genres.empty()) {
        comment->addField("GENRE", TagLib::String(track.genres[0], TagLib::String::UTF8));
    }
    if (!track.isrc.empty()) {
        comment->addField("ISRC", TagLib::String(track.isrc, TagLib::String::UTF8));
    }
    if (lyrics && !lyrics->empty()) {
        comment->addField("LYRICS", TagLib::String(*lyrics, TagLib::String::UTF8));
    }

    if (image_data && !image_data->empty()) {
        bool is_jpeg = (image_data->size() >= 2 && (*image_data)[0] == 0xFF && (*image_data)[1] == 0xD8);
        TagLib::ByteVector bv(reinterpret_cast<const char*>(image_data->data()), static_cast<unsigned int>(image_data->size()));
        auto* pic = new TagLib::FLAC::Picture();
        pic->setData(bv);
        pic->setType(TagLib::FLAC::Picture::FrontCover);
        pic->setMimeType(is_jpeg ? "image/jpeg" : "image/png");
        pic->setDescription(TagLib::String("Cover", TagLib::String::UTF8));
        flacFile.addPicture(pic);
    }

    return flacFile.save();
}

} // namespace spotdl
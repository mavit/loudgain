/*
 * Loudness normalizer based on the EBU R128 standard
 *
 * Copyright (c) 2014, Alessandro Ghedini
 * All rights reserved.
 * 2019-06-30 - Matthias C. Hormann
 *   - Tag format in accordance with ReplayGain 2.0 spec
 *     https://wiki.hydrogenaud.io/index.php?title=ReplayGain_2.0_specification
 *   - Add Ogg Vorbis file handling
 * 2019-07-07 - v0.2.3 - Matthias C. Hormann
 *   - Write lowercase REPLAYGAIN_* tags to MP3 ID3v2, for incompatible players
 * 2019-07-08 - v0.2.4 - Matthias C. Hormann
 *   - add -s e mode, writes extra tags (REPLAYGAIN_REFERENCE_LOUDNESS,
 *     REPLAYGAIN_TRACK_RANGE and REPLAYGAIN_ALBUM_RANGE)
 *   - add "-s l" mode (like "-s e" but uses LU/LUFS instead of dB)
 * 2019-07-09 - v0.2.6 - Matthias C. Hormann
 *  - Add "-L" mode to force lowercase tags in MP3/ID3v2.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <taglib.h>

#include <textidentificationframe.h>

#include <mpegfile.h>
#include <id3v2tag.h>

#include <flacfile.h>
#include <vorbisfile.h>
#include <xiphcomment.h>

#include "scan.h"
#include "tag.h"
#include "printf.h"

static void tag_add_txxx(TagLib::ID3v2::Tag *tag, char *name, char *value) {
	TagLib::ID3v2::UserTextIdentificationFrame *frame =
		new TagLib::ID3v2::UserTextIdentificationFrame;

	frame -> setDescription(name);
	frame -> setText(value);

	tag -> addFrame(frame);
}

// Even if the ReplayGain 2 standard proposes replaygain tags to be uppercase,
// unfortunately some players only respect the lowercase variant (still).
// So for the time being, we write non-standard lowercase tags to ID3v2.
void tag_write_mp3(scan_result *scan, bool do_album, char mode, char *unit, bool lowercase) {
	char value[2048];

	TagLib::MPEG::File f(scan -> file);
	TagLib::ID3v2::Tag *tag = f.ID3v2Tag(true);

	if (lowercase) {
		// use lowercase replaygain tags
		// (non-standard but used by foobar2000 and needed by some players like IDJC)
		snprintf(value, sizeof(value), "%.2f %s", scan -> track_gain, unit);
		tag_add_txxx(tag, const_cast<char *>("replaygain_track_gain"), value);

		snprintf(value, sizeof(value), "%.6f", scan -> track_peak);
		tag_add_txxx(tag, const_cast<char *>("replaygain_track_peak"), value);

		// Only write album tags if in album mode (would be zero otherwise)
		if (do_album) {
			snprintf(value, sizeof(value), "%.2f %s", scan -> album_gain, unit);
			tag_add_txxx(tag, const_cast<char *>("replaygain_album_gain"), value);

			snprintf(value, sizeof(value), "%.6f", scan -> album_peak);
			tag_add_txxx(tag, const_cast<char *>("replaygain_album_peak"), value);
		}

		// extra tags mode -s e or -s l
		if (mode == 'e' || mode == 'l') {
			snprintf(value, sizeof(value), "%.2f LUFS", scan -> loudness_reference);
			tag_add_txxx(tag, const_cast<char *>("replaygain_reference_loudness"), value);

			snprintf(value, sizeof(value), "%.2f %s", scan -> track_loudness_range, unit);
			tag_add_txxx(tag, const_cast<char *>("replaygain_track_range"), value);

			if (do_album) {
				snprintf(value, sizeof(value), "%.2f %s", scan -> album_loudness_range, unit);
				tag_add_txxx(tag, const_cast<char *>("replaygain_album_range"), value);
			}
		}
	} else {
		// use standard-compliant uppercase replaygain tags (default)
		// required, for instance, by VLC
		// unforunately not respected by others, like IDJC
		snprintf(value, sizeof(value), "%.2f %s", scan -> track_gain, unit);
		tag_add_txxx(tag, const_cast<char *>("REPLAYGAIN_TRACK_GAIN"), value);

		snprintf(value, sizeof(value), "%.6f", scan -> track_peak);
		tag_add_txxx(tag, const_cast<char *>("REPLAYGAIN_TRACK_PEAK"), value);

		// Only write album tags if in album mode (would be zero otherwise)
		if (do_album) {
			snprintf(value, sizeof(value), "%.2f %s", scan -> album_gain, unit);
			tag_add_txxx(tag, const_cast<char *>("REPLAYGAIN_ALBUM_GAIN"), value);

			snprintf(value, sizeof(value), "%.6f", scan -> album_peak);
			tag_add_txxx(tag, const_cast<char *>("REPLAYGAIN_ALBUM_PEAK"), value);
		}

		// extra tags mode -s e or -s l
		if (mode == 'e' || mode == 'l') {
			snprintf(value, sizeof(value), "%.2f LUFS", scan -> loudness_reference);
			tag_add_txxx(tag, const_cast<char *>("REPLAYGAIN_REFERENCE_LOUDNESS"), value);

			snprintf(value, sizeof(value), "%.2f %s", scan -> track_loudness_range, unit);
			tag_add_txxx(tag, const_cast<char *>("REPLAYGAIN_TRACK_RANGE"), value);

			if (do_album) {
				snprintf(value, sizeof(value), "%.2f %s", scan -> album_loudness_range, unit);
				tag_add_txxx(tag, const_cast<char *>("REPLAYGAIN_ALBUM_RANGE"), value);
			}
		}
	}

	f.save(TagLib::MPEG::File::ID3v2, false);
}

void tag_clear_mp3(scan_result *scan) {
	TagLib::MPEG::File f(scan -> file);
	TagLib::ID3v2::Tag *tag = f.ID3v2Tag(true);

	TagLib::ID3v2::FrameList::Iterator it;
	TagLib::ID3v2::FrameList frames = tag -> frameList("TXXX");

	for (it = frames.begin(); it != frames.end(); ++it) {
		TagLib::ID3v2::UserTextIdentificationFrame *frame =
		 dynamic_cast<TagLib::ID3v2::UserTextIdentificationFrame*>(*it);

		 // this removes all variants of upper-/lower-/mixed-case tags
		if (frame && frame -> fieldList().size() >= 2) {
			TagLib::String desc = frame -> description().upper();

			// also remove (old) reference loudness, it might be wrong after recalc
			if ((desc == "REPLAYGAIN_TRACK_GAIN") ||
			    (desc == "REPLAYGAIN_TRACK_PEAK") ||
					(desc == "REPLAYGAIN_TRACK_RANGE") ||
			    (desc == "REPLAYGAIN_ALBUM_GAIN") ||
			    (desc == "REPLAYGAIN_ALBUM_PEAK") ||
					(desc == "REPLAYGAIN_ALBUM_RANGE") ||
			    (desc == "REPLAYGAIN_REFERENCE_LOUDNESS"))
				tag -> removeFrame(frame);
		}
	}

	f.save(TagLib::MPEG::File::ID3v2, false);
}

void tag_write_flac(scan_result *scan, bool do_album, char mode, char *unit) {
	char value[2048];

	TagLib::FLAC::File f(scan -> file);
	TagLib::Ogg::XiphComment *tag = f.xiphComment(true);

	snprintf(value, sizeof(value), "%.2f %s", scan -> track_gain, unit);
	tag -> addField("REPLAYGAIN_TRACK_GAIN", value);

	snprintf(value, sizeof(value), "%.6f", scan -> track_peak);
	tag -> addField("REPLAYGAIN_TRACK_PEAK", value);

	// Only write album tags if in album mode (would be zero otherwise)
	if (do_album) {
		snprintf(value, sizeof(value), "%.2f %s", scan -> album_gain, unit);
		tag -> addField("REPLAYGAIN_ALBUM_GAIN", value);

		snprintf(value, sizeof(value), "%.6f", scan -> album_peak);
		tag -> addField("REPLAYGAIN_ALBUM_PEAK", value);
	}

	// extra tags mode -s e or -s l
	if (mode == 'e' || mode == 'l') {
		snprintf(value, sizeof(value), "%.2f LUFS", scan -> loudness_reference);
		tag -> addField("REPLAYGAIN_REFERENCE_LOUDNESS", value);

		snprintf(value, sizeof(value), "%.2f %s", scan -> track_loudness_range, unit);
		tag -> addField("REPLAYGAIN_TRACK_RANGE", value);

		if (do_album) {
			snprintf(value, sizeof(value), "%.2f %s", scan -> album_loudness_range, unit);
			tag -> addField("REPLAYGAIN_ALBUM_RANGE", value);
		}
	}

	f.save();
}

void tag_clear_flac(scan_result *scan) {
	TagLib::FLAC::File f(scan -> file);
	TagLib::Ogg::XiphComment *tag = f.xiphComment(true);

	tag -> removeField("REPLAYGAIN_TRACK_GAIN");
	tag -> removeField("REPLAYGAIN_TRACK_PEAK");
	tag -> removeField("REPLAYGAIN_TRACK_RANGE");
	tag -> removeField("REPLAYGAIN_ALBUM_GAIN");
	tag -> removeField("REPLAYGAIN_ALBUM_PEAK");
	tag -> removeField("REPLAYGAIN_ALBUM_RANGE");
	tag -> removeField("REPLAYGAIN_REFERENCE_LOUDNESS");

	f.save();
}

void tag_write_vorbis(scan_result *scan, bool do_album, char mode, char *unit) {
	char value[2048];

	TagLib::Ogg::Vorbis::File f(scan -> file);
	TagLib::Ogg::XiphComment *tag = f.tag();

	snprintf(value, sizeof(value), "%.2f %s", scan -> track_gain, unit);
	tag -> addField("REPLAYGAIN_TRACK_GAIN", value);

	snprintf(value, sizeof(value), "%.6f", scan -> track_peak);
	tag -> addField("REPLAYGAIN_TRACK_PEAK", value);

	// Only write album tags if in album mode (would be zero otherwise)
	if (do_album) {
		snprintf(value, sizeof(value), "%.2f %s", scan -> album_gain, unit);
		tag -> addField("REPLAYGAIN_ALBUM_GAIN", value);

		snprintf(value, sizeof(value), "%.6f", scan -> album_peak);
		tag -> addField("REPLAYGAIN_ALBUM_PEAK", value);
	}

	// extra tags mode -s e or -s l
	if (mode == 'e' || mode == 'l') {
		snprintf(value, sizeof(value), "%.2f LUFS", scan -> loudness_reference);
		tag -> addField("REPLAYGAIN_REFERENCE_LOUDNESS", value);

		snprintf(value, sizeof(value), "%.2f %s", scan -> track_loudness_range, unit);
		tag -> addField("REPLAYGAIN_TRACK_RANGE", value);

		if (do_album) {
			snprintf(value, sizeof(value), "%.2f %s", scan -> album_loudness_range, unit);
			tag -> addField("REPLAYGAIN_ALBUM_RANGE", value);
		}
	}

	f.save();
}

void tag_clear_vorbis(scan_result *scan) {
	TagLib::Ogg::Vorbis::File f(scan -> file);
	TagLib::Ogg::XiphComment *tag = f.tag();

	tag -> removeField("REPLAYGAIN_TRACK_GAIN");
	tag -> removeField("REPLAYGAIN_TRACK_PEAK");
	tag -> removeField("REPLAYGAIN_TRACK_RANGE");
	tag -> removeField("REPLAYGAIN_ALBUM_GAIN");
	tag -> removeField("REPLAYGAIN_ALBUM_PEAK");
	tag -> removeField("REPLAYGAIN_ALBUM_RANGE");
	tag -> removeField("REPLAYGAIN_REFERENCE_LOUDNESS");

	f.save();
}

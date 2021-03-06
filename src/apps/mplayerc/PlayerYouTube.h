/*
 * (C) 2012-2016 see Authors.txt
 *
 * This file is part of MPC-BE.
 *
 * MPC-BE is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-BE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include <vector>

namespace Youtube
{
	enum yformat {
		y_mp4,
		y_webm,
		// not used
		y_flv,
		y_3gp,
	};

	enum ytype {
		y_media,
		y_video,
		y_audio,
	};

	struct YoutubeProfile {
		int		iTag;
		yformat	format;
		ytype	type;
		int		quality;
		LPCTSTR	ext;
		bool	fps60;
	};

	// MP4, WebM and 360p resolution or above only
	static const YoutubeProfile YProfiles[] = {
		// MP4 (H.264)
		{266, y_mp4,  y_video, 2160, L"mp4",  false},
		//{138, y_mp4,  y_video, 2160, L"mp4",  false}, // sometimes duplicates the 266, so useless
		{264, y_mp4,  y_video, 1440, L"mp4",  false},
		{299, y_mp4,  y_video, 1080, L"mp4",  true },
		{137, y_mp4,  y_video, 1080, L"mp4",  false},
		{298, y_mp4,  y_video,  720, L"mp4",  true },
		{ 22, y_mp4,  y_media,  720, L"mp4",  false}, // H.264 + AAC
		{135, y_mp4,  y_video,  480, L"mp4",  false},
		{ 18, y_mp4,  y_media,  360, L"mp4",  false}, // H.264 + AAC
		// WebM (VP9)
		{315, y_webm, y_video, 2160, L"webm", true },
		{313, y_webm, y_video, 2160, L"webm", false},
		{308, y_webm, y_video, 1440, L"webm", true },
		{271, y_webm, y_video, 1440, L"webm", false},
		{303, y_webm, y_video, 1080, L"webm", true },
		{248, y_webm, y_video, 1080, L"webm", false},
		{302, y_webm, y_video,  720, L"webm", true },
		{247, y_webm, y_video,  720, L"webm", false},
		{244, y_webm, y_video,  480, L"webm", false},
		{ 43, y_webm, y_media,  360, L"webm", false}, // VP8 + Vorbis
	};

	static const YoutubeProfile YAudioProfiles[] = {
		// AAC
		{140, y_mp4,  y_audio, 128, L"m4a",  false},
		{139, y_mp4,  y_audio,  48, L"m4a",  false}, // may be outdated and no longer supported
		// Opus
		{251, y_webm, y_audio, 160, L"webm", false},
		{250, y_webm, y_audio,  70, L"webm", false},
		{249, y_webm, y_audio,  50, L"webm", false},
		// Vorbis
		//{249, y_webm, y_audio, 128, L"webm", false},
	};

	struct YoutubeFields {
		CString        author;
		CString        title;
		CString        content;
		CString        fname;
		SYSTEMTIME     dtime    = { 0 };
		REFERENCE_TIME duration = 0;

		void Empty() {
			author.Empty();
			title.Empty();
			content.Empty();
			fname.Empty();
			dtime = { 0 };
			duration = 0;
		}
	};

	struct YoutubePlaylistItem {
		CString url, title;

		YoutubePlaylistItem() {};
		YoutubePlaylistItem(CString _url, CString _title)
			: url(_url)
			, title(_title) {};
	};
	typedef std::vector<YoutubePlaylistItem> YoutubePlaylist;

	struct YoutubeUrllistItem : YoutubePlaylistItem {
		const YoutubeProfile* profile;
	};
	typedef std::vector<YoutubeUrllistItem> YoutubeUrllist;

	bool CheckURL(CString url);
	bool CheckPlaylist(CString url);

	bool Parse_URL(CString url, CAtlList<CString>& urls, YoutubeFields& y_fields, YoutubeUrllist& youtubeUrllist, CSubtitleItemList& subs, REFERENCE_TIME& rtStart);
	bool Parse_Playlist(CString url, YoutubePlaylist& youtubePlaylist, int& idx_CurrentPlay);

	bool Parse_URL(CString url, YoutubeFields& y_fields);
}

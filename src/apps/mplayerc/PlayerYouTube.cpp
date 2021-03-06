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

#include "stdafx.h"
#include <afxinet.h>
#include <regex>
#include <jsoncpp/include/json/json.h>
#include "../../DSUtil/text.h"
#include "PlayerYouTube.h"

#define YOUTUBE_PL_URL              L"youtube.com/playlist?"
#define YOUTUBE_URL                 L"youtube.com/watch?"
#define YOUTUBE_URL_A               L"www.youtube.com/attribution_link"
#define YOUTUBE_URL_V               L"youtube.com/v/"
#define YOUTU_BE_URL                L"youtu.be/"

#define MATCH_STREAM_MAP_START      "\"url_encoded_fmt_stream_map\":\""
#define MATCH_ADAPTIVE_FMTS_START   "\"adaptive_fmts\":\""
#define MATCH_WIDTH_START           "meta property=\"og:video:width\" content=\""
#define MATCH_HLSVP_START           "\"hlsvp\":\""
#define MATCH_JS_START              "\"js\":\""
#define MATCH_END                   "\""

#define MATCH_PLAYLIST_ITEM_START   "<li class=\"yt-uix-scroller-scroll-unit "
#define MATCH_PLAYLIST_ITEM_START2  "<tr class=\"pl-video yt-uix-tile "

#define MATCH_AGE_RESTRICTION       "player-age-gate-content\">"
#define MATCH_STREAM_MAP_START_2    "url_encoded_fmt_stream_map="
#define MATCH_ADAPTIVE_FMTS_START_2 "adaptive_fmts="
#define MATCH_JS_START_2            "'PREFETCH_JS_RESOURCES': [\""
#define MATCH_END_2                 "&"

#define INTERNET_OPEN_FALGS         INTERNET_FLAG_NO_COOKIES | INTERNET_FLAG_TRANSFER_BINARY | INTERNET_FLAG_EXISTING_CONNECT | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD

namespace Youtube
{
	static const LPCTSTR GOOGLE_API_KEY = L"AIzaSyDggqSjryBducTomr4ttodXqFpl2HGdoyg";

	static const LPCTSTR videoIdRegExps[] = {
		L"v=([-a-zA-Z0-9_]+)",
		L"video_ids=([-a-zA-Z0-9_]+)"
	};

	const YoutubeProfile* GetProfile(int iTag)
	{
		for (int i = 0; i < _countof(YProfiles); i++) {
			if (iTag == YProfiles[i].iTag) {
				return &YProfiles[i];
			}
		}

		return NULL;
	}

	const YoutubeProfile* GetAudioProfile(int iTag)
	{
		for (int i = 0; i < _countof(YAudioProfiles); i++) {
			if (iTag == YAudioProfiles[i].iTag) {
				return &YAudioProfiles[i];
			}
		}

		return NULL;
	}

	static bool CompareProfile(const YoutubeProfile* a, const YoutubeProfile* b)
	{
		if (a->format != b->format) {
			return (a->format < b->format);
		}

		if (a->quality != b->quality) {
			return (a->quality > b->quality);
		}

		return (a->fps60 > b->fps60);
	}

	static bool CompareUrllistItem(YoutubeUrllistItem a, YoutubeUrllistItem b)
	{
		return CompareProfile(a.profile, b.profile);
	}

	static CString FixHtmlSymbols(CString inStr)
	{
		inStr.Replace(L"&quot;", L"\"");
		inStr.Replace(L"&amp;",  L"&");
		inStr.Replace(L"&#39;",  L"'");
		inStr.Replace(L"&#039;", L"'");
		inStr.Replace(L"\\n",    L"\r\n");
		inStr.Replace(L"\n",     L"\r\n");
		inStr.Replace(L"\\",     L"");

		return inStr;
	}

	static inline CStringA GetEntry(LPCSTR pszBuff, LPCSTR pszMatchStart, LPCSTR pszMatchEnd)
	{
		LPCSTR pStart = CStringA::StrTraits::StringFindString(pszBuff, pszMatchStart);
		if (pStart) {
			pStart += CStringA::StrTraits::SafeStringLen(pszMatchStart);

			LPCSTR pEnd = CStringA::StrTraits::StringFindString(pStart, pszMatchEnd);
			if (pEnd) {
				return CStringA(pStart, pEnd - pStart);
			}
		}

		return "";
	}

	static void HandleURL(CString& url)
	{
		url = UrlDecode(CStringA(url));

		int pos = url.Find(L"youtube.com/");
		if (pos == -1) {
			pos = url.Find(L"youtu.be/");
		}

		if (pos != -1) {
			url.Delete(0, pos);
			url = L"https://www." + url;

			if (url.Find(YOUTU_BE_URL) != -1) {
				url.Replace(YOUTU_BE_URL, YOUTUBE_URL);
				url.Replace(L"watch?", L"watch?v=");
			} else if (url.Find(YOUTUBE_URL_V) != -1) {
				url.Replace(L"v/", L"watch?v=");
			}
		}
	}

	bool CheckURL(CString url)
	{
		CString tmp_fn(url);
		tmp_fn.MakeLower();

		if (tmp_fn.Find(YOUTUBE_URL) != -1
				|| tmp_fn.Find(YOUTUBE_URL_A) != -1
				|| tmp_fn.Find(YOUTUBE_URL_V) != -1
				|| tmp_fn.Find(YOUTU_BE_URL) != -1) {
			return true;
		}

		return false;
	}

	bool CheckPlaylist(CString url)
	{
		CString tmp_fn(url);
		tmp_fn.MakeLower();

		if (tmp_fn.Find(YOUTUBE_PL_URL) != -1
				|| (tmp_fn.Find(YOUTUBE_URL) != -1 && tmp_fn.Find(L"&list=") != -1)
				|| (tmp_fn.Find(YOUTUBE_URL_A) != -1 && tmp_fn.Find(L"/watch_videos?video_ids") != -1)) {
			return true;
		}

		return false;
	}

	static CString RegExpParse(LPCTSTR szIn, LPCTSTR szRE)
	{
		const std::wregex regex(szRE);
		std::wcmatch match;
		if (std::regex_search(szIn, match, regex) && match.size() == 2) {
			return CString(match[1].first, match[1].length());
		}

		return L"";
	}

	static void InternetReadData(HINTERNET& f, char** pData, DWORD& dataSize, LPCSTR endCondition)
	{
		char buffer[16 * KILOBYTE] = { 0 };
		const DWORD dwNumberOfBytesToRead = _countof(buffer);
		DWORD dwBytesRead = 0;
		for (;;) {
			if (InternetReadFile(f, (LPVOID)buffer, dwNumberOfBytesToRead, &dwBytesRead) == FALSE || dwBytesRead == 0) {
				break;
			}

			*pData = (char*)realloc(*pData, dataSize + dwBytesRead + 1);
			memcpy(*pData + dataSize, buffer, dwBytesRead);
			dataSize += dwBytesRead;
			(*pData)[dataSize] = 0;

			if (endCondition && strstr(*pData, endCondition)) {
				break;
			}
		}
	}

	static bool ParseMetadata(HINTERNET& hInet, const CString videoId, YoutubeFields& y_fields)
	{
		if (hInet && !videoId.IsEmpty()) {
			CString link;
			link.Format(L"https://www.googleapis.com/youtube/v3/videos?id=%s&key=%s&part=snippet,contentDetails&fields=items/snippet/title,items/snippet/publishedAt,items/snippet/channelTitle,items/snippet/description,items/contentDetails/duration", videoId, GOOGLE_API_KEY);
			HINTERNET hUrl = InternetOpenUrl(hInet, link, NULL, 0, INTERNET_OPEN_FALGS, 0);
			if (hUrl) {
				char* data = NULL;
				DWORD dataSize = 0;
				InternetReadData(hUrl, &data, dataSize, NULL);
				InternetCloseHandle(hUrl);

				if (dataSize) {
					Json::Reader reader;
					Json::Value root;
					if (reader.parse(data, root)) {
						Json::Value snippet = root["items"][0]["snippet"];
						if (!snippet["title"].empty()
								&& snippet["title"].isString()) {
							const CStringA sTitle = snippet["title"].asCString();
							y_fields.title = FixHtmlSymbols(UTF8To16(sTitle));
						}

						if (!snippet["channelTitle"].empty()
								&& snippet["channelTitle"].isString()) {
							const CStringA sAuthor = snippet["channelTitle"].asCString();
							y_fields.author = UTF8To16(sAuthor);
						}

						if (!snippet["description"].empty()
								&& snippet["description"].isString()) {
							const CStringA sContent = snippet["description"].asCString();
							y_fields.content = FixHtmlSymbols(UTF8To16(sContent));
						}

						if (!snippet["publishedAt"].empty()
								&& snippet["publishedAt"].isString()) {
							const CStringA sDate = snippet["publishedAt"].asCString();
							WORD y, m, d;
							WORD hh, mm, ss;
							if (sscanf_s(sDate, "%04hu-%02hu-%02huT%02hu:%02hu:%02hu", &y, &m, &d, &hh, &mm, &ss) == 6) {
								y_fields.dtime.wYear   = y;
								y_fields.dtime.wMonth  = m;
								y_fields.dtime.wDay    = d;
								y_fields.dtime.wHour   = hh;
								y_fields.dtime.wMinute = mm;
								y_fields.dtime.wSecond = ss;
							}
						}

						Json::Value contentDetails = root["items"][0]["contentDetails"];
						if (!contentDetails["duration"].empty()
								&& contentDetails["duration"].isString()) {
							CStringA sDuration = contentDetails["duration"].asCString();
							const std::regex regex("PT(\\d+H)?(\\d{1,2}M)?(\\d{1,2}S)?", std::regex_constants::icase);
							std::cmatch match;
							if (std::regex_search(sDuration.GetBuffer(), match, regex) && match.size() == 4) {
								int h = 0;
								int m = 0;
								int s = 0;
								if (match[1].matched) {
									h = atoi(match[1].first);
								}
								if (match[2].matched) {
									m = atoi(match[2].first);
								}
								if (match[3].matched) {
									s = atoi(match[3].first);
								}

								y_fields.duration = (h * 3600 + m * 60 + s) * UNITS;
							}
						}
					}

					free(data);

					return true;
				}
			}
		}

		return false;
	}

	enum youtubeFuncType {
		funcNONE = -1,
		funcDELETE,
		funcREVERSE,
		funcSWAP
	};

	bool Parse_URL(CString url, CAtlList<CString>& urls, YoutubeFields& y_fields, YoutubeUrllist& youtubeUrllist, CSubtitleItemList& subs, REFERENCE_TIME& rtStart)
	{
		if (CheckURL(url)) {
			char* data = NULL;
			DWORD dataSize = 0;

			CString videoId;

			HINTERNET hUrl;
			HINTERNET hInet = InternetOpen(L"Googlebot", 0, NULL, NULL, 0);
			if (hInet) {
				HandleURL(url);

				for (int i = 0; i < _countof(videoIdRegExps) && videoId.IsEmpty(); i++) {
					videoId = RegExpParse(url, videoIdRegExps[i]);
				}

				if (rtStart <= 0) {
					BOOL bMatch = FALSE;

					const std::wregex regex(L"t=(\\d+h)?(\\d{1,2}m)?(\\d{1,2}s)?", std::regex_constants::icase);
					std::wcmatch match;
					if (std::regex_search(url.GetBuffer(), match, regex) && match.size() == 4) {
						int h = 0;
						int m = 0;
						int s = 0;
						if (match[1].matched) {
							h = _wtoi(match[1].first);
							bMatch = TRUE;
						}
						if (match[2].matched) {
							m = _wtoi(match[2].first);
							bMatch = TRUE;
						}
						if (match[3].matched) {
							s = _wtoi(match[3].first);
							bMatch = TRUE;
						}

						rtStart = (h * 3600 + m * 60 + s) * UNITS;
					}

					if (!bMatch) {
						const CString timeStart = RegExpParse(url, L"t=([0-9]+)");
						if (!timeStart.IsEmpty()) {
							rtStart = _wtol(timeStart) * UNITS;
						}
					}
				}

				hUrl = InternetOpenUrl(hInet, url, NULL, 0, INTERNET_OPEN_FALGS, 0);
				if (hUrl) {
					InternetReadData(hUrl, &data, dataSize, NULL);
					InternetCloseHandle(hUrl);
				}
			}

			if (!data) {
				if (hInet) {
					InternetCloseHandle(hInet);
				}

				return false;
			}

			const CString Title = UTF8To16(GetEntry(data, "<title>", "</title>"));
			y_fields.title = FixHtmlSymbols(Title);

			CAtlArray<youtubeFuncType> JSFuncs;
			CAtlArray<int> JSFuncArgs;
			BOOL bJSParsed = FALSE;
			CString JSUrl = UTF8To16(GetEntry(data, MATCH_JS_START, MATCH_END));
			if (JSUrl.IsEmpty()) {
				JSUrl = UTF8To16(GetEntry(data, MATCH_JS_START_2, MATCH_END));
			}
			if (!JSUrl.IsEmpty()) {
				JSUrl.Replace(L"\\/", L"/");
				JSUrl = L"https:" + JSUrl;
			}

			CStringA strUrls;

			if (strstr(data, MATCH_AGE_RESTRICTION)) {
				free(data);
				data = NULL;
				dataSize = 0;
				CString link; link.Format(L"https://www.youtube.com/get_video_info?video_id=%s&ps=default&eurl=&gl=US&hl=en&el=info", videoId);
				hUrl = InternetOpenUrl(hInet, link, NULL, 0, INTERNET_OPEN_FALGS, 0);
				if (hUrl) {
					InternetReadData(hUrl, &data, dataSize, NULL);
					InternetCloseHandle(hUrl);
				}

				if (!data) {
					if (hInet) {
						InternetCloseHandle(hInet);
					}
					return false;
				}

				// url_encoded_fmt_stream_map
				const CStringA stream_map = UrlDecode(GetEntry(data, MATCH_STREAM_MAP_START_2, MATCH_END_2));
				if (stream_map.IsEmpty()) {
					free(data);
					InternetCloseHandle(hInet);
					return false;
				}
				// adaptive_fmts
				const CStringA adaptive_fmts = UrlDecode(GetEntry(data, MATCH_ADAPTIVE_FMTS_START_2, MATCH_END_2));

				strUrls = stream_map;
				if (!adaptive_fmts.IsEmpty()) {
					strUrls += ',';
					strUrls += adaptive_fmts;
				}
			} else {
				// "hlspv" - live streaming
				const CStringA hlspv_url = GetEntry(data, MATCH_HLSVP_START, MATCH_END);
				if (!hlspv_url.IsEmpty()) {
					url = UrlDecode(UrlDecode(hlspv_url));
					url.Replace(L"\\/", L"/");
					urls.AddHead(url);

					InternetCloseHandle(hInet);
					free(data);
					return true;
				}

				// url_encoded_fmt_stream_map
				const CStringA stream_map = GetEntry(data, MATCH_STREAM_MAP_START, MATCH_END);
				if (stream_map.IsEmpty()) {
					free(data);
					InternetCloseHandle(hInet);
					return false;
				}
				// adaptive_fmts
				const CStringA adaptive_fmts = GetEntry(data, MATCH_ADAPTIVE_FMTS_START, MATCH_END);

				strUrls = stream_map;
				if (!adaptive_fmts.IsEmpty()) {
					strUrls += ',';
					strUrls += adaptive_fmts;
				}
				strUrls.Replace("\\u0026", "&");
			}

			free(data);

			YoutubeUrllist AudioList;

			CAtlList<CStringA> linesA;
			Explode(strUrls, linesA, ',');
			POSITION posLine = linesA.GetHeadPosition();

			while (posLine) {
				const CStringA &lineA = linesA.GetNext(posLine);

				int itag = 0;
				CStringA url;
				CStringA signature;

				CAtlList<CStringA> paramsA;
				Explode(lineA, paramsA, '&');

				POSITION posParam = paramsA.GetHeadPosition();
				while (posParam) {
					const CStringA &paramA = paramsA.GetNext(posParam);

					int k = paramA.Find('=');
					if (k > 0) {
						const CStringA paramHeader = paramA.Left(k);
						const CStringA paramValue = paramA.Mid(k + 1);

						if (paramHeader == "url") {
							url = UrlDecode(UrlDecode(paramValue));
						} else if (paramHeader == "s") {
							signature = paramValue;
						} else if (paramHeader == "itag") {
							if (sscanf_s(paramValue, "%d", &itag) != 1) {
								itag = 0;
							}
						}
					}
				}

				if (itag) {
					auto SignatureDecode = [&](CStringA& final_url) {
						if (!signature.IsEmpty() && !JSUrl.IsEmpty()) {
							if (!bJSParsed) {
								bJSParsed = TRUE;
								hUrl = InternetOpenUrl(hInet, JSUrl, NULL, 0, INTERNET_OPEN_FALGS, 0);
								if (hUrl) {
									char* data = NULL;
									DWORD dataSize = 0;
									InternetReadData(hUrl, &data, dataSize, NULL);
									InternetCloseHandle(hUrl);
									if (dataSize) {
										const CStringA funcName = GetEntry(data, "\"signature\",", "(");
										if (!funcName.IsEmpty()) {
											const CStringA varfunc = funcName + "=function(a){";
											const CStringA funcBody = GetEntry(data, varfunc, "};");
											if (!funcBody.IsEmpty()) {
												CStringA funcGroup;
												CAtlList<CStringA> funcList;
												CAtlList<CStringA> funcCodeList;

												CAtlList<CStringA> code;
												Explode(funcBody, code, ';');

												POSITION pos = code.GetHeadPosition();
												while (pos) {
													const CStringA &line = code.GetNext(pos);

													if (line.Find("split") >= 0 || line.Find("return") >= 0) {
														continue;
													}

													funcList.AddTail(line);

													if (funcGroup.IsEmpty()) {
														const int k = line.Find('.');
														if (k > 0) {
															funcGroup = line.Left(k);
														}
													}
												}

												if (!funcGroup.IsEmpty()) {
													CStringA tmp = "var " + funcGroup + "={";
													tmp = GetEntry(data, tmp, "};");
													if (!tmp.IsEmpty()) {
														Explode(tmp, funcCodeList, "},");
													}
												}

												if (!funcList.IsEmpty() && !funcCodeList.IsEmpty()) {
													funcGroup += '.';

													POSITION pos = funcList.GetHeadPosition();
													while (pos) {
														const CStringA& func = funcList.GetNext(pos);

														int funcArg = 0;
														const CStringA funcArgs = GetEntry(func, "(", ")");
														CAtlList<CStringA> args;
														Explode(funcArgs, args, ',');
														if (args.GetCount() >= 1) {
															CStringA& arg = args.GetTail();
															int value = 0;
															if (sscanf_s(arg, "%d", &value) == 1) {
																funcArg = value;
															}
														}

														CStringA funcName = GetEntry(func, funcGroup, "(");
														funcName += ":function";

														youtubeFuncType funcType = youtubeFuncType::funcNONE;

														POSITION pos2 = funcCodeList.GetHeadPosition();
														while (pos2) {
															const CStringA& funcCode = funcCodeList.GetNext(pos2);
															if (funcCode.Find(funcName) >= 0) {
																if (funcCode.Find("splice") > 0) {
																	funcType = youtubeFuncType::funcDELETE;
																} else if (funcCode.Find("reverse") > 0) {
																	funcType = youtubeFuncType::funcREVERSE;
																} else if (funcCode.Find(".length]") > 0) {
																	funcType = youtubeFuncType::funcSWAP;
																}
																break;
															}
														}

														if (funcType != youtubeFuncType::funcNONE) {
															JSFuncs.Add(funcType);
															JSFuncArgs.Add(funcArg);
														}
													}
												}
											}
										}

										free(data);
									}
								}
							}
						}

						if (!JSFuncs.IsEmpty()) {
							auto Delete = [](CStringA& a, int b) {
								a.Delete(0, b);
							};
							auto Swap = [](CStringA& a, int b) {
								const CHAR c = a[0];
								b %= a.GetLength();
								a.SetAt(0, a[b]);
								a.SetAt(b, c);
							};
							auto Reverse = [](CStringA& a) {
								CHAR c;
								const int len = a.GetLength();

								for (int i = 0; i < len / 2; ++i) {
									c = a[i];
									a.SetAt(i, a[len - i - 1]);
									a.SetAt(len - i - 1, c);
								}
							};

							for (size_t i = 0; i < JSFuncs.GetCount(); i++) {
								const youtubeFuncType func = JSFuncs[i];
								const int arg = JSFuncArgs[i];
								switch (func) {
									case youtubeFuncType::funcDELETE:
										Delete(signature, arg);
										break;
									case youtubeFuncType::funcSWAP:
										Swap(signature, arg);
										break;
									case youtubeFuncType::funcREVERSE:
										Reverse(signature);
										break;
								}
							}

							final_url.AppendFormat("&signature=%s", signature);
						}
					};

					SignatureDecode(url);

					if (const YoutubeProfile* profile = GetProfile(itag)) {
						YoutubeUrllistItem item;
						item.profile = profile;
						item.url = url;
						item.title.Format(L"%s %dp",
							profile->format == 1 ? L"WebM" : L"MP4",
							profile->quality);
						if (profile->fps60) {
							item.title.Append(L" 60fps");
						}
						if (profile->type == y_video) {
							item.title.Append(L" dash");
						}

						youtubeUrllist.emplace_back(item);
					}
					else if (const YoutubeProfile* audioprofile = GetAudioProfile(itag)) {
						YoutubeUrllistItem item;
						item.profile = audioprofile;
						item.url = url;

						AudioList.emplace_back(item);
					}
				}
			}

			if (youtubeUrllist.empty()) {
				return false;
			}

			std::sort(youtubeUrllist.begin(), youtubeUrllist.end(), CompareUrllistItem);
			std::sort(AudioList.begin(), AudioList.end(), CompareUrllistItem);

			const CAppSettings& s = AfxGetAppSettings();

			// select video stream
			const YoutubeUrllistItem* final_item = NULL;

			if (s.iYoutubeTagSelected) {
				for (auto item : youtubeUrllist) {
					if (s.iYoutubeTagSelected == item.profile->iTag) {
						final_item = &item;
						break;
					}
				}
			}

			if (!final_item) {
				size_t k;
				for (k = 0; k < youtubeUrllist.size(); k++) {
					if (s.YoutubeFormat.fmt == youtubeUrllist[k].profile->format) {
						final_item = &youtubeUrllist[k];
						break;
					}
				}
				if (!final_item) {
					final_item = &youtubeUrllist[0];
					k = 0;
					DLog(L"YouTube::Parse_URL() : %s format not found, used %s", s.YoutubeFormat.fmt == 1 ? L"WebM" : L"MP4", final_item->profile->format == y_webm ? L"WebM" : L"MP4");
				}

				for (size_t i = k + 1; i < youtubeUrllist.size(); i++) {
					auto profile = youtubeUrllist[i].profile;

					if (final_item->profile->format == profile->format) {
						if (profile->quality == final_item->profile->quality && profile->fps60 != s.YoutubeFormat.fps60) {
							// same resolution as that of the previous, but not suitable fps
							continue;
						}

						if (profile->quality < final_item->profile->quality && final_item->profile->quality <= s.YoutubeFormat.res) {
							break;
						}

						final_item = &youtubeUrllist[i];
					}
				}
			}

			CString final_video_url = final_item->url;
			const YoutubeProfile* final_video_profile = final_item->profile;

			CString final_audio_url;
			if (final_item->profile->type == y_video && !AudioList.empty()) {
				int fmt = final_item->profile->format;
				final_item = NULL;

				// select audio stream
				for (auto item : AudioList) {
					if (fmt == item.profile->format) {
						final_item = &item;
						break;
					}
				}
				if (!final_item) {
					final_item = &AudioList[0];
				}

				final_audio_url = final_item->url;
			}

			if (!final_audio_url.IsEmpty()) {
				final_audio_url.Replace(L"http://", L"https://");
			}

			if (!final_video_url.IsEmpty()) {
				final_video_url.Replace(L"http://", L"https://");

				ParseMetadata(hInet, videoId, y_fields);

				y_fields.fname.Format(L"%s.%dp.%s", y_fields.title, final_video_profile->quality, final_video_profile->ext);
				FixFilename(y_fields.fname);

				if (!videoId.IsEmpty()) { // subtitle
					CString link;
					link.Format(L"https://video.google.com/timedtext?hl=en&type=list&v=%s", videoId);
					hUrl = InternetOpenUrl(hInet, link, NULL, 0, INTERNET_OPEN_FALGS, 0);
					if (hUrl) {
						char* data = NULL;
						DWORD dataSize = 0;
						InternetReadData(hUrl, &data, dataSize, NULL);
						InternetCloseHandle(hUrl);

						if (dataSize) {
							CString xml = UTF8To16(data);
							free(data);
							const std::wregex regex(L"<track id(.*?)/>");
							std::wcmatch match;
							LPCTSTR text = xml.GetBuffer();
							while (std::regex_search(text, match, regex)) {
								if (match.size() == 2) {
									CString url, name;
									CString xmlElement = CString(match[1].first, match[1].length());

									const std::wregex regexValues(L"([a-z_]+)=\"([^\"]+)\"");
									std::wcmatch matchValues;
									LPCTSTR textValues = xmlElement.GetBuffer();
									while (std::regex_search(textValues, matchValues, regexValues)) {
										if (matchValues.size() == 3) {
											const CString xmlHeader = CString(matchValues[1].first, matchValues[1].length());
											const CString xmlValue = CString(matchValues[2].first, matchValues[2].length());

											if (xmlHeader == L"lang_code") {
												url.Format(L"https://www.youtube.com/api/timedtext?lang=%s&v=%s&fmt=vtt", xmlValue, videoId);
											} else if (xmlHeader == L"lang_original") {
												name = xmlValue;
											}
										}

										textValues = matchValues[0].second;
									}

									if (!url.IsEmpty() && !name.IsEmpty()) {
										subs.AddTail(CSubtitleItem(url, name));
									}
								}

								text = match[0].second;
							}
						}
					}
				}

				InternetCloseHandle(hInet);

				if (!final_video_url.IsEmpty()) {
					urls.AddHead(final_video_url);

					if (!final_audio_url.IsEmpty()) {
						urls.AddTail(final_audio_url);
					}
				}

				return !urls.IsEmpty();
			}
		}

		return false;
	}

	bool Parse_Playlist(CString url, YoutubePlaylist& youtubePlaylist, int& idx_CurrentPlay)
	{
		idx_CurrentPlay = 0;
		if (CheckPlaylist(url)) {

			char* data = NULL;
			DWORD dataSize = 0;

			HINTERNET hUrl;
			HINTERNET hInet = InternetOpen(L"Googlebot", 0, NULL, NULL, 0);
			if (hInet) {
				HandleURL(url);
				hUrl = InternetOpenUrl(hInet, url, NULL, 0, INTERNET_OPEN_FALGS, 0);
				if (hUrl) {
					InternetReadData(hUrl, &data, dataSize, "id=\"footer\"");
					InternetCloseHandle(hUrl);
				}
				InternetCloseHandle(hInet);
			}

			if (!data) {
				return false;
			}

			LPCSTR sMatch = NULL;
			if (strstr(data, MATCH_PLAYLIST_ITEM_START)) {
				sMatch = MATCH_PLAYLIST_ITEM_START;
			} else if (strstr(data, MATCH_PLAYLIST_ITEM_START2)) {
				sMatch = MATCH_PLAYLIST_ITEM_START2;
			} else {
				free(data);
				return false;
			}

			LPCSTR block = data;
			while ((block = strstr(block, sMatch)) != NULL) {
				const CStringA blockEntry = GetEntry(block, sMatch, ">");
				if (blockEntry.IsEmpty()) {
					break;
				}
				block += blockEntry.GetLength();

				CString item = UTF8To16(blockEntry);
				CString data_video_id;
				int data_index = 0;
				CString data_video_title;

				bool bCurrentPlay = (item.Find(L"currently-playing") != -1);

				const std::wregex regex(L"([a-z-]+)=\"([^\"]+)\"");
				std::wcmatch match;
				LPCTSTR text = item.GetBuffer();
				while (std::regex_search(text, match, regex)) {
					if (match.size() == 3) {
						const CString propHeader = CString(match[1].first, match[1].length());
						const CString propValue = CString(match[2].first, match[2].length());

						// data-video-id, data-video-clip-end, data-index, data-video-username, data-video-title, data-video-clip-start.
						if (propHeader == L"data-video-id") {
							data_video_id = propValue;
						} else if (propHeader == L"data-index") {
							data_index = _wtoi(propValue);
						} else if (propHeader == L"data-video-title" || propHeader == L"data-title") {
							data_video_title = FixHtmlSymbols(propValue);
						}
					}

					text = match[0].second;
				}

				if (!data_video_id.IsEmpty()) {
					CString url;
					url.Format(L"https://www.youtube.com/watch?v=%s", data_video_id);
					YoutubePlaylistItem item(url, data_video_title);
					youtubePlaylist.emplace_back(item);

					if (bCurrentPlay) {
						idx_CurrentPlay = youtubePlaylist.size() - 1;
					}
				}
			}

			free(data);

			if (!youtubePlaylist.empty()) {
				return true;
			}
		}

		return false;
	}

	bool Parse_URL(CString url, YoutubeFields& y_fields)
	{
		bool bRet = false;
		if (CheckURL(url)) {
			HINTERNET hInet = InternetOpen(L"Googlebot", 0, NULL, NULL, 0);
			if (hInet) {
				HandleURL(url);

				CString videoId;
				for (int i = 0; i < _countof(videoIdRegExps) && videoId.IsEmpty(); i++) {
					videoId = RegExpParse(url, videoIdRegExps[i]);
				}

				bRet = ParseMetadata(hInet, videoId, y_fields);

				InternetCloseHandle(hInet);
			}
		}

		return bRet;
	}
}

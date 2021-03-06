/*
 * (C) 2006-2016 see Authors.txt
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

#define HAVE_AV_CONFIG_H

#include <Windows.h>
#include <time.h> // for the _time64 workaround
#include <mpc_defines.h>
#include "../../../DSUtil/SysVersion.h"
#include "ffmpegContext.h"

extern "C" {
	#include <ffmpeg/libavcodec/avcodec.h>
// This is kind of an hack but it avoids using a C++ keyword as a struct member name
#define class classFFMPEG
	#include <ffmpeg/libavcodec/mpegvideo.h>
	#include <ffmpeg/libavcodec/h264dec.h>
#undef class
	#include <ffmpeg/libavcodec/dxva_h264.h>
	#include <ffmpeg/libavcodec/dxva_hevc.h>
	#include <ffmpeg/libavcodec/dxva_mpeg2.h>
	#include <ffmpeg/libavcodec/dxva_vc1.h>
	#include <ffmpeg/libavcodec/dxva_vp9.h>
}

static const WORD PCID_ATI_UVD [] = {
	0x94C7, // ATI Radeon HD 2350
	0x94C1, // ATI Radeon HD 2400 XT
	0x94CC, // ATI Radeon HD 2400 Series
	0x958A, // ATI Radeon HD 2600 X2 Series
	0x9588, // ATI Radeon HD 2600 XT
	0x9405, // ATI Radeon HD 2900 GT
	0x9400, // ATI Radeon HD 2900 XT
	0x9611, // ATI Radeon 3100 Graphics
	0x9610, // ATI Radeon HD 3200 Graphics
	0x9614, // ATI Radeon HD 3300 Graphics
	0x95C0, // ATI Radeon HD 3400 Series (and others)
	0x95C5, // ATI Radeon HD 3400 Series (and others)
	0x95C4, // ATI Radeon HD 3400 Series (and others)
	0x94C3, // ATI Radeon HD 3410
	0x9589, // ATI Radeon HD 3600 Series (and others)
	0x9598, // ATI Radeon HD 3600 Series (and others)
	0x9591, // ATI Radeon HD 3600 Series (and others)
	0x9501, // ATI Radeon HD 3800 Series (and others)
	0x9505, // ATI Radeon HD 3800 Series (and others)
	0x9507, // ATI Radeon HD 3830
	0x9513, // ATI Radeon HD 3850 X2
	0x9515, // ATI Radeon HD 3850 AGP
	0x950F, // ATI Radeon HD 3850 X2
};

static const WORD PCID_AMD_4K [] = { // not tested
	// http://pci-ids.ucw.cz/read/PC/1002
	// UVD ?
	0x67C0, // Polaris10 (Ellesmere)
	0x67DF, // Radeon RX 470/480 (Ellesmere)
	0X67E0, // Polaris11 (Baffin)
	0X67E1, // Polaris11 (Baffin)
	0X67E8, // Polaris11 (Baffin)
	0X67E9, // Polaris11 (Baffin)
	0X67EB, // Polaris11 (Baffin)
	0X67EF, // Radeon RX 460 (Baffin)
	0X67FF, // Polaris11 (Baffin)
	// UVD 5
	0x6939, // Radeon R9 285/380 (Tonga PRO)
	// UVD 6
	0x7300, // Radeon R9 FURY/NANO Series (Fiji)
	0x9874, // Carrizo
};

static const WORD PCID_INTEL_4K [] = {
	// IvyBridge
	0x0152, // HD Graphics 2500        (4k tested)
	0x0156, // HD Graphics 2500 Mobile
	0x015A, // HD Graphics P2500
	0x0162, // HD Graphics 4000        (fully tested)
	0x0166, // HD Graphics 4000 Mobile
	0x016A, // HD Graphics P4000
	// Haswell
	0x0402, // HD Graphics
	0x0406, // HD Graphics Mobile
	0x040A, // HD Graphics
	0x0412, // HD Graphics 4600
	0x0416, // HD Graphics 4600 Mobile
	0x041A, // HD Graphics P4600/P4700
	0x041E, // HD Graphics 4400"
	0x0A06, // HD Graphics
	0x0A0E, // HD Graphics
	0x0A16, // HD Graphics Family
	0x0A1E, // HD Graphics Family
	0x0A26, // HD Graphics 5000
	0x0A2E, // Iris Graphics 5100
	0x0D22, // Iris Pro Graphics 5200
	0x0D26, // Iris Pro Graphics 5200
	// Atom Z3700 Series
	0x0F31, // HD Graphics
	// Broadwell
	0x1606, // HD Graphics
	0x160E, // HD Graphics
	0x1612, // HD Graphics 5600
	0x1616, // HD Graphics 5500
	0x161A, // HD Graphics P5700
	0x161E, // HD Graphics 5300
	0x1622, // Iris Pro Graphics 6200
	0x1626, // HD Graphics 6000
	0x162A, // Iris Pro Graphics P6300
	0x162B, // Iris Graphics 6100
	// Skylake
	0x1902, // HD Graphics 510
	0x1906, // HD Graphics 510
	0x190B, // HD Graphics 510
	0x1912, // HD Graphics 530
	0x1916, // HD Graphics 520
	0x191B, // HD Graphics 530
	0x191D, // HD Graphics P530
	0x191E, // HD Graphics 515
	0x1921, // HD Graphics 520
	0x1923, // HD Graphics 535
	0x1926, // Iris Graphics 540
	0x1927, // Iris Graphics 550
	0x192B, // Iris Graphics 555
	0x192D, // Iris Graphics P555
	0x193A, // Iris Pro Graphics P580
	0x193B, // Iris Pro Graphics 580
	0x193D, // Iris Pro Graphics P580
	// Atom Z8000 Series
	0x22B0, // HD Graphics
	// Braswell
	0x22B1, // HD Graphics
};

static bool CheckPCID(DWORD pcid, const WORD* pPCIDs, size_t count)
{
	WORD wPCID = (WORD)pcid;
	for (size_t i = 0; i < count; i++) {
		if (wPCID == pPCIDs[i]) {
			return true;
		}
	}

	return false;
}

// returns 'true' if version is equal to or higher than A.B.C.D, returns 'false' otherwise
static bool DriverVersionCheck(UINT64 VideoDriverVersion, UINT16 A, UINT16 B, UINT16 C, UINT16 D)
{
	LONGLONG ver = (UINT64)A << 48 | (UINT64)B << 32 | (UINT64)C << 16 | (UINT64)D;

	return VideoDriverVersion >= ver;
}

static UINT64 GetFileVersion(LPCWSTR wstrFilename)
{
	UINT64 ret = 0;

	const DWORD len = GetFileVersionInfoSizeW(wstrFilename, NULL);
	if (len) {
		WCHAR* buf = new WCHAR[len];
		if (buf) {
			UINT uLen;
			VS_FIXEDFILEINFO* pvsf = NULL;
			if (GetFileVersionInfoW(wstrFilename, 0, len, buf) && VerQueryValueW(buf, L"\\", (LPVOID*)&pvsf, &uLen)) {
				ret = ((UINT64)pvsf->dwFileVersionMS << 32) | pvsf->dwFileVersionLS;
			}

			delete [] buf;
		}
	}

	return ret;
}

// === H264 functions

int FFH264CheckCompatibility(int nWidth, int nHeight, struct AVCodecContext* pAVCtx,
							 DWORD nPCIVendor, DWORD nPCIDevice, UINT64 VideoDriverVersion)
{
	const H264Context* h           = (H264Context*)pAVCtx->priv_data;
	const SPS* sps                 = h264_getSPS(h);

	int video_is_level51           = 0;
	int no_level51_support         = 1;
	int too_much_ref_frames        = 0;
	const int max_ref_frames_dpb41 = min(11, 8388608/(nWidth * nHeight));

	if (sps) {
		if (sps->bit_depth_luma > 8 || sps->chroma_format_idc > 1) {
			return DXVA_HIGH_BIT;
		}

		if (sps->profile_idc > 100) {
			return DXVA_PROFILE_HIGHER_THAN_HIGH;
		}

		video_is_level51   = sps->level_idc >= 51 ? 1 : 0;
		int max_ref_frames = max_ref_frames_dpb41; // default value is calculate

		if (nPCIVendor == PCIV_nVidia) {
			// nVidia cards support level 5.1 since drivers v7.15.11.7800 for Vista/7
			if (DriverVersionCheck(VideoDriverVersion, 7, 15, 11, 7800)) {
				no_level51_support = 0;
				max_ref_frames = 16;
			}
		} else if (nPCIVendor == PCIV_ATI && !CheckPCID(nPCIDevice, PCID_ATI_UVD, _countof(PCID_ATI_UVD))) {
			TCHAR path[MAX_PATH] = { 0 };
			GetSystemDirectory(path, MAX_PATH);
			wcscat(path, L"\\drivers\\atikmdag.sys\0");
			UINT64 atikmdag_ver = GetFileVersion(path);

			if (atikmdag_ver) {
				// file version 8.1.1.1016 - Catalyst 10.4, WinVista & Win7
				if (DriverVersionCheck(atikmdag_ver, 8, 1, 1, 1016)) {
					no_level51_support = 0;
					max_ref_frames = 16;
				}
			} else {
				// driver version 8.14.1.6105 - Catalyst 10.4; TODO - verify this information
				if (DriverVersionCheck(VideoDriverVersion, 8, 14, 1, 6105)) {
					no_level51_support = 0;
					max_ref_frames = 16;
				}
			}
		} else if (nPCIVendor == PCIV_S3_Graphics || nPCIVendor == PCIV_Intel) {
			no_level51_support = 0;
			max_ref_frames = 16;
		}

		// Check maximum allowed number reference frames
		if (sps->ref_frame_count > max_ref_frames) {
			too_much_ref_frames = 1;
		}
	}

	int Flags = 0;
	if (video_is_level51 * no_level51_support) {
		Flags |= DXVA_UNSUPPORTED_LEVEL;
	}
	if (too_much_ref_frames) {
		Flags |= DXVA_TOO_MANY_REF_FRAMES;
	}

	return Flags;
}

// === Mpeg2 functions
int MPEG2CheckCompatibility(struct AVCodecContext* pAVCtx)
{
	return (((MpegEncContext*)pAVCtx->priv_data)->chroma_format < 2);
}

// === Common functions
HRESULT FFGetCurFrame(struct AVCodecContext* pAVCtx, AVFrame** ppFrameOut)
{
	*ppFrameOut = NULL;
	switch (pAVCtx->codec_id) {
		case AV_CODEC_ID_H264:
			h264_getcurframe(pAVCtx, ppFrameOut);
			break;
		case AV_CODEC_ID_HEVC:
			hevc_getcurframe(pAVCtx, ppFrameOut);
			break;
		case AV_CODEC_ID_MPEG2VIDEO:
			mpeg2_getcurframe(pAVCtx, ppFrameOut);
			break;
		case AV_CODEC_ID_VC1:
		case AV_CODEC_ID_WMV3:
			vc1_getcurframe(pAVCtx, ppFrameOut);
			break;
		case AV_CODEC_ID_VP9:
			vp9_getcurframe(pAVCtx, ppFrameOut);
			break;
		default:
			return E_FAIL;
	}

	return S_OK;
}

UINT FFGetMBCount(struct AVCodecContext* pAVCtx)
{
	UINT MBCount = 0;
	switch (pAVCtx->codec_id) {
		case AV_CODEC_ID_H264 :
			{
				const H264Context* h    = (H264Context*)pAVCtx->priv_data;
				MBCount                 = h->mb_width * h->mb_height;
			}
			break;
		case AV_CODEC_ID_MPEG2VIDEO:
			{
				const MpegEncContext* s = (MpegEncContext*)pAVCtx->priv_data;
				const int is_field      = s->picture_structure != PICT_FRAME;
				MBCount                 = s->mb_width * (s->mb_height >> is_field);
			}
			break;
	}

	return MBCount;
}

void FillAVCodecProps(struct AVCodecContext* pAVCtx)
{
	// fill "Bitstream height" properties
	if (pAVCtx->codec_id == AV_CODEC_ID_H264) {
		const H264Context* h = (H264Context*)pAVCtx->priv_data;
		const SPS* sps       = h264_getSPS(h);
		if (sps && sps->mb_height > 0) {
			pAVCtx->coded_height = sps->mb_height * (2 - sps->frame_mbs_only_flag) * 16;
		}
	}

	// fill "Pixel format" properties
	if (pAVCtx->pix_fmt == AV_PIX_FMT_NONE) {
		switch (pAVCtx->codec_id) {
			case AV_CODEC_ID_MPEG1VIDEO:
			case AV_CODEC_ID_MPEG2VIDEO:
				{
					const MpegEncContext* s = (MpegEncContext*)pAVCtx->priv_data;
					if (s->chroma_format < 2) {
						pAVCtx->pix_fmt = AV_PIX_FMT_YUV420P;
					} else if(s->chroma_format == 2) {
						pAVCtx->pix_fmt = AV_PIX_FMT_YUV422P;
					} else {
						pAVCtx->pix_fmt = AV_PIX_FMT_YUV444P;
					}
				}
				break;
			case AV_CODEC_ID_LAGARITH:
				if (pAVCtx->extradata_size >= 4) {
					switch (*(DWORD*)pAVCtx->extradata) {
					case 0:
						if (pAVCtx->bits_per_coded_sample == 32) {
							pAVCtx->pix_fmt = AV_PIX_FMT_RGBA;
						} else if (pAVCtx->bits_per_coded_sample == 24) {
							pAVCtx->pix_fmt = AV_PIX_FMT_RGB24;
						}
						break;
					case 1:
						pAVCtx->pix_fmt = AV_PIX_FMT_YUV422P;
						break;
					case 2:
						pAVCtx->pix_fmt = AV_PIX_FMT_YUV420P;
						break;
					}
				}
				break;
			case AV_CODEC_ID_PRORES:
				if (pAVCtx->extradata_size >= 8) {
					switch (*(DWORD*)(pAVCtx->extradata + 4)) {
					case 'hcpa': // Apple ProRes 422 High Quality
					case 'ncpa': // Apple ProRes 422 Standard Definition
					case 'scpa': // Apple ProRes 422 LT
					case 'ocpa': // Apple ProRes 422 Proxy
						pAVCtx->pix_fmt = AV_PIX_FMT_YUV422P10LE;
						break;
					case 'h4pa': // Apple ProRes 4444
						pAVCtx->pix_fmt = pAVCtx->bits_per_coded_sample == 32 ? AV_PIX_FMT_YUVA444P10LE : AV_PIX_FMT_YUV444P10LE;
						break;
					}
				}
				break;
			case AV_CODEC_ID_PNG:
				pAVCtx->pix_fmt = AV_PIX_FMT_RGBA; // and other RGB formats, but it is not important here
				break;
			case AV_CODEC_ID_HQ_HQA:
				pAVCtx->pix_fmt = AV_PIX_FMT_YUV422P;
				break;
			case AV_CODEC_ID_HQX:
				pAVCtx->pix_fmt = AV_PIX_FMT_YUV422P16; // or AV_PIX_FMT_YUV444P16
				break;
			case AV_CODEC_ID_CFHD:
				pAVCtx->pix_fmt = AV_PIX_FMT_YUV422P10; // most common format
				break;
			case AV_CODEC_ID_MJPEG:
			case AV_CODEC_ID_DNXHD:
				pAVCtx->pix_fmt = AV_PIX_FMT_YUV422P; // most common format
				break;
			case AV_CODEC_ID_MAGICYUV:
				if (pAVCtx->extradata_size >= 32 && *(DWORD*)pAVCtx->extradata == FCC('MAGY')) {
					int hsize = *(DWORD*)(pAVCtx->extradata + 4);
					if (hsize >= 32 && pAVCtx->extradata[8] == 7) {
						switch (pAVCtx->extradata[9]) {
						case 0x65: pAVCtx->pix_fmt = AV_PIX_FMT_GBRP;     break;
						case 0x66: pAVCtx->pix_fmt = AV_PIX_FMT_GBRAP;    break;
						case 0x67: pAVCtx->pix_fmt = AV_PIX_FMT_YUV444P;  break;
						case 0x68: pAVCtx->pix_fmt = AV_PIX_FMT_YUV422P;  break;
						case 0x69: pAVCtx->pix_fmt = AV_PIX_FMT_YUV420P;  break;
						case 0x6a: pAVCtx->pix_fmt = AV_PIX_FMT_YUVA444P; break;
						case 0x6b: pAVCtx->pix_fmt = AV_PIX_FMT_GRAY8;    break;
						}
					}
				}
				else if (pAVCtx->extradata_size >= 8) {
					switch (*(DWORD*)(pAVCtx->extradata + 4)) {
					case FCC('M8RG'): pAVCtx->pix_fmt = AV_PIX_FMT_GBRP;     break;
					case FCC('M8RA'): pAVCtx->pix_fmt = AV_PIX_FMT_GBRAP;    break;
					case FCC('M8Y4'): pAVCtx->pix_fmt = AV_PIX_FMT_YUV444P;  break;
					case FCC('M8Y2'): pAVCtx->pix_fmt = AV_PIX_FMT_YUV422P;  break;
					case FCC('M8Y0'): pAVCtx->pix_fmt = AV_PIX_FMT_YUV420P;  break;
					case FCC('M8YA'): pAVCtx->pix_fmt = AV_PIX_FMT_YUVA444P; break;
					case FCC('M8G0'): pAVCtx->pix_fmt = AV_PIX_FMT_GRAY8;    break;
					}
				}
				break;
			default: // most codecs
				pAVCtx->pix_fmt = AV_PIX_FMT_YUV420P; // most common format
		}
	}
}

bool IsATIUVD(DWORD nPCIVendor, DWORD nPCIDevice)
{
	return (nPCIVendor == PCIV_ATI && CheckPCID(nPCIDevice, PCID_ATI_UVD, _countof(PCID_ATI_UVD)));
}

#define CHECK_AVC_L52_SIZE(w, h) ((w) <= 4096 && (h) <= 4096 && (w) * (h) <= 36864 * 16 * 16)

BOOL DXVACheckFramesize(enum AVCodecID nCodecId, int width, int height, DWORD nPCIVendor, DWORD nPCIDevice, UINT64 VideoDriverVersion)
{
	width = (width + 15) & ~15; // (width + 15) / 16 * 16;
	height = (height + 15) & ~15; // (height + 15) / 16 * 16;

	if (nPCIVendor == PCIV_nVidia) {
		if (DriverVersionCheck(VideoDriverVersion, 9, 18, 13, 2018)) {
			// For Nvidia graphics cards with support for 4k, you must install the driver v320.18 or newer.
			return TRUE;
		}
		if (width <= 2032 && height <= 2032 && width * height <= 8190 * 16 * 16) {
			// tested H.264, VC-1 and MPEG-2 on VP4 (feature set C) (G210M, GT220)
			return TRUE;
		}
	}
	else if (nPCIVendor == PCIV_ATI) {
		// UVD 5, UVD 6
		if (CheckPCID(nPCIDevice, PCID_AMD_4K, _countof(PCID_AMD_4K)) && CHECK_AVC_L52_SIZE(width, height)) {
			// not tested
			return TRUE;
		}
		if (width <= 2048 && height <= 2304 && width * height <= 2048 * 2048) {
			// tested H.264 on UVD 2.2 (HD5670, HD5770, HD5850)
			// it may also work if width = 2064, but unstable
			return TRUE;
		}
	}
	else if (nPCIVendor == PCIV_Intel && DriverVersionCheck(VideoDriverVersion, 10, 18, 10, 4061)) {
		// For Intel graphics cards with support for 4k, you must install the driver v15.33.32.4061 or newer.
		return TRUE;
	}
	else if (width <= 1920 && height <= 1088) {
		return TRUE;
	}

	return FALSE;
}

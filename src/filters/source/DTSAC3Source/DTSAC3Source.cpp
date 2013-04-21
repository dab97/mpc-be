/*
 * $Id$
 *
 * (C) 2003-2006 Gabest
 * (C) 2006-2013 see Authors.txt
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
#include <MMReg.h>
#include <ks.h>
#ifdef REGISTER_FILTER
#include <InitGuid.h>
#endif
#include <uuids.h>
#include "moreuuids.h"
#include "DTSAC3Source.h"
#include "../../../DSUtil/AudioParser.h"
#include <atlpath.h>
// TODO: remove this when it's fixed in MSVC
// Work around warning C4005: 'XXXX' : macro redefinition
#pragma warning(push)
#pragma warning(disable: 4005)
#include <stdint.h>
#pragma warning(pop)

enum {
	unknown,
	AC3,
	EAC3,
	//TrueHD,
	//TrueHDAC3,
	MLP,
	DTS,
	DTSHD,
	DTSPaded,
	SPDIF_AC3,
};

bool isDTSSync(const DWORD sync)
{
	if (sync == 0x0180fe7f || // '7FFE8001' 16 bits and big endian bitstream
		sync == 0x80017ffe || // 'FE7F0180' 16 bits and little endian bitstream
		sync == 0x00e8ff1f || // '1FFFE800' 14 bits and big endian bitstream
		sync == 0xe8001fff) { // 'FF1F00E8' 14 bits and little endian bitstream
			return true;
	} else {
		return false;
}
}

DWORD ParseWAVECDHeader(const BYTE wh[44])
{
	if (*(DWORD*)wh != 0x46464952 //"RIFF"
	 || *(DWORDLONG*)(wh+8) != 0x20746d6645564157 //"WAVEfmt "
	 || *(DWORD*)(wh+36) != 0x61746164) { //"data"
		return 0;
	}
	PCMWAVEFORMAT pcmwf = *(PCMWAVEFORMAT*)(wh+20);
	if (pcmwf.wf.wFormatTag != 1
	 || pcmwf.wf.nChannels != 2
	 || pcmwf.wf.nSamplesPerSec != 44100 && pcmwf.wf.nSamplesPerSec != 48000
	 || pcmwf.wf.nBlockAlign != 4
	 || pcmwf.wf.nAvgBytesPerSec != pcmwf.wf.nSamplesPerSec * pcmwf.wf.nBlockAlign
	 || pcmwf.wBitsPerSample != 16) {
		return 0;
	}
	return *(DWORD*)(wh+40); //return size of "data"
}

int ParseAC3IEC61937Header(const BYTE *buf)
{
	WORD* wbuf = (WORD*)buf;
	if (*(DWORD*)buf == IEC61937_SYNC_WORD
	   && wbuf[2] ==  0x0001
	   && wbuf[3] > 0 && wbuf[3] < (6144-8)*8
	   && wbuf[4] == 0x0B77 ) {
		return 6144;
	}
	return 0;
}

#ifdef REGISTER_FILTER

const AMOVIESETUP_MEDIATYPE sudPinTypesOut[] = {
	{&MEDIATYPE_Audio, &MEDIASUBTYPE_DTS},
	{&MEDIATYPE_DVD_ENCRYPTED_PACK, &MEDIASUBTYPE_DTS},
	{&MEDIATYPE_Audio, &MEDIASUBTYPE_DOLBY_AC3},
	{&MEDIATYPE_DVD_ENCRYPTED_PACK, &MEDIASUBTYPE_DOLBY_AC3},
};

const AMOVIESETUP_PIN sudOpPin[] = {
	{L"Output", FALSE, TRUE, FALSE, FALSE, &CLSID_NULL, NULL, _countof(sudPinTypesOut), sudPinTypesOut}
};

const AMOVIESETUP_FILTER sudFilter[] = {
	{&__uuidof(CDTSAC3Source), DTSAC3SourceName, MERIT_NORMAL, _countof(sudOpPin), sudOpPin, CLSID_LegacyAmFilterCategory}
};

CFactoryTemplate g_Templates[] = {
	{sudFilter[0].strName, sudFilter[0].clsID, CreateInstance<CDTSAC3Source>, NULL, &sudFilter[0]}
};

int g_cTemplates = _countof(g_Templates);

STDAPI DllRegisterServer()
{
	SetRegKeyValue(
		_T("Media Type\\{e436eb83-524f-11ce-9f53-0020af0ba770}"), _T("{B4A7BE85-551D-4594-BDC7-832B09185041}"),
		_T("0"), _T("0,4,,7FFE8001")); // DTS

	SetRegKeyValue(
		_T("Media Type\\{e436eb83-524f-11ce-9f53-0020af0ba770}"), _T("{B4A7BE85-551D-4594-BDC7-832B09185041}"),
		_T("0"), _T("0,4,,fE7f0180")); // DTS LE

	SetRegKeyValue(
		_T("Media Type\\{e436eb83-524f-11ce-9f53-0020af0ba770}"), _T("{B4A7BE85-551D-4594-BDC7-832B09185041}"),
		_T("1"), _T("0,2,,0B77")); // AC3, E-AC3

	SetRegKeyValue(
		_T("Media Type\\{e436eb83-524f-11ce-9f53-0020af0ba770}"), _T("{B4A7BE85-551D-4594-BDC7-832B09185041}"),
		_T("2"), _T("0,16,,52494646xxxx57415645666D7420")); // RIFFxxxxWAVEfmt_ for DTSWAV

	SetRegKeyValue(
		_T("Media Type\\{e436eb83-524f-11ce-9f53-0020af0ba770}"), _T("{B4A7BE85-551D-4594-BDC7-832B09185041}"),
		_T("0"), _T("4,4,,F8726FBB")); // MLP

	SetRegKeyValue(
		_T("Media Type\\{e436eb83-524f-11ce-9f53-0020af0ba770}"), _T("{B4A7BE85-551D-4594-BDC7-832B09185041}"),
		_T("Source Filter"), _T("{B4A7BE85-551D-4594-BDC7-832B09185041}"));

	SetRegKeyValue(
		_T("Media Type\\Extensions"), _T(".dts"),
		_T("Source Filter"), _T("{B4A7BE85-551D-4594-BDC7-832B09185041}"));

	SetRegKeyValue(
		_T("Media Type\\Extensions"), _T(".ac3"),
		_T("Source Filter"), _T("{B4A7BE85-551D-4594-BDC7-832B09185041}"));

	SetRegKeyValue(
		_T("Media Type\\Extensions"), _T(".eac3"),
		_T("Source Filter"), _T("{B4A7BE85-551D-4594-BDC7-832B09185041}"));

	return AMovieDllRegisterServer2(TRUE);
}

STDAPI DllUnregisterServer()
{
	DeleteRegKey(_T("Media Type\\{e436eb83-524f-11ce-9f53-0020af0ba770}"), _T("{B4A7BE85-551D-4594-BDC7-832B09185041}"));
	DeleteRegKey(_T("Media Type\\Extensions"), _T(".dts"));
	DeleteRegKey(_T("Media Type\\Extensions"), _T(".ac3"));
	DeleteRegKey(_T("Media Type\\Extensions"), _T(".eac3"));

	return AMovieDllRegisterServer2(FALSE);
}

#include "../../core/FilterApp.h"

CFilterApp theApp;

#endif

//
// CDTSAC3Source
//

CDTSAC3Source::CDTSAC3Source(LPUNKNOWN lpunk, HRESULT* phr)
	: CBaseSource<CDTSAC3Stream>(NAME("CDTSAC3Source"), lpunk, phr, __uuidof(this))
{
}

CDTSAC3Source::~CDTSAC3Source()
{
}

STDMETHODIMP CDTSAC3Source::QueryFilterInfo(FILTER_INFO* pInfo)
{
	CheckPointer(pInfo, E_POINTER);
	ValidateReadWritePtr(pInfo, sizeof(FILTER_INFO));
	wcscpy_s(pInfo->achName, DTSAC3SourceName);
	pInfo->pGraph = m_pGraph;
	if (m_pGraph) {
		m_pGraph->AddRef();
	}

	return S_OK;
}

// CDTSAC3Stream

CDTSAC3Stream::CDTSAC3Stream(const WCHAR* wfn, CSource* pParent, HRESULT* phr)
	: CBaseStream(NAME("CDTSAC3Stream"), pParent, phr)
	, m_dataOffset(0)
	, m_subtype(GUID_NULL)
	, m_wFormatTag(WAVE_FORMAT_UNKNOWN)
	, m_channels(0)
	, m_samplerate(0)
	, m_bitrate(0)
	, m_framesize(0)
	, m_fixedframesize(true)
	, m_framelength(0)
	, m_bitdepth(0)
	, m_streamtype(unknown)
{
	CAutoLock cAutoLock(&m_cSharedState);
	CString fn(wfn);
	CFileException	ex;
	HRESULT hr = E_FAIL;
	m_AvgTimePerFrame = 0;

	bool waveheader = false;

	do {
		if (!m_file.Open(fn, CFile::modeRead|CFile::shareDenyNone, &ex)) {
			hr = AmHresultFromWin32 (ex.m_lOsError);
			break;
		}
		const CString path = m_file.GetFilePath();
		const CString ext = CPath(m_file.GetFileName()).GetExtension().MakeLower();

		DWORD id = 0;
		DWORD id2 = 0;
		if (m_file.Read(&id, sizeof(id)) != sizeof(id) ||
			m_file.Read(&id2, sizeof(id2)) != sizeof(id2)) {
			break;
		}

		// WAVE-CD header
		if (id == RIFF_DWORD) {
			if (ext != _T(".dtswav") && ext != _T(".dts") && ext != _T(".wav")) { //check only specific extensions
				break;
			}
			BYTE buf[44];
			m_file.SeekToBegin();
			if (m_file.Read(&buf, 44) != 44
			 || ParseWAVECDHeader(buf) == 0
			 || m_file.Read(&id, sizeof(id)) != sizeof(id)) {
				break;
			}
			waveheader = true;
		}

		m_dataOffset = m_file.GetPosition() - sizeof(id) - (!waveheader ? sizeof(id2) : 0);

		bool isFound = isDTSSync(id) || (WORD)id==AC3_SYNC_WORD || id==IEC61937_SYNC_WORD || id2==MLP_SYNC_WORD /*|| id2!=TRUEHD_SYNC_WORD*/;

		// search DTS and AC3 headers (skip garbage in the beginning)
		if (!isFound) {
			UINT buflen = 0;
			if (waveheader && ext == _T(".wav") && PathFileExists(path.Left(path.GetLength() - ext.GetLength()) + _T(".cue"))) {
				buflen = 64 * 1024; // for .wav+.cue sometimes need to use a more deep search
			} else if (ext == _T(".dtswav") || ext == _T(".dts") || ext == _T(".wav") || ext == _T(".ac3") || ext == _T(".eac3")) { //check only specific extensions
				buflen = 6 * 1024;
			} else {
				break;
			}

			if (m_dataOffset < buflen) {
				buflen -= (UINT)m_dataOffset;    // tiny optimization
			}
			m_file.Seek(m_dataOffset, CFile::begin);

			BYTE* buf = DNew BYTE[buflen];
			UINT len = m_file.Read(buf, buflen);
			if (len < 100) {
				break;    // file is very small
			}

			for (UINT i = 1; i + 8 < len; i++) { // looking for DTS or AC3 sync
				id = *(DWORD*)(buf + i);
				if (isDTSSync(id) || (WORD)id == AC3_SYNC_WORD && (GetAC3FrameSize(buf + i) > 0 || GetEAC3FrameSize(buf + i) > 0)) {
					isFound = true;
					m_dataOffset += i;
					break;
				}
			}
			delete [] buf;
		}
		if (!isFound) {
			break;
		}

		m_file.Seek(m_dataOffset, CFile::begin);

		// DTS & DTS-HD
		if (isDTSSync(id)) {
			BYTE buf[16];
			if (m_file.Read(&buf, 16) != 16) {
				break;
			}

			// DTS header
			int fsize = 0, targeted_bitrate;
			fsize = ParseDTSHeader(buf, &m_samplerate, &m_channels, &m_framelength, &targeted_bitrate);
			if (fsize == 0) {
				break;
			}
			m_streamtype = DTS;

			// DTS-HD header and zero padded
			unsigned long sync = -1;
			unsigned int HD_size = 0;
			int zero_bytes = 0;

			m_file.Seek(m_dataOffset+fsize, CFile::begin);
			m_file.Read(&sync, sizeof(sync));
			if (id == DTS_SYNC_WORD && sync == DTSHD_SYNC_WORD && m_file.Read(&buf, 8)==8) {
				unsigned char isBlownUpHeader = (buf[1]>>5)&1;
				if (isBlownUpHeader) {
					HD_size = ((buf[2]&1)<<19 | buf[3]<<11 | buf[4]<<3 | buf[5]>>5) + 1;
				} else {
					HD_size = ((buf[2]&31)<<11 | buf[3]<<3 | buf[4]>>5) + 1;
				}
				//TODO: get more information about DTS-HD
				m_streamtype = DTSHD;
				m_fixedframesize = false;
			} else if (sync == 0 && fsize < 2048) { // zero padded?
				m_file.Seek(m_dataOffset+2048, CFile::begin);
				m_file.Read(&sync, sizeof(sync));
				if (sync == id) {
					zero_bytes = 2048 - fsize;
					m_streamtype = DTSPaded;
				}
			}

			// calculate actual bitrate
			m_framesize = fsize + HD_size + zero_bytes;
			m_bitrate   = int ((m_framesize) * 8i64 * m_samplerate / m_framelength); // inaccurate, because HD_size is not constant

			// calculate framesize to support a sonic audio decoder 4.3 (TODO: make otherwise)
			// sonicDTSminsize = fsize + HD_size + 4096
			int k = (m_framesize + 4096 + m_framesize - 1) / m_framesize;
			m_framesize   *= k;
			m_framelength *= k;

			m_wFormatTag = WAVE_FORMAT_DTS;
			m_subtype = MEDIASUBTYPE_DTS;
		}
		// AC3 & E-AC3
		else if ((WORD)id == AC3_SYNC_WORD) {
			BYTE buf[20];
			if (m_file.Read(&buf, 8) != 8) {
				break;
			}

			BYTE bsid = (buf[5] >> 3);
			int fsize   = 0;
			//int HD_size = 0;

			// AC3 header
			if (bsid <= 10) {
				fsize = ParseAC3Header(buf, &m_samplerate, &m_channels, &m_framelength, &m_bitrate);
				if (fsize == 0) {
					break;
				}
				m_streamtype = AC3;
				m_subtype = MEDIASUBTYPE_DOLBY_AC3;
				/*
				// TrueHD+AC3
				m_file.Seek(m_dataOffset+fsize, CFile::begin);
				if (m_file.Read(&buf, 20) == 20) {
					int samplerate2, channels2, framelength2;
					HD_size = ParseTrueHDHeader(buf, &samplerate2, &channels2, &framelength2);
					if (HD_size > 0) {
						m_streamtype = TrueHDAC3;
						m_fixedframesize = false;
					}
				}
				*/
			}
			// E-AC3 header
			else if (bsid <= 16) {
				int frametype;
				fsize = ParseEAC3Header(buf, &m_samplerate, &m_channels, &m_framelength, &frametype);
				if (fsize == 0) {
					break;
				}

				m_file.Seek(m_dataOffset+fsize, CFile::begin);
				if (m_file.Read(&buf, 8) == 8) {
					int samplerate2, channels2, samples2, frametype2;
					int fsize2 = ParseEAC3Header(buf, &samplerate2, &channels2, &samples2, &frametype2);
					if (fsize2 > 0 && frametype2 == EAC3_FRAME_TYPE_DEPENDENT) {
						fsize += fsize2;
					}
				}
				m_bitrate = int (fsize * 8i64 * m_samplerate / m_framelength);
				m_streamtype = EAC3;
				m_subtype = MEDIASUBTYPE_DOLBY_DDPLUS;
			} else { //unknown bsid
				break;
			}

			// calculate framesize to support a sonic audio decoder 4.3 (TODO: make otherwise)
			// sonicAC3minsize = framesize + 64
			m_framesize = fsize * 2;
			m_framelength *= 2;

			m_wFormatTag = WAVE_FORMAT_UNKNOWN;
		}
		// SPDIF AC3
		else if (waveheader && id == IEC61937_SYNC_WORD) {
			BYTE buf[16];
			if (m_file.Read(&buf, 16) != 16) {
				break;
			}
			m_framesize = ParseAC3IEC61937Header(buf);
			if (m_framesize == 0) {
				break;
			}

			m_wFormatTag  = WAVE_FORMAT_DOLBY_AC3_SPDIF;
			m_channels    = 2;
			m_samplerate  = 44100;
			m_bitrate     = 1411200;
			m_bitdepth    = 16;
			m_framelength = 1536;

			m_subtype = MEDIASUBTYPE_DOLBY_AC3_SPDIF;
			m_streamtype = SPDIF_AC3;
		}
		// MLP
		else if (id2 == MLP_SYNC_WORD) {
			BYTE buf[20];
			if (m_file.Read(&buf, 20) != 20) {
				break;
			}

			bool istruehd;
			m_framesize = ParseMLPHeader(buf, &m_samplerate, &m_channels, &m_framelength, &m_bitdepth, &istruehd);
			if (m_framesize == 0) {
				break;
			}

			m_streamtype = MLP;
			m_fixedframesize = false;

			m_bitrate = (int)(m_framesize * 8i64 * m_samplerate / m_framelength); // inaccurate, because framesize is not constant

			m_wFormatTag = WAVE_FORMAT_UNKNOWN;
			m_subtype    = MEDIASUBTYPE_MLP;
		}
		/*
		// TrueHD
		else if (id2 == TRUEHD_SYNC_WORD) {
			BYTE buf[20];
			if (m_file.Read(&buf, 20) != 20)
				break;
			int fsize  = 0;
			int fsize2 = 0;
			fsize = ParseTrueHDHeader(buf, &m_samplerate, &m_channels, &m_framelength);
			if (fsize == 0)
				break;
			m_streamtype = TrueHD;
			m_fixedframesize = false;

			// TrueHD+AC3
			if (m_file.Read(&buf, 8) == 8) {
				int samplerate2, channels2, framelength2, bitrate2;
				fsize2 = ParseAC3Header(buf, &samplerate2, &channels2, &framelength2, &bitrate2);
				if (fsize2 > 0) {
					m_streamtype = TrueHDAC3;

					if (samplerate2 > 0) {
						m_samplerate  = samplerate2;
						m_framelength = framelength2;
					}
				}
			}

			m_framesize = fsize + fsize2;
			m_bitrate   = int ((m_framesize) * 8i64 * m_samplerate / m_framelength);
			//m_bitdepth = 24;

			m_wFormatTag = WAVE_FORMAT_UNKNOWN;
			m_subtype = MEDIASUBTYPE_DOLBY_TRUEHD;
		*/
		else {
			break;
		}

		if (m_samplerate > 0) {
			m_AvgTimePerFrame = 10000000i64 * m_framelength / m_samplerate;
		}
		m_rtDuration = m_AvgTimePerFrame * (m_file.GetLength() - m_dataOffset) / m_framesize;
		m_rtStop = m_rtDuration;

		hr = S_OK;
	} while (false);

	if (phr) {
		*phr = hr;
	}
}

CDTSAC3Stream::~CDTSAC3Stream()
{
}

HRESULT CDTSAC3Stream::DecideBufferSize(IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* pProperties)
{
	ASSERT(pAlloc);
	ASSERT(pProperties);

	HRESULT hr = NOERROR;

	pProperties->cBuffers = 1;
	pProperties->cbBuffer = m_framesize;

	ALLOCATOR_PROPERTIES Actual;
	if (FAILED(hr = pAlloc->SetProperties(pProperties, &Actual))) {
		return hr;
	}

	if (Actual.cbBuffer < pProperties->cbBuffer) {
		return E_FAIL;
	}
	ASSERT(Actual.cBuffers == pProperties->cBuffers);

	return NOERROR;
}

HRESULT CDTSAC3Stream::FillBuffer(IMediaSample* pSample, int nFrame, BYTE* pOut, long& len)
{
	BYTE* pOutOrg = pOut;

	const GUID* majortype = &m_mt.majortype;
	const GUID* subtype = &m_mt.subtype;
	UNREFERENCED_PARAMETER(subtype);

	if (*majortype == MEDIATYPE_Audio) {
		m_file.Seek(m_dataOffset + nFrame*m_framesize, CFile::begin);
		if ((int)m_file.Read(pOut, m_framesize) < m_framesize) {
			return S_FALSE;
		}
		pOut += m_framesize;
	}

	len = (long)(pOut - pOutOrg);

	return S_OK;
}

HRESULT CDTSAC3Stream::GetMediaType(int iPosition, CMediaType* pmt)
{
	CAutoLock cAutoLock(m_pFilter->pStateLock());

	if (iPosition == 0) {
		pmt->majortype = MEDIATYPE_Audio;
		pmt->subtype = m_subtype;
		pmt->formattype = FORMAT_WaveFormatEx;

		WAVEFORMATEX* wfe = (WAVEFORMATEX*)pmt->AllocFormatBuffer(sizeof(WAVEFORMATEX));
		wfe->wFormatTag      = m_wFormatTag;
		wfe->nChannels       = m_channels;
		wfe->nSamplesPerSec  = m_samplerate;
		wfe->nAvgBytesPerSec = (m_bitrate + 4) /8;
		wfe->nBlockAlign     = m_framesize < WORD_MAX ? m_framesize : WORD_MAX;
		wfe->wBitsPerSample  = m_bitdepth;
		wfe->cbSize = 0;

		if (m_streamtype == SPDIF_AC3) {
			wfe->nBlockAlign = 4;
			pmt->SetSampleSize(m_framesize);
		} else if (m_streamtype == MLP /*|| m_streamtype == TrueHD*/) {
			pmt->SetSampleSize(0);
		}

	} else {
		return VFW_S_NO_MORE_ITEMS;
	}

	pmt->SetTemporalCompression(FALSE);

	return S_OK;
}

HRESULT CDTSAC3Stream::CheckMediaType(const CMediaType* pmt)
{
	if (pmt->majortype == MEDIATYPE_Audio && pmt->formattype == FORMAT_WaveFormatEx) {
		WORD& wFmtTag = ((WAVEFORMATEX*)pmt->pbFormat)->wFormatTag;

		if (pmt->subtype == MEDIASUBTYPE_DTS && wFmtTag == WAVE_FORMAT_DTS) {
			return S_OK;
		} else if (pmt->subtype == MEDIASUBTYPE_WAVE_DTS && wFmtTag == WAVE_FORMAT_DVD_DTS) {
			return S_OK;
		} else if (pmt->subtype == MEDIASUBTYPE_DOLBY_AC3 && (wFmtTag == WAVE_FORMAT_UNKNOWN || wFmtTag == WAVE_FORMAT_DOLBY_AC3)) {
			return S_OK;
		} else if (pmt->subtype == MEDIASUBTYPE_DOLBY_DDPLUS && wFmtTag == WAVE_FORMAT_UNKNOWN) {
			return S_OK;
		} else if (pmt->subtype == MEDIASUBTYPE_DOLBY_AC3_SPDIF && wFmtTag == WAVE_FORMAT_DOLBY_AC3_SPDIF) {
			return S_OK;
		} else if (pmt->subtype == MEDIASUBTYPE_MLP && wFmtTag == WAVE_FORMAT_UNKNOWN) {
			return S_OK;
		}
		//else if (pmt->subtype == MEDIASUBTYPE_DOLBY_TRUEHD && wFmtTag == WAVE_FORMAT_UNKNOWN) {
		//    return S_OK;
		//}
	}
	return E_INVALIDARG;
}

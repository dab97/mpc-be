/*
 * $Id$
 *
 * (C) 2006-2012 see Authors.txt
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
#include "HdmvSub.h"
#include "../DSUtil/GolombBuffer.h"

#if (0)	// Set to 1 to activate HDMV subtitles traces
#define TRACE_HDMVSUB	TRACE
#else
#define TRACE_HDMVSUB
#endif

CHdmvSub::CHdmvSub(void)
	: CBaseSub(ST_HDMV)
{
	m_nColorNumber				= 0;

	m_nCurSegment				= NO_SEGMENT;
	m_pSegBuffer				= NULL;
	m_nTotalSegBuffer			= 0;
	m_nSegBufferPos				= 0;
	m_nSegSize					= 0;
	m_pDefaultPalette			= NULL;
	m_nDefaultPaletteNbEntry	= 0;
	m_pCurrentWindow			= NULL;

	memset(&m_VideoDescriptor, 0, sizeof(VIDEO_DESCRIPTOR));
}

CHdmvSub::~CHdmvSub()
{
	Reset();

	delete[] m_pSegBuffer;
	delete[] m_pDefaultPalette;

	delete m_pCurrentWindow;
}

void CHdmvSub::AllocSegment(int nSize)
{
	if (nSize > m_nTotalSegBuffer) {
		delete[] m_pSegBuffer;
		m_pSegBuffer		= DNew BYTE[nSize];
		m_nTotalSegBuffer	= nSize;
	}
	m_nSegBufferPos	 = 0;
	m_nSegSize       = nSize;
}

POSITION CHdmvSub::GetStartPosition(REFERENCE_TIME rt, double fps, bool CleanOld)
{
	if (CleanOld) {
		CHdmvSub::CleanOld(rt);
	}

	return m_pObjects.GetHeadPosition();
}

HRESULT CHdmvSub::ParseSample(IMediaSample* pSample)
{
	CheckPointer (pSample, E_POINTER);
	HRESULT			hr;
	REFERENCE_TIME	rtStart = INVALID_TIME, rtStop = INVALID_TIME;
	BYTE*			pData = NULL;
	int				lSampleLen;

	hr = pSample->GetPointer(&pData);
	if (FAILED(hr) || pData == NULL) {
		return hr;
	}
	lSampleLen = pSample->GetActualDataLength();

	pSample->GetTime(&rtStart, &rtStop);

	return ParseSample(pData, lSampleLen, rtStart, rtStop);
}

HRESULT CHdmvSub::ParseSample(BYTE* pData, int lSampleLen, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop)
{
	HRESULT hr = S_OK;

	if (pData) {
		CGolombBuffer SampleBuffer (pData, lSampleLen);

		while (!SampleBuffer.IsEOF()) {
			if (m_nCurSegment == NO_SEGMENT) {
				HDMV_SEGMENT_TYPE	nSegType	= (HDMV_SEGMENT_TYPE)SampleBuffer.ReadByte();
				USHORT				nUnitSize	= SampleBuffer.ReadShort();
				lSampleLen -=3;

				switch (nSegType) {
					case PALETTE :
					case OBJECT :
					case PRESENTATION_SEG :
					case END_OF_DISPLAY :
						m_nCurSegment = nSegType;
						AllocSegment (nUnitSize);
						break;

					case WINDOW_DEF :
					case INTERACTIVE_SEG :
					case HDMV_SUB1 :
					case HDMV_SUB2 :
						// Ignored stuff...
						SampleBuffer.SkipBytes(nUnitSize);
						break;
					default :
						return VFW_E_SAMPLE_REJECTED;
				}
			}

			if (m_nCurSegment != NO_SEGMENT) {
				if (m_nSegBufferPos < m_nSegSize) {
					int nSize = min (m_nSegSize-m_nSegBufferPos, lSampleLen);
					SampleBuffer.ReadBuffer (m_pSegBuffer+m_nSegBufferPos, nSize);
					m_nSegBufferPos += nSize;
				}

				if (m_nSegBufferPos >= m_nSegSize) {
					CGolombBuffer SegmentBuffer (m_pSegBuffer, m_nSegSize);

					switch (m_nCurSegment) {
						case PALETTE :
							TRACE_HDMVSUB ("CHdmvSub::ParseSample() : PALETTE\n");
							ParsePalette(&SegmentBuffer, m_nSegSize);
							break;
						case OBJECT :
							TRACE_HDMVSUB ("CHdmvSub::ParseSample() : OBJECT\n");
							ParseObject(&SegmentBuffer, m_nSegSize);
							break;
						case PRESENTATION_SEG :
							TRACE_HDMVSUB ("CHdmvSub::ParseSample() : PRESENTATION_SEG = [%10I64d], %S, size = %d\n", rtStart, ReftimeToString(rtStart), m_nSegSize);
							ParsePresentationSegment(&SegmentBuffer, rtStart);
							break;
						case WINDOW_DEF :
							//TRACE_HDMVSUB ("CHdmvSub::ParseSample() : WINDOW_DEF = %10I64d, %S\n", rtStart, ReftimeToString(rtStart));
							break;
						case END_OF_DISPLAY :
							//TRACE_HDMVSUB ("CHdmvSub::ParseSample() : END_OF_DISPLAY = %10I64d, %S\n", rtStart, ReftimeToString(rtStart));
							break;
						default :
							TRACE_HDMVSUB ("CHdmvSub::ParseSample() : UNKNOWN Seg [%d] = [%10I64d], %S\n", m_nCurSegment, rtStart, ReftimeToString(rtStart));
					}

					m_nCurSegment = NO_SEGMENT;
				}
			}
		}
	}

	return hr;
}

void CHdmvSub::ParsePresentationSegment(CGolombBuffer* pGBuffer, REFERENCE_TIME rtTime)
{
	COMPOSITION_DESCRIPTOR	CompositionDescriptor;
	BYTE					nObjectNumber;
	bool					palette_update_flag;
	BYTE					palette_id_ref;

	ParseVideoDescriptor(pGBuffer, &m_VideoDescriptor);
	ParseCompositionDescriptor(pGBuffer, &CompositionDescriptor);
	palette_update_flag	= !!(pGBuffer->ReadByte() & 0x80);
	UNREFERENCED_PARAMETER(palette_update_flag);
	palette_id_ref		= pGBuffer->ReadByte();
	nObjectNumber		= pGBuffer->ReadByte();

	TRACE_HDMVSUB ("CHdmvSub::ParsePresentationSegment() : Size = %d, nObjectNumber = %d, palette_id_ref = %d, compositionNumber = %d\n",
					pGBuffer->GetSize(), nObjectNumber, palette_id_ref, CompositionDescriptor.nNumber);

	if (m_pCurrentWindow && m_pCurrentWindow->m_nObjectNumber) {
		if (m_pCurrentWindow->m_compositionNumber != -1 && m_pCurrentWindow->m_compositionNumber != CompositionDescriptor.nNumber) {

			for (int i = 0; i<m_pCurrentWindow->m_nObjectNumber; i++) {
				if (m_pCurrentWindow->Objects[i] && m_pCurrentWindow->Objects[i]->GetRLEDataSize()) {
					TRACE_HDMVSUB ("*** CHdmvSub::ParsePresentationSegment() - AddTail() : m_object_id_ref = %d, m_window_id_ref = %d, compositionNumber = %d, [%10I64d -> %10I64d], [%S -> %S]\n",
									m_pCurrentWindow->Objects[i]->m_object_id_ref,
									m_pCurrentWindow->Objects[i]->m_window_id_ref,
									m_pCurrentWindow->Objects[i]->m_compositionNumber,
									m_pCurrentWindow->Objects[i]->m_rtStart, m_pCurrentWindow->Objects[i]->m_rtStop,
									ReftimeToString(m_pCurrentWindow->Objects[i]->m_rtStart), ReftimeToString(m_pCurrentWindow->Objects[i]->m_rtStop)
									);
	
					m_pObjects.AddTail (m_pCurrentWindow->Objects[i]);
				}
			}
		}
	}

	if (CompositionDescriptor.bState == 0 && nObjectNumber == 0) {
		POSITION pos = m_pObjects.GetHeadPosition();
		while (pos) {
			CompositionObject* pObject = m_pObjects.GetAt (pos);
			
			if (pObject->m_rtStop == _I64_MAX) {
				pObject->m_rtStop = rtTime;

				TRACE_HDMVSUB ("****** CHdmvSub::ParsePresentationSegment() - Clear Screen : m_object_id_ref = %d, m_window_id_ref = %d, compositionNumber = %d, [%10I64d -> %10I64d], [%S -> %S]\n",
								pObject->m_object_id_ref,
								pObject->m_window_id_ref,
								pObject->m_compositionNumber,
								pObject->m_rtStart, pObject->m_rtStop,
								ReftimeToString(pObject->m_rtStart), ReftimeToString(pObject->m_rtStop)
								);

			}

			m_pObjects.GetNext(pos);
		}
	}

	if (m_pCurrentWindow) {
		m_pCurrentWindow->Reset();
	} else {
		m_pCurrentWindow = DNew HDMV_WindowDefinition();
	}


	if (nObjectNumber > 0) {
		
		m_pCurrentWindow->m_nObjectNumber		= nObjectNumber;
		m_pCurrentWindow->m_palette_id_ref		= (SHORT)palette_id_ref;
		m_pCurrentWindow->m_compositionNumber	= CompositionDescriptor.nNumber;

		for (int i=0; i<nObjectNumber; i++) {
			m_pCurrentWindow->Objects[i]						= DNew CompositionObject();
			m_pCurrentWindow->Objects[i]->m_rtStart				= rtTime;
			m_pCurrentWindow->Objects[i]->m_rtStop				= _I64_MAX;
			m_pCurrentWindow->Objects[i]->m_compositionNumber	= CompositionDescriptor.nNumber;

			ParseCompositionObject (pGBuffer, m_pCurrentWindow->Objects[i]);
		}

		POSITION pos = m_pObjects.GetHeadPosition();
		while (pos) {
			bool objectWindowFound		= false;
			CompositionObject* pObject	= m_pObjects.GetAt (pos);

			if (pObject->m_rtStop == _I64_MAX) {

				for (int i=0; i<nObjectNumber; i++) {
					if (pObject->m_object_id_ref == m_pCurrentWindow->Objects[i]->m_object_id_ref && pObject->m_window_id_ref == m_pCurrentWindow->Objects[i]->m_window_id_ref) {
						objectWindowFound = true;
						break;
					}
				}

				if (!objectWindowFound && CompositionDescriptor.bState >= 0) {
					pObject->m_rtStop = rtTime;
					TRACE_HDMVSUB ("*** CHdmvSub::ParsePresentationSegment() - Set TimeStamp : m_object_id_ref = %d, m_window_id_ref = %d, compositionNumber = %d, [%10I64d -> %10I64d], [%S -> %S]\n",
									pObject->m_object_id_ref,
									pObject->m_window_id_ref,
									pObject->m_compositionNumber,
									pObject->m_rtStart, pObject->m_rtStop,
									ReftimeToString(pObject->m_rtStart), ReftimeToString(pObject->m_rtStop)
									);

				}
			}

			m_pObjects.GetNext(pos);
		}
	}
}

void CHdmvSub::ParseCompositionObject(CGolombBuffer* pGBuffer, CompositionObject* pCompositionObject)
{
	pCompositionObject->m_object_id_ref			= pGBuffer->ReadShort();
	pCompositionObject->m_window_id_ref			= (SHORT)pGBuffer->ReadByte();
	BYTE bTemp									= pGBuffer->ReadByte();
	pCompositionObject->m_object_cropped_flag	= !!(bTemp & 0x80);
	pCompositionObject->m_forced_on_flag		= !!(bTemp & 0x40);
	pCompositionObject->m_horizontal_position	= pGBuffer->ReadShort();
	pCompositionObject->m_vertical_position		= pGBuffer->ReadShort();

	if (pCompositionObject->m_object_cropped_flag) {
		pCompositionObject->m_cropping_horizontal_position	= pGBuffer->ReadShort();
		pCompositionObject->m_cropping_vertical_position	= pGBuffer->ReadShort();
		pCompositionObject->m_cropping_width				= pGBuffer->ReadShort();
		pCompositionObject->m_cropping_height				= pGBuffer->ReadShort();
	}

	TRACE_HDMVSUB ("CHdmvSub::ParseCompositionObject() : m_object_id_ref = %d, m_window_id_ref = %d, pos = %d:%d\n", 
					pCompositionObject->m_object_id_ref, pCompositionObject->m_window_id_ref,
					pCompositionObject->m_horizontal_position, pCompositionObject->m_vertical_position);
}

void CHdmvSub::ParsePalette(CGolombBuffer* pGBuffer, USHORT nSize)
{
	int		nNbEntry;
	BYTE	palette_id				= pGBuffer->ReadByte();
	BYTE	palette_version_number	= pGBuffer->ReadByte();
	UNREFERENCED_PARAMETER(palette_version_number);

	ASSERT ((nSize-2) % sizeof(HDMV_PALETTE) == 0);
	nNbEntry = (nSize-2) / sizeof(HDMV_PALETTE);
	HDMV_PALETTE* pPalette = (HDMV_PALETTE*)pGBuffer->GetBufferPos();

	if (m_pDefaultPalette == NULL || m_nDefaultPaletteNbEntry != nNbEntry) {
		delete[] m_pDefaultPalette;
		m_pDefaultPalette		 = DNew HDMV_PALETTE[nNbEntry];
		m_nDefaultPaletteNbEntry = nNbEntry;
	}
	memcpy (m_pDefaultPalette, pPalette, nNbEntry*sizeof(HDMV_PALETTE));

	if (m_pCurrentWindow && m_pCurrentWindow->m_palette_id_ref == palette_id && m_pCurrentWindow->m_nObjectNumber) {
		for (int i = 0; i<m_pCurrentWindow->m_nObjectNumber; i++) {
			m_pCurrentWindow->Objects[i]->SetPalette(nNbEntry, pPalette, m_VideoDescriptor.nVideoWidth > 720);
		}
	}
}

void CHdmvSub::ParseObject(CGolombBuffer* pGBuffer, USHORT nUnitSize)
{
	SHORT object_id = pGBuffer->ReadShort();
	TRACE_HDMVSUB ("CHdmvSub::ParseObject() : object_id = %d\n", object_id);

	if (m_pCurrentWindow && m_pCurrentWindow->m_nObjectNumber) {
		for (int i = 0; i<m_pCurrentWindow->m_nObjectNumber; i++) {
			if (m_pCurrentWindow->Objects[i]->m_object_id_ref == object_id) {
			
				CompositionObject* pObject	= m_pCurrentWindow->Objects[i];
				pObject->m_version_number	= pGBuffer->ReadByte();
				BYTE m_sequence_desc		= pGBuffer->ReadByte();

				if (m_sequence_desc & 0x80) {
					DWORD object_data_length = (DWORD)pGBuffer->BitRead(24);

					pObject->m_width	= pGBuffer->ReadShort();
					pObject->m_height 	= pGBuffer->ReadShort();

					pObject->SetRLEData (pGBuffer->GetBufferPos(), nUnitSize-11, object_data_length-4);

					TRACE_HDMVSUB ("CHdmvSub::ParseObject() : NewObject - size = %ld, version = %d, total obj = %d, size = %dx%d\n",
									object_data_length,
									pObject->m_version_number,
									m_pObjects.GetCount(),
									pObject->m_width, pObject->m_height);
				} else {
					pObject->AppendRLEData (pGBuffer->GetBufferPos(), nUnitSize-4);
				}

			}
		}
	}
}

void CHdmvSub::ParseVideoDescriptor(CGolombBuffer* pGBuffer, VIDEO_DESCRIPTOR* pVideoDescriptor)
{
	pVideoDescriptor->nVideoWidth   = pGBuffer->ReadShort();
	pVideoDescriptor->nVideoHeight  = pGBuffer->ReadShort();
	pVideoDescriptor->bFrameRate	= pGBuffer->ReadByte();
}

void CHdmvSub::ParseCompositionDescriptor(CGolombBuffer* pGBuffer, COMPOSITION_DESCRIPTOR* pCompositionDescriptor)
{
	pCompositionDescriptor->nNumber	= pGBuffer->ReadShort();
	pCompositionDescriptor->bState	= pGBuffer->ReadByte();
}

void CHdmvSub::Render(SubPicDesc& spd, REFERENCE_TIME rt, RECT& bbox)
{
	POSITION pos = m_pObjects.GetHeadPosition();

	while (pos) {
		CompositionObject* pObject = m_pObjects.GetAt (pos);

		if (rt >= pObject->m_rtStart && (rt < pObject->m_rtStop || pObject->m_rtStop == _I64_MAX)) {

			ASSERT (pObject!=NULL && spd.w >= (pObject->m_horizontal_position + pObject->m_width) && spd.h >= (pObject->m_vertical_position + pObject->m_height));

			if (pObject && pObject->GetRLEDataSize() && pObject->m_width > 0 && pObject->m_height > 0 &&
					spd.w >= (pObject->m_horizontal_position + pObject->m_width) &&
					spd.h >= (pObject->m_vertical_position + pObject->m_height)) {

				if (g_bForcedSubtitle && !pObject->m_forced_on_flag) {
					TRACE_HDMVSUB ("CHdmvSub::Render() : skip non forced subtitle - forced = %d, %I64d = %S", pObject->m_forced_on_flag, rt, ReftimeToString(rt));
					return;
				}

				if (!pObject->HavePalette() && m_nDefaultPaletteNbEntry) {
					pObject->SetPalette (m_nDefaultPaletteNbEntry, m_pDefaultPalette, m_VideoDescriptor.nVideoWidth>720);
				}

				if (!pObject->HavePalette()) {
					return;
				}

				TRACE_HDMVSUB ("CHdmvSub::Render() : size = %ld, ObjRes = %dx%d, SPDRes = %dx%d, %I64d = %S\n",
								pObject->GetRLEDataSize(),
								pObject->m_width, pObject->m_height, spd.w, spd.h,
								rt, ReftimeToString(rt));

				pObject->RenderHdmv(spd);

			}
		}

		m_pObjects.GetNext(pos);
	}

	bbox.left	= 0;
	bbox.top	= 0;
	bbox.right	= m_VideoDescriptor.nVideoWidth;
	bbox.bottom	= m_VideoDescriptor.nVideoHeight;
}

HRESULT CHdmvSub::GetTextureSize(POSITION pos, SIZE& MaxTextureSize, SIZE& VideoSize, POINT& VideoTopLeft)
{
	CompositionObject* pObject = m_pObjects.GetAt (pos);

	if (pObject) {
		MaxTextureSize.cx = VideoSize.cx = m_VideoDescriptor.nVideoWidth;
		MaxTextureSize.cy = VideoSize.cy = m_VideoDescriptor.nVideoHeight;

		VideoTopLeft.x = 0;
		VideoTopLeft.y = 0;

		return S_OK;
	}

	ASSERT (FALSE);
	return E_INVALIDARG;
}

void CHdmvSub::Reset()
{
	CompositionObject* pObject;
	while (m_pObjects.GetCount() > 0) {
		pObject = m_pObjects.RemoveHead();
		delete pObject;
	}

	if (m_pCurrentWindow) {
		delete m_pCurrentWindow;
		m_pCurrentWindow = NULL;
	}
}

CompositionObject* CHdmvSub::FindObject(REFERENCE_TIME rt)
{
	POSITION pos = m_pObjects.GetHeadPosition();

	while (pos) {
		CompositionObject* pObject = m_pObjects.GetAt (pos);

		if (rt >= pObject->m_rtStart && rt < pObject->m_rtStop) {
			return pObject;
		}

		m_pObjects.GetNext(pos);
	}

	return NULL;
}

void CHdmvSub::CleanOld(REFERENCE_TIME rt)
{
	CompositionObject* pObject_old;

	while (m_pObjects.GetCount()>0) {
		pObject_old = m_pObjects.GetHead();
		if (pObject_old->m_rtStop < rt) {
			TRACE_HDMVSUB ("CHdmvSub:HDMV remove object %d  %S => %S (rt=%S)\n", pObject_old->GetRLEDataSize(),
						   ReftimeToString (pObject_old->m_rtStart), ReftimeToString(pObject_old->m_rtStop),
						   ReftimeToString(rt));
			m_pObjects.RemoveHead();
			delete pObject_old;
		} else {
			break;
		}
	}
}

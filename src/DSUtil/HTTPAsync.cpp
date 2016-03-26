/*
 * (C) 2016 see Authors.txt
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
#include "HTTPAsync.h"
#include "DSUtil.h"

void CALLBACK CHTTPAsync::Callback(_In_ HINTERNET hInternet,
								   __in_opt DWORD_PTR dwContext,
								   __in DWORD dwInternetStatus,
								   __in_opt LPVOID lpvStatusInformation,
								   __in DWORD dwStatusInformationLength)
{
	Context* pContext = (Context*)dwContext;
	switch (pContext->dwContext) {
		case CONTEXT_CONNECT:
			if (dwInternetStatus == INTERNET_STATUS_HANDLE_CREATED) {
				INTERNET_ASYNC_RESULT *pRes = (INTERNET_ASYNC_RESULT *)lpvStatusInformation;
				pContext->pObj->m_hConnect = (HINTERNET)pRes->dwResult;
				SetEvent(pContext->pObj->m_hConnectedEvent);
			}
			break;
		case CONTEXT_REQUESTHANDLE: // Request handle
			{
				switch (dwInternetStatus) {
					case INTERNET_STATUS_HANDLE_CREATED:
						{
							INTERNET_ASYNC_RESULT *pRes = (INTERNET_ASYNC_RESULT *)lpvStatusInformation;
							pContext->pObj->m_hRequest = (HINTERNET)pRes->dwResult;
							SetEvent(pContext->pObj->m_hRequestOpenedEvent);
						}
						break;

					case INTERNET_STATUS_REQUEST_SENT:
						{
							DWORD* lpBytesSent = (DWORD*)lpvStatusInformation;
							UNREFERENCED_PARAMETER(lpBytesSent);
						}
						break;

					case INTERNET_STATUS_REQUEST_COMPLETE:
						{
							INTERNET_ASYNC_RESULT *pAsyncRes = (INTERNET_ASYNC_RESULT *)lpvStatusInformation;
							SetEvent(pContext->pObj->m_hRequestCompleteEvent);
						}
						break;

					case INTERNET_STATUS_REDIRECT:
						{
							CString strRealAddr = (LPCTSTR)lpvStatusInformation;
							UNREFERENCED_PARAMETER(strRealAddr);
						}
						break;

					case INTERNET_STATUS_RECEIVING_RESPONSE:
						break;

					case INTERNET_STATUS_RESPONSE_RECEIVED:
						{
							DWORD* dwBytesReceived = (DWORD*)lpvStatusInformation;
							UNREFERENCED_PARAMETER(dwBytesReceived);
						}
						break;
					}
			}
	}
}

CString const CHTTPAsync::QueryInfo(DWORD dwInfoLevel)
{
	CString queryInfo;
    DWORD   dwLen = 0 ;
	if (!HttpQueryInfo(m_hRequest, dwInfoLevel, NULL, &dwLen, 0) && dwLen) {
		const DWORD dwError = GetLastError();
		if (dwError == ERROR_INSUFFICIENT_BUFFER) {
			if (HttpQueryInfo(m_hRequest, dwInfoLevel, (LPVOID)queryInfo.GetBuffer(dwLen), &dwLen, 0)) {
				queryInfo.ReleaseBuffer(dwLen);
			}
		}
    }
	
	return queryInfo;
}

CHTTPAsync::CHTTPAsync()
{
	m_hConnectedEvent       = CreateEvent(NULL, FALSE, FALSE, NULL);
	m_hRequestOpenedEvent   = CreateEvent(NULL, FALSE, FALSE, NULL);
	m_hRequestCompleteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
}

CHTTPAsync::~CHTTPAsync()
{
	if (m_hConnectedEvent) {
		CloseHandle(m_hConnectedEvent);
	}
	if (m_hRequestOpenedEvent) {
		CloseHandle(m_hRequestOpenedEvent);
	}
	if (m_hRequestCompleteEvent) {
		CloseHandle(m_hRequestCompleteEvent);
	}

	Close();
}

#define SAFE_INTERNET_CLOSE_HANDLE(p) { if (p) { VERIFY(InternetCloseHandle(p)); (p) = NULL; } }
#define CheckLastError(ret)           { if (GetLastError() != ERROR_IO_PENDING) return (ret); }

void CHTTPAsync::Close()
{
	SAFE_INTERNET_CLOSE_HANDLE(m_hRequest);
	SAFE_INTERNET_CLOSE_HANDLE(m_hConnect);
	SAFE_INTERNET_CLOSE_HANDLE(m_hInstance);

	m_url.Clear();
	m_url_str.Empty();
	m_host.Empty();
	m_path.Empty();

	m_nPort   = INTERNET_DEFAULT_HTTP_PORT;
	m_nScheme = ATL_URL_SCHEME_HTTP;

	m_lenght = 0;
}

HRESULT CHTTPAsync::Connect(LPCTSTR lpszURL, DWORD dwTimeOut/* = INFINITE*/, LPCTSTR lpszAgent/* = L"MPC-BE"*/, BOOL bSendRequest/* = TRUE*/)
{
	Close();

	ResetEvent(m_hConnectedEvent);
	ResetEvent(m_hRequestOpenedEvent);
	ResetEvent(m_hRequestCompleteEvent);

	if (!m_url.CrackUrl(lpszURL)) {
		return E_INVALIDARG;
	}
	if (m_url.GetScheme() != ATL_URL_SCHEME_HTTP && m_url.GetScheme() != ATL_URL_SCHEME_HTTPS) {
		return E_FAIL;
	}
	if (m_url.GetPortNumber() == ATL_URL_INVALID_PORT_NUMBER) {
		m_url.SetPortNumber(ATL_URL_DEFAULT_HTTP_PORT);
	}

	m_url_str = lpszURL;
	if (m_url.GetUrlPathLength() == 0) {
		m_url.SetUrlPath(L"/");
		m_url_str += '/';
	}

	m_host    = m_url.GetHostName();
	m_path    = CString(m_url.GetUrlPath()) + CString(m_url.GetExtraInfo());
	m_nPort   = m_url.GetPortNumber();
	m_nScheme = m_url.GetScheme();

	m_hInstance = InternetOpen(lpszAgent, 
							   INTERNET_OPEN_TYPE_PRECONFIG,
							   NULL,
							   NULL,
							   INTERNET_FLAG_ASYNC);
	CheckPointer(m_hInstance, E_FAIL);


	if (InternetSetStatusCallback(m_hInstance, (INTERNET_STATUS_CALLBACK)&Callback) == INTERNET_INVALID_STATUS_CALLBACK) {
		return E_FAIL;
	}

	m_context.dwContext = CONTEXT_CONNECT;
	m_context.pObj      = this;

	m_hConnect = InternetConnect(m_hInstance, 
								 m_host,
								 m_nPort,
								 NULL,
								 NULL,
								 INTERNET_SERVICE_HTTP,
								 INTERNET_FLAG_KEEP_CONNECTION | INTERNET_FLAG_NO_CACHE_WRITE,
								 (DWORD)&m_context);

	if (m_hConnect == NULL) {
		CheckLastError(E_FAIL);

		if (WaitForSingleObject(m_hConnectedEvent, dwTimeOut) == WAIT_TIMEOUT) {
			return E_FAIL;
		}
	}

	CheckPointer(m_hConnect, E_FAIL);

	if (bSendRequest) {
		m_context.dwContext = CONTEXT_REQUESTHANDLE;
		m_context.pObj      = this;

		DWORD dwFlags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
		if (m_nScheme == ATL_URL_SCHEME_HTTPS) {
			dwFlags |= (INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID);
		}


		m_hRequest = HttpOpenRequest(m_hConnect, 
									 L"GET",
									 m_path,
									 NULL,
									 NULL,
									 NULL,
									 dwFlags,
									 (DWORD) &m_context); 

		if (m_hRequest == NULL) {
			CheckLastError(E_FAIL);
			
			if (WaitForSingleObject(m_hRequestOpenedEvent, dwTimeOut) == WAIT_TIMEOUT) {
				return E_FAIL;
			}
		}

		CheckPointer(m_hRequest, E_FAIL);

		const CString lpszHeaders = L"Accept: */*\r\n";
		if (!HttpSendRequest(m_hRequest, 
							 lpszHeaders, 
							 lpszHeaders.GetLength(), 
							 NULL,
							 0)) {
			CheckLastError(E_FAIL);
		}

		if (WaitForSingleObject(m_hRequestCompleteEvent, dwTimeOut) == WAIT_TIMEOUT) {
			Close();
			return E_FAIL;
		}

		CString queryInfo = QueryInfo(HTTP_QUERY_STATUS_CODE);
		if (queryInfo != L"200") {
			Close();
			return E_FAIL;
		}

		queryInfo = QueryInfo(HTTP_QUERY_CONTENT_LENGTH);
		if (!queryInfo.IsEmpty()) {
			QWORD val = 0;
			if (1 == swscanf_s(queryInfo, L"%I64u", &val)) {
				m_lenght = val;
			}
		}
	}
	
	return S_OK;
}

DWORD CHTTPAsync::Read(PBYTE pBuffer, DWORD dwSize, DWORD dwTimeOut/* = INFINITE*/)
{
	INTERNET_BUFFERS InetBuff = { sizeof(InetBuff) };
	InetBuff.lpvBuffer        = pBuffer;
	InetBuff.dwBufferLength   = dwSize;

	m_context.dwContext = CONTEXT_REQUESTHANDLE;
	m_context.pObj      = this;

	if (!InternetReadFileEx(m_hRequest,
							&InetBuff,
							0,
							(DWORD)&m_context)) {
		CheckLastError(0);
		if (WaitForSingleObject(m_hRequestCompleteEvent, dwTimeOut) == WAIT_TIMEOUT) {
			return 0;
		}
	}

	return InetBuff.dwBufferLength;
}
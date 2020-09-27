#include "WinHttpClientAsync.h"
#include <vector>
#include <iostream>
#include <windows.h>
#include <winhttp.h>

using namespace std;

#pragma comment(lib, "winhttp.lib")

struct WinHttpClientAsync::Impl
{
	HINTERNET _session;
	HINTERNET _connect;
	HINTERNET _request;

	vector<char> _readDataVector;
	wstring _responseHeader;

	SuccessCallback _successCallback;
	FailureCallback _failureCallback;

	Impl() : _session(nullptr), _connect(nullptr), _request(nullptr), _successCallback(nullptr), _failureCallback(nullptr)
	{}

	~Impl()
	{}

	static void __stdcall AsyncCallback (
		HINTERNET hInternet,
		DWORD_PTR dwContext,
		DWORD dwInternetStatus,
		LPVOID lpvStatusInformation,
		DWORD dwStatusInformationLength )
	{
		WinHttpClientAsync::Impl* pRequester = (WinHttpClientAsync::Impl*)dwContext;
		if (pRequester == NULL)	
			return;

		switch (dwInternetStatus) 
		{
		case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
			pRequester->OnSendRequestComplete(dwStatusInformationLength);
			break;
		case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
			pRequester->OnHeadersAvailable(dwStatusInformationLength);
			break;
		case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE:
			pRequester->OnDataAvailable(lpvStatusInformation);
			break;
		case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
			pRequester->OnReadComplete(lpvStatusInformation, dwStatusInformationLength);
			break;
		case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
			pRequester->OnRequestError((WINHTTP_ASYNC_RESULT*)lpvStatusInformation);
			break;
		default:
			std::wcout << L"Unknown/unhandled callback - status " << dwInternetStatus << "given";
			break;
		}
	}

	bool QueryHeaders() 
	{
		DWORD dwSize = 0;

		// Get header size
		if (false == ::WinHttpQueryHeaders( 
			_request, WINHTTP_QUERY_RAW_HEADERS_CRLF, 
			WINHTTP_HEADER_NAME_BY_INDEX, NULL, 
			&dwSize, WINHTTP_NO_HEADER_INDEX)) 
		{
			const DWORD errorCode = GetLastError();
			if (errorCode != ERROR_INSUFFICIENT_BUFFER)
			{
				FinalizeWithFailure(L"Error while WinHttpQueryHeaders", errorCode);
				return false;
			}
		}

		LPVOID lpOutBuffer = new WCHAR[dwSize];
		// Read header
		if (::WinHttpQueryHeaders(_request,
			WINHTTP_QUERY_RAW_HEADERS_CRLF,
			WINHTTP_HEADER_NAME_BY_INDEX, lpOutBuffer, &dwSize, WINHTTP_NO_HEADER_INDEX))
		{
			_responseHeader.assign((LPWSTR)lpOutBuffer, wcslen((LPWSTR)lpOutBuffer));
			delete[] lpOutBuffer;
			lpOutBuffer = NULL;
			return true;
		}
		else
		{
			delete[] lpOutBuffer;
			lpOutBuffer = NULL;
			return false;
		}
	}

	void QueryData() 
	{
		// WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE, OnDataAvailable
		if (false == ::WinHttpQueryDataAvailable(_request, NULL)) 
		{
			FinalizeWithFailure(L"Error while WinHttpQueryDataAvailable");
		}
	}

	bool ReadData(size_t bufferSize) 
	{
		LPSTR lpOutBuffer = new char[bufferSize + 1];
		ZeroMemory(lpOutBuffer, bufferSize + 1);
		// WINHTTP_CALLBACK_STATUS_READ_COMPLETE, OnReadComplete
		if (false == ::WinHttpReadData(_request, (LPVOID)lpOutBuffer, (DWORD)bufferSize, NULL))
		{
			FinalizeWithFailure(L"Error while WinHttpReadData");
			delete[] lpOutBuffer;
			return false;
		}
		return true;
	}

	void OnSendRequestComplete(DWORD dwStatusInformationLength) 
	{
		// WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE, OnHeadersAvailable
		if (false == ::WinHttpReceiveResponse(_request, NULL)) 
		{
			CloseHandles();
		}
	}

	void OnHeadersAvailable(DWORD dwStatusInformationLength) 
	{
		if (false == QueryHeaders())
		{
			FinalizeWithFailure(L"Error getting header");
			return;
		}

		QueryData();
	}

	void OnDataAvailable(LPVOID lpvStatusInformation) 
	{
		const size_t dataSize = *((LPDWORD)lpvStatusInformation);
		if (dataSize == 0) // complete
		{
			FinalizeWithSuccess();
		}
		else // read more
		{
			if (this->ReadData(dataSize) == false)
				this->CloseHandles();
		}
	}

	void OnReadComplete(LPVOID lpvStatusInformation, DWORD dwStatusInformationLength) 
	{
		if (dwStatusInformationLength == 0)
			return;

		LPSTR lpBuffer = (LPSTR)lpvStatusInformation;
		const DWORD bufferLen = dwStatusInformationLength;
		_readDataVector.insert(_readDataVector.end(), lpBuffer, lpBuffer + bufferLen);
		delete[] lpBuffer;
		lpBuffer = NULL;
		QueryData();
	}

	void OnRequestError(LPWINHTTP_ASYNC_RESULT pAsyncResult) 
	{
		FinalizeWithFailure(L"WINHTTP_CALLBACK_STATUS_REQUEST_ERROR ", pAsyncResult->dwError);
	}

	void CloseHandles()
	{
		if (_request)
		{
			::WinHttpSetStatusCallback(_request, NULL, NULL, NULL);
			::WinHttpCloseHandle(_request);
			_request = NULL;
		}

		if (_connect)
		{
			WinHttpCloseHandle(_connect);
			_connect = NULL;
		}

		if (_session)
		{
			WinHttpCloseHandle(_session);
			_session = NULL;
		}
	}

	void FinalizeWithFailure(wstring errorMsg, DWORD errorCodeParam = 0)
	{
		const DWORD errorCode = errorCodeParam != 0 ? errorCodeParam : GetLastError();
		CloseHandles();
		if (nullptr == _failureCallback)
			return;

		const wstring errorMsgWithCode = errorMsg + L". ErrorCode: " + std::to_wstring(errorCode);
		_failureCallback(errorMsgWithCode);
	}

	void FinalizeWithSuccess()
	{
		CloseHandles();
		if (nullptr == _successCallback)
			return;

		const int strLen = (int)_readDataVector.size();
		const string mbcsResponse(_readDataVector.begin(), _readDataVector.end());
		wchar_t* lpWideBuffer = new wchar_t[strLen];
		::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, mbcsResponse.c_str(), strLen, lpWideBuffer, strLen);
		const wstring wResponse(lpWideBuffer, strLen);
		_successCallback(_responseHeader, wResponse);
		delete[] lpWideBuffer;
		lpWideBuffer = nullptr;
	}
};

WinHttpClientAsync::WinHttpClientAsync() : _impl(new Impl())
{
}

WinHttpClientAsync::~WinHttpClientAsync() 
{
	if (_impl) 
	{
		delete _impl;
		_impl = nullptr;
	}
}

bool WinHttpClientAsync::PostRequest(const wchar_t* szURL, const char* postData, SuccessCallback success, FailureCallback failure)
{
	return RequestImpl(szURL, postData, true, success, failure);
}

bool WinHttpClientAsync::GetRequest(const wchar_t* szURL, SuccessCallback success, FailureCallback failure)
{
	return RequestImpl(szURL, nullptr, false, success, failure);
}

bool WinHttpClientAsync::RequestImpl(const wchar_t* szURL, const char* postData, bool isPostReq, SuccessCallback success, FailureCallback failure)
{
	constexpr LPCWSTR userAgent = L"WinHttpClientAsync";
	_impl->_session = ::WinHttpOpen(userAgent,
		WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS,
		WINHTTP_FLAG_ASYNC);
	if (_impl->_session == NULL)
	{
		return false;
	}

	WCHAR szHost[256] = { 0, };
	URL_COMPONENTS urlComp;
	ZeroMemory(&urlComp, sizeof(urlComp));
	urlComp.dwStructSize = sizeof(urlComp);
	urlComp.lpszHostName = szHost;
	urlComp.dwHostNameLength = sizeof(szHost) / sizeof(szHost[0]);
	urlComp.dwUrlPathLength = -1;
	urlComp.dwSchemeLength = -1;
	if (!::WinHttpCrackUrl(szURL, 0, 0, &urlComp))
	{
		_impl->FinalizeWithFailure(L"Error while WinHttpCrackUrl");
		return false;
	}

	_impl->_connect = ::WinHttpConnect(_impl->_session, szHost, urlComp.nPort, 0);
	if (NULL == _impl->_connect)
	{
		_impl->FinalizeWithFailure(L"Error while WinHttpConnect");
		return false;
	}

	DWORD dwOpenRequestFlag = (INTERNET_SCHEME_HTTPS == urlComp.nScheme) ? WINHTTP_FLAG_SECURE : 0;
	const LPCWSTR verb = isPostReq ? L"POST" : L"GET";
	_impl->_request = ::WinHttpOpenRequest(_impl->_connect,
		verb, urlComp.lpszUrlPath,
		NULL, WINHTTP_NO_REFERER,
		WINHTTP_DEFAULT_ACCEPT_TYPES,
		dwOpenRequestFlag);

	if (_impl->_request == 0)
	{
		_impl->FinalizeWithFailure(L"Error while WinHttpOpenRequest");
		return false;
	}

	const WINHTTP_STATUS_CALLBACK pCallback = ::WinHttpSetStatusCallback(_impl->_request,
		(WINHTTP_STATUS_CALLBACK)Impl::AsyncCallback,
		WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS |
		WINHTTP_CALLBACK_FLAG_REDIRECT,
		NULL);
	if (pCallback != NULL)
	{
		_impl->FinalizeWithFailure(L"Error while WinHttpSetStatusCallback");
		return false;
	}

	_impl->_successCallback = success;
	_impl->_failureCallback = failure;

	// WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE, OnSendRequestComplete
	constexpr wchar_t RequestHeader[] = L"Content-Type: application/json;charset=utf-8";
	const DWORD headerLen = (DWORD)wcslen(RequestHeader);
	LPVOID postBuffer = (postData == nullptr) ? WINHTTP_NO_REQUEST_DATA : (LPVOID)postData;
	const DWORD postDataLen = (postData == nullptr) ? 0 : (DWORD)strlen(postData);

	if (false == ::WinHttpSendRequest(_impl->_request,
		RequestHeader, headerLen,
		postBuffer, postDataLen, postDataLen,
		(DWORD_PTR)this->_impl))
	{
		_impl->FinalizeWithFailure(L"Error while WinHttpSendRequest");
		return false;
	}

	return true;
}
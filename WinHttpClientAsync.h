#ifndef WIN_HTTP_CLIENT_ASYNC
#define WIN_HTTP_CLIENT_ASYNC

#include <string>
#include <functional>

typedef std::function<void(const std::wstring& , const std::wstring& )> SuccessCallback;
typedef std::function<void(const std::wstring& )> FailureCallback;

class WinHttpClientAsync 
{
public:
	WinHttpClientAsync();
	virtual ~WinHttpClientAsync();

	bool GetRequest(const wchar_t* szURL, SuccessCallback success, FailureCallback failure);
	bool PostRequest(const wchar_t* szURL, const char* postData, SuccessCallback success, FailureCallback failure);

protected:
	bool RequestImpl(const wchar_t* szURL, const char* postData, bool isPostReq, SuccessCallback success, FailureCallback failure);

private:
	struct Impl;
	Impl* _impl;
};

#endif // WIN_HTTP_CLIENT_ASYNC
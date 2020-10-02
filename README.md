# WinHttpClientAsync

Simple wrapper class for asynchronus http client using WinHTTP. This can be entry point to implement asynchronus http client on Windows.
```c++
    void OnSuccess(const std::wstring& header, const std::wstring& response){...}
    void OnFailure(const std::wstring& errorMsg) {...}
    
    constexpr wchar_t url[] = L"https://httpbin.org/anything";
    constexpr char postData[] = "{\"id\":24, \"team\":\"lakers\"}";
    WinHttpClientAsync client;
    client.PostRequest(url, postData, OnSuccess, OnFailure);
```

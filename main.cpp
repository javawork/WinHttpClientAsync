#include "WinHttpClientAsync.h"
#include <iostream>

static void OnSuccess(const std::wstring& header, const std::wstring& response)
{
    std::wcout << L"<OnSuccess>" << std::endl;
    std::wcout << L"- Header: " << std::endl;
    std::wcout << header << std::endl;
    std::wcout << L"- Response: " << std::endl;
    std::wcout << response << std::endl;
}

static void OnFailure(const std::wstring& errorMsg)
{
    std::wcout << L"<OnFailure>" << std::endl;
    std::wcout << L"- Error: " << std::endl;
    std::wcout << errorMsg << std::endl;
}

int main()
{
    constexpr wchar_t url1[] = L"https://httpbin.org/get";
    WinHttpClientAsync client1;
    client1.GetRequest( url1, OnSuccess, OnFailure );
    std::wcout << L"GetRequest: " << url1 << std::endl;
    char c1 = getchar();
    
    constexpr wchar_t url2[] = L"https://httpbin.org/anything";
    constexpr char postData2[] = "{\"id\":24, \"team\":\"lakers\"}";
    WinHttpClientAsync client2;
    client2.PostRequest(url2, postData2, OnSuccess, OnFailure);
    std::wcout << L"PostRequest: " << url2 << L", " << postData2 << std::endl;
    char c2 = getchar();
    std::cout << "Terminated\n";
}
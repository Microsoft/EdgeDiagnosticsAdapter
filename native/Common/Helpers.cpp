﻿//
// Copyright (C) Microsoft. All rights reserved.
//

#include "stdafx.h"
#include "Helpers.h"
#include <atlsafe.h>
#include <DocObj.h>
#include <Mshtmhst.h>
#include <ExDisp.h>
#include <webapplication.h>
#include <sstream>
#include <Appmodel.h>

#define IDM_STARTDIAGNOSTICSMODE 3802
#define CP_AUTO 50001
#define VERSION_SIGNATURE 0xFEEF04BD

namespace Helpers
{
    BOOL EnumWindowsHelper(_In_ const function<BOOL(HWND)>& callbackFunc)
    {
        return ::EnumWindows([](HWND hwnd, LPARAM lparam) -> BOOL {
            return (*(function<BOOL(HWND)>*)lparam)(hwnd);
        }, (LPARAM)&callbackFunc);
    }

    BOOL EnumChildWindowsHelper(HWND hwndParent, _In_ const function<BOOL(HWND)>& callbackFunc)
    {
        return EnumChildWindows(hwndParent, [](HWND hwnd, LPARAM lparam) -> BOOL {
            return (*(function<BOOL(HWND)>*)lparam)(hwnd);
        }, (LPARAM)&callbackFunc);
    }

    bool IsWindowClass(_In_ const HWND hwnd, _In_ LPCWSTR pszWindowClass)
    {
        if (hwnd && pszWindowClass && *pszWindowClass)
        {
            const int BUFFER_SIZE = 100;
            WCHAR szClassname[BUFFER_SIZE];

            int result = ::GetClassName(hwnd, (LPWSTR)&szClassname, BUFFER_SIZE);
            if (result && _wcsicmp(szClassname, pszWindowClass) == 0)
            {
                return true;
            }
        }

        return false;
    }

    HRESULT GetCurrentModuleWithoutRef(_Out_ HMODULE& moduleOut)
    {
        BOOL result = ::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCTSTR)Helpers::GetCurrentModuleWithoutRef, &moduleOut);
        ATLENSURE_RETURN_HR(result != FALSE, AtlHresultFromLastError());
        return S_OK;
    }

    HRESULT ReadFileFromModule(_In_ LPCWSTR resourceName, _Out_ CString& sFileData)
    {
        HMODULE module;
        Helpers::GetCurrentModuleWithoutRef(module);
        ATLENSURE_RETURN_HR(module, E_FAIL);

        HRSRC hresInfo = ::FindResource(module, resourceName, RT_HTML);
        if (hresInfo != nullptr)
        {
            // Load the resource
            HGLOBAL hGlobal = ::LoadResource(module, hresInfo);
            ATLENSURE_RETURN_HR(hGlobal != nullptr, AtlHresultFromLastError());

            // Get the size of resource
            DWORD size = ::SizeofResource(module, hresInfo);
            ATLENSURE_RETURN_HR(size > 0, AtlHresultFromLastError());

            // Get the raw data
            const BYTE* pbData = (const BYTE*) ::LockResource(hGlobal);
            ATLENSURE_RETURN_HR(pbData != nullptr, E_FAIL);

            // Convert the resource into unicode text
            int nDstBufferSize = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, (const CHAR*)pbData, (int)size, nullptr, 0);
            ATLENSURE_RETURN_HR(nDstBufferSize > 0, AtlHresultFromLastError());

            try
            {
                nDstBufferSize = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, (const CHAR*)pbData, (int)size, sFileData.GetBuffer(nDstBufferSize), nDstBufferSize);
                sFileData.ReleaseBuffer(nDstBufferSize);
            }
            catch (CAtlException& ex) // GetBuffer out of memory
            {
                return ex.m_hr;
            }
        }

        // Ensure that we actually got some text
        ATLENSURE_RETURN_HR(sFileData.GetLength() > 0, E_UNEXPECTED);

        return S_OK;
    }

    HRESULT ExpandEnvironmentString(_In_z_ LPCWSTR path, _Out_ CString& expandedString)
    {
        ATLENSURE_RETURN_HR(path != nullptr, E_INVALIDARG);

        expandedString.Empty();
        CString correctedFilePath(path);

        DWORD charsNeeded = ::ExpandEnvironmentStrings(correctedFilePath, expandedString.GetBuffer(MAX_PATH), MAX_PATH);
        ATLENSURE_RETURN_HR(charsNeeded > 0, AtlHresultFromLastError());

        if (charsNeeded > MAX_PATH)
        {
            charsNeeded = ::ExpandEnvironmentStrings(path, expandedString.GetBuffer(charsNeeded), charsNeeded);
            ATLENSURE_RETURN_HR(charsNeeded > 0, AtlHresultFromLastError());
        }
        expandedString.ReleaseBufferSetLength(charsNeeded - 1);

        return S_OK;
    }

    bool Is64OS()
    {
#ifdef _WIN64
        return true;  // 64-bit programs run only on Win64
#else
        BOOL is64 = FALSE;

        typedef BOOL(APIENTRY *LPFN_ISWOW64PROCESS)(HANDLE, PBOOL);

        LPFN_ISWOW64PROCESS fnIsWow64Process;

        HMODULE mod = ::GetModuleHandle(L"kernel32");
        fnIsWow64Process = (LPFN_ISWOW64PROCESS)::GetProcAddress(mod, "IsWow64Process");

        if (fnIsWow64Process != nullptr)
        {
            fnIsWow64Process(::GetCurrentProcess(), &is64) && is64;
        }

        return !!is64;
#endif
    }

    HRESULT GetDocumentFromSite(_In_ IUnknown* spSite, _Out_ CComPtr<IDispatch>& spDocumentOut)
    {
        ATLENSURE_RETURN_HR(spSite != nullptr, E_INVALIDARG);

        CComQIPtr<IHTMLDocument2> spDocument(spSite);
        if (spDocument.p != nullptr)
        {
            CComQIPtr<IDispatch> spDocDisp(spDocument);
            ATLENSURE_RETURN_HR(spDocDisp.p != nullptr, E_NOINTERFACE);

            spDocumentOut = spDocDisp;
            return S_OK;
        }
        else
        {
            CComQIPtr<IWebBrowser2> spBrowser2(spSite);
            if (spBrowser2 != nullptr)
            {
                CComPtr<IDispatch> spDocDisp;
                HRESULT hr = spBrowser2->get_Document(&spDocDisp);
                FAIL_IF_NOT_S_OK(hr);
                ATLENSURE_RETURN_HR(spDocDisp.p != nullptr, E_FAIL);

                spDocumentOut = spDocDisp;
                return hr;
            }
            else
            {
                CComQIPtr<IWebApplicationHost> webAppHost(spSite);
                if (webAppHost.p != nullptr)
                {
                    CComPtr<IHTMLDocument2> spDoc;
                    HRESULT hr = webAppHost->get_Document(&spDoc);
                    FAIL_IF_NOT_S_OK(hr);
                    CComQIPtr<IDispatch> spDocDisp(spDoc);
                    ATLENSURE_RETURN_HR(spDocDisp.p != nullptr, E_NOINTERFACE);

                    spDocumentOut = spDocDisp;
                    return S_OK;
                }
            }
        }

        return E_NOINTERFACE;
    }

    HRESULT GetDocumentFromHwnd(_In_ const HWND browserHwnd, _Out_ CComPtr<IHTMLDocument2>& spDocument)
    {
        DWORD_PTR messageResult = 0;
        LRESULT sendMessageResult = ::SendMessageTimeoutW(browserHwnd, Helpers::GetHtmlDocumentMessage(), 0L, 0L, SMTO_ABORTIFHUNG, 2000, &messageResult);

        if (sendMessageResult != 0 && messageResult != 0)
        {
            HMODULE* hModule = new HMODULE(::LoadLibraryEx(L"oleacc.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32));
            shared_ptr<HMODULE> spOleAcc(hModule, [](HMODULE* hModule) { ::FreeLibrary(*hModule); delete hModule; });
            if (spOleAcc)
            {
                auto pfObjectFromLresult = reinterpret_cast<LPFNOBJECTFROMLRESULT>(::GetProcAddress(*spOleAcc.get(), "ObjectFromLresult"));
                if (pfObjectFromLresult != nullptr)
                {
                    return pfObjectFromLresult(messageResult, IID_IHTMLDocument2, 0, reinterpret_cast<void**>(&spDocument.p));
                }
            }
        }

        return E_FAIL;
    }

    UINT GetHtmlDocumentMessage()
    {
        if (s_getHtmlDocumentMessage == 0)
        {
            s_getHtmlDocumentMessage = ::RegisterWindowMessageW(L"WM_HTML_GETOBJECT");
        }

        return s_getHtmlDocumentMessage;
    }

    HRESULT StartDiagnosticsMode(_In_ IHTMLDocument2* pDocument, REFCLSID clsid, _In_ LPCWSTR path, REFIID iid, _COM_Outptr_opt_ void** ppOut)
    {
        ATLENSURE_RETURN_HR(pDocument != nullptr, E_INVALIDARG);

        if (ppOut)
        {
            (*ppOut) = nullptr;
        }

        // Get the target from the document
        CComQIPtr<IOleCommandTarget> spCommandTarget(pDocument);
        ATLENSURE_RETURN_HR(spCommandTarget.p != nullptr, E_INVALIDARG);

        // Setup the diagnostics mode parameters
        HRESULT hr = S_OK;
        CComBSTR guid(clsid);
        CComSafeArray<BSTR> spSafeArray(4);
        hr = spSafeArray.SetAt(0, ::SysAllocString(guid), FALSE);
        FAIL_IF_NOT_S_OK(hr);
        hr = spSafeArray.SetAt(1, ::SysAllocString(path), FALSE);
        FAIL_IF_NOT_S_OK(hr);
        hr = spSafeArray.SetAt(2, ::SysAllocString(L""), FALSE);
        FAIL_IF_NOT_S_OK(hr);
        hr = spSafeArray.SetAt(3, ::SysAllocString(L""), FALSE);
        FAIL_IF_NOT_S_OK(hr);

        // Start diagnostics mode
        CComVariant varParams(spSafeArray);
        CComVariant varSite;
        hr = spCommandTarget->Exec(&CGID_MSHTML, IDM_STARTDIAGNOSTICSMODE, OLECMDEXECOPT_DODEFAULT, &varParams, &varSite);
        FAIL_IF_NOT_S_OK(hr);
        ATLENSURE_RETURN_VAL(varSite.vt == VT_UNKNOWN, E_UNEXPECTED);
        ATLENSURE_RETURN_VAL(varSite.punkVal != nullptr, E_UNEXPECTED);

        // Get the requested interface
        if (ppOut)
        {
            hr = varSite.punkVal->QueryInterface(iid, ppOut);
            FAIL_IF_NOT_S_OK(hr);
        }
        return hr;
    }

    CStringA EscapeJsonString(const CString& value)
    {
        CStringA escapedValue;

        for (auto i = 0; i < value.GetLength(); i++)
        {
            auto c = value[i];
            switch (c)
            {
            case '\\': escapedValue.Append("\\\\"); break;
            case '\"': escapedValue.Append("\\\""); break;
            case '\b': escapedValue.Append("\\b"); break;
            case '\f': escapedValue.Append("\\f"); break;
            case '\n': escapedValue.Append("\\n"); break;
            case '\r': escapedValue.Append("\\r"); break;
            case '\t': escapedValue.Append("\\t"); break;
            default:
                if (c < 0x20 || c > 0x7e)
                {
                    CStringA charLiteral;
                    charLiteral.Format("\\u%04x", c);
                    escapedValue.Append(charLiteral);
                }
                else
                {
                    escapedValue.AppendChar((char)c);
                }
                break;
            }
        }

        return escapedValue;
    }

    CStringA GetFileVersion(_In_ LPCWSTR filePath)
    {
        ::SetLastError(0);
        DWORD  verSize = ::GetFileVersionInfoSizeW(filePath, NULL);
        if (::GetLastError() != 0 || verSize == NULL || verSize == 0)
        {
            return "";
        }

        std::vector<char> verData(verSize);

        if (!::GetFileVersionInfoW(filePath, NULL, verSize, &verData[0]))
        {
            return "";
        }

        VS_FIXEDFILEINFO* pVerInfo = nullptr;
        UINT sizeOfVersionNumber;
        if (!::VerQueryValueW(&verData[0], L"\\", (LPVOID*)&pVerInfo, &sizeOfVersionNumber))
        {
            return "";
        }

        if (sizeOfVersionNumber <= 0)
        {
            return "";
        }

        // The signature value should always be 0xFEEF04BD according to MSDN
        // https://msdn.microsoft.com/en-us/library/windows/desktop/ms646997(v=vs.85).aspx
        // Though checking in case it's not as the bitwise operators below won't work if not
        if (pVerInfo->dwSignature != VERSION_SIGNATURE)
        {
            return "";
        }

        std::stringstream ss;
        ss << ((pVerInfo->dwFileVersionMS >> 16) & 0xffff);
        ss << ".";
        ss << ((pVerInfo->dwFileVersionMS >> 0) & 0xffff);
        ss << ".";
        ss << ((pVerInfo->dwFileVersionLS >> 16) & 0xffff);
        ss << ".";
        ss << ((pVerInfo->dwFileVersionLS >> 0) & 0xffff);

        return ss.str().c_str();
    }

    CStringA GetLastErrorMessage()
    {
        std::stringstream ss;
        ss << "Error Code: ";
        ss << GetLastError();

        return ss.str().c_str();
    }

    CStringA UTF16toUTF8(const CString& utf16)
    {
        CStringA utf8;
        int length = ::WideCharToMultiByte(CP_UTF8, 0, utf16, -1, nullptr, 0, 0, 0);
        if (length > 1)
        {
            char* buffer = utf8.GetBuffer(length - 1);
            if (buffer)
            {
                ::WideCharToMultiByte(CP_UTF8, 0, utf16, -1, buffer, length, 0, 0);
            }
            utf8.ReleaseBuffer();
        }
        return utf8;
    }

    CString UTF8toUTF16(const CStringA& utf8)
    {
        CString utf16;
        int length = ::MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
        if (length > 1)
        {
            wchar_t* buffer = utf16.GetBuffer(length - 1);
            if (buffer)
            {
                ::MultiByteToWideChar(CP_UTF8, 0, utf8, -1, buffer, length);
            }
            utf16.ReleaseBuffer();
        }
        return utf16;
    }

    HRESULT OpenUrlInMicrosoftEdge(__in PCWSTR url)
    {
        HRESULT hr = E_FAIL;

        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        SHELLEXECUTEINFOW sei = { sizeof sei };
        sei.lpVerb = L"open";
        std::wstring mywstring(url);
        std::wstring concatted_stdstr = L"microsoft-edge:" + mywstring;
        sei.lpFile = concatted_stdstr.c_str();
        hr = ShellExecuteExW(&sei);

        return hr;
    }

    BOOL CALLBACK TerminateAppEnum(HWND hwnd, LPARAM lParam)
    {
        DWORD dwID;

        ::GetWindowThreadProcessId(hwnd, &dwID);

        if (dwID == (DWORD)lParam)
        {
            ::PostMessage(hwnd, WM_CLOSE, 0, 0);
        }

        return TRUE;
    }

    DWORD WINAPI TerminateApp(DWORD dwPID, DWORD dwTimeout)
    {
        HANDLE   hProc;
        DWORD   dwRet;

        // If we can't open the process with PROCESS_TERMINATE rights,
        // then we give up immediately.
        hProc = ::OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, dwPID);

        if (hProc == NULL)
        {
            return E_FAIL;
        }

        // TerminateAppEnum() posts WM_CLOSE to all windows whose PID
        // matches your process's.
        EnumWindows((WNDENUMPROC)TerminateAppEnum, (LPARAM)dwPID);

        // Wait on the handle. If it signals, great. If it times out,
        // then you kill it.
        if (WaitForSingleObject(hProc, dwTimeout) != WAIT_OBJECT_0)
        {
            dwRet = (::TerminateProcess(hProc, 0) ? S_OK : E_FAIL);
        }
        else
        {
            dwRet = 0;
        }

        CloseHandle(hProc);

        return dwRet;
    }

    HRESULT KillAllProcessByExe(const wchar_t *filename)
    {
        HANDLE hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPALL, NULL);
        PROCESSENTRY32 pEntry;
        pEntry.dwSize = sizeof(pEntry);
        BOOL hRes = Process32First(hSnapShot, &pEntry);
        while (hRes)
        {
            if (wcscmp(pEntry.szExeFile, filename) == 0)
            {
                TerminateApp(pEntry.th32ProcessID, 1000);

            }
            hRes = Process32Next(hSnapShot, &pEntry);
        }
        CloseHandle(hSnapShot); 

        return S_OK;
    }

    HRESULT CloseWindow(_In_ const HWND hwnd)
    {        
        if (!IsWindow(hwnd)) 
        {
            return E_INVALIDARG;
        }

        HWND parentWindow = GetParent(hwnd);
        ::PostMessage(parentWindow, WM_CLOSE, 0, 0);
        
        return S_OK;
    }

    HRESULT GetEdgePackageFamilyName(_Out_ CString& packageFullName)
	{
		LONG result = ERROR_SUCCESS;
		CString edgeFamilyName = L"Microsoft.MicrosoftEdge_8wekyb3d8bbwe";

		UINT32 packageCount = 0;
		UINT32 packageNamesBufferLength = 0;
		result = FindPackagesByPackageFamily(edgeFamilyName, PACKAGE_FILTER_HEAD | PACKAGE_INFORMATION_BASIC, &packageCount, nullptr, &packageNamesBufferLength, nullptr, nullptr);

		// The first time we call the API we are getting the length of the buffer which also returns the error ERROR_INSUFFICIENT_BUFFER
		if (result != ERROR_SUCCESS && result != ERROR_INSUFFICIENT_BUFFER)
		{
			return E_FAIL;
		}

		if (packageCount <= 0)
		{
			return E_FAIL;
		}

		//unique_ptr<PWSTR[]> packageNames(new PWSTR[packageCount]);
		vector<PWSTR> packageNames;
		packageNames.resize(packageCount);
		//unique_ptr<wchar_t[]> buffer(new wchar_t[packageNamesBufferLength]);
		CString buffer;

		result = FindPackagesByPackageFamily(edgeFamilyName, PACKAGE_FILTER_HEAD | PACKAGE_INFORMATION_BASIC, &packageCount, packageNames.data(), &packageNamesBufferLength, buffer.GetBufferSetLength(packageNamesBufferLength), nullptr);
		buffer.ReleaseBufferSetLength(packageNamesBufferLength);

		if (result != ERROR_SUCCESS)
		{
			return E_FAIL;
		}

		packageFullName = packageNames[0];

		return S_OK;
	}

    BOOL EnumThreadWindowsHelper(_In_ DWORD threadId, _In_ const function<BOOL(HWND)>& callbackFunc)
    {
        return ::EnumThreadWindows(threadId, [](HWND hwnd, LPARAM lparam) -> BOOL {
            return (*(function<BOOL(HWND)>*)lparam)(hwnd);
        }, (LPARAM)&callbackFunc);
    }
}

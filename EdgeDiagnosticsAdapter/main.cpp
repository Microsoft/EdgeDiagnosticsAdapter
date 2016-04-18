//
// Copyright (C) Microsoft. All rights reserved.
//

#include "stdafx.h"
#include "EdgeDiagnosticsAdapter.h"
#include "Helpers.h"
#include <iostream>
#include <Shellapi.h>
#include <Shlobj.h>
#include "Aclapi.h"
#include "boost/program_options.hpp"

CHandle hChromeProcess;
namespace po = boost::program_options;

BOOL WINAPI OnClose(DWORD reason)
{
	if (hChromeProcess.m_h)
	{
		::TerminateProcess(hChromeProcess.m_h, 0);
	}
	return TRUE;
}

int wmain(int argc, wchar_t* argv[])
{
	// Initialize COM and deinitialize when we go out of scope
	HRESULT hrCoInit = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	po::options_description desc("Allowed options");
	desc.add_options()
		("launch,l", po::value<string>(), "Launches Edge. Optionally at the URL specified in the value")
		("killall,k", "Kills all running Edge processes.")
		;

	po::variables_map vm;
	try
	{
		po::store(po::parse_command_line(argc, argv, desc), vm);
	}
	catch (po::error& e)
	{
		std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
		std::cerr << desc << std::endl;
		return E_FAIL;
	}

	shared_ptr<HRESULT> spCoInit(&hrCoInit, [](const HRESULT* hrCom) -> void { if (SUCCEEDED(*hrCom)) { ::CoUninitialize(); } });
	{
		// Set a close handler to shutdown the chrome instance we launch
		::SetConsoleCtrlHandler(OnClose, TRUE);

		// Kill all Edge instances if their is an aegument /killall
		if (vm.count("killall"))
		{
			//killAllProcessByExe(L"MicrosoftEdgeCP.exe");
			Helpers::KillAllProcessByExe(L"MicrosoftEdge.exe");
		}

		// Launch Edge if their is an argument set /launch:<url>
		if (vm.count("launch"))
		{
			CString url(vm["launch"].as<string>().c_str());
			if (url.GetLength() == 0)
			{
				url = L"https://www.bing.com";
			}

			HRESULT hr = Helpers::OpenUrlInMicrosoftEdge(url);

			if (FAILED(hr))
			{
				std::cout << L"Failed to launch Microsoft Edge";
			}
		}

		// Get the current path that we are running from
		CString fullPath;
		DWORD count = ::GetModuleFileName(nullptr, fullPath.GetBuffer(MAX_PATH), MAX_PATH);
		fullPath.ReleaseBufferSetLength(count);

		LPWSTR buffer = fullPath.GetBuffer();
		LPWSTR newPath = ::PathFindFileName(buffer);
		if (newPath && newPath != buffer)
		{
			fullPath.ReleaseBufferSetLength((newPath - buffer));
		}
		else
		{
			fullPath.ReleaseBuffer();
		}

		// Check to make sure that the dll has the ACLs to load in an appcontainer
		// We're doing this here as the adapter has no setup script and should be xcopy deployable/removeable

		PACL pOldDACL = NULL, pNewDACL = NULL;
		PSECURITY_DESCRIPTOR pSD = NULL;
		EXPLICIT_ACCESS ea;
		SECURITY_INFORMATION si = DACL_SECURITY_INFORMATION;

		// The check is done on the folder and should be inherited to all objects
		DWORD dwRes = GetNamedSecurityInfo(fullPath, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, &pOldDACL, NULL, &pSD);

		// Initialize an EXPLICIT_ACCESS structure for the new ACE. 
		ZeroMemory(&ea, sizeof(EXPLICIT_ACCESS));
		ea.grfAccessPermissions = GENERIC_READ | GENERIC_EXECUTE;
		ea.grfAccessMode = SET_ACCESS;
		ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
		ea.Trustee.TrusteeForm = TRUSTEE_IS_NAME;
		ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
		ea.Trustee.ptstrName = L"ALL APPLICATION PACKAGES";

		// Create a new ACL that merges the new ACE into the existing DACL.
		dwRes = SetEntriesInAcl(1, &ea, pOldDACL, &pNewDACL);
		if (dwRes == ERROR_SUCCESS)
		{
			dwRes = SetNamedSecurityInfo(fullPath.GetBuffer(), SE_FILE_OBJECT, si, NULL, NULL, pNewDACL, NULL);
			if (dwRes == ERROR_SUCCESS)
			{

			}
		}

		if (dwRes != ERROR_SUCCESS)
		{
			// The ACL was not set, this isn't fatal as it only impacts IE in EPM and Edge and the user can set it manually
			wcout << L"Could not set ACL to allow access to IE EPM or Edge.";
			wcout << L"\n";
			wcout << Helpers::GetLastErrorMessage().GetBuffer();
			wcout << L"\n";
			wcout << L"You can set the ACL manually by adding Read & Execute permissions for 'All APPLICATION PACAKGES' to each dll.";
			wcout << L"\n";
		}

		if (pSD != NULL)
		{
			LocalFree((HLOCAL)pSD);
		}
		if (pNewDACL != NULL)
		{
			LocalFree((HLOCAL)pNewDACL);
		}

		// Load the proxy server
		EdgeDiagnosticsAdapter proxy(fullPath);

		if (proxy.IsServerRunning)
		{

			// Create a message pump
			MSG msg;
			::PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

			// Thread message loop
			BOOL getMessageRet;
			while ((getMessageRet = ::GetMessage(&msg, NULL, 0, 0)) != 0)
			{
				if (getMessageRet != -1)
				{
					::TranslateMessage(&msg);
					::DispatchMessage(&msg);
				}
			}

			// Leave the window open so we can read the log file
			wcout << endl << L"Press [ENTER] to exit." << endl;
			cin.ignore();
		}
		else
		{
			wcout << L"Error starting. Quiting.";
			return E_FAIL;
		}
	}

	return S_OK;
}
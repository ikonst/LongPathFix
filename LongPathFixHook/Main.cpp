#define WIN32_LEAN_AND_MEAN

#include "Error.h"
#include "Path.h"

#include <Windows.h>
#include <stdio.h>
#include <stdlib.h> // for _countof
#include <MinHook.h>

WCHAR g_ModuleName[MAX_PATH];

//////////////////////////////////////////////////////////////////
//
// CreateFileW/A
//
//////////////////////////////////////////////////////////////////

// Pointer to original function
HANDLE (WINAPI *fpCreateFileW)(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);

HANDLE WINAPI CreateFileW_Detour(
  _In_      LPCWSTR lpFileName,
  _In_      DWORD dwDesiredAccess,
  _In_      DWORD dwShareMode,
  _In_opt_  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
  _In_      DWORD dwCreationDisposition,
  _In_      DWORD dwFlagsAndAttributes,
  _In_opt_  HANDLE hTemplateFile)
{
	WCHAR Buffer[256];
	LPCWSTR NewPath = CanonizePath(lpFileName, Buffer);
	if (NewPath == NULL)
	{
		SetLastError(ERROR_OUTOFMEMORY);
		return INVALID_HANDLE_VALUE;
	}
	HANDLE hFile = fpCreateFileW(NewPath, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
	if (NewPath != lpFileName && NewPath != Buffer)
		delete []NewPath;
	return hFile;
}

HANDLE WINAPI CreateFileA_Detour(
  _In_      LPCSTR lpFileName,
  _In_      DWORD dwDesiredAccess,
  _In_      DWORD dwShareMode,
  _In_opt_  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
  _In_      DWORD dwCreationDisposition,
  _In_      DWORD dwFlagsAndAttributes,
  _In_opt_  HANDLE hTemplateFile)
{
	DWORD RequiredSize = MultiByteToWideChar(CP_ACP, 0, lpFileName, -1, NULL, 0);
	if (RequiredSize == 0)
		return INVALID_HANDLE_VALUE;

	if (RequiredSize < 256)
	{
		WCHAR Buffer[256];
		if (MultiByteToWideChar(CP_ACP, 0, lpFileName, -1, Buffer, _countof(Buffer)) == 0)
			return INVALID_HANDLE_VALUE;
		return CreateFileW_Detour(Buffer, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
	}
	else
	{
		WCHAR *Buffer = new WCHAR[RequiredSize];
		if (Buffer == NULL)
		{
			SetLastError(ERROR_OUTOFMEMORY);
			return INVALID_HANDLE_VALUE;
		}
		if (MultiByteToWideChar(CP_ACP, 0, lpFileName, -1, Buffer, RequiredSize) == 0)
		{
			delete[] Buffer;
			return INVALID_HANDLE_VALUE;
		}
		HANDLE hFile = CreateFileW_Detour(Buffer, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
		delete [] Buffer;
		return hFile;
	}
}

//////////////////////////////////////////////////////////////////
//
// GetFileAttributesW/A
//
//////////////////////////////////////////////////////////////////

// Pointer to original function
DWORD (WINAPI *fpGetFileAttributesW)(LPCWSTR lpFileName);

DWORD WINAPI GetFileAttributesW_Detour(
    __in LPCWSTR lpFileName)
{
	WCHAR Buffer[256];
	LPCWSTR NewPath = CanonizePath(lpFileName, Buffer);
	if (NewPath == NULL)
	{
		SetLastError(ERROR_OUTOFMEMORY);
		return INVALID_FILE_ATTRIBUTES;
	}
	DWORD ret = fpGetFileAttributesW(NewPath);
	if (NewPath != lpFileName && NewPath != Buffer)
		delete []NewPath;
	return ret;
}

DWORD WINAPI GetFileAttributesA_Detour(
    __in LPCSTR lpFileName)
{
	DWORD RequiredSize = MultiByteToWideChar(CP_ACP, 0, lpFileName, -1, NULL, 0);
	if (RequiredSize == 0)
		return INVALID_FILE_ATTRIBUTES;

	if (RequiredSize < 256)
	{
		WCHAR Buffer[256];
		if (MultiByteToWideChar(CP_ACP, 0, lpFileName, -1, Buffer, _countof(Buffer)) == 0)
			return INVALID_FILE_ATTRIBUTES;
		return GetFileAttributesW_Detour(Buffer);
	}
	else
	{
		WCHAR *Buffer = new WCHAR[RequiredSize];
		if (Buffer == NULL)
		{
			SetLastError(ERROR_OUTOFMEMORY);
			return INVALID_FILE_ATTRIBUTES;
		}
		if (MultiByteToWideChar(CP_ACP, 0, lpFileName, -1, Buffer, RequiredSize) == 0)
		{
			delete[] Buffer;
			return INVALID_FILE_ATTRIBUTES;
		}
		DWORD FileAttributes = GetFileAttributesW_Detour(Buffer);
		delete [] Buffer;
		return FileAttributes;
	}
}

//////////////////////////////////////////////////////////////////
//
// CreateProcessW/A
//
//////////////////////////////////////////////////////////////////

// Pointer to original widechar function
BOOL (WINAPI *fpCreateProcessW)(LPCWSTR lpApplicationName, LPWSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation);

// Pointer to original ANSI function
BOOL (WINAPI *fpCreateProcessA)(LPCSTR lpApplicationName, LPSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCSTR lpCurrentDirectory, LPSTARTUPINFOA lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation);

// Inject a DLL into the target process by creating a new thread at LoadLibrary.
// Waits for injected thread to finish and returns its exit code.
bool LoadLibraryInjection(HANDLE hProcess, LPCWSTR LibraryName)
{
	// Allocate remote memory for LibraryName
	size_t LibraryNameSize = (wcslen(LibraryName) + 1) * sizeof(WCHAR);
	LPVOID RemoteLibraryName = (LPVOID)VirtualAllocEx(hProcess, NULL, LibraryNameSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (RemoteLibraryName == NULL)
	{
		return false;
	}

	// Write LibraryName to remote process
	if (WriteProcessMemory(hProcess, (LPVOID)RemoteLibraryName, LibraryName, LibraryNameSize, NULL) == 0)
	{
		DWORD error = GetLastError();
		VirtualFreeEx(hProcess, RemoteLibraryName, 0, MEM_RELEASE); // free the memory we were going to use
		SetLastError(error);
		return false;
	}

	// Load library into target process
	HANDLE hThread = CreateRemoteThread(hProcess, NULL, NULL, (LPTHREAD_START_ROUTINE)LoadLibraryW, (LPVOID)RemoteLibraryName, 0, NULL);
	if (hThread == NULL)
	{
		DWORD error = GetLastError();
		VirtualFreeEx(hProcess, RemoteLibraryName, 0, MEM_RELEASE);
		SetLastError(error);
		return false;
	}

	// Wait for the LoadLibraryW thread to finish
	WaitForSingleObject(hThread, INFINITE);

	// Lets see what it says
	DWORD dwThreadExitCode;
	if (!GetExitCodeThread(hThread,  &dwThreadExitCode))
	{
		DWORD error = GetLastError();
		CloseHandle(hThread);
		SetLastError(error);
		return false;
	}

	// Done with the LoadLibraryW thread
	CloseHandle(hThread);

	// Free remote memory
	VirtualFreeEx(hProcess, RemoteLibraryName, 0, MEM_RELEASE);

	if (dwThreadExitCode == 0) // LoadLibrary failed
	{
		// Get Win32 last error from process
		hThread = CreateRemoteThread(hProcess, NULL, NULL, (LPTHREAD_START_ROUTINE)GetLastError, (LPVOID)NULL, 0, NULL);
		if (hThread == NULL)
		{
			return false;
		}

		// Wait for GetLastError to finish within remote process and get result
		WaitForSingleObject(hThread, INFINITE);
		if (!GetExitCodeThread(hThread,  &dwThreadExitCode))
		{
			DWORD error = GetLastError();
			CloseHandle(hThread);
			SetLastError(error);
			return false;
		}

		// Done with the GetLastError thread
		CloseHandle(hThread);

		SetLastError(dwThreadExitCode);
		return false;
	}

	return true;
}

BOOL WINAPI CreateProcessW_Detour(
    __in_opt    LPCWSTR lpApplicationName,
    __inout_opt LPWSTR lpCommandLine,
    __in_opt    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    __in_opt    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    __in        BOOL bInheritHandles,
    __in        DWORD dwCreationFlags,
    __in_opt    LPVOID lpEnvironment,
    __in_opt    LPCWSTR lpCurrentDirectory,
    __in        LPSTARTUPINFOW lpStartupInfo,
    __out       LPPROCESS_INFORMATION lpProcessInformation
    )
{
	PROCESS_INFORMATION pi;
	if (lpProcessInformation == NULL)
		lpProcessInformation = &pi;

	// Start our new process with a suspended main thread
	BOOL CreateProcessResult = fpCreateProcessW(lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, dwCreationFlags | CREATE_SUSPENDED, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation);

	if (CreateProcessResult)
	{
		// Inject our DLL
		// This method returns only after the injected DLL loads and initializes.
		if (!LoadLibraryInjection(lpProcessInformation->hProcess, g_ModuleName))
		{
			PrintError(L"WARNING: Failed to inject DLL %s", g_ModuleName);
		}

		// Once the injection thread has returned it is safe to resume the main thread,
		// but only if the caller hasn't requested CREATE_SUSPENDED themselves.
		if ((dwCreationFlags & CREATE_SUSPENDED) == 0)
			ResumeThread(lpProcessInformation->hThread);
	}

	return CreateProcessResult;
}

BOOL WINAPI CreateProcessA_Detour(
    __in_opt    LPCSTR lpApplicationName,
    __inout_opt LPSTR lpCommandLine,
    __in_opt    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    __in_opt    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    __in        BOOL bInheritHandles,
    __in        DWORD dwCreationFlags,
    __in_opt    LPVOID lpEnvironment,
    __in_opt    LPCSTR lpCurrentDirectory,
	__in        LPSTARTUPINFOA lpStartupInfo,
    __out       LPPROCESS_INFORMATION lpProcessInformation
    )
{
	PROCESS_INFORMATION pi;
	if (lpProcessInformation == NULL)
		lpProcessInformation = &pi;

	// Start our new process with a suspended main thread
	BOOL CreateProcessResult = fpCreateProcessA(lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, dwCreationFlags | CREATE_SUSPENDED, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation);

	if (CreateProcessResult)
	{
		// Inject our DLL
		// This method returns only after the injected DLL loads and initializes.
		if (!LoadLibraryInjection(lpProcessInformation->hProcess, g_ModuleName))
		{
			PrintError(L"WARNING: Failed to inject DLL %s", g_ModuleName);
		}

		// Once the injection thread has returned it is safe to resume the main thread,
		// but only if the caller hasn't requested CREATE_SUSPENDED themselves.
		if ((dwCreationFlags & CREATE_SUSPENDED) == 0)
			ResumeThread(lpProcessInformation->hThread);
	}

	return CreateProcessResult;
}

//////////////////////////////////////////////////////////////////

BOOL Init(HINSTANCE hinstDLL)
{
	if (GetModuleFileName(hinstDLL, g_ModuleName, MAX_PATH) == 0)
		return FALSE;

    // Initialize MinHook
    if (MH_Initialize() != MH_OK)
		return FALSE;

    // Create hooks in disabled state
    if (MH_CreateHook(&CreateFileW, &CreateFileW_Detour, (LPVOID*)&fpCreateFileW) != MH_OK)
		return FALSE;
    if (MH_CreateHook(&CreateFileA, &CreateFileA_Detour, NULL) != MH_OK) // no need for original
		return FALSE;
    if (MH_CreateHook(&GetFileAttributesW, &GetFileAttributesW_Detour, (LPVOID*)&fpGetFileAttributesW) != MH_OK)
		return FALSE;
    if (MH_CreateHook(&GetFileAttributesA, &GetFileAttributesA_Detour, NULL) != MH_OK) // no need for original
		return FALSE;
    if (MH_CreateHook(&CreateProcessW, &CreateProcessW_Detour, (LPVOID*)&fpCreateProcessW) != MH_OK)
		return FALSE;
    if (MH_CreateHook(&CreateProcessA, &CreateProcessA_Detour, (LPVOID*)&fpCreateProcessA) != MH_OK)
		return FALSE;

	// Enable the hooks
	if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK)
		return FALSE;

	return TRUE;
}

BOOL Uninit()
{
	// Disable all hooks
	if (MH_DisableHook(MH_ALL_HOOKS) != MH_OK)
		return FALSE;

	// Uninitialize MinHook
	if (MH_Uninitialize() != MH_OK)
		return FALSE;

	return TRUE;
}

BOOL WINAPI DllMain(
  _In_  HINSTANCE hinstDLL,
  _In_  DWORD fdwReason,
  _In_  LPVOID lpvReserved
)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		return Init(hinstDLL);
	case DLL_PROCESS_DETACH:
		return Uninit();
	default:
		return TRUE;
	}
}

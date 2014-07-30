#define WIN32_LEAN_AND_MEAN

#include "Error.h"
#include "Path.h"

#include <Windows.h>
#include <stdio.h>
#include <MinHook.h>

WCHAR g_ModuleName[MAX_PATH];

//////////////////////////////////////////////////////////////////
//
// CreateFileW
//
//////////////////////////////////////////////////////////////////

// Pointer to original function
HANDLE (WINAPI *fpCreateFileW)(LPCTSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);

HANDLE WINAPI CreateFileW_Detour(
  _In_      LPCTSTR lpFileName,
  _In_      DWORD dwDesiredAccess,
  _In_      DWORD dwShareMode,
  _In_opt_  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
  _In_      DWORD dwCreationDisposition,
  _In_      DWORD dwFlagsAndAttributes,
  _In_opt_  HANDLE hTemplateFile)
{
	LPCWSTR NewPath = CanonizePath(lpFileName);
	if (NewPath == NULL)
	{
		SetLastError(ERROR_OUTOFMEMORY);
		return INVALID_HANDLE_VALUE;
	}
	HANDLE hFile = fpCreateFileW(NewPath, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
	if (NewPath != lpFileName)
		delete []NewPath;
	return hFile;
}

//////////////////////////////////////////////////////////////////
//
// GetFileAttributesW
//
//////////////////////////////////////////////////////////////////

// Pointer to original function
DWORD (WINAPI *fpGetFileAttributesW)(LPCWSTR lpFileName);

DWORD WINAPI GetFileAttributesW_Detour(
    __in LPCWSTR lpFileName)
{
	LPCWSTR NewPath = CanonizePath(lpFileName);
	if (NewPath == NULL)
	{
		SetLastError(ERROR_OUTOFMEMORY);
		return INVALID_FILE_ATTRIBUTES;
	}
	DWORD ret = fpGetFileAttributesW(NewPath);
	if (NewPath != lpFileName)
		delete []NewPath;
	return ret;
}

//////////////////////////////////////////////////////////////////
//
// CreateProcessW
//
//////////////////////////////////////////////////////////////////

// Pointer to original function
BOOL (WINAPI *fpCreateProcessW)(LPCWSTR lpApplicationName, LPWSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation);

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
    if (MH_CreateHook(&CreateProcessW, &CreateProcessW_Detour, (LPVOID*)&fpCreateProcessW) != MH_OK)
		return FALSE;
    if (MH_CreateHook(&GetFileAttributesW, &GetFileAttributesW_Detour, (LPVOID*)&fpGetFileAttributesW) != MH_OK)
		return FALSE;

    // Enable the hooks
    if (MH_EnableHook(&CreateFileW) != MH_OK)
		return FALSE;
    if (MH_EnableHook(&CreateProcessW) != MH_OK)
		return FALSE;
    if (MH_EnableHook(&GetFileAttributesW) != MH_OK)
		return FALSE;

	return TRUE;
}

BOOL Uninit()
{
    // Disable hooks
    if (MH_DisableHook(&CreateFileW) != MH_OK)
		return FALSE;
    if (MH_DisableHook(&CreateProcessW) != MH_OK)
		return FALSE;
    if (MH_DisableHook(&GetFileAttributesW) != MH_OK)
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

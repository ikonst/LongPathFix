#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <Psapi.h>

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

// Skips the "argv[0]" portion of the command line
LPCWSTR GetChildCommandLine()
{
	const WCHAR *Next;

	LPCWSTR CmdLine = GetCommandLineW();
	if (CmdLine[0] == '"')
	{
		Next = wcschr(CmdLine + 1, L'"');
		if (Next == NULL)
			return NULL;
		CmdLine = Next + 1;
	}
	else
	{
		Next = wcschr(CmdLine, L' ');
		if (Next == NULL)
			return NULL;
		CmdLine = Next + 1;
	}

	// Skip any repeating spaces between "argv[0]" and first argument
	while (*CmdLine == ' ')
		++CmdLine;

	return CmdLine;
}

void PrintError(LPCWSTR format, ...)
{
	va_list vl;
	va_start(vl, format);

	DWORD error = GetLastError();

	vfwprintf(stderr, format, vl);

	LPWSTR Message;
	if (FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, 0, error, 0, (LPWSTR)&Message, 0, NULL))
	{
		fwprintf(stderr, L": %s\n", Message);
		LocalFree(Message);
	}

	va_end(vl);
}

const WCHAR DllName[] =
#ifdef _WIN64
	L"LongPathFix_x64.dll";
#else
	L"LongPathFix_x86.dll";
#endif

#if defined(CONSOLE)
int wmain(int argc, const wchar_t* argv[])
#else
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
#endif
{
	// Extract command line
	LPCWSTR CmdLine = GetChildCommandLine();
	if (CmdLine == NULL || CmdLine[0] == '\0')
	{
		WCHAR ExecutablePath[MAX_PATH];
		if (!GetModuleFileName(NULL, ExecutablePath, MAX_PATH))
			return -1;
		WCHAR *ExecutableName = wcsrchr(ExecutablePath, L'\\') + 1;
		wprintf(L"Syntax: %s [file.exe] arguments\n", ExecutableName);
		return -1;
	}

	LPWSTR CmdLineCopy = _wcsdup(CmdLine); // CreateProcess requires writeable lpCommandLine
	if (CmdLineCopy == NULL)
		return -1;

	// Get own directory and append DllName
	WCHAR DllPath[MAX_PATH];
	if (!GetModuleFileName(NULL, DllPath, MAX_PATH))
		return -1;
	wcscpy(wcsrchr(DllPath, L'\\') + 1, DllName);

	// Load hooks into current processs
	if (!LoadLibraryW(DllPath))
	{
		PrintError(L"Error loading %s", DllPath);
		return -1;
	}

	// Start our new process with a suspended main thread
	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	GetStartupInfo(&si);

	// Start process
	BOOL CreateProcessResult = CreateProcess(NULL, CmdLineCopy, NULL, NULL, 0, 0, NULL, NULL, &si, &pi);
	free(CmdLineCopy);
	if (!CreateProcessResult)
	{
		PrintError(L"Error starting \"%s\"", CmdLine);
		return -1;
	}

	// Wait for the target application to exit
	WaitForSingleObject(pi.hProcess, INFINITE);

	return 0;
}

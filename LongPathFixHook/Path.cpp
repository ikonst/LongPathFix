#include "Path.h"

#include <stdlib.h>
#include <string.h>

const WCHAR NtPathPrefix[] = L"\\\\?\\";
const WCHAR NtUncPrefix[] = L"\\\\?\\UNC\\";

// Pass those as=is
const WCHAR DevicePrefix[] = L"\\\\.\\";
const WCHAR NtDevicePrefix[] = L"\\??\\"; // TODO: is equivalent to NtPathPrefix?

// Special device names that shold be skipped
const WCHAR *DeviceNames[] =
{
	L"CONIN$", // this is not a real device; cannot be prefixed by "\\.\"
	L"CONOUT$", // this is not a real device; cannot be prefixed by "\\.\"
	L"CON",
	L"PRN",
	L"AUX",
	L"NUL",
	L"COM1",
	L"COM2",
	L"COM3",
	L"COM4",
	L"COM5",
	L"COM6",
	L"COM7",
	L"COM8",
	L"COM9",
	L"LPT1",
	L"LPT2",
	L"LPT3",
	L"LPT4",
	L"LPT5",
	L"LPT6",
	L"LPT7",
	L"LPT8",
	L"LPT9"
};

LPCWSTR FindSep(LPCWSTR Path)
{
	while (*Path != '/' && *Path != '\\' && *Path != '\0')
		++Path;

	return Path;
}

template<size_t BufferSize> LPCWSTR CanonizePath(LPCWSTR Path, WCHAR (&Buffer)[BufferSize])
{
	if (Path[0] == '\0')
		return Path;

	// Skip device names
	for (int i=0; i<_countof(DeviceNames); ++i)
	{
		if (wcsicmp(Path, DeviceNames[i]) == 0)
			return Path;
	}

	if (wcsncmp(Path, NtPathPrefix, _countof(NtPathPrefix)-1) == 0)
		return Path; // already NT path

	if (wcsncmp(Path, DevicePrefix, _countof(DevicePrefix)-1) == 0)
		return Path; // device prefix

	if (wcsncmp(Path, NtDevicePrefix, _countof(NtDevicePrefix)-1) == 0)
		return Path; // device prefix

	// TODO: Note that \\.\C:\foo\..\foo\bar is also:
	//        1) normalized by CreateFile
	//        2) susceptible to MAX_PATH limitation

	size_t len = wcslen(Path);

	const WCHAR *src = Path;

	WCHAR *NewPath;
	WCHAR *dst;

	if (Path[0] == L'\\' || Path[0] == L'/')
	{
		// root path ("\foo\bar")

		WCHAR Dir[MAX_PATH];
		DWORD DirLength = GetCurrentDirectoryW(MAX_PATH, Dir);
		if (DirLength == 0)
			return NULL;
		WCHAR Drive = Dir[0];

		// allocate buffer if needed
		DWORD RequiredSize = (_countof(NtPathPrefix) - 1) + 2 + len + 1;
		NewPath = (RequiredSize > BufferSize) ? new WCHAR[RequiredSize] : Buffer;
		if (NewPath == NULL)
			return NULL;
		dst = NewPath;

		wcscpy(dst, NtPathPrefix);
		dst += _countof(NtPathPrefix) - 1;

		*dst = toupper(Drive);
		++dst;
		*dst = ':';
		++dst;
		*dst = '\\';
		++dst;

		src += 1; // skip the first slash
	}
	else if (Path[0] != '\0' && Path[1] == ':')
	{
		// absolute path ("c:\foo\bar")

		// allocate buffer if needed
		DWORD RequiredSize = (_countof(NtPathPrefix) - 1) + len + 1;
		NewPath = (RequiredSize > BufferSize) ? new WCHAR[RequiredSize] : Buffer;
		if (NewPath == NULL)
			return NULL;
		dst = NewPath;

		wcscpy(dst, NtPathPrefix);
		dst += _countof(NtPathPrefix) - 1;

		*dst = toupper(*src);
		++dst;
		*dst = ':';
		++dst;
		*dst = '\\';
		++dst;
		src += 3;
	}
	else if (Path[0] == '\\' && Path[1] == '\\')
	{
		// UNC (\\foo\bar\baz)

		// allocate buffer if needed
		DWORD RequiredSize = (_countof(NtUncPrefix) - 1) + len + 1;
		NewPath = (RequiredSize > BufferSize) ? new WCHAR[RequiredSize] : Buffer;
		if (NewPath == NULL)
			return NULL;
		dst = NewPath;

		wcscpy(dst, NtUncPrefix);
		dst += _countof(NtUncPrefix) - 1;

		src += 2;
	}
	else
	{
		// relative path

		WCHAR Dir[MAX_PATH];
		DWORD DirLength = GetCurrentDirectoryW(MAX_PATH, Dir); // DirLength does not include NUL
		if (DirLength == 0)
			return NULL;

		// allocate buffer if needed
		DWORD RequiredSize = (_countof(NtPathPrefix) - 1) + (DirLength) + 1 + len + 1;
		NewPath = (RequiredSize > BufferSize) ? new WCHAR[RequiredSize] : Buffer;
		if (NewPath == NULL)
			return NULL;
		dst = NewPath;

		wcscpy(dst, NtPathPrefix);
		dst += _countof(NtPathPrefix) - 1;

		wcscpy(dst, Dir);
		dst += DirLength;

		*dst = L'\\';
		++dst;
	}

	while (*src != '\0')
	{
		LPCWSTR sep = FindSep(src);
		size_t len = sep - src;

		if (sep == src)
		{
			// skip
		}
		else if (wcsncmp(src, L".", len) == 0)
		{
			// skip
		}
		else if (wcsncmp(src, L"..", len) == 0)
		{
			// Move dst cursor:
			//
			// dst = \\?\C:\foo\bar\
			//              ^   ^   ^ cursor
			//              |   |
			//              |   +-new cursor
			//              |
			//              +-NewPath + 7
			//
			// move dst back to the previous backslash
			for (dst -= 2; *dst != L'\\' && dst > NewPath + 7; --dst);
			++dst;
		}
		else
		{
			wcsncpy(dst, src, len);
			dst += len;

			if (*sep != '\0')
			{
				*dst = L'\\'; // append a backslash (never a forward-slash)
				++dst;
			}
		}

		if (*sep == '\0')
			break;
		src = sep + 1;
	}

	*dst = '\0';

	return NewPath;
}

// Instantitate for the common buffer size
template LPCWSTR CanonizePath(LPCWSTR Path, WCHAR (&Buffer)[256]);

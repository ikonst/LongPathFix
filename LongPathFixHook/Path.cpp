#include "Path.h"

#include <stdlib.h>
#include <string.h>

const WCHAR NtPathPrefix[] = L"\\\\?\\";
const WCHAR NtOtherPathPrefix[] = L"\\??\\";

LPCWSTR FindSep(LPCWSTR Path)
{
	while (*Path != '/' && *Path != '\\' && *Path != '\0')
		++Path;

	return Path;
}

LPCWSTR CanonizePath(LPCWSTR Path)
{
	if (Path[0] == '\0')
		return Path;

	if (wcsncmp(Path, NtPathPrefix, _countof(NtPathPrefix)-1) == 0)
		return Path; // already NT path

	if (wcsncmp(Path, NtOtherPathPrefix, _countof(NtOtherPathPrefix)-1) == 0)
		return Path; // already NT path

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

		NewPath = new WCHAR[(_countof(NtPathPrefix) - 1) + 2 + len + 1];
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

		NewPath = new WCHAR[(_countof(NtPathPrefix) - 1) + len + 1];
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
	else
	{
		// relative path

		WCHAR Dir[MAX_PATH];
		DWORD DirLength = GetCurrentDirectoryW(MAX_PATH, Dir); // DirLength does not include NUL
		if (DirLength == 0)
			return NULL;

		NewPath = new WCHAR[(_countof(NtPathPrefix) - 1) + (DirLength) + 1 + len + 1];
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

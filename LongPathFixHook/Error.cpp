#include "Error.h"

#include <stdio.h>

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

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <conio.h>
#include <assert.h>

#include "../LongPathFixHook/Path.h"

void test(LPCWSTR TestPath, LPCWSTR ExpectedResult)
{
	WCHAR Buffer[256];

	LPCWSTR Result = CanonizePath(TestPath, Buffer);
	assert(wcscmp(Result, ExpectedResult) == 0);
	if (Result != TestPath && Result != Buffer)
		delete[] Result;
}

int wmain(int argc, const wchar_t* argv[])
{
	test(L"c:\\foo\\bar", L"\\\\?\\C:\\foo\\bar");
	test(L"c:\\foo\\..\\bar", L"\\\\?\\C:\\bar");
	test(L"c:\\foo\\..\\..", L"\\\\?\\C:\\");
	test(L"c:\\foo\\..\\..\\..", L"\\\\?\\C:\\");
	test(L"c:\\foo\\..\\..\\..\\", L"\\\\?\\C:\\");
	test(L"c:\\foo", L"\\\\?\\C:\\foo");
	test(L"c:\\", L"\\\\?\\C:\\");
	test(L"c:", L"\\\\?\\C:");

	// test UNC paths
	test(L"\\\\", L"\\\\?\\UNC\\");
	test(L"\\\\.", L"\\\\?\\UNC\\.");
	test(L"\\\\host", L"\\\\?\\UNC\\host");
	test(L"\\\\host\\", L"\\\\?\\UNC\\host\\");
	test(L"\\\\host\\", L"\\\\?\\UNC\\host\\");
	test(L"\\\\host\\share", L"\\\\?\\UNC\\host\\share");
	test(L"\\\\host\\share\\", L"\\\\?\\UNC\\host\\share\\");
	test(L"\\\\host\\share\\..", L"\\\\?\\UNC\\host\\");
	test(L"\\\\host\\share\\..\\..", L"\\\\?\\UNC\\host\\");
	test(L"\\\\host\\share\\..\\..\\", L"\\\\?\\UNC\\host\\");

	// test no-ops
	test(L"\\\\?\\", L"\\\\?\\");

	_getch();
}

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

template<size_t BufferSize> LPCWSTR CanonizePath(LPCWSTR Path, WCHAR (&Buffer)[BufferSize]);

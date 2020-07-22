This utility adds long path (> MAX PATH = 260 characters) support to existing Windows executables.

Traditionally Windows filesystem APIs would [limit non-UNC paths to 260 characters](https://docs.microsoft.com/en-us/windows/win32/fileio/naming-a-file#maximum-path-length-limitation). LongPathFix is a usermode utility that hooks into filesystem API calls (with [MinHook](https://github.com/TsudaKageyu/minhook)) and modifies non-UNC paths to UNC paths in an attempt to enable long path support in arbitrary executables. Since UNC paths don't go through path expansion, LongPathFix also attempts to expand `.` and `..` references.

For new versions of Windows, follow [Enable Long Paths in Windows 10, Version 1607, and Later](https://docs.microsoft.com/en-us/windows/win32/fileio/naming-a-file#enable-long-paths-in-windows-10-version-1607-and-later).

Usage
=====

Run either `LongPathFix_x86.exe [foo.exe] args to foo` or `LongPathFix_x64.exe [foo.exe] args to foo` (for 32-bit and 64-bit executables respectively).

Child processes of your executable will also have the fix applied on them, as long as they are of the same 32/64-bit architecture as their parent.

Known limitations
=================

* Only a handful of APIs are converted.
* 64-bit applications executing 32-bit child processes and vice versa are not supported.

Motivation
==========

This utility was created to workaround [clang](http://llvm.org/bugs/show_bug.cgi?id=20440) and [gcc](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=61922) issues on Win32, but it might come in handy for other purposes.

Credits
=======

* MinHook - The Minimalistic API Hooking Library for x64/x86
 
  Copyright (C) 2009-2014 Tsuda Kageyu.

* Hacker Disassembler Engine 64 C

  Copyright (c) 2008-2009, Vyacheslav Patkov.

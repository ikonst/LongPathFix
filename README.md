LongPathFix
===========

This utility adds long path (> MAX_PATH = 260 characters) support to existing Win32 applications.

Many Windows file APIs are limited to 260 characters unless you prefix with the path with \??\ (and then you cannot use relative paths, "..", etc.). This fix converts regular paths to \??\-style paths on the fly, in an attmept to add long path support to an arbitrary application.

Usage
=====

Simply run, depending on whether foobar.exe is a 32-bit or 64-bit executable:

`LongPathFix_x86.exe [foobar.exe] args to foobar`

or

`LongPathFix_x64.exe [foobar.exe] args to foobar`

Child processes of your app will also have the fix applied on them, as long as they are of the same 32/64-bit architecture as their parent.

Known limitations
=================

* Only a handful of APIs are converted.
* Only Unicode applications are supported.
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

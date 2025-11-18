/*
# cr.h

A single file header-only live reload solution for C, written in C++:

- simple public API, 3 functions to use only (and another to export);
- works and tested on Linux, MacOSX and Windows;
- automatic crash protection;
- automatic static state transfer;
- based on dynamic reloadable binary (.so/.dylib/.dll);
- support multiple plugins;
- MIT licensed;

NOTE: The only file that matters in this repository is `cr.h`.

This file contains the documentation in markdown, the license, the
implementation and the public api. All other files in this repository are
supporting files and can be safely ignored.

### Building cr - Using vcpkg

You can download and install cr using the
[vcpkg](https://github.com/Microsoft/vcpkg) dependency manager:

    git clone https://github.com/Microsoft/vcpkg.git
    cd vcpkg
    ./bootstrap-vcpkg.sh
    ./vcpkg integrate install
    ./vcpkg install cr

The cr port in vcpkg is kept up to date by Microsoft team members and community
contributors. If the version is out of date, please [create an issue or pull
request](https://github.com/Microsoft/vcpkg) on the vcpkg repository.

### Building cr - Using CMake FetchContent

FetchContent_Declare(cr GIT_REPOSITORY https://github.com/fungos/cr GIT_TAG
master) FetchContent_MakeAvailable(cr)

### Example

A (thin) host application executable will make use of `cr` to manage
live-reloading of the real application in the form of dynamic loadable binary, a
host would be something like:

```c
#define CR_HOST // required in the host only and before including cr.h
#include "cr.h"

int main(int argc, char *argv[]) {
    // the host application should initalize a plugin with a context, a plugin
    cr_plugin ctx;

    // the full path to the live-reloadable application
    cr_plugin_open(ctx, "c:/path/to/build/game.dll");

    // call the update function at any frequency matters to you, this will give
    // the real application a chance to run
    while (!cr_plugin_update(ctx)) {
        // do anything you need to do on host side (ie. windowing and input
stuff?)
    }

    // at the end do not forget to cleanup the plugin context
    cr_plugin_close(ctx);
    return 0;
}
```

While the guest (real application), would be like:

```c
CR_EXPORT int cr_main(struct cr_plugin *ctx, enum cr_op operation) {
    assert(ctx);
    switch (operation) {
        case CR_LOAD:   return on_load(...); // loading back from a reload
        case CR_UNLOAD: return on_unload(...); // preparing to a new reload
        case CR_CLOSE: ...; // the plugin will close and not reload anymore
    }
    // CR_STEP
    return on_update(...);
}
```

### Changelog

#### 2025-03-30

- Removed FIPS and moved to pure CMake.
- As a result, cr.h has been moved into the cr directory.
- Using cr as a cmake dependency (`target_link_libraries(<my_target> PRIVATE
cr)`) will expose the cr.h header file to the target.

#### 2020-04-19

- Added a failure `CR_INITIAL_FAILURE`. If the initial plugin crashes, the host
must determine the next path, and we will not reload the broken plugin.

#### 2020-01-09

- Deprecated `cr_plugin_load` in favor to `cr_plugin_open` for consistency with
`cr_plugin_close`. See issue #49.
- Minor documentation improvements.

#### 2018-11-17

- Support to OSX finished, thanks to MESH Consultants Inc.
- Added a new possible failure `CR_BAD_IMAGE` in case the binary file is stil
not ready even if its timestamp changed. This could happen if generating the
file (compiler or copying) was slow.
- Windows: Fix issue with too long paths causing the PDB patch process to fail,
causing the reload process to fail.
- **Possible breaking change:** Fix rollback flow. Before, during a rollback
(for any reason) two versions were decremented one-shot so that the in following
load, the version would bump again getting us effectively on the previous
version, but in some cases not related to crashes this wasn't completely valid
(see `CR_BAD_IMAGE`). Now the version is decremented one time in the crash
handler and then another time during the rollback and then be bumped again. A
rollback due an incomplete image will not incorrectly rollback two versions, it
will continue at the same version retrying the load until the image is valid
(copy or compiler finished writing to it). This may impact current uses of `cr`
if the `version` info is used during `CR_UNLOAD` as it will now be a different
value.

### Samples

Two simple samples can be found in the `samples` directory.

The first is one is a simple console application that demonstrate some basic
static states working between instances and basic crash handling tests. Print to
output is used to show what is happening.

The second one demonstrates how to live-reload an opengl application using
 [Dear ImGui](https://github.com/ocornut/imgui). Some state lives in the host
 side while most of the code is in the guest side.

 ![imgui sample](https://i.imgur.com/Nq6s0GP.gif)

#### Samples and Tests

To build, use the given CMake preset:

```
$ cmake --preset Default .
$ cmake --build build
```

To run the tests, you can use the vscode Launch Tests option (Windows only,
currently), or:

```
$ cd build/tests
$ ctest build
```

To use the basic sample, you can use the vscode Launch basic sample option
(Windows only, currently), or:

```
$ cd build/samples/basic
$ ./basic_host # or basic_host_b

# Edit basic_guest.c, or just:
$ touch basic_guest.c

# rebuild
$ cmake --build ../../
```

For the imgui sample, after building:
```
$ cd build/samples/imgui
$ ./imgui_host

# Edit imgui_guest.cpp, or just:
$ touch imgui_guest.cpp

# rebuild
$ cmake --build ../../
```

### Documentation

#### `int (*cr_main)(struct cr_plugin *ctx, enum cr_op operation)`

This is the function pointer to the dynamic loadable binary entry point
function.

Arguments

- `ctx` pointer to a context that will be passed from `host` to the `guest`
containing valuable information about the current loaded version, failure reason
and user data. For more info see `cr_plugin`.
- `operation` which operation is being executed, see `cr_op`.

Return

- A negative value indicating an error, forcing a rollback to happen and failure
 being set to `CR_USER`. 0 or a positive value that will be passed to the
  `host` process.

#### `bool cr_plugin_open(cr_plugin &ctx, const char *fullpath)`

Loads and initialize the plugin.

Arguments

- `ctx` a context that will manage the plugin internal data and user data.
- `fullpath` full path with filename to the loadable binary for the plugin or
 `NULL`.

Return

- `true` in case of success, `false` otherwise.

#### `int cr_plugin_update(cr_plugin &ctx, bool reloadCheck = true)`

This function will call the plugin `cr_main` function. It should be called as
 frequently as the core logic/application needs.

Arguments

- `ctx` the current plugin context data.
- `reloadCheck` optional: do a disk check (stat()) to see if the dynamic library
needs a reload.

Return

- -1 if a failure happened during an update;
- -2 if a failure happened during a load or unload;
- anything else is returned directly from the plugin `cr_main`.

#### `void cr_plugin_close(cr_plugin &ctx)`

Cleanup internal states once the plugin is not required anymore.

Arguments

- `ctx` the current plugin context data.

#### `cr_op`

Enum indicating the kind of step that is being executed by the `host`:

- `CR_LOAD` A load caused by reload is being executed, can be used to restore
any saved internal state.
- `CR_STEP` An application update, this is the normal and most frequent
operation;
- `CR_UNLOAD` An unload for reloading the plugin will be executed, giving the
 application one chance to store any required data;
- `CR_CLOSE` Used when closing the plugin, This works like `CR_UNLOAD` but no
`CR_LOAD` should be expected afterwards;

#### `cr_plugin`

The plugin instance context struct.

- `p` opaque pointer for internal cr data;
- `userdata` may be used by the user to pass information between reloads;
- `version` incremetal number for each succeded reload, starting at 1 for the
 first load. **The version will change during a crash handling process**;
- `failure` used by the crash protection system, will hold the last failure
error code that caused a rollback. See `cr_failure` for more info on possible
values;

#### `cr_failure`

If a crash in the loadable binary happens, the crash handler will indicate the
 reason of the crash with one of these:

- `CR_NONE` No error;
- `CR_SEGFAULT` Segmentation fault. `SIGSEGV` on Linux/OSX or
 `EXCEPTION_ACCESS_VIOLATION` on Windows;
- `CR_ILLEGAL` In case of illegal instruction. `SIGILL` on Linux/OSX or
 `EXCEPTION_ILLEGAL_INSTRUCTION` on Windows;
- `CR_ABORT` Abort, `SIGBRT` on Linux/OSX, not used on Windows;
- `CR_MISALIGN` Bus error, `SIGBUS` on Linux/OSX or
`EXCEPTION_DATATYPE_MISALIGNMENT` on Windows;
- `CR_BOUNDS` Is `EXCEPTION_ARRAY_BOUNDS_EXCEEDED`, Windows only;
- `CR_STACKOVERFLOW` Is `EXCEPTION_STACK_OVERFLOW`, Windows only;
- `CR_STATE_INVALIDATED` Static `CR_STATE` management safety failure;
- `CR_BAD_IMAGE` The plugin is not a valid image (i.e. the compiler may still
writing it);
- `CR_OTHER` Other signal, Linux only;
- `CR_USER` User error (for negative values returned from `cr_main`);

#### `CR_HOST` define

This define should be used before including the `cr.h` in the `host`, if
`CR_HOST` is not defined, `cr.h` will work as a public API header file to be
used in the `guest` implementation.

Optionally `CR_HOST` may also be defined to one of the following values as a way
 to configure the `safety` operation mode for automatic static state management
  (`CR_STATE`):

- `CR_SAFEST` Will validate address and size of the state data sections during
 reloads, if anything changes the load will rollback;
- `CR_SAFE` Will validate only the size of the state section, this mean that the
 address of the statics may change (and it is best to avoid holding any pointer
  to static stuff);
- `CR_UNSAFE` Will validate nothing but that the size of section fits, may not
 be necessarelly exact (growing is acceptable but shrinking isn't), this is the
 default behavior;
- `CR_DISABLE` Completely disable automatic static state management;

#### `CR_STATE` macro

Used to tag a global or local static variable to be saved and restored during a
reload.

Usage

`static bool CR_STATE bInitialized = false;`

#### Overridable macros

You can define these macros before including cr.h in host (CR_HOST) to customize
cr.h memory allocations and other behaviours:

- `CR_MAIN_FUNC`: changes 'cr_main' symbol to user-defined function name.
default: #define CR_MAIN_FUNC "cr_main"
- `CR_ASSERT`: override assert. default: #define CA_ASSERT(e) assert(e)
- `CR_REALLOC`: override libc's realloc. default: #define CR_REALLOC(ptr, size)
::realloc(ptr, size)
- `CR_MALLOC`: override libc's malloc. default: #define CR_MALLOC(size)
::malloc(size)
- `CR_FREE`: override libc's free. default: #define CR_FREE(ptr) ::free(ptr)
- `CR_DEBUG`: outputs debug messages in CR_ERROR, CR_LOG and CR_TRACE
- `CR_ERROR`: logs debug messages to stderr. default (CR_DEBUG only): #define
CR_ERROR(...) fprintf(stderr, __VA_ARGS__)
- `CR_LOG`: logs debug messages. default (CR_DEBUG only): #define CR_LOG(...)
fprintf(stdout, __VA_ARGS__)
- `CR_TRACE`: prints function calls. default (CR_DEBUG only): #define
CR_TRACE(...) fprintf(stdout, "CR_TRACE: %s\n", __FUNCTION__)

### FAQ / Troubleshooting

#### Q: Why?

A: Read about why I made this
[here](https://fungos.github.io/blog/2017/11/20/cr.h-a-simple-c-hot-reload-header-only-library/).

#### Q: My application asserts/crash when freeing heap data allocated inside the
dll, what is happening?

A: Make sure both your application host and your dll are using the dynamic
 run-time (/MD or /MDd) as any data allocated in the heap must be freed with
  the same allocator instance, by sharing the run-time between guest and
   host you will guarantee the same allocator is being used.

#### Q: Can we load multiple plugins at the same time?

A: Yes. This should work without issues on Windows. On Linux and OSX there may
be issues with crash handling

#### Q: You said this wouldn't lock my PDB, but it still locks! Why?

If you had to load the dll before `cr` for any reason, Visual Studio may still
hold a lock to the PDB. You may be having [this
issue](https://github.com/fungos/cr/issues/12) and the solution is
[here](https://stackoverflow.com/questions/38427425/how-to-force-visual-studio-2015-to-unlock-pdb-file-after-freelibrary-call).

#### Q: Hot-reload is not working at all, what I'm doing wrong?

First, be sure that your build system is not interfering by somewhat still
linking to your shared library. There are so many things that can go wrong and
you need to be sure only `cr` will deal with your shared library. On linux, for
more info on how to find what is happening, check [this
issue](https://github.com/fungos/cr/issues/9).

#### Q: How much can I change things in the plugin without risking breaking
everything?

`cr` is `C` reloader and dealing with C it assume simple things will mostly
work.

The problem is how the linker will decide do rearrange things accordingly the
amount of changes you do in the code. For incremental and localized changes I
never had any issues, in general I hardly had any issues at all by writing
normal C code. Now, when things start to become more complex and bordering C++,
it becomes riskier. If you need do complex things, I suggest checking
[RCCPP](https://github.com/RuntimeCompiledCPlusPlus/RuntimeCompiledCPlusPlus)
and reading [this
PDF](http://www.gameaipro.com/GameAIPro/GameAIPro_Chapter15_Runtime_Compiled_C++_for_Rapid_AI_Development.pdf)
and my original blog post about `cr`
[here](https://fungos.github.io/blog/2017/11/20/cr.h-a-simple-c-hot-reload-header-only-library/).

With all these information you'll be able to decide which is better to your use
case.

### `cr` Sponsors

![MESH](https://static1.squarespace.com/static/5a5f5f08aeb625edacf9327b/t/5a7b78aa8165f513404129a3/1534346581876/?format=150w)

#### [MESH Consultants Inc.](http://meshconsultants.ca/)
**For sponsoring the port of `cr` to the MacOSX.**

### Contributors

[Danny Grein](https://github.com/fungos)

[Rokas Kupstys](https://github.com/rokups)

[Noah Rinehart](https://github.com/noahrinehart)

[Niklas Lundberg](https://github.com/datgame)

[Sepehr Taghdisian](https://github.com/septag)

[Robert Gabriel Jakabosky](https://github.com/neopallium)

[@pixelherodev](https://github.com/pixelherodev)

[Alexander](https://github.com/clibequilibrium)

[Vikram Saran](https://github.com/vikhik)

### Contributing

We welcome *ALL* contributions, there is no minor things to contribute with,
even one letter typo fixes are welcome.

The only things we require is to test thoroughly, maintain code style and
keeping documentation up-to-date.

Also, accepting and agreeing to release any contribution under the same license.

----

### License

The MIT License (MIT)

Copyright (c) 2017 Danny Angelo Carminati Grein

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

----

### Source

<details>
<summary>View Source Code</summary>

```c
*/
#ifndef __CR_H__
#define __CR_H__

//
// Global OS specific defines/customizations
//
#if defined(_WIN32)
#define CR_PLUGIN(name) "" name ".dll"
#elif defined(__linux__)
#define CR_PLUGIN(name) "lib" name ".so"
#elif defined(__APPLE__)
#define CR_PLUGIN(name) "lib" name ".dylib"
#else
#error "Unknown/unsupported platform, please open an issue if you think this \
platform should be supported."
#endif // _WIN32 || __linux__ || __APPLE__

//
// Global compiler specific defines/customizations
//
#if defined(_MSC_VER)
#if defined(__cplusplus)
#define CR_EXPORT extern "C" __declspec(dllexport)
#define CR_IMPORT extern "C" __declspec(dllimport)
#else
#define CR_EXPORT __declspec(dllexport)
#define CR_IMPORT __declspec(dllimport)
#endif
#endif // defined(_MSC_VER)

#if defined(__GNUC__) // clang & gcc
#if defined(__cplusplus)
#define CR_EXPORT extern "C" __attribute__((visibility("default")))
#else
#define CR_EXPORT __attribute__((visibility("default")))
#endif
#define CR_IMPORT
#endif // defined(__GNUC__)

#if defined(__MINGW32__)
#undef CR_EXPORT
#if defined(__cplusplus)
#define CR_EXPORT extern "C" __declspec(dllexport)
#else
#define CR_EXPORT __declspec(dllexport)
#endif
#endif

// cr_mode defines how much we validate global state transfer between
// instances. The default is CR_UNSAFE, you can choose another mode by
// defining CR_HOST, ie.: #define CR_HOST CR_SAFEST
enum cr_mode
{
    CR_SAFEST = 0, // validate address and size of the state section, if
                   // anything changes the load will rollback
    CR_SAFE = 1,   // validate only the size of the state section, this means
                   // that address is assumed to be safe if avoided keeping
                   // references to global/static states
    CR_UNSAFE = 2, // don't validate anything but that the size of the section
                   // fits, may not be identical though
    CR_DISABLE = 3 // completely disable the auto state transfer
};

// cr_op is passed into the guest process to indicate the current operation
// happening so the process can manage its internal data if it needs.
enum cr_op
{
    CR_LOAD   = 0,
    CR_STEP   = 1,
    CR_UNLOAD = 2,
    CR_CLOSE  = 3,
};

enum cr_failure
{
    CR_NONE,              // No error
    CR_SEGFAULT,          // SIGSEGV / EXCEPTION_ACCESS_VIOLATION
    CR_ILLEGAL,           // illegal instruction (SIGILL) / EXCEPTION_ILLEGAL_INSTRUCTION
    CR_ABORT,             // abort (SIGBRT)
    CR_MISALIGN,          // bus error (SIGBUS) / EXCEPTION_DATATYPE_MISALIGNMENT
    CR_BOUNDS,            // EXCEPTION_ARRAY_BOUNDS_EXCEEDED
    CR_STACKOVERFLOW,     // EXCEPTION_STACK_OVERFLOW
    CR_STATE_INVALIDATED, // one or more global data section changed and does
                          // not safely match basically a failure of
                          // cr_plugin_validate_sections
    CR_BAD_IMAGE,         // The binary is not valid - compiler is still writing it
    CR_INITIAL_FAILURE,   // Plugin version 1 crashed, cannot rollback
    CR_OTHER,             // Unknown or other signal,
    CR_USER = 0x100,
};

typedef struct cr_plugin cr_plugin;

typedef int (*cr_plugin_main_func)(cr_plugin* ctx, enum cr_op operation);

// public interface for the plugin context, this has some user facing
// variables that may be used to manage reload feedback.
// - userdata may be used by the user to pass information between reloads
// - version is the reload counter (after loading the first instance it will
//   be 1, not 0)
// - failure is the (platform specific) last error code for any crash that may
//   happen to cause a rollback reload used by the crash protection system
struct cr_plugin
{
    void*           p;
    void*           userdata;
    unsigned int    version;
    enum cr_failure failure;
    unsigned int    next_version;
    unsigned int    last_working_version;
};

#ifndef CR_HOST

// Guest specific compiler defines/customizations
#if defined(_MSC_VER)
#pragma section(".state", read, write)
#define CR_STATE __declspec(allocate(".state"))
#endif // defined(_MSC_VER)

#if defined(__APPLE__)
#define CR_STATE __attribute__((used, section("__DATA,__state")))
#else
#if defined(__GNUC__) // clang & gcc
#define CR_STATE __attribute__((section(".state")))
#endif // defined(__GNUC__)
#endif

#else // #ifndef CR_HOST

// Overridable macros
#ifndef CR_LOG
#ifdef CR_DEBUG
#include <stdio.h>
#define CR_LOG(...) fprintf(stdout, __VA_ARGS__)
#else
#define CR_LOG(...)
#endif
#endif

#ifndef CR_ERROR
#ifdef CR_DEBUG
#include <stdio.h>
#define CR_ERROR(...) fprintf(stderr, __VA_ARGS__)
#else
#define CR_ERROR(...)
#endif
#endif

#ifndef CR_TRACE
#ifdef CR_DEBUG
#include <stdio.h>
#define CR_TRACE fprintf(stdout, "CR_TRACE: %s\n", __FUNCTION__);
#else
#define CR_TRACE
#endif
#endif

#ifndef CR_MAIN_FUNC
#define CR_MAIN_FUNC "cr_main"
#endif

#ifndef CR_ASSERT
#include <assert.h>
#define CR_ASSERT(e) assert(e)
#endif

#ifndef CR_REALLOC
#include <stdlib.h>
#define CR_REALLOC(ptr, size) realloc(ptr, size)
#endif

#ifndef CR_FREE
#include <stdlib.h>
#define CR_FREE(ptr) free(ptr)
#endif

#ifndef CR_MALLOC
#include <stdlib.h>
#define CR_MALLOC(size) malloc(size)
#endif

#if defined(_MSC_VER)
// we should probably push and pop this
#pragma warning(disable : 4003) // not enough actual parameters for macro 'identifier'
#endif

#define CR_DO_EXPAND(x) x##1337
#define CR_EXPAND(x)    CR_DO_EXPAND(x)

#if CR_EXPAND(CR_HOST) == 1337
#define CR_OP_MODE CR_UNSAFE
#else
#define CR_OP_MODE CR_HOST
#endif

#include <stdbool.h>
#include <stdint.h>

#if defined(_WIN32)
#define CR_PATH_SEPARATOR         '\\'
#define CR_PATH_SEPARATOR_INVALID '/'
#else
#define CR_PATH_SEPARATOR         '/'
#define CR_PATH_SEPARATOR_INVALID '\\'
#endif

const char* cr_path_get_filename(const char* path)
{
    const char* filename = NULL;
    for (const char* c = path; *c != 0; c++)
    {
        if (*c == CR_PATH_SEPARATOR)
            filename = c + 1;
    }
    return filename;
}
const char* cr_path_get_extension(const char* path)
{
    const char* ext = NULL;
    for (const char* c = path; *c != 0; c++)
    {
        if (*c == '.')
            ext = c;
    }
    return ext;
}

static int cr_version_path(const char* basepath, unsigned version, char* outbuf, size_t outbuflen)
{
    const char* name = cr_path_get_filename(basepath);
    CR_ASSERT(name);
    const char* ext = cr_path_get_extension(name);
    CR_ASSERT(ext);
    return snprintf(outbuf, outbuflen, "%.*s%u%s", (int)(ext - basepath), basepath, version, ext);
}

typedef enum CRSectionType
{
    CR_SECTION_TYPE_STATE,
    CR_SECTION_TYPE_BSS,
    CR_SECTION_TYPE_COUNT,
} CRSectionType;

typedef enum CRSectionVersion
{
    CR_SECTION_VERSION_BACKUP,
    CR_SECTION_VERSION_CURRENT,
    CR_SECTION_VERSION_COUNT,
} CRSectionVersion;

typedef struct cr_plugin_section
{
    CRSectionType type;
    intptr_t      base;
    char*         ptr;
    int64_t       size;
    void*         data;
} cr_plugin_section;

typedef struct cr_plugin_segment
{
    char*   ptr;
    int64_t size;
} cr_plugin_segment;

// keep track of some internal state about the plugin, should not be messed
// with by user
typedef struct cr_internal
{
    char    filepath[1024];
    int64_t timestamp;
    void*   handle;

    cr_plugin_main_func main;
    cr_plugin_segment   seg;
    cr_plugin_section   data[CR_SECTION_TYPE_COUNT][CR_SECTION_VERSION_COUNT];
    enum cr_mode        mode;
} cr_internal;

static bool cr_plugin_section_validate(cr_plugin* ctx, CRSectionType type, intptr_t vaddr, intptr_t ptr, int64_t size);
static void cr_plugin_sections_reload(cr_plugin* ctx, CRSectionVersion version);
static void cr_plugin_sections_store(cr_plugin* ctx);
static void cr_plugin_sections_backup(cr_plugin* ctx);
static void cr_plugin_reload(cr_plugin* ctx);
static int  cr_plugin_unload(cr_plugin* ctx, bool rollback, bool close);
static bool cr_plugin_changed(cr_plugin* ctx);
static bool cr_plugin_rollback(cr_plugin* ctx);
static int  cr_plugin_main(cr_plugin* ctx, enum cr_op operation);

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>

#include <dbghelp.h>

#if defined(_MSC_VER)
#pragma comment(lib, "dbghelp.lib")
#endif
typedef HMODULE so_handle;

static int cr_windows_convert_path(const char* in, wchar_t* out)
{
    int num = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, in, -1, out, MAX_PATH);
    if (num == 0)
        __debugbreak();

    return num;
}

static int64_t cr_last_write_time(const char* path)
{
    WCHAR wpath[MAX_PATH];
    cr_windows_convert_path(path, wpath);

    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(wpath, GetFileExInfoStandard, &fad))
    {
        return -1;
    }

    if (fad.nFileSizeHigh == 0 && fad.nFileSizeLow == 0)
    {
        return -1;
    }

    LARGE_INTEGER time;
    time.HighPart = fad.ftLastWriteTime.dwHighDateTime;
    time.LowPart  = fad.ftLastWriteTime.dwLowDateTime;

    return (int64_t)(time.QuadPart / 10000000 - 11644473600LL);
}

static bool cr_exists(const char* path)
{
    WCHAR wpath[MAX_PATH];
    cr_windows_convert_path(path, wpath);
    return GetFileAttributesW(wpath) != INVALID_FILE_ATTRIBUTES;
}

static bool cr_copy(const char* from, const char* to)
{
    WCHAR wfrom[MAX_PATH];
    WCHAR wto[MAX_PATH];
    cr_windows_convert_path(from, wfrom);
    cr_windows_convert_path(to, wto);
    return CopyFileW(wfrom, wto, FALSE) ? true : false;
}

static void cr_del(const char* path)
{
    WCHAR wpath[MAX_PATH];
    cr_windows_convert_path(path, wpath);
    DeleteFileW(wpath);
}

// If using Microsoft Visual C/C++ compiler we need to do some workaround the
// fact that the compiled binary has a fullpath to the PDB hardcoded inside
// it. This causes a lot of headaches when trying compile while debugging as
// the referenced PDB will be locked by the debugger.
// To solve this problem, we patch the binary to rename the PDB to something
// we know will be unique to our in-flight instance, so when debugging it will
// lock this unique PDB and the compiler will be able to overwrite the
// original one.
#if defined(_MSC_VER)
// RSDS Debug Information for PDB files
// http://www.godevtool.com/Other/pdb.htm
#define CR_RSDS_SIGNATURE 'SDSR'
typedef struct cr_rsds_hdr
{
    DWORD signature;
    GUID  guid;
    long  version;
    char  filename[1];
} cr_rsds_hdr;
_Static_assert(offsetof(cr_rsds_hdr, guid) == 4, "");
_Static_assert(offsetof(cr_rsds_hdr, version) == 20, "");
_Static_assert(offsetof(cr_rsds_hdr, filename) == 24, "");

static bool cr_duplicate_and_patch_dll_and_pdb(const char* next_path_dll)
{
    HANDLE fp      = NULL;
    HANDLE filemap = NULL;
    LPVOID mem     = 0;
    bool   success = false;

    {
        WCHAR wpath[MAX_PATH];
        cr_windows_convert_path(next_path_dll, wpath);
        fp = CreateFileW(
            wpath,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        if (fp == INVALID_HANDLE_VALUE)
            fp = NULL;
        if (fp)
            filemap = CreateFileMappingW(fp, NULL, PAGE_READWRITE, 0, 0, NULL);

        if (filemap)
            mem = MapViewOfFile(filemap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        CR_ASSERT(mem);
    }

    if (mem)
    {
        // https://microsoft.github.io/windows-docs-rs/doc/windows/Win32/System/SystemServices/struct.IMAGE_DOS_HEADER.html
        const PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)mem;
        CR_ASSERT(dosHeader->e_magic == IMAGE_DOS_SIGNATURE);

        // https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-image_nt_headers64
        // https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-image_file_header
        // e_lfanew = offset of exe file header
        const PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((char*)dosHeader + dosHeader->e_lfanew);
        CR_ASSERT(ntHeaders->Signature == IMAGE_NT_SIGNATURE);
        CR_ASSERT(ntHeaders->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC);

        // https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-image_section_header
        PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(ntHeaders);

        // https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-image_optional_header32
        // https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-image_data_directory
        const IMAGE_DATA_DIRECTORY entry = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
        CR_ASSERT(entry.VirtualAddress);
        CR_ASSERT(entry.Size == sizeof(IMAGE_DEBUG_DIRECTORY));

        cr_rsds_hdr* rsds = NULL;
        for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++, section++)
        {
            const DWORD pStart = section->VirtualAddress;
            const DWORD pEnd   = section->VirtualAddress + section->Misc.VirtualSize;
            if ((entry.VirtualAddress >= pStart) && (entry.VirtualAddress < pEnd))
            {
                const DWORD diff   = section->VirtualAddress - section->PointerToRawData;
                const DWORD offset = entry.VirtualAddress - diff;

                // https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-image_debug_directory
                const PIMAGE_DEBUG_DIRECTORY dir = mem + offset;

                CR_ASSERT(dir->Type == IMAGE_DEBUG_TYPE_CODEVIEW);
                CR_ASSERT(dir->SizeOfData >= sizeof(cr_rsds_hdr));

                rsds = (cr_rsds_hdr*)(mem + dir->PointerToRawData);
                CR_ASSERT(rsds->signature == CR_RSDS_SIGNATURE);

                break;
            }
        }
        CR_ASSERT(rsds);
        if (rsds)
        {
            // This is the path embedded in the DLL by your compiler (eg. C:\\path\\to\\plugin.pdb)
            char* embedded_pdb_path = rsds->filename;

            // Duplicate PDB
            char        next_pdb_path[MAX_PATH] = {0};
            const char* dll_ext                 = cr_path_get_extension(next_path_dll);
            snprintf(next_pdb_path, sizeof(next_pdb_path), "%.*s.pdb", (int)(dll_ext - next_path_dll), next_path_dll);
            success = cr_copy(embedded_pdb_path, next_pdb_path);

            // Patch DLL with new path
            // We append a version number to the file name, so the string length will be longer.
            // Replacing the path with only the new filename (no directory) appears to work fine
            const char* next_pdb_filename = cr_path_get_filename(next_pdb_path);

            size_t path_bufsize = 1 + strlen(embedded_pdb_path);
            CR_ASSERT(path_bufsize >= strlen(next_pdb_filename));
            snprintf(embedded_pdb_path, path_bufsize, "%s", next_pdb_filename);

            success &= true;
            CR_ASSERT(success);
        }
    }

    if (mem != NULL)
        UnmapViewOfFile(mem);
    if (filemap != NULL)
        CloseHandle(filemap);
    if ((fp != NULL) && (fp != INVALID_HANDLE_VALUE))
        CloseHandle(fp);

    return success;
}
#endif // _MSC_VER

static void
cr_pe_section_save(cr_plugin* ctx, CRSectionType type, int64_t vaddr, int64_t base, const IMAGE_SECTION_HEADER* shdr)
{
    const CRSectionVersion version = CR_SECTION_VERSION_CURRENT;
    cr_internal*           p       = (cr_internal*)ctx->p;
    cr_plugin_section*     data    = &p->data[type][version];

    const size_t old_size = data->size;
    data->base            = base;
    data->ptr             = (char*)vaddr;
    data->size            = shdr->SizeOfRawData;
    data->data            = CR_REALLOC(data->data, shdr->SizeOfRawData);
    if (old_size < shdr->SizeOfRawData)
    {
        memset((char*)data->data + old_size, '\0', shdr->SizeOfRawData - old_size);
    }
}

static bool cr_plugin_validate_sections(cr_plugin* ctx, so_handle handle, const char* imagefile, bool rollback)
{
    (void)imagefile;
    CR_ASSERT(handle);
    cr_internal* p = (cr_internal*)ctx->p;
    if (p->mode == CR_DISABLE)
    {
        return true;
    }
    PIMAGE_NT_HEADERS     ntHeaders      = ImageNtHeader(handle);
    ULONGLONG             base           = ntHeaders->OptionalHeader.ImageBase;
    IMAGE_SECTION_HEADER* sectionHeaders = (IMAGE_SECTION_HEADER*)(ntHeaders + 1);
    bool                  result         = true;
    for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i)
    {
        const IMAGE_SECTION_HEADER* sectionHeader = &sectionHeaders[i];
        const int64_t               size          = sectionHeader->SizeOfRawData;
        if (!strcmp((const char*)sectionHeader->Name, ".state"))
        {
            if (ctx->version || rollback)
            {
                result &= cr_plugin_section_validate(
                    ctx,
                    CR_SECTION_TYPE_STATE,
                    base + sectionHeader->VirtualAddress,
                    base,
                    size);
            }
            if (result)
            {
                CRSectionType sec = CR_SECTION_TYPE_STATE;
                cr_pe_section_save(ctx, sec, base + sectionHeader->VirtualAddress, base, sectionHeader);
            }
        }
        else if (!strcmp((const char*)sectionHeader->Name, ".bss"))
        {
            if (ctx->version || rollback)
            {
                result &= cr_plugin_section_validate(
                    ctx,
                    CR_SECTION_TYPE_BSS,
                    base + sectionHeader->VirtualAddress,
                    base,
                    size);
            }
            if (result)
            {
                CRSectionType sec = CR_SECTION_TYPE_BSS;
                cr_pe_section_save(ctx, sec, base + sectionHeader->VirtualAddress, base, sectionHeader);
            }
        }
    }
    return result;
}

static void cr_so_unload(cr_plugin* ctx)
{
    cr_internal* p = (cr_internal*)ctx->p;
    CR_ASSERT(p->handle);
    FreeLibrary((HMODULE)p->handle);
}

static so_handle cr_so_load(const char* path)
{
    WCHAR wpath[MAX_PATH];
    cr_windows_convert_path(path, wpath);
    HMODULE new_dll = LoadLibraryW(wpath);
    if (!new_dll)
    {
        CR_ERROR("Couldn't load plugin: %ld\n", GetLastError());
    }
    return new_dll;
}

static cr_plugin_main_func cr_so_symbol(so_handle handle)
{
    CR_ASSERT(handle);
    cr_plugin_main_func new_main = (cr_plugin_main_func)(void*)GetProcAddress(handle, CR_MAIN_FUNC);
    if (!new_main)
    {
        CR_ERROR("Couldn't find plugin entry point: %ld\n", GetLastError());
    }
    return new_main;
}

#ifdef __MINGW32__
#include <setjmp.h>
#include <signal.h>

static jmp_buf env;
static void    cr_signal_handler(int sig) { __builtin_longjmp(env, 1); }

static cr_failure cr_signal_to_failure(int sig)
{
    switch (sig)
    {
    case 0:
        return CR_NONE;
    case SIGILL:
        return CR_ILLEGAL;
    case SIGSEGV:
        return CR_SEGFAULT;
    case SIGABRT:
        return CR_ABORT;
    }
    return static_cast<cr_failure>(CR_OTHER + sig);
}
#endif

static void cr_plat_init()
{
#ifdef __MINGW32__
    signal(SIGILL, cr_signal_handler);
    signal(SIGSEGV, cr_signal_handler);
    signal(SIGABRT, cr_signal_handler);
#endif
}

static int cr_seh_filter(cr_plugin* ctx, unsigned long seh)
{
    if (ctx->version == 1)
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    ctx->version = ctx->last_working_version;
    switch (seh)
    {
    case EXCEPTION_ACCESS_VIOLATION:
        ctx->failure = CR_SEGFAULT;
        return EXCEPTION_EXECUTE_HANDLER;
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        ctx->failure = CR_ILLEGAL;
        return EXCEPTION_EXECUTE_HANDLER;
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        ctx->failure = CR_MISALIGN;
        return EXCEPTION_EXECUTE_HANDLER;
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        ctx->failure = CR_BOUNDS;
        return EXCEPTION_EXECUTE_HANDLER;
    case EXCEPTION_STACK_OVERFLOW:
        ctx->failure = CR_STACKOVERFLOW;
        return EXCEPTION_EXECUTE_HANDLER;
    default:
        break;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static int cr_plugin_main(cr_plugin* ctx, enum cr_op operation)
{
    cr_internal* p = (cr_internal*)ctx->p;
#if !defined(__MINGW32__)
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wlanguage-extension-token"
#endif
    __try
    {
        if (p->main)
        {
            return p->main(ctx, operation);
        }
    }
    __except (cr_seh_filter(ctx, GetExceptionCode()))
    {
        return -1;
    }
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#else
    if (int sig = __builtin_setjmp(env))
    {
        ctx->version = ctx->last_working_version;
        ctx->failure = cr_signal_to_failure(sig);
        CR_LOG("1 FAILURE: %d (CR: %d)\n", sig, ctx->failure);
        return -1;
    }
    else
    {
        CR_ASSERT(p);
        if (p->main)
        {
            return p->main(&ctx, operation);
        }
    }
#endif

    return -1;
}

#endif // _WIN32

#if defined(__linux__) || defined(__APPLE__)

#include <dlfcn.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ucontext.h>
#include <unistd.h>

#if defined(__linux__)
#include <sys/sendfile.h> // sendfile
#elif defined(__APPLE__)
#include <copyfile.h> // copyfile
#endif

typedef void* so_handle;

static int64_t cr_last_write_time(const char* path)
{
    struct stat stats;
    if (stat(path, &stats) == -1)
    {
        return -1;
    }

    if (stats.st_size == 0)
    {
        return -1;
    }

#if defined(__APPLE__)
    return stats.st_mtime;
#else
    return stats.st_mtim.tv_sec;
#endif
}

static bool cr_exists(const char* path)
{
    struct stat stats = {0};
    return stat(path, &stats) != -1;
}

static bool cr_copy(const char* from, const char* to)
{
#if defined(__linux__)
    // Reference:
    // http://www.informit.com/articles/article.aspx?p=23618&seqNum=13
    int         input, output;
    struct stat src_stat;
    if ((input = open(from.c_str(), O_RDONLY)) == -1)
    {
        return false;
    }
    fstat(input, &src_stat);

    if ((output = open(to.c_str(), O_WRONLY | O_CREAT, O_NOFOLLOW | src_stat.st_mode)) == -1)
    {
        close(input);
        return false;
    }

    int result = sendfile(output, input, NULL, src_stat.st_size);
    close(input);
    close(output);
    return result > -1;
#elif defined(__APPLE__)
    return copyfile(from, to, NULL, COPYFILE_ALL | COPYFILE_NOFOLLOW_DST) == 0;
#endif
}

static void cr_del(const char* path) { unlink(path); }

// unix,internal
// a helper function to validate that an area of memory is empty
// this is used to validate that the data in the .bss haven't changed
// and that we are safe to discard it and uses the new one.
bool cr_is_empty(const void* const buf, int64_t len)
{
    if (!buf || !len)
    {
        return true;
    }

    bool r = false;

    const char* const c = (const char* const)buf;
    for (int i = 0; i < len; ++i)
    {
        r |= c[i];
    }
    return !r;
}

#if defined(__linux__)
#include <elf.h>
#include <link.h>

static size_t cr_file_size(const std::string& path)
{
    struct stat stats;
    if (stat(path.c_str(), &stats) == -1)
    {
        return 0;
    }
    return static_cast<size_t>(stats.st_size);
}

// unix,internal
// save section information to be used during load/unload when copying
// around global state (from .bss and .state binary sections).
// vaddr = is the in memory loaded address of the segment-section
// base = is the in file section address
// shdr = the in file section header
template <class H>
void cr_elf_section_save(cr_plugin* ctx, CRSectionType type, int64_t vaddr, int64_t base, H shdr)
{
    const auto   version  = CR_SECTION_VERSION_CURRENT;
    auto         p        = (cr_internal*)ctx->p;
    auto         data     = &p->data[type][version];
    const size_t old_size = data->size;
    data->base            = base;
    data->ptr             = (char*)vaddr;
    data->size            = shdr.sh_size;
    data->data            = CR_REALLOC(data->data, shdr.sh_size);
    if (old_size < shdr.sh_size)
    {
        memset((char*)data->data + old_size, '\0', shdr.sh_size - old_size);
    }
}

// unix,internal
// validates that the sections being loaded are compatible with the previous
// one accordingly with desired `cr_mode` mode. If this is a first load, a
// validation is not necessary. At the same time it will initialize the
// section tracking information and alloc the required temporary space to use
// during unload.
template <class H>
bool cr_elf_validate_sections(cr_plugin* ctx, bool rollback, H shdr, int shnum, const char* sh_strtab_p)
{
    CR_ASSERT(sh_strtab_p);
    auto p      = (cr_internal*)ctx->p;
    bool result = true;
    for (int i = 0; i < shnum; ++i)
    {
        const char*   name          = sh_strtab_p + shdr[i].sh_name;
        auto          sectionHeader = shdr[i];
        const int64_t addr          = sectionHeader.sh_addr;
        const int64_t size          = sectionHeader.sh_size;
        const int64_t base          = (intptr_t)p->seg.ptr + p->seg.size;
        if (!strcmp(name, ".state"))
        {
            const int64_t vaddr = base - size;
            auto          sec   = CR_SECTION_TYPE_STATE;
            if (ctx->version || rollback)
            {
                result &= cr_plugin_section_validate(ctx, sec, vaddr, addr, size);
            }
            if (result)
            {
                cr_elf_section_save(ctx, sec, vaddr, addr, sectionHeader);
            }
        }
        else if (!strcmp(name, ".bss"))
        {
            // .bss goes past segment filesz, but it may be just padding
            const int64_t vaddr = base;
            auto          sec   = CR_SECTION_TYPE_BSS;
            if (ctx->version || rollback)
            {
                // this is kinda hack to skip bss validation if our data is zero
                // this means we don't care scrapping it, and helps skipping
                // validating a .bss that serves only as padding in the segment.
                if (!cr_is_empty(p->data[sec][0].data, p->data[sec][0].size))
                {
                    result &= cr_plugin_section_validate(ctx, sec, vaddr, addr, size);
                }
            }
            if (result)
            {
                cr_elf_section_save(ctx, sec, vaddr, addr, sectionHeader);
            }
        }
    }
    return result;
}

struct cr_ld_data
{
    cr_plugin*  ctx                  = NULL;
    int64_t     data_segment_address = 0;
    int64_t     data_segment_size    = 0;
    const char* fullname             = NULL;
};

// Iterate over all loaded shared objects and then for each one, iterates
// over each segment.
// So we find our plugin by filename and try to find the segment that
// contains our data sections (.state and .bss) to find their virtual
// addresses.
// We search segments with type PT_LOAD (1), meaning it is a loadable
// segment (anything that really matters ie. .text, .data, .bss, etc...)
// The segment where the p_memsz is bigger than p_filesz is the segment
// that contains the section .bss (if there is one or there is padding).
// Also, the segment will have sensible p_flags value (PF_W for exemple).
//
// Some useful references:
// http://www.skyfree.org/linux/references/ELF_Format.pdf
// https://eli.thegreenplace.net/2011/08/25/load-time-relocation-of-shared-libraries/
static int cr_dl_header_handler(struct dl_phdr_info* info, size_t, void* data)
{
    CR_ASSERT(info && data);
    auto p   = (cr_ld_data*)data;
    auto ctx = p->ctx;
    if (strcasecmp(info->dlpi_name, p->fullname))
    {
        return 0;
    }

    for (int i = 0; i < info->dlpi_phnum; i++)
    {
        auto phdr = info->dlpi_phdr[i];
        if (phdr.p_type != PT_LOAD)
        {
            continue;
        }

        // assume the first writable segment is the one that contains our
        // sections this may not be true I imagine, but if this becomes an
        // issue we fix it by comparing against section addresses, but this
        // will require some rework on the code flow.
        if (phdr.p_flags & PF_W)
        {
            auto pimpl      = (cr_internal*)ctx->p;
            pimpl->seg.ptr  = (char*)(info->dlpi_addr + phdr.p_vaddr);
            pimpl->seg.size = phdr.p_filesz;
            break;
        }
    }
    return 0;
}

static bool cr_plugin_validate_sections(cr_plugin* ctx, so_handle handle, const std::string& imagefile, bool rollback)
{
    CR_ASSERT(handle);
    cr_ld_data data;
    data.ctx   = &ctx;
    auto pimpl = (cr_internal*)ctx->p;
    if (pimpl->mode == CR_DISABLE)
    {
        return true;
    }
    data.fullname = imagefile.c_str();
    dl_iterate_phdr(cr_dl_header_handler, (void*)&data);

    const auto len    = cr_file_size(imagefile);
    char*      p      = NULL;
    bool       result = false;
    do
    {
        int fd = open(imagefile.c_str(), O_RDONLY);
        p      = (char*)mmap(0, len, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);

        // The ElfW() macro definition turns its argument into the name of an
        // ELF data type suitable for the hardware architecture. For example,
        // ElfW(Ehdr) yeilds the data type name Elf32_Ehdr on a 32-bit
        // platforms, and Elf64_Ehdr on 64-bit platforms.
        ElfW(Ehdr)* ehdr = (ElfW(Ehdr)*)p;
        if (ehdr->e_ident[EI_MAG0] != ELFMAG0 || ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
            ehdr->e_ident[EI_MAG2] != ELFMAG2 || ehdr->e_ident[EI_MAG3] != ELFMAG3)
        {
            break;
        }

        ElfW(Shdr*) shdr              = (ElfW(Shdr)*)(p + ehdr->e_shoff);
        auto              sh_strtab   = &shdr[ehdr->e_shstrndx];
        const char* const sh_strtab_p = p + sh_strtab->sh_offset;
        result                        = cr_elf_validate_sections(ctx, rollback, shdr, ehdr->e_shnum, sh_strtab_p);
    }
    while (0);

    if (p)
    {
        munmap(p, len);
    }

    if (!result)
    {
        ctx->failure = CR_STATE_INVALIDATED;
    }

    return result;
}

#elif defined(__APPLE__)
#include <dlfcn.h>
#include <limits.h> // PATH_MAX
#include <mach-o/dyld.h>
#include <mach-o/getsect.h>
#include <mach-o/ldsyms.h>
#include <stdlib.h> // realpath

#if __LP64__
typedef struct mach_header_64 macho_hdr;
#define CR_MH_MAGIC MH_MAGIC_64
#else
typedef struct mach_header macho_hdr;
#define CR_MH_MAGIC MH_MAGIC
#endif

// osx,internal
// save section information to be used during load/unload when copying
// around global state (from .bss and .state binary sections).
// vaddr = is the in memory loaded address of the segment-section
void cr_macho_section_save(cr_plugin* ctx, CRSectionType type, intptr_t addr, size_t size)
{

    cr_internal*       p        = (cr_internal*)ctx->p;
    cr_plugin_section* data     = &p->data[type][CR_SECTION_VERSION_CURRENT];
    const size_t       old_size = data->size;
    data->base                  = 0;
    data->ptr                   = (char*)addr;
    data->size                  = size;
    data->data                  = CR_REALLOC(data->data, size);
    if (old_size < size)
    {
        memset((char*)data->data + old_size, '\0', size - old_size);
    }
}

// Iterate over all loaded shared objects and then for each one to find
// our plugin by filename. Then knowing its image index we can get our
// data sections (__state and __bss) and calculate their virtual
// addresses.
//
// Some useful references:
// man 3 dyld
static bool cr_plugin_validate_sections(cr_plugin* ctx, so_handle handle, const char* imagefile, bool rollback)
{
    bool         result = true;
    cr_internal* pimpl  = (cr_internal*)ctx->p;
    if (pimpl->mode == CR_DISABLE)
    {
        return result;
    }
    CR_TRACE

    // resolve absolute path of the image, because _dyld_get_image_name returns
    // abs path
    char imageAbsPath[PATH_MAX + 1];
    if (!realpath(imagefile, imageAbsPath))
    {
        CR_ASSERT(0 && "resolving absolute path for plugin failed");
        return false;
    }

    const int count = (int)_dyld_image_count();
    for (int i = 0; i < count; i++)
    {
        const char* name = _dyld_get_image_name(i);

        if (strcasecmp(name, imageAbsPath))
        {
            // match loaded image filename
            continue;
        }

        const struct mach_header* hdr = _dyld_get_image_header(i);
        if (hdr->filetype != MH_DYLIB)
        {
            // assure it is a valid dylib
            continue;
        }

        intptr_t vaddr = _dyld_get_image_vmaddr_slide(i);
        (void)vaddr;
        // auto cmd_stride = sizeof(struct mach_header);
        if (hdr->magic != CR_MH_MAGIC)
        {
            // check for conforming mach-o header
            continue;
        }

        // auto validate_and_save = [&](cr_plugin_section_type::e sec, intptr_t addr, unsigned long size)
        // {
        //     if (addr != 0 && size != 0)
        //     {
        //         if (ctx->version || rollback)
        //         {
        //             result &= cr_plugin_section_validate(ctx, sec, addr, 0, size);
        //         }
        //         if (result)
        //         {
        //             cr_macho_section_save(ctx, sec, addr, size);
        //         }
        //     }
        // };

        macho_hdr*    mhdr = (macho_hdr*)hdr;
        unsigned long size = 0;
        intptr_t      ptr  = (intptr_t)getsectiondata(mhdr, SEG_DATA, "__bss", &size);

        if (ptr != 0 && size != 0)
        {
            if (ctx->version || rollback)
                result &= cr_plugin_section_validate(ctx, CR_SECTION_TYPE_BSS, ptr, 0, size);
            if (result)
                cr_macho_section_save(ctx, CR_SECTION_TYPE_BSS, ptr, size);
        }

        if (result)
        {
            ptr = (intptr_t)getsectiondata(mhdr, SEG_DATA, "__state", &size);
            if (ptr != 0 && size != 0)
            {
                if (ctx->version || rollback)
                    result &= cr_plugin_section_validate(ctx, CR_SECTION_TYPE_STATE, ptr, 0, size);
                if (result)
                    cr_macho_section_save(ctx, CR_SECTION_TYPE_STATE, ptr, size);
            }
        }
        break;
    }

    return result;
}

#endif

static void cr_so_unload(cr_plugin* ctx)
{
    CR_ASSERT(ctx->p);
    cr_internal* p = (cr_internal*)ctx->p;
    CR_ASSERT(p->handle);

    const int r = dlclose(p->handle);
    if (r)
    {
        CR_ERROR("Error closing plugin: %d\n", r);
    }

    p->handle = NULL;
    p->main   = NULL;
}

static so_handle cr_so_load(const char* new_file)
{
    dlerror();
    void* new_dll = dlopen(new_file, RTLD_NOW);
    if (!new_dll)
    {
        CR_ERROR("Couldn't load plugin: %s\n", dlerror());
    }
    return new_dll;
}

static cr_plugin_main_func cr_so_symbol(so_handle handle)
{
    CR_ASSERT(handle);
    dlerror();
    cr_plugin_main_func new_main = (cr_plugin_main_func)dlsym(handle, CR_MAIN_FUNC);
    if (!new_main)
    {
        CR_ERROR("Couldn't find plugin entry point: %s\n", dlerror());
    }
    return new_main;
}

sigjmp_buf env;

static void cr_signal_handler(int sig, siginfo_t* si, void* uap)
{
    CR_TRACE(void) uap;
    CR_ASSERT(si);
    siglongjmp(env, sig);
}

static void cr_plat_init()
{
    CR_TRACE
    static bool initialized = false;
    if (initialized)
    {
        return;
    }
    initialized = true;
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO | SA_RESTART | SA_NODEFER;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = cr_signal_handler;
#if defined(__linux__)
    sa.sa_restorer = NULL;
#endif

    if (sigaction(SIGILL, &sa, NULL) == -1)
    {
        CR_ERROR("Failed to setup SIGILL handler\n");
    }
    if (sigaction(SIGBUS, &sa, NULL) == -1)
    {
        CR_ERROR("Failed to setup SIGBUS handler\n");
    }
    if (sigaction(SIGSEGV, &sa, NULL) == -1)
    {
        CR_ERROR("Failed to setup SIGSEGV handler\n");
    }
    if (sigaction(SIGABRT, &sa, NULL) == -1)
    {
        CR_ERROR("Failed to setup SIGABRT handler\n");
    }
}

static enum cr_failure cr_signal_to_failure(int sig)
{
    switch (sig)
    {
    case 0:
        return CR_NONE;
    case SIGILL:
        return CR_ILLEGAL;
    case SIGBUS:
        return CR_MISALIGN;
    case SIGSEGV:
        return CR_SEGFAULT;
    case SIGABRT:
        return CR_ABORT;
    }
    return (enum cr_failure)(CR_OTHER + sig);
}

static int cr_plugin_main(cr_plugin* ctx, enum cr_op operation)
{
    int sig = sigsetjmp(env, 1);
    if (sig)
    {
        ctx->version = ctx->last_working_version;
        ctx->failure = cr_signal_to_failure(sig);
        CR_LOG("1 FAILURE: %d (CR: %d)\n", sig, ctx->failure);
        return -1;
    }
    else
    {
        cr_internal* p = (cr_internal*)ctx->p;
        CR_ASSERT(p);
        if (p->main)
        {
            return p->main(ctx, operation);
        }
    }

    return -1;
}

#endif // __linux__ || __APPLE__

static bool cr_plugin_load_internal(cr_plugin* ctx, bool rollback)
{
    CR_TRACE
    cr_internal* p = (cr_internal*)ctx->p;
    if (cr_exists(p->filepath) || rollback)
    {
        if (ctx->version)
        {
            CR_LOG("unload version #%u with rollback: %d\n", ctx->version, rollback);
        }
        int r = cr_plugin_unload(ctx, rollback, false);
        if (r < 0)
            return false;

        unsigned int new_version = rollback ? ctx->version : ctx->next_version;
        char         new_filepath[1024];
        cr_version_path(p->filepath, new_version, new_filepath, sizeof(new_filepath));
        if (rollback)
        {
            if (ctx->version == 0)
            {
                ctx->failure = CR_INITIAL_FAILURE;
                return false;
            }
            // Don't rollback to this version again, if it crashes.
            ctx->last_working_version = ctx->version > 0 ? ctx->version - 1 : 0;
        }
        else
        {
            // Save current version for rollback.
            ctx->last_working_version = ctx->version;
            cr_copy(p->filepath, new_filepath);

            // Update `next_version` for use by the next reload.
            ctx->next_version = new_version + 1;

#if defined(_MSC_VER)
            if (!cr_duplicate_and_patch_dll_and_pdb(new_filepath))
            {
                CR_ERROR("Couldn't process PDB, debugging may be "
                         "affected and/or reload may fail\n");
            }
#endif // defined(_MSC_VER)
        }

        so_handle new_dll = cr_so_load(new_filepath);
        if (!new_dll)
        {
            ctx->failure = CR_BAD_IMAGE;
            return false;
        }

        if (!cr_plugin_validate_sections(ctx, new_dll, new_filepath, rollback))
        {
            return false;
        }

        if (rollback)
        {
            cr_plugin_sections_reload(ctx, CR_SECTION_VERSION_BACKUP);
        }
        else if (ctx->version)
        {
            cr_plugin_sections_reload(ctx, CR_SECTION_VERSION_CURRENT);
        }

        cr_plugin_main_func new_main = cr_so_symbol(new_dll);
        if (!new_main)
        {
            return false;
        }

        cr_internal* p2 = (cr_internal*)ctx->p;
        p2->handle      = new_dll;
        p2->main        = new_main;
        if (ctx->failure != CR_BAD_IMAGE)
        {
            p2->timestamp = cr_last_write_time(p->filepath);
        }
        ctx->version = new_version;
        CR_LOG("loaded: %s (version: %d)\n", new_filepath, ctx->version);
    }
    else
    {
        CR_ERROR("Error loading plugin.\n");
        return false;
    }
    return true;
}

static bool cr_plugin_section_validate(cr_plugin* ctx, CRSectionType type, intptr_t ptr, intptr_t base, int64_t size)
{
    CR_TRACE(void) ptr;
    cr_internal* p = (cr_internal*)ctx->p;
    switch (p->mode)
    {
    case CR_SAFE:
        return (p->data[type][0].size == size);
    case CR_UNSAFE:
        return (p->data[type][0].size <= size);
    case CR_DISABLE:
        return true;
    default:
        break;
    }
    // CR_SAFEST
    return (p->data[type][0].base == base && p->data[type][0].size == size);
}

// internal
static void cr_plugin_sections_backup(cr_plugin* ctx)
{
    cr_internal* p = (cr_internal*)ctx->p;
    if (p->mode == CR_DISABLE)
    {
        return;
    }
    CR_TRACE

    for (int i = 0; i < CR_SECTION_TYPE_COUNT; ++i)
    {
        cr_plugin_section* cur = &p->data[i][CR_SECTION_VERSION_CURRENT];
        if (cur->ptr)
        {
            cr_plugin_section* bkp = &p->data[i][CR_SECTION_VERSION_BACKUP];
            bkp->data              = CR_REALLOC(bkp->data, cur->size);
            bkp->ptr               = cur->ptr;
            bkp->size              = cur->size;
            bkp->base              = cur->base;

            if (bkp->data)
            {
                memcpy(bkp->data, cur->data, bkp->size);
            }
        }
    }
}

// internal
// Before unloading iterate over possible global static state and keeps an
// internal copy to be used in next version load and a backup copy as a known
// valid state checkpoint. This is mostly due that a new load may want to
// modify the state and if anything bad happens we are sure to have a valid
// and compatible copy of the state for the previous version of the plugin.
static void cr_plugin_sections_store(cr_plugin* ctx)
{
    cr_internal* p = (cr_internal*)ctx->p;
    if (p->mode == CR_DISABLE)
    {
        return;
    }
    CR_TRACE

    CRSectionVersion version = CR_SECTION_VERSION_CURRENT;
    for (int i = 0; i < CR_SECTION_TYPE_COUNT; ++i)
    {
        if (p->data[i][version].ptr && p->data[i][version].data)
        {
            const char*   ptr = p->data[i][version].ptr;
            const int64_t len = p->data[i][version].size;
            memcpy(p->data[i][version].data, ptr, len);
        }
    }

    cr_plugin_sections_backup(ctx);
}

// internal
// After a load happens reload the global state from previous version from our
// internal copy created during the unload step.
static void cr_plugin_sections_reload(cr_plugin* ctx, CRSectionVersion version)
{
    CR_ASSERT(version < CR_SECTION_VERSION_COUNT);
    cr_internal* p = (cr_internal*)ctx->p;
    if (p->mode == CR_DISABLE)
    {
        return;
    }
    CR_TRACE

    for (int i = 0; i < CR_SECTION_TYPE_COUNT; ++i)
    {
        if (p->data[i][version].data)
        {
            const int64_t len = p->data[i][version].size;
            // restore backup into the current section address as it may
            // change due aslr and backup address may be invalid
            const CRSectionVersion current = CR_SECTION_VERSION_CURRENT;

            void* dest = (void*)p->data[i][current].ptr;
            if (dest)
                memcpy(dest, p->data[i][version].data, len);
        }
    }
}

// internal
// Cleanup and frees any temporary memory used to keep global static data
// between sessions, used during shutdown.
static void cr_so_sections_free(cr_plugin* ctx)
{
    CR_TRACE
    cr_internal* p = (cr_internal*)ctx->p;
    for (int i = 0; i < CR_SECTION_TYPE_COUNT; ++i)
    {
        for (int v = 0; v < CR_SECTION_VERSION_COUNT; ++v)
        {
            if (p->data[i][v].data)
            {
                CR_FREE(p->data[i][v].data);
            }
            p->data[i][v].data = NULL;
        }
    }
}

static bool cr_plugin_changed(cr_plugin* ctx)
{
    cr_internal*  p   = (cr_internal*)ctx->p;
    const int64_t src = cr_last_write_time(p->filepath);
    const int64_t cur = p->timestamp;
    return src > cur;
}

// internal
// Unload current running plugin, if it is not a rollback it will trigger a
// last update with `cr_op::CR_UNLOAD` (that may crash and cause another
// rollback, etc.) storing global static states to use with next load. If the
// unload is due a rollback, no `cr_op::CR_UNLOAD` is called neither any state
// is saved, giving opportunity to the previous version to continue with valid
// previous state.
static int cr_plugin_unload(cr_plugin* ctx, bool rollback, bool close)
{
    CR_TRACE
    cr_internal* p = (cr_internal*)ctx->p;
    int          r = 0;
    if (p->handle)
    {
        if (!rollback)
        {
            r = cr_plugin_main(ctx, close ? CR_CLOSE : CR_UNLOAD);
            // Don't store state if unload crashed.  Rollback will use backup.
            if (r < 0)
            {
                CR_LOG("4 FAILURE: %d\n", r);
            }
            else
            {
                cr_plugin_sections_store(ctx);
            }
        }
        cr_so_unload(ctx);
        p->handle = NULL;
        p->main   = NULL;
    }
    return r;
}

// internal
// Force a version rollback, causing a partial-unload and a load with the
// previous version, also triggering an update with `cr_op::CR_LOAD` that
// in turn may also cause more rollbacks.
static bool cr_plugin_rollback(cr_plugin* ctx)
{
    CR_TRACE
    bool loaded = cr_plugin_load_internal(ctx, true);
    if (loaded)
    {
        loaded = cr_plugin_main(ctx, CR_LOAD) >= 0;
        if (loaded)
        {
            ctx->failure = CR_NONE;
        }
    }
    return loaded;
}

// internal
// Checks if a rollback or a reload is needed, do the unload/loading and call
// update one time with `cr_op::CR_LOAD`. Note that this may fail due to crash
// handling during this first update, effectivelly rollbacking if possible and
// causing a consecutive `CR_LOAD` with the previous version.
static void cr_plugin_reload(cr_plugin* ctx)
{
    if (cr_plugin_changed(ctx))
    {
        CR_TRACE
        if (!cr_plugin_load_internal(ctx, false))
        {
            return;
        }
        int r = cr_plugin_main(ctx, CR_LOAD);
        if (r < 0 && !ctx->failure)
        {
            CR_LOG("2 FAILURE: %d\n", r);
            ctx->failure = CR_USER;
        }
    }
}

// This is basically the plugin `main` function, should be called as
// frequently as your core logic/application needs. -1 and -2 are the only
// possible return values from cr meaning a fatal error (causes rollback),
// other return values are returned directly from `cr_main`.
int cr_plugin_update(cr_plugin* ctx, bool reloadCheck)
{
    if (ctx->failure)
    {
        CR_LOG("1 ROLLBACK version was %d\n", ctx->version);
        cr_plugin_rollback(ctx);
        CR_LOG("1 ROLLBACK version is now %d\n", ctx->version);
#ifdef __MINGW32__
        cr_plat_init();
#endif
    }
    else
    {
        if (reloadCheck)
        {
            cr_plugin_reload(ctx);
        }
    }

    // -2 to differentiate from crash handling code path, meaning the crash
    // happened probably during load or unload and not update
    if (ctx->failure)
    {
        CR_LOG("3 FAILURE: -2\n");
        return -2;
    }

    int r = cr_plugin_main(ctx, CR_STEP);
    if (r < 0 && !ctx->failure)
    {
        CR_LOG("4 FAILURE: CR_USER\n");
        ctx->failure = CR_USER;
    }
    return r;
}

// Loads a plugin from the specified full path (or current directory if NULL).
bool cr_plugin_open(cr_plugin* ctx, const char* path)
{
    CR_TRACE
    CR_ASSERT(path);
    if (!cr_exists(path))
    {
        return false;
    }
    cr_internal* p = (cr_internal*)calloc(1, sizeof(*p));
    p->mode        = CR_OP_MODE;
    snprintf(p->filepath, sizeof(p->filepath), "%s", path);
    ctx->p                    = p;
    ctx->next_version         = 1;
    ctx->last_working_version = 0;
    ctx->version              = 0;
    ctx->failure              = CR_NONE;
    cr_plat_init();
    return true;
}

// 20200109 [DEPRECATED] Use `cr_plugin_open` instead.
bool cr_plugin_load(cr_plugin* ctx, const char* fullpath) { return cr_plugin_open(ctx, fullpath); }

// Call to cleanup internal state once the plugin is not required anymore.
void cr_plugin_close(cr_plugin* ctx)
{
    CR_TRACE
    const bool rollback = false;
    const bool close    = true;
    cr_plugin_unload(ctx, rollback, close);
    cr_so_sections_free(ctx);
    cr_internal* p = (cr_internal*)ctx->p;

    // delete backups
    for (unsigned int i = 0; i < ctx->version; i++)
    {
        char scratchpath[1024];
        cr_version_path(p->filepath, i, scratchpath, sizeof(scratchpath));
        cr_del(scratchpath);
#if defined(_MSC_VER)
        const char* ext = cr_path_get_extension(scratchpath);
        strcpy((char*)ext, ".pdb");
        cr_del(scratchpath);
#endif
    }

    free(p);
    ctx->p       = NULL;
    ctx->version = 0;
}

#endif // #ifndef CR_HOST

#endif // __CR_H__
       // clang-format off
/*
```

</details>
*/

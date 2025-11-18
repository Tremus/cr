#include <stdlib.h>
#define UNICODE

#include <stdint.h>
#define XHL_FILES_IMPL
#define XHL_TIME_IMPL
#include "xdebug.h"
#include "xfiles.h"
#include "xtime.h"

#ifdef __APPLE__
#include <unistd.h>
#endif

#define CR_ASSERT xassert

#define CR_HOST CR_UNSAFE // try to best manage static states

#include <signal.h>
#include <stdio.h>

#include <cr.h>

uint64_t g_last_file_change = 0; // throttle rebuild triggers

void my_cb(enum XFILES_WATCH_TYPE type, const char* path, void* udata)
{
    switch (type)
    {
    case XFILES_WATCH_CREATED:
        fprintf(stderr, "Created %s\n", path);
        break;
    case XFILES_WATCH_DELETED:
        fprintf(stderr, "Deleted %s\n", path);
        break;
    case XFILES_WATCH_MODIFIED:
        fprintf(stderr, "Modified %s\n", path);
        g_last_file_change = xtime_now_ns();
        // Recompile program?
        // Recompile shader?
        // Note that if you are modifying files in an IDE with a linter for formatter, you will likely get multiple
        // 'modified' callbacks. If you're hoping to recompile code, you may want to write your own throttle for
        // whatever actions you make in response
        break;
    }
}

int rebuild()
{
    uint64_t buildStart = xtime_now_ns();
    int      exitcode   = 0;

#ifdef _WIN32
    STARTUPINFO         si = {0};
    PROCESS_INFORMATION pi = {0};
    SECURITY_ATTRIBUTES sa = {0};
    HANDLE              hChildStdoutRd, hChildStdoutWr;

    sa.nLength              = sizeof(sa);
    sa.bInheritHandle       = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &sa, 0) ||
        !SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0))
    {
        fprintf(stderr, "Failed to create pipes.");
        xassert(false);
        return 1;
    }

    si.cb          = sizeof(si);
    si.dwFlags    |= STARTF_USESHOWWINDOW; // Stops a terminal window popping up as it runs the command
    si.hStdOutput  = hChildStdoutWr;
    si.dwFlags    |= STARTF_USESTDHANDLES; // Lets us use the stdout pipe

    // Run build command in child process.
    WCHAR cmdbuf[512];
    DWORD exitCode = 0;
    wcscpy_s(cmdbuf, ARRAYSIZE(cmdbuf), TEXT(HOTRELOAD_BUILD_COMMAND));
    if (!CreateProcessW(0, cmdbuf, 0, 0, TRUE, 0, NULL, NULL, &si, &pi))
    {
        fprintf(stderr, "CreateProcess failed (%lu).\n", GetLastError());
        return 1;
    }

    // Wait until child process exits
    WaitForSingleObject(pi.hProcess, INFINITE);

    char  buffer[4096] = {0};
    DWORD bytesRead    = 0;
    do
    {
        BOOL ok = ReadFile(hChildStdoutRd, buffer, sizeof(buffer) - 1, &bytesRead, NULL);
        if (ok)
            fwrite(buffer, 1, bytesRead, stderr);
    }
    while (bytesRead == sizeof(buffer) - 1);
    GetExitCodeProcess(pi.hProcess, &exitCode);

    // Cleanup build process
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hChildStdoutWr);

    exitcode = exitCode;
#else // _WIN32
    exitcode = system(HOTRELOAD_BUILD_COMMAND);
#endif

    if (exitcode != 0)
    {
        fprintf(stderr, "[WARNING] Rebuild failed. Exited with code: %d\n", exitcode);
    }
    else
    {
        uint64_t buildEnd   = xtime_now_ns();
        double   rebuild_ms = (double)(buildEnd - buildStart) / 1.e6;
        fprintf(stderr, "Rebuild time %.2fms\n", rebuild_ms);
    }
    return 0;
}

int  g_running = 1;
void ctrl_c_callback(int code)
{
    fprintf(stderr, "Terminating\n");
    g_running = 0;
}

int main(int argc, char* argv[])
{
    xtime_init();

    cr_plugin ctx;
    // the host application should initalize a plugin with a context, a plugin
    // filename without extension and the full path to the plugin
    cr_plugin_open(&ctx, HOTRELOAD_LIB_PATH);
    // call the plugin update function with the plugin context to execute it
    // at any frequency matters to you
    xfiles_watch_context_t watch_ctx = xfiles_watch_create(HOTRELOAD_WATCH_DIR, 0, my_cb);

    fprintf(stderr, "Press Crtl+C to exit\n");
    g_running = 1;
    signal(SIGINT, ctrl_c_callback);
    while (g_running)
    {
        cr_plugin_update(&ctx, true);
        fflush(stdout);
        fflush(stderr);

        int sleep_ms = 10;
#ifdef _WIN32
        Sleep(sleep_ms);
#else
        usleep(sleep_ms * 1000);
#endif

        xfiles_watch_flush(watch_ctx);

        uint64_t diff = 0;
        if (g_last_file_change)
            diff = xtime_now_ns() - g_last_file_change;

        if (diff > 20000000) // 20ms throttle
        {
            g_last_file_change = 0;
            int failed         = rebuild();
            if (failed)
                break;
        }
    }

    xfiles_watch_destroy(watch_ctx);

    // at the end do not forget to cleanup the plugin context, as it needs to
    // allocate some memory to track internal and plugin states
    cr_plugin_close(&ctx);
    return 0;
}

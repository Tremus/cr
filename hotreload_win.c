#define xassert(cond) (cond) ? (void)0 : __debugbreak();

#define CR_ASSERT xassert

#define CR_HOST CR_UNSAFE // try to best manage static states

#include <stdio.h>

#include <cr.h>

BYTE infobuffer[1024 * 32];

struct
{
    LARGE_INTEGER freq, start;
} g_Timer;

static inline INT64 GetNowNS()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    now.QuadPart -= g_Timer.start.QuadPart;
    INT64 q       = now.QuadPart / g_Timer.freq.QuadPart;
    INT64 r       = now.QuadPart % g_Timer.freq.QuadPart;
    return q * 1000000000 + r * 1000000000 / g_Timer.freq.QuadPart;
}

int main(int argc, char* argv[])
{
    QueryPerformanceFrequency(&g_Timer.freq);
    QueryPerformanceCounter(&g_Timer.start);

    HANDLE hDirectory = CreateFileW(
        TEXT(HOTRELOAD_WATCH_DIR),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL);
    xassert(hDirectory);

    // Setup overlapped
    OVERLAPPED overlapped;
    overlapped.hEvent = CreateEventW(NULL, FALSE, 0, NULL);

    // https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-readdirectorychangesw
    BOOL success = ReadDirectoryChangesW(
        hDirectory,
        infobuffer,
        sizeof(infobuffer),
        TRUE,
        FILE_NOTIFY_CHANGE_LAST_WRITE,
        NULL,
        &overlapped,
        NULL);
    if (!success)
    {
        fprintf(stderr, "Failed to queue info buffer\n");
        return 1;
    }

    cr_plugin ctx;
    // the host application should initalize a plugin with a context, a plugin
    // filename without extension and the full path to the plugin
    cr_plugin_open(&ctx, HOTRELOAD_LIB_PATH);
    // call the plugin update function with the plugin context to execute it
    // at any frequency matters to you
    while (true)
    {
        cr_plugin_update(&ctx, true);
        fflush(stdout);
        fflush(stderr);
        Sleep(10);

        DWORD result = WaitForSingleObject(overlapped.hEvent, 0);
        if (result == WAIT_OBJECT_0)
        {
            DWORD bytes_transferred;
            GetOverlappedResult(hDirectory, &overlapped, &bytes_transferred, TRUE);

            bool                     files_changed = false;
            FILE_NOTIFY_INFORMATION* event         = (FILE_NOTIFY_INFORMATION*)infobuffer;

            while (TRUE)
            {
                DWORD name_len = event->FileNameLength / sizeof(wchar_t);

                if (event->Action == FILE_ACTION_MODIFIED)
                {
                    fwprintf(stderr, L"File changed: %.*s\n", name_len, event->FileName);
                    files_changed = true;
                }

                // Iterate events
                if (event->NextEntryOffset)
                    *((BYTE**)&event) += event->NextEntryOffset;
                else
                    break;
            }

            // Queue next event
            success = ReadDirectoryChangesW(
                hDirectory,
                infobuffer,
                sizeof(infobuffer),
                TRUE,
                FILE_NOTIFY_CHANGE_LAST_WRITE,
                NULL,
                &overlapped,
                NULL);

            if (!success)
            {
                fprintf(stderr, "Failed to queue info buffer\n");
                return 1;
            }

            if (files_changed)
            {
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
                    return -1;
                }

                si.cb          = sizeof(si);
                si.dwFlags    |= STARTF_USESHOWWINDOW; // Stops a terminal window popping up as it runs the command
                si.hStdOutput  = hChildStdoutWr;
                si.dwFlags    |= STARTF_USESTDHANDLES; // Lets us use the stdout pipe

                UINT64 buildStart = GetNowNS();
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

                if (exitCode != 0)
                {
                    fprintf(stderr, "[WARNING] Rebuild failed. Exited with code: %lu\n", exitCode);
                }
                else
                {
                    UINT64 buildEnd   = GetNowNS();
                    double rebuild_ms = (double)(buildEnd - buildStart) / 1.e6;
                    fprintf(stderr, "Rebuild time %.2fms\n", rebuild_ms);
                }
            }
        }
    }

    // at the end do not forget to cleanup the plugin context, as it needs to
    // allocate some memory to track internal and plugin states
    cr_plugin_close(&ctx);
    return 0;
}

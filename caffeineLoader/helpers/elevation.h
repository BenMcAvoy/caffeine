#define WIN32_LEAN_AND_MEAN

#include <corecrt_wstdio.h>
#include <Windows.h>
#include <shellapi.h>

namespace Caffeine {
    inline bool IsElevated() {
        BOOL isAdmin = false;
        PSID adminGroup = nullptr;
        
        SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
        if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
            CheckTokenMembership(nullptr, adminGroup, &isAdmin);
            FreeSid(adminGroup);
        }
        
        return isAdmin;
    }
    
    inline void EnsureElevation() {
        if (!IsElevated()) {
            // We need to relaunch the application with elevated privileges
            wchar_t path[MAX_PATH];
            GetModuleFileNameW(nullptr, path, MAX_PATH);

            DWORD consoleOwnerPid = 0;
            HWND consoleHwnd = GetConsoleWindow();
            if (consoleHwnd) {
                DWORD pid = 0;
                GetWindowThreadProcessId(consoleHwnd, &pid);
                consoleOwnerPid = pid;
            }

            wchar_t params[128] = {};
            if (consoleOwnerPid != 0) {
                swprintf_s(params, L"--parent-console-pid=%u", consoleOwnerPid);
            }

            SHELLEXECUTEINFOW sei = { sizeof(sei) };
            sei.lpVerb = L"runas"; // Request elevation
            sei.lpFile = path;
            sei.lpParameters = params[0] ? params : nullptr;
            sei.nShow = SW_SHOWNORMAL;

            if (!ShellExecuteExW(&sei)) {
                MessageBoxW(nullptr, L"Failed to elevate privileges.", L"Error", MB_ICONERROR);
                ExitProcess(1);
            }

            ExitProcess(0);
        }
    }
}
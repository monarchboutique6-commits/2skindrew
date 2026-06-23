#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <psapi.h>

DWORD GetPID(const wchar_t* procName) {
    PROCESSENTRY32W entry{ sizeof(entry) };
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    DWORD pid = 0;
    if (Process32FirstW(snap, &entry)) {
        do {
            if (!_wcsicmp(entry.szExeFile, procName)) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return pid;
}

bool InjectDLL(DWORD pid, const char* dllPath) {
    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) {
        std::cout << "[-] OpenProcess failed. Error: " << GetLastError() << "\n";
        return false;
    }

    size_t pathLen = strlen(dllPath) + 1;
    LPVOID alloc = VirtualAllocEx(hProc, nullptr, pathLen, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!alloc) {
        std::cout << "[-] VirtualAllocEx failed\n";
        CloseHandle(hProc);
        return false;
    }

    if (!WriteProcessMemory(hProc, alloc, dllPath, pathLen, nullptr)) {
        std::cout << "[-] WriteProcessMemory failed\n";
        VirtualFreeEx(hProc, alloc, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return false;
    }

    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    LPTHREAD_START_ROUTINE loadLib = (LPTHREAD_START_ROUTINE)GetProcAddress(k32, "LoadLibraryA");

    HANDLE hThread = CreateRemoteThread(hProc, nullptr, 0, loadLib, alloc, 0, nullptr);
    if (!hThread) {
        std::cout << "[-] CreateRemoteThread failed. Error: " << GetLastError() << "\n";
        VirtualFreeEx(hProc, alloc, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    VirtualFreeEx(hProc, alloc, 0, MEM_RELEASE);
    CloseHandle(hProc);
    return true;
}

int main() {
    std::cout << "[*] Starting CS2 with -insecure...\n";

    ShellExecuteW(nullptr, L"open", L"steam://rungameid/730//-insecure -d3d11", nullptr, nullptr, SW_SHOWNORMAL);

    DWORD pid = 0;
    while (!(pid = GetPID(L"cs2.exe")))
        Sleep(300);

    std::cout << "[+] CS2 PID: " << pid << "\n";
    std::cout << "[*] Waiting for client.dll...\n";

    while (true) {
        HMODULE hMods[1024];
        DWORD cbNeeded;
        HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (EnumProcessModules(hProc, hMods, sizeof(hMods), &cbNeeded)) {
            for (unsigned i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
                char modName[MAX_PATH];
                if (GetModuleFileNameExA(hProc, hMods[i], modName, sizeof(modName))) {
                    if (strstr(modName, "client.dll")) {
                        std::cout << "[+] client.dll loaded\n";
                        CloseHandle(hProc);
                        goto ready;
                    }
                }
            }
        }
        CloseHandle(hProc);
        Sleep(500);
    }

ready:
    const char* dllPath = "C:\\Users\\makcu\\OneDrive\\Рабочий стол\\drewskins\\CS2_InternalSkinChanger2\\x64\\Release\\CS2_InternalSkinChanger2.dll";

    if (InjectDLL(pid, dllPath))
        std::cout << "[+] DLL injected successfully\n";
    else
        std::cout << "[-] Injection failed\n";

    system("pause");
    return 0;
}
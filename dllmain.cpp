#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <cstdint>
#include <d3d11.h>
#include <dxgi.h>
#include <vector>

#include "offsets.hpp"
#include "schema.hpp"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"

#pragma comment(lib, "d3d11.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

typedef HRESULT(__stdcall* PresentFn)(IDXGISwapChain*, UINT, UINT);
PresentFn oPresent = nullptr;

uintptr_t g_client = 0;
ID3D11Device* g_pDevice = nullptr;
ID3D11DeviceContext* g_pContext = nullptr;
ID3D11RenderTargetView* g_mainRTV = nullptr;
HWND g_hWindow = nullptr;
WNDPROC g_oWndProc = nullptr;
bool g_imguiInit = false;
bool g_showMenu = true;
bool g_enableChanger = true;

struct InvItem {
    int paintKit;
    int itemDef;
    const char* name;
};

std::vector<InvItem> g_invItems;

struct SkinOpt {
    const char* name;
    int paintKit;
    int itemDef;
};

SkinOpt g_opts[] = {
    {"AK-47 | Asiimov", 279, 7},
    {"AK-47 | Redline", 282, 7},
    {"AK-47 | Vulcan", 302, 7},
    {"AWP | Dragon Lore", 344, 9},
    {"AWP | Asiimov", 279, 9},
    {"M4A1-S | Cyrex", 360, 16},
    {"Glock-18 | Fade", 38, 4},
};

const int g_optCount = sizeof(g_opts) / sizeof(g_opts[0]);

int g_dbgCount = 0;
uintptr_t g_dbgArr = 0;
uintptr_t g_dbgWs = 0;
int g_dbgWeaponsFound = 0;
int g_dbgSkinsApplied = 0;
int g_dbgLastDefs[16] = { 0 };
int g_dbgLastDefCount = 0;

uintptr_t g_dbgItemAddr = 0;
uintptr_t g_dbgItemBase = 0;
int g_dbgDefRead = 0;

uint32_t g_dbgHandles[16] = { 0 };
uintptr_t g_dbgEnts[16] = { 0 };
int g_dbgHC = 0;

int g_dbgCountA = 0; uintptr_t g_dbgDataA = 0;
int g_dbgCountB = 0; uintptr_t g_dbgDataB = 0;
int g_dbgHealth = 0;

template<typename T>
T Read(uintptr_t a) {
    if (!a) return T{};
    __try { return *reinterpret_cast<T*>(a); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return T{}; }
}

template<typename T>
void Write(uintptr_t a, T v) {
    if (!a) return;
    __try { *reinterpret_cast<T*>(a) = v; }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

uintptr_t GetEntityByHandle(uint32_t handle) {
    if (!handle || handle == 0xFFFFFFFF) return 0;

    uintptr_t entityList = Read<uintptr_t>(g_client + offsets::dwEntityList);
    if (!entityList) return 0;

    uint32_t index = handle & 0x7FFF;
    uintptr_t chunk = Read<uintptr_t>(entityList + 0x10 + ((index >> 9) * 8));
    if (!chunk) return 0;

    uintptr_t entity = Read<uintptr_t>(chunk + ((index & 0x1FF) * 0x78));
    return entity;
}

uintptr_t GetLocalPawn() {
    g_client = (uintptr_t)GetModuleHandleA("client.dll");
    return g_client ? Read<uintptr_t>(g_client + offsets::dwLocalPlayerPawn) : 0;
}

uintptr_t GetLocalController() {
    g_client = (uintptr_t)GetModuleHandleA("client.dll");
    return g_client ? Read<uintptr_t>(g_client + offsets::dwLocalPlayerController) : 0;
}

void ApplySkin(uintptr_t weapon, int paintKit) {
    if (!weapon) return;

    uintptr_t item = weapon + schema::m_AttributeManager + schema::m_Item;

    Write<int>(item + schema::m_iItemIDHigh, -1);
    Write<int>(item + schema::m_iItemIDLow, -1);
    Write<int>(item + schema::m_iAccountID, 0);
    Write<bool>(item + schema::m_bInitialized, true);

    Write<int>(weapon + schema::m_nFallbackPaintKit, paintKit);
    Write<int>(weapon + schema::m_nFallbackSeed, 0);
    Write<float>(weapon + schema::m_flFallbackWear, 0.0001f);
    Write<int>(weapon + schema::m_nFallbackStatTrak, -1);

    g_dbgSkinsApplied++;
}

void RunChanger() {
    uintptr_t pawn = GetLocalPawn();
    if (!pawn) return;

    g_dbgHealth = Read<int>(pawn + 0x34C);
    if (g_dbgHealth <= 0) return;

    uintptr_t ws = Read<uintptr_t>(pawn + schema::m_pWeaponServices);
    g_dbgWs = ws;
    if (!ws) return;

    g_dbgWeaponsFound = 0;
    g_dbgLastDefCount = 0;
    g_dbgHC = 0;

    uintptr_t vecBase = ws + schema::m_hMyWeapons;

    g_dbgCountA = Read<int>(vecBase + 0x0);
    g_dbgDataA = Read<uintptr_t>(vecBase + 0x8);
    g_dbgCountB = Read<int>(vecBase + 0x8);
    g_dbgDataB = Read<uintptr_t>(vecBase + 0x0);

    int count = 0;
    uintptr_t data = 0;

    if (g_dbgCountA > 0 && g_dbgCountA <= 64 && g_dbgDataA > 0x10000) {
        count = g_dbgCountA;
        data = g_dbgDataA;
    }
    else if (g_dbgCountB > 0 && g_dbgCountB <= 64 && g_dbgDataB > 0x10000) {
        count = g_dbgCountB;
        data = g_dbgDataB;
    }
    else {
        return;
    }

    g_dbgCount = count;
    g_dbgArr = data;

    for (int i = 0; i < count; i++) {
        uint32_t handle = Read<uint32_t>(data + i * 4);
        if (handle == 0 || handle == 0xFFFFFFFF) continue;

        uintptr_t weapon = GetEntityByHandle(handle);

        if (g_dbgHC < 16) {
            g_dbgHandles[g_dbgHC] = handle;
            g_dbgEnts[g_dbgHC] = weapon;
            g_dbgHC++;
        }

        if (!weapon) continue;

        uintptr_t item = weapon + schema::m_AttributeManager + schema::m_Item;
        uint16_t def = Read<uint16_t>(item + 0x1BA);

        if (def == 0 || def > 1000) continue;

        if (g_dbgLastDefCount < 16)
            g_dbgLastDefs[g_dbgLastDefCount++] = def;

        g_dbgItemAddr = item;
        g_dbgItemBase = weapon + schema::m_AttributeManager;
        g_dbgDefRead = def;

        g_dbgWeaponsFound++;

        if (!g_enableChanger) continue;

        for (const auto& inv : g_invItems) {
            if (inv.itemDef == def) {
                ApplySkin(weapon, inv.paintKit);
                break;
            }
        }
    }
}

void AddFakeItem(int paintKit, int itemDef, const char* name) {
    g_invItems.push_back({ paintKit, itemDef, name });
}

void DrawMenu() {
    if (!g_showMenu) return;

    ImGui::SetNextWindowSize(ImVec2(580, 720), ImGuiCond_FirstUseEver);
    ImGui::Begin("Cleus Inventory Changer", &g_showMenu);

    uintptr_t pawn = GetLocalPawn();
    uintptr_t controller = GetLocalController();

    ImGui::Checkbox("Enable Changer", &g_enableChanger);

    ImGui::Text("client.dll: %llX", (unsigned long long)g_client);
    ImGui::Text("pawn: %llX", (unsigned long long)pawn);
    ImGui::Text("controller: %llX", (unsigned long long)controller);

    ImGui::Separator();
    ImGui::Text("=== DEBUG ===");
    ImGui::Text("pawn health: %d", g_dbgHealth);
    ImGui::Text("VariantA (count@0/data@8): %d / %llX", g_dbgCountA, (unsigned long long)g_dbgDataA);
    ImGui::Text("VariantB (count@8/data@0): %d / %llX", g_dbgCountB, (unsigned long long)g_dbgDataB);
    ImGui::Text("WeaponServices: %llX", (unsigned long long)g_dbgWs);
    ImGui::Text("ws vec count: %d", g_dbgCount);
    ImGui::Text("ws vec data: %llX", (unsigned long long)g_dbgArr);
    ImGui::Text("weapons found: %d", g_dbgWeaponsFound);
    ImGui::Text("skins applied: %d", g_dbgSkinsApplied);

    ImGui::Separator();
    ImGui::Text("Handle debug (handle / ent):");
    for (int i = 0; i < g_dbgHC; i++) {
        ImGui::Text("%08X  %llX", g_dbgHandles[i], (unsigned long long)g_dbgEnts[i]);
    }

    ImGui::Separator();
    ImGui::Text("Item addr: %llX", (unsigned long long)g_dbgItemAddr);
    ImGui::Text("Def read: %d", g_dbgDefRead);

    ImGui::Text("Weapon defs: ");
    for (int i = 0; i < g_dbgLastDefCount; i++) {
        ImGui::SameLine();
        ImGui::Text("%d", g_dbgLastDefs[i]);
    }

    ImGui::Separator();
    ImGui::Text("Items in list: %d", (int)g_invItems.size());
    ImGui::Text("Select skin to add:");

    if (ImGui::BeginListBox("##opts", ImVec2(-1, 130))) {
        for (int i = 0; i < g_optCount; i++) {
            if (ImGui::Selectable(g_opts[i].name)) {
                AddFakeItem(g_opts[i].paintKit, g_opts[i].itemDef, g_opts[i].name);
            }
        }
        ImGui::EndListBox();
    }

    ImGui::Separator();
    ImGui::Text("Current inventory:");

    if (ImGui::BeginListBox("##inv", ImVec2(-1, 120))) {
        for (size_t i = 0; i < g_invItems.size(); i++) {
            ImGui::Selectable(g_invItems[i].name);
        }
        ImGui::EndListBox();
    }

    ImGui::Text("INSERT - Toggle Menu");
    ImGui::End();
}

LRESULT __stdcall hkWndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_KEYDOWN && w == VK_INSERT)
        g_showMenu = !g_showMenu;

    if (g_showMenu && ImGui_ImplWin32_WndProcHandler(h, m, w, l))
        return true;

    return CallWindowProc(g_oWndProc, h, m, w, l);
}

HRESULT __stdcall hkPresent(IDXGISwapChain* sc, UINT si, UINT f) {
    if (!g_imguiInit) {
        if (SUCCEEDED(sc->GetDevice(__uuidof(ID3D11Device), (void**)&g_pDevice))) {
            g_pDevice->GetImmediateContext(&g_pContext);
            DXGI_SWAP_CHAIN_DESC sd; sc->GetDesc(&sd);
            g_hWindow = sd.OutputWindow;

            ID3D11Texture2D* bb = nullptr;
            sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&bb);
            if (bb) {
                g_pDevice->CreateRenderTargetView(bb, nullptr, &g_mainRTV);
                bb->Release();
            }

            ImGui::CreateContext();
            ImGui::StyleColorsDark();
            ImGui_ImplWin32_Init(g_hWindow);
            ImGui_ImplDX11_Init(g_pDevice, g_pContext);
            g_oWndProc = (WNDPROC)SetWindowLongPtr(g_hWindow, GWLP_WNDPROC, (LONG_PTR)hkWndProc);
            g_imguiInit = true;
        }
        else {
            return oPresent(sc, si, f);
        }
    }

    static DWORD lastRun = 0;
    DWORD now = GetTickCount();
    if (now - lastRun > 1000) {
        RunChanger();
        lastRun = now;
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    DrawMenu();

    ImGui::Render();
    g_pContext->OMSetRenderTargets(1, &g_mainRTV, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    return oPresent(sc, si, f);
}

DWORD WINAPI MainThread(LPVOID) {
    while (!GetModuleHandleA("client.dll"))
        Sleep(100);

    Sleep(3000);

    HMODULE d3d11 = GetModuleHandleA("d3d11.dll");
    if (!d3d11) return 0;

    typedef HRESULT(WINAPI* D3D11CreateDeviceFn)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL*, UINT, UINT, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);
    auto D3D11CreateDevice = (D3D11CreateDeviceFn)GetProcAddress(d3d11, "D3D11CreateDevice");

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    D3D_FEATURE_LEVEL fl;

    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &device, &fl, &context)))
        return 0;

    IDXGIDevice* dxgiDevice = nullptr;
    device->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);

    IDXGIAdapter* adapter = nullptr;
    dxgiDevice->GetAdapter(&adapter);

    IDXGIFactory* factory = nullptr;
    adapter->GetParent(__uuidof(IDXGIFactory), (void**)&factory);

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = GetForegroundWindow();
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    IDXGISwapChain* swapChain = nullptr;
    if (FAILED(factory->CreateSwapChain(device, &sd, &swapChain)))
        return 0;

    void** vmt = *(void***)swapChain;
    oPresent = (PresentFn)vmt[8];

    DWORD oldProtect;
    VirtualProtect(&vmt[8], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
    vmt[8] = hkPresent;
    VirtualProtect(&vmt[8], sizeof(void*), oldProtect, &oldProtect);

    swapChain->Release();
    factory->Release();
    adapter->Release();
    dxgiDevice->Release();
    device->Release();
    context->Release();

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hMod);
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
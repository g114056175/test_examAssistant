#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <shellapi.h>
#include <objidl.h>
#include <gdiplus.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include <stdarg.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "crypt32.lib")

#define APP_NAME "Helper"
#define CONFIG_FILE "config.ini"

#define HOTKEY_SEND_SELECTED  1
#define HOTKEY_SEND_PROMPT2   4
#define HOTKEY_SET_TL         2
#define HOTKEY_SET_BR         3
#define HOTKEY_TOGGLE_VISIBLE 5
#define HOTKEY_OPACITY_UP     6
#define HOTKEY_OPACITY_DOWN   7
#define HOTKEY_OPEN_SETTINGS  8
#define HOTKEY_EXIT_APP       9
#define HOTKEY_CANCEL_REQ     10
#define HOTKEY_SEND_PROMPT3   11

#define ID_EDIT_PROMPT 401
#define ID_BTN_SAVE 1
#define ID_BTN_ASK 3
#define ID_BTN_RESET 4
#define ID_BTN_TAB_BASIC 360
#define ID_BTN_TAB_ADV 361
#define ID_THEME_COMBO 304
#define ID_CHK_STREAM 305
#define ID_HKCAP_PREVIEW 710
#define ID_HKCAP_SAVE 711
#define ID_HKCAP_CANCEL 712

#define ID_LBL_ENDPOINT 900
#define ID_LBL_APIKEY 901
#define ID_LBL_MODEL 902
#define ID_LBL_SYSTEM 903
#define ID_LBL_TEMPLATE 904
#define ID_LBL_QUICK 905
#define ID_LBL_HOTKEYS 906
#define ID_LBL_SEND 908
#define ID_LBL_SETTL 909
#define ID_LBL_SETBR 910
#define ID_LBL_TOGGLEVIS 911
#define ID_LBL_OPP 912
#define ID_LBL_OPM 913
#define ID_LBL_OPENSET 914
#define ID_LBL_OPACITY 915
#define ID_LBL_THEME 916
#define ID_LBL_EXIT 917
#define ID_LBL_CANCEL 918
#define ID_LBL_SEND2 919
#define ID_LBL_SEND3 920
#define ID_LBL_PROMPT2 921
#define ID_LBL_PROMPT3 922

#define WM_APP_RESPONSE (WM_APP + 1)
#define WM_APP_TRAY (WM_APP + 2)
#define WM_APP_STREAM (WM_APP + 3)
#define TRAY_UID 9001

typedef struct AppConfig {
    char endpoint[512];
    char api_key[256];
    char model[128];
    char system_prompt[1024];
    char prompt_2[1024];
    char prompt_3[1024];
    char user_template[2048];
    int overlay_enabled;
    int overlay_visible;
    int opacity;
    int theme_light;
    int stream;
    char hk_send[64];
    char hk_send2[64];
    char hk_send3[64];
    char hk_tl[64];
    char hk_br[64];
    char hk_toggle_enable[64];
    char hk_toggle_visible[64];
    char hk_opacity_up[64];
    char hk_opacity_down[64];
    char hk_open_settings[64];
    char hk_exit[64];
    char hk_cancel[64];
} AppConfig;

typedef struct RequestPayload {
    char *user_text;
    char region[128];
    char image_path[MAX_PATH];
    POINT anchor;
    int from_ask;
    int req_id;
    char system_prompt[1024];
} RequestPayload;

typedef struct ResponsePayload {
    char *text;
    POINT anchor;
    int from_ask;
    int req_id;
} ResponsePayload;

typedef struct StreamPayload {
    char *text;
    POINT anchor;
    int req_id;
} StreamPayload;

typedef enum ProviderKind {
    PROVIDER_OPENAI_COMPAT = 0,
    PROVIDER_GOOGLE_GEMINI = 1
} ProviderKind;

typedef struct ProviderRequestInfo {
    ProviderKind kind;
    int use_stream;
    char endpoint[512];
} ProviderRequestInfo;

static AppConfig g_cfg;
static HWND g_hwnd_main = NULL;
static HWND g_hwnd_overlay = NULL;
static HWND g_hwnd_capture = NULL;
static HWND g_hwnd_settings = NULL;
static char g_overlay_text[4096];
static wchar_t g_overlay_text_w[4096];
static char g_config_path[MAX_PATH] = CONFIG_FILE;
static int g_have_tl = 0;
static int g_have_br = 0;
static POINT g_tl = {0};
static POINT g_br = {0};
static int g_capture_active = 0;
static POINT g_capture_anchor = {0};
static POINT g_capture_current = {0};
static ULONGLONG g_capture_deadline = 0;
static char g_last_capture_path[MAX_PATH] = "";
static int g_tray_added = 0;
static int g_show_advanced = 0;
static int g_ask_inflight = 0;
static HANDLE g_singleton_mutex = NULL;
static int g_req_inflight = 0;
static int g_stream_has_output = 0;
static POINT g_wait_anchor = {0};
static int g_wait_dots = 1;
static char g_wait_prefix[2048] = "";
static int g_loading_controls = 0;
static HWND g_hwnd_hotkey_capture = NULL;
static HWND g_hwnd_hotkey_capture_owner = NULL;
static int g_hotkey_capture_target_id = 0;
static char g_hotkey_capture_value[64] = "";
static volatile LONG g_request_seq = 0;
static int g_active_request_id = -1;
static HINTERNET g_active_hrequest = NULL;
static CRITICAL_SECTION g_req_cs;
static ULONG_PTR g_gdiplus_token = 0;

static void LoadConfig(AppConfig *cfg);
static void SaveConfig(const AppConfig *cfg);
static void RegisterHotkeys(HWND hwnd, const AppConfig *cfg);
static void UnregisterHotkeys(HWND hwnd);
static int ParseHotkey(const char *text, UINT *mod, UINT *vk);
static void EnsureOverlayWindow(HWND hwnd_parent);
static void ShowOverlayText(const char *text, POINT anchor);
static void HideOverlay(void);
static void EnsureCaptureWindow(HWND hwnd_parent);
static void StartCaptureSelection(POINT anchor);
static void CancelCaptureSelection(void);
static int ConfirmCaptureSelection(POINT cursor, char *path, int path_size);
static char *GetSelectedText(void);
static unsigned __stdcall RequestThread(void *param);
static char *SendLLMRequest(const char *user_text, const char *region, const char *image_path, const char *system_prompt, int req_id);
static char *ExtractDeltaContent(const char *json);
static void StripThinkBlocks(char *text);
static char *BuildUserMessage(const char *user_text, const char *region);
static void StartRequestEx(const char *text, const char *region, const char *image_path, POINT anchor, int from_ask, const char *system_prompt);
static void StartRequest(const char *text, const char *region, const char *image_path, POINT anchor, const char *system_prompt);
static void CancelCurrentRequest(const char *reason, POINT anchor);
static void InitConfigPath(void);
static ProviderKind DetectProviderKind(const char *endpoint, const char *api_key);
static void ResolveProviderRequestInfo(ProviderRequestInfo *info, const char *endpoint, const char *api_key, const char *model, int stream_enabled);
static char *ExtractProviderText(const ProviderRequestInfo *info, const char *json);
static void CreateSettingsWindow(HWND owner);
static void ApplyConfigToSettingsControls(HWND hwnd, const AppConfig *cfg);
static int IsHotkeyButtonId(int id);
static const char *HotkeyIdName(int id);
static int ValidateHotkeyControls(HWND hwnd, int changing_id, const char *new_value, char *err, int err_size);
static void OpenHotkeyCaptureDialog(HWND owner, int target_id);
static LRESULT CALLBACK HotkeyCaptureProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
static char *DupPrintf(const char *fmt, ...);
static void NormalizeEndpoint(char *s);
static void NormalizeFriendlyEndpointAlias(char *s, int s_size);
static void AddTrayIcon(HWND hwnd);
static void RemoveTrayIcon(HWND hwnd);
static void SetDlgItemTextUtf8(HWND hwnd, int id, const char *utf8);
static void GetDlgItemTextUtf8(HWND hwnd, int id, char *out, int out_size);
static void SetAdvancedVisible(HWND hwnd, int show);
static void BuildSettingsLayout(HWND hwnd);
static void RebuildPageControls(HWND hwnd);
static void CreateBasicPageControls(HWND hwnd);
static void CreateAdvancedPageControls(HWND hwnd);
static void ApplyRuntimeHotkeysFromControls(HWND hwnd);
static void ApplyRuntimeConfigFromControls(HWND hwnd);
static void SyncSettingsUiFromRuntime(void);
static void ShowCachedOverlayAt(POINT anchor);
static int StepOpacityTier(int current, int direction);
static void MoveCtrl(HWND hwnd, int id, int x, int y, int w, int h);
static void ReleaseModifierKeys(void);
static int HasVisibleText(const char *s);
static void ShowWaitingOverlay(POINT anchor);

// config helpers split into module file
#include "modules/config_module.inc.c"
// provider detection/alias/body parsing split into module file
#include "modules/provider_module.inc.c"

// hotkey parsing/format/registration split into module file
#include "modules/hotkey_module.inc.c"
// input/clipboard/waiting helpers split into module file
#include "modules/input_utils_module.inc.c"
// overlay drawing/window split into module file
#include "modules/overlay_module.inc.c"
// region capture overlay/saving split into module file
#include "modules/capture_module.inc.c"
// settings UI and dialogs split into module file
#include "modules/ui_settings_module.inc.c"
// LLM request/build/streaming split into module file
#include "modules/llm_request_module.inc.c"

static LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_CREATE:
        InitConfigPath();
        LoadConfig(&g_cfg);
        RegisterHotkeys(hwnd, &g_cfg);
        CreateSettingsWindow(hwnd);
        SetWindowTextW(g_hwnd_settings, L"Helper");
        SetWindowTextW(g_hwnd_main, L"Helper");
        return 0;
    case WM_HOTKEY: {
        POINT cursor;
        GetCursorPos(&cursor);
        switch (wparam) {
        case HOTKEY_SEND_SELECTED: {
            if (g_req_inflight) {
                ShowOverlayText("Request in progress. Please wait...", cursor);
                break;
            }
            char *text = GetSelectedText();
            if (HasVisibleText(text)) {
                g_wait_prefix[0] = 0;
                StartRequest(text, "", "", cursor, g_cfg.system_prompt);
            } else {
                g_wait_prefix[0] = 0;
                ShowOverlayText("No selected text captured. Please re-select text and try again.", cursor);
            }
            free(text);
            break;
        }
        case HOTKEY_SEND_PROMPT2: {
            if (g_req_inflight) {
                ShowOverlayText("Request in progress. Please wait...", cursor);
                break;
            }
            char *text = GetSelectedText();
            if (HasVisibleText(text)) {
                g_wait_prefix[0] = 0;
                StartRequest(text, "", "", cursor, g_cfg.prompt_2);
            } else {
                g_wait_prefix[0] = 0;
                ShowOverlayText("No selected text captured. Please re-select text and try again.", cursor);
            }
            free(text);
            break;
        }
        case HOTKEY_SEND_PROMPT3: {
            if (g_req_inflight) {
                ShowOverlayText("Request in progress. Please wait...", cursor);
                break;
            }
            char *text = GetSelectedText();
            if (HasVisibleText(text)) {
                g_wait_prefix[0] = 0;
                StartRequest(text, "", "", cursor, g_cfg.prompt_3);
            } else {
                g_wait_prefix[0] = 0;
                ShowOverlayText("No selected text captured. Please re-select text and try again.", cursor);
            }
            free(text);
            break;
        }
        case HOTKEY_SET_TL:
            StartCaptureSelection(cursor);
            break;
        case HOTKEY_SET_BR: {
            char saved_path[MAX_PATH];
            if (ConfirmCaptureSelection(cursor, saved_path, sizeof(saved_path))) {
                g_wait_prefix[0] = 0;
                StartRequest("", "", saved_path, cursor, g_cfg.system_prompt);
            }
            break;
        }
        case HOTKEY_TOGGLE_VISIBLE:
            g_cfg.overlay_visible = !g_cfg.overlay_visible;
            SaveConfig(&g_cfg);
            if (!g_cfg.overlay_visible) {
                HideOverlay();
            } else {
                g_wait_anchor = cursor;
                ShowCachedOverlayAt(cursor);
            }
            SyncSettingsUiFromRuntime();
            break;
        case HOTKEY_OPACITY_UP:
            g_cfg.opacity = StepOpacityTier(g_cfg.opacity, +1);
            SaveConfig(&g_cfg);
            if (g_hwnd_overlay) {
                SetLayeredWindowAttributes(g_hwnd_overlay, 0, (BYTE)g_cfg.opacity, LWA_ALPHA);
                InvalidateRect(g_hwnd_overlay, NULL, TRUE);
            }
            SyncSettingsUiFromRuntime();
            break;
        case HOTKEY_OPACITY_DOWN:
            g_cfg.opacity = StepOpacityTier(g_cfg.opacity, -1);
            SaveConfig(&g_cfg);
            if (g_hwnd_overlay) {
                SetLayeredWindowAttributes(g_hwnd_overlay, 0, (BYTE)g_cfg.opacity, LWA_ALPHA);
                InvalidateRect(g_hwnd_overlay, NULL, TRUE);
            }
            SyncSettingsUiFromRuntime();
            break;
        case HOTKEY_OPEN_SETTINGS:
            if (!g_hwnd_settings) break;
            if (IsWindowVisible(g_hwnd_settings)) {
                ShowWindow(g_hwnd_settings, SW_HIDE);
            } else {
                ShowWindow(g_hwnd_settings, SW_SHOW);
                ShowWindow(g_hwnd_settings, SW_RESTORE);
                SetForegroundWindow(g_hwnd_settings);
            }
            break;
        case HOTKEY_EXIT_APP:
            DestroyWindow(g_hwnd_main);
            break;
        case HOTKEY_CANCEL_REQ:
            CancelCurrentRequest("Request canceled.", cursor);
            break;
        }
        return 0;
    }
    case WM_TIMER:
        if (wparam == 1 && g_req_inflight && !g_stream_has_output) {
            ShowWaitingOverlay(g_wait_anchor);
            g_wait_dots++;
            if (g_wait_dots > 3) g_wait_dots = 1;
            return 0;
        }
        if (wparam == 2 && g_capture_active) {
            POINT cursor;
            GetCursorPos(&cursor);
            g_capture_current = cursor;
            if (GetTickCount64() >= g_capture_deadline) {
                CancelCaptureSelection();
            } else if (g_hwnd_capture) {
                InvalidateRect(g_hwnd_capture, NULL, TRUE);
            }
            return 0;
        }
        break;
    case WM_APP_STREAM: {
        StreamPayload *sp = (StreamPayload *)lparam;
        if (sp) {
            if (sp->req_id != g_active_request_id) {
                free(sp->text);
                free(sp);
                return 0;
            }
            if (sp->text && sp->text[0]) {
                g_stream_has_output = 1;
                ShowOverlayText(sp->text, g_wait_anchor);
            }
            free(sp->text);
            free(sp);
        }
        return 0;
    }
    case WM_APP_RESPONSE: {
        ResponsePayload *resp = (ResponsePayload *)lparam;
        if (!resp) return 0;
        if (resp->req_id != g_active_request_id) {
            free(resp->text);
            free(resp);
            return 0;
        }
        KillTimer(hwnd, 1);
        g_req_inflight = 0;
        g_wait_prefix[0] = 0;
        g_active_request_id = -1;
        if (resp && resp->text) {
            ShowOverlayText(resp->text, g_wait_anchor);
            if (resp->from_ask && g_hwnd_settings) {
                g_ask_inflight = 0;
                SetWindowTextA(GetDlgItem(g_hwnd_settings, ID_BTN_ASK), "Ask");
                EnableWindow(GetDlgItem(g_hwnd_settings, ID_BTN_ASK),
                             GetWindowTextLengthA(GetDlgItem(g_hwnd_settings, ID_EDIT_PROMPT)) > 0);
            }
            free(resp->text);
            free(resp);
        }
        return 0;
    }
    case WM_DESTROY:
        CancelCurrentRequest("", g_wait_anchor);
        CancelCaptureSelection();
        UnregisterHotkeys(hwnd);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR lpCmdLine, int nCmdShow) {
    Gdiplus::GdiplusStartupInput gdiplus_input;
    g_singleton_mutex = CreateMutexA(NULL, TRUE, "LLMOverlayHelperSingleton");
    if (g_singleton_mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxA(NULL, "Helper is already running.", "Helper", MB_OK | MB_ICONINFORMATION);
        CloseHandle(g_singleton_mutex);
        return 0;
    }
    if (Gdiplus::GdiplusStartup(&g_gdiplus_token, &gdiplus_input, NULL) != Gdiplus::Ok) {
        MessageBoxA(NULL, "Failed to initialize GDI+.", "Helper", MB_OK | MB_ICONERROR);
        if (g_singleton_mutex) {
            ReleaseMutex(g_singleton_mutex);
            CloseHandle(g_singleton_mutex);
            g_singleton_mutex = NULL;
        }
        return 0;
    }

    InitializeCriticalSection(&g_req_cs);

    WNDCLASSW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = MainProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"LLMOverlayMainW";
    RegisterClassW(&wc);
    g_hwnd_main = CreateWindowW(wc.lpszClassName, L"HelperCore", 0,
                                0, 0, 0, 0,
                                HWND_MESSAGE, NULL, hInstance, NULL);
    ShowWindow(g_hwnd_settings, SW_SHOW);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    if (g_singleton_mutex) {
        ReleaseMutex(g_singleton_mutex);
        CloseHandle(g_singleton_mutex);
        g_singleton_mutex = NULL;
    }
    if (g_gdiplus_token) {
        Gdiplus::GdiplusShutdown(g_gdiplus_token);
        g_gdiplus_token = 0;
    }
    DeleteCriticalSection(&g_req_cs);
    return 0;
}

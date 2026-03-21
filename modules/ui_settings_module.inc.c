static void SetDlgItemTextUtf8(HWND hwnd, int id, const char *utf8) {
    HWND ctrl = GetDlgItem(hwnd, id);
    if (!ctrl) return;
    wchar_t wbuf[4096];
    const char *src = utf8 ? utf8 : "";
    if (MultiByteToWideChar(CP_UTF8, 0, src, -1, wbuf, (int)(sizeof(wbuf) / sizeof(wchar_t))) == 0) {
        MultiByteToWideChar(CP_ACP, 0, src, -1, wbuf, (int)(sizeof(wbuf) / sizeof(wchar_t)));
    }
    SetWindowTextW(ctrl, wbuf);
}

static void GetDlgItemTextUtf8(HWND hwnd, int id, char *out, int out_size) {
    HWND ctrl = GetDlgItem(hwnd, id);
    if (!ctrl) {
        if (out_size > 0) out[0] = 0;
        return;
    }
    wchar_t wbuf[4096];
    GetWindowTextW(ctrl, wbuf, (int)(sizeof(wbuf) / sizeof(wchar_t)));
    if (WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, out, out_size, NULL, NULL) == 0) {
        if (out_size > 0) out[0] = 0;
    }
}

static int IsHotkeyButtonId(int id) {
    return id == 201 || id == 211 || id == 212 || id == 210 || id == 202 || id == 203 ||
           id == 206 || id == 207 || id == 205 || id == 208 || id == 209;
}

static const char *HotkeyIdName(int id) {
    switch (id) {
    case 201: return "Send Prompt-1";
    case 211: return "Send Prompt-2";
    case 212: return "Send Prompt-3";
    case 210: return "Cancel Request";
    case 202: return "Select Area";
    case 203: return "Ask Image";
    case 206: return "Opacity +";
    case 207: return "Opacity -";
    case 205: return "Toggle Visible";
    case 208: return "Toggle Settings";
    case 209: return "Exit App";
    default: return "Unknown";
    }
}

static int ValidateHotkeyControls(HWND hwnd, int changing_id, const char *new_value, char *err, int err_size) {
    const int ids[] = {201, 211, 212, 210, 202, 203, 206, 207, 205, 208, 209};
    UINT mods[11] = {0};
    UINT vks[11] = {0};
    char text[64];
    for (int i = 0; i < 11; ++i) {
        const int id = ids[i];
        if (id == changing_id && new_value) {
            strncpy(text, new_value, sizeof(text) - 1);
            text[sizeof(text) - 1] = 0;
        } else {
            GetWindowTextA(GetDlgItem(hwnd, id), text, sizeof(text));
        }
        if (!ParseHotkey(text, &mods[i], &vks[i])) {
            if (err && err_size > 0) snprintf(err, err_size, "Invalid hotkey format at '%s'.", HotkeyIdName(id));
            return 0;
        }
    }
    for (int i = 0; i < 11; ++i) {
        for (int j = i + 1; j < 11; ++j) {
            if (mods[i] == mods[j] && vks[i] == vks[j]) {
                if (err && err_size > 0) {
                    snprintf(err, err_size, "Hotkey conflict: '%s' and '%s' are the same.",
                             HotkeyIdName(ids[i]), HotkeyIdName(ids[j]));
                }
                return 0;
            }
        }
    }
    if (err && err_size > 0) err[0] = 0;
    return 1;
}

static void ApplyConfigToSettingsControls(HWND hwnd, const AppConfig *cfg) {
    char opbuf[16];
    g_loading_controls = 1;
    SetDlgItemTextUtf8(hwnd, 101, cfg->endpoint);
    SetDlgItemTextUtf8(hwnd, 102, cfg->api_key);
    SetDlgItemTextUtf8(hwnd, 103, cfg->model);
    SetDlgItemTextUtf8(hwnd, 104, cfg->system_prompt);
    SetDlgItemTextUtf8(hwnd, 106, cfg->prompt_2);
    SetDlgItemTextUtf8(hwnd, 107, cfg->prompt_3);
    SetDlgItemTextUtf8(hwnd, 105, cfg->user_template);
    SetDlgItemTextUtf8(hwnd, 201, cfg->hk_send);
    SetDlgItemTextUtf8(hwnd, 211, cfg->hk_send2);
    SetDlgItemTextUtf8(hwnd, 212, cfg->hk_send3);
    SetDlgItemTextUtf8(hwnd, 202, cfg->hk_tl);
    SetDlgItemTextUtf8(hwnd, 203, cfg->hk_br);
    SetDlgItemTextUtf8(hwnd, 205, cfg->hk_toggle_visible);
    SetDlgItemTextUtf8(hwnd, 206, cfg->hk_opacity_up);
    SetDlgItemTextUtf8(hwnd, 207, cfg->hk_opacity_down);
    SetDlgItemTextUtf8(hwnd, 208, cfg->hk_open_settings);
    SetDlgItemTextUtf8(hwnd, 209, cfg->hk_exit);
    SetDlgItemTextUtf8(hwnd, 210, cfg->hk_cancel);
    if (GetDlgItem(hwnd, 302)) CheckDlgButton(hwnd, 302, cfg->overlay_visible ? BST_CHECKED : BST_UNCHECKED);
    snprintf(opbuf, sizeof(opbuf), "%d", cfg->opacity);
    if (GetDlgItem(hwnd, 303)) SetWindowTextA(GetDlgItem(hwnd, 303), opbuf);
    if (GetDlgItem(hwnd, ID_THEME_COMBO)) SendMessageA(GetDlgItem(hwnd, ID_THEME_COMBO), CB_SETCURSEL, cfg->theme_light ? 1 : 0, 0);
    if (GetDlgItem(hwnd, ID_CHK_STREAM)) CheckDlgButton(hwnd, ID_CHK_STREAM, cfg->stream ? BST_CHECKED : BST_UNCHECKED);
    if (GetDlgItem(hwnd, ID_EDIT_PROMPT) && GetWindowTextLengthA(GetDlgItem(hwnd, ID_EDIT_PROMPT)) == 0) {
        SetWindowTextA(GetDlgItem(hwnd, ID_EDIT_PROMPT), "Test LLM prompt: please reply alive.");
    }
    g_loading_controls = 0;
}

static void ApplyRuntimeHotkeysFromControls(HWND hwnd) {
    if (GetDlgItem(hwnd, 201)) GetDlgItemTextUtf8(hwnd, 201, g_cfg.hk_send, sizeof(g_cfg.hk_send));
    if (GetDlgItem(hwnd, 211)) GetDlgItemTextUtf8(hwnd, 211, g_cfg.hk_send2, sizeof(g_cfg.hk_send2));
    if (GetDlgItem(hwnd, 212)) GetDlgItemTextUtf8(hwnd, 212, g_cfg.hk_send3, sizeof(g_cfg.hk_send3));
    if (GetDlgItem(hwnd, 202)) GetDlgItemTextUtf8(hwnd, 202, g_cfg.hk_tl, sizeof(g_cfg.hk_tl));
    if (GetDlgItem(hwnd, 203)) GetDlgItemTextUtf8(hwnd, 203, g_cfg.hk_br, sizeof(g_cfg.hk_br));
    if (GetDlgItem(hwnd, 205)) GetDlgItemTextUtf8(hwnd, 205, g_cfg.hk_toggle_visible, sizeof(g_cfg.hk_toggle_visible));
    if (GetDlgItem(hwnd, 206)) GetDlgItemTextUtf8(hwnd, 206, g_cfg.hk_opacity_up, sizeof(g_cfg.hk_opacity_up));
    if (GetDlgItem(hwnd, 207)) GetDlgItemTextUtf8(hwnd, 207, g_cfg.hk_opacity_down, sizeof(g_cfg.hk_opacity_down));
    if (GetDlgItem(hwnd, 208)) GetDlgItemTextUtf8(hwnd, 208, g_cfg.hk_open_settings, sizeof(g_cfg.hk_open_settings));
    if (GetDlgItem(hwnd, 209)) GetDlgItemTextUtf8(hwnd, 209, g_cfg.hk_exit, sizeof(g_cfg.hk_exit));
    if (GetDlgItem(hwnd, 210)) GetDlgItemTextUtf8(hwnd, 210, g_cfg.hk_cancel, sizeof(g_cfg.hk_cancel));
    RegisterHotkeys(g_hwnd_main, &g_cfg);
}

static void ApplyRuntimeConfigFromControls(HWND hwnd) {
    if (g_loading_controls) return;
    char obuf[32];
    if (GetDlgItem(hwnd, 101)) GetDlgItemTextUtf8(hwnd, 101, g_cfg.endpoint, sizeof(g_cfg.endpoint));
    if (GetDlgItem(hwnd, 102)) GetDlgItemTextUtf8(hwnd, 102, g_cfg.api_key, sizeof(g_cfg.api_key));
    if (GetDlgItem(hwnd, 103)) GetDlgItemTextUtf8(hwnd, 103, g_cfg.model, sizeof(g_cfg.model));
    if (GetDlgItem(hwnd, 104)) GetDlgItemTextUtf8(hwnd, 104, g_cfg.system_prompt, sizeof(g_cfg.system_prompt));
    if (GetDlgItem(hwnd, 106)) GetDlgItemTextUtf8(hwnd, 106, g_cfg.prompt_2, sizeof(g_cfg.prompt_2));
    if (GetDlgItem(hwnd, 107)) GetDlgItemTextUtf8(hwnd, 107, g_cfg.prompt_3, sizeof(g_cfg.prompt_3));
    if (GetDlgItem(hwnd, 105)) GetDlgItemTextUtf8(hwnd, 105, g_cfg.user_template, sizeof(g_cfg.user_template));
    if (GetDlgItem(hwnd, 302)) g_cfg.overlay_visible = (IsDlgButtonChecked(hwnd, 302) == BST_CHECKED);
    if (GetDlgItem(hwnd, ID_CHK_STREAM)) g_cfg.stream = (IsDlgButtonChecked(hwnd, ID_CHK_STREAM) == BST_CHECKED);
    if (GetDlgItem(hwnd, 303)) {
        GetWindowTextA(GetDlgItem(hwnd, 303), obuf, sizeof(obuf));
        g_cfg.opacity = ClampInt(atoi(obuf), 30, 255);
        if (g_hwnd_overlay) SetLayeredWindowAttributes(g_hwnd_overlay, 0, (BYTE)g_cfg.opacity, LWA_ALPHA);
    }
    if (GetDlgItem(hwnd, ID_THEME_COMBO)) {
        g_cfg.theme_light = (int)SendMessageA(GetDlgItem(hwnd, ID_THEME_COMBO), CB_GETCURSEL, 0, 0) == 1;
    }
}

static void SyncSettingsUiFromRuntime(void) {
    if (!g_hwnd_settings || !IsWindow(g_hwnd_settings)) return;
    if (GetDlgItem(g_hwnd_settings, 302)) CheckDlgButton(g_hwnd_settings, 302, g_cfg.overlay_visible ? BST_CHECKED : BST_UNCHECKED);
    if (GetDlgItem(g_hwnd_settings, 303)) {
        char obuf[16];
        snprintf(obuf, sizeof(obuf), "%d", g_cfg.opacity);
        SetWindowTextA(GetDlgItem(g_hwnd_settings, 303), obuf);
    }
}

static void SetAdvancedVisible(HWND hwnd, int show) { g_show_advanced = show ? 1 : 0; RebuildPageControls(hwnd); }

static void OpenHotkeyCaptureDialog(HWND owner, int target_id) {
    if (g_hwnd_hotkey_capture) { SetForegroundWindow(g_hwnd_hotkey_capture); return; }
    g_hotkey_capture_target_id = target_id;
    g_hotkey_capture_value[0] = 0;
    g_hwnd_hotkey_capture_owner = owner;
    WNDCLASSA wc; ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = HotkeyCaptureProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "HotkeyCaptureWindow";
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassA(&wc);
    g_hwnd_hotkey_capture = CreateWindowA(wc.lpszClassName, "Set Hotkey", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                                          CW_USEDEFAULT, CW_USEDEFAULT, 360, 180, owner, NULL, wc.hInstance, NULL);
    UnregisterHotkeys(g_hwnd_main);
    ShowWindow(g_hwnd_hotkey_capture, SW_SHOW);
    SetForegroundWindow(g_hwnd_hotkey_capture);
    SetFocus(g_hwnd_hotkey_capture);
    EnableWindow(owner, FALSE);
}

static LRESULT CALLBACK HotkeyCaptureProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_CREATE:
        CreateWindowA("STATIC", "Press a hotkey combo (e.g. Ctrl+Alt+Q):", WS_CHILD | WS_VISIBLE, 16, 16, 320, 18, hwnd, NULL, GetModuleHandle(NULL), NULL);
        CreateWindowA("STATIC", "(waiting...)", WS_CHILD | WS_VISIBLE | WS_BORDER, 16, 42, 320, 22, hwnd, (HMENU)ID_HKCAP_PREVIEW, GetModuleHandle(NULL), NULL);
        CreateWindowA("BUTTON", "Save", WS_CHILD | WS_VISIBLE | WS_DISABLED, 168, 86, 80, 28, hwnd, (HMENU)ID_HKCAP_SAVE, GetModuleHandle(NULL), NULL);
        CreateWindowA("BUTTON", "Cancel", WS_CHILD | WS_VISIBLE, 256, 86, 80, 28, hwnd, (HMENU)ID_HKCAP_CANCEL, GetModuleHandle(NULL), NULL);
        return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        UINT vk = (UINT)wparam;
        if (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL || vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU ||
            vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_LWIN || vk == VK_RWIN) return 0;
        UINT mod = 0;
        if (GetKeyState(VK_CONTROL) & 0x8000) mod |= MOD_CONTROL;
        if (GetKeyState(VK_MENU) & 0x8000) mod |= MOD_ALT;
        if (GetKeyState(VK_SHIFT) & 0x8000) mod |= MOD_SHIFT;
        if ((GetKeyState(VK_LWIN) & 0x8000) || (GetKeyState(VK_RWIN) & 0x8000)) mod |= MOD_WIN;
        BuildHotkeyString(mod, vk, g_hotkey_capture_value, sizeof(g_hotkey_capture_value));
        SetWindowTextA(GetDlgItem(hwnd, ID_HKCAP_PREVIEW), g_hotkey_capture_value);
        EnableWindow(GetDlgItem(hwnd, ID_HKCAP_SAVE), TRUE);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wparam) == ID_HKCAP_CANCEL) { DestroyWindow(hwnd); return 0; }
        if (LOWORD(wparam) == ID_HKCAP_SAVE) {
            char err[256];
            if (g_hwnd_hotkey_capture_owner && g_hotkey_capture_target_id && g_hotkey_capture_value[0]) {
                if (!ValidateHotkeyControls(g_hwnd_hotkey_capture_owner, g_hotkey_capture_target_id, g_hotkey_capture_value, err, sizeof(err))) {
                    MessageBoxA(hwnd, err, "Hotkey Conflict", MB_OK | MB_ICONWARNING);
                    return 0;
                }
                SetWindowTextA(GetDlgItem(g_hwnd_hotkey_capture_owner, g_hotkey_capture_target_id), g_hotkey_capture_value);
            }
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        g_hwnd_hotkey_capture = NULL;
        g_hotkey_capture_target_id = 0;
        g_hotkey_capture_value[0] = 0;
        if (g_hwnd_hotkey_capture_owner && IsWindow(g_hwnd_hotkey_capture_owner)) {
            EnableWindow(g_hwnd_hotkey_capture_owner, TRUE);
            SetForegroundWindow(g_hwnd_hotkey_capture_owner);
            if (GetDlgItem(g_hwnd_hotkey_capture_owner, 201)) ApplyRuntimeHotkeysFromControls(g_hwnd_hotkey_capture_owner);
            else RegisterHotkeys(g_hwnd_main, &g_cfg);
        } else RegisterHotkeys(g_hwnd_main, &g_cfg);
        g_hwnd_hotkey_capture_owner = NULL;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

static int IsPersistentControlId(int id) { return id == ID_BTN_TAB_BASIC || id == ID_BTN_TAB_ADV || id == ID_BTN_RESET || id == ID_BTN_SAVE; }

static void CreateBasicPageControls(HWND hwnd) {
    CreateWindowA("STATIC", "Endpoint:", WS_CHILD | WS_VISIBLE, 20, 40, 100, 20, hwnd, (HMENU)ID_LBL_ENDPOINT, GetModuleHandle(NULL), NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 130, 40, 470, 20, hwnd, (HMENU)101, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "API Key:", WS_CHILD | WS_VISIBLE, 20, 70, 100, 20, hwnd, (HMENU)ID_LBL_APIKEY, GetModuleHandle(NULL), NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 130, 70, 470, 20, hwnd, (HMENU)102, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Model:", WS_CHILD | WS_VISIBLE, 20, 100, 100, 20, hwnd, (HMENU)ID_LBL_MODEL, GetModuleHandle(NULL), NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 130, 100, 470, 20, hwnd, (HMENU)103, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Quick Prompt:", WS_CHILD | WS_VISIBLE, 20, 130, 120, 20, hwnd, (HMENU)ID_LBL_QUICK, GetModuleHandle(NULL), NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 145, 130, 375, 24, hwnd, (HMENU)ID_EDIT_PROMPT, GetModuleHandle(NULL), NULL);
    SendMessageA(GetDlgItem(hwnd, ID_EDIT_PROMPT), EM_LIMITTEXT, 200, 0);
    CreateWindowA("BUTTON", "Ask", WS_CHILD | WS_VISIBLE | WS_DISABLED, 530, 130, 70, 24, hwnd, (HMENU)ID_BTN_ASK, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Prompt-1:", WS_CHILD | WS_VISIBLE, 20, 162, 120, 20, hwnd, (HMENU)ID_LBL_SYSTEM, GetModuleHandle(NULL), NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 145, 160, 455, 24, hwnd, (HMENU)104, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Prompt-2:", WS_CHILD | WS_VISIBLE, 20, 192, 120, 20, hwnd, (HMENU)ID_LBL_PROMPT2, GetModuleHandle(NULL), NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 145, 190, 455, 24, hwnd, (HMENU)106, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Prompt-3:", WS_CHILD | WS_VISIBLE, 20, 222, 120, 20, hwnd, (HMENU)ID_LBL_PROMPT3, GetModuleHandle(NULL), NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 145, 220, 455, 24, hwnd, (HMENU)107, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Hotkeys:", WS_CHILD | WS_VISIBLE, 20, 255, 100, 20, hwnd, (HMENU)ID_LBL_HOTKEYS, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Send Prompt-1:", WS_CHILD | WS_VISIBLE, 20, 280, 110, 20, hwnd, (HMENU)ID_LBL_SEND, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "", WS_CHILD | WS_VISIBLE, 130, 278, 150, 24, hwnd, (HMENU)201, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Send Prompt-2:", WS_CHILD | WS_VISIBLE, 300, 280, 110, 20, hwnd, (HMENU)ID_LBL_SEND2, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "", WS_CHILD | WS_VISIBLE, 420, 278, 150, 24, hwnd, (HMENU)211, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Send Prompt-3:", WS_CHILD | WS_VISIBLE, 20, 310, 110, 20, hwnd, (HMENU)ID_LBL_SEND3, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "", WS_CHILD | WS_VISIBLE, 130, 308, 150, 24, hwnd, (HMENU)212, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Cancel Req:", WS_CHILD | WS_VISIBLE, 300, 310, 110, 20, hwnd, (HMENU)ID_LBL_CANCEL, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "", WS_CHILD | WS_VISIBLE, 420, 308, 150, 24, hwnd, (HMENU)210, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Select Area:", WS_CHILD | WS_VISIBLE, 20, 340, 100, 20, hwnd, (HMENU)ID_LBL_SETTL, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "", WS_CHILD | WS_VISIBLE, 130, 338, 150, 24, hwnd, (HMENU)202, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Ask Image:", WS_CHILD | WS_VISIBLE, 300, 340, 100, 20, hwnd, (HMENU)ID_LBL_SETBR, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "", WS_CHILD | WS_VISIBLE, 420, 338, 150, 24, hwnd, (HMENU)203, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Opacity +:", WS_CHILD | WS_VISIBLE, 20, 370, 100, 20, hwnd, (HMENU)ID_LBL_OPP, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "", WS_CHILD | WS_VISIBLE, 130, 368, 150, 24, hwnd, (HMENU)206, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Opacity -:", WS_CHILD | WS_VISIBLE, 300, 370, 100, 20, hwnd, (HMENU)ID_LBL_OPM, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "", WS_CHILD | WS_VISIBLE, 420, 368, 150, 24, hwnd, (HMENU)207, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Toggle Visible:", WS_CHILD | WS_VISIBLE, 20, 400, 110, 20, hwnd, (HMENU)ID_LBL_TOGGLEVIS, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "", WS_CHILD | WS_VISIBLE, 130, 398, 150, 24, hwnd, (HMENU)205, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Toggle Settings:", WS_CHILD | WS_VISIBLE, 300, 400, 110, 20, hwnd, (HMENU)ID_LBL_OPENSET, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "", WS_CHILD | WS_VISIBLE, 420, 398, 150, 24, hwnd, (HMENU)208, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Exit App:", WS_CHILD | WS_VISIBLE, 20, 430, 100, 20, hwnd, (HMENU)ID_LBL_EXIT, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "", WS_CHILD | WS_VISIBLE, 130, 428, 150, 24, hwnd, (HMENU)209, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "Overlay Visible", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 462, 150, 20, hwnd, (HMENU)302, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "Stream", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 180, 462, 80, 20, hwnd, (HMENU)ID_CHK_STREAM, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Opacity:", WS_CHILD | WS_VISIBLE, 270, 462, 50, 20, hwnd, (HMENU)ID_LBL_OPACITY, GetModuleHandle(NULL), NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER, 325, 462, 50, 20, hwnd, (HMENU)303, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Theme:", WS_CHILD | WS_VISIBLE, 390, 462, 50, 20, hwnd, (HMENU)ID_LBL_THEME, GetModuleHandle(NULL), NULL);
    CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_BORDER, 445, 462, 155, 180, hwnd, (HMENU)ID_THEME_COMBO, GetModuleHandle(NULL), NULL);
    SendMessageA(GetDlgItem(hwnd, ID_THEME_COMBO), CB_ADDSTRING, 0, (LPARAM)"Dark (Black/White)");
    SendMessageA(GetDlgItem(hwnd, ID_THEME_COMBO), CB_ADDSTRING, 0, (LPARAM)"Light (White/Black)");
}

static void CreateAdvancedPageControls(HWND hwnd) {
    CreateWindowA("STATIC", "User Template:", WS_CHILD | WS_VISIBLE, 20, 55, 100, 20, hwnd, (HMENU)ID_LBL_TEMPLATE, GetModuleHandle(NULL), NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOVSCROLL | ES_MULTILINE | WS_VSCROLL, 130, 55, 470, 405, hwnd, (HMENU)105, GetModuleHandle(NULL), NULL);
}

static void RebuildPageControls(HWND hwnd) {
    HWND to_delete[512];
    int del_count = 0;
    HWND c = GetWindow(hwnd, GW_CHILD);
    while (c && del_count < (int)(sizeof(to_delete) / sizeof(to_delete[0]))) {
        int id = GetDlgCtrlID(c);
        if (!IsPersistentControlId(id)) to_delete[del_count++] = c;
        c = GetWindow(c, GW_HWNDNEXT);
    }
    for (int i = 0; i < del_count; ++i) if (IsWindow(to_delete[i])) DestroyWindow(to_delete[i]);
    SetWindowTextA(GetDlgItem(hwnd, ID_BTN_TAB_BASIC), g_show_advanced ? "Basic" : "[Basic]");
    SetWindowTextA(GetDlgItem(hwnd, ID_BTN_TAB_ADV), g_show_advanced ? "[Advanced]" : "Advanced");
    if (g_show_advanced) CreateAdvancedPageControls(hwnd); else CreateBasicPageControls(hwnd);
    ApplyConfigToSettingsControls(hwnd, &g_cfg);
    if (!g_show_advanced) EnableWindow(GetDlgItem(hwnd, ID_BTN_ASK), !g_req_inflight && GetWindowTextLengthA(GetDlgItem(hwnd, ID_EDIT_PROMPT)) > 0);
    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

static int IsAlwaysVisibleId(int id) { return id == ID_BTN_TAB_BASIC || id == ID_BTN_TAB_ADV || id == ID_BTN_RESET || id == ID_BTN_SAVE; }
static int IsAdvancedOnlyId(int id) { return id == ID_LBL_TEMPLATE || id == 105; }

static void BuildSettingsLayout(HWND hwnd) {
    HWND c;
    while ((c = GetWindow(hwnd, GW_CHILD)) != NULL) DestroyWindow(c);
    CreateWindowA("BUTTON", g_show_advanced ? "Basic" : "[Basic]", WS_CHILD | WS_VISIBLE, 20, 10, 90, 22, hwnd, (HMENU)ID_BTN_TAB_BASIC, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", g_show_advanced ? "[Advanced]" : "Advanced", WS_CHILD | WS_VISIBLE, 115, 10, 90, 22, hwnd, (HMENU)ID_BTN_TAB_ADV, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "Reset", WS_CHILD | WS_VISIBLE, 400, 560, 95, 28, hwnd, (HMENU)ID_BTN_RESET, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "Save", WS_CHILD | WS_VISIBLE, 505, 560, 95, 28, hwnd, (HMENU)ID_BTN_SAVE, GetModuleHandle(NULL), NULL);
    RebuildPageControls(hwnd);
}

static LRESULT CALLBACK SettingsProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_SIZE:
        if (wparam == SIZE_MINIMIZED) { HideOverlay(); ShowWindow(hwnd, SW_HIDE); return 0; }
        break;
    case WM_SHOWWINDOW:
        if (wparam) SetWindowTextW(hwnd, L"Helper");
        break;
    case WM_CLOSE:
        DestroyWindow(g_hwnd_main);
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(wparam);
        if (IsHotkeyButtonId(id) && HIWORD(wparam) == BN_CLICKED) { OpenHotkeyCaptureDialog(hwnd, id); return 0; }
        if (id == ID_THEME_COMBO && HIWORD(wparam) == CBN_SELCHANGE) { g_cfg.theme_light = (int)SendMessageA(GetDlgItem(hwnd, ID_THEME_COMBO), CB_GETCURSEL, 0, 0) == 1; if (g_hwnd_overlay) InvalidateRect(g_hwnd_overlay, NULL, TRUE); return 0; }
        if (id == ID_CHK_STREAM && HIWORD(wparam) == BN_CLICKED) { g_cfg.stream = (IsDlgButtonChecked(hwnd, ID_CHK_STREAM) == BST_CHECKED); return 0; }
        if (id == 302 && HIWORD(wparam) == BN_CLICKED) { g_cfg.overlay_visible = (IsDlgButtonChecked(hwnd, 302) == BST_CHECKED); if (!g_cfg.overlay_visible) HideOverlay(); else ShowCachedOverlayAt(g_wait_anchor); return 0; }
        if (id == 303 && HIWORD(wparam) == EN_CHANGE) {
            if (g_loading_controls) return 0;
            char obuf[32]; GetWindowTextA(GetDlgItem(hwnd, 303), obuf, sizeof(obuf));
            g_cfg.opacity = ClampInt(atoi(obuf), 30, 255);
            if (g_hwnd_overlay) { SetLayeredWindowAttributes(g_hwnd_overlay, 0, (BYTE)g_cfg.opacity, LWA_ALPHA); InvalidateRect(g_hwnd_overlay, NULL, TRUE); }
            return 0;
        }
        if (id == 101 && HIWORD(wparam) == EN_KILLFOCUS) {
            char endpoint[512];
            GetDlgItemTextUtf8(hwnd, 101, endpoint, sizeof(endpoint));
            NormalizeFriendlyEndpointAlias(endpoint, sizeof(endpoint));
            SetDlgItemTextUtf8(hwnd, 101, endpoint);
            strncpy(g_cfg.endpoint, endpoint, sizeof(g_cfg.endpoint) - 1);
            g_cfg.endpoint[sizeof(g_cfg.endpoint) - 1] = 0;
            return 0;
        }
        if ((id == 101 || id == 102 || id == 103 || id == 104 || id == 105 || id == 106 || id == 107) && HIWORD(wparam) == EN_CHANGE) { ApplyRuntimeConfigFromControls(hwnd); return 0; }
        if (id == ID_EDIT_PROMPT && HIWORD(wparam) == EN_CHANGE) { EnableWindow(GetDlgItem(hwnd, ID_BTN_ASK), !g_req_inflight && GetWindowTextLengthA(GetDlgItem(hwnd, ID_EDIT_PROMPT)) > 0); return 0; }
        if (id == ID_BTN_TAB_BASIC) { SetAdvancedVisible(hwnd, 0); return 0; }
        if (id == ID_BTN_TAB_ADV) { SetAdvancedVisible(hwnd, 1); return 0; }
        if (id == ID_BTN_SAVE) {
            char buf[2048], hk_err[256];
            if (GetDlgItem(hwnd, 201) && !ValidateHotkeyControls(hwnd, 0, NULL, hk_err, sizeof(hk_err))) { MessageBoxA(hwnd, hk_err, "Hotkey Validation", MB_OK | MB_ICONWARNING); return 0; }
            if (MessageBoxA(hwnd, "Save settings to config.ini now?", "Confirm Save", MB_YESNO | MB_ICONQUESTION) != IDYES) return 0;
            if (GetDlgItem(hwnd, 101)) GetDlgItemTextUtf8(hwnd, 101, g_cfg.endpoint, sizeof(g_cfg.endpoint));
            if (GetDlgItem(hwnd, 102)) GetDlgItemTextUtf8(hwnd, 102, g_cfg.api_key, sizeof(g_cfg.api_key));
            if (GetDlgItem(hwnd, 103)) GetDlgItemTextUtf8(hwnd, 103, g_cfg.model, sizeof(g_cfg.model));
            if (GetDlgItem(hwnd, 104)) GetDlgItemTextUtf8(hwnd, 104, g_cfg.system_prompt, sizeof(g_cfg.system_prompt));
            if (GetDlgItem(hwnd, 106)) GetDlgItemTextUtf8(hwnd, 106, g_cfg.prompt_2, sizeof(g_cfg.prompt_2));
            if (GetDlgItem(hwnd, 107)) GetDlgItemTextUtf8(hwnd, 107, g_cfg.prompt_3, sizeof(g_cfg.prompt_3));
            if (GetDlgItem(hwnd, 105)) GetDlgItemTextUtf8(hwnd, 105, g_cfg.user_template, sizeof(g_cfg.user_template));
            if (GetDlgItem(hwnd, 201)) GetDlgItemTextUtf8(hwnd, 201, g_cfg.hk_send, sizeof(g_cfg.hk_send));
            if (GetDlgItem(hwnd, 211)) GetDlgItemTextUtf8(hwnd, 211, g_cfg.hk_send2, sizeof(g_cfg.hk_send2));
            if (GetDlgItem(hwnd, 212)) GetDlgItemTextUtf8(hwnd, 212, g_cfg.hk_send3, sizeof(g_cfg.hk_send3));
            if (GetDlgItem(hwnd, 202)) GetDlgItemTextUtf8(hwnd, 202, g_cfg.hk_tl, sizeof(g_cfg.hk_tl));
            if (GetDlgItem(hwnd, 203)) GetDlgItemTextUtf8(hwnd, 203, g_cfg.hk_br, sizeof(g_cfg.hk_br));
            if (GetDlgItem(hwnd, 205)) GetDlgItemTextUtf8(hwnd, 205, g_cfg.hk_toggle_visible, sizeof(g_cfg.hk_toggle_visible));
            if (GetDlgItem(hwnd, 206)) GetDlgItemTextUtf8(hwnd, 206, g_cfg.hk_opacity_up, sizeof(g_cfg.hk_opacity_up));
            if (GetDlgItem(hwnd, 207)) GetDlgItemTextUtf8(hwnd, 207, g_cfg.hk_opacity_down, sizeof(g_cfg.hk_opacity_down));
            if (GetDlgItem(hwnd, 208)) GetDlgItemTextUtf8(hwnd, 208, g_cfg.hk_open_settings, sizeof(g_cfg.hk_open_settings));
            if (GetDlgItem(hwnd, 209)) GetDlgItemTextUtf8(hwnd, 209, g_cfg.hk_exit, sizeof(g_cfg.hk_exit));
            if (GetDlgItem(hwnd, 210)) GetDlgItemTextUtf8(hwnd, 210, g_cfg.hk_cancel, sizeof(g_cfg.hk_cancel));
            if (GetDlgItem(hwnd, 302)) g_cfg.overlay_visible = (IsDlgButtonChecked(hwnd, 302) == BST_CHECKED);
            if (GetDlgItem(hwnd, 303)) { GetWindowTextA(GetDlgItem(hwnd, 303), buf, sizeof(buf)); g_cfg.opacity = ClampInt(atoi(buf), 30, 255); }
            if (GetDlgItem(hwnd, ID_THEME_COMBO)) g_cfg.theme_light = (int)SendMessageA(GetDlgItem(hwnd, ID_THEME_COMBO), CB_GETCURSEL, 0, 0) == 1;
            if (GetDlgItem(hwnd, ID_CHK_STREAM)) g_cfg.stream = (IsDlgButtonChecked(hwnd, ID_CHK_STREAM) == BST_CHECKED);
            SaveConfig(&g_cfg); RegisterHotkeys(g_hwnd_main, &g_cfg);
            if (g_hwnd_overlay) SetLayeredWindowAttributes(g_hwnd_overlay, 0, (BYTE)g_cfg.opacity, LWA_ALPHA);
            if (!g_cfg.overlay_visible) HideOverlay();
            if (g_hwnd_overlay) InvalidateRect(g_hwnd_overlay, NULL, TRUE);
            MessageBoxA(hwnd, "Saved to config.ini", "Saved", MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        if (id == ID_BTN_ASK) {
            char prompt[2048]; POINT cursor;
            if (g_req_inflight) return 0;
            GetDlgItemTextUtf8(hwnd, ID_EDIT_PROMPT, prompt, sizeof(prompt));
            GetCursorPos(&cursor);
            if (prompt[0]) { g_ask_inflight = 1; g_wait_prefix[0] = 0; EnableWindow(GetDlgItem(hwnd, ID_BTN_ASK), 0); SetWindowTextA(GetDlgItem(hwnd, ID_BTN_ASK), "Asking..."); StartRequestEx(prompt, "__RAW__", "", cursor, 1, g_cfg.system_prompt); }
            return 0;
        }
        if (id == ID_BTN_RESET) {
            if (MessageBoxA(hwnd, "Reset current settings to built-in defaults? This will not save to config.ini until you press Save.", "Confirm Reset", MB_YESNO | MB_ICONWARNING) == IDYES) {
                ConfigDefaults(&g_cfg);
                ApplyConfigToSettingsControls(hwnd, &g_cfg);
                RegisterHotkeys(g_hwnd_main, &g_cfg);
                if (g_hwnd_overlay) SetLayeredWindowAttributes(g_hwnd_overlay, 0, (BYTE)g_cfg.opacity, LWA_ALPHA);
                if (!g_cfg.overlay_visible) HideOverlay();
                if (g_hwnd_overlay) InvalidateRect(g_hwnd_overlay, NULL, TRUE);
                SetAdvancedVisible(hwnd, 0);
            }
            return 0;
        }
        break;
    }}
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

static void CreateSettingsWindow(HWND owner) {
    if (g_hwnd_settings) return;
    WNDCLASSW wc; ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = SettingsProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"LLMSettingsWindowW";
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassW(&wc);
    g_hwnd_settings = CreateWindowW(wc.lpszClassName, L"Helper",
                                    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                    100, 100, 640, 630, NULL, NULL, wc.hInstance, NULL);
    BuildSettingsLayout(g_hwnd_settings);
}

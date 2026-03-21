static void ConfigDefaults(AppConfig *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    strcpy(cfg->endpoint, "http://192.168.1.10:8081/v1/chat/completions");
    strcpy(cfg->api_key, "");
    strcpy(cfg->model, "qwen3-coder");
    strcpy(cfg->system_prompt, "Reply in Traditional Chinese. Give the direct answer first. If there are multiple questions, list each answer in order. Keep the response short and do not add explanation unless the question explicitly asks for it.");
    strcpy(cfg->prompt_2, "Reply in Traditional Chinese. Answer the question first, then give a short and simple explanation of the main idea. Keep it brief and easy to understand.");
    strcpy(cfg->prompt_3, "Reply in Traditional Chinese. Answer the question first, then provide a detailed and well-structured explanation. If useful, include the reasoning, steps, and a short summary of the key points.");
    strcpy(cfg->user_template, "{{text}}");
    cfg->overlay_enabled = 1;
    cfg->overlay_visible = 1;
    cfg->opacity = 180;
    cfg->theme_light = 1;
    cfg->stream = 1;
    strcpy(cfg->hk_send, "Ctrl+Q");
    strcpy(cfg->hk_send2, "Ctrl+W");
    strcpy(cfg->hk_send3, "Ctrl+E");
    strcpy(cfg->hk_tl, "Ctrl+Alt+1");
    strcpy(cfg->hk_br, "Ctrl+Alt+2");
    strcpy(cfg->hk_toggle_enable, "Ctrl+Alt+E");
    strcpy(cfg->hk_toggle_visible, "Alt+V");
    strcpy(cfg->hk_opacity_up, "Ctrl+Alt+Up");
    strcpy(cfg->hk_opacity_down, "Ctrl+Alt+Down");
    strcpy(cfg->hk_open_settings, "Ctrl+Alt+S");
    strcpy(cfg->hk_exit, "Ctrl+Alt+X");
    strcpy(cfg->hk_cancel, "Ctrl+R");
}

static void InitConfigPath(void) {
    DWORD len = GetModuleFileNameA(NULL, g_config_path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        strcpy(g_config_path, CONFIG_FILE);
        return;
    }
    for (int i = (int)len - 1; i >= 0; --i) {
        if (g_config_path[i] == '\\' || g_config_path[i] == '/') {
            g_config_path[i + 1] = 0;
            strncat(g_config_path, CONFIG_FILE, MAX_PATH - strlen(g_config_path) - 1);
            return;
        }
    }
    strcpy(g_config_path, CONFIG_FILE);
}

static char *DupPrintf(const char *fmt, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    return _strdup(buf);
}

static void NormalizeEndpoint(char *s) {
    for (char *p = s; *p; ++p) {
        if (*p == '\\') *p = '/';
    }
}

static void EscapeIniValue(const char *src, char *dst, int dst_size) {
    int o = 0;
    for (int i = 0; src && src[i] && o + 2 < dst_size; ++i) {
        char c = src[i];
        if (c == '\\') {
            dst[o++] = '\\'; dst[o++] = '\\';
        } else if (c == '\n') {
            dst[o++] = '\\'; dst[o++] = 'n';
        } else if (c == '\r') {
            dst[o++] = '\\'; dst[o++] = 'r';
        } else {
            dst[o++] = c;
        }
    }
    dst[o] = 0;
}

static void UnescapeIniValue(char *s) {
    if (!s) return;
    char *src = s, *dst = s;
    while (*src) {
        if (*src == '\\' && src[1]) {
            src++;
            if (*src == 'n') *dst++ = '\n';
            else if (*src == 'r') *dst++ = '\r';
            else *dst++ = *src;
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = 0;
}

static void AddTrayIcon(HWND hwnd) {
    if (g_tray_added) return;
    NOTIFYICONDATAA nid;
    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = TRAY_UID;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.uCallbackMessage = WM_APP_TRAY;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    strcpy(nid.szTip, APP_NAME);
    if (Shell_NotifyIconA(NIM_ADD, &nid)) {
        g_tray_added = 1;
    }
}

static void RemoveTrayIcon(HWND hwnd) {
    if (!g_tray_added) return;
    NOTIFYICONDATAA nid;
    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = TRAY_UID;
    Shell_NotifyIconA(NIM_DELETE, &nid);
    g_tray_added = 0;
}

static void LoadConfig(AppConfig *cfg) {
    char tmpbuf[2048];
    ConfigDefaults(cfg);
    GetPrivateProfileStringA("API", "endpoint", cfg->endpoint, cfg->endpoint, sizeof(cfg->endpoint), g_config_path);
    GetPrivateProfileStringA("API", "api_key", cfg->api_key, cfg->api_key, sizeof(cfg->api_key), g_config_path);
    GetPrivateProfileStringA("API", "model", cfg->model, cfg->model, sizeof(cfg->model), g_config_path);
    GetPrivateProfileStringA("Prompt", "prompt_1", cfg->system_prompt, cfg->system_prompt, sizeof(cfg->system_prompt), g_config_path);
    GetPrivateProfileStringA("Prompt", "prompt_2", cfg->prompt_2, cfg->prompt_2, sizeof(cfg->prompt_2), g_config_path);
    GetPrivateProfileStringA("Prompt", "prompt_3", cfg->prompt_3, cfg->prompt_3, sizeof(cfg->prompt_3), g_config_path);
    GetPrivateProfileStringA("Prompt", "user_template", cfg->user_template, cfg->user_template, sizeof(cfg->user_template), g_config_path);
    UnescapeIniValue(cfg->system_prompt);
    UnescapeIniValue(cfg->prompt_2);
    UnescapeIniValue(cfg->prompt_3);
    UnescapeIniValue(cfg->user_template);
    cfg->overlay_enabled = GetPrivateProfileIntA("UI", "overlay_enabled", cfg->overlay_enabled, g_config_path);
    cfg->overlay_visible = GetPrivateProfileIntA("UI", "overlay_visible", cfg->overlay_visible, g_config_path);
    cfg->opacity = GetPrivateProfileIntA("UI", "opacity", cfg->opacity, g_config_path);
    cfg->theme_light = GetPrivateProfileIntA("UI", "theme_light", cfg->theme_light, g_config_path);
    cfg->stream = GetPrivateProfileIntA("UI", "stream", cfg->stream, g_config_path);
    GetPrivateProfileStringA("Hotkeys", "send_prompt_1", "", tmpbuf, sizeof(tmpbuf), g_config_path);
    if (tmpbuf[0]) {
        strncpy(cfg->hk_send, tmpbuf, sizeof(cfg->hk_send) - 1);
        cfg->hk_send[sizeof(cfg->hk_send) - 1] = 0;
    }
    else {
        GetPrivateProfileStringA("Hotkeys", "send_selected", "", tmpbuf, sizeof(tmpbuf), g_config_path);
        if (tmpbuf[0]) {
            strncpy(cfg->hk_send, tmpbuf, sizeof(cfg->hk_send) - 1);
            cfg->hk_send[sizeof(cfg->hk_send) - 1] = 0;
        }
    }
    GetPrivateProfileStringA("Hotkeys", "send_prompt_2", cfg->hk_send2, cfg->hk_send2, sizeof(cfg->hk_send2), g_config_path);
    GetPrivateProfileStringA("Hotkeys", "send_prompt_3", cfg->hk_send3, cfg->hk_send3, sizeof(cfg->hk_send3), g_config_path);
    GetPrivateProfileStringA("Hotkeys", "set_tl", cfg->hk_tl, cfg->hk_tl, sizeof(cfg->hk_tl), g_config_path);
    GetPrivateProfileStringA("Hotkeys", "set_br", cfg->hk_br, cfg->hk_br, sizeof(cfg->hk_br), g_config_path);
    GetPrivateProfileStringA("Hotkeys", "toggle_enable", cfg->hk_toggle_enable, cfg->hk_toggle_enable, sizeof(cfg->hk_toggle_enable), g_config_path);
    GetPrivateProfileStringA("Hotkeys", "toggle_visible", cfg->hk_toggle_visible, cfg->hk_toggle_visible, sizeof(cfg->hk_toggle_visible), g_config_path);
    GetPrivateProfileStringA("Hotkeys", "opacity_up", cfg->hk_opacity_up, cfg->hk_opacity_up, sizeof(cfg->hk_opacity_up), g_config_path);
    GetPrivateProfileStringA("Hotkeys", "opacity_down", cfg->hk_opacity_down, cfg->hk_opacity_down, sizeof(cfg->hk_opacity_down), g_config_path);
    GetPrivateProfileStringA("Hotkeys", "open_settings", cfg->hk_open_settings, cfg->hk_open_settings, sizeof(cfg->hk_open_settings), g_config_path);
    GetPrivateProfileStringA("Hotkeys", "exit_app", cfg->hk_exit, cfg->hk_exit, sizeof(cfg->hk_exit), g_config_path);
    GetPrivateProfileStringA("Hotkeys", "cancel_request", cfg->hk_cancel, cfg->hk_cancel, sizeof(cfg->hk_cancel), g_config_path);
    if (cfg->system_prompt[0] == 0 || strcmp(cfg->system_prompt, "You are a concise helper.") == 0) {
        strcpy(cfg->system_prompt, "Debug mode: reply with exactly the user's text only. No extra words.");
    }
    if (strncmp(cfg->user_template, "Selected text:", 14) == 0) {
        strcpy(cfg->user_template, "{{text}}");
    }
}

static void SaveConfig(const AppConfig *cfg) {
    char tmp[16];
    char p1_esc[2048];
    char p2_esc[2048];
    char p3_esc[2048];
    char tpl_esc[4096];
    WritePrivateProfileStringA("API", "endpoint", cfg->endpoint, g_config_path);
    WritePrivateProfileStringA("API", "api_key", cfg->api_key, g_config_path);
    WritePrivateProfileStringA("API", "model", cfg->model, g_config_path);
    EscapeIniValue(cfg->system_prompt, p1_esc, sizeof(p1_esc));
    EscapeIniValue(cfg->prompt_2, p2_esc, sizeof(p2_esc));
    EscapeIniValue(cfg->prompt_3, p3_esc, sizeof(p3_esc));
    EscapeIniValue(cfg->user_template, tpl_esc, sizeof(tpl_esc));
    WritePrivateProfileStringA("Prompt", "prompt_1", p1_esc, g_config_path);
    WritePrivateProfileStringA("Prompt", "prompt_2", p2_esc, g_config_path);
    WritePrivateProfileStringA("Prompt", "prompt_3", p3_esc, g_config_path);
    WritePrivateProfileStringA("Prompt", "system", NULL, g_config_path);
    WritePrivateProfileStringA("Prompt", "user_template", tpl_esc, g_config_path);
    sprintf(tmp, "%d", cfg->overlay_enabled);
    WritePrivateProfileStringA("UI", "overlay_enabled", tmp, g_config_path);
    sprintf(tmp, "%d", cfg->overlay_visible);
    WritePrivateProfileStringA("UI", "overlay_visible", tmp, g_config_path);
    sprintf(tmp, "%d", cfg->opacity);
    WritePrivateProfileStringA("UI", "opacity", tmp, g_config_path);
    sprintf(tmp, "%d", cfg->theme_light);
    WritePrivateProfileStringA("UI", "theme_light", tmp, g_config_path);
    sprintf(tmp, "%d", cfg->stream);
    WritePrivateProfileStringA("UI", "stream", tmp, g_config_path);
    WritePrivateProfileStringA("Hotkeys", "send_prompt_1", cfg->hk_send, g_config_path);
    WritePrivateProfileStringA("Hotkeys", "send_prompt_2", cfg->hk_send2, g_config_path);
    WritePrivateProfileStringA("Hotkeys", "send_prompt_3", cfg->hk_send3, g_config_path);
    WritePrivateProfileStringA("Hotkeys", "send_selected", cfg->hk_send, g_config_path);
    WritePrivateProfileStringA("Hotkeys", "set_tl", cfg->hk_tl, g_config_path);
    WritePrivateProfileStringA("Hotkeys", "set_br", cfg->hk_br, g_config_path);
    WritePrivateProfileStringA("Hotkeys", "toggle_enable", cfg->hk_toggle_enable, g_config_path);
    WritePrivateProfileStringA("Hotkeys", "toggle_visible", cfg->hk_toggle_visible, g_config_path);
    WritePrivateProfileStringA("Hotkeys", "opacity_up", cfg->hk_opacity_up, g_config_path);
    WritePrivateProfileStringA("Hotkeys", "opacity_down", cfg->hk_opacity_down, g_config_path);
    WritePrivateProfileStringA("Hotkeys", "open_settings", cfg->hk_open_settings, g_config_path);
    WritePrivateProfileStringA("Hotkeys", "exit_app", cfg->hk_exit, g_config_path);
    WritePrivateProfileStringA("Hotkeys", "cancel_request", cfg->hk_cancel, g_config_path);
}

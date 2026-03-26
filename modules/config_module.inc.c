static void ConfigDefaults(AppConfig *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    strcpy(cfg->endpoint, "http://192.168.1.10:8081/v1/chat/completions");
    strcpy(cfg->api_key, "");
    strcpy(cfg->model, "qwen3-coder");
    strcpy(cfg->system_prompt, "Reply in Traditional Chinese. Plain text only. Give the direct answer only, no explanation. If and only if the question is multiple-choice, answer with selected options like: (A) (C). For non-multiple-choice questions, reply with normal direct text answer.");
    strcpy(cfg->prompt_2, "Reply in Traditional Chinese. Plain text only. Give the answer first, then add a short and simple explanation.");
    strcpy(cfg->prompt_3, "Reply in Traditional Chinese. Plain text only. Give the answer first, then provide a detailed explanation with key reasoning and conclusion.");
    strcpy(cfg->user_template, "{{text}}");
    cfg->rag_enabled = 0;
    strcpy(cfg->rag_source_path, "");
    cfg->ensemble_enabled = 0;
    strcpy(cfg->ensemble_primary_endpoint, "");
    strcpy(cfg->ensemble_primary_api_key, "");
    strcpy(cfg->ensemble_primary_model, "");
    cfg->ensemble_reviewer_count = 0;
    strcpy(cfg->ensemble_side_prompt[0], "Reply in Traditional Chinese. Give direct answers only. No explanation.");
    strcpy(cfg->ensemble_side_prompt[1], "Reply in Traditional Chinese. Answer briefly with direct answers first, minimal explanation.");
    strcpy(cfg->ensemble_side_prompt[2], "Reply in Traditional Chinese. Solve carefully and provide the likely answer with short supporting reasoning.");
    strcpy(cfg->ensemble_main_prompt[0], "Reply in Traditional Chinese. Based on the other model answers, give the final answer only. Mark disagreement only if present.");
    strcpy(cfg->ensemble_main_prompt[1], "Reply in Traditional Chinese. Based on the other model answers, summarize the best final answer and briefly mention disagreement if present.");
    strcpy(cfg->ensemble_main_prompt[2], "Reply in Traditional Chinese. Based on the other model answers, provide the final answer, explain the main reason briefly, and clearly mark disagreement if present.");
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
    strcpy(cfg->hk_scroll_up, "Ctrl+Alt+Left");
    strcpy(cfg->hk_scroll_down, "Ctrl+Alt+Right");
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

static void EncodeIniUtf8Value(const char *src, char *dst, int dst_size) {
    int o = 0;
    for (int i = 0; src && src[i] && o + 4 < dst_size; ++i) {
        unsigned char c = (unsigned char)src[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
            c == ':' || c == '/' || c == '\\') {
            dst[o++] = (char)c;
        } else {
            static const char hex[] = "0123456789ABCDEF";
            dst[o++] = '%';
            dst[o++] = hex[(c >> 4) & 0xF];
            dst[o++] = hex[c & 0xF];
        }
    }
    dst[o] = 0;
}

static void DecodeIniUtf8Value(char *s) {
    char *src = s;
    char *dst = s;
    while (src && *src) {
        if (src[0] == '%' && isxdigit((unsigned char)src[1]) && isxdigit((unsigned char)src[2])) {
            char hexbuf[3];
            hexbuf[0] = src[1];
            hexbuf[1] = src[2];
            hexbuf[2] = 0;
            *dst++ = (char)strtol(hexbuf, NULL, 16);
            src += 3;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = 0;
}

static int IsBlankText(const char *s) {
    if (!s) return 1;
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
        if (*p > ' ') return 0;
    }
    return 1;
}

static int IsReviewerAllEmptyAt(const AppConfig *cfg, int index) {
    if (!cfg || index < 0 || index >= cfg->ensemble_reviewer_count) return 1;
    return IsBlankText(cfg->ensemble_reviewer_endpoint[index]) &&
           IsBlankText(cfg->ensemble_reviewer_api_key[index]) &&
           IsBlankText(cfg->ensemble_reviewer_model[index]);
}

static int IsReviewerUsableAt(const AppConfig *cfg, int index) {
    if (!cfg || index < 0 || index >= cfg->ensemble_reviewer_count) return 0;
    return !IsBlankText(cfg->ensemble_reviewer_endpoint[index]) &&
           !IsBlankText(cfg->ensemble_reviewer_model[index]);
}

static void RemoveReviewerAt(AppConfig *cfg, int index) {
    if (!cfg || index < 0 || index >= cfg->ensemble_reviewer_count) return;
    for (int i = index; i + 1 < cfg->ensemble_reviewer_count; ++i) {
        strcpy(cfg->ensemble_reviewer_endpoint[i], cfg->ensemble_reviewer_endpoint[i + 1]);
        strcpy(cfg->ensemble_reviewer_api_key[i], cfg->ensemble_reviewer_api_key[i + 1]);
        strcpy(cfg->ensemble_reviewer_model[i], cfg->ensemble_reviewer_model[i + 1]);
    }
    if (cfg->ensemble_reviewer_count > 0) cfg->ensemble_reviewer_count--;
    if (cfg->ensemble_reviewer_count < 0) cfg->ensemble_reviewer_count = 0;
}

static int PruneEmptyReviewers(AppConfig *cfg) {
    int removed = 0;
    if (!cfg) return 0;
    for (int i = cfg->ensemble_reviewer_count - 1; i >= 0; --i) {
        if (IsReviewerAllEmptyAt(cfg, i)) {
            RemoveReviewerAt(cfg, i);
            removed++;
        }
    }
    return removed;
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
    cfg->rag_enabled = GetPrivateProfileIntA("RAG", "enabled", cfg->rag_enabled, g_config_path);
    GetPrivateProfileStringA("RAG", "source_path", cfg->rag_source_path, cfg->rag_source_path, sizeof(cfg->rag_source_path), g_config_path);
    DecodeIniUtf8Value(cfg->rag_source_path);
    cfg->ensemble_enabled = GetPrivateProfileIntA("Ensemble", "enabled", cfg->ensemble_enabled, g_config_path);
    GetPrivateProfileStringA("Ensemble", "primary_endpoint", cfg->ensemble_primary_endpoint, cfg->ensemble_primary_endpoint, sizeof(cfg->ensemble_primary_endpoint), g_config_path);
    GetPrivateProfileStringA("Ensemble", "primary_api_key", cfg->ensemble_primary_api_key, cfg->ensemble_primary_api_key, sizeof(cfg->ensemble_primary_api_key), g_config_path);
    GetPrivateProfileStringA("Ensemble", "primary_model", cfg->ensemble_primary_model, cfg->ensemble_primary_model, sizeof(cfg->ensemble_primary_model), g_config_path);
    UnescapeIniValue(cfg->ensemble_primary_endpoint);
    UnescapeIniValue(cfg->ensemble_primary_api_key);
    UnescapeIniValue(cfg->ensemble_primary_model);
    cfg->ensemble_reviewer_count = GetPrivateProfileIntA("Ensemble", "reviewer_count", 0, g_config_path);
    if (cfg->ensemble_reviewer_count < 0) cfg->ensemble_reviewer_count = 0;
    if (cfg->ensemble_reviewer_count > MAX_REVIEW_MODELS) cfg->ensemble_reviewer_count = MAX_REVIEW_MODELS;
    for (int i = 0; i < cfg->ensemble_reviewer_count; ++i) {
        char key[64];
        snprintf(key, sizeof(key), "reviewer_%d_endpoint", i + 1);
        GetPrivateProfileStringA("Ensemble", key, "", cfg->ensemble_reviewer_endpoint[i], sizeof(cfg->ensemble_reviewer_endpoint[i]), g_config_path);
        snprintf(key, sizeof(key), "reviewer_%d_api_key", i + 1);
        GetPrivateProfileStringA("Ensemble", key, "", cfg->ensemble_reviewer_api_key[i], sizeof(cfg->ensemble_reviewer_api_key[i]), g_config_path);
        snprintf(key, sizeof(key), "reviewer_%d_model", i + 1);
        GetPrivateProfileStringA("Ensemble", key, "", cfg->ensemble_reviewer_model[i], sizeof(cfg->ensemble_reviewer_model[i]), g_config_path);
        UnescapeIniValue(cfg->ensemble_reviewer_endpoint[i]);
        UnescapeIniValue(cfg->ensemble_reviewer_api_key[i]);
        UnescapeIniValue(cfg->ensemble_reviewer_model[i]);
    }
    for (int i = 0; i < 3; ++i) {
        char key[64];
        snprintf(key, sizeof(key), "side_prompt_%d", i + 1);
        GetPrivateProfileStringA("Ensemble", key, cfg->ensemble_side_prompt[i], cfg->ensemble_side_prompt[i], sizeof(cfg->ensemble_side_prompt[i]), g_config_path);
        UnescapeIniValue(cfg->ensemble_side_prompt[i]);
        snprintf(key, sizeof(key), "main_prompt_%d", i + 1);
        GetPrivateProfileStringA("Ensemble", key, cfg->ensemble_main_prompt[i], cfg->ensemble_main_prompt[i], sizeof(cfg->ensemble_main_prompt[i]), g_config_path);
        UnescapeIniValue(cfg->ensemble_main_prompt[i]);
    }
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
    GetPrivateProfileStringA("Hotkeys", "scroll_up", cfg->hk_scroll_up, cfg->hk_scroll_up, sizeof(cfg->hk_scroll_up), g_config_path);
    GetPrivateProfileStringA("Hotkeys", "scroll_down", cfg->hk_scroll_down, cfg->hk_scroll_down, sizeof(cfg->hk_scroll_down), g_config_path);
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
    SaveBasicConfig(cfg);
    SaveAdvancedConfig(cfg);
}

static void SaveBasicConfig(const AppConfig *cfg) {
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
    WritePrivateProfileStringA("Hotkeys", "scroll_up", cfg->hk_scroll_up, g_config_path);
    WritePrivateProfileStringA("Hotkeys", "scroll_down", cfg->hk_scroll_down, g_config_path);
    WritePrivateProfileStringA("Hotkeys", "open_settings", cfg->hk_open_settings, g_config_path);
    WritePrivateProfileStringA("Hotkeys", "exit_app", cfg->hk_exit, g_config_path);
    WritePrivateProfileStringA("Hotkeys", "cancel_request", cfg->hk_cancel, g_config_path);
}

static void SaveAdvancedConfig(const AppConfig *cfg) {
    char tmp[16];
    char rag_path_esc[2048];
    char ep_esc[1024];
    char key_esc[512];
    char model_esc[256];
    char prompt_esc[2048];
    sprintf(tmp, "%d", cfg->rag_enabled);
    WritePrivateProfileStringA("RAG", "enabled", tmp, g_config_path);
    EncodeIniUtf8Value(cfg->rag_source_path, rag_path_esc, sizeof(rag_path_esc));
    WritePrivateProfileStringA("RAG", "source_path", rag_path_esc, g_config_path);
    sprintf(tmp, "%d", cfg->ensemble_enabled);
    WritePrivateProfileStringA("Ensemble", "enabled", tmp, g_config_path);
    EscapeIniValue(cfg->ensemble_primary_endpoint, ep_esc, sizeof(ep_esc));
    EscapeIniValue(cfg->ensemble_primary_api_key, key_esc, sizeof(key_esc));
    EscapeIniValue(cfg->ensemble_primary_model, model_esc, sizeof(model_esc));
    WritePrivateProfileStringA("Ensemble", "primary_endpoint", ep_esc, g_config_path);
    WritePrivateProfileStringA("Ensemble", "primary_api_key", key_esc, g_config_path);
    WritePrivateProfileStringA("Ensemble", "primary_model", model_esc, g_config_path);
    sprintf(tmp, "%d", cfg->ensemble_reviewer_count);
    WritePrivateProfileStringA("Ensemble", "reviewer_count", tmp, g_config_path);
    for (int i = 0; i < 3; ++i) {
        char name[64];
        snprintf(name, sizeof(name), "side_prompt_%d", i + 1);
        WritePrivateProfileStringA("Ensemble", name, NULL, g_config_path);
        snprintf(name, sizeof(name), "main_prompt_%d", i + 1);
        EscapeIniValue(cfg->ensemble_main_prompt[i], prompt_esc, sizeof(prompt_esc));
        WritePrivateProfileStringA("Ensemble", name, prompt_esc, g_config_path);
    }
    for (int i = 0; i < MAX_REVIEW_MODELS; ++i) {
        char name[64];
        if (i < cfg->ensemble_reviewer_count) {
            EscapeIniValue(cfg->ensemble_reviewer_endpoint[i], ep_esc, sizeof(ep_esc));
            EscapeIniValue(cfg->ensemble_reviewer_api_key[i], key_esc, sizeof(key_esc));
            EscapeIniValue(cfg->ensemble_reviewer_model[i], model_esc, sizeof(model_esc));
            snprintf(name, sizeof(name), "reviewer_%d_endpoint", i + 1);
            WritePrivateProfileStringA("Ensemble", name, ep_esc, g_config_path);
            snprintf(name, sizeof(name), "reviewer_%d_api_key", i + 1);
            WritePrivateProfileStringA("Ensemble", name, key_esc, g_config_path);
            snprintf(name, sizeof(name), "reviewer_%d_model", i + 1);
            WritePrivateProfileStringA("Ensemble", name, model_esc, g_config_path);
        } else {
            snprintf(name, sizeof(name), "reviewer_%d_endpoint", i + 1);
            WritePrivateProfileStringA("Ensemble", name, NULL, g_config_path);
            snprintf(name, sizeof(name), "reviewer_%d_api_key", i + 1);
            WritePrivateProfileStringA("Ensemble", name, NULL, g_config_path);
            snprintf(name, sizeof(name), "reviewer_%d_model", i + 1);
            WritePrivateProfileStringA("Ensemble", name, NULL, g_config_path);
        }
    }
}

static int g_route_kind = 0;
static int g_route_index = -1;

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

static int Utf8ToWide(const char *src, wchar_t *dst, int dst_count) {
    if (!dst || dst_count <= 0) return 0;
    dst[0] = 0;
    if (!src) return 1;
    return MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, dst_count) > 0;
}

static int WideToUtf8(const wchar_t *src, char *dst, int dst_size) {
    if (!dst || dst_size <= 0) return 0;
    dst[0] = 0;
    if (!src) return 1;
    return WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, dst_size, NULL, NULL) > 0;
}

static void UpdateRagControlsEnabled(HWND hwnd) {
    const int enabled = GetDlgItem(hwnd, ID_CHK_RAG_ENABLED) &&
                        (IsDlgButtonChecked(hwnd, ID_CHK_RAG_ENABLED) == BST_CHECKED);
    if (GetDlgItem(hwnd, ID_LBL_RAG_PATH)) EnableWindow(GetDlgItem(hwnd, ID_LBL_RAG_PATH), enabled);
    if (GetDlgItem(hwnd, ID_EDIT_RAG_PATH)) EnableWindow(GetDlgItem(hwnd, ID_EDIT_RAG_PATH), enabled);
    if (GetDlgItem(hwnd, ID_BTN_BROWSE_RAG)) EnableWindow(GetDlgItem(hwnd, ID_BTN_BROWSE_RAG), enabled);
}

static int IsModelRouteAllBlankAt(const AppConfig *cfg, int idx) {
    if (!cfg || idx < 0 || idx >= cfg->model_route_count) return 1;
    return IsBlankText(cfg->model_route_endpoint[idx]) &&
           IsBlankText(cfg->model_route_api_key[idx]) &&
           IsBlankText(cfg->model_route_model[idx]) &&
           IsBlankText(cfg->model_route_prompt[idx]) &&
           IsBlankText(cfg->model_route_hotkey[idx]);
}

static void RemoveModelRouteAt(AppConfig *cfg, int idx) {
    if (!cfg || idx < 0 || idx >= cfg->model_route_count) return;
    for (int i = idx; i + 1 < cfg->model_route_count; ++i) {
        cfg->model_route_kind[i] = cfg->model_route_kind[i + 1];
        strcpy(cfg->model_route_endpoint[i], cfg->model_route_endpoint[i + 1]);
        strcpy(cfg->model_route_api_key[i], cfg->model_route_api_key[i + 1]);
        strcpy(cfg->model_route_model[i], cfg->model_route_model[i + 1]);
        strcpy(cfg->model_route_prompt[i], cfg->model_route_prompt[i + 1]);
        strcpy(cfg->model_route_hotkey[i], cfg->model_route_hotkey[i + 1]);
    }
    if (cfg->model_route_count > 0) cfg->model_route_count--;
}

static void PruneBlankModelRoutes(AppConfig *cfg) {
    if (!cfg) return;
    for (int i = cfg->model_route_count - 1; i >= 0; --i) {
        if (IsModelRouteAllBlankAt(cfg, i)) RemoveModelRouteAt(cfg, i);
    }
}

static void BuildRouteLabel(const AppConfig *cfg, int idx, char *out, int out_size) {
    const char *type_text = "Prompt";
    if (cfg->model_route_kind[idx] == 1) type_text = "Image";
    if (cfg->model_route_model[idx][0]) snprintf(out, out_size, "%d. %s - %s", idx + 1, type_text, cfg->model_route_model[idx]);
    else snprintf(out, out_size, "%d. %s", idx + 1, type_text);
}

static void LoadModelRouteEditor(HWND hwnd) {
    HWND kind_combo = GetDlgItem(hwnd, ID_CMB_ROUTE_KIND);
    HWND slot_combo = GetDlgItem(hwnd, ID_CMB_ROUTE_SLOT);
    char label[160];

    if (!kind_combo || !slot_combo) return;
    g_loading_controls = 1;

    SendMessageA(kind_combo, CB_RESETCONTENT, 0, 0);
    SendMessageA(kind_combo, CB_ADDSTRING, 0, (LPARAM)"Prompt");
    SendMessageA(kind_combo, CB_ADDSTRING, 0, (LPARAM)"Image");

    SendMessageA(slot_combo, CB_RESETCONTENT, 0, 0);
    for (int i = 0; i < g_cfg.model_route_count; ++i) {
        BuildRouteLabel(&g_cfg, i, label, sizeof(label));
        SendMessageA(slot_combo, CB_ADDSTRING, 0, (LPARAM)label);
    }

    if (g_cfg.model_route_count <= 0) {
        g_route_index = -1;
        g_route_kind = 0;
        SendMessageA(kind_combo, CB_SETCURSEL, 0, 0);
        SetDlgItemTextUtf8(hwnd, ID_EDIT_ROUTE_EP, "");
        SetDlgItemTextUtf8(hwnd, ID_EDIT_ROUTE_KEY, "");
        SetDlgItemTextUtf8(hwnd, ID_EDIT_ROUTE_MODEL, "");
        SetDlgItemTextUtf8(hwnd, ID_EDIT_ROUTE_PROMPT, "");
        SetDlgItemTextUtf8(hwnd, ID_EDIT_ROUTE_HOTKEY, "");
        if (GetDlgItem(hwnd, ID_CMB_ROUTE_SLOT)) EnableWindow(GetDlgItem(hwnd, ID_CMB_ROUTE_SLOT), FALSE);
        if (GetDlgItem(hwnd, ID_CMB_ROUTE_KIND)) EnableWindow(GetDlgItem(hwnd, ID_CMB_ROUTE_KIND), FALSE);
        if (GetDlgItem(hwnd, ID_EDIT_ROUTE_EP)) EnableWindow(GetDlgItem(hwnd, ID_EDIT_ROUTE_EP), FALSE);
        if (GetDlgItem(hwnd, ID_EDIT_ROUTE_KEY)) EnableWindow(GetDlgItem(hwnd, ID_EDIT_ROUTE_KEY), FALSE);
        if (GetDlgItem(hwnd, ID_EDIT_ROUTE_MODEL)) EnableWindow(GetDlgItem(hwnd, ID_EDIT_ROUTE_MODEL), FALSE);
        if (GetDlgItem(hwnd, ID_EDIT_ROUTE_PROMPT)) EnableWindow(GetDlgItem(hwnd, ID_EDIT_ROUTE_PROMPT), FALSE);
        if (GetDlgItem(hwnd, ID_EDIT_ROUTE_HOTKEY)) EnableWindow(GetDlgItem(hwnd, ID_EDIT_ROUTE_HOTKEY), FALSE);
        if (GetDlgItem(hwnd, ID_BTN_ROUTE_ADD)) EnableWindow(GetDlgItem(hwnd, ID_BTN_ROUTE_ADD), TRUE);
        if (GetDlgItem(hwnd, ID_BTN_ROUTE_REMOVE)) EnableWindow(GetDlgItem(hwnd, ID_BTN_ROUTE_REMOVE), FALSE);
        if (GetDlgItem(hwnd, ID_BTN_TEST_ROUTE)) EnableWindow(GetDlgItem(hwnd, ID_BTN_TEST_ROUTE), FALSE);
        g_loading_controls = 0;
        return;
    }

    if (g_route_index < 0) g_route_index = 0;
    if (g_route_index >= g_cfg.model_route_count) g_route_index = g_cfg.model_route_count - 1;
    g_route_kind = g_cfg.model_route_kind[g_route_index] ? 1 : 0;
    SendMessageA(slot_combo, CB_SETCURSEL, g_route_index, 0);
    SendMessageA(kind_combo, CB_SETCURSEL, g_route_kind, 0);

    SetDlgItemTextUtf8(hwnd, ID_EDIT_ROUTE_EP, g_cfg.model_route_endpoint[g_route_index]);
    SetDlgItemTextUtf8(hwnd, ID_EDIT_ROUTE_KEY, g_cfg.model_route_api_key[g_route_index]);
    SetDlgItemTextUtf8(hwnd, ID_EDIT_ROUTE_MODEL, g_cfg.model_route_model[g_route_index]);
    SetDlgItemTextUtf8(hwnd, ID_EDIT_ROUTE_PROMPT, g_cfg.model_route_prompt[g_route_index]);
    SetDlgItemTextUtf8(hwnd, ID_EDIT_ROUTE_HOTKEY, g_cfg.model_route_hotkey[g_route_index]);

    if (GetDlgItem(hwnd, ID_CMB_ROUTE_SLOT)) EnableWindow(GetDlgItem(hwnd, ID_CMB_ROUTE_SLOT), TRUE);
    if (GetDlgItem(hwnd, ID_CMB_ROUTE_KIND)) EnableWindow(GetDlgItem(hwnd, ID_CMB_ROUTE_KIND), TRUE);
    if (GetDlgItem(hwnd, ID_EDIT_ROUTE_EP)) EnableWindow(GetDlgItem(hwnd, ID_EDIT_ROUTE_EP), TRUE);
    if (GetDlgItem(hwnd, ID_EDIT_ROUTE_KEY)) EnableWindow(GetDlgItem(hwnd, ID_EDIT_ROUTE_KEY), TRUE);
    if (GetDlgItem(hwnd, ID_EDIT_ROUTE_MODEL)) EnableWindow(GetDlgItem(hwnd, ID_EDIT_ROUTE_MODEL), TRUE);
    if (GetDlgItem(hwnd, ID_EDIT_ROUTE_PROMPT)) EnableWindow(GetDlgItem(hwnd, ID_EDIT_ROUTE_PROMPT), TRUE);
    if (GetDlgItem(hwnd, ID_EDIT_ROUTE_HOTKEY)) EnableWindow(GetDlgItem(hwnd, ID_EDIT_ROUTE_HOTKEY), TRUE);
    if (GetDlgItem(hwnd, ID_BTN_ROUTE_ADD)) EnableWindow(GetDlgItem(hwnd, ID_BTN_ROUTE_ADD), TRUE);
    if (GetDlgItem(hwnd, ID_BTN_ROUTE_REMOVE)) EnableWindow(GetDlgItem(hwnd, ID_BTN_ROUTE_REMOVE), TRUE);
    if (GetDlgItem(hwnd, ID_BTN_TEST_ROUTE)) EnableWindow(GetDlgItem(hwnd, ID_BTN_TEST_ROUTE), TRUE);

    g_loading_controls = 0;
}

static void SaveModelRouteEditor(HWND hwnd) {
    if (g_loading_controls || g_route_index < 0 || g_route_index >= g_cfg.model_route_count) return;

    g_cfg.model_route_kind[g_route_index] = g_route_kind ? 1 : 0;
    GetDlgItemTextUtf8(hwnd, ID_EDIT_ROUTE_EP, g_cfg.model_route_endpoint[g_route_index], sizeof(g_cfg.model_route_endpoint[g_route_index]));
    GetDlgItemTextUtf8(hwnd, ID_EDIT_ROUTE_KEY, g_cfg.model_route_api_key[g_route_index], sizeof(g_cfg.model_route_api_key[g_route_index]));
    GetDlgItemTextUtf8(hwnd, ID_EDIT_ROUTE_MODEL, g_cfg.model_route_model[g_route_index], sizeof(g_cfg.model_route_model[g_route_index]));
    GetDlgItemTextUtf8(hwnd, ID_EDIT_ROUTE_PROMPT, g_cfg.model_route_prompt[g_route_index], sizeof(g_cfg.model_route_prompt[g_route_index]));
    GetDlgItemTextUtf8(hwnd, ID_EDIT_ROUTE_HOTKEY, g_cfg.model_route_hotkey[g_route_index], sizeof(g_cfg.model_route_hotkey[g_route_index]));
}

static int ValidateModelRouteHotkeys(HWND hwnd, char *err, int err_size) {
    UINT mods[64] = {0};
    UINT vks[64] = {0};
    char names[64][64];
    int n = 0;
    struct {
        const char *name;
        int id;
        const char *value;
    } base_keys[] = {
        {"Send Prompt", 201, g_cfg.hk_send},
        {"Select Area", 202, g_cfg.hk_tl},
        {"Ask Image", 203, g_cfg.hk_br},
        {"Cancel Request", 210, g_cfg.hk_cancel},
        {"Toggle Visible", 205, g_cfg.hk_toggle_visible},
        {"Opacity +", 206, g_cfg.hk_opacity_up},
        {"Opacity -", 207, g_cfg.hk_opacity_down},
        {"Scroll Up", 213, g_cfg.hk_scroll_up},
        {"Scroll Down", 214, g_cfg.hk_scroll_down},
        {"Toggle Settings", 208, g_cfg.hk_open_settings},
        {"Exit App", 209, g_cfg.hk_exit}
    };

    for (int i = 0; i < (int)(sizeof(base_keys) / sizeof(base_keys[0])); ++i) {
        char value[64];
        const char *src = base_keys[i].value;
        value[0] = 0;
        if (hwnd && GetDlgItem(hwnd, base_keys[i].id)) {
            GetWindowTextA(GetDlgItem(hwnd, base_keys[i].id), value, sizeof(value));
            src = value;
        }
        if (!src || !src[0]) continue;
        if (!ParseHotkey(src, &mods[n], &vks[n])) {
            if (err && err_size > 0) snprintf(err, err_size, "Invalid hotkey format at '%s'.", base_keys[i].name);
            return 0;
        }
        strncpy(names[n], base_keys[i].name, sizeof(names[n]) - 1);
        names[n][sizeof(names[n]) - 1] = 0;
        ++n;
    }

    for (int i = 0; i < g_cfg.model_route_count && i < MAX_MODEL_ROUTES; ++i) {
        char label[64];
        if (!g_cfg.model_route_hotkey[i][0]) continue;
        if (!ParseHotkey(g_cfg.model_route_hotkey[i], &mods[n], &vks[n])) {
            if (err && err_size > 0) snprintf(err, err_size, "Invalid hotkey format at Model Router %d.", i + 1);
            return 0;
        }
        snprintf(label, sizeof(label), "Model Route %d", i + 1);
        strncpy(names[n], label, sizeof(names[n]) - 1);
        names[n][sizeof(names[n]) - 1] = 0;
        ++n;
    }

    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            if (mods[i] == mods[j] && vks[i] == vks[j]) {
                if (err && err_size > 0) snprintf(err, err_size, "Hotkey conflict: '%s' and '%s' are the same.", names[i], names[j]);
                return 0;
            }
        }
    }

    if (err && err_size > 0) err[0] = 0;
    return 1;
}

static void SaveReviewerEditor(HWND hwnd, int index) { (void)hwnd; (void)index; }
static void LoadReviewerEditor(HWND hwnd, int index) { (void)hwnd; (void)index; }
static void RefreshReviewerSlotCombo(HWND hwnd, int select_index) { (void)hwnd; (void)select_index; }
static void PruneEmptyReviewersFromUi(HWND hwnd) { (void)hwnd; }
static void UpdateMultiLlmControls(HWND hwnd) { (void)hwnd; }

static int BrowseRagSourceWithDialog(HWND hwnd, int pick_folder) {
    IFileOpenDialog *dialog = NULL;
    IShellItem *item = NULL;
    HRESULT hr;
    FILEOPENDIALOGOPTIONS options = 0;
    char path_utf8[1024];
    wchar_t title[64];
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
        return 0;
    }
    hr = dialog->GetOptions(&options);
    if (FAILED(hr)) {
        dialog->Release();
        return 0;
    }
    options |= FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST;
    if (pick_folder) options |= FOS_PICKFOLDERS;
    else options |= FOS_FILEMUSTEXIST;
    dialog->SetOptions(options);
    Utf8ToWide(pick_folder ? "Select RAG Source Folder" : "Select RAG Source File", title, (int)(sizeof(title) / sizeof(title[0])));
    dialog->SetTitle(title);
    if (!pick_folder) {
        COMDLG_FILTERSPEC filters[] = {
            {L"Supported Files", L"*.txt;*.md;*.pdf;*.ppt;*.pptx"},
            {L"All Files", L"*.*"}
        };
        dialog->SetFileTypes((UINT)(sizeof(filters) / sizeof(filters[0])), filters);
        dialog->SetFileTypeIndex(1);
    }
    hr = dialog->Show(hwnd);
    if (FAILED(hr)) {
        dialog->Release();
        return 0;
    }
    hr = dialog->GetResult(&item);
    if (FAILED(hr) || !item) {
        dialog->Release();
        return 0;
    }
    PWSTR selected_path = NULL;
    hr = item->GetDisplayName(SIGDN_FILESYSPATH, &selected_path);
    if (SUCCEEDED(hr) && selected_path && WideToUtf8(selected_path, path_utf8, sizeof(path_utf8))) {
        SetDlgItemTextUtf8(hwnd, ID_EDIT_RAG_PATH, path_utf8);
        strncpy(g_cfg.rag_source_path, path_utf8, sizeof(g_cfg.rag_source_path) - 1);
        g_cfg.rag_source_path[sizeof(g_cfg.rag_source_path) - 1] = 0;
        CoTaskMemFree(selected_path);
        item->Release();
        dialog->Release();
        return 1;
    }
    if (selected_path) CoTaskMemFree(selected_path);
    item->Release();
    dialog->Release();
    return 0;
}

static void BrowseRagSourceFile(HWND hwnd) { BrowseRagSourceWithDialog(hwnd, 0); }
static void BrowseRagSourceFolder(HWND hwnd) { BrowseRagSourceWithDialog(hwnd, 1); }

static void BrowseRagSourcePath(HWND hwnd) {
    HMENU menu = CreatePopupMenu();
    RECT rc;
    if (!menu) return;
    AppendMenuA(menu, MF_STRING, 1, "Select File");
    AppendMenuA(menu, MF_STRING, 2, "Select Folder");
    GetWindowRect(GetDlgItem(hwnd, ID_BTN_BROWSE_RAG), &rc);
    switch (TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, rc.left, rc.bottom, 0, hwnd, NULL)) {
    case 1: BrowseRagSourceFile(hwnd); break;
    case 2: BrowseRagSourceFolder(hwnd); break;
    }
    DestroyMenu(menu);
}

static int IsHotkeyButtonId(int id) {
    return id == 201 || id == 202 || id == 203 || id == 205 || id == 206 || id == 207 ||
           id == 208 || id == 209 || id == 210 || id == 213 || id == 214 || id == ID_EDIT_ROUTE_HOTKEY;
}

static const char *HotkeyIdName(int id) {
    switch (id) {
    case 201: return "Send Prompt-1";
    case 202: return "Select Area";
    case 203: return "Ask Image-1";
    case 205: return "Toggle Visible";
    case 206: return "Opacity +";
    case 207: return "Opacity -";
    case 208: return "Toggle Settings";
    case 209: return "Exit App";
    case 210: return "Cancel Request";
    case 213: return "Scroll Up";
    case 214: return "Scroll Down";
    case ID_EDIT_ROUTE_HOTKEY: return "Model Router Hotkey";
    default: return "Unknown";
    }
}

static int ValidateHotkeyControls(HWND hwnd, int changing_id, const char *new_value, char *err, int err_size) {
    const int ids[] = {201, 202, 203, 210, 205, 206, 207, 213, 214, 208, 209, ID_EDIT_ROUTE_HOTKEY};
    UINT mods[12] = {0};
    UINT vks[12] = {0};
    int used[12] = {0};
    char text[64];

    for (int i = 0; i < (int)(sizeof(ids) / sizeof(ids[0])); ++i) {
        HWND ctrl = GetDlgItem(hwnd, ids[i]);
        if (!ctrl) continue;
        if (ids[i] == changing_id && new_value) {
            strncpy(text, new_value, sizeof(text) - 1);
            text[sizeof(text) - 1] = 0;
        } else {
            GetWindowTextA(ctrl, text, sizeof(text));
        }
        if (!text[0]) continue;
        if (!ParseHotkey(text, &mods[i], &vks[i])) {
            if (err && err_size > 0) snprintf(err, err_size, "Invalid hotkey format at '%s'.", HotkeyIdName(ids[i]));
            return 0;
        }
        used[i] = 1;
    }

    for (int i = 0; i < (int)(sizeof(ids) / sizeof(ids[0])); ++i) {
        if (!used[i]) continue;
        for (int j = i + 1; j < (int)(sizeof(ids) / sizeof(ids[0])); ++j) {
            if (!used[j]) continue;
            if (mods[i] == mods[j] && vks[i] == vks[j]) {
                if (err && err_size > 0) snprintf(err, err_size, "Hotkey conflict: '%s' and '%s' are the same.", HotkeyIdName(ids[i]), HotkeyIdName(ids[j]));
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

    if (GetDlgItem(hwnd, ID_CHK_RAG_ENABLED)) CheckDlgButton(hwnd, ID_CHK_RAG_ENABLED, cfg->rag_enabled ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemTextUtf8(hwnd, ID_EDIT_RAG_PATH, cfg->rag_source_path);

    SetDlgItemTextUtf8(hwnd, 201, cfg->hk_send);
    SetDlgItemTextUtf8(hwnd, 202, cfg->hk_tl);
    SetDlgItemTextUtf8(hwnd, 203, cfg->hk_br);
    SetDlgItemTextUtf8(hwnd, 210, cfg->hk_cancel);
    SetDlgItemTextUtf8(hwnd, 205, cfg->hk_toggle_visible);
    SetDlgItemTextUtf8(hwnd, 206, cfg->hk_opacity_up);
    SetDlgItemTextUtf8(hwnd, 207, cfg->hk_opacity_down);
    SetDlgItemTextUtf8(hwnd, 213, cfg->hk_scroll_up);
    SetDlgItemTextUtf8(hwnd, 214, cfg->hk_scroll_down);
    SetDlgItemTextUtf8(hwnd, 208, cfg->hk_open_settings);
    SetDlgItemTextUtf8(hwnd, 209, cfg->hk_exit);

    if (GetDlgItem(hwnd, 302)) CheckDlgButton(hwnd, 302, cfg->overlay_visible ? BST_CHECKED : BST_UNCHECKED);
    if (GetDlgItem(hwnd, ID_CHK_STREAM)) CheckDlgButton(hwnd, ID_CHK_STREAM, cfg->stream ? BST_CHECKED : BST_UNCHECKED);
    if (GetDlgItem(hwnd, ID_CHK_DARK_THEME)) CheckDlgButton(hwnd, ID_CHK_DARK_THEME, cfg->theme_light ? BST_UNCHECKED : BST_CHECKED);
    snprintf(opbuf, sizeof(opbuf), "%d", cfg->opacity);
    if (GetDlgItem(hwnd, 303)) SetWindowTextA(GetDlgItem(hwnd, 303), opbuf);

    LoadModelRouteEditor(hwnd);

    if (GetDlgItem(hwnd, ID_EDIT_PROMPT) && GetWindowTextLengthA(GetDlgItem(hwnd, ID_EDIT_PROMPT)) == 0) {
        SetWindowTextA(GetDlgItem(hwnd, ID_EDIT_PROMPT), "Test LLM prompt: please reply alive.");
    }
    UpdateRagControlsEnabled(hwnd);
    g_loading_controls = 0;
}

static void ApplyRuntimeHotkeysFromControls(HWND hwnd) {
    if (GetDlgItem(hwnd, 201)) GetDlgItemTextUtf8(hwnd, 201, g_cfg.hk_send, sizeof(g_cfg.hk_send));
    if (GetDlgItem(hwnd, 202)) GetDlgItemTextUtf8(hwnd, 202, g_cfg.hk_tl, sizeof(g_cfg.hk_tl));
    if (GetDlgItem(hwnd, 203)) GetDlgItemTextUtf8(hwnd, 203, g_cfg.hk_br, sizeof(g_cfg.hk_br));
    if (GetDlgItem(hwnd, 210)) GetDlgItemTextUtf8(hwnd, 210, g_cfg.hk_cancel, sizeof(g_cfg.hk_cancel));
    if (GetDlgItem(hwnd, 205)) GetDlgItemTextUtf8(hwnd, 205, g_cfg.hk_toggle_visible, sizeof(g_cfg.hk_toggle_visible));
    if (GetDlgItem(hwnd, 206)) GetDlgItemTextUtf8(hwnd, 206, g_cfg.hk_opacity_up, sizeof(g_cfg.hk_opacity_up));
    if (GetDlgItem(hwnd, 207)) GetDlgItemTextUtf8(hwnd, 207, g_cfg.hk_opacity_down, sizeof(g_cfg.hk_opacity_down));
    if (GetDlgItem(hwnd, 213)) GetDlgItemTextUtf8(hwnd, 213, g_cfg.hk_scroll_up, sizeof(g_cfg.hk_scroll_up));
    if (GetDlgItem(hwnd, 214)) GetDlgItemTextUtf8(hwnd, 214, g_cfg.hk_scroll_down, sizeof(g_cfg.hk_scroll_down));
    if (GetDlgItem(hwnd, 208)) GetDlgItemTextUtf8(hwnd, 208, g_cfg.hk_open_settings, sizeof(g_cfg.hk_open_settings));
    if (GetDlgItem(hwnd, 209)) GetDlgItemTextUtf8(hwnd, 209, g_cfg.hk_exit, sizeof(g_cfg.hk_exit));
    SaveModelRouteEditor(hwnd);
    RegisterHotkeys(g_hwnd_main, &g_cfg);
}

static void ApplyRuntimeConfigFromControls(HWND hwnd) {
    if (g_loading_controls) return;
    char obuf[32];
    if (GetDlgItem(hwnd, 101)) GetDlgItemTextUtf8(hwnd, 101, g_cfg.endpoint, sizeof(g_cfg.endpoint));
    if (GetDlgItem(hwnd, 102)) GetDlgItemTextUtf8(hwnd, 102, g_cfg.api_key, sizeof(g_cfg.api_key));
    if (GetDlgItem(hwnd, 103)) GetDlgItemTextUtf8(hwnd, 103, g_cfg.model, sizeof(g_cfg.model));
    if (GetDlgItem(hwnd, 104)) GetDlgItemTextUtf8(hwnd, 104, g_cfg.system_prompt, sizeof(g_cfg.system_prompt));
    if (GetDlgItem(hwnd, ID_CHK_RAG_ENABLED)) g_cfg.rag_enabled = (IsDlgButtonChecked(hwnd, ID_CHK_RAG_ENABLED) == BST_CHECKED);
    if (GetDlgItem(hwnd, ID_EDIT_RAG_PATH)) GetDlgItemTextUtf8(hwnd, ID_EDIT_RAG_PATH, g_cfg.rag_source_path, sizeof(g_cfg.rag_source_path));
    SaveModelRouteEditor(hwnd);

    if (GetDlgItem(hwnd, 302)) g_cfg.overlay_visible = (IsDlgButtonChecked(hwnd, 302) == BST_CHECKED);
    if (GetDlgItem(hwnd, ID_CHK_STREAM)) g_cfg.stream = (IsDlgButtonChecked(hwnd, ID_CHK_STREAM) == BST_CHECKED);
    if (GetDlgItem(hwnd, 303)) {
        GetWindowTextA(GetDlgItem(hwnd, 303), obuf, sizeof(obuf));
        g_cfg.opacity = ClampInt(atoi(obuf), 30, 255);
        if (g_hwnd_overlay) SetLayeredWindowAttributes(g_hwnd_overlay, 0, (BYTE)g_cfg.opacity, LWA_ALPHA);
    }
    if (GetDlgItem(hwnd, ID_CHK_DARK_THEME)) g_cfg.theme_light = (IsDlgButtonChecked(hwnd, ID_CHK_DARK_THEME) == BST_CHECKED) ? 0 : 1;
}

static void SyncSettingsUiFromRuntime(void) {
    if (!g_hwnd_settings || !IsWindow(g_hwnd_settings)) return;
    if (GetDlgItem(g_hwnd_settings, 302)) CheckDlgButton(g_hwnd_settings, 302, g_cfg.overlay_visible ? BST_CHECKED : BST_UNCHECKED);
    if (GetDlgItem(g_hwnd_settings, 303)) {
        char obuf[16];
        snprintf(obuf, sizeof(obuf), "%d", g_cfg.opacity);
        SetWindowTextA(GetDlgItem(g_hwnd_settings, 303), obuf);
    }
    if (GetDlgItem(g_hwnd_settings, ID_CHK_DARK_THEME)) CheckDlgButton(g_hwnd_settings, ID_CHK_DARK_THEME, g_cfg.theme_light ? BST_UNCHECKED : BST_CHECKED);
}

static void SetAdvancedVisible(HWND hwnd, int show) {
    g_show_advanced = show ? 1 : 0;
    RebuildPageControls(hwnd);
}

static void OpenHotkeyCaptureDialog(HWND owner, int target_id) {
    if (g_hwnd_hotkey_capture) { SetForegroundWindow(g_hwnd_hotkey_capture); return; }
    g_hotkey_capture_target_id = target_id;
    g_hotkey_capture_value[0] = 0;
    g_hwnd_hotkey_capture_owner = owner;
    WNDCLASSA wc;
    ZeroMemory(&wc, sizeof(wc));
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
        UINT mod = 0;
        if (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL || vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU ||
            vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_LWIN || vk == VK_RWIN) return 0;
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
                char old_value[64];
                HWND target = GetDlgItem(g_hwnd_hotkey_capture_owner, g_hotkey_capture_target_id);
                old_value[0] = 0;
                if (target) GetWindowTextA(target, old_value, sizeof(old_value));
                if (target) SetWindowTextA(target, g_hotkey_capture_value);
                if (g_hotkey_capture_target_id == ID_EDIT_ROUTE_HOTKEY) SaveModelRouteEditor(g_hwnd_hotkey_capture_owner);
                if (!ValidateHotkeyControls(g_hwnd_hotkey_capture_owner, 0, NULL, err, sizeof(err)) ||
                    !ValidateModelRouteHotkeys(g_hwnd_hotkey_capture_owner, err, sizeof(err))) {
                    if (target) SetWindowTextA(target, old_value);
                    if (g_hotkey_capture_target_id == ID_EDIT_ROUTE_HOTKEY) SaveModelRouteEditor(g_hwnd_hotkey_capture_owner);
                    MessageBoxA(hwnd, err, "Hotkey Conflict", MB_OK | MB_ICONWARNING);
                    return 0;
                }
                g_settings_dirty = 1;
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
            ApplyRuntimeHotkeysFromControls(g_hwnd_hotkey_capture_owner);
        } else {
            RegisterHotkeys(g_hwnd_main, &g_cfg);
        }
        g_hwnd_hotkey_capture_owner = NULL;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

static int IsPersistentControlId(int id) {
    return id == ID_BTN_TAB_BASIC || id == ID_BTN_TAB_ADV || id == ID_BTN_RESET || id == ID_BTN_SAVE;
}

static void CreateBasicPageControls(HWND hwnd) {
    CreateWindowA("STATIC", "Endpoint:", WS_CHILD | WS_VISIBLE, 20, 40, 100, 20, hwnd, (HMENU)ID_LBL_ENDPOINT, GetModuleHandle(NULL), NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 130, 40, 470, 20, hwnd, (HMENU)101, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "API Key:", WS_CHILD | WS_VISIBLE, 20, 70, 100, 20, hwnd, (HMENU)ID_LBL_APIKEY, GetModuleHandle(NULL), NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 130, 70, 470, 20, hwnd, (HMENU)102, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Model:", WS_CHILD | WS_VISIBLE, 20, 100, 100, 20, hwnd, (HMENU)ID_LBL_MODEL, GetModuleHandle(NULL), NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 130, 100, 470, 20, hwnd, (HMENU)103, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Prompt:", WS_CHILD | WS_VISIBLE, 20, 130, 120, 20, hwnd, (HMENU)ID_LBL_SYSTEM, GetModuleHandle(NULL), NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 130, 130, 470, 24, hwnd, (HMENU)104, GetModuleHandle(NULL), NULL);

    CreateWindowA("STATIC", "Quick Prompt:", WS_CHILD | WS_VISIBLE, 20, 166, 120, 20, hwnd, (HMENU)ID_LBL_QUICK, GetModuleHandle(NULL), NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 145, 166, 375, 24, hwnd, (HMENU)ID_EDIT_PROMPT, GetModuleHandle(NULL), NULL);
    SendMessageA(GetDlgItem(hwnd, ID_EDIT_PROMPT), EM_LIMITTEXT, 200, 0);
    CreateWindowA("BUTTON", "Ask", WS_CHILD | WS_VISIBLE | WS_DISABLED, 530, 166, 70, 24, hwnd, (HMENU)ID_BTN_ASK, GetModuleHandle(NULL), NULL);

    CreateWindowA("STATIC", "Basic Hotkeys:", WS_CHILD | WS_VISIBLE, 20, 206, 100, 20, hwnd, (HMENU)ID_LBL_HOTKEYS, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Send Prompt:", WS_CHILD | WS_VISIBLE, 20, 232, 110, 20, hwnd, (HMENU)ID_LBL_SEND, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "", WS_CHILD | WS_VISIBLE, 130, 230, 150, 24, hwnd, (HMENU)201, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Cancel Req:", WS_CHILD | WS_VISIBLE, 300, 232, 100, 20, hwnd, (HMENU)ID_LBL_CANCEL, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "", WS_CHILD | WS_VISIBLE, 420, 230, 150, 24, hwnd, (HMENU)210, GetModuleHandle(NULL), NULL);

    CreateWindowA("STATIC", "Select Area:", WS_CHILD | WS_VISIBLE, 20, 262, 110, 20, hwnd, (HMENU)ID_LBL_SETTL, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "", WS_CHILD | WS_VISIBLE, 130, 260, 150, 24, hwnd, (HMENU)202, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Ask Image:", WS_CHILD | WS_VISIBLE, 300, 262, 100, 20, hwnd, (HMENU)ID_LBL_SETBR, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "", WS_CHILD | WS_VISIBLE, 420, 260, 150, 24, hwnd, (HMENU)203, GetModuleHandle(NULL), NULL);

    CreateWindowA("STATIC", "Opacity +:", WS_CHILD | WS_VISIBLE, 20, 292, 100, 20, hwnd, (HMENU)ID_LBL_OPP, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "", WS_CHILD | WS_VISIBLE, 130, 290, 150, 24, hwnd, (HMENU)206, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Opacity -:", WS_CHILD | WS_VISIBLE, 300, 292, 100, 20, hwnd, (HMENU)ID_LBL_OPM, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "", WS_CHILD | WS_VISIBLE, 420, 290, 150, 24, hwnd, (HMENU)207, GetModuleHandle(NULL), NULL);

    CreateWindowA("STATIC", "Scroll Up:", WS_CHILD | WS_VISIBLE, 20, 322, 100, 20, hwnd, (HMENU)ID_LBL_SCROLLUP, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "", WS_CHILD | WS_VISIBLE, 130, 320, 150, 24, hwnd, (HMENU)213, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Scroll Down:", WS_CHILD | WS_VISIBLE, 300, 322, 100, 20, hwnd, (HMENU)ID_LBL_SCROLLDOWN, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "", WS_CHILD | WS_VISIBLE, 420, 320, 150, 24, hwnd, (HMENU)214, GetModuleHandle(NULL), NULL);

    CreateWindowA("STATIC", "Toggle Visible:", WS_CHILD | WS_VISIBLE, 20, 352, 100, 20, hwnd, (HMENU)ID_LBL_TOGGLEVIS, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "", WS_CHILD | WS_VISIBLE, 130, 350, 150, 24, hwnd, (HMENU)205, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Toggle Settings:", WS_CHILD | WS_VISIBLE, 300, 352, 110, 20, hwnd, (HMENU)ID_LBL_OPENSET, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "", WS_CHILD | WS_VISIBLE, 420, 350, 150, 24, hwnd, (HMENU)208, GetModuleHandle(NULL), NULL);

    CreateWindowA("STATIC", "Exit App:", WS_CHILD | WS_VISIBLE, 20, 382, 100, 20, hwnd, (HMENU)ID_LBL_EXIT, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "", WS_CHILD | WS_VISIBLE, 130, 380, 150, 24, hwnd, (HMENU)209, GetModuleHandle(NULL), NULL);

    CreateWindowA("STATIC", "Display:", WS_CHILD | WS_VISIBLE, 20, 418, 70, 20, hwnd, NULL, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "Visible", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 95, 416, 85, 20, hwnd, (HMENU)302, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "Stream", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 185, 416, 85, 20, hwnd, (HMENU)ID_CHK_STREAM, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "Dark", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 275, 416, 65, 20, hwnd, (HMENU)ID_CHK_DARK_THEME, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Opacity:", WS_CHILD | WS_VISIBLE, 350, 418, 55, 20, hwnd, (HMENU)ID_LBL_OPACITY, GetModuleHandle(NULL), NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER, 410, 416, 190, 22, hwnd, (HMENU)303, GetModuleHandle(NULL), NULL);
}

static void CreateAdvancedPageControls(HWND hwnd) {
    CreateWindowA("STATIC", "RAG:", WS_CHILD | WS_VISIBLE, 20, 50, 100, 20, hwnd, (HMENU)ID_LBL_RAG, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "Enable RAG reference source", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 130, 48, 220, 22, hwnd, (HMENU)ID_CHK_RAG_ENABLED, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Data Source:", WS_CHILD | WS_VISIBLE, 20, 80, 100, 20, hwnd, (HMENU)ID_LBL_RAG_PATH, GetModuleHandle(NULL), NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 130, 78, 385, 24, hwnd, (HMENU)ID_EDIT_RAG_PATH, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "Browse...", WS_CHILD | WS_VISIBLE, 525, 77, 75, 24, hwnd, (HMENU)ID_BTN_BROWSE_RAG, GetModuleHandle(NULL), NULL);

    CreateWindowA("STATIC", "Model Router:", WS_CHILD | WS_VISIBLE, 20, 126, 140, 20, hwnd, NULL, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Route:", WS_CHILD | WS_VISIBLE, 20, 154, 100, 20, hwnd, (HMENU)ID_LBL_ROUTE_SLOT, GetModuleHandle(NULL), NULL);
    CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_BORDER, 130, 152, 300, 180, hwnd, (HMENU)ID_CMB_ROUTE_SLOT, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "+", WS_CHILD | WS_VISIBLE, 438, 152, 30, 24, hwnd, (HMENU)ID_BTN_ROUTE_ADD, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "-", WS_CHILD | WS_VISIBLE, 472, 152, 30, 24, hwnd, (HMENU)ID_BTN_ROUTE_REMOVE, GetModuleHandle(NULL), NULL);

    CreateWindowA("STATIC", "Type:", WS_CHILD | WS_VISIBLE, 20, 184, 100, 20, hwnd, (HMENU)ID_LBL_ROUTE_KIND, GetModuleHandle(NULL), NULL);
    CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_BORDER, 130, 182, 140, 180, hwnd, (HMENU)ID_CMB_ROUTE_KIND, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Hotkey:", WS_CHILD | WS_VISIBLE, 290, 184, 80, 20, hwnd, NULL, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "", WS_CHILD | WS_VISIBLE, 360, 182, 140, 24, hwnd, (HMENU)ID_EDIT_ROUTE_HOTKEY, GetModuleHandle(NULL), NULL);

    CreateWindowA("STATIC", "Endpoint:", WS_CHILD | WS_VISIBLE, 20, 216, 100, 20, hwnd, (HMENU)ID_LBL_ROUTE_EP, GetModuleHandle(NULL), NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 130, 214, 470, 22, hwnd, (HMENU)ID_EDIT_ROUTE_EP, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "API Key:", WS_CHILD | WS_VISIBLE, 20, 244, 100, 20, hwnd, (HMENU)ID_LBL_ROUTE_KEY, GetModuleHandle(NULL), NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 130, 242, 470, 22, hwnd, (HMENU)ID_EDIT_ROUTE_KEY, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Model Name:", WS_CHILD | WS_VISIBLE, 20, 272, 100, 20, hwnd, (HMENU)ID_LBL_ROUTE_MODEL, GetModuleHandle(NULL), NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 130, 270, 470, 22, hwnd, (HMENU)ID_EDIT_ROUTE_MODEL, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Prompt:", WS_CHILD | WS_VISIBLE, 20, 300, 100, 20, hwnd, (HMENU)ID_LBL_ROUTE_PROMPT, GetModuleHandle(NULL), NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL, 130, 298, 470, 56, hwnd, (HMENU)ID_EDIT_ROUTE_PROMPT, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "Quick Test", WS_CHILD | WS_VISIBLE, 130, 362, 90, 24, hwnd, (HMENU)ID_BTN_TEST_ROUTE, GetModuleHandle(NULL), NULL);

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

    if (g_show_advanced) CreateAdvancedPageControls(hwnd);
    else CreateBasicPageControls(hwnd);

    ApplyConfigToSettingsControls(hwnd, &g_cfg);
    if (!g_show_advanced) EnableWindow(GetDlgItem(hwnd, ID_BTN_ASK), !g_req_inflight && GetWindowTextLengthA(GetDlgItem(hwnd, ID_EDIT_PROMPT)) > 0);
    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

static int IsAlwaysVisibleId(int id) { return id == ID_BTN_TAB_BASIC || id == ID_BTN_TAB_ADV || id == ID_BTN_RESET || id == ID_BTN_SAVE; }
static int IsAdvancedOnlyId(int id) {
    return id == ID_LBL_RAG || id == ID_CHK_RAG_ENABLED || id == ID_LBL_RAG_PATH || id == ID_EDIT_RAG_PATH || id == ID_BTN_BROWSE_RAG ||
           id == ID_LBL_ROUTE_KIND || id == ID_CMB_ROUTE_KIND || id == ID_LBL_ROUTE_SLOT || id == ID_CMB_ROUTE_SLOT ||
           id == ID_EDIT_ROUTE_HOTKEY || id == ID_BTN_ROUTE_ADD || id == ID_BTN_ROUTE_REMOVE || id == ID_LBL_ROUTE_EP || id == ID_EDIT_ROUTE_EP ||
           id == ID_LBL_ROUTE_KEY || id == ID_EDIT_ROUTE_KEY || id == ID_LBL_ROUTE_MODEL || id == ID_EDIT_ROUTE_MODEL ||
           id == ID_LBL_ROUTE_PROMPT || id == ID_EDIT_ROUTE_PROMPT || id == ID_BTN_TEST_ROUTE;
}

static void BuildSettingsLayout(HWND hwnd) {
    HWND c;
    while ((c = GetWindow(hwnd, GW_CHILD)) != NULL) DestroyWindow(c);
    CreateWindowA("BUTTON", g_show_advanced ? "Basic" : "[Basic]", WS_CHILD | WS_VISIBLE, 20, 10, 90, 22, hwnd, (HMENU)ID_BTN_TAB_BASIC, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", g_show_advanced ? "[Advanced]" : "Advanced", WS_CHILD | WS_VISIBLE, 115, 10, 90, 22, hwnd, (HMENU)ID_BTN_TAB_ADV, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "Reset", WS_CHILD | WS_VISIBLE, 400, 458, 95, 28, hwnd, (HMENU)ID_BTN_RESET, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "Save", WS_CHILD | WS_VISIBLE, 505, 458, 95, 28, hwnd, (HMENU)ID_BTN_SAVE, GetModuleHandle(NULL), NULL);
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
        if (id == ID_CHK_DARK_THEME && HIWORD(wparam) == BN_CLICKED) {
            g_cfg.theme_light = (IsDlgButtonChecked(hwnd, ID_CHK_DARK_THEME) == BST_CHECKED) ? 0 : 1;
            if (g_hwnd_overlay) InvalidateRect(g_hwnd_overlay, NULL, TRUE);
            g_settings_dirty = 1;
            return 0;
        }
        if (id == ID_CHK_STREAM && HIWORD(wparam) == BN_CLICKED) { g_cfg.stream = (IsDlgButtonChecked(hwnd, ID_CHK_STREAM) == BST_CHECKED); g_settings_dirty = 1; return 0; }
        if (id == ID_CHK_RAG_ENABLED && HIWORD(wparam) == BN_CLICKED) { g_cfg.rag_enabled = (IsDlgButtonChecked(hwnd, ID_CHK_RAG_ENABLED) == BST_CHECKED); UpdateRagControlsEnabled(hwnd); g_settings_dirty = 1; return 0; }
        if (id == ID_BTN_BROWSE_RAG && HIWORD(wparam) == BN_CLICKED) { BrowseRagSourcePath(hwnd); return 0; }

        if (id == ID_BTN_ROUTE_ADD && HIWORD(wparam) == BN_CLICKED) {
            SaveModelRouteEditor(hwnd);
            if (g_cfg.model_route_count >= MAX_MODEL_ROUTES) {
                MessageBoxA(hwnd, "Model Router reached max routes.", "Model Router", MB_OK | MB_ICONWARNING);
                return 0;
            }
            g_route_index = g_cfg.model_route_count;
            g_route_kind = 0;
            g_cfg.model_route_kind[g_route_index] = 0;
            g_cfg.model_route_endpoint[g_route_index][0] = 0;
            g_cfg.model_route_api_key[g_route_index][0] = 0;
            g_cfg.model_route_model[g_route_index][0] = 0;
            g_cfg.model_route_prompt[g_route_index][0] = 0;
            g_cfg.model_route_hotkey[g_route_index][0] = 0;
            g_cfg.model_route_count++;
            g_settings_dirty = 1;
            LoadModelRouteEditor(hwnd);
            return 0;
        }

        if (id == ID_BTN_ROUTE_REMOVE && HIWORD(wparam) == BN_CLICKED) {
            if (g_route_index >= 0 && g_route_index < g_cfg.model_route_count) {
                RemoveModelRouteAt(&g_cfg, g_route_index);
                if (g_route_index >= g_cfg.model_route_count) g_route_index = g_cfg.model_route_count - 1;
                g_settings_dirty = 1;
                LoadModelRouteEditor(hwnd);
            }
            return 0;
        }

        if (id == ID_CMB_ROUTE_SLOT && HIWORD(wparam) == CBN_SELCHANGE) {
            SaveModelRouteEditor(hwnd);
            g_route_index = (int)SendMessageA(GetDlgItem(hwnd, ID_CMB_ROUTE_SLOT), CB_GETCURSEL, 0, 0);
            if (g_route_index < 0) g_route_index = 0;
            LoadModelRouteEditor(hwnd);
            return 0;
        }

        if (id == ID_CMB_ROUTE_KIND && HIWORD(wparam) == CBN_SELCHANGE) {
            g_route_kind = (int)SendMessageA(GetDlgItem(hwnd, ID_CMB_ROUTE_KIND), CB_GETCURSEL, 0, 0);
            if (g_route_kind < 0 || g_route_kind > 1) g_route_kind = 0;
            SaveModelRouteEditor(hwnd);
            g_settings_dirty = 1;
            LoadModelRouteEditor(hwnd);
            return 0;
        }

        if (id == ID_BTN_TEST_ROUTE && HIWORD(wparam) == BN_CLICKED) {
            LlmTargetConfig target;
            char prompt_text[1024];
            POINT cursor;
            if (g_req_inflight) return 0;
            if (g_route_index < 0 || g_route_index >= g_cfg.model_route_count) return 0;
            memset(&target, 0, sizeof(target));
            GetDlgItemTextUtf8(hwnd, ID_EDIT_ROUTE_EP, target.endpoint, sizeof(target.endpoint));
            GetDlgItemTextUtf8(hwnd, ID_EDIT_ROUTE_KEY, target.api_key, sizeof(target.api_key));
            GetDlgItemTextUtf8(hwnd, ID_EDIT_ROUTE_MODEL, target.model, sizeof(target.model));
            GetDlgItemTextUtf8(hwnd, ID_EDIT_ROUTE_PROMPT, prompt_text, sizeof(prompt_text));
            target.stream = 0;
            if (!target.endpoint[0] || !target.model[0]) {
                MessageBoxA(hwnd, "Endpoint and Model Name are required.", "Model Router Test", MB_OK | MB_ICONWARNING);
                return 0;
            }
            GetCursorPos(&cursor);
            g_wait_prefix[0] = 0;
            StartRequestExTarget("Please reply with exactly: OK", "__RAW__", "", cursor, 0,
                                 prompt_text[0] ? prompt_text : "Reply with exactly OK only. No explanation.",
                                 &target);
            return 0;
        }

        if (id == 302 && HIWORD(wparam) == BN_CLICKED) {
            g_cfg.overlay_visible = (IsDlgButtonChecked(hwnd, 302) == BST_CHECKED);
            if (!g_cfg.overlay_visible) HideOverlay(); else ShowCachedOverlayAt(g_wait_anchor);
            g_settings_dirty = 1;
            return 0;
        }
        if (id == 303 && HIWORD(wparam) == EN_CHANGE) {
            if (g_loading_controls) return 0;
            char obuf[32];
            GetWindowTextA(GetDlgItem(hwnd, 303), obuf, sizeof(obuf));
            g_cfg.opacity = ClampInt(atoi(obuf), 30, 255);
            if (g_hwnd_overlay) { SetLayeredWindowAttributes(g_hwnd_overlay, 0, (BYTE)g_cfg.opacity, LWA_ALPHA); InvalidateRect(g_hwnd_overlay, NULL, TRUE); }
            g_settings_dirty = 1;
            return 0;
        }

        if ((id == 101 || id == ID_EDIT_ROUTE_EP) && HIWORD(wparam) == EN_KILLFOCUS) {
            char endpoint[512];
            if (id == 101) GetDlgItemTextUtf8(hwnd, 101, endpoint, sizeof(endpoint));
            else GetDlgItemTextUtf8(hwnd, ID_EDIT_ROUTE_EP, endpoint, sizeof(endpoint));
            NormalizeFriendlyEndpointAlias(endpoint, sizeof(endpoint));
            if (id == 101) {
                SetDlgItemTextUtf8(hwnd, 101, endpoint);
                strncpy(g_cfg.endpoint, endpoint, sizeof(g_cfg.endpoint) - 1);
                g_cfg.endpoint[sizeof(g_cfg.endpoint) - 1] = 0;
            } else {
                SetDlgItemTextUtf8(hwnd, ID_EDIT_ROUTE_EP, endpoint);
                SaveModelRouteEditor(hwnd);
            }
            return 0;
        }

        if ((id == 101 || id == 102 || id == 103 || id == 104 || id == ID_EDIT_RAG_PATH || id == ID_EDIT_ROUTE_EP ||
             id == ID_EDIT_ROUTE_KEY || id == ID_EDIT_ROUTE_MODEL || id == ID_EDIT_ROUTE_PROMPT) && HIWORD(wparam) == EN_CHANGE) {
            ApplyRuntimeConfigFromControls(hwnd);
            g_settings_dirty = 1;
            return 0;
        }

        if (id == ID_EDIT_PROMPT && HIWORD(wparam) == EN_CHANGE) {
            EnableWindow(GetDlgItem(hwnd, ID_BTN_ASK), !g_req_inflight && GetWindowTextLengthA(GetDlgItem(hwnd, ID_EDIT_PROMPT)) > 0);
            return 0;
        }

        if (id == ID_BTN_TAB_BASIC) {
            SaveModelRouteEditor(hwnd);
            {
                int old_count = g_cfg.model_route_count;
                PruneBlankModelRoutes(&g_cfg);
                if (g_cfg.model_route_count != old_count) g_settings_dirty = 1;
            }
            SetAdvancedVisible(hwnd, 0);
            return 0;
        }
        if (id == ID_BTN_TAB_ADV) { SetAdvancedVisible(hwnd, 1); return 0; }

        if (id == ID_BTN_SAVE) {
            char hk_err[256];
            ApplyRuntimeConfigFromControls(hwnd);
            SaveModelRouteEditor(hwnd);
            PruneBlankModelRoutes(&g_cfg);
            if (!ValidateHotkeyControls(hwnd, 0, NULL, hk_err, sizeof(hk_err))) { MessageBoxA(hwnd, hk_err, "Hotkey Validation", MB_OK | MB_ICONWARNING); return 0; }
            if (!ValidateModelRouteHotkeys(hwnd, hk_err, sizeof(hk_err))) { MessageBoxA(hwnd, hk_err, "Hotkey Validation", MB_OK | MB_ICONWARNING); return 0; }
            if (MessageBoxA(hwnd, "Save settings to config.ini now?", "Confirm Save", MB_YESNO | MB_ICONQUESTION) != IDYES) return 0;
            ApplyRuntimeHotkeysFromControls(hwnd);
            SaveConfig(&g_cfg);
            g_settings_dirty = 0;
            LoadModelRouteEditor(hwnd);
            if (g_hwnd_overlay) SetLayeredWindowAttributes(g_hwnd_overlay, 0, (BYTE)g_cfg.opacity, LWA_ALPHA);
            if (!g_cfg.overlay_visible) HideOverlay();
            if (g_hwnd_overlay) InvalidateRect(g_hwnd_overlay, NULL, TRUE);
            MessageBoxA(hwnd, "Settings saved to config.ini", "Saved", MB_OK | MB_ICONINFORMATION);
            return 0;
        }

        if (id == ID_BTN_ASK) {
            char prompt[2048];
            POINT cursor;
            if (g_req_inflight) return 0;
            GetDlgItemTextUtf8(hwnd, ID_EDIT_PROMPT, prompt, sizeof(prompt));
            GetCursorPos(&cursor);
            if (prompt[0]) {
                g_ask_inflight = 1;
                g_wait_prefix[0] = 0;
                EnableWindow(GetDlgItem(hwnd, ID_BTN_ASK), 0);
                SetWindowTextA(GetDlgItem(hwnd, ID_BTN_ASK), "Asking...");
                StartRequestEx(prompt, "__RAW__", "", cursor, 1, g_cfg.system_prompt);
            }
            return 0;
        }

        if (id == ID_BTN_RESET) {
            if (MessageBoxA(hwnd, "Reset current settings to built-in defaults? This will not save to config.ini until you press Save.", "Confirm Reset", MB_YESNO | MB_ICONWARNING) == IDYES) {
                ConfigDefaults(&g_cfg);
                g_route_index = -1;
                g_route_kind = 0;
                ApplyConfigToSettingsControls(hwnd, &g_cfg);
                RegisterHotkeys(g_hwnd_main, &g_cfg);
                if (g_hwnd_overlay) SetLayeredWindowAttributes(g_hwnd_overlay, 0, (BYTE)g_cfg.opacity, LWA_ALPHA);
                if (!g_cfg.overlay_visible) HideOverlay();
                if (g_hwnd_overlay) InvalidateRect(g_hwnd_overlay, NULL, TRUE);
                g_settings_dirty = 1;
                SetAdvancedVisible(hwnd, 0);
            }
            return 0;
        }
        break;
    }}
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

static void CreateSettingsWindow(HWND owner) {
    (void)owner;
    if (g_hwnd_settings) return;
    WNDCLASSW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = SettingsProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"LLMSettingsWindowW";
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassW(&wc);
    g_hwnd_settings = CreateWindowW(wc.lpszClassName, L"Helper",
                                    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                    100, 100, 640, 565, NULL, NULL, wc.hInstance, NULL);
    BuildSettingsLayout(g_hwnd_settings);
}

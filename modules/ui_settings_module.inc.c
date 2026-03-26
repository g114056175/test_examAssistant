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

static void SaveReviewerEditor(HWND hwnd, int index) {
    if (index < 0 || index >= g_cfg.ensemble_reviewer_count) return;
    if (GetDlgItem(hwnd, ID_EDIT_REVIEWER_EP)) GetDlgItemTextUtf8(hwnd, ID_EDIT_REVIEWER_EP, g_cfg.ensemble_reviewer_endpoint[index], sizeof(g_cfg.ensemble_reviewer_endpoint[index]));
    if (GetDlgItem(hwnd, ID_EDIT_REVIEWER_KEY)) GetDlgItemTextUtf8(hwnd, ID_EDIT_REVIEWER_KEY, g_cfg.ensemble_reviewer_api_key[index], sizeof(g_cfg.ensemble_reviewer_api_key[index]));
    if (GetDlgItem(hwnd, ID_EDIT_REVIEWER_MODEL)) GetDlgItemTextUtf8(hwnd, ID_EDIT_REVIEWER_MODEL, g_cfg.ensemble_reviewer_model[index], sizeof(g_cfg.ensemble_reviewer_model[index]));
}

static void LoadReviewerEditor(HWND hwnd, int index) {
    g_loading_controls = 1;
    if (!GetDlgItem(hwnd, ID_EDIT_REVIEWER_EP)) {
        g_loading_controls = 0;
        return;
    }
    if (index < 0 || index >= g_cfg.ensemble_reviewer_count) {
        SetDlgItemTextUtf8(hwnd, ID_EDIT_REVIEWER_EP, "");
        SetDlgItemTextUtf8(hwnd, ID_EDIT_REVIEWER_KEY, "");
        SetDlgItemTextUtf8(hwnd, ID_EDIT_REVIEWER_MODEL, "");
        g_loading_controls = 0;
        return;
    }
    SetDlgItemTextUtf8(hwnd, ID_EDIT_REVIEWER_EP, g_cfg.ensemble_reviewer_endpoint[index]);
    SetDlgItemTextUtf8(hwnd, ID_EDIT_REVIEWER_KEY, g_cfg.ensemble_reviewer_api_key[index]);
    SetDlgItemTextUtf8(hwnd, ID_EDIT_REVIEWER_MODEL, g_cfg.ensemble_reviewer_model[index]);
    g_loading_controls = 0;
}

static void RefreshReviewerSlotCombo(HWND hwnd, int select_index) {
    HWND combo = GetDlgItem(hwnd, ID_CMB_REVIEWER_SLOT);
    char label[64];
    if (!combo) return;
    SendMessageA(combo, CB_RESETCONTENT, 0, 0);
    for (int i = 0; i < g_cfg.ensemble_reviewer_count; ++i) {
        snprintf(label, sizeof(label), IsReviewerUsableAt(&g_cfg, i) ? "Side %d" : "Side %d*", i + 1);
        SendMessageA(combo, CB_ADDSTRING, 0, (LPARAM)label);
    }
    if (g_cfg.ensemble_reviewer_count <= 0) {
        g_reviewer_edit_index = 0;
        LoadReviewerEditor(hwnd, -1);
        return;
    }
    if (select_index < 0) select_index = 0;
    if (select_index >= g_cfg.ensemble_reviewer_count) select_index = g_cfg.ensemble_reviewer_count - 1;
    g_reviewer_edit_index = select_index;
    SendMessageA(combo, CB_SETCURSEL, g_reviewer_edit_index, 0);
    LoadReviewerEditor(hwnd, g_reviewer_edit_index);
}

static void PruneEmptyReviewersFromUi(HWND hwnd) {
    int old_count = g_cfg.ensemble_reviewer_count;
    if (g_cfg.ensemble_reviewer_count > 0) {
        SaveReviewerEditor(hwnd, g_reviewer_edit_index);
    }
    PruneEmptyReviewers(&g_cfg);
    if (g_cfg.ensemble_reviewer_count != old_count) {
        RefreshReviewerSlotCombo(hwnd, g_reviewer_edit_index);
        UpdateMultiLlmControls(hwnd);
    }
}

static void UpdateMultiLlmControls(HWND hwnd) {
    const int enabled = GetDlgItem(hwnd, ID_CHK_MULTI_LLM) &&
                        (IsDlgButtonChecked(hwnd, ID_CHK_MULTI_LLM) == BST_CHECKED);
    const int has_reviewer = enabled && g_cfg.ensemble_reviewer_count > 0;
    const int ids[] = {
        ID_LBL_PRIMARY_EP, ID_EDIT_PRIMARY_EP, ID_EDIT_PRIMARY_KEY, ID_EDIT_PRIMARY_MODEL, ID_BTN_TEST_PRIMARY,
        ID_CMB_REVIEWER_SLOT, ID_BTN_ADD_REVIEWER, ID_BTN_REMOVE_REVIEWER,
        ID_EDIT_REVIEWER_EP, ID_EDIT_REVIEWER_KEY, ID_EDIT_REVIEWER_MODEL, ID_BTN_TEST_REVIEWER,
        ID_LBL_MERGE1, ID_LBL_MERGE2, ID_LBL_MERGE3,
        ID_EDIT_MAIN_PROMPT1, ID_EDIT_MAIN_PROMPT2, ID_EDIT_MAIN_PROMPT3
    };
    for (int i = 0; i < (int)(sizeof(ids) / sizeof(ids[0])); ++i) {
        if (GetDlgItem(hwnd, ids[i])) EnableWindow(GetDlgItem(hwnd, ids[i]), enabled);
    }
    if (GetDlgItem(hwnd, ID_BTN_REMOVE_REVIEWER)) EnableWindow(GetDlgItem(hwnd, ID_BTN_REMOVE_REVIEWER), has_reviewer);
    if (GetDlgItem(hwnd, ID_BTN_TEST_REVIEWER)) EnableWindow(GetDlgItem(hwnd, ID_BTN_TEST_REVIEWER), has_reviewer);
    if (GetDlgItem(hwnd, ID_EDIT_REVIEWER_EP)) EnableWindow(GetDlgItem(hwnd, ID_EDIT_REVIEWER_EP), has_reviewer);
    if (GetDlgItem(hwnd, ID_EDIT_REVIEWER_KEY)) EnableWindow(GetDlgItem(hwnd, ID_EDIT_REVIEWER_KEY), has_reviewer);
    if (GetDlgItem(hwnd, ID_EDIT_REVIEWER_MODEL)) EnableWindow(GetDlgItem(hwnd, ID_EDIT_REVIEWER_MODEL), has_reviewer);
    if (GetDlgItem(hwnd, ID_CMB_REVIEWER_SLOT)) EnableWindow(GetDlgItem(hwnd, ID_CMB_REVIEWER_SLOT), has_reviewer);
}

static void LoadPrimaryTargetFromControls(HWND hwnd, LlmTargetConfig *target) {
    if (!target) return;
    memset(target, 0, sizeof(*target));
    GetDlgItemTextUtf8(hwnd, ID_EDIT_PRIMARY_EP, target->endpoint, sizeof(target->endpoint));
    GetDlgItemTextUtf8(hwnd, ID_EDIT_PRIMARY_KEY, target->api_key, sizeof(target->api_key));
    GetDlgItemTextUtf8(hwnd, ID_EDIT_PRIMARY_MODEL, target->model, sizeof(target->model));
    target->stream = 0;
}

static void LoadReviewerTargetFromControls(HWND hwnd, LlmTargetConfig *target) {
    if (!target) return;
    memset(target, 0, sizeof(*target));
    GetDlgItemTextUtf8(hwnd, ID_EDIT_REVIEWER_EP, target->endpoint, sizeof(target->endpoint));
    GetDlgItemTextUtf8(hwnd, ID_EDIT_REVIEWER_KEY, target->api_key, sizeof(target->api_key));
    GetDlgItemTextUtf8(hwnd, ID_EDIT_REVIEWER_MODEL, target->model, sizeof(target->model));
    target->stream = 0;
}

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
    case 1:
        BrowseRagSourceFile(hwnd);
        break;
    case 2:
        BrowseRagSourceFolder(hwnd);
        break;
    }
    DestroyMenu(menu);
}

static int IsHotkeyButtonId(int id) {
    return id == 201 || id == 211 || id == 212 || id == 210 || id == 202 || id == 203 ||
           id == 206 || id == 207 || id == 213 || id == 214 || id == 205 || id == 208 || id == 209;
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
    case 213: return "Scroll Up";
    case 214: return "Scroll Down";
    case 205: return "Toggle Visible";
    case 208: return "Toggle Settings";
    case 209: return "Exit App";
    default: return "Unknown";
    }
}

static int ValidateHotkeyControls(HWND hwnd, int changing_id, const char *new_value, char *err, int err_size) {
    const int ids[] = {201, 211, 212, 210, 202, 203, 206, 207, 213, 214, 205, 208, 209};
    UINT mods[13] = {0};
    UINT vks[13] = {0};
    char text[64];
    for (int i = 0; i < (int)(sizeof(ids) / sizeof(ids[0])); ++i) {
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
    for (int i = 0; i < (int)(sizeof(ids) / sizeof(ids[0])); ++i) {
        for (int j = i + 1; j < (int)(sizeof(ids) / sizeof(ids[0])); ++j) {
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
    if (GetDlgItem(hwnd, ID_CHK_RAG_ENABLED)) CheckDlgButton(hwnd, ID_CHK_RAG_ENABLED, cfg->rag_enabled ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemTextUtf8(hwnd, ID_EDIT_RAG_PATH, cfg->rag_source_path);
    if (GetDlgItem(hwnd, ID_CHK_MULTI_LLM)) CheckDlgButton(hwnd, ID_CHK_MULTI_LLM, cfg->ensemble_enabled ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemTextUtf8(hwnd, ID_EDIT_PRIMARY_EP, cfg->ensemble_primary_endpoint);
    SetDlgItemTextUtf8(hwnd, ID_EDIT_PRIMARY_KEY, cfg->ensemble_primary_api_key);
    SetDlgItemTextUtf8(hwnd, ID_EDIT_PRIMARY_MODEL, cfg->ensemble_primary_model);
    SetDlgItemTextUtf8(hwnd, ID_EDIT_SIDE_PROMPT1, cfg->ensemble_side_prompt[0]);
    SetDlgItemTextUtf8(hwnd, ID_EDIT_SIDE_PROMPT2, cfg->ensemble_side_prompt[1]);
    SetDlgItemTextUtf8(hwnd, ID_EDIT_SIDE_PROMPT3, cfg->ensemble_side_prompt[2]);
    SetDlgItemTextUtf8(hwnd, ID_EDIT_MAIN_PROMPT1, cfg->ensemble_main_prompt[0]);
    SetDlgItemTextUtf8(hwnd, ID_EDIT_MAIN_PROMPT2, cfg->ensemble_main_prompt[1]);
    SetDlgItemTextUtf8(hwnd, ID_EDIT_MAIN_PROMPT3, cfg->ensemble_main_prompt[2]);
    SetDlgItemTextUtf8(hwnd, 201, cfg->hk_send);
    SetDlgItemTextUtf8(hwnd, 211, cfg->hk_send2);
    SetDlgItemTextUtf8(hwnd, 212, cfg->hk_send3);
    SetDlgItemTextUtf8(hwnd, 202, cfg->hk_tl);
    SetDlgItemTextUtf8(hwnd, 203, cfg->hk_br);
    SetDlgItemTextUtf8(hwnd, 205, cfg->hk_toggle_visible);
    SetDlgItemTextUtf8(hwnd, 206, cfg->hk_opacity_up);
    SetDlgItemTextUtf8(hwnd, 207, cfg->hk_opacity_down);
    SetDlgItemTextUtf8(hwnd, 213, cfg->hk_scroll_up);
    SetDlgItemTextUtf8(hwnd, 214, cfg->hk_scroll_down);
    SetDlgItemTextUtf8(hwnd, 208, cfg->hk_open_settings);
    SetDlgItemTextUtf8(hwnd, 209, cfg->hk_exit);
    SetDlgItemTextUtf8(hwnd, 210, cfg->hk_cancel);
    if (GetDlgItem(hwnd, 302)) CheckDlgButton(hwnd, 302, cfg->overlay_visible ? BST_CHECKED : BST_UNCHECKED);
    snprintf(opbuf, sizeof(opbuf), "%d", cfg->opacity);
    if (GetDlgItem(hwnd, 303)) SetWindowTextA(GetDlgItem(hwnd, 303), opbuf);
    if (GetDlgItem(hwnd, ID_CHK_STREAM)) CheckDlgButton(hwnd, ID_CHK_STREAM, cfg->stream ? BST_CHECKED : BST_UNCHECKED);
    if (GetDlgItem(hwnd, ID_CHK_DARK_THEME)) CheckDlgButton(hwnd, ID_CHK_DARK_THEME, cfg->theme_light ? BST_UNCHECKED : BST_CHECKED);
    if (GetDlgItem(hwnd, ID_EDIT_PROMPT) && GetWindowTextLengthA(GetDlgItem(hwnd, ID_EDIT_PROMPT)) == 0) {
        SetWindowTextA(GetDlgItem(hwnd, ID_EDIT_PROMPT), "Test LLM prompt: please reply alive.");
    }
    RefreshReviewerSlotCombo(hwnd, g_reviewer_edit_index);
    UpdateRagControlsEnabled(hwnd);
    UpdateMultiLlmControls(hwnd);
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
    if (GetDlgItem(hwnd, 213)) GetDlgItemTextUtf8(hwnd, 213, g_cfg.hk_scroll_up, sizeof(g_cfg.hk_scroll_up));
    if (GetDlgItem(hwnd, 214)) GetDlgItemTextUtf8(hwnd, 214, g_cfg.hk_scroll_down, sizeof(g_cfg.hk_scroll_down));
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
    if (GetDlgItem(hwnd, ID_CHK_RAG_ENABLED)) g_cfg.rag_enabled = (IsDlgButtonChecked(hwnd, ID_CHK_RAG_ENABLED) == BST_CHECKED);
    if (GetDlgItem(hwnd, ID_EDIT_RAG_PATH)) GetDlgItemTextUtf8(hwnd, ID_EDIT_RAG_PATH, g_cfg.rag_source_path, sizeof(g_cfg.rag_source_path));
    if (GetDlgItem(hwnd, ID_CHK_MULTI_LLM)) g_cfg.ensemble_enabled = (IsDlgButtonChecked(hwnd, ID_CHK_MULTI_LLM) == BST_CHECKED);
    if (GetDlgItem(hwnd, ID_EDIT_PRIMARY_EP)) GetDlgItemTextUtf8(hwnd, ID_EDIT_PRIMARY_EP, g_cfg.ensemble_primary_endpoint, sizeof(g_cfg.ensemble_primary_endpoint));
    if (GetDlgItem(hwnd, ID_EDIT_PRIMARY_KEY)) GetDlgItemTextUtf8(hwnd, ID_EDIT_PRIMARY_KEY, g_cfg.ensemble_primary_api_key, sizeof(g_cfg.ensemble_primary_api_key));
    if (GetDlgItem(hwnd, ID_EDIT_PRIMARY_MODEL)) GetDlgItemTextUtf8(hwnd, ID_EDIT_PRIMARY_MODEL, g_cfg.ensemble_primary_model, sizeof(g_cfg.ensemble_primary_model));
    if (GetDlgItem(hwnd, ID_EDIT_SIDE_PROMPT1)) GetDlgItemTextUtf8(hwnd, ID_EDIT_SIDE_PROMPT1, g_cfg.ensemble_side_prompt[0], sizeof(g_cfg.ensemble_side_prompt[0]));
    if (GetDlgItem(hwnd, ID_EDIT_SIDE_PROMPT2)) GetDlgItemTextUtf8(hwnd, ID_EDIT_SIDE_PROMPT2, g_cfg.ensemble_side_prompt[1], sizeof(g_cfg.ensemble_side_prompt[1]));
    if (GetDlgItem(hwnd, ID_EDIT_SIDE_PROMPT3)) GetDlgItemTextUtf8(hwnd, ID_EDIT_SIDE_PROMPT3, g_cfg.ensemble_side_prompt[2], sizeof(g_cfg.ensemble_side_prompt[2]));
    if (GetDlgItem(hwnd, ID_EDIT_MAIN_PROMPT1)) GetDlgItemTextUtf8(hwnd, ID_EDIT_MAIN_PROMPT1, g_cfg.ensemble_main_prompt[0], sizeof(g_cfg.ensemble_main_prompt[0]));
    if (GetDlgItem(hwnd, ID_EDIT_MAIN_PROMPT2)) GetDlgItemTextUtf8(hwnd, ID_EDIT_MAIN_PROMPT2, g_cfg.ensemble_main_prompt[1], sizeof(g_cfg.ensemble_main_prompt[1]));
    if (GetDlgItem(hwnd, ID_EDIT_MAIN_PROMPT3)) GetDlgItemTextUtf8(hwnd, ID_EDIT_MAIN_PROMPT3, g_cfg.ensemble_main_prompt[2], sizeof(g_cfg.ensemble_main_prompt[2]));
    SaveReviewerEditor(hwnd, g_reviewer_edit_index);
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
    CreateWindowA("STATIC", "Scroll Up:", WS_CHILD | WS_VISIBLE, 20, 400, 100, 20, hwnd, (HMENU)ID_LBL_SCROLLUP, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "", WS_CHILD | WS_VISIBLE, 130, 398, 150, 24, hwnd, (HMENU)213, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Scroll Down:", WS_CHILD | WS_VISIBLE, 300, 400, 110, 20, hwnd, (HMENU)ID_LBL_SCROLLDOWN, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "", WS_CHILD | WS_VISIBLE, 420, 398, 150, 24, hwnd, (HMENU)214, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Toggle Visible:", WS_CHILD | WS_VISIBLE, 20, 430, 110, 20, hwnd, (HMENU)ID_LBL_TOGGLEVIS, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "", WS_CHILD | WS_VISIBLE, 130, 428, 150, 24, hwnd, (HMENU)205, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Toggle Settings:", WS_CHILD | WS_VISIBLE, 300, 430, 110, 20, hwnd, (HMENU)ID_LBL_OPENSET, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "", WS_CHILD | WS_VISIBLE, 420, 428, 150, 24, hwnd, (HMENU)208, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Exit App:", WS_CHILD | WS_VISIBLE, 20, 460, 100, 20, hwnd, (HMENU)ID_LBL_EXIT, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "", WS_CHILD | WS_VISIBLE, 130, 458, 150, 24, hwnd, (HMENU)209, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "Visible", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 492, 120, 20, hwnd, (HMENU)302, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "Stream", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 155, 492, 120, 20, hwnd, (HMENU)ID_CHK_STREAM, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "Dark Theme", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 290, 492, 120, 20, hwnd, (HMENU)ID_CHK_DARK_THEME, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Opacity:", WS_CHILD | WS_VISIBLE, 430, 492, 50, 20, hwnd, (HMENU)ID_LBL_OPACITY, GetModuleHandle(NULL), NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER, 485, 490, 115, 22, hwnd, (HMENU)303, GetModuleHandle(NULL), NULL);
}

static void CreateAdvancedPageControls(HWND hwnd) {
    CreateWindowA("STATIC", "RAG:", WS_CHILD | WS_VISIBLE, 20, 55, 100, 20, hwnd, (HMENU)ID_LBL_RAG, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "Enable RAG reference source", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 130, 53, 220, 22, hwnd, (HMENU)ID_CHK_RAG_ENABLED, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Data Source:", WS_CHILD | WS_VISIBLE, 20, 88, 100, 20, hwnd, (HMENU)ID_LBL_RAG_PATH, GetModuleHandle(NULL), NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 130, 86, 385, 24, hwnd, (HMENU)ID_EDIT_RAG_PATH, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "Browse...", WS_CHILD | WS_VISIBLE, 525, 85, 75, 24, hwnd, (HMENU)ID_BTN_BROWSE_RAG, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Main / Side:", WS_CHILD | WS_VISIBLE, 20, 135, 100, 20, hwnd, (HMENU)ID_LBL_MULTI, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "Enable multi-LLM review", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 130, 133, 220, 22, hwnd, (HMENU)ID_CHK_MULTI_LLM, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Main Endpoint:", WS_CHILD | WS_VISIBLE, 20, 168, 110, 20, hwnd, (HMENU)ID_LBL_PRIMARY_EP, GetModuleHandle(NULL), NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 135, 166, 385, 22, hwnd, (HMENU)ID_EDIT_PRIMARY_EP, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Main Token:", WS_CHILD | WS_VISIBLE, 20, 196, 110, 20, hwnd, (HMENU)ID_LBL_PRIMARY_KEY, GetModuleHandle(NULL), NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 135, 194, 385, 22, hwnd, (HMENU)ID_EDIT_PRIMARY_KEY, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Main Model:", WS_CHILD | WS_VISIBLE, 20, 224, 110, 20, hwnd, (HMENU)ID_LBL_PRIMARY_MODEL, GetModuleHandle(NULL), NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 135, 222, 300, 22, hwnd, (HMENU)ID_EDIT_PRIMARY_MODEL, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "Quick Test", WS_CHILD | WS_VISIBLE, 445, 221, 75, 24, hwnd, (HMENU)ID_BTN_TEST_PRIMARY, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Sides:", WS_CHILD | WS_VISIBLE, 20, 260, 100, 20, hwnd, (HMENU)ID_LBL_REVIEWER, GetModuleHandle(NULL), NULL);
    CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_BORDER, 135, 258, 170, 180, hwnd, (HMENU)ID_CMB_REVIEWER_SLOT, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "+", WS_CHILD | WS_VISIBLE, 315, 258, 28, 24, hwnd, (HMENU)ID_BTN_ADD_REVIEWER, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "-", WS_CHILD | WS_VISIBLE, 348, 258, 28, 24, hwnd, (HMENU)ID_BTN_REMOVE_REVIEWER, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Side Endpoint:", WS_CHILD | WS_VISIBLE, 20, 294, 110, 20, hwnd, NULL, GetModuleHandle(NULL), NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 135, 292, 385, 22, hwnd, (HMENU)ID_EDIT_REVIEWER_EP, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Side Token:", WS_CHILD | WS_VISIBLE, 20, 322, 110, 20, hwnd, NULL, GetModuleHandle(NULL), NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 135, 320, 385, 22, hwnd, (HMENU)ID_EDIT_REVIEWER_KEY, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Side Model:", WS_CHILD | WS_VISIBLE, 20, 350, 110, 20, hwnd, NULL, GetModuleHandle(NULL), NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 135, 348, 300, 22, hwnd, (HMENU)ID_EDIT_REVIEWER_MODEL, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "Quick Test", WS_CHILD | WS_VISIBLE, 445, 347, 75, 24, hwnd, (HMENU)ID_BTN_TEST_REVIEWER, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Merge Prompt-1:", WS_CHILD | WS_VISIBLE, 20, 410, 100, 18, hwnd, (HMENU)ID_LBL_MERGE1, GetModuleHandle(NULL), NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 130, 408, 470, 22, hwnd, (HMENU)ID_EDIT_MAIN_PROMPT1, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Merge Prompt-2:", WS_CHILD | WS_VISIBLE, 20, 436, 100, 18, hwnd, (HMENU)ID_LBL_MERGE2, GetModuleHandle(NULL), NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 130, 434, 470, 22, hwnd, (HMENU)ID_EDIT_MAIN_PROMPT2, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Merge Prompt-3:", WS_CHILD | WS_VISIBLE, 20, 462, 100, 18, hwnd, (HMENU)ID_LBL_MERGE3, GetModuleHandle(NULL), NULL);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 130, 460, 470, 22, hwnd, (HMENU)ID_EDIT_MAIN_PROMPT3, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "* Side with incomplete endpoint/model is unavailable.", WS_CHILD | WS_VISIBLE, 20, 492, 580, 18, hwnd, NULL, GetModuleHandle(NULL), NULL);
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
    SetWindowTextA(GetDlgItem(hwnd, ID_BTN_SAVE), g_show_advanced ? "Save Adv" : "Save Basic");
    if (g_show_advanced) CreateAdvancedPageControls(hwnd); else CreateBasicPageControls(hwnd);
    ApplyConfigToSettingsControls(hwnd, &g_cfg);
    if (!g_show_advanced) EnableWindow(GetDlgItem(hwnd, ID_BTN_ASK), !g_req_inflight && GetWindowTextLengthA(GetDlgItem(hwnd, ID_EDIT_PROMPT)) > 0);
    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

static int IsAlwaysVisibleId(int id) { return id == ID_BTN_TAB_BASIC || id == ID_BTN_TAB_ADV || id == ID_BTN_RESET || id == ID_BTN_SAVE; }
static int IsAdvancedOnlyId(int id) {
    return id == ID_LBL_RAG || id == ID_CHK_RAG_ENABLED || id == ID_LBL_RAG_PATH ||
           id == ID_EDIT_RAG_PATH || id == ID_BTN_BROWSE_RAG || id == ID_LBL_MULTI ||
           id == ID_CHK_MULTI_LLM || id == ID_LBL_PRIMARY_EP || id == ID_LBL_PRIMARY_KEY ||
           id == ID_LBL_PRIMARY_MODEL || id == ID_BTN_TEST_PRIMARY || id == ID_EDIT_PRIMARY_EP ||
           id == ID_EDIT_PRIMARY_KEY || id == ID_EDIT_PRIMARY_MODEL || id == ID_LBL_REVIEWER ||
           id == ID_CMB_REVIEWER_SLOT || id == ID_BTN_ADD_REVIEWER || id == ID_BTN_REMOVE_REVIEWER ||
           id == ID_EDIT_REVIEWER_EP || id == ID_EDIT_REVIEWER_KEY || id == ID_EDIT_REVIEWER_MODEL ||
           id == ID_BTN_TEST_REVIEWER || id == ID_EDIT_SIDE_PROMPT1 || id == ID_EDIT_SIDE_PROMPT2 ||
           id == ID_EDIT_SIDE_PROMPT3 || id == ID_EDIT_MAIN_PROMPT1 || id == ID_EDIT_MAIN_PROMPT2 ||
           id == ID_EDIT_MAIN_PROMPT3;
}

static void BuildSettingsLayout(HWND hwnd) {
    HWND c;
    while ((c = GetWindow(hwnd, GW_CHILD)) != NULL) DestroyWindow(c);
    CreateWindowA("BUTTON", g_show_advanced ? "Basic" : "[Basic]", WS_CHILD | WS_VISIBLE, 20, 10, 90, 22, hwnd, (HMENU)ID_BTN_TAB_BASIC, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", g_show_advanced ? "[Advanced]" : "Advanced", WS_CHILD | WS_VISIBLE, 115, 10, 90, 22, hwnd, (HMENU)ID_BTN_TAB_ADV, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "Reset", WS_CHILD | WS_VISIBLE, 400, 620, 95, 28, hwnd, (HMENU)ID_BTN_RESET, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "Save", WS_CHILD | WS_VISIBLE, 505, 620, 95, 28, hwnd, (HMENU)ID_BTN_SAVE, GetModuleHandle(NULL), NULL);
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
        if (id == ID_CHK_DARK_THEME && HIWORD(wparam) == BN_CLICKED) { g_cfg.theme_light = (IsDlgButtonChecked(hwnd, ID_CHK_DARK_THEME) == BST_CHECKED) ? 0 : 1; if (g_hwnd_overlay) InvalidateRect(g_hwnd_overlay, NULL, TRUE); return 0; }
        if (id == ID_CHK_STREAM && HIWORD(wparam) == BN_CLICKED) { g_cfg.stream = (IsDlgButtonChecked(hwnd, ID_CHK_STREAM) == BST_CHECKED); return 0; }
        if (id == ID_CHK_RAG_ENABLED && HIWORD(wparam) == BN_CLICKED) {
            g_cfg.rag_enabled = (IsDlgButtonChecked(hwnd, ID_CHK_RAG_ENABLED) == BST_CHECKED);
            UpdateRagControlsEnabled(hwnd);
            return 0;
        }
        if (id == ID_CHK_MULTI_LLM && HIWORD(wparam) == BN_CLICKED) {
            g_cfg.ensemble_enabled = (IsDlgButtonChecked(hwnd, ID_CHK_MULTI_LLM) == BST_CHECKED);
            UpdateMultiLlmControls(hwnd);
            return 0;
        }
        if (id == ID_BTN_BROWSE_RAG && HIWORD(wparam) == BN_CLICKED) { BrowseRagSourcePath(hwnd); return 0; }
        if (id == ID_BTN_ADD_REVIEWER && HIWORD(wparam) == BN_CLICKED) {
            if (g_cfg.ensemble_reviewer_count > 0) SaveReviewerEditor(hwnd, g_reviewer_edit_index);
            if (g_cfg.ensemble_reviewer_count < MAX_REVIEW_MODELS) {
                const int idx = g_cfg.ensemble_reviewer_count++;
                g_cfg.ensemble_reviewer_endpoint[idx][0] = 0;
                g_cfg.ensemble_reviewer_api_key[idx][0] = 0;
                g_cfg.ensemble_reviewer_model[idx][0] = 0;
                RefreshReviewerSlotCombo(hwnd, idx);
                UpdateMultiLlmControls(hwnd);
            }
            return 0;
        }
        if (id == ID_BTN_REMOVE_REVIEWER && HIWORD(wparam) == BN_CLICKED) {
            if (g_cfg.ensemble_reviewer_count > 0 && g_reviewer_edit_index >= 0 && g_reviewer_edit_index < g_cfg.ensemble_reviewer_count) {
                SaveReviewerEditor(hwnd, g_reviewer_edit_index);
                if (!IsReviewerAllEmptyAt(&g_cfg, g_reviewer_edit_index)) {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "Side %d is not empty. Remove it anyway?", g_reviewer_edit_index + 1);
                    if (MessageBoxA(hwnd, msg, "Confirm Remove Side", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) {
                        return 0;
                    }
                }
                RemoveReviewerAt(&g_cfg, g_reviewer_edit_index);
                RefreshReviewerSlotCombo(hwnd, g_reviewer_edit_index);
                UpdateMultiLlmControls(hwnd);
            }
            return 0;
        }
        if (id == ID_CMB_REVIEWER_SLOT && HIWORD(wparam) == CBN_SELCHANGE) {
            int next_index = (int)SendMessageA(GetDlgItem(hwnd, ID_CMB_REVIEWER_SLOT), CB_GETCURSEL, 0, 0);
            SaveReviewerEditor(hwnd, g_reviewer_edit_index);
            RefreshReviewerSlotCombo(hwnd, next_index);
            return 0;
        }
        if (id == ID_BTN_TEST_PRIMARY && HIWORD(wparam) == BN_CLICKED) {
            LlmTargetConfig target;
            POINT cursor;
            if (g_req_inflight) return 0;
            LoadPrimaryTargetFromControls(hwnd, &target);
            if (!target.endpoint[0] || !target.model[0]) {
                MessageBoxA(hwnd, "Primary endpoint and model are required.", "Primary Test", MB_OK | MB_ICONWARNING);
                return 0;
            }
            GetCursorPos(&cursor);
            g_wait_prefix[0] = 0;
            StartRequestExTarget("Please reply with exactly: OK", "__RAW__", "", cursor, 0,
                                 "Reply with exactly OK only. No explanation.", &target);
            return 0;
        }
        if (id == ID_BTN_TEST_REVIEWER && HIWORD(wparam) == BN_CLICKED) {
            LlmTargetConfig target;
            POINT cursor;
            if (g_req_inflight) return 0;
            LoadReviewerTargetFromControls(hwnd, &target);
            if (!target.endpoint[0] || !target.model[0]) {
                MessageBoxA(hwnd, "Reviewer endpoint and model are required.", "Reviewer Test", MB_OK | MB_ICONWARNING);
                return 0;
            }
            GetCursorPos(&cursor);
            g_wait_prefix[0] = 0;
            StartRequestExTarget("Please reply with exactly: OK", "__RAW__", "", cursor, 0,
                                 "Reply with exactly OK only. No explanation.", &target);
            return 0;
        }
        if (id == 302 && HIWORD(wparam) == BN_CLICKED) { g_cfg.overlay_visible = (IsDlgButtonChecked(hwnd, 302) == BST_CHECKED); if (!g_cfg.overlay_visible) HideOverlay(); else ShowCachedOverlayAt(g_wait_anchor); return 0; }
        if (id == 303 && HIWORD(wparam) == EN_CHANGE) {
            if (g_loading_controls) return 0;
            char obuf[32]; GetWindowTextA(GetDlgItem(hwnd, 303), obuf, sizeof(obuf));
            g_cfg.opacity = ClampInt(atoi(obuf), 30, 255);
            if (g_hwnd_overlay) { SetLayeredWindowAttributes(g_hwnd_overlay, 0, (BYTE)g_cfg.opacity, LWA_ALPHA); InvalidateRect(g_hwnd_overlay, NULL, TRUE); }
            return 0;
        }
        if ((id == 101 || id == ID_EDIT_PRIMARY_EP || id == ID_EDIT_REVIEWER_EP) && HIWORD(wparam) == EN_KILLFOCUS) {
            char endpoint[512];
            if (id == 101) GetDlgItemTextUtf8(hwnd, 101, endpoint, sizeof(endpoint));
            else if (id == ID_EDIT_PRIMARY_EP) GetDlgItemTextUtf8(hwnd, ID_EDIT_PRIMARY_EP, endpoint, sizeof(endpoint));
            else GetDlgItemTextUtf8(hwnd, ID_EDIT_REVIEWER_EP, endpoint, sizeof(endpoint));
            NormalizeFriendlyEndpointAlias(endpoint, sizeof(endpoint));
            if (id == 101) {
                SetDlgItemTextUtf8(hwnd, 101, endpoint);
                strncpy(g_cfg.endpoint, endpoint, sizeof(g_cfg.endpoint) - 1);
                g_cfg.endpoint[sizeof(g_cfg.endpoint) - 1] = 0;
            } else if (id == ID_EDIT_PRIMARY_EP) {
                SetDlgItemTextUtf8(hwnd, ID_EDIT_PRIMARY_EP, endpoint);
                strncpy(g_cfg.ensemble_primary_endpoint, endpoint, sizeof(g_cfg.ensemble_primary_endpoint) - 1);
                g_cfg.ensemble_primary_endpoint[sizeof(g_cfg.ensemble_primary_endpoint) - 1] = 0;
            } else {
                SetDlgItemTextUtf8(hwnd, ID_EDIT_REVIEWER_EP, endpoint);
                if (g_reviewer_edit_index >= 0 && g_reviewer_edit_index < g_cfg.ensemble_reviewer_count) {
                    strncpy(g_cfg.ensemble_reviewer_endpoint[g_reviewer_edit_index], endpoint, sizeof(g_cfg.ensemble_reviewer_endpoint[g_reviewer_edit_index]) - 1);
                    g_cfg.ensemble_reviewer_endpoint[g_reviewer_edit_index][sizeof(g_cfg.ensemble_reviewer_endpoint[g_reviewer_edit_index]) - 1] = 0;
                }
            }
            return 0;
        }
        if ((id == 101 || id == 102 || id == 103 || id == 104 || id == 106 || id == 107 || id == ID_EDIT_RAG_PATH ||
             id == ID_EDIT_PRIMARY_EP || id == ID_EDIT_PRIMARY_KEY || id == ID_EDIT_PRIMARY_MODEL ||
             id == ID_EDIT_REVIEWER_EP || id == ID_EDIT_REVIEWER_KEY || id == ID_EDIT_REVIEWER_MODEL ||
             id == ID_EDIT_SIDE_PROMPT1 || id == ID_EDIT_SIDE_PROMPT2 || id == ID_EDIT_SIDE_PROMPT3 ||
             id == ID_EDIT_MAIN_PROMPT1 || id == ID_EDIT_MAIN_PROMPT2 || id == ID_EDIT_MAIN_PROMPT3) &&
            HIWORD(wparam) == EN_CHANGE) { ApplyRuntimeConfigFromControls(hwnd); return 0; }
        if (id == ID_EDIT_PROMPT && HIWORD(wparam) == EN_CHANGE) { EnableWindow(GetDlgItem(hwnd, ID_BTN_ASK), !g_req_inflight && GetWindowTextLengthA(GetDlgItem(hwnd, ID_EDIT_PROMPT)) > 0); return 0; }
        if (id == ID_BTN_TAB_BASIC) { PruneEmptyReviewersFromUi(hwnd); SetAdvancedVisible(hwnd, 0); return 0; }
        if (id == ID_BTN_TAB_ADV) { SetAdvancedVisible(hwnd, 1); return 0; }
        if (id == ID_BTN_SAVE) {
            char buf[2048], hk_err[256];
            if (g_show_advanced) {
                if (MessageBoxA(hwnd, "Save advanced RAG settings to config.ini now?", "Confirm Save", MB_YESNO | MB_ICONQUESTION) != IDYES) return 0;
            PruneEmptyReviewersFromUi(hwnd);
                if (GetDlgItem(hwnd, ID_CHK_RAG_ENABLED)) g_cfg.rag_enabled = (IsDlgButtonChecked(hwnd, ID_CHK_RAG_ENABLED) == BST_CHECKED);
                if (GetDlgItem(hwnd, ID_EDIT_RAG_PATH)) GetDlgItemTextUtf8(hwnd, ID_EDIT_RAG_PATH, g_cfg.rag_source_path, sizeof(g_cfg.rag_source_path));
                if (GetDlgItem(hwnd, ID_CHK_MULTI_LLM)) g_cfg.ensemble_enabled = (IsDlgButtonChecked(hwnd, ID_CHK_MULTI_LLM) == BST_CHECKED);
                if (GetDlgItem(hwnd, ID_EDIT_PRIMARY_EP)) GetDlgItemTextUtf8(hwnd, ID_EDIT_PRIMARY_EP, g_cfg.ensemble_primary_endpoint, sizeof(g_cfg.ensemble_primary_endpoint));
                if (GetDlgItem(hwnd, ID_EDIT_PRIMARY_KEY)) GetDlgItemTextUtf8(hwnd, ID_EDIT_PRIMARY_KEY, g_cfg.ensemble_primary_api_key, sizeof(g_cfg.ensemble_primary_api_key));
                if (GetDlgItem(hwnd, ID_EDIT_PRIMARY_MODEL)) GetDlgItemTextUtf8(hwnd, ID_EDIT_PRIMARY_MODEL, g_cfg.ensemble_primary_model, sizeof(g_cfg.ensemble_primary_model));
                if (GetDlgItem(hwnd, ID_EDIT_MAIN_PROMPT1)) GetDlgItemTextUtf8(hwnd, ID_EDIT_MAIN_PROMPT1, g_cfg.ensemble_main_prompt[0], sizeof(g_cfg.ensemble_main_prompt[0]));
                if (GetDlgItem(hwnd, ID_EDIT_MAIN_PROMPT2)) GetDlgItemTextUtf8(hwnd, ID_EDIT_MAIN_PROMPT2, g_cfg.ensemble_main_prompt[1], sizeof(g_cfg.ensemble_main_prompt[1]));
                if (GetDlgItem(hwnd, ID_EDIT_MAIN_PROMPT3)) GetDlgItemTextUtf8(hwnd, ID_EDIT_MAIN_PROMPT3, g_cfg.ensemble_main_prompt[2], sizeof(g_cfg.ensemble_main_prompt[2]));
                SaveReviewerEditor(hwnd, g_reviewer_edit_index);
                SaveAdvancedConfig(&g_cfg);
                MessageBoxA(hwnd, "Advanced settings saved to config.ini", "Saved", MB_OK | MB_ICONINFORMATION);
                return 0;
            }
            if (GetDlgItem(hwnd, 201) && !ValidateHotkeyControls(hwnd, 0, NULL, hk_err, sizeof(hk_err))) { MessageBoxA(hwnd, hk_err, "Hotkey Validation", MB_OK | MB_ICONWARNING); return 0; }
            if (MessageBoxA(hwnd, "Save basic settings to config.ini now?", "Confirm Save", MB_YESNO | MB_ICONQUESTION) != IDYES) return 0;
            if (GetDlgItem(hwnd, 101)) GetDlgItemTextUtf8(hwnd, 101, g_cfg.endpoint, sizeof(g_cfg.endpoint));
            if (GetDlgItem(hwnd, 102)) GetDlgItemTextUtf8(hwnd, 102, g_cfg.api_key, sizeof(g_cfg.api_key));
            if (GetDlgItem(hwnd, 103)) GetDlgItemTextUtf8(hwnd, 103, g_cfg.model, sizeof(g_cfg.model));
            if (GetDlgItem(hwnd, 104)) GetDlgItemTextUtf8(hwnd, 104, g_cfg.system_prompt, sizeof(g_cfg.system_prompt));
            if (GetDlgItem(hwnd, 106)) GetDlgItemTextUtf8(hwnd, 106, g_cfg.prompt_2, sizeof(g_cfg.prompt_2));
            if (GetDlgItem(hwnd, 107)) GetDlgItemTextUtf8(hwnd, 107, g_cfg.prompt_3, sizeof(g_cfg.prompt_3));
            if (GetDlgItem(hwnd, 201)) GetDlgItemTextUtf8(hwnd, 201, g_cfg.hk_send, sizeof(g_cfg.hk_send));
            if (GetDlgItem(hwnd, 211)) GetDlgItemTextUtf8(hwnd, 211, g_cfg.hk_send2, sizeof(g_cfg.hk_send2));
            if (GetDlgItem(hwnd, 212)) GetDlgItemTextUtf8(hwnd, 212, g_cfg.hk_send3, sizeof(g_cfg.hk_send3));
            if (GetDlgItem(hwnd, 202)) GetDlgItemTextUtf8(hwnd, 202, g_cfg.hk_tl, sizeof(g_cfg.hk_tl));
            if (GetDlgItem(hwnd, 203)) GetDlgItemTextUtf8(hwnd, 203, g_cfg.hk_br, sizeof(g_cfg.hk_br));
            if (GetDlgItem(hwnd, 205)) GetDlgItemTextUtf8(hwnd, 205, g_cfg.hk_toggle_visible, sizeof(g_cfg.hk_toggle_visible));
            if (GetDlgItem(hwnd, 206)) GetDlgItemTextUtf8(hwnd, 206, g_cfg.hk_opacity_up, sizeof(g_cfg.hk_opacity_up));
            if (GetDlgItem(hwnd, 207)) GetDlgItemTextUtf8(hwnd, 207, g_cfg.hk_opacity_down, sizeof(g_cfg.hk_opacity_down));
            if (GetDlgItem(hwnd, 213)) GetDlgItemTextUtf8(hwnd, 213, g_cfg.hk_scroll_up, sizeof(g_cfg.hk_scroll_up));
            if (GetDlgItem(hwnd, 214)) GetDlgItemTextUtf8(hwnd, 214, g_cfg.hk_scroll_down, sizeof(g_cfg.hk_scroll_down));
            if (GetDlgItem(hwnd, 208)) GetDlgItemTextUtf8(hwnd, 208, g_cfg.hk_open_settings, sizeof(g_cfg.hk_open_settings));
            if (GetDlgItem(hwnd, 209)) GetDlgItemTextUtf8(hwnd, 209, g_cfg.hk_exit, sizeof(g_cfg.hk_exit));
            if (GetDlgItem(hwnd, 210)) GetDlgItemTextUtf8(hwnd, 210, g_cfg.hk_cancel, sizeof(g_cfg.hk_cancel));
            if (GetDlgItem(hwnd, 302)) g_cfg.overlay_visible = (IsDlgButtonChecked(hwnd, 302) == BST_CHECKED);
            if (GetDlgItem(hwnd, 303)) { GetWindowTextA(GetDlgItem(hwnd, 303), buf, sizeof(buf)); g_cfg.opacity = ClampInt(atoi(buf), 30, 255); }
            if (GetDlgItem(hwnd, ID_CHK_DARK_THEME)) g_cfg.theme_light = (IsDlgButtonChecked(hwnd, ID_CHK_DARK_THEME) == BST_CHECKED) ? 0 : 1;
            if (GetDlgItem(hwnd, ID_CHK_STREAM)) g_cfg.stream = (IsDlgButtonChecked(hwnd, ID_CHK_STREAM) == BST_CHECKED);
            SaveBasicConfig(&g_cfg); RegisterHotkeys(g_hwnd_main, &g_cfg);
            if (g_hwnd_overlay) SetLayeredWindowAttributes(g_hwnd_overlay, 0, (BYTE)g_cfg.opacity, LWA_ALPHA);
            if (!g_cfg.overlay_visible) HideOverlay();
            if (g_hwnd_overlay) InvalidateRect(g_hwnd_overlay, NULL, TRUE);
            MessageBoxA(hwnd, "Basic settings saved to config.ini", "Saved", MB_OK | MB_ICONINFORMATION);
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
                                    100, 100, 640, 720, NULL, NULL, wc.hInstance, NULL);
    BuildSettingsLayout(g_hwnd_settings);
}

static char *ReadFileBase64(const char *path) {
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD size = 0;
    DWORD read = 0;
    BYTE *bytes = NULL;
    DWORD out_len = 0;
    char *out = NULL;
    if (file == INVALID_HANDLE_VALUE) return NULL;
    size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0) {
        CloseHandle(file);
        return NULL;
    }
    bytes = (BYTE *)malloc(size);
    if (!bytes) {
        CloseHandle(file);
        return NULL;
    }
    if (!ReadFile(file, bytes, size, &read, NULL) || read != size) {
        CloseHandle(file);
        free(bytes);
        return NULL;
    }
    CloseHandle(file);
    if (!CryptBinaryToStringA(bytes, size, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &out_len)) {
        free(bytes);
        return NULL;
    }
    out = (char *)malloc(out_len + 1);
    if (!out) {
        free(bytes);
        return NULL;
    }
    if (!CryptBinaryToStringA(bytes, size, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, out, &out_len)) {
        free(bytes);
        free(out);
        return NULL;
    }
    out[out_len] = 0;
    free(bytes);
    return out;
}

static char *BuildImageUserMessage(void) {
    char *msg = BuildUserMessage("", "");
    if (!msg) return NULL;
    if (!HasVisibleText(msg)) {
        free(msg);
        return _strdup("Please analyze the attached screenshot and answer based on the visible content.");
    }
    return msg;
}

static char *BuildUserMessage(const char *user_text, const char *region) {
    if (region && strcmp(region, "__RAW__") == 0) {
        return _strdup(user_text ? user_text : "");
    }
    const char *tpl = g_cfg.user_template;
    size_t out_cap = strlen(tpl) + (user_text ? strlen(user_text) : 0) + (region ? strlen(region) : 0) + 64;
    char *out = (char *)malloc(out_cap);
    if (!out) return NULL;
    out[0] = 0;
    const char *p = tpl;
    while (*p) {
        if (strncmp(p, "{{text}}", 8) == 0) {
            strcat(out, user_text ? user_text : "");
            p += 8;
        } else if (strncmp(p, "{{region}}", 10) == 0) {
            strcat(out, region ? region : "");
            p += 10;
        } else {
            size_t len = strlen(out);
            out[len] = *p;
            out[len + 1] = 0;
            p++;
        }
    }
    return out;
}

static char *JsonEscape(const char *s) {
    size_t len = 0;
    for (const char *p = s; *p; ++p) {
        if (*p == '\"' || *p == '\\') len += 2;
        else if (*p == '\n' || *p == '\r' || *p == '\t') len += 2;
        else len += 1;
    }
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    char *o = out;
    for (const char *p = s; *p; ++p) {
        if (*p == '\"') { *o++ = '\\'; *o++ = '\"'; }
        else if (*p == '\\') { *o++ = '\\'; *o++ = '\\'; }
        else if (*p == '\n') { *o++ = '\\'; *o++ = 'n'; }
        else if (*p == '\r') { *o++ = '\\'; *o++ = 'r'; }
        else if (*p == '\t') { *o++ = '\\'; *o++ = 't'; }
        else { *o++ = *p; }
    }
    *o = 0;
    return out;
}

static char *ExtractDeltaContent(const char *json) {
    const char *d = strstr(json, "\"delta\"");
    if (!d) return NULL;
    const char *p = strstr(d, "\"content\"");
    if (!p) return NULL;
    p = strchr(p, ':');
    if (!p) return NULL;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != '\"') return NULL;
    p++;
    char *out = (char *)malloc(4096);
    if (!out) return NULL;
    size_t idx = 0;
    while (*p && idx + 1 < 4096) {
        if (*p == '\\') {
            p++;
            if (*p == 'n') out[idx++] = '\n';
            else if (*p == 'r') out[idx++] = '\r';
            else if (*p == 't') out[idx++] = '\t';
            else if (*p) out[idx++] = *p;
            p++;
        } else if (*p == '\"') {
            break;
        } else {
            out[idx++] = *p++;
        }
    }
    out[idx] = 0;
    return out;
}

static void StripThinkBlocks(char *text) {
    if (!text || !text[0]) return;
    char *src = text;
    char *dst = text;
    while (*src) {
        if (strncmp(src, "<think>", 7) == 0) {
            src += 7;
            while (*src && strncmp(src, "</think>", 8) != 0) src++;
            if (*src) src += 8;
            continue;
        }
        *dst++ = *src++;
    }
    *dst = 0;
    while (*text == '\n' || *text == '\r' || *text == ' ') {
        memmove(text, text + 1, strlen(text));
    }
}

static char *SendLLMRequest(const char *user_text, const char *region, const char *image_path, const char *system_prompt, int req_id) {
    ProviderRequestInfo provider;
    int use_stream;
    char endpoint[512];
    strncpy(endpoint, g_cfg.endpoint, sizeof(endpoint) - 1);
    endpoint[sizeof(endpoint) - 1] = 0;
    ResolveProviderRequestInfo(&provider, endpoint, g_cfg.api_key, g_cfg.model, g_cfg.stream);
    strncpy(endpoint, provider.endpoint, sizeof(endpoint) - 1);
    endpoint[sizeof(endpoint) - 1] = 0;
    use_stream = provider.use_stream;
    char *user_msg = image_path && image_path[0] ? BuildImageUserMessage() : BuildUserMessage(user_text, region);
    if (!user_msg) return NULL;
    char system_all[1600];
    snprintf(system_all, sizeof(system_all),
             "%s\n\nOutput rule: Return final answer only. Do not output chain-of-thought, hidden reasoning, or <think> blocks.",
             (system_prompt && system_prompt[0]) ? system_prompt : g_cfg.system_prompt);
    char *sys_esc = JsonEscape(system_all);
    char *user_esc = JsonEscape(user_msg);
    char *img_b64 = NULL;
    free(user_msg);
    if (!sys_esc || !user_esc) {
        free(sys_esc);
        free(user_esc);
        return NULL;
    }
    char *body = NULL;
    if (image_path && image_path[0]) {
        size_t body_cap;
        char *img_url_esc;
        img_b64 = ReadFileBase64(image_path);
        if (!img_b64) {
            free(sys_esc);
            free(user_esc);
            return DupPrintf("Failed to read screenshot: %s", image_path);
        }
        body_cap = strlen(g_cfg.model) + strlen(sys_esc) + strlen(user_esc) + strlen(img_b64) + 512;
        body = (char *)malloc(body_cap);
        if (!body) {
            free(sys_esc);
            free(user_esc);
            free(img_b64);
            return NULL;
        }
        img_url_esc = (char *)malloc(strlen(img_b64) + 32);
        if (!img_url_esc) {
            free(sys_esc);
            free(user_esc);
            free(img_b64);
            free(body);
            return NULL;
        }
        strcpy(img_url_esc, "data:image/png;base64,");
        strcat(img_url_esc, img_b64);
        snprintf(body, body_cap,
                 "{\"model\":\"%s\",\"messages\":[{\"role\":\"system\",\"content\":\"%s\"},"
                 "{\"role\":\"user\",\"content\":[{\"type\":\"text\",\"text\":\"%s\"},"
                 "{\"type\":\"image_url\",\"image_url\":{\"url\":\"%s\"}}]}],\"temperature\":0.2,\"stream\":%s}",
                 g_cfg.model, sys_esc, user_esc, img_url_esc, use_stream ? "true" : "false");
        free(img_url_esc);
    } else {
        size_t body_cap = strlen(g_cfg.model) + strlen(sys_esc) + strlen(user_esc) + 256;
        body = (char *)malloc(body_cap);
        if (!body) {
            free(sys_esc);
            free(user_esc);
            return NULL;
        }
        snprintf(body, body_cap,
                 "{\"model\":\"%s\",\"messages\":[{\"role\":\"system\",\"content\":\"%s\"},"
                 "{\"role\":\"user\",\"content\":\"%s\"}],\"temperature\":0.2,\"stream\":%s}",
                 g_cfg.model, sys_esc, user_esc, use_stream ? "true" : "false");
    }
    if (provider.kind == PROVIDER_GOOGLE_GEMINI) {
        size_t body_cap;
        free(body);
        body = NULL;
        if (image_path && image_path[0] && !img_b64) {
            img_b64 = ReadFileBase64(image_path);
            if (!img_b64) return DupPrintf("Failed to read screenshot: %s", image_path);
        }
        body_cap = strlen(sys_esc ? sys_esc : "") + strlen(user_esc ? user_esc : "") + strlen(g_cfg.model) +
                   (img_b64 ? strlen(img_b64) : 0) + 512;
        body = (char *)malloc(body_cap);
        if (!body) {
            free(img_b64);
            free(sys_esc);
            free(user_esc);
            return NULL;
        }
        if (img_b64 && img_b64[0]) {
            snprintf(body, body_cap,
                     "{\"system_instruction\":{\"parts\":[{\"text\":\"%s\"}]},"
                     "\"contents\":[{\"role\":\"user\",\"parts\":[{\"text\":\"%s\"},"
                     "{\"inline_data\":{\"mime_type\":\"image/png\",\"data\":\"%s\"}}]}],"
                     "\"generationConfig\":{\"temperature\":0.2}}",
                     sys_esc, user_esc, img_b64);
        } else {
            snprintf(body, body_cap,
                     "{\"system_instruction\":{\"parts\":[{\"text\":\"%s\"}]},"
                     "\"contents\":[{\"role\":\"user\",\"parts\":[{\"text\":\"%s\"}]}],"
                     "\"generationConfig\":{\"temperature\":0.2}}",
                     sys_esc, user_esc);
        }
    }
    free(sys_esc);
    free(user_esc);
    WCHAR endpoint_w[512];
    if (MultiByteToWideChar(CP_UTF8, 0, endpoint, -1, endpoint_w, 512) == 0) {
        free(img_b64);
        free(body);
        return DupPrintf("Invalid endpoint encoding.");
    }
    URL_COMPONENTS url;
    ZeroMemory(&url, sizeof(url));
    url.dwStructSize = sizeof(url);
    WCHAR host_w[256];
    WCHAR path_w[1024];
    WCHAR extra_w[512];
    url.lpszHostName = host_w;
    url.dwHostNameLength = sizeof(host_w) / sizeof(WCHAR);
    url.lpszUrlPath = path_w;
    url.dwUrlPathLength = sizeof(path_w) / sizeof(WCHAR);
    url.lpszExtraInfo = extra_w;
    url.dwExtraInfoLength = sizeof(extra_w) / sizeof(WCHAR);
    if (!WinHttpCrackUrl(endpoint_w, 0, 0, &url)) {
        free(img_b64);
        free(body);
        return DupPrintf("Invalid endpoint URL: %s", endpoint);
    }
    if (url.dwHostNameLength >= (sizeof(host_w) / sizeof(WCHAR))) {
        free(img_b64);
        free(body);
        return DupPrintf("Host is too long.");
    }
    if (url.dwUrlPathLength >= (sizeof(path_w) / sizeof(WCHAR))) {
        free(img_b64);
        free(body);
        return DupPrintf("URL path is too long.");
    }
    if (url.dwExtraInfoLength >= (sizeof(extra_w) / sizeof(WCHAR))) {
        free(img_b64);
        free(body);
        return DupPrintf("URL query is too long.");
    }
    host_w[url.dwHostNameLength] = 0;
    path_w[url.dwUrlPathLength] = 0;
    extra_w[url.dwExtraInfoLength] = 0;
    WCHAR full_path_w[1536];
    full_path_w[0] = 0;
    wcsncat(full_path_w, path_w, (sizeof(full_path_w) / sizeof(WCHAR)) - 1);
    if (extra_w[0]) wcsncat(full_path_w, extra_w, (sizeof(full_path_w) / sizeof(WCHAR)) - wcslen(full_path_w) - 1);
    HINTERNET hSession = WinHttpOpen(L"LLMOverlay/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        free(img_b64);
        free(body);
        return DupPrintf("WinHttpOpen failed: %lu", GetLastError());
    }
    HINTERNET hConnect = WinHttpConnect(hSession, host_w, url.nPort, 0);
    if (!hConnect) {
        DWORD err = GetLastError();
        WinHttpCloseHandle(hSession);
        free(img_b64);
        free(body);
        return DupPrintf("WinHttpConnect failed: %lu", err);
    }
    DWORD flags = (url.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", full_path_w, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        DWORD err = GetLastError();
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        free(img_b64);
        free(body);
        return DupPrintf("WinHttpOpenRequest failed: %lu", err);
    }
    WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json", -1, WINHTTP_ADDREQ_FLAG_ADD);
    EnterCriticalSection(&g_req_cs);
    if (req_id == g_active_request_id) {
        g_active_hrequest = hRequest;
    }
    LeaveCriticalSection(&g_req_cs);
    if (g_cfg.api_key[0] && provider.kind == PROVIDER_OPENAI_COMPAT) {
        char header_auth[512];
        snprintf(header_auth, sizeof(header_auth), "Authorization: Bearer %s", g_cfg.api_key);
        WCHAR header_auth_w[512];
        MultiByteToWideChar(CP_UTF8, 0, header_auth, -1, header_auth_w, 512);
        WinHttpAddRequestHeaders(hRequest, header_auth_w, -1, WINHTTP_ADDREQ_FLAG_ADD);
    } else if (g_cfg.api_key[0] && provider.kind == PROVIDER_GOOGLE_GEMINI) {
        char header_api[512];
        WCHAR header_api_w[512];
        snprintf(header_api, sizeof(header_api), "x-goog-api-key: %s", g_cfg.api_key);
        MultiByteToWideChar(CP_UTF8, 0, header_api, -1, header_api_w, 512);
        WinHttpAddRequestHeaders(hRequest, header_api_w, -1, WINHTTP_ADDREQ_FLAG_ADD);
    }
    BOOL ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)body, (DWORD)strlen(body), (DWORD)strlen(body), 0);
    if (!ok || !WinHttpReceiveResponse(hRequest, NULL)) {
        DWORD err = GetLastError();
        int should_close_request = 0;
        EnterCriticalSection(&g_req_cs);
        if (g_active_hrequest == hRequest) {
            g_active_hrequest = NULL;
            should_close_request = 1;
        }
        LeaveCriticalSection(&g_req_cs);
        if (should_close_request) WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        free(img_b64);
        free(body);
        return DupPrintf("HTTP request failed (WinHTTP %lu). Endpoint: %s", err, endpoint);
    }
    DWORD status = 0, status_size = sizeof(status);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX);
    DWORD total = 0, size = 0;
    char *resp = (char *)malloc(8192);
    char *stream_acc = NULL;
    size_t stream_acc_len = 0, stream_acc_cap = 0, parse_pos = 0;
    if (!resp) {
        int should_close_request = 0;
        EnterCriticalSection(&g_req_cs);
        if (g_active_hrequest == hRequest) {
            g_active_hrequest = NULL;
            should_close_request = 1;
        }
        LeaveCriticalSection(&g_req_cs);
        if (should_close_request) WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        free(img_b64);
        free(body);
        return NULL;
    }
    resp[0] = 0;
    while (WinHttpQueryDataAvailable(hRequest, &size) && size > 0) {
        char *buf = (char *)malloc(size + 1);
        if (!buf) break;
        DWORD read = 0;
        if (WinHttpReadData(hRequest, buf, size, &read) && read > 0) {
            buf[read] = 0;
            size_t new_total = total + read + 1;
            char *new_resp = (char *)realloc(resp, new_total);
            if (!new_resp) { free(buf); break; }
            resp = new_resp;
            memcpy(resp + total, buf, read + 1);
            total += read;
            resp[total] = 0;
            if (use_stream) {
                while (parse_pos < total) {
                    char *nl = strchr(resp + parse_pos, '\n');
                    if (!nl) break;
                    size_t line_len = (size_t)(nl - (resp + parse_pos));
                    while (line_len > 0 && (resp[parse_pos + line_len - 1] == '\r' || resp[parse_pos + line_len - 1] == '\n')) line_len--;
                    if (line_len >= 5 && strncmp(resp + parse_pos, "data:", 5) == 0) {
                        size_t data_pos = parse_pos + 5;
                        while (data_pos < parse_pos + line_len && resp[data_pos] == ' ') data_pos++;
                        if (!((line_len - (data_pos - parse_pos)) == 6 && strncmp(resp + data_pos, "[DONE]", 6) == 0)) {
                            char json_line[4096];
                            size_t copy_len = line_len - (data_pos - parse_pos);
                            if (copy_len >= sizeof(json_line)) copy_len = sizeof(json_line) - 1;
                            memcpy(json_line, resp + data_pos, copy_len);
                            json_line[copy_len] = 0;
                            char *delta = ExtractDeltaContent(json_line);
                            if (delta && delta[0]) {
                                size_t dlen = strlen(delta);
                                if (stream_acc_len + dlen + 1 > stream_acc_cap) {
                                    size_t new_cap = (stream_acc_len + dlen + 1) * 2;
                                    char *n = (char *)realloc(stream_acc, new_cap);
                                    if (n) { stream_acc = n; stream_acc_cap = new_cap; }
                                }
                                if (stream_acc && stream_acc_len + dlen + 1 <= stream_acc_cap) {
                                    memcpy(stream_acc + stream_acc_len, delta, dlen);
                                    stream_acc_len += dlen;
                                    stream_acc[stream_acc_len] = 0;
                                    StreamPayload *sp = (StreamPayload *)malloc(sizeof(StreamPayload));
                                    if (sp) {
                                        sp->text = _strdup(stream_acc);
                                        sp->anchor = g_wait_anchor;
                                        sp->req_id = req_id;
                                        PostMessage(g_hwnd_main, WM_APP_STREAM, 0, (LPARAM)sp);
                                    }
                                }
                            }
                            free(delta);
                        }
                    }
                    parse_pos = (size_t)(nl - resp) + 1;
                }
            }
        }
        free(buf);
    }
    {
        int should_close_request = 0;
        EnterCriticalSection(&g_req_cs);
        if (g_active_hrequest == hRequest) {
            g_active_hrequest = NULL;
            should_close_request = 1;
        }
        LeaveCriticalSection(&g_req_cs);
        if (should_close_request) WinHttpCloseHandle(hRequest);
    }
    WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
    free(img_b64);
    free(body);
    char *content = NULL;
    if (use_stream && stream_acc && stream_acc[0]) content = _strdup(stream_acc);
    else content = ExtractProviderText(&provider, resp);
    if (!content) {
        if (status >= 400) {
            char *err = DupPrintf("HTTP %lu. Raw response: %.600s", (unsigned long)status, resp ? resp : "");
            free(resp); free(stream_acc); return err;
        }
        content = _strdup(resp && resp[0] ? resp : "LLM response had no content field.");
    }
    if (content) StripThinkBlocks(content);
    free(resp); free(stream_acc);
    return content;
}

static unsigned __stdcall RequestThread(void *param) {
    RequestPayload *req = (RequestPayload *)param;
    char *result = SendLLMRequest(req->user_text, req->region, req->image_path, req->system_prompt, req->req_id);
    if (!result) result = _strdup("LLM request failed.");
    if (req->image_path[0]) {
        DeleteFileA(req->image_path);
    }
    ResponsePayload *resp = (ResponsePayload *)malloc(sizeof(ResponsePayload));
    if (resp) {
        resp->text = result;
        resp->anchor = req->anchor;
        resp->from_ask = req->from_ask;
        resp->req_id = req->req_id;
        PostMessage(g_hwnd_main, WM_APP_RESPONSE, 0, (LPARAM)resp);
    } else free(result);
    free(req->user_text);
    free(req);
    return 0;
}

static void StartRequestEx(const char *text, const char *region, const char *image_path, POINT anchor, int from_ask, const char *system_prompt) {
    if (g_req_inflight) return;
    RequestPayload *req = (RequestPayload *)malloc(sizeof(RequestPayload));
    if (!req) return;
    req->user_text = _strdup(text ? text : "");
    strncpy(req->region, region ? region : "", sizeof(req->region) - 1);
    req->region[sizeof(req->region) - 1] = 0;
    strncpy(req->image_path, image_path ? image_path : "", sizeof(req->image_path) - 1);
    req->image_path[sizeof(req->image_path) - 1] = 0;
    req->anchor = anchor;
    req->from_ask = from_ask;
    req->req_id = (int)InterlockedIncrement(&g_request_seq);
    strncpy(req->system_prompt, (system_prompt && system_prompt[0]) ? system_prompt : g_cfg.system_prompt, sizeof(req->system_prompt) - 1);
    req->system_prompt[sizeof(req->system_prompt) - 1] = 0;
    g_active_request_id = req->req_id;
    g_req_inflight = 1;
    g_stream_has_output = 0;
    g_wait_anchor = anchor;
    g_wait_dots = 1;
    ShowWaitingOverlay(anchor);
    SetTimer(g_hwnd_main, 1, 500, NULL);
    _beginthreadex(NULL, 0, RequestThread, req, 0, NULL);
}

static void StartRequest(const char *text, const char *region, const char *image_path, POINT anchor, const char *system_prompt) {
    StartRequestEx(text, region, image_path, anchor, 0, system_prompt);
}

static void CancelCurrentRequest(const char *reason, POINT anchor) {
    HINTERNET hreq = NULL;
    EnterCriticalSection(&g_req_cs);
    hreq = g_active_hrequest;
    g_active_hrequest = NULL;
    g_active_request_id = -1;
    LeaveCriticalSection(&g_req_cs);

    if (hreq) {
        WinHttpCloseHandle(hreq);
    }

    if (g_req_inflight) {
        KillTimer(g_hwnd_main, 1);
        g_req_inflight = 0;
        g_stream_has_output = 0;
        g_wait_prefix[0] = 0;
        if (g_ask_inflight && g_hwnd_settings && IsWindow(g_hwnd_settings)) {
            g_ask_inflight = 0;
            if (GetDlgItem(g_hwnd_settings, ID_BTN_ASK)) {
                SetWindowTextA(GetDlgItem(g_hwnd_settings, ID_BTN_ASK), "Ask");
                EnableWindow(GetDlgItem(g_hwnd_settings, ID_BTN_ASK),
                             GetWindowTextLengthA(GetDlgItem(g_hwnd_settings, ID_EDIT_PROMPT)) > 0);
            }
        }
        if (reason && reason[0]) {
            ShowOverlayText(reason, anchor);
        }
    }
}

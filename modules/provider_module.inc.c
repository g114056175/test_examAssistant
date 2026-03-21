static ProviderKind DetectProviderKind(const char *endpoint, const char *api_key) {
    return ((api_key && strncmp(api_key, "AIza", 4) == 0) ||
            (endpoint && strstr(endpoint, "generativelanguage.googleapis.com") != NULL))
               ? PROVIDER_GOOGLE_GEMINI
               : PROVIDER_OPENAI_COMPAT;
}

static void BuildGoogleEndpoint(char *out, int out_size, const char *model) {
    snprintf(out, out_size,
             "https://generativelanguage.googleapis.com/v1beta/models/%s:generateContent",
             model && model[0] ? model : "gemini-2.0-flash");
}

static void ResolveProviderRequestInfo(ProviderRequestInfo *info, const char *endpoint, const char *api_key, const char *model, int stream_enabled) {
    if (!info) return;
    ZeroMemory(info, sizeof(*info));
    info->kind = DetectProviderKind(endpoint, api_key);
    info->use_stream = stream_enabled && info->kind == PROVIDER_OPENAI_COMPAT;
    strncpy(info->endpoint, endpoint ? endpoint : "", sizeof(info->endpoint) - 1);
    info->endpoint[sizeof(info->endpoint) - 1] = 0;
    NormalizeEndpoint(info->endpoint);
    if (info->kind == PROVIDER_GOOGLE_GEMINI) {
        BuildGoogleEndpoint(info->endpoint, sizeof(info->endpoint), model);
    }
}

static void NormalizeFriendlyEndpointAlias(char *s, int s_size) {
    char buf[512];
    int start = 0;
    int end;
    if (!s || s_size <= 0) return;
    strncpy(buf, s, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    end = (int)strlen(buf);
    while (buf[start] == ' ' || buf[start] == '\t' || buf[start] == '\r' || buf[start] == '\n') start++;
    while (end > start && (buf[end - 1] == ' ' || buf[end - 1] == '\t' || buf[end - 1] == '\r' || buf[end - 1] == '\n')) end--;
    buf[end] = 0;
    if (_stricmp(buf + start, "openai") == 0) {
        strncpy(s, "https://api.openai.com/v1/chat/completions", s_size - 1);
    } else if (_stricmp(buf + start, "openrouter") == 0) {
        strncpy(s, "https://openrouter.ai/api/v1/chat/completions", s_size - 1);
    } else if (_stricmp(buf + start, "google") == 0) {
        strncpy(s, "https://generativelanguage.googleapis.com", s_size - 1);
    }
    s[s_size - 1] = 0;
    NormalizeEndpoint(s);
}

static char *ExtractJsonStringByKey(const char *json, const char *key) {
    const char *p = strstr(json, key);
    char *out;
    size_t idx = 0;
    if (!p) return NULL;
    p = strchr(p, ':');
    if (!p) return NULL;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != '\"') return NULL;
    p++;
    out = (char *)malloc(4096);
    if (!out) return NULL;
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

static char *ExtractProviderText(const ProviderRequestInfo *info, const char *json) {
    if (info && info->kind == PROVIDER_GOOGLE_GEMINI) {
        return ExtractJsonStringByKey(json, "\"text\"");
    }
    return ExtractJsonStringByKey(json, "\"content\"");
}

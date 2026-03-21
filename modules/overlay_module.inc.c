static void CacheOverlayText(const char *text) {
    strncpy(g_overlay_text, text ? text : "", sizeof(g_overlay_text) - 1);
    g_overlay_text[sizeof(g_overlay_text) - 1] = 0;
    if (MultiByteToWideChar(CP_UTF8, 0, g_overlay_text, -1, g_overlay_text_w,
                            (int)(sizeof(g_overlay_text_w) / sizeof(wchar_t))) == 0) {
        MultiByteToWideChar(CP_ACP, 0, g_overlay_text, -1, g_overlay_text_w,
                            (int)(sizeof(g_overlay_text_w) / sizeof(wchar_t)));
    }
}

static LRESULT CALLBACK OverlayProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        COLORREF bgc = g_cfg.theme_light ? RGB(250, 250, 250) : RGB(20, 20, 20);
        COLORREF fgc = g_cfg.theme_light ? RGB(15, 15, 15) : RGB(230, 230, 230);
        HBRUSH bg = CreateSolidBrush(bgc);
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, fgc);
        DrawTextW(hdc, g_overlay_text_w, -1, &rc, DT_LEFT | DT_TOP | DT_WORDBREAK);
        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

static void EnsureOverlayWindow(HWND hwnd_parent) {
    if (g_hwnd_overlay) return;
    WNDCLASSA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = OverlayProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "LLMOverlayWindow";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassA(&wc);
    g_hwnd_overlay = CreateWindowExA(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
        wc.lpszClassName, "Overlay",
        WS_POPUP,
        0, 0, 10, 10,
        hwnd_parent, NULL, wc.hInstance, NULL);
    SetLayeredWindowAttributes(g_hwnd_overlay, 0, (BYTE)g_cfg.opacity, LWA_ALPHA);
}

static void ShowOverlayText(const char *text, POINT anchor) {
    CacheOverlayText(text);
    if (!g_cfg.overlay_visible) return;
    EnsureOverlayWindow(g_hwnd_main);
    HDC hdc = GetDC(g_hwnd_overlay);
    RECT rc = {0, 0, 480, 9999};
    DrawTextW(hdc, g_overlay_text_w, -1, &rc, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_CALCRECT);
    ReleaseDC(g_hwnd_overlay, hdc);
    int width = ClampInt(rc.right - rc.left + 20, 220, 480);
    int height = ClampInt(rc.bottom - rc.top + 20, 60, 600);
    int x = anchor.x + 16;
    int y = anchor.y + 16;
    int sx = GetSystemMetrics(SM_CXSCREEN);
    int sy = GetSystemMetrics(SM_CYSCREEN);
    if (x + width > sx) x = sx - width - 10;
    if (y + height > sy) y = sy - height - 10;
    SetWindowPos(g_hwnd_overlay, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(g_hwnd_overlay, NULL, TRUE);
}

static void HideOverlay(void) {
    if (g_hwnd_overlay) ShowWindow(g_hwnd_overlay, SW_HIDE);
}

static void ShowCachedOverlayAt(POINT anchor) {
    if (g_overlay_text[0]) ShowOverlayText(g_overlay_text, anchor);
    else ShowOverlayText("(No response yet)", anchor);
}

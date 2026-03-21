# Helper

Lightweight Win32 desktop helper for quick LLM queries from selected text or a screenshot region.  
It shows a non-interactive overlay near the cursor and can keep working in the background without forcing the settings window to stay open.

## What It Can Do

- Send currently selected text to an LLM with three preset prompt levels
- Select a screen region and ask about the captured image
- Show answers in a lightweight overlay window near the cursor
- Keep requests working even when overlay visibility is turned off
- Cancel an in-flight request with a hotkey
- Save settings to `config.ini`

## Provider Support

This project currently supports:

- OpenAI-compatible APIs
- OpenRouter
- Google Gemini REST API

Provider detection is automatic:

- API keys starting with `AIza` are treated as Google Gemini
- Other common keys such as `sk-...` or `sk-or-...` use the OpenAI-compatible path

### Endpoint Shortcuts

In the settings window, you can type one of these shortcut words into the `Endpoint` field:

- `openai`
- `openrouter`
- `google`

When the field loses focus, it will auto-expand to:

- `openai` -> `https://api.openai.com/v1/chat/completions`
- `openrouter` -> `https://openrouter.ai/api/v1/chat/completions`
- `google` -> `https://generativelanguage.googleapis.com`

## Build

```powershell
.\build.ps1
```

## Run

```powershell
.\llm_overlay.exe
```

## Default Hotkeys

- `Ctrl+Q` Send selected text with Prompt-1
- `Ctrl+W` Send selected text with Prompt-2
- `Ctrl+E` Send selected text with Prompt-3
- `Ctrl+R` Cancel current request
- `Ctrl+Alt+1` Select Area
- `Ctrl+Alt+2` Ask Image
- `Alt+V` Toggle overlay visibility
- `Ctrl+Alt+Up` Increase overlay opacity
- `Ctrl+Alt+Down` Decrease overlay opacity
- `Ctrl+Alt+S` Show or hide settings
- `Ctrl+Alt+X` Exit app

## Screenshot Flow

1. Press `Select Area`
2. Move the mouse to stretch the selection box
3. Press `Ask Image`

Behavior:

- Small selections are ignored
- Press `Select Area` again near the original point to cancel
- The selection overlay auto-hides after 5 seconds
- The captured image is written to a temporary file, sent to the LLM, then deleted immediately
- If the system temp folder is unavailable, the app falls back to the executable directory and still deletes the file after sending

## Overlay Behavior

- The overlay window is click-through and non-activating
- If `Overlay Visible` is off, requests still run in the background
- Latest response text is still cached even when the overlay is hidden

## Config

Settings are stored in `config.ini`.

Main sections:

- `[API]` endpoint, API key, model
- `[Prompt]` prompt_1, prompt_2, prompt_3, user_template
- `[UI]` overlay visibility, opacity, theme, stream
- `[Hotkeys]` hotkey bindings

`user_template` supports:

- `{{text}}`
- `{{region}}`

## Notes

- Google Gemini uses its native REST format automatically when an `AIza...` key is detected
- OpenAI and OpenRouter use the OpenAI-compatible chat completions format
- Image requests are currently sent as PNG

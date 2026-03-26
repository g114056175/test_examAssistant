# Helper

Download the latest `llm_overlay.exe` directly from this repository:

- [llm_overlay.exe](./llm_overlay.exe)

Lightweight Win32 desktop helper for quick LLM queries from selected text or a screenshot region.  
It shows a non-interactive overlay near the cursor and can keep working in the background without forcing the settings window to stay open.

## Developer Note

It just works.

This is a small practice project made mostly for experimenting with AI-generated code and a lightweight desktop workflow.  
The idea was simply to make something tiny that can stay in the background, quickly ask about highlighted text, and also handle screenshot-based questions without needing a big chat app UI.

It is still a rough little tool, but it works well enough for everyday testing.  
Prompt-1, Prompt-2, and Prompt-3 give different levels of reply depth, screenshot asking is supported, and most of the basic workflow is already there.  
Some parts are still pretty unpolished, and the screenshot interaction is honestly a bit awkward, but overall it is a usable small project rather than a finished product.

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

- `Ctrl+Q` Send the currently selected text with Prompt-1
- `Ctrl+W` Send the currently selected text with Prompt-2
- `Ctrl+E` Send the currently selected text with Prompt-3
- `Ctrl+R` Cancel the current in-flight request and clear the request lock
- `Ctrl+Alt+1` Start screenshot region selection, or cancel it if pressed again near the anchor point
- `Ctrl+Alt+2` Confirm the selected screenshot region and ask the LLM about that image
- `Alt+V` Show or hide the answer overlay
- `Ctrl+Alt+Up` Increase overlay opacity
- `Ctrl+Alt+Down` Decrease overlay opacity
- `Ctrl+Alt+S` Show or hide the settings window
- `Ctrl+Alt+X` Exit the app

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

## Version Notes

### v1 (baseline)

- Single-model workflow with prompt level switching and screenshot query support.
- Lightweight overlay interaction focused on quick daily usage.

### v2 (current)

- Added RAG reference data support (injects reference context into requests).
- Added multi-LLM review/merge workflow:
	- primary + side models produce candidate answers
	- merge stage integrates candidate answers into final output
	- error/timeout fallback logic for unstable providers or endpoints
- Added endpoint alias expansion in settings (`openai`, `openrouter`, `google`) including multi-LLM endpoint fields.
- Unified output style to plain text (reduced markdown artifacts in responses).

## Performance Tradeoff (Important)

- Multi-LLM can improve robustness and cross-check quality, but latency is usually higher.
- In many real tasks, one strong single model can be faster and good enough.
- Recommended strategy:
	- Use single model for speed-critical tasks.
	- Use multi-LLM for high-risk or high-importance tasks where cross-validation matters.

## Local Config Safety

- `config.ini` is local/private and should not be committed.
- This repository already ignores it via `.gitignore`.

## Suggested Git Workflow (v1 / v2 / v3)

If `v1` is already pushed and this workspace has moved to `v2`, you can still use `worktree` cleanly.

1. Mark stable points with tags.
2. Keep each major line in its own worktree directory.
3. Continue `v3` in a separate worktree without disturbing `v2`.

Example commands:

```powershell
# mark versions
git tag v1 <v1_commit>
git tag v2 HEAD

# create parallel worktrees
git worktree add ..\My_helper_v1 v1
git worktree add ..\My_helper_v2 v2

# start v3 from current main branch state
git worktree add ..\My_helper_v3 -b v3-dev
```

This gives side-by-side code and executable testing across versions while keeping one README as the shared entry point.

# Helper (v3)

Lightweight Win32 desktop helper for quick LLM queries from selected text or a screenshot region.

## Build

```powershell
.\build.ps1
```

## Run

```powershell
.\llm_overlay.exe
```

## What Changed In v3

- Removed v2 multi-LLM merge runtime path.
- Switched to single-model request flow.
- Added Model Router in Advanced settings:
	- Dynamic routes with add/remove (`+` / `-`), not fixed prompt/image slots.
	- Each route can configure: `Type`, `Endpoint`, `API Key`, `Model Name`, `Prompt`, `Hotkey`.
	- Blank routes are auto-pruned on save and when leaving Advanced.
- Basic page now keeps only core hotkeys and core fields.
- Settings persistence behavior:
	- Only `Save` writes to `config.ini`.
	- Changes without `Save` stay in runtime memory only.
- Selection box behavior changed:
	- Keeps updating while moving.
	- Auto-hides only after staying still for about 2 seconds.
- Closing the app is immediate (no unsaved prompt dialog).

## Provider Support

- OpenAI-compatible APIs
- OpenRouter
- Google Gemini REST API

Provider detection:

- API keys starting with `AIza` use Gemini request format.
- Other keys (for example `sk-...`, `sk-or-...`) use OpenAI-compatible format.

Endpoint shortcuts in settings:

- `openai` -> `https://api.openai.com/v1/chat/completions`
- `openrouter` -> `https://openrouter.ai/api/v1/chat/completions`
- `google` -> `https://generativelanguage.googleapis.com`

## Default Hotkeys (Reset Defaults)

- `Ctrl+Q` Send Prompt
- `Ctrl+Alt+1` Select Area
- `Ctrl+Alt+2` Ask Image
- `Ctrl+R` Cancel Request
- `Alt+V` Toggle Visible
- `Ctrl+Alt+Right` Opacity +
- `Ctrl+Alt+Left` Opacity -
- `Ctrl+Alt+Up` Scroll Up
- `Ctrl+Alt+Down` Scroll Down
- `Ctrl+Alt+S` Toggle Settings
- `Ctrl+Alt+X` Exit App

## Save Behavior

- No `Save`: runtime only, not persisted to disk.
- Press `Save`: writes full config to `config.ini`.

## Config Notes

- `config.ini` stores API and UI settings.
- `config.ini` should remain local/private and is ignored by git.

## Reference Data (RAG-like)

- Reference Data in Advanced settings accepts only `.txt` and `.md` sources.
- PDF/PPT files must be converted to `.txt` or `.md` first, then used as source path.
- You can select a single `.txt/.md` file or a folder that contains `.txt/.md` files.

## Known Limitation (Security)

- API keys in `config.ini` are currently not encrypted.
- If someone gets your local `config.ini`, they can read those keys.
- Planned future option: Windows DPAPI-based local encryption.

## Notes

- Overlay is click-through and non-activating.
- Image requests are sent as PNG captures.
- Reference Data support remains available in Advanced (`.txt` / `.md` only).

# Kenzitui Chrome Extension

Small Chrome extension popup to generate `.env` values for Kenzitui from current domain cookies.

## Features
- Dark themed popup UI.
- Reads cookies for the provided/current domain.
- Generates and copies to clipboard:
  - `PHAB_COOKIE=...`
  - `PHAB_USER=...`

## Install (Load Unpacked)
1. Open Chrome: `chrome://extensions`
2. Enable **Developer mode**
3. Click **Load unpacked**
4. Select this folder: `kenzitui-extension/`

## Usage
1. Open popup.
2. Confirm domain input (auto-filled from active tab).
3. Click **Copy ENV to Clipboard**.
4. Paste into `.env`.

## Notes
- Default scope is `https://p.cermati.com/*` (configured in `host_permissions`).
- If your host differs, update `host_permissions` in `manifest.json`.

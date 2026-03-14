# Kenzitui Extension

Chrome extension helper to generate Kenzitui `.env` entries from cookies on a selected host.

## Features
- Background service worker for privileged cookie API access.
- Configurable host/domain from popup input.
- Runtime host permission request (per domain).
- Dark themed popup UI.
- ENV preview textarea (review before copy).
- One-click clipboard copy for generated ENV text.

Generated output:
- `PHAB_COOKIE=...`
- `PHAB_USER=...`

## Install (Load Unpacked)
1. Open `chrome://extensions`
2. Enable **Developer mode**
3. Click **Load unpacked**
4. Select this folder: `kenzitui-extension/`

## Usage
1. Open popup.
2. Confirm or edit domain.
3. Click **Fetch + Build ENV**.
4. Approve host permission prompt (if asked).
5. ENV result appears in preview and is copied to clipboard.

## Dev
- Lint: `npm run lint`
- Test: `npm run test`

## Security Notes
- Keep generated `.env` local and untracked.
- Rotate cookie when expired or exposed.

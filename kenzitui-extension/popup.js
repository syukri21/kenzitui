import { parseCookiesJsonToEnv } from './env_parser.js';

const STRINGS = {
  ready: 'Popup ready',
  running: 'Resolving host permission and reading cookies...',
  invalidDomain: 'Invalid domain. Example: p.cermati.com',
  permissionDenied: 'Permission denied for this host. Approve permission prompt and retry.',
  noCookies: 'No cookies found for this host.',
  copied: 'ENV copied to clipboard.',
  clipboardBlocked: 'Clipboard blocked. Use manual copy from preview.',
  cookiesError: 'Cookie read failed. Check extension permissions.',
  backgroundDown: 'Background worker unavailable. Reload extension.',
};

let formEl;
let inputEl;
let messageEl;
let previewEl;
let copyBtnEl;

function setMessage(str) {
  if (!messageEl) return;
  messageEl.textContent = str;
  messageEl.hidden = false;
}

function stringToDomain(input) {
  if (!input) return null;
  try {
    const url = new URL(input.includes('://') ? input : `https://${input}`);
    return url.hostname;
  } catch {
    return null;
  }
}

function sendMessage(message) {
  return new Promise((resolve, reject) => {
    chrome.runtime.sendMessage(message, (response) => {
      if (chrome.runtime.lastError) {
        reject(new Error(chrome.runtime.lastError.message));
        return;
      }
      resolve(response);
    });
  });
}

async function ensureHostPermission(domain) {
  const res = await sendMessage({ type: 'ensureHostPermission', domain });
  return !!(res && res.ok && res.granted);
}

async function getCookies(domain) {
  const res = await sendMessage({ type: 'getCookiesForDomain', domain });
  if (!res || !res.ok) {
    const code = res?.code || 'UNKNOWN';
    throw new Error(code);
  }
  return res.cookies || [];
}

async function copyPreviewToClipboard() {
  const text = previewEl.value || '';
  if (!text.trim()) {
    setMessage('Nothing to copy yet. Fetch first.');
    return;
  }
  try {
    await navigator.clipboard.writeText(text);
    setMessage(STRINGS.copied);
  } catch {
    setMessage(STRINGS.clipboardBlocked);
  }
}

async function handleFormSubmit(event) {
  event.preventDefault();
  setMessage(STRINGS.running);

  const domain = stringToDomain(inputEl.value);
  if (!domain) {
    setMessage(STRINGS.invalidDomain);
    return;
  }

  let permitted = false;
  try {
    permitted = await ensureHostPermission(domain);
  } catch {
    setMessage(STRINGS.backgroundDown);
    return;
  }

  if (!permitted) {
    setMessage(STRINGS.permissionDenied);
    return;
  }

  try {
    const cookies = await getCookies(domain);
    if (!cookies.length) {
      previewEl.value = '';
      setMessage(STRINGS.noCookies);
      return;
    }

    const envText = parseCookiesJsonToEnv(cookies);
    previewEl.value = envText;

    try {
      await navigator.clipboard.writeText(envText);
      setMessage(STRINGS.copied);
    } catch {
      setMessage(STRINGS.clipboardBlocked);
    }
  } catch (err) {
    console.error('[popup] getCookies error:', err);
    setMessage(`${STRINGS.cookiesError} (${err.message})`);
  }
}

async function initPopupWindow() {
  formEl = document.getElementById('control-row');
  inputEl = document.getElementById('input');
  messageEl = document.getElementById('message');
  previewEl = document.getElementById('env-preview');
  copyBtnEl = document.getElementById('copy');

  if (!formEl || !inputEl || !messageEl || !previewEl || !copyBtnEl) {
    return;
  }

  formEl.addEventListener('submit', handleFormSubmit);
  copyBtnEl.addEventListener('click', copyPreviewToClipboard);

  setMessage(STRINGS.ready);

  try {
    const [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
    if (tab?.url) {
      const url = new URL(tab.url);
      inputEl.value = url.hostname;
    }
  } catch (_) {
    // ignore
  }

  inputEl.focus();
}

document.addEventListener('DOMContentLoaded', initPopupWindow);

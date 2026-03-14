function normalizeDomain(input) {
  if (!input || typeof input !== 'string') return '';
  return input.trim().replace(/^https?:\/\//i, '').replace(/\/$/, '').replace(/^\./, '');
}

function toOriginPattern(domain) {
  return `*://${domain}/*`;
}

chrome.runtime.onInstalled.addListener(() => {
  console.log('[kenzitui-extension] background installed');
});

chrome.runtime.onMessage.addListener((message, _sender, sendResponse) => {
  if (!message || typeof message !== 'object') {
    sendResponse({ ok: false, code: 'BAD_MESSAGE' });
    return false;
  }

  if (message.type === 'ping') {
    sendResponse({ ok: true, source: 'background', now: Date.now() });
    return false;
  }

  if (message.type === 'ensureHostPermission') {
    const domain = normalizeDomain(message.domain);
    if (!domain) {
      sendResponse({ ok: false, code: 'INVALID_DOMAIN' });
      return false;
    }

    const origin = toOriginPattern(domain);
    chrome.permissions.contains({ origins: [origin] }, (has) => {
      if (has) {
        sendResponse({ ok: true, granted: true });
        return;
      }
      chrome.permissions.request({ origins: [origin] }, (granted) => {
        sendResponse({ ok: granted, granted });
      });
    });
    return true;
  }

  if (message.type === 'getCookiesForDomain') {
    const domain = normalizeDomain(message.domain);
    if (!domain) {
      sendResponse({ ok: false, code: 'INVALID_DOMAIN' });
      return false;
    }

    chrome.cookies.getAll({ domain }, (cookies) => {
      if (chrome.runtime.lastError) {
        sendResponse({ ok: false, code: 'COOKIES_ERROR', detail: chrome.runtime.lastError.message });
        return;
      }
      sendResponse({ ok: true, domain, cookies: cookies || [] });
    });
    return true;
  }

  sendResponse({ ok: false, code: 'UNKNOWN_TYPE' });
  return false;
});

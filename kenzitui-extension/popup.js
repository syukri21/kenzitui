let formEl;
let inputEl;
let messageEl;

function setMessage(str) {
  if (!messageEl) {
    return;
  }
  messageEl.textContent = str;
  messageEl.hidden = false;
}

function stringToUrl(input) {
  try {
    return new URL(input);
  } catch (_) {
    // ignore
  }
  try {
    return new URL("http://" + input);
  } catch (_) {
    // ignore
  }
  return null;
}

function parseCookiesJsonToEnv(cookies) {
  if (!Array.isArray(cookies)) {
    throw new Error("cookies must be an array");
  }

  const byName = new Map();
  for (const c of cookies) {
    if (c && typeof c.name === "string" && typeof c.value === "string") {
      byName.set(c.name, c.value);
    }
  }

  const orderedNames = ["phusr", "phsid", "VouchCookie"];
  const cookieParts = [];

  for (const name of orderedNames) {
    if (byName.has(name)) {
      cookieParts.push(`${name}=${byName.get(name)}`);
      byName.delete(name);
    }
  }

  for (const [name, value] of byName.entries()) {
    cookieParts.push(`${name}=${value}`);
  }

  const phabCookie = cookieParts.join("; ");
  const phabUser = byName.get("phusr") || cookies.find((c) => c?.name === "phusr")?.value || "";

  return [
    `PHAB_COOKIE=${phabCookie}`,
    `PHAB_USER=${phabUser}`,
  ].join("\n");
}

function parseCookiesJsonTextToEnv(jsonText) {
  const parsed = JSON.parse(jsonText);
  return parseCookiesJsonToEnv(parsed);
}

async function deleteDomainCookies(domain) {
  console.log("[popup] deleteDomainCookies domain:", domain);
  try {
    const cookies = await chrome.cookies.getAll({ domain });
    console.log("[popup] cookies found:", cookies.length, cookies);

    if (cookies.length === 0) {
      return "No cookies found";
    }

    const envText = parseCookiesJsonToEnv(cookies);
    try {
      await navigator.clipboard.writeText(envText);
      return `Found ${cookies.length} cookie(s). ENV copied to clipboard.`;
    } catch (clipErr) {
      console.error("[popup] clipboard write failed:", clipErr);
      console.log("[popup] generated env fallback:\n" + envText);
      return `Found ${cookies.length} cookie(s). Clipboard failed, check popup console.`;
    }
  } catch (error) {
    console.error("[popup] deleteDomainCookies error:", error);
    return `Unexpected error: ${error.message}`;
  }
}

async function handleFormSubmit(event) {
  event.preventDefault();
  setMessage("Running...");

  const url = stringToUrl(inputEl.value);
  if (!url) {
    setMessage("Invalid URL");
    return;
  }

  const msg = await deleteDomainCookies(url.hostname);
  setMessage(msg);
}

async function initPopupWindow() {
  formEl = document.getElementById("control-row");
  inputEl = document.getElementById("input");
  messageEl = document.getElementById("message");

  if (!formEl || !inputEl || !messageEl) {
    console.error("[popup] required DOM elements not found");
    return;
  }

  formEl.addEventListener("submit", handleFormSubmit);
  console.log("[popup] initialized");
  setMessage("Popup ready");

  try {
    const [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
    if (tab?.url) {
      const url = new URL(tab.url);
      inputEl.value = url.hostname;
    }
  } catch (error) {
    console.error("[popup] tabs.query error:", error);
  }

  inputEl.focus();
}

document.addEventListener("DOMContentLoaded", () => {
  initPopupWindow();
});

export function parseCookiesJsonToEnv(cookies) {
  if (!Array.isArray(cookies)) {
    throw new Error('cookies must be an array');
  }

  const byName = new Map();
  for (const c of cookies) {
    if (c && typeof c.name === 'string' && typeof c.value === 'string') {
      byName.set(c.name, c.value);
    }
  }

  const phusr = byName.get('phusr') || '';
  const orderedNames = ['phusr', 'phsid', 'VouchCookie'];
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

  return [
    `PHAB_COOKIE=${cookieParts.join('; ')}`,
    `PHAB_USER=${phusr}`
  ].join('\n');
}

export function parseCookiesJsonTextToEnv(jsonText) {
  return parseCookiesJsonToEnv(JSON.parse(jsonText));
}

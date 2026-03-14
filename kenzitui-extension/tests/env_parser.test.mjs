import assert from 'node:assert/strict';
import { parseCookiesJsonToEnv, parseCookiesJsonTextToEnv } from '../env_parser.js';

const sample = [
  { name: 'phusr', value: 'syukri.khairi' },
  { name: 'phsid', value: 'abc123' },
  { name: 'VouchCookie', value: 'tokenxyz' }
];

const env = parseCookiesJsonToEnv(sample);
assert.equal(
  env,
  'PHAB_COOKIE=phusr=syukri.khairi; phsid=abc123; VouchCookie=tokenxyz\nPHAB_USER=syukri.khairi'
);

const textEnv = parseCookiesJsonTextToEnv(JSON.stringify(sample));
assert.equal(textEnv, env);

assert.throws(() => parseCookiesJsonToEnv({}), /array/);

console.log('env_parser tests passed');

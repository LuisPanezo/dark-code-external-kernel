import {test} from 'node:test';
import assert from 'node:assert/strict';
import {handle, AuthState} from './worker.mjs';

const device = 'aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee';
function setup() {
  const objects = new Map();
  const env = {SAO_APP_NAME: 'Test app', SAO_OWNERID: 'test-owner', SAO_SECRET: 'test-secret',
    AUTH_STATE: {idFromName: key => key, get(key) {
      if (!objects.has(key)) {
        const values = new Map();
        let queue = Promise.resolve();
        const storage = {
          async get(key) { return structuredClone(values.get(key)); },
          async put(key, value) { values.set(key, structuredClone(value)); },
          async deleteAll() { values.clear(); }, async setAlarm() {},
          transaction(callback) {
            const result = queue.then(() => callback(storage));
            queue = result.catch(() => {}); return result;
          }
        };
        const object = new AuthState({storage});
        objects.set(key, {fetch: (url, init) => object.fetch(new Request(url, init))});
      }
      return objects.get(key);
    }}};
  return env;
}
function req(path = '/v1/login', body = {}, token, ip = '192.0.2.1') {
  return new Request('https://auth.example' + path, {method: 'POST', headers: {
    'Content-Type': 'application/json', 'CF-Connecting-IP': ip,
    ...(token ? {Authorization: 'Bearer ' + token} : {})
  }, body: JSON.stringify({username: 'alice', password: ' pass🔐word ', device_id: device, ...body})});
}
const ok = async url => Response.json(url.endsWith('/auth') ?
  {success: true, session_token: 'provider-token'} : {success: true});

test('provider redirects are rejected without forwarding credentials', async () => {
  let calls = 0;
  const response = await handle(req(), setup(), async (url, options) => {
    calls++;
    assert.equal(options.redirect, 'manual');
    return new Response(null, {status: 302, headers: {Location: 'https://other.example'}});
  });
  assert.equal(response.status, 503);
  assert.equal(calls, 1);
});

test('device binding errors stay distinct from credentials and server configuration errors', async () => {
  for (const [upstreamError, status, error] of [
    ['hwid_mismatch',403,'device_mismatch'],
    ['subscription_expired',403,'subscription_expired'],
    ['invalid_application',503,'not_configured'],
    ['invalid_credentials',401,'access_denied']
  ]) {
    const response = await handle(req(), setup(), async()=>Response.json({success:false,error:upstreamError},{status:400}));
    assert.equal(response.status,status);
    assert.deepEqual(await response.json(),{error});
  }
});

test('login requires provider acceptance AND session validation; no provider token/secret leaks', async () => {
  const calls = [];
  const upstream = async (url, options) => {
    calls.push(url.endsWith('/auth') ? Object.fromEntries(new URLSearchParams(options.body)) : JSON.parse(options.body));
    return ok(url);
  };
  const response = await handle(req('/v1/login', {name: 'attacker', secret: 'attacker'}), setup(), upstream);
  assert.equal(response.status, 200);
  const data = await response.json();
  assert.match(data.session_token, /^[a-f0-9]{64}$/);
  assert.equal(calls.length, 2);
  assert.equal(calls[0].name, 'Test app');
  assert.equal(calls[0].type, 'login');
  assert.equal(calls[0].auth_type, undefined);
  assert.equal(calls[0].password, ' pass🔐word ');
  assert.equal(calls[0].secret, 'test-secret');
  assert.ok(!JSON.stringify(data).includes('provider-token'));
  assert.ok(!JSON.stringify(data).includes('test-secret'));
  assert.equal(response.headers.get('Cache-Control'), 'no-store');
});
test('provider errors, malformed JSON and network failures fail closed', async () => {
  for (const upstream of [async () => Response.json({success: false}),
    async () => Response.json({success: 'true', session_token: 'x'}),
    async () => new Response('<html>offline</html>'),
    async () => { throw new Error('sensitive upstream failure'); }]) {
    const response = await handle(req(), setup(), upstream);
    assert.ok(response.status >= 400);
    assert.ok(!(await response.text()).includes('sensitive'));
  }
});
test('session check rejection prevents creation of a session', async () => {
  const response = await handle(req(), setup(), async url => url.endsWith('/auth') ? ok(url) :
    Response.json({success: true, authenticated: false}));
  assert.equal(response.status, 401);
});
test('tokens are device-bound and random invalid tokens cannot authenticate', async () => {
  const env = setup();
  const {session_token} = await (await handle(req(), env, ok)).json();
  assert.equal((await handle(req('/v1/session', {}, session_token), env, ok)).status, 200);
  assert.equal((await handle(req('/v1/session', {device_id: 'bbbbbbbb-bbbb-4ccc-8ddd-eeeeeeeeeeee'}, session_token), env, ok)).status, 401);
  assert.equal((await handle(req('/v1/session', {}, '0'.repeat(64)), env, ok)).status, 401);
});
test('logout revokes gateway token even when provider is unavailable', async () => {
  const env = setup();
  const {session_token} = await (await handle(req(), env, ok)).json();
  const response = await handle(req('/v1/logout', {}, session_token), env, async () => {throw Error();});
  assert.equal(response.status, 200);
  assert.equal((await response.json()).upstream_revoked, false);
  assert.equal((await handle(req('/v1/session', {}, session_token), env, ok)).status, 401);
});
test('expired sessions are rejected even before cleanup alarm', async () => {
  const env = setup(), now = Date.now;
  const {session_token} = await (await handle(req(), env, ok)).json();
  try {
    const future = now() + 16 * 60 * 1000;
    Date.now = () => future;
    assert.equal((await handle(req('/v1/session', {}, session_token), env, ok)).status, 401);
  } finally { Date.now = now; }
});
test('account rate limit persists across requests and counts concurrent attempts', async () => {
  const env = setup();
  const results = await Promise.all(Array.from({length: 8}, () => handle(req(), env,
    async () => Response.json({success: false}))));
  assert.equal(results.filter(r => r.status === 401).length, 5);
  assert.equal(results.filter(r => r.status === 429).length, 3);
});
test('missing configuration and oversized bodies never call provider', async () => {
  let calls = 0;
  const upstream = async () => {calls++; return ok('');};
  assert.equal((await handle(req(), {}, upstream)).status, 503);
  assert.equal((await handle(req('/v1/login', {password: 'x'.repeat(10000)}), setup(), upstream)).status, 413);
  assert.equal(calls, 0);
});
test('passwords are not truncated; invalid input is rejected', async () => {
  for (const body of [{password: ''}, {password: 'x'.repeat(1025)}, {username: ''}, {device_id: '../bad'}])
    assert.equal((await handle(req('/v1/login', body), setup(), ok)).status, 400);
});

test('verified info creates a five-minute device-bound gateway session with local revocation', async () => {
  const env = setup();
  let calls = 0;
  const provider = async () => { calls++; return Response.json({success: true, info: {
    username: 'alice', hwid: device, expiry: String(Math.floor(Date.now()/1000)+3600)
  }}); };
  const response = await handle(req(), env, provider);
  assert.equal(response.status, 200);
  const login = await response.json();
  assert.ok(login.expires_at <= Date.now() + 300000);
  assert.ok(login.subscription.expires_at > login.expires_at);
  assert.equal((await handle(req('/v1/session', {}, login.session_token), env, provider)).status, 200);
  assert.equal((await handle(req('/v1/session', {device_id:'bbbbbbbb-bbbb-4ccc-8ddd-eeeeeeeeeeee'}, login.session_token), env, provider)).status, 401);
  assert.equal((await handle(req('/v1/logout', {}, login.session_token), env, provider)).status, 200);
  assert.equal((await handle(req('/v1/session', {}, login.session_token), env, provider)).status, 401);
  assert.equal(calls, 1);
});

test('missing, mismatched or expired provider info cannot create a gateway session', async () => {
  const valid = {username:'alice', hwid:device, expiry:Math.floor(Date.now()/1000)+3600};
  for (const info of [null, {...valid,username:'other'}, {...valid,hwid:'other'},
      {...valid,expiry:undefined}, {...valid,expiry:'invalid'}, {...valid,expiry:1},
      {...valid,expiry:1e308}, {...valid,expiry:123.5}]) {
    const result = await handle(req(), setup(), async()=>Response.json({success:true,info}));
    assert.ok(result.status >= 400);
  }
});

test('gateway session expires after five minutes even when the subscription remains valid', async () => {
  const env = setup(), now = Date.now;
  const provider = async () => Response.json({success: true, info: {
    username:'alice', hwid:device, expiry:Math.floor(now()/1000)+3600
  }});
  const login = await (await handle(req(), env, provider)).json();
  try {
    Date.now = () => now() + 301000;
    const response = await handle(req('/v1/session', {}, login.session_token), env, provider);
    assert.equal(response.status, 401);
  } finally { Date.now = now; }
});

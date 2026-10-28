// Standalone authentication only. No privileged commands or downloadable payloads.
const SESSION_MS = 15 * 60 * 1000;
class Failure extends Error {
  constructor(code, status) { super(code); this.code = code; this.status = status; }
}
function json(body, status = 200) {
  return new Response(JSON.stringify(body), { status, headers: {
    'Content-Type': 'application/json', 'Cache-Control': 'no-store',
    'X-Content-Type-Options': 'nosniff', ...(status === 429 ? {'Retry-After': '60'} : {})
  }});
}
export async function readJson(response, max = 8192) {
  if (!response.body) throw new Failure('invalid_request', 400);
  const reader = response.body.getReader();
  let bytes = 0, parts = [];
  try {
    while (true) {
      const {done, value} = await reader.read();
      if (done) break;
      bytes += value.byteLength;
      if (bytes > max) { await reader.cancel(); throw new Failure('too_large', 413); }
      parts.push(value);
    }
    const all = new Uint8Array(bytes);
    let offset = 0;
    for (const part of parts) { all.set(part, offset); offset += part.length; }
    const result = JSON.parse(new TextDecoder('utf-8', {fatal: true}).decode(all));
    if (!result || typeof result !== 'object' || Array.isArray(result)) throw new Error();
    return result;
  } catch (e) {
    if (e instanceof Failure) throw e;
    throw new Failure('invalid_request', 400);
  } finally { reader.releaseLock(); }
}
async function hash(value) {
  return [...new Uint8Array(await crypto.subtle.digest('SHA-256', new TextEncoder().encode(value)))]
    .map(n => n.toString(16).padStart(2, '0')).join('');
}
function token() {
  return [...crypto.getRandomValues(new Uint8Array(32))].map(n => n.toString(16).padStart(2, '0')).join('');
}
async function state(env, key, action, body = {}) {
  const stub = env.AUTH_STATE.get(env.AUTH_STATE.idFromName(key));
  const response = await stub.fetch('https://state/' + action, {method: 'POST', body: JSON.stringify(body)});
  if (!response.ok) throw new Failure('unavailable', 503);
  return response.json();
}
async function rate(env, key, max) {
  const result = await state(env, 'rate:' + await hash(key), 'limit', {max});
  if (!result.allowed) throw new Failure('rate_limited', 429);
}
async function sao(path, data, env, upstream) {
  try {
    const response = await upstream('https://saoauth.com/api/client/' + path, {
      method: 'POST', redirect: 'error', signal: AbortSignal.timeout(8000),
      headers: {'Content-Type': 'application/json', 'Accept': 'application/json'},
      body: JSON.stringify({...data, name: env.SAO_APP_NAME, ownerid: env.SAO_OWNERID,
        secret: env.SAO_SECRET, version: env.SAO_VERSION || '1.0'})
    });
    if (response.status === 429) throw new Failure('rate_limited', 429);
    if (response.status >= 500) throw new Failure('unavailable', 503);
    let result;
    try { result = await readJson(response, 65536); }
    catch { throw new Failure('unavailable', 503); }
    if (!response.ok || result.success !== true || result.authenticated === false)
      throw new Failure('access_denied', 401);
    return result;
  } catch (e) {
    if (e instanceof Failure) throw e;
    throw new Failure('unavailable', 503);
  }
}
export async function handle(request, env, upstream = fetch) {
  try {
    const path = new URL(request.url).pathname;
    if (path === '/health' && request.method === 'GET') return json({status: 'ok'});
    if (!['/v1/login', '/v1/session', '/v1/logout'].includes(path)) return json({error: 'not_found'}, 404);
    if (request.method !== 'POST') return json({error: 'method_not_allowed'}, 405);
    if (new URL(request.url).protocol !== 'https:' || request.headers.has('Origin'))
      throw new Failure('invalid_request', 400);
    if (!env.SAO_APP_NAME || !env.SAO_OWNERID || !env.SAO_SECRET || !env.AUTH_STATE)
      throw new Failure('not_configured', 503);
    // CF-Connecting-IP is supplied by Cloudflare at the edge, not trusted from JSON.
    const ip = request.headers.get('CF-Connecting-IP');
    if (!ip) throw new Failure('unavailable', 503);
    await rate(env, 'ip:' + ip, path === '/v1/login' ? 20 : 120);
    if (!(request.headers.get('Content-Type') || '').startsWith('application/json'))
      throw new Failure('invalid_request', 400);
    const body = await readJson(request);
    if (typeof body.device_id !== 'string' || !/^[a-f0-9-]{36}$/.test(body.device_id))
      throw new Failure('invalid_request', 400);
    const deviceHash = await hash(body.device_id);
    if (path === '/v1/login') {
      if (typeof body.username !== 'string' || !body.username.trim() || body.username.length > 128 ||
          typeof body.password !== 'string' || !body.password.length || body.password.length > 1024)
        throw new Failure('invalid_request', 400);
      const username = body.username.trim();
      await rate(env, 'user:' + username.toLowerCase(), 5);
      const login = await sao('auth', {auth_type: 'user_login', username, password: body.password,
        hwid: body.device_id}, env, upstream);
      if (typeof login.session_token !== 'string' || !login.session_token || login.session_token.length > 4096)
        throw new Failure('unavailable', 503);
      await sao('session/check', {session_token: login.session_token, hwid: body.device_id}, env, upstream);
      const sessionToken = token(), expiresAt = Date.now() + SESSION_MS;
      await state(env, 'session:' + await hash(sessionToken), 'put', {
        upstreamToken: login.session_token, deviceHash, username, expiresAt
      });
      return json({session_token: sessionToken, username, expires_at: expiresAt});
    }
    const authorization = request.headers.get('Authorization') || '';
    if (!/^Bearer [a-f0-9]{64}$/.test(authorization)) throw new Failure('access_denied', 401);
    const key = 'session:' + await hash(authorization.slice(7));
    const record = await state(env, key, 'get');
    if (!record || record.expiresAt <= Date.now() || record.deviceHash !== deviceHash)
      throw new Failure('access_denied', 401);
    if (path === '/v1/logout') {
      await state(env, key, 'delete'); // Local revocation is authoritative even if upstream is down.
      let upstreamRevoked = true;
      try { await sao('session/logout', {session_token: record.upstreamToken}, env, upstream); }
      catch { upstreamRevoked = false; }
      return json({success: true, upstream_revoked: upstreamRevoked});
    }
    try { await sao('session/check', {session_token: record.upstreamToken, hwid: body.device_id}, env, upstream); }
    catch (e) { if (e.status === 401) await state(env, key, 'delete'); throw e; }
    return json({success: true, username: record.username, expires_at: record.expiresAt});
  } catch (e) {
    return json({error: e instanceof Failure ? e.code : 'unavailable'}, e instanceof Failure ? e.status : 503);
  }
}
export default { fetch(request, env) { return handle(request, env); } };

/** Private Durable Object: counters and sessions survive Worker restarts. */
export class AuthState {
  constructor(ctx) { this.storage = ctx.storage; }
  async fetch(request) {
    const action = new URL(request.url).pathname;
    if (action === '/get') return json(await this.storage.get('value') || null);
    if (action === '/delete') { await this.storage.deleteAll(); return json({ok: true}); }
    const body = await request.json();
    if (action === '/put') {
      await this.storage.put('value', body);
      await this.storage.setAlarm(body.expiresAt);
      return json({ok: true});
    }
    if (action === '/limit') {
      const result = await this.storage.transaction(async txn => {
        let value = await txn.get('value');
        if (!value || value.expiresAt <= Date.now()) value = {count: 0, expiresAt: Date.now() + 60000};
        const allowed = value.count < body.max;
        if (allowed) value.count++;
        await txn.put('value', value);
        await txn.setAlarm(value.expiresAt);
        return {allowed};
      });
      return json(result);
    }
    return json({error: 'not_found'}, 404);
  }
  async alarm() {
    const value = await this.storage.get('value');
    if (!value || value.expiresAt <= Date.now()) await this.storage.deleteAll();
    else await this.storage.setAlarm(value.expiresAt);
  }
}

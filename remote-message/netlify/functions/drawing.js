import { getStore } from "@netlify/blobs";

const SECRET = process.env.MESSAGE_SECRET;

function isAuthorized(req, url) {
  const keyParam = url.searchParams.get("key");
  if (keyParam && keyParam === SECRET) return true;
  const cookieHeader = req.headers.get("cookie") || "";
  const match = cookieHeader.match(/(?:^|;\s*)session=([^;]+)/);
  return !!(match && match[1] === SECRET);
}

// 128x64 1-bit image, row-major MSB-first -> 1024 bytes per frame.
const EXPECTED_BYTES = (128 * 64) / 8;
const MAX_FRAMES = 24;

export default async (req) => {
  const url = new URL(req.url);
  if (!SECRET || !isAuthorized(req, url)) {
    return new Response("Unauthorized", { status: 401 });
  }

  const store = getStore("deskbot-messages");

  if (req.method === "GET") {
    const data = await store.get("drawing", { type: "json" });
    if (!data) return Response.json({ active: false }, { headers: { "Cache-Control": "no-store" } });
    const remainingMs = data.expiresAt - Date.now();
    if (remainingMs <= 0) return Response.json({ active: false }, { headers: { "Cache-Control": "no-store" } });
    return Response.json({ active: true, frames: data.frames, delayMs: data.delayMs, remainingMs }, { headers: { "Cache-Control": "no-store" } });
  }

  if (req.method === "POST") {
    const action = url.searchParams.get("action");
    if (action === "clear") {
      await store.delete("drawing");
      return Response.json({ ok: true }, { headers: { "Cache-Control": "no-store" } });
    }

    let body;
    try {
      body = await req.json();
    } catch {
      return new Response("Invalid JSON body", { status: 400 });
    }

    const frames = Array.isArray(body.frames) ? body.frames : [];
    if (frames.length === 0 || frames.length > MAX_FRAMES) {
      return new Response(`frames must be an array of 1-${MAX_FRAMES} items`, { status: 400 });
    }
    for (const f of frames) {
      let len;
      try {
        len = Buffer.from(String(f), "base64").length;
      } catch {
        return new Response("Invalid frame encoding", { status: 400 });
      }
      if (len !== EXPECTED_BYTES) {
        return new Response(`each frame must decode to exactly ${EXPECTED_BYTES} bytes`, { status: 400 });
      }
    }

    let delayMs = Number(body.delayMs) || 200;
    delayMs = Math.min(5000, Math.max(20, delayMs));

    const durationMinutes = Number(body.durationMinutes) || 10;
    const expiresAt = Date.now() + durationMinutes * 60000;
    await store.setJSON("drawing", { frames, delayMs, expiresAt });
    return Response.json({ ok: true }, { headers: { "Cache-Control": "no-store" } });
  }

  return new Response("Method not allowed", { status: 405 });
};

export const config = { path: "/api/drawing" };

import { getStore } from "@netlify/blobs";

const SECRET = process.env.MESSAGE_SECRET;

function isAuthorized(req, url) {
  const keyParam = url.searchParams.get("key");
  if (keyParam && keyParam === SECRET) return true;
  const cookieHeader = req.headers.get("cookie") || "";
  const match = cookieHeader.match(/(?:^|;\s*)session=([^;]+)/);
  return !!(match && match[1] === SECRET);
}

const EXPECTED_BYTES = (128 * 64) / 8;
const MAX_FRAMES = 24;

export default async (req, context) => {
  const url = new URL(req.url);
  if (!SECRET || !isAuthorized(req, url)) {
    return new Response("Unauthorized", { status: 401 });
  }
  const id = context.params.id;
  const store = getStore("deskbot-slides");

  if (req.method === "GET") {
    const slide = await store.get(id, { type: "json" });
    if (!slide) return new Response("Not found", { status: 404 });
    return Response.json(slide);
  }

  if (req.method === "PUT") {
    const existing = await store.get(id, { type: "json" });
    if (!existing) return new Response("Not found", { status: 404 });
    let body;
    try {
      body = await req.json();
    } catch {
      return new Response("Invalid JSON body", { status: 400 });
    }

    const name = body.name !== undefined ? String(body.name).slice(0, 60) : existing.name;
    const frames = Array.isArray(body.frames) ? body.frames : existing.frames;
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
    let delayMs = body.delayMs !== undefined ? Number(body.delayMs) : existing.delayMs;
    delayMs = Math.min(5000, Math.max(20, delayMs || 200));
    const longPressAlternateSlideId = body.longPressAlternateSlideId !== undefined
      ? (body.longPressAlternateSlideId || null) : existing.longPressAlternateSlideId;

    const updated = { ...existing, name, frames, delayMs, longPressAlternateSlideId };
    await store.setJSON(id, updated);
    return Response.json(updated);
  }

  if (req.method === "DELETE") {
    await store.delete(id);
    const rotationStore = getStore("deskbot-messages");
    const rotation = await rotationStore.get("rotation", { type: "json" });
    if (rotation && Array.isArray(rotation.entries)) {
      const filtered = rotation.entries.filter((e) => e.slideId !== id);
      if (filtered.length !== rotation.entries.length) {
        await rotationStore.setJSON("rotation", { entries: filtered });
        await rotationStore.setJSON("bundleVersion", { version: Date.now() });
      }
    }
    return Response.json({ ok: true }, { headers: { "Cache-Control": "no-store" } });
  }

  return new Response("Method not allowed", { status: 405 });
};

export const config = { path: "/api/slides/:id" };

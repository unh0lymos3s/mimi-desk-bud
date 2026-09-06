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

function randomId() {
  return "custom-" + Date.now() + "-" + Math.random().toString(36).slice(2, 8);
}

export default async (req) => {
  const url = new URL(req.url);
  if (!SECRET || !isAuthorized(req, url)) {
    return new Response("Unauthorized", { status: 401 });
  }
  const store = getStore("deskbot-slides");

  if (req.method === "GET") {
    const { blobs } = await store.list();
    const results = [];
    for (const b of blobs) {
      const slide = await store.get(b.key, { type: "json" });
      if (slide) {
        results.push({
          id: slide.id,
          name: slide.name,
          delayMs: slide.delayMs,
          frameCount: slide.frames.length,
          previewFrame: slide.frames[0],
          longPressAlternateSlideId: slide.longPressAlternateSlideId,
          createdAt: slide.createdAt,
        });
      }
    }
    return Response.json({ slides: results }, { headers: { "Cache-Control": "no-store" } });
  }

  if (req.method === "POST") {
    let body;
    try {
      body = await req.json();
    } catch {
      return new Response("Invalid JSON body", { status: 400 });
    }

    const name = String(body.name || "Untitled").slice(0, 60);
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
    const longPressAlternateSlideId = body.longPressAlternateSlideId || null;

    const id = randomId();
    const slide = { id, name, frames, delayMs, longPressAlternateSlideId, createdAt: Date.now() };
    await store.setJSON(id, slide);
    return Response.json({ id }, { headers: { "Cache-Control": "no-store" } });
  }

  return new Response("Method not allowed", { status: 405 });
};

export const config = { path: "/api/slides" };

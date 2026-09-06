import { getStore } from "@netlify/blobs";

const SECRET = process.env.MESSAGE_SECRET;

function isAuthorized(req, url) {
  const keyParam = url.searchParams.get("key");
  if (keyParam && keyParam === SECRET) return true;
  const cookieHeader = req.headers.get("cookie") || "";
  const match = cookieHeader.match(/(?:^|;\s*)session=([^;]+)/);
  return !!(match && match[1] === SECRET);
}

const BUILTIN_IDS = ["alive", "love", "angry", "sad", "dizzy", "clock", "weather", "spotify"];

function defaultRotation() {
  return { entries: BUILTIN_IDS.map((slideId) => ({ slideId, isBuiltin: true, longPressEnabled: true })) };
}

export default async (req) => {
  const url = new URL(req.url);
  if (!SECRET || !isAuthorized(req, url)) {
    return new Response("Unauthorized", { status: 401 });
  }
  const store = getStore("deskbot-messages");

  if (req.method === "GET") {
    const rotation = await store.get("rotation", { type: "json" });
    return Response.json(rotation || defaultRotation(), { headers: { "Cache-Control": "no-store" } });
  }

  if (req.method === "PUT") {
    let body;
    try {
      body = await req.json();
    } catch {
      return new Response("Invalid JSON body", { status: 400 });
    }
    const entries = Array.isArray(body.entries) ? body.entries : [];
    if (entries.length === 0) {
      return new Response("entries must be a non-empty array", { status: 400 });
    }

    const slideStore = getStore("deskbot-slides");
    for (const e of entries) {
      if (!e.slideId) return new Response("each entry needs a slideId", { status: 400 });
      const isBuiltin = BUILTIN_IDS.includes(e.slideId);
      if (!isBuiltin) {
        const exists = await slideStore.get(e.slideId, { type: "json" });
        if (!exists) return new Response(`unknown slideId: ${e.slideId}`, { status: 400 });
      }
    }

    const normalized = entries.map((e) => ({
      slideId: e.slideId,
      isBuiltin: BUILTIN_IDS.includes(e.slideId),
      longPressEnabled: e.longPressEnabled !== false,
    }));

    await store.setJSON("rotation", { entries: normalized });
    await store.setJSON("bundleVersion", { version: Date.now() });
    return Response.json({ ok: true }, { headers: { "Cache-Control": "no-store" } });
  }

  return new Response("Method not allowed", { status: 405 });
};

export const config = { path: "/api/rotation" };

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
  const versionDoc = await store.get("bundleVersion", { type: "json" });
  const version = versionDoc ? versionDoc.version : 0;

  if (url.searchParams.get("versionOnly") === "1") {
    return Response.json({ version }, { headers: { "Cache-Control": "no-store" } });
  }

  const rotation = (await store.get("rotation", { type: "json" })) || defaultRotation();
  const slideStore = getStore("deskbot-slides");

  const neededIds = new Set();
  for (const e of rotation.entries) {
    if (!BUILTIN_IDS.includes(e.slideId)) neededIds.add(e.slideId);
  }

  const slides = {};
  for (const id of neededIds) {
    const slide = await slideStore.get(id, { type: "json" });
    if (slide) {
      slides[id] = {
        name: slide.name,
        frames: slide.frames,
        delayMs: slide.delayMs,
        longPressAlternateSlideId: slide.longPressAlternateSlideId,
      };
      if (slide.longPressAlternateSlideId && !BUILTIN_IDS.includes(slide.longPressAlternateSlideId)) {
        neededIds.add(slide.longPressAlternateSlideId);
      }
    }
  }

  return Response.json({ version, rotation, slides }, { headers: { "Cache-Control": "no-store" } });
};

export const config = { path: "/api/bundle" };

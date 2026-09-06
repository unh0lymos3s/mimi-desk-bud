import { getStore } from "@netlify/blobs";

const SECRET = process.env.MESSAGE_SECRET;

function isAuthorized(req, url) {
  const keyParam = url.searchParams.get("key");
  if (keyParam && keyParam === SECRET) return true;
  const cookieHeader = req.headers.get("cookie") || "";
  const match = cookieHeader.match(/(?:^|;\s*)session=([^;]+)/);
  return !!(match && match[1] === SECRET);
}

export default async (req) => {
  const url = new URL(req.url);
  if (!SECRET || !isAuthorized(req, url)) {
    return new Response("Unauthorized", { status: 401 });
  }

  const store = getStore("deskbot-messages");

  if (req.method === "GET") {
    const data = await store.get("current", { type: "json" });
    if (!data) return Response.json({ active: false }, { headers: { "Cache-Control": "no-store" } });
    const remainingMs = data.expiresAt - Date.now();
    if (remainingMs <= 0) return Response.json({ active: false }, { headers: { "Cache-Control": "no-store" } });
    return Response.json({ active: true, text: data.text, remainingMs }, { headers: { "Cache-Control": "no-store" } });
  }

  if (req.method === "POST") {
    const action = url.searchParams.get("action");
    if (action === "clear") {
      await store.delete("current");
      return Response.json({ ok: true }, { headers: { "Cache-Control": "no-store" } });
    }

    let body;
    try {
      body = await req.json();
    } catch {
      return new Response("Invalid JSON body", { status: 400 });
    }
    const text = String(body.text || "").slice(0, 60);
    if (!text) {
      return new Response("Missing text", { status: 400 });
    }
    const durationMinutes = Number(body.durationMinutes) || 10;
    const expiresAt = Date.now() + durationMinutes * 60000;
    await store.setJSON("current", { text, expiresAt });
    return Response.json({ ok: true }, { headers: { "Cache-Control": "no-store" } });
  }

  return new Response("Method not allowed", { status: 405 });
};

export const config = { path: "/api/message" };

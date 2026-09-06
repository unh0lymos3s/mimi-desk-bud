const SECRET = process.env.MESSAGE_SECRET;
const GIPHY_API_KEY = process.env.GIPHY_API_KEY;

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
  if (!GIPHY_API_KEY) {
    return new Response("Giphy search is not configured (missing GIPHY_API_KEY)", { status: 500 });
  }

  const q = url.searchParams.get("q") || "";
  if (!q) return Response.json({ data: [] }, { headers: { "Cache-Control": "no-store" } });

  const giphyUrl = `https://api.giphy.com/v1/gifs/search?api_key=${GIPHY_API_KEY}&q=${encodeURIComponent(q)}&limit=9&rating=g`;
  const res = await fetch(giphyUrl);
  if (!res.ok) {
    return new Response("Giphy search failed", { status: 502 });
  }
  const data = await res.json();
  const results = (data.data || []).map((g) => ({
    title: g.title,
    thumbUrl: g.images.fixed_height_small.url,
    originalUrl: g.images.original.url,
  }));
  return Response.json({ results }, { headers: { "Cache-Control": "no-store" } });
};

export const config = { path: "/api/giphy" };

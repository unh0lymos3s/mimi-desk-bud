const SECRET = process.env.MESSAGE_SECRET;
const PIN = process.env.MESSAGE_PIN;

export default async (req) => {
  if (req.method !== "POST") {
    return new Response("Method not allowed", { status: 405 });
  }

  let body;
  try {
    body = await req.json();
  } catch {
    return new Response("Invalid JSON body", { status: 400 });
  }

  const pin = String(body.pin || "");
  if (!PIN || pin !== PIN) {
    return new Response("Wrong PIN", { status: 401 });
  }

  const maxAgeSeconds = 60 * 60 * 24 * 90; // 90 days
  return new Response(JSON.stringify({ ok: true }), {
    status: 200,
    headers: {
      "Content-Type": "application/json",
      "Set-Cookie": `session=${SECRET}; Path=/; HttpOnly; Secure; SameSite=Lax; Max-Age=${maxAgeSeconds}`,
    },
  });
};

export const config = { path: "/api/login" };

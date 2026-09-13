#ifndef DKC2_DESKTOP_RECONSTRUCT_SHADER_H
#define DKC2_DESKTOP_RECONSTRUCT_SHADER_H

/* Shared by the WGL and SDL presenters. See third_party/dkc3_menu/README.md. */
/*
 * Reconstruct: an experimental single-pass upscaler for pixel art on a
 * high-density display, written for the GLSL 1.20 that the legacy OpenGL
 * 2.1 context provides.
 *
 * Every output fragment locates its source texel and fetches the 21-texel
 * xBR footprint (5x5 without corners) with nearest sampling. From those it
 * derives, per mode:
 *
 *   mode 0  sharp boundaries: inside a texel the color is flat; within one
 *           output pixel of a texel edge it blends with the neighbor, so a
 *           fractional scale (the 16-inch panel shows a 342-pixel frame at
 *           about ten times) has neither uneven pixel widths nor blur.
 *   mode 1  + dither decoding: a 2x2 checkerboard or a one-texel line
 *           dither between two colors is what SNES artists used for a
 *           mid-tone a CRT would blur into; the texel takes that average.
 *   mode 2  + diagonal edges: the xBR level-1 corner test decides whether
 *           a texel corner belongs to a diagonal edge, and that corner
 *           takes the neighbor's color along an antialiased 45-degree
 *           line, evaluated analytically at the fragment rather than on a
 *           fixed 2x or 3x grid.
 *   mode 3  + level-2 slopes: 2:1 and 1:2 edge lines where the corner
 *           test says the edge continues.
 *   mode 4  + level-3 slopes: 3:1 and 1:3 lines where it continues further.
 *
 * strength scales the edge blend (1 = full); softness widens every
 * transition band from one output pixel to up to three; shading blends the
 * flat interior of a texel toward a bilinear gradient wherever its
 * neighbors are close in color (shading bands, not outlines). Colors are
 * the frame after the selected screen model, so CRT/Composite/Trinitron
 * still apply.
 */
static const char kReconstructVertexSource[] =
    "#version 120\n"
    "varying vec2 uv;\n"
    "void main() {\n"
    "  uv = gl_MultiTexCoord0.xy;\n"
    "  gl_Position = gl_Vertex;\n"
    "}\n";

static const char kReconstructFragmentSource[] =
    "#version 120\n"
    "uniform sampler2D source;\n"
    "uniform vec2 source_size;\n"
    "uniform vec2 output_size;\n"
    "uniform int mode;\n"
    "uniform float strength;\n"
    "uniform float softness;\n"
    "uniform float shading;\n"
    "varying vec2 uv;\n"
    "vec3 tx(vec2 t) { return texture2D(source, (t + 0.5) / source_size).rgb; }\n"
    "float df(vec3 a, vec3 b) {\n"
    "  vec3 d = abs(a - b);\n"
    "  return dot(d, vec3(0.299, 0.587, 0.114)) * 2.0 +\n"
    "         abs((a.r - b.r) - (a.b - b.b)) * 0.5;\n"
    "}\n"
    "bool eq(vec3 a, vec3 b) { return df(a, b) < 0.004; }\n"
    "vec3 decode(vec3 c, vec3 n, vec3 s, vec3 w, vec3 e,\n"
    "            vec3 nw, vec3 ne, vec3 sw, vec3 se) {\n"
    "  if (mode < 1) return c;\n"
    "  if (eq(c, nw) && eq(c, ne) && eq(c, sw) && eq(c, se) &&\n"
    "      eq(n, s) && eq(n, e) && eq(n, w) && !eq(c, n))\n"
    "    return mix(c, n, 0.5);\n"
    "  if (eq(c, n) && eq(c, s) && eq(e, ne) && eq(e, se) &&\n"
    "      eq(w, nw) && eq(w, sw) && eq(e, w) && !eq(c, e))\n"
    "    return mix(c, e, 0.5);\n"
    "  if (eq(c, e) && eq(c, w) && eq(n, ne) && eq(n, nw) &&\n"
    "      eq(s, se) && eq(s, sw) && eq(n, s) && !eq(c, n))\n"
    "    return mix(c, n, 0.5);\n"
    "  return c;\n"
    "}\n"
    "void main() {\n"
    "  vec2 pos = uv * source_size;\n"
    "  vec2 t = floor(pos);\n"
    "  vec2 fp = pos - t;\n"
    "  vec2 dir = vec2(fp.x < 0.5 ? -1.0 : 1.0, fp.y < 0.5 ? -1.0 : 1.0);\n"
    "  vec2 f = abs(fp - 0.5) + 0.5;\n"
    "  vec2 scale = max(output_size / source_size, vec2(1.0));\n"
    "  /* softness widens every transition from one output pixel to three. */\n"
    "  float band = 1.0 + 2.0 * softness;\n"
    "  float aa = min(scale.x, scale.y) / band;\n"
    "  vec2 dx = vec2(dir.x, 0.0);\n"
    "  vec2 dy = vec2(0.0, dir.y);\n"
    "  vec3 A1 = tx(t - dx - dy - dy), B1 = tx(t - dy - dy), C1 = tx(t + dx - dy - dy);\n"
    "  vec3 A0 = tx(t - dx - dx - dy), A = tx(t - dx - dy), B = tx(t - dy), C = tx(t + dx - dy), C4 = tx(t + dx + dx - dy);\n"
    "  vec3 D0 = tx(t - dx - dx), D = tx(t - dx), E = tx(t), F = tx(t + dx), F4 = tx(t + dx + dx);\n"
    "  vec3 G0 = tx(t - dx - dx + dy), G = tx(t - dx + dy), H = tx(t + dy), I = tx(t + dx + dy), I4 = tx(t + dx + dx + dy);\n"
    "  vec3 G5 = tx(t - dx + dy + dy), H5 = tx(t + dy + dy), I5 = tx(t + dx + dy + dy);\n"
    "  vec3 e = decode(E, B, H, D, F, A, C, G, I);\n"
    "  vec3 fc = decode(F, C, I, E, F4, B, C4, H, I4);\n"
    "  vec3 hc = decode(H, E, H5, G, I, D, F, G5, I5);\n"
    "  vec3 ic = decode(I, F, I5, H, I4, E, F4, H5, I5);\n"
    "  /* f runs from the texel center (0.5) to its edge (1.0); blend half\n"
    "     way to the neighbor over the last output pixel before the edge,\n"
    "     and the neighbor's fragments continue the other half. */\n"
    "  vec2 adj = 0.5 * clamp((f - (1.0 - 0.5 * band / scale)) * scale / band,\n"
    "                         0.0, 1.0);\n"
    "  vec3 base = mix(mix(e, fc, adj.x), mix(hc, ic, adj.x), adj.y);\n"
    "  /* Smooth shading: where the neighbors are close in color (a shading\n"
    "     band of the pre-rendered art, not an outline), interpolate them\n"
    "     into a gradient instead of flat steps. */\n"
    "  if (shading > 0.0) {\n"
    "    vec2 g = f - 0.5;\n"
    "    vec3 bil = mix(mix(e, fc, g.x), mix(hc, ic, g.x), g.y);\n"
    "    float sim = max(max(df(e, fc), df(e, hc)), df(e, ic));\n"
    "    float w = shading * (1.0 - smoothstep(0.03, 0.14, sim));\n"
    "    base = mix(base, bil, w);\n"
    "  }\n"
    "  if (mode < 2) { gl_FragColor = vec4(base, 1.0); return; }\n"
    "  float wd1 = df(E, C) + df(E, G) + df(I, H5) + df(I, F4) + 4.0 * df(H, F);\n"
    "  float wd2 = df(H, D) + df(H, I5) + df(F, I4) + df(F, B) + 4.0 * df(E, I);\n"
    "  bool edr = wd1 < wd2 && !eq(E, H) && !eq(E, F) && !(eq(E, I) && eq(H, F));\n"
    "  if (!edr) { gl_FragColor = vec4(base, 1.0); return; }\n"
    "  vec3 nc = (df(E, F) <= df(E, H)) ? fc : hc;\n"
    "  float cov = clamp((f.x + f.y - 1.5) * aa + 0.5, 0.0, 1.0);\n"
    "  if (mode >= 3) {\n"
    "    bool left = 2.0 * df(F, G) <= df(H, C) && !eq(E, G) && !eq(D, G);\n"
    "    bool up = df(F, G) >= 2.0 * df(H, C) && !eq(E, C) && !eq(B, C);\n"
    "    if (left) cov = max(cov, clamp((2.0 * f.x + f.y - 2.0) * aa * 0.75 + 0.5, 0.0, 1.0));\n"
    "    if (up) cov = max(cov, clamp((f.x + 2.0 * f.y - 2.0) * aa * 0.75 + 0.5, 0.0, 1.0));\n"
    "    if (mode >= 4) {\n"
    "      bool left3 = left && 4.0 * df(F, G) <= df(H, C) && !eq(E, G0) && !eq(D0, G0);\n"
    "      bool up3 = up && df(F, G) >= 4.0 * df(H, C) && !eq(E, C1) && !eq(B1, C1);\n"
    "      if (left3) cov = max(cov, clamp((3.0 * f.x + f.y - 2.5) * aa * 0.6 + 0.5, 0.0, 1.0));\n"
    "      if (up3) cov = max(cov, clamp((f.x + 3.0 * f.y - 2.5) * aa * 0.6 + 0.5, 0.0, 1.0));\n"
    "    }\n"
    "  }\n"
    "  gl_FragColor = vec4(mix(base, nc, cov * strength), 1.0);\n"
    "}\n";

#endif

# Komposition The-One-Reel: 1080x1920, 30 fps, 178 s. GUI-Frames (2200x850) + Kamerafahrten + Titel + Endkarte
import math, subprocess, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter

W, H, FPS = 1080, 1920, 30
TOTAL, GUI_UNTIL = 178.0, 166.0
FONT = "/usr/share/fonts/opentype/inter/"
def font(w, s): return ImageFont.truetype(FONT + {"black": "Inter-Black.otf", "bold": "Inter-Bold.otf", "med": "Inter-Medium.otf", "semi": "Inter-SemiBold.otf"}[w], s)

BG1, BG2 = (23, 24, 28), (13, 14, 17)
TEXT, DIM = (236, 238, 242), (125, 130, 140)
OR, VI, CY, GR, BL, YE, LA, SEQ = (255, 154, 77), (167, 139, 250), (77, 208, 225), (94, 230, 168), (79, 184, 238), (255, 209, 102), (159, 168, 218), (255, 138, 61)
URL = "danikoenig.github.io/plugins"

CROP = {
    "full":    [0, 0, 2200, 850],
    "osc":     [40, 172, 782, 312],
    "filt":    [1250, 2, 910, 274],
    "lfo":     [40, 2, 782, 190],
    "chord":   [244, 606, 572, 132],
    "chordKb": [222, 606, 940, 240],
    "display": [820, 2, 430, 350],
    "out":     [1250, 474, 180, 270],
    "seq":     [1420, 474, 740, 272],
    "seqL":    [1420, 474, 460, 272],
    "seqRows": [1560, 600, 600, 148],
    "seqCtl":  [1420, 474, 420, 272],
    "fx":      [820, 344, 430, 400],
}

SCENES = [  # start, ende, titel, untertitel, ausschnitt, farbe
    (0, 6, "", "", "full", OR),
    (6, 18, "3 OSCILLATORS", "wave angle · pulse · sync · FM · sub", "osc", OR),
    (18, 30, "TWO FILTERS", "ladder + state variable · serial or parallel", "filt", CY),
    (30, 38, "4 LFOs · TEMPO SYNC", "straight · dotted · triplet", "lfo", VI),
    (38, 50, "CHORDS ON EVERY KEY", "18 types or your own learned chord", "chord", BL),
    (50, 60, "INVERSION · VOICING · BASS", "open · drop 2 · wide · -1 / -2 oct", "chordKb", BL),
    (60, 65, "PRESETS", "save your own sounds", "display", BL),
    (65, 72, "BIG PADS", "16 voices · unison · ensemble", "full", OR),
    (72, 82, "KNACK CLIP", "push the bass", "out", BL),
    (82, 90, "GLIDING LEADS", "mono · legato · vibrato", "display", BL),
    (90, 100, "16-STEP SEQUENCER", "hold one key and it plays", "seq", SEQ),
    (100, 106, "PITCH + OCTAVE", "per step, in semitones", "seqL", SEQ),
    (106, 112, "SLIDE + ACCENT", "acid glides and punch", "seqRows", SEQ),
    (112, 118, "RANDOM", "pitch · octave · accent · velocity", "seqCtl", SEQ),
    (118, 124, "PLAY ORDER", "forward · reverse · ping-pong · random", "seq", SEQ),
    (124, 138, "3 FX SLOTS", "delays · chorus · phaser · resonator", "fx", BL),
    (138, 152, "ALL ON ONE PANEL", "every control on the surface", "full", OR),
    (152, 166, "FREE DOWNLOAD", "VST3 · AU · CLAP · Mac", "full", GR),
]

def scene_at(t):
    for i, s in enumerate(SCENES):
        if s[0] <= t < s[1]: return i
    return len(SCENES) - 1

def ease(x): x = max(0.0, min(1.0, x)); return x * x * (3 - 2 * x)

BOX_W, BOX_TOP, BOX_MAXH = 1020, 600, 1060
SMAX = 2.3          # maximale Vergroesserung der GUI-Frames
def view_in(i, t):
    """Quellrechteck und Massstab fuer Szene i zur Zeit t (breite Ausschnitte werden abgefahren)."""
    sc = SCENES[i]
    c = CROP[sc[4]]
    s = min(SMAX, BOX_MAXH / c[3], 2.2 if sc[4] == "full" else 9)
    if sc[4] == "full": s = min(s, 1.15)
    vw = min(c[2], BOX_W / s)
    u = ease((t - sc[0] - 0.4) / max(0.5, (sc[1] - sc[0]) - 1.2))
    vx = c[0] + (c[2] - vw) * u
    return [vx, c[1], vw, c[3]], s

def crop_at(t):
    i = scene_at(t)
    cur, s = view_in(i, t)
    if i == 0: return cur, s
    prev, ps = view_in(i - 1, SCENES[i][0] - 1e-3)
    u = ease((t - SCENES[i][0]) / 0.7)
    r = [prev[k] + (cur[k] - prev[k]) * u for k in range(4)]
    return r, ps + (s - ps) * u

def box_for(crop, s):
    w, h = crop[2] * s, crop[3] * s
    return (W - w) / 2, BOX_TOP + (BOX_MAXH - h) * 0.38, w, h, s

def make_bg():
    bg = np.zeros((H, W, 3), dtype=np.float32)
    for k in range(3):
        bg[:, :, k] = np.linspace(BG1[k], BG2[k], H)[:, None]
    glow = Image.new("RGB", (W, H), (0, 0, 0))
    g = ImageDraw.Draw(glow)
    for (cx, cy), c in zip([(0, 0), (W, 0), (0, H), (W, H)], [OR, BL, VI, CY]):
        g.ellipse([cx - 520, cy - 520, cx + 520, cy + 520], fill=tuple(int(v * 0.15) for v in c))
    glow = np.asarray(glow.filter(ImageFilter.GaussianBlur(170)), dtype=np.float32)
    return Image.fromarray(np.clip(bg + glow, 0, 255).astype("uint8"))
BG = make_bg()

def logo(d, cx, cy, r):
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=OR, width=max(3, int(r * 0.17)))
    d.ellipse([cx - r * 0.42, cy - r * 0.42, cx + r * 0.42, cy + r * 0.42], fill=OR)
    d.ellipse([cx + r * 0.7, cy - r * 1.2, cx + r * 1.2, cy - r * 0.7], fill=(255, 171, 74))

def centered(d, y, text, f, fill):
    w = d.textlength(text, font=f)
    d.text(((W - w) / 2, y), text, font=f, fill=fill)

F_CAP_T, F_CAP_S = font("black", 86), font("med", 40)
F_TITLE, F_TSUB = font("black", 140), font("semi", 46)
F_SMALL = font("semi", 34)

def gui_frame(f):
    return Image.open(f"out/frames/{f:04d}.jpg").convert("RGB")

def render(f):
    t = f / FPS
    img = BG.copy()
    d = ImageDraw.Draw(img, "RGBA")
    if t < GUI_UNTIL + 0.8:
        gf = gui_frame(min(f, int(GUI_UNTIL * FPS) - 1))
        crop, sc_ = crop_at(t)
        bx, by, bw, bh, s = box_for(crop, sc_)
        region = gf.crop((int(crop[0]), int(crop[1]), int(crop[0] + crop[2]), int(crop[1] + crop[3]))).resize((int(bw), int(bh)), Image.LANCZOS)
        i = scene_at(t)
        sc = SCENES[i]
        if i == 0:
            k = 0.55 * (1 - ease((t - 4.6) / 1.2))
            region = Image.blend(region, Image.new("RGB", region.size, BG2), k)
        sh = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        ImageDraw.Draw(sh).rounded_rectangle([bx + 6, by + 18, bx + bw + 6, by + bh + 24], radius=26, fill=(0, 0, 0, 160))
        sh = sh.filter(ImageFilter.GaussianBlur(22))
        img.paste(sh, (0, 0), sh)
        mask = Image.new("L", region.size, 0)
        ImageDraw.Draw(mask).rounded_rectangle([0, 0, region.size[0] - 1, region.size[1] - 1], radius=22, fill=255)
        img.paste(region, (int(bx), int(by)), mask)
        d = ImageDraw.Draw(img, "RGBA")
        d.rounded_rectangle([bx, by, bx + bw, by + bh], radius=22, outline=sc[5] + (90,), width=3)

        if i == 0:
            u = ease(t / 0.8)
            logo(d, W / 2, 250 - 30 * (1 - u), 54)
            centered(d, 360, "THE ONE", F_TITLE, TEXT)
            centered(d, 530, "free 16-voice polyphonic synth  ·  v2.0", F_TSUB, DIM)
        else:
            u = ease((t - sc[0]) / 0.4)
            a = int(255 * u)
            yoff = 26 * (1 - u)
            d.rounded_rectangle([W / 2 - 40, 300 + yoff, W / 2 + 40, 308 + yoff], radius=4, fill=sc[5] + (a,))
            tw = d.textlength(sc[2], font=F_CAP_T)
            fcap = F_CAP_T if tw < W - 80 else font("black", int(86 * (W - 80) / tw))
            centered(d, 335 + yoff, sc[2], fcap, TEXT + (a,))
            centered(d, 450 + yoff, sc[3], F_CAP_S, DIM + (a,))
            logo(d, 96, 134, 22)
            d.text((136, 110), "THE ONE", font=font("black", 42), fill=TEXT)
        centered(d, 1790, "free  ·  VST3 + AU + CLAP  ·  Mac", F_SMALL, DIM)

    if t >= GUI_UNTIL:
        u = ease((t - GUI_UNTIL) / 0.8)
        end = BG.copy()
        e = ImageDraw.Draw(end, "RGBA")
        logo(e, W / 2, 470, 76)
        centered(e, 600, "THE ONE", font("black", 130), TEXT)
        centered(e, 760, "free 16-voice polyphonic synth  ·  v2.0", F_TSUB, DIM)
        lines = ["3 oscillators  ·  sub  ·  ring mod  ·  noise", "ladder + SVF filter  ·  4 LFOs  ·  3 envelopes",
                 "18 chord types  ·  inversion  ·  voicing  ·  bass", "16-step sequencer  ·  3 FX  ·  Knack clip  ·  presets"]
        for k, ln in enumerate(lines):
            centered(e, 860 + k * 58, ln, font("semi", 38), TEXT)
        centered(e, 1110, "VST3 + AU + CLAP  ·  Mac (Apple Silicon)", font("bold", 38), BL)
        fu = font("bold", 40)
        tw = e.textlength(URL, font=fu)
        bx0 = (W - tw) / 2 - 40
        e.rounded_rectangle([bx0, 1210, W - bx0, 1312], radius=24, fill=(29, 31, 36, 255), outline=OR + (255,), width=3)
        centered(e, 1238, URL, fu, TEXT)
        centered(e, 1352, "free download  ·  link in bio", font("med", 36), DIM)
        centered(e, 1700, "by Dani Koenig", font("semi", 36), DIM)
        img = Image.blend(img, end, u) if u < 1 else end
    return img

if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "preview":
        for ts in sys.argv[2:]:
            render(int(float(ts) * FPS)).save(f"/tmp/prev_{float(ts):06.2f}.jpg", quality=88)
        sys.exit(0)
    n = int(round(TOTAL * FPS))
    p = subprocess.Popen(["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{W}x{H}", "-r", str(FPS),
                          "-i", "-", "-c:v", "libx264", "-preset", "veryfast", "-crf", "18", "-pix_fmt", "yuv420p", "video_tmp.mp4"],
                         stdin=subprocess.PIPE)
    for f in range(n):
        p.stdin.write(render(f).tobytes())
        if f % 300 == 0: print("compose", f, "/", n, flush=True)
    p.stdin.close(); p.wait()
    print("video done")

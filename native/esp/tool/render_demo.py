"""Renderiza las lineas ESP con datos REALES capturados en partida (dist/esp-loop.txt)."""
import re
from PIL import Image, ImageDraw, ImageFont

BG = (22, 24, 28)
ENEMY = (255, 112, 112)
TEAMMATE = (110, 235, 160)

# BOX reales capturadas en partida (3088x1440). x,y,w,h,team
def load_boxes(path):
    boxes = []
    for line in open(path, encoding="utf-8", errors="replace"):
        m = re.match(r"BOX x=([-\d.]+) y=([-\d.]+) w=([-\d.]+) h=([-\d.]+) \| \d+m \| HP=-?\d+ \| T(-?\d+)", line)
        if m:
            x, y, w, h = map(float, m.groups()[:4])
            team = int(m.group(5))
            boxes.append((x, y, w, h, team))
    return boxes

def blend(c, a):
    return tuple(round(c[i] * a + BG[i] * (1 - a)) for i in range(3))

def line(draw, p0, p1, c, a, width=1):
    draw.line([p0, p1], fill=blend(c, a), width=width)

def snapline(draw, origin, anchor, c):
    k = 12
    for i in range(k):
        f0, f1 = i / k, (i + 1) / k
        a0 = (origin[0] + (anchor[0] - origin[0]) * f0, origin[1] + (anchor[1] - origin[1]) * f0)
        a1 = (origin[0] + (anchor[0] - origin[0]) * f1, origin[1] + (anchor[1] - origin[1]) * f1)
        # borde oscuro + núcleo verde brillante
        line(draw, a0, a1, (0, 0, 0), 0.55, width=3)
        line(draw, a0, a1, (0, 255, 120), (120 + 135 * f1) / 255, width=1)

def anchor_point(w, h, anchor):
    return (w / 2, (0, h / 2, h)[anchor])

def draw_scene(img, boxes, anchor, show_boxes, title):
    d = ImageDraw.Draw(img)
    w, h = img.size
    line(d, (w / 2, 0), (w / 2, h), (80, 84, 92), 0.35)
    line(d, (0, h / 2), (w, h / 2), (80, 84, 92), 0.25)
    for (x, y, bw, bh, team) in boxes:
        c = TEAMMATE if team == 0 else ENEMY  # T0 era el grupo sin vida; lo pinto distinto
        origin = (x + bw / 2, y + bh / 2)
        snapline(d, origin, anchor_point(w, h, anchor), c)
        if show_boxes:
            for p0, p1 in [((x, y), (x + bw, y)), ((x, y + bh), (x + bw, y + bh)),
                           ((x, y), (x, y + bh)), ((x + bw, y), (x + bw, y + bh))]:
                line(d, p0, p1, c, 0.85)
    if title:
        font = ImageFont.load_default()
        d.text((8, 6), title, fill=(255, 255, 255), font=font)

def main():
    boxes = load_boxes("dist/esp-loop.txt")
    real = boxes[:6]  # dos ticks reales
    # escala 0.5 para que sea ligera
    s = 0.5
    real = [(x * s, y * s, w * s, h * s, t) for (x, y, w, h, t) in real]

    # Imagen 1: solo lineas, ancla abajo
    w, h = int(3088 * s), int(1440 * s)
    img = Image.new("RGB", (w, h), BG)
    draw_scene(img, real, 2, False, "solo lineas · ancla abajo · datos reales")
    img.save("dist/esp-demo-lineas.png")

    # Imagen 2: las tres anclas, apiladas
    panel_h = h
    canvas = Image.new("RGB", (w, panel_h * 3), BG)
    for i, (anchor, label) in enumerate([(0, "ARRIBA"), (1, "CENTRO"), (2, "ABAJO")]):
        panel = Image.new("RGB", (w, panel_h), BG)
        draw_scene(panel, real, anchor, True, label)
        canvas.paste(panel, (0, i * panel_h))
    canvas.save("dist/esp-demo-anclas.png")
    print("ok:", len(boxes), "cajas reales, 2 imagenes")

main()

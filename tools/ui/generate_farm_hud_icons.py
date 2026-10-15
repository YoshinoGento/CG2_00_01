"""Original, deterministic farm HUD artwork; no input images or external fonts.

Run with --preview for the labeled 64/48px contact sheet, or --check to verify
existing outputs without writing. Cell order is the runtime UV contract below.
The atlas is straight-alpha RGBA with integer-grid art and a transparent gutter.
"""

from __future__ import annotations

import argparse
from hashlib import sha256
from io import BytesIO
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[2]
ATLAS_PATH = ROOT / "project/Resources/ui/farm_hud_icons.png"
PREVIEW_PATH = ROOT / "generated/codex_checks/farm_icon_hud_20261015/icons_preview.png"
CELL = 64
COLUMNS = 8
ROWS = 4
PADDING = 4
OUTLINE = 2

# One shared palette; coordinates below are authored in final 64px cell space.
INK = "#26383c"
PAPER = "#f2e8c9"
PAPER_SHADE = "#cbbc94"
WHITE = "#fff5dc"
METAL = "#91adb3"
METAL_SHADE = "#617e89"
METAL_LIGHT = "#d2e3df"
WOOD = "#bc804c"
WOOD_SHADE = "#855438"
WOOD_LIGHT = "#e0b16a"
GREEN = "#5e9d62"
GREEN_SHADE = "#356c55"
GREEN_LIGHT = "#a8cc78"
RED = "#df6354"
RED_SHADE = "#b44149"
RED_LIGHT = "#f59b76"
ORANGE = "#ee9d47"
ORANGE_SHADE = "#c77537"
ORANGE_LIGHT = "#ffd077"
BLUE = "#59b5cf"
BLUE_SHADE = "#337d9e"
BLUE_LIGHT = "#b6e9e8"
GOLD = "#e8bd55"
GOLD_SHADE = "#bb873d"
GOLD_LIGHT = "#ffe69a"
TRANSPARENT = (0, 0, 0, 0)


def polygon(draw, points, fill, outline=INK, width=OUTLINE):
    draw.polygon(points, fill=fill, outline=outline, width=width)


def rectangle(draw, box, fill, outline=INK, width=OUTLINE):
    draw.rectangle(box, fill=fill, outline=outline, width=width)


def ellipse(draw, box, fill, outline=INK, width=OUTLINE):
    draw.ellipse(box, fill=fill, outline=outline, width=width)


def line(draw, points, fill, width=OUTLINE):
    draw.line(points, fill=fill, width=width)


def hoe(draw):
    # A long shaft ends in one broad, downturned blade, never a two-ended pick.
    polygon(draw, [(12, 51), (16, 55), (43, 20), (38, 16)], WOOD)
    line(draw, [(16, 50), (38, 21)], WOOD_LIGHT)
    polygon(draw, [(34, 14), (39, 9), (53, 15), (55, 27), (44, 32),
                   (41, 23), (36, 21)], METAL)
    polygon(draw, [(43, 20), (52, 17), (53, 26), (45, 29)], METAL_SHADE, None)
    line(draw, [(39, 13), (49, 17)], METAL_LIGHT)
    line(draw, [(45, 29), (52, 26)], METAL_LIGHT)
    rectangle(draw, (34, 19, 40, 23), WOOD_SHADE)


def watering_can(draw):
    polygon(draw, [(13, 21), (8, 23), (6, 29), (7, 38), (13, 43),
                   (20, 40), (21, 24)], METAL)
    polygon(draw, [(13, 27), (11, 30), (12, 36), (16, 37), (16, 27)], TRANSPARENT)
    polygon(draw, [(36, 35), (43, 30), (49, 20), (54, 23), (49, 37),
                   (39, 44)], BLUE)
    polygon(draw, [(47, 18), (51, 15), (58, 22), (55, 27)], METAL)
    line(draw, [(51, 18), (55, 22)], METAL_LIGHT)
    polygon(draw, [(17, 24), (34, 24), (40, 31), (40, 48), (35, 53),
                   (17, 53), (13, 48), (13, 30)], BLUE)
    polygon(draw, [(33, 28), (38, 32), (38, 47), (34, 51), (29, 51)], BLUE_SHADE, None)
    line(draw, [(17, 32), (17, 45)], BLUE_LIGHT, 3)
    ellipse(draw, (18, 20, 34, 28), METAL)
    ellipse(draw, (22, 22, 30, 25), INK, None)


def seed_bag(draw):
    polygon(draw, [(23, 18), (20, 9), (29, 11), (35, 8), (43, 10),
                   (39, 19)], PAPER)
    polygon(draw, [(23, 21), (39, 21), (47, 33), (49, 47), (44, 54),
                   (20, 54), (15, 49), (16, 36)], PAPER)
    polygon(draw, [(39, 24), (44, 34), (47, 47), (42, 52), (37, 52)], PAPER_SHADE, None)
    rectangle(draw, (21, 18, 41, 23), WOOD)
    line(draw, [(25, 39), (32, 44), (38, 35)], GREEN_SHADE, 3)
    polygon(draw, [(32, 39), (24, 37), (22, 30), (30, 31), (33, 35)], GREEN)
    polygon(draw, [(33, 38), (34, 30), (41, 28), (40, 35)], GREEN_LIGHT)
    ellipse(draw, (21, 45, 25, 49), WOOD_SHADE, None)
    ellipse(draw, (29, 47, 33, 50), WOOD_SHADE, None)


def harvest_basket(draw):
    draw.arc((16, 8, 48, 43), 180, 360, fill=INK, width=6)
    draw.arc((18, 10, 46, 41), 180, 360, fill=WOOD_LIGHT, width=2)
    polygon(draw, [(16, 25), (13, 16), (19, 18), (22, 13), (25, 23)], GREEN)
    polygon(draw, [(15, 24), (24, 21), (33, 43), (23, 45)], ORANGE)
    line(draw, [(20, 28), (24, 27)], ORANGE_LIGHT)
    ellipse(draw, (27, 23, 48, 44), RED)
    polygon(draw, [(34, 27), (30, 23), (36, 23), (39, 18), (40, 24),
                   (46, 25), (40, 28)], GREEN_SHADE)
    rectangle(draw, (31, 30, 34, 33), RED_LIGHT, None)
    polygon(draw, [(40, 36), (45, 24), (54, 22), (51, 32)], GREEN_LIGHT)
    polygon(draw, [(10, 34), (54, 34), (49, 52), (45, 55), (18, 55),
                   (14, 51)], WOOD)
    polygon(draw, [(43, 38), (51, 38), (47, 51), (43, 53)], WOOD_SHADE, None)
    for x in (21, 30, 39):
        line(draw, [(x, 40), (x + 1, 51)], WOOD_LIGHT, 3)
    line(draw, [(16, 46), (48, 46)], WOOD_SHADE)
    rectangle(draw, (9, 33, 55, 39), WOOD_LIGHT)


def carrot(draw):
    polygon(draw, [(33, 23), (29, 13), (33, 8), (38, 20), (40, 9),
                   (46, 8), (44, 20), (52, 15), (56, 19), (43, 28)], GREEN)
    line(draw, [(38, 24), (43, 14)], GREEN_LIGHT)
    polygon(draw, [(32, 22), (42, 23), (47, 30), (44, 39), (16, 55),
                   (11, 55), (15, 43), (24, 28)], ORANGE)
    polygon(draw, [(43, 30), (45, 32), (41, 38), (17, 52), (31, 37)], ORANGE_SHADE, None)
    line(draw, [(19, 44), (28, 30), (33, 27)], ORANGE_LIGHT, 3)
    line(draw, [(26, 29), (32, 34)], ORANGE_SHADE)
    line(draw, [(22, 39), (27, 43)], ORANGE_SHADE)


def tomato(draw):
    polygon(draw, [(17, 22), (25, 19), (33, 22), (42, 19), (50, 24),
                   (55, 33), (53, 44), (46, 52), (34, 55), (22, 53),
                   (13, 46), (9, 35), (11, 27)], RED)
    polygon(draw, [(49, 29), (53, 34), (51, 43), (44, 50), (33, 52),
                   (27, 49), (41, 46), (48, 39)], RED_SHADE, None)
    polygon(draw, [(17, 29), (21, 26), (26, 27), (24, 31), (19, 34),
                   (16, 34)], RED_LIGHT, None)
    polygon(draw, [(31, 20), (23, 14), (30, 15), (34, 8), (39, 10),
                   (36, 20), (46, 17), (42, 24), (47, 28), (35, 26),
                   (27, 31), (28, 24), (19, 23)], GREEN_SHADE)
    line(draw, [(33, 22), (37, 14)], GREEN_LIGHT)


def pumpkin(draw):
    polygon(draw, [(29, 21), (28, 11), (35, 8), (40, 11), (35, 15),
                   (36, 23)], GREEN_SHADE)
    polygon(draw, [(12, 24), (22, 19), (32, 22), (43, 19), (52, 25),
                   (56, 36), (53, 48), (45, 54), (20, 54), (11, 48),
                   (7, 37)], ORANGE_SHADE)
    ellipse(draw, (15, 21, 39, 54), ORANGE)
    ellipse(draw, (28, 21, 49, 54), ORANGE)
    ellipse(draw, (24, 20, 40, 55), ORANGE)
    line(draw, [(30, 26), (28, 32), (28, 43)], ORANGE_LIGHT, 3)
    line(draw, [(19, 28), (17, 35)], ORANGE_LIGHT)
    polygon(draw, [(34, 21), (40, 13), (49, 14), (46, 20), (39, 23)], GREEN)


def water_drop(draw):
    polygon(draw, [(32, 7), (40, 19), (48, 30), (52, 40), (50, 48),
                   (44, 54), (33, 57), (22, 54), (15, 47), (13, 39),
                   (17, 29), (26, 17)], BLUE)
    polygon(draw, [(42, 29), (49, 40), (47, 47), (41, 52), (31, 54),
                   (24, 51), (37, 48), (43, 41)], BLUE_SHADE, None)
    line(draw, [(27, 25), (22, 34), (21, 40)], BLUE_LIGHT, 4)
    rectangle(draw, (23, 44, 26, 47), BLUE_LIGHT, None)


def sprout(draw):
    polygon(draw, [(13, 48), (22, 43), (43, 43), (53, 49), (51, 54),
                   (12, 54), (9, 51)], WOOD)
    line(draw, [(18, 49), (26, 48)], WOOD_LIGHT)
    line(draw, [(32, 46), (32, 25)], INK, 6)
    line(draw, [(32, 45), (32, 25)], GREEN_SHADE)
    polygon(draw, [(30, 34), (20, 33), (12, 26), (10, 17), (20, 18),
                   (29, 24), (32, 30)], GREEN)
    polygon(draw, [(33, 28), (34, 17), (42, 11), (54, 10), (52, 21),
                   (44, 28)], GREEN_LIGHT)
    line(draw, [(17, 24), (28, 30)], GREEN_LIGHT)
    line(draw, [(37, 24), (46, 17)], GREEN_SHADE)


def calendar(draw):
    polygon(draw, [(12, 13), (51, 13), (54, 16), (54, 52), (51, 55),
                   (12, 55), (9, 52), (9, 16)], PAPER)
    rectangle(draw, (10, 14, 53, 26), RED)
    line(draw, [(13, 28), (50, 28)], PAPER_SHADE)
    rectangle(draw, (18, 8, 23, 19), METAL_LIGHT)
    rectangle(draw, (40, 8, 45, 19), METAL_LIGHT)
    for x, y in ((17, 33), (28, 33), (39, 33), (17, 44), (28, 44)):
        rectangle(draw, (x, y, x + 5, y + 5), METAL_SHADE, None)
    rectangle(draw, (38, 43, 47, 51), GREEN)


def coin(draw):
    polygon(draw, [(23, 9), (40, 9), (51, 19), (55, 31), (52, 45),
                   (41, 55), (24, 55), (13, 47), (8, 33), (12, 19)], GOLD_SHADE)
    polygon(draw, [(23, 10), (38, 10), (48, 18), (52, 30), (48, 42),
                   (38, 51), (23, 51), (13, 42), (9, 30), (13, 18)], GOLD)
    ellipse(draw, (16, 16, 45, 45), GOLD, GOLD_SHADE)
    polygon(draw, [(31, 20), (40, 30), (31, 41), (22, 30)], GOLD_LIGHT)
    line(draw, [(16, 19), (22, 14), (34, 14)], GOLD_LIGHT, 3)
    line(draw, [(45, 47), (48, 44)], GOLD_LIGHT)


def terrain_steps(draw):
    polygon(draw, [(8, 43), (23, 43), (23, 29), (38, 29), (38, 15),
                   (55, 15), (55, 54), (8, 54)], WOOD)
    rectangle(draw, (8, 41, 23, 46), GREEN)
    rectangle(draw, (23, 27, 38, 32), GREEN)
    rectangle(draw, (38, 13, 55, 18), GREEN_LIGHT)
    line(draw, [(25, 35), (25, 51)], WOOD_SHADE)
    line(draw, [(40, 22), (40, 51)], WOOD_SHADE)
    line(draw, [(12, 50), (18, 50)], WOOD_LIGHT)
    line(draw, [(29, 38), (34, 38)], WOOD_LIGHT)
    line(draw, [(45, 25), (50, 25)], WOOD_LIGHT)


def pause(draw):
    rectangle(draw, (14, 11, 27, 53), GOLD)
    rectangle(draw, (37, 11, 50, 53), GOLD)
    line(draw, [(18, 15), (18, 48)], GOLD_LIGHT, 3)
    line(draw, [(41, 15), (41, 48)], GOLD_LIGHT, 3)


def play(draw):
    polygon(draw, [(17, 9), (54, 30), (54, 34), (17, 55), (13, 53),
                   (13, 11)], GREEN)
    line(draw, [(18, 16), (18, 45)], GREEN_LIGHT, 3)
    line(draw, [(23, 47), (47, 33)], GREEN_SHADE, 3)


def overview_camera(draw):
    polygon(draw, [(9, 47), (31, 37), (55, 47), (33, 57)], GREEN)
    line(draw, [(19, 43), (42, 53)], GREEN_LIGHT)
    line(draw, [(42, 42), (20, 52)], GREEN_SHADE)
    line(draw, [(22, 33), (16, 42)], METAL_SHADE)
    line(draw, [(42, 33), (48, 42)], METAL_SHADE)
    polygon(draw, [(12, 15), (21, 15), (24, 10), (36, 10), (40, 15),
                   (51, 15), (53, 18), (53, 32), (50, 35), (13, 35),
                   (10, 32), (10, 18)], METAL)
    ellipse(draw, (23, 16, 41, 33), METAL_SHADE)
    ellipse(draw, (27, 20, 37, 30), BLUE)
    rectangle(draw, (29, 22, 32, 24), BLUE_LIGHT, None)
    rectangle(draw, (15, 19, 19, 22), PAPER, None)


def follow_person(draw):
    # Corner brackets express target-follow; the camera icon instead shows land.
    for points in ([(17, 23), (9, 23), (9, 13), (17, 13)],
                   [(47, 13), (55, 13), (55, 23), (47, 23)],
                   [(9, 43), (9, 53), (18, 53)],
                   [(46, 53), (55, 53), (55, 43)]):
        line(draw, points, INK, 5)
        line(draw, points, GOLD, 2)
    ellipse(draw, (25, 10, 39, 24), PAPER)
    polygon(draw, [(25, 28), (38, 28), (44, 38), (40, 42), (37, 36),
                   (37, 43), (42, 54), (34, 54), (31, 45), (28, 54),
                   (21, 54), (26, 41), (25, 36), (21, 42), (16, 38)], BLUE)
    line(draw, [(29, 31), (34, 31)], BLUE_LIGHT)


def menu(draw):
    for y in (13, 28, 43):
        rectangle(draw, (10, y, 16, y + 7), GOLD)
        rectangle(draw, (22, y, 54, y + 7), PAPER)
        line(draw, [(25, y + 2), (50, y + 2)], WHITE)


def shop(draw):
    rectangle(draw, (13, 26, 51, 54), PAPER)
    rectangle(draw, (35, 34, 46, 54), WOOD)
    rectangle(draw, (18, 34, 29, 45), BLUE)
    line(draw, [(20, 36), (23, 36), (23, 42)], BLUE_LIGHT)
    rectangle(draw, (38, 43, 40, 45), GOLD_LIGHT, None)
    polygon(draw, [(15, 12), (49, 12), (56, 26), (56, 30), (8, 30),
                   (8, 26)], RED)
    polygon(draw, [(21, 14), (28, 14), (27, 28), (17, 28)], PAPER, None)
    polygon(draw, [(36, 14), (43, 14), (48, 28), (38, 28)], PAPER, None)
    line(draw, [(9, 30), (55, 30)], INK)
    line(draw, [(12, 55), (52, 55)], METAL_SHADE, 3)


def seed_packet(draw, crop):
    polygon(draw, [(17, 8), (46, 8), (49, 12), (49, 53), (46, 56),
                   (17, 56), (14, 53), (14, 12)], PAPER)
    rectangle(draw, (15, 9, 48, 17), crop)
    line(draw, [(20, 12), (43, 12)], WHITE)
    line(draw, [(19, 51), (44, 51)], PAPER_SHADE)
    ellipse(draw, (21, 46, 24, 48), WOOD_SHADE, None)
    ellipse(draw, (29, 46, 32, 48), WOOD_SHADE, None)
    ellipse(draw, (37, 46, 40, 48), WOOD_SHADE, None)


def seed_carrot(draw):
    seed_packet(draw, ORANGE)
    polygon(draw, [(32, 27), (29, 21), (32, 19), (35, 24), (41, 20),
                   (42, 24), (36, 30)], GREEN)
    polygon(draw, [(30, 26), (37, 28), (39, 32), (24, 43), (21, 43),
                   (24, 33)], ORANGE)
    line(draw, [(26, 34), (30, 29)], ORANGE_LIGHT)


def seed_tomato(draw):
    seed_packet(draw, RED)
    ellipse(draw, (21, 26, 43, 43), RED)
    polygon(draw, [(31, 28), (25, 25), (30, 24), (33, 20), (35, 24),
                   (40, 24), (37, 28), (38, 31), (33, 29), (28, 32)], GREEN_SHADE)
    rectangle(draw, (25, 32, 28, 34), RED_LIGHT, None)


def seed_pumpkin(draw):
    seed_packet(draw, GREEN)
    rectangle(draw, (30, 21, 34, 28), GREEN_SHADE)
    ellipse(draw, (20, 27, 44, 43), ORANGE_SHADE)
    ellipse(draw, (25, 26, 40, 43), ORANGE)
    ellipse(draw, (29, 26, 36, 43), ORANGE)
    line(draw, [(32, 29), (32, 35)], ORANGE_LIGHT)


def rank_rosette(draw):
    polygon(draw, [(20, 33), (31, 39), (25, 57), (20, 51), (13, 52)], RED)
    polygon(draw, [(33, 39), (43, 33), (51, 52), (43, 51), (38, 57)], RED)
    polygon(draw, [(32, 7), (38, 11), (45, 11), (47, 18), (53, 23),
                   (49, 30), (49, 37), (42, 40), (37, 46), (30, 43),
                   (23, 46), (18, 40), (11, 37), (12, 30), (9, 23),
                   (15, 18), (17, 11), (24, 11)], GOLD)
    ellipse(draw, (18, 14, 44, 40), GOLD_SHADE)
    polygon(draw, [(31, 18), (35, 24), (41, 25), (36, 30), (37, 36),
                   (31, 33), (25, 36), (26, 30), (21, 25), (28, 24)], GOLD_LIGHT)


def canal(draw):
    polygon(draw, [(10, 12), (52, 12), (56, 16), (56, 51), (51, 55),
                   (12, 55), (8, 51), (8, 16)], WOOD)
    polygon(draw, [(25, 13), (41, 13), (41, 28), (37, 33), (37, 54),
                   (21, 54), (21, 32), (25, 27)], BLUE)
    polygon(draw, [(35, 15), (39, 15), (39, 27), (35, 32), (35, 52),
                   (30, 52), (30, 32), (35, 26)], BLUE_SHADE, None)
    line(draw, [(29, 18), (29, 27), (26, 31), (26, 46)], BLUE_LIGHT)
    line(draw, [(12, 19), (20, 19)], GREEN_LIGHT, 3)
    line(draw, [(44, 43), (52, 43)], GREEN_LIGHT, 3)
    line(draw, [(12, 34), (18, 34)], WOOD_LIGHT)


def water_source(draw):
    polygon(draw, [(9, 48), (15, 43), (47, 43), (55, 48), (53, 54),
                   (42, 57), (19, 56), (9, 53)], BLUE)
    polygon(draw, [(11, 41), (14, 24), (21, 19), (26, 10), (37, 8),
                   (46, 18), (51, 32), (50, 43), (42, 47), (18, 47)], METAL)
    polygon(draw, [(37, 12), (44, 20), (48, 33), (47, 43), (38, 45),
                   (35, 31), (39, 24)], METAL_SHADE, None)
    line(draw, [(20, 25), (29, 21), (30, 15)], METAL_LIGHT, 3)
    polygon(draw, [(26, 30), (33, 27), (39, 31), (39, 39), (25, 39)], INK)
    polygon(draw, [(29, 33), (36, 33), (37, 44), (43, 50), (39, 54),
                   (26, 54), (22, 50), (28, 44)], BLUE)
    line(draw, [(32, 35), (32, 45), (29, 49)], BLUE_LIGHT, 3)
    line(draw, [(13, 51), (19, 51)], BLUE_LIGHT)


def back(draw):
    polygon(draw, [(29, 12), (9, 30), (9, 34), (29, 52), (33, 48),
                   (33, 40), (54, 40), (54, 24), (33, 24), (33, 16)], PAPER)
    line(draw, [(17, 31), (28, 21)], WHITE, 3)
    line(draw, [(35, 36), (50, 36)], PAPER_SHADE, 3)


def check(draw):
    polygon(draw, [(8, 32), (14, 25), (25, 36), (47, 12), (55, 19),
                   (25, 53)], GREEN)
    line(draw, [(13, 31), (25, 43), (48, 19)], GREEN_LIGHT, 3)


def turn_arrow(draw, mirror):
    def points(vertices):
        return [(CELL - 1 - x, y) if mirror else (x, y) for x, y in vertices]

    polygon(draw, points([(26, 9), (8, 25), (26, 40), (26, 30), (38, 30),
                          (44, 34), (46, 39), (44, 44), (36, 48), (39, 55),
                          (49, 51), (55, 44), (56, 35), (52, 27), (44, 21),
                          (26, 20)]), METAL)
    line(draw, points([(16, 25), (23, 19)]), METAL_LIGHT, 3)
    line(draw, points([(31, 24), (42, 24), (48, 29)]), METAL_LIGHT, 3)
    line(draw, points([(49, 39), (47, 46), (42, 49)]), METAL_SHADE, 3)


def undo(draw):
    turn_arrow(draw, mirror=False)


def redo(draw):
    turn_arrow(draw, mirror=True)


def plus(draw):
    polygon(draw, [(25, 10), (39, 10), (39, 25), (54, 25), (54, 39),
                   (39, 39), (39, 54), (25, 54), (25, 39), (10, 39),
                   (10, 25), (25, 25)], GREEN)
    line(draw, [(29, 15), (29, 29), (15, 29)], GREEN_LIGHT, 3)


def minus(draw):
    rectangle(draw, (10, 25, 54, 39), GOLD)
    line(draw, [(15, 29), (49, 29)], GOLD_LIGHT, 3)


def close(draw):
    polygon(draw, [(18, 10), (32, 24), (46, 10), (54, 18), (40, 32),
                   (54, 46), (46, 54), (32, 40), (18, 54), (10, 46),
                   (24, 32), (10, 18)], RED)
    line(draw, [(17, 17), (30, 30)], RED_LIGHT, 3)
    line(draw, [(46, 17), (36, 27)], RED_LIGHT, 3)


def inventory_crate(draw):
    polygon(draw, [(9, 19), (31, 9), (55, 19), (55, 48), (32, 57),
                   (9, 47)], WOOD)
    polygon(draw, [(9, 19), (31, 9), (55, 19), (32, 29)], WOOD_LIGHT)
    polygon(draw, [(32, 29), (55, 19), (55, 48), (32, 57)], WOOD_SHADE)
    line(draw, [(13, 29), (29, 36)], WOOD_SHADE)
    line(draw, [(13, 39), (29, 46)], WOOD_SHADE)
    line(draw, [(36, 36), (51, 30)], WOOD_LIGHT)
    line(draw, [(36, 46), (51, 40)], WOOD_LIGHT)
    polygon(draw, [(13, 24), (18, 26), (28, 48), (28, 52), (23, 50),
                   (13, 29)], WOOD_LIGHT)
    line(draw, [(22, 14), (44, 24)], WOOD_SHADE)
    line(draw, [(18, 20), (28, 24), (45, 17)], PAPER)


# Append/reorder only with a coordinated runtime index change.
ICONS = (
    ("hoe", hoe),
    ("watering_can", watering_can),
    ("seed_bag", seed_bag),
    ("harvest_basket", harvest_basket),
    ("carrot", carrot),
    ("tomato", tomato),
    ("pumpkin", pumpkin),
    ("water_drop", water_drop),
    ("sprout", sprout),
    ("calendar", calendar),
    ("coin", coin),
    ("terrain_steps", terrain_steps),
    ("pause", pause),
    ("play", play),
    ("overview_camera", overview_camera),
    ("follow_person", follow_person),
    ("menu", menu),
    ("shop", shop),
    ("seed_carrot", seed_carrot),
    ("seed_tomato", seed_tomato),
    ("seed_pumpkin", seed_pumpkin),
    ("rank_rosette", rank_rosette),
    ("canal", canal),
    ("water_source", water_source),
    ("back", back),
    ("check", check),
    ("undo", undo),
    ("redo", redo),
    ("plus", plus),
    ("minus", minus),
    ("close", close),
    ("inventory_crate", inventory_crate),
)


def generate_atlas():
    if len(ICONS) != COLUMNS * ROWS or len({name for name, _ in ICONS}) != len(ICONS):
        raise ValueError("The atlas requires exactly 32 unique named icons")
    atlas = Image.new("RGBA", (COLUMNS * CELL, ROWS * CELL))
    for index, (name, paint) in enumerate(ICONS):
        cell = Image.new("RGBA", (CELL, CELL))
        paint(ImageDraw.Draw(cell))
        bounds = cell.getchannel("A").getbbox()
        if bounds is None or not (
            PADDING <= bounds[0] < bounds[2] <= CELL - PADDING
            and PADDING <= bounds[1] < bounds[3] <= CELL - PADDING
        ):
            raise ValueError(f"{index}:{name} violates the transparent gutter: {bounds}")
        atlas.paste(cell, (index % COLUMNS * CELL, index // COLUMNS * CELL))
    return atlas


def generate_preview(atlas):
    # Built-in bitmap font avoids machine-specific font discovery/rasterization.
    font = ImageFont.load_default_imagefont()
    margin, tile_width, tile_height, header = 20, 146, 122, 52
    width = margin * 2 + COLUMNS * tile_width
    section_height = ROWS * tile_height + header
    preview = Image.new("RGB", (width, section_height * 2), "#26383c")
    draw = ImageDraw.Draw(preview)
    for section, (background, foreground) in enumerate(
        (("#26383c", "#f2e8c9"), ("#e5e9df", "#26383c"))
    ):
        top = section * section_height
        draw.rectangle((0, top, width - 1, top + section_height - 1), fill=background)
        draw.text((margin, top + 17), "ORIGINAL FARM HUD | 64px / 48px | fixed indices 00-31",
                  fill=foreground, font=font)
        for index, (name, _) in enumerate(ICONS):
            x = margin + index % COLUMNS * tile_width
            y = top + header + index // COLUMNS * tile_height
            source = (index % COLUMNS * CELL, index // COLUMNS * CELL)
            cell = atlas.crop((*source, source[0] + CELL, source[1] + CELL))
            preview.paste(cell, (x, y), cell)
            small = cell.resize((48, 48), Image.Resampling.LANCZOS)
            preview.paste(small, (x + 76, y + 8), small)
            draw.text((x + 1, y + 72), f"{index:02d} {name}", fill=foreground, font=font)
    return preview


def png_bytes(image):
    output = BytesIO()
    image.save(output, format="PNG", optimize=False, compress_level=9)
    return output.getvalue()


def emit(path, image, check):
    data = png_bytes(image)
    if check:
        if not path.is_file() or path.read_bytes() != data:
            raise SystemExit(f"FAIL: missing or stale output: {path}")
    else:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    print(f"{'PASS' if check else 'WROTE'} {path} | {len(data)} bytes | SHA256 {sha256(data).hexdigest()}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preview", action="store_true", help="Include the labeled 64/48px contact sheet")
    parser.add_argument("--check", action="store_true", help="Verify exact PNG bytes without writing")
    args = parser.parse_args()
    atlas = generate_atlas()
    emit(ATLAS_PATH, atlas, args.check)
    if args.preview:
        emit(PREVIEW_PATH, generate_preview(atlas), args.check)
    print(f"PASS: 32 cells, {atlas.width}x{atlas.height} RGBA, {PADDING}px minimum transparent gutter")


if __name__ == "__main__":
    main()

"""Deterministic branch-led broadleaf shapes for offline scenery generation."""
from dataclasses import dataclass
import math
import random


# X/Z use crown spread; Y uses tree height. Separate branch tips keep lower gaps open.
LEAF_GROUPS = (
    ((-.78, .60, -.16), (.48, .110, .40)),
    ((-.42, .82, -.13), (.47, .130, .39)),
    (( .01, .88,  .01), (.44, .120, .37)),
    (( .74, .66,  .05), (.49, .115, .41)),
    (( .38, .85,  .12), (.45, .130, .38)),
    (( .03, .66,  .62), (.46, .110, .43)),
)
HEIGHT_RANGE = (3.8, 6.4)
SPREAD_RANGE = (1.4, 2.2)
ROOT_EMBED = .04
MAX_TREE_TRIANGLES = 1500


@dataclass(frozen=True)
class TreeShape:
    # Each canopy is (center, radii); each branch is (start, end, base radius, tip radius).
    canopies: tuple
    branches: tuple


def make_tree_shape(height, spread, seed):
    if not math.isfinite(height) or not HEIGHT_RANGE[0] <= height <= HEIGHT_RANGE[1]:
        raise ValueError("Tree height outside authoring range")
    if not math.isfinite(spread) or not SPREAD_RANGE[0] <= spread <= SPREAD_RANGE[1]:
        raise ValueError("Tree spread outside authoring range")
    if type(seed) is not int or not 0 <= seed < 2**32:
        raise ValueError("Tree requires a bounded integer seed")
    rng = random.Random(seed)
    yaw = rng.uniform(-math.pi, math.pi)
    c, s = math.cos(yaw), math.sin(yaw)

    def point(p):
        x, y, z = p
        return (spread*(x*c-z*s), height*y, spread*(x*s+z*c))

    canopies = []
    for center, radii in LEAF_GROUPS:
        x, y, z = center
        # Bounded variation affects shape, never the number of lobes or their branch support.
        center = point((x+rng.uniform(-.035,.035), y, z+rng.uniform(-.035,.035)))
        rx, ry, rz = radii
        scale = rng.uniform(.94,1.04)
        radius = (spread*rx*scale, height*ry, spread*rz*scale)
        canopies.append((center, radius))

    root = (0, -ROOT_EMBED, 0)
    lower = point((.035,.31,-.01))
    upper = point((-.025,.54,.03))
    left = point((-.30,.45,-.09))
    right = point((.28,.48,.04))
    back = point((.02,.47,.28))
    tips = [center for center, _ in canopies]
    radius_scale = height/5.6
    branches = tuple((a,b,r*radius_scale,t*radius_scale) for a,b,r,t in (
        (root,lower,.18,.13), (lower,upper,.13,.08), (upper,tips[2],.08,.025),
        (lower,left,.115,.075), (left,tips[0],.075,.035), (left,tips[1],.065,.025),
        (lower,right,.110,.075), (right,tips[3],.075,.035), (right,tips[4],.065,.025),
        (lower,back,.100,.060), (back,tips[5],.060,.030),
    ))
    return TreeShape(tuple(canopies), branches)

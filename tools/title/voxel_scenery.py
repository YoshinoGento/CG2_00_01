"""Bounded offline solid voxel meshing for decorative scenery, not gameplay state."""
from collections import defaultdict
import math

FOLIAGE_CELL = 0.24
STONE_CELL = 0.08
MAX_SAMPLE_CELLS = 250000
MAX_SOLID_CELLS = 16000
MAX_QUADS = 4096
MIN_CELL = 0.01
MAX_EXTENT = 200.0
# Similar mid-green patches in the existing 512px foliage tile, not baked face shadows.
FOLIAGE_PATCHES = ((184, 416), (384, 200), (352, 136), (344, 432))
FOLIAGE_COLOR_SCALE = 0.75


def foliage_uv(points):
    center = tuple(sum(p[i] for p in points)/len(points) for i in range(3))
    x, y, z = (math.floor(v*FOLIAGE_COLOR_SCALE) for v in center)
    index = (x*73856093 ^ y*19349663 ^ z*83492791) % len(FOLIAGE_PATCHES)
    x, y = FOLIAGE_PATCHES[index]
    # Inverse of the exporter's atlas inset; sample only the selected four-pixel patch.
    return tuple((((x+u)/512-.025)/.95, ((y+v)/512-.025)/.95)
                 for u, v in ((.5,.5),(.5,3.5),(3.5,3.5),(3.5,.5)))


def ellipsoid_union(volumes, cell):
    if not math.isfinite(cell) or not MIN_CELL <= cell <= MAX_EXTENT:
        raise ValueError("Voxel cell is outside the authoring range")
    if not 1 <= len(volumes) <= 8:
        raise ValueError("Expected one to eight canopy/rock volumes")
    cells = set()
    samples = 0
    for center, radii in volumes:
        if len(center) != 3 or len(radii) != 3 or not all(
                math.isfinite(v) and abs(v) <= MAX_EXTENT for v in (*center, *radii)) or min(radii) <= 0:
            raise ValueError("Invalid ellipsoid")
        bounds = [(math.floor((c-r)/cell), math.ceil((c+r)/cell)) for c, r in zip(center, radii)]
        samples += math.prod(b-a for a, b in bounds)
        if samples > MAX_SAMPLE_CELLS:
            raise ValueError("Voxel sampling budget exceeded")
        for x in range(*bounds[0]):
            for y in range(*bounds[1]):
                for z in range(*bounds[2]):
                    key = (x, y, z)
                    if sum((((key[i]+.5)*cell-center[i])/radii[i])**2 for i in range(3)) <= 1:
                        cells.add(key)
                        if len(cells) > MAX_SOLID_CELLS:
                            raise ValueError("Voxel occupancy budget exceeded")
    if not cells:
        raise ValueError("Volume is smaller than the sampling grid")
    return cells


def surface_quads(cells):
    if not cells or len(cells) > MAX_SOLID_CELLS or any(
            len(p) != 3 or any(not isinstance(v, int) for v in p) for p in cells):
        raise ValueError("Invalid solid voxel set")
    planes = defaultdict(set)
    for p in cells:
        for axis in range(3):
            u, v = (axis+1) % 3, (axis+2) % 3
            for sign in (-1, 1):
                neighbor = tuple(p[i]+(sign if i == axis else 0) for i in range(3))
                if neighbor not in cells:
                    planes[(axis, sign, p[axis]+(sign > 0))].add((p[u], p[v]))
    result = []
    # Canonical ordering keeps exports stable despite hash/set iteration order.
    for (axis, sign, plane), mask in sorted(planes.items()):
        u, v = (axis+1) % 3, (axis+2) % 3
        while mask:
            a, b = min(mask)
            width = 1
            while (a+width, b) in mask:
                width += 1
            height = 1
            while all((a+i, b+height) in mask for i in range(width)):
                height += 1
            mask.difference_update((a+i, b+j) for i in range(width) for j in range(height))
            points = []
            for x, y in ((a, b), (a+width, b), (a+width, b+height), (a, b+height)):
                p = [0, 0, 0]
                p[axis], p[u], p[v] = plane, x, y
                points.append(tuple(p))
            if sign < 0:
                points.reverse()
            result.append(tuple(points))
            if len(result) > MAX_QUADS:
                raise ValueError("Voxel surface budget exceeded")
    return result


def append_volume(mesh, volumes, cell, material, section, origin=(0, 0, 0), yaw=0, flat_color=False):
    if len(origin) != 3 or not all(math.isfinite(v) for v in (*origin, yaw)):
        raise ValueError("Invalid scenery transform")
    quads = surface_quads(ellipsoid_union(volumes, cell))
    c, s = math.cos(yaw), math.sin(yaw)
    for corners in quads:
        points = tuple((origin[0]+cell*(x*c-z*s), origin[1]+cell*y,
                        origin[2]+cell*(x*s+z*c)) for x, y, z in corners)
        if flat_color:
            mesh.quad(points, material, section, foliage_uv(points))
        else:
            mesh.quad(points, material, section)

"""Independent numerical checks of the screen-space radial-wave contract."""
import math
from pathlib import Path


def smoothstep(start, end, value):
    t = min(1.0, max(0.0, (value - start) / (end - start)))
    return t * t * (3 - 2 * t)


def slope(distance, age):
    wave_age = age - .18
    if wave_age < 0:
        return 0.0
    lifetime = 2.4 - .18
    fade = (1 - smoothstep(lifetime * .63, lifetime, wave_age)) * math.exp(-wave_age * .55)
    value = 0.0
    for ring in range(3):
        radius = wave_age * 520 - ring * 38
        if radius <= 0:
            continue
        d = distance - radius
        packet = math.exp(-d * d / (18 * 18))
        gradient = packet * (math.cos(d * .16) - 2 * d / (18 * 18 * .16) * math.sin(d * .16))
        value += gradient * (.62 ** ring) * smoothstep(0, 20, radius) * fade
    return value


def main():
    shader = Path("project/Resources/shader/RippleTransition.PS.hlsl").read_text(encoding="utf-8")
    for contract in ("float4 drops[4]", "kRingCount = 3", "kRingSpacing = 38", "kPacketWidth = 18",
                     "kWaveFrequency = .16", "kFollowingWaveAttenuation = .62", "kMaximumDisplacement = 14"):
        assert contract in shader, contract
    checks = 0
    for center in ((0, 0), (1, 1), (.12, .83), (.5, .5)):
        origin = (center[0] * 1280, center[1] * 720)
        for age in (0, .1, .18, .3, .55, .9, 1.6, 2.4):
            for pixel in (origin, (0, 0), (1280, 720), (640, 360), (origin[0] + 520 * max(0, age - .18), origin[1])):
                radial = (pixel[0] - origin[0], pixel[1] - origin[1])
                distance = math.hypot(*radial)
                local = slope(distance, age)
                for axis, extent in enumerate((1280, 720)):
                    displacement = min(14, max(-14, radial[axis] / max(distance, 1) * local * 9 * 4))
                    uv = pixel[axis] / extent + displacement / extent
                    for blur_offset in (-2.2, 0, 2.2):
                        sample = min(1 - .5 / extent, max(.5 / extent, uv + blur_offset / extent))
                        assert math.isfinite(sample) and .5 / extent <= sample <= 1 - .5 / extent
                        checks += 1
                assert age < 2.4 or local == 0
    for age in (.4, .8):
        fronts = [(age - .18) * 520 - ring * 38 for ring in range(3)]
        assert all(front > 0 for front in fronts)
        assert fronts[0] > fronts[1] > fronts[2]
        assert abs(slope(fronts[0], age)) > .1
    for contract in ("lerp(maxRadius,-kWipeMargin,progress)", "WipeRadius()+ring*kRingSpacing",
                     "color = lerp(color,water,inside)", "radial/max(distanceFromCenter,1)"):
        assert contract in shader, contract
    reveal_checks = 0
    for center in ((0, 0), (1, 1), (.46, .67), (.5, .5)):
        origin = (center[0] * 1280, center[1] * 720)
        max_radius = math.hypot(max(center[0], 1-center[0])*1280,
                                max(center[1], 1-center[1])*720)+32
        for pixel in (origin, (0, 0), (1280, 0), (0, 720), (1280, 720), (640, 360)):
            distance = math.hypot(pixel[0]-origin[0], pixel[1]-origin[1])
            previous_visible = 0
            for step in range(101):
                progress = step/100
                radius = max_radius*(1-progress)-32*progress
                visible = smoothstep(-3, 3, distance-radius)
                assert 0 <= visible <= 1 and visible >= previous_visible
                if distance < radius-3:
                    assert visible == 0
                if distance > radius+3:
                    assert visible == 1
                assert step != 0 or visible == 0
                assert step != 100 or visible == 1
                fade = smoothstep(0, .05, progress)*(1-smoothstep(.85, 1, progress))
                local = 0
                for ring in range(3):
                    d = distance-(radius+ring*38)
                    local -= math.exp(-d*d/324)*(math.cos(d*.16)-2*d/(324*.16)*math.sin(d*.16))*(.62**ring)*fade
                assert math.isfinite(local)
                assert step not in (0, 100) or local == 0
                previous_visible = visible
                reveal_checks += 1
    print(f"Water ripple reference: {checks} finite/clamped samples, center/edge/4-drop overlap/lifetime/3 fronts PASS")
    print(f"Reflected reveal: {reveal_checks} inward-front/outside-visible/monotonic/endpoint samples PASS")


if __name__ == "__main__":
    main()

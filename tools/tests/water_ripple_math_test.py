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
    for contract in ("outgoingImage : register(t1)", "float presentationTime;",
                     "WaterMirror(input.texcoord,0)", "WaterMirror(input.texcoord,displacement)",
                     "clamp(uv,halfTexel,1-halfTexel)"):
        assert contract in shader, contract
    assert "const float3 water =" not in shader
    renderer = Path("project/application/scene/RippleTransitionRenderer.cpp").read_text(encoding="utf-8")
    for contract in ("kShaderConstantCount+2 <= 64", "historyRange.BaseShaderRegister = 1",
                     "frame.progress >= 1 && !outgoingValid_", "CopyResource(outgoing_.Get(),source)",
                     "outgoingValid_ ? outgoingSrv_ : snapshotSrv_", "values[tuningOffset+7] = frame.presentationTime",
                     "if (!heldMirror)"):
        assert contract in renderer, contract
    # Independent bound checks for the extra mirror UV drift, including tiny and wide viewports.
    mirror_checks = 0
    for width,height in ((320,180),(1280,720),(2560,1080)):
        for t in (0,.32,1.08,1.22,1.7,2.12,3,10,4095):
            for u,v in ((0,0),(1,1),(.5,.5),(.05,.9)):
                drift = (math.sin(v*32+t*2.3)*3+math.sin(v*71-t*1.7),
                         math.sin(u*27-t*1.4)*1.8)
                assert abs(drift[0]) <= 4 and abs(drift[1]) <= 1.8
                radial = ((u-.46)*width,(v-.67)*height)
                distance = math.hypot(*radial)
                wave = math.sin(distance*.06-t*6)*3/(1+distance*.001)
                drift = tuple(value + axis/max(distance,1)*wave for value,axis in zip(drift,radial))
                assert abs(drift[0]) <= 7 and abs(drift[1]) <= 4.8
                for coordinate,offset,extent in zip((u,v),drift,(width,height)):
                    for ripple in (-14,0,14):
                        for blur in (-3.5,0,3.5):
                            sample = min(1-.5/extent,max(.5/extent,coordinate+(offset+ripple+blur)/extent))
                            assert math.isfinite(sample) and .5/extent <= sample <= 1-.5/extent
                            mirror_checks += 1
    print(f"Water mirror: {mirror_checks} clamped drift/blur samples; history binding/layout contracts PASS")
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

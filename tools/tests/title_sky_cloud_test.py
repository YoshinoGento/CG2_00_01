"""Reference blend checks and a source-order guard, not GPU readback."""
from pathlib import Path
import math

ROOT = Path(__file__).resolve().parents[2]
shader = (ROOT/"project/Resources/shader/TitleSky.PS.hlsl").read_text(encoding="utf-8")
main = shader[shader.index("float4 main("):]
cloud_blend = "color = lerp(color,cloudColor.rgb,cloudOpacity);"
assert main.count(cloud_blend) == 1
assert main.index(cloud_blend) > main.index("color += star")
assert main.index(cloud_blend) > main.index("color = lerp(color,MoonSurface")
assert main.index(cloud_blend) > main.index("color = lerp(color,float3(1.0,.82,.36),sunDisc)")
assert "float cloudOpacity = cloud*smoothstep(-.04,.16,direction.y);" in main
assert "*(1-cloud)" not in main
assert main.index(cloud_blend) < main.index("OutputDither(uint2(input.position.xy))")


def smoothstep(a, b, value):
    t = min(1, max(0, (value-a)/(b-a)))
    return t*t*(3-2*t)


def composite(background, foreground, opacity):
    return tuple(a+(b-a)*opacity for a,b in zip(background,foreground))


cloud_tint = (.52,.61,.70)
for background in ((1,.82,.36),(.83,.85,.86),(.13,.24,.40)):
    assert composite(background,cloud_tint,0) == background
    assert max(abs(a-b) for a,b in zip(composite(background,cloud_tint,1),cloud_tint)) < 1e-7
    previous_error = math.inf
    for density_step in range(101):
        density = density_step/100
        result = composite(background,cloud_tint,density)
        assert all(math.isfinite(c) and 0 <= c <= 1 for c in result)
        error = sum(abs(c-b) for c,b in zip(result,cloud_tint))
        assert error <= previous_error+1e-7
        previous_error = error
        for elevation in (-1,-.04,0,.08,.16,1):
            opacity = density*smoothstep(-.04,.16,elevation)
            assert 0 <= opacity <= 1
assert smoothstep(-.04,.16,-.04) == 0
assert smoothstep(-.04,.16,.16) == 1
print("PASS title cloud: foreground order, opaque/clear/thin blend, monotonic transmission, horizon bounds; CPU reference only")

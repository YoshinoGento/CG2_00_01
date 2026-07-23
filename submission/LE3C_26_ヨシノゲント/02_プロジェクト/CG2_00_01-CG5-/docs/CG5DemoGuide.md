# CG5 PostEffect Demo Guide

## Purpose

Use the existing Farm gameplay scene to demonstrate the required Grayscale PostEffect without adding a second rendering path.

## Operation

1. Start the GamePlay Scene.
2. Open `Window > Fullscreen PostEffect` if the bottom tab is closed.
3. Select `Original Color` and record the original scene.
4. Select `Required Grayscale` and record the same scene and camera position.
5. Select `Grayscale` in `Fullscreen Effect` and adjust `Grayscale Amount` when showing parameter control.

`Required Grayscale` disables PostEffect Chain Mode so another pass cannot overwrite the required single-effect comparison.

The Release build starts directly in the GamePlay Scene. Press `Space` to cycle the
deterministic submission presets. The current preset is shown inside the game image
as the only Release HUD line and is also shown in the window title. Farm and Stage
Clear HUD text is hidden in this submission path. A small `SPACE : NEXT` hint remains
inside the same contrast panel. Selecting `Dissolve` appends its
progress to the same line and automatically advances the threshold from `0%` to
`100%` over four seconds. The label is a display-space overlay drawn after PostEffect
completion, so the demonstrated effect cannot erase its own name.
The final two presets intentionally use different visual signatures: Dissolve has a
wide bright orange removal edge, while RandomNoise applies strong coarse animated
grain to the whole surviving image.

## Scored Effect Quick Checks

The `Scored Effects` section exposes the connected rubric effects without requiring the evaluator to search the full Effect combo.

| Button | Expected rubric item | Demo focus |
|---|---:|---|
| `Vignette (+3)` | Vignetting | darkened edges and center focus |
| `BoxFilter (+3)` | BoxFilter | stable 3x3 neighborhood blur |
| `Gaussian Filter (+5)` | GaussianFilter | normalized 5x5 Gaussian kernel and adjustable sigma |
| `Luminance Outline (+5)` | LuminanceBasedOutline | color-luminance edges |
| `Depth Outline (+8)` | DepthBasedOutline | depth discontinuities using the depth SRV |
| `RadialBlur (+5)` | RadialBlur | center-directed motion |
| `Dissolve (+4)` | Dissolve | automatic 0% to 100% noise threshold sweep and colored edge |
| `RandomNoise (+4)` | Random | animated scene noise |

Each quick check disables Chain Mode and applies deterministic parameters. After selecting a preset, use the context-sensitive parameter section to demonstrate that the result is adjustable.

GaussianFilter uses the same fullscreen `t0` / `s0` / `b0` contract as the other
single passes. Its sigma is clamped to `0.1 .. 4.0`.

## Rendering Flow

```text
Farm Scene
  -> SceneRenderTexture (Linear)
  -> Selected scored PostEffect
  -> PostEffectResultTexture (Linear)
  -> LinearToSRGB (once)
  -> FinalDisplayTexture
  -> Game Viewport / Swapchain
```

The PostEffect workspace creates no GPU resource, Descriptor, PSO, or additional draw call. It sends typed UI actions to `EditorShell`; `PostEffectSystem` owns Effect selection, parameter validation, and deterministic demo presets.

## Submission Evidence

- one original-color screenshot
- one full Grayscale screenshot from the same camera
- a short video changing `Grayscale Amount`
- one before/after pair for every claimed scored Effect
- a short clip showing each quick-check button and one adjustable parameter
- a Release clip cycling the scored presets with `Space`
- README entries naming the shader, C++ control path, and game usage
- Debug/Release x64 build results with zero warnings/errors

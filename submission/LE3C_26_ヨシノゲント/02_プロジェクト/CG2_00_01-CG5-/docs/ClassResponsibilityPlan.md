# Class Responsibility Plan

## Goal

Keep classes small enough to understand, but split them by ownership and reason to change rather than by line count alone.

## Comment Policy

Comments should explain only information that code cannot express clearly:

- ownership and lifetime
- processing order and invariants
- simulation time versus real time
- DX12 `ResourceState`, Descriptor, Fence, and CPU/GPU synchronization
- non-obvious validation or performance constraints

Do not comment assignments, obvious branches, or function names. Prefer a precise name over a comment.

```cpp
// Draw still reads the buffer as an SRV while simulation is paused.
TransitionResource(..., D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
```

## Current Risks

| Area | Approx. size | Current responsibilities | Risk |
|---|---:|---|---|
| `GamePlayScene.cpp` | 2000 lines | Scene flow, camera, Level presentation, Farm scheduling, particle debug, test objects, debug UI | Very high |
| `ParticleManager.cpp` | 1240 lines | CPU particles, GPU resources, compute setup, emit, update, draw, interaction | High |
| `FarmDocumentSystem.cpp` | 860 lines | Validation, catalog, file I/O, atomic save, rename/delete | Medium |
| `DirectXCommon.cpp` | 660 lines | DX12 foundation plus PostEffect-specific constant buffers | High |
| `EditorShell.cpp` | 610 lines | Dock layout, window lifetime, command routing, shortcuts | Medium |
| `GamePlayEditorBridge.cpp` | 580 lines | Farm, camera, object, particle, visibility, simulation editor commands | Medium |

The main problem is not the number itself. `GamePlayScene` and `ParticleManager` have several independent reasons to change and therefore broad regression surfaces.

## Target Boundaries

```text
GamePlayScene
  -> SceneCameraController
  -> LevelPresentationSystem
  -> ParticleDebugController
  -> SceneTestObjectSystem
  -> existing Farm Systems

ParticleManager facade
  -> CpuParticleSystem
  -> GpuParticleSystem
  -> GpuParticleInteractionSystem

DirectXCommon
  -> DX12 device/queue/swapchain/fence only

PostEffectSystem
  -> PostEffect parameter buffers
```

`GamePlayScene` should keep only initialization order, update scheduling, draw scheduling, and scene transitions. Each extracted class must own its state instead of exposing many writable fields back to the Scene.

`ParticleManager` should remain a stable facade while its internals are separated. GPU buffers, SRV/UAV descriptors, state tracking, and PSOs must have one clear owner; splitting them across unrelated classes would make DX12 lifetime bugs more likely.

## Safe Extraction Order

1. Extract `SceneCameraController` because its input, position, rotation, and real-time delta form one closed responsibility.
2. Extract `ParticleDebugController` while keeping GPU resources in `ParticleManager`.
3. Extract `LevelPresentationSystem` for runtime objects, player visuals, route gizmos, and Level camera presentation.
4. Extract test Sphere/Ring/Cylinder objects from the production Scene path.
5. Move PostEffect-specific constant-buffer ownership out of `DirectXCommon` without changing RootSignature registers.
6. Split `ParticleManager` internally only after behavior tests cover emit, pause, interaction, and draw-state transitions.
7. Split Farm document validation/catalog/file I/O only if save tests protect rollback and atomic replacement.

Perform one extraction per build and runtime check. Do not combine these steps with PostEffect or Farm behavior changes.

## Avoid Micro-classes

Do not create a class merely to wrap one stateless forwarding function. A useful class should own at least one of these:

- mutable state with an invariant
- a resource lifetime
- a domain rule
- a processing phase
- a stable external boundary

This keeps files smaller without replacing one large class with dozens of empty pass-through objects.

## CG5 Readiness

The engine is ready to begin CG5 work, but it is not automatically submission-ready.

Already available:

- `Grayscale` for the required scene
- Vignette, BoxFilter, Blur/Gaussian, Luminance/Depth/Normal Outline
- RadialBlur, Dissolve, RandomNoise, HSVFilter, Bloom
- PostEffectChain, Ping/Pong textures, and final Linear-to-sRGB output
- ImGui controls for runtime comparison

Before submission:

1. Prepare one stable scene that visibly demonstrates Grayscale.
2. Select a small set of additional effects that are clearly visible and game-integrated.
3. Record before/after footage and parameter changes without debug clutter.
4. Document implementation files, equations, runtime use, and controls in `README.md`.
5. Reconfirm GammaCorrection occurs once, ResourceState transitions are valid, and Debug/Release builds have zero warnings/errors.

Architecture cleanup should continue, but it must not delay a stable CG5 demonstration build.

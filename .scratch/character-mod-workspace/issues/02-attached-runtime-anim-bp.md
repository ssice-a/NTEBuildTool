Status: ready-for-agent

# Attached mesh runtime AnimBP generator

## Goal

Generate attached mesh AnimBPs from `CharacterModSpec` / `AppearanceAssemblyPlan`.

Each attached runtime AnimBP should be able to:

- inherit the parent/main mesh pose when required;
- run KawaiiPhysics or future secondary-motion nodes in the AnimGraph;
- host simple runtime actions in the EventGraph when the attached mesh owns hotkey/UI switching;
- reference any Widget/SaveGame/action data assets required for cook reachability.

## Decisions

- Do not create a separate controller Blueprint by default.
- Do not make PostProcess AnimBP the new primary route.
- Keep the generated AnimBP contract small and inspectable.
- Runtime actions should come from one `CharacterModSpec.RuntimeActions` model, shared by hotkeys and UI buttons.

## Blockers

- Need concrete template/graph-generation design for CopyPose + Kawaii + output pose.
- Need Kawaii schema compatibility before final cooked physics AnimBPs are trusted.

## Tests

- BuildPlugin with strict includes.
- Commandlet inspection of generated AnimBP graph structure.
- Cook/package only after generated AnimBP compiles and is referenced by MeshAsset attached mesh data.

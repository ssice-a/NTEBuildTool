# ADR 0002: Package-first Pakmod Project architecture

Status: accepted

Date: 2026-08-01

Supersedes: the `CharacterModSpec`-first orchestration decision in ADR 0001. ADR 0001 remains historical evidence for the implemented authoring features and attachment research.

## Context

The plugin has successfully produced and packaged a character mod with imported meshes, material instances, runtime UI and hotkeys, SaveGame persistence, Kawaii assets, and a merged-main-mesh Post Process AnimBP. That proof also exposed an architectural problem: packaging currently derives its inputs by rebuilding Material, Runtime Action, Appearance, and Kawaii plans from one large `CharacterModSpec`.

This makes optional authoring automation part of the package Interface. A user who only wants to package manually authored UE assets still inherits authoring fields, validation, fallback branches, and commandlet flags. The modal Character Workspace also creates a fresh draft whenever it opens and closes to execute actions, so reopening or updating one feature can force the user to reconstruct unrelated state.

The plugin's durable purpose is narrower: package the UE assets selected by the user. Material creation, Blueprint generation, Kawaii setup, and source-game reconstruction are conveniences that should feed the package pipeline without defining it.

## Decision

### Package-first core

The package pipeline becomes a deep Module whose Interface is a Package Manifest plus build settings. It reads current on-disk UE assets and produces `.pak/.utoc/.ucas` outputs.

The Module must not:

- infer or execute authoring operations;
- rebuild Material, Runtime, Appearance, or Kawaii plans;
- mutate asset package paths during build;
- require FModel exports when all manifest assets already exist;
- reject a package because an unused optional authoring feature is incomplete.

`Apply Changes` and `Build Package` are separate commands. Apply may generate assets from Authoring Recipes. Build never invokes Apply implicitly.

### Persistent project model

`NTE.PakmodProject` version 1 replaces `CharacterModSpec` as the persistent model. It stores only:

- project identity and target game/mirror references;
- Game Source References and UE asset references;
- lightweight Authoring Recipes;
- the Package Manifest.

Derived fingerprints, last Apply/Build results, diagnostics, and caches live under `Saved/NTEBuildTool`. Most-recent-project and editor presentation state live in `EditorPerProjectUserSettings`.

### Resource classification

Every referenced asset is classified on two independent axes:

- Origin: `GameReference`, `UserImported`, or `ToolGenerated`;
- Intent: `ExternalReference`, `ReplacementAsset`, or `AddedAsset`.

No Origin implies an Intent. This prevents source-game references from entering a package merely because an authoring Recipe used them.

Staging an asset as a replacement is an explicit operation. It copies the asset to the exact game `/Game/...` package path and backs up any existing mirror asset. Package-time path remapping is rejected because UE package identity and serialized references are already fixed before IoStore packaging.

### Optional automation

Material, Runtime, Physics, Kawaii, and Post Process Blueprint automation remain supported as Recipe Modules. Users may skip them and package assets authored directly in UE.

Generated assets are Recipe-owned. Reapply may overwrite only the marked generated content owned by that Recipe. Users may Sync supported native UE edits back to Recipe deltas or detach an output from Recipe ownership.

When multiple Recipes contribute to a Blueprint, a Generated Blueprint Composer owns the write. Kawaii contributes marked AnimGraph declarations; Runtime contributes marked EventGraph declarations; Runtime UI contributes Widget and SaveGame declarations. Writers must not destroy declarations owned by other Recipes.

### Main Kawaii route

The accepted default for a merged character mesh is the existing main-mesh Post Process AnimBP route. Original game AnimBPs remain external and provide the base pose. The generated Post Process graph preserves original bones and initializes only topology-safe custom-chain anchors before evaluating ordered Kawaii nodes.

Direct replacement of original-game AnimBPs, `HTPlayerAppearance`, `PlayerUIShow` SCS, or game DataAssets is retained in research/manual documentation and Diagnostics. It is not integrated into the main Pakmod Project GUI in this architecture.

### Validation

Default blocking validation is deliberately small:

- package and filesystem paths are valid;
- manifest-owned UE assets exist;
- known package-breaking conditions are absent;
- cook and IoStore return success;
- outputs are fresh and non-empty.

Skeleton comparison, material-slot audits, exact AnimGraph topology, FModel provenance, container precedence, and character-specific assertions are opt-in Diagnostics. Diagnostics may explain risk but do not become hidden prerequisites for unrelated builds.

## Consequences

- Package planning no longer calls optional authoring plans. This increases Depth: one small manifest Interface covers manual and automated asset workflows.
- Authoring bugs gain Locality inside their Recipe Module and cannot block unrelated packaging.
- The GUI becomes one persistent non-modal tab rather than a modal action dispatcher.
- `CharacterModSpec` requires a one-way migration Adapter. After migration is proven, legacy runtime fallback and the broad flag matrix are deleted.
- Existing standalone tools can temporarily remain as advanced Adapters, but the normal menu exposes one Pakmod Project entry.
- Original-game appearance/AnimBP reconstruction remains available to expert workflows without making its fragile schema assumptions part of the main Interface.
- Formal automated tests must target the new Interfaces: project serialization/migration, Recipe planning, Blueprint composition ownership, manifest planning, and build output acceptance.

## Rejected alternatives

### Keep CharacterModSpec as the universal source of truth

Rejected because its Interface expands every time an optional authoring feature is added. Packaging then has to understand and validate fields unrelated to package construction.

### Package-time path remapping

Rejected because changing only container paths cannot safely rewrite UE package identity and serialized object references. Replacement ownership must be established before cook through Stage as Replacement.

### Remove authoring automation and keep only a package dialog

Rejected because material, runtime UI, hotkey, physics, and Kawaii automation remove substantial repetitive UE work and have already been proven. They remain optional deep Modules behind the project GUI.

### Continue generating each Blueprint independently

Rejected because Runtime and Kawaii can share an AnimBP. Whole-graph rewrites by independent writers destroy each other's work and make ordering part of the Interface. The Generated Blueprint Composer provides one ownership seam.

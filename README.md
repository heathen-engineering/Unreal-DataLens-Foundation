> **Moved to Codeberg:** this repo now lives at [codeberg.org/Heathen-Engineering/Unreal-DataLens-Foundation](https://codeberg.org/Heathen-Engineering/Unreal-DataLens-Foundation). GitHub will remain a read-only mirror during the transition.

# DataLens Foundation

![License](https://img.shields.io/badge/License-Apache_2.0-blue?style=flat-square)
![Maintained](https://img.shields.io/badge/Maintained%3F-yes-green?style=flat-square)
![Unreal](https://img.shields.io/badge/Unreal-5.8%20%2B-%23313131?style=flat-square&logo=unrealengine&logoColor=white)
[![Native](https://img.shields.io/badge/Native-C%2B%2B17%20core-lightgrey?style=flat-square)](https://github.com/heathen-engineering/DataLens)

A cache-aware, column-oriented in-memory simulation store for Unreal Engine. Entities are rows, attributes are bit-packed columns, and updates run as branchless passes over native memory — a `UGameInstanceSubsystem` binding straight onto the DataLens Core C ABI, no P/Invoke marshalling layer in the way.

-----

## 🛠 Also Available For
[![Unity](https://img.shields.io/badge/Unity-2021.3%20%2B-%23313131?style=for-the-badge&logo=unity&logoColor=white)](https://github.com/heathen-engineering/Unity-DataLens-Foundation)

-----

## ⚙ Built on the DataLens Core

This plugin is the Unreal binding over the engine-agnostic native [**DataLens Core**](https://github.com/heathen-engineering/DataLens) (C++17, Linux/Windows), vendored as a git submodule at `extern/DataLens`. The Core is where portability lives; each engine ships its own thin Foundation over it. Same substrate powers [DataLens Foundation (Unity)](https://github.com/heathen-engineering/Unity-DataLens-Foundation) and [HATE](https://github.com/heathen-engineering/Unity-Heathen-Attribute-gpTag-Engine-Foundation) on top.

-----

## Become a GitHub Sponsor
[![Discord](https://img.shields.io/badge/Discord--1877F2?style=social&logo=discord)](https://discord.gg/6X3xrRc)
[![GitHub followers](https://img.shields.io/github/followers/heathen-engineering?style=social)](https://github.com/heathen-engineering?tab=followers)  
Support Heathen by becoming a [GitHub Sponsor](https://github.com/sponsors/heathen-engineering). Sponsorship directly funds the development and maintenance of free tools like this, as well as our game development [Knowledge Base](https://heathen.group/) and community on [Discord](https://discord.gg/6X3xrRc).

Sponsors also get access to our private SourceRepo, which includes developer tools for O3DE, Unreal, Unity, and Godot.  
Learn more or explore other ways to support @ [heathen.group/kb](https://heathen.group/kb/do-more/)

-----

## What it does

DataLens is a data-oriented simulation database. Instead of one `UObject` per entity, state lives in column-major native tables (GameplayTag-addressed), and a consumer works entirely through `UDataLensSubsystem` — a `UGameInstanceSubsystem` wrapping the Core's `dl_store`/`dl_lens` C ABI directly, no vendored prebuilt binary. The public surface:

| Member | Purpose |
|--------|---------|
| `CreateStore(Columns, PreallocRows)` | Declare the database — a set of typed columns (`FDataLensColumn`, built via `OfFloat`/`OfInt32`). |
| `AllocRow` / `FreeRow` / `SetValid` / `IsValid` | Row lifecycle within the store. |
| `SetFloat` / `GetFloat` / `SetInt32` / `GetInt32` | Per-cell reads/writes, by raw `uint64` column id or by `FGameplayTag` convenience overload. |
| `CreateView(ColumnIds)` | Opens a read-only multi-column `FDataLensView` snapshot. |
| `RunSystemFloat(If)` / `RunSystemInt32(If)` / `RunSystemColumnFloat(If)` / `RunSystemCurvedFloat(If)` | Parallel column-wide `Set`/`Add`/`Sub`/`Mul`/`Min`/`Max` ops, unconditional, predicated, cross-column, or curve-transformed. |
| `GetRevision` / `SetRevision` / `BumpRevision` / `MarkColumnDirty` | Per-store replication revision tracking. |
| `Snapshot` / `CollectDelta(SinceRevision)` / `ApplyPayload` | Replication provider: a full baseline, a since-revision delta, and applying either as an authoritative commit — the wire primitives a netcode stack consumes; this plugin opens no socket and dictates no topology. |

> No wrapper tag type — raw `uint64` is the one true column/tag identity throughout the API. `FGameplayTag` stays the only real tag concept in Unreal (hierarchy, picker, authoring); every tag-taking method also has a canonical `uint64` overload, with `HashDataLensTag(FGameplayTag) -> uint64` as the one-line stateless derivation (`XXH3_64bits_withSeed(..., 0)` over `FGameplayTag::ToString()` — bit-identical to Unreal's own native `FXxHash64`, and to every other engine's GameplayTags/Lexicon hash).

### Blueprint surface

`UDataLensBlueprintLibrary` mirrors nearly all of the above as `UFUNCTION(BlueprintCallable)` static wrappers, taking `FGameplayTag` directly (already Blueprint-native) and reinterpreting every `uint64` as `int64` at the boundary (UHT can't reflect `uint64` in a function signature at all). `UDataLensSubsystem` itself carries `UCLASS(BlueprintType)` so Blueprint can hold a reference obtained from "Get Game Instance Subsystem," but has no `UFUNCTION`s of its own — all Blueprint-callable boundary conversion lives in the library, not the subsystem class. Not wrapped: `CreateView`/`FDataLensView` — move-only with a template `GetRows<TRow>()` zero-copy accessor, no Blueprint equivalent without a separate copying/marshalling design; `SetFloat`/`GetFloat` already cover single-cell Blueprint access.

-----

## Requirements

- Unreal Engine **5.8** or compatible
- Linux or Win64, **x86_64**
- No plugin dependencies — links directly against the vendored `extern/DataLens` submodule

-----

## Installation

1. Clone with submodules (the Core is vendored as a git submodule, not a prebuilt binary):
   ```
   git clone --recurse-submodules https://github.com/heathen-engineering/Unreal-DataLens-Foundation.git
   ```
   Already cloned without `--recurse-submodules`? Run `git submodule update --init` from the repo root.
2. Copy (or symlink) the repo into your project's `Plugins/` folder, or reference it as an engine plugin.
3. Enable **DataLens Foundation** in `Edit > Plugins`, restart the editor.
4. Get `UDataLensSubsystem` via `GetGameInstance()->GetSubsystem<UDataLensSubsystem>()` (C++) or "Get Game Instance Subsystem" (Blueprint).

-----

## Enums

| Type | Values |
|------|--------|
| `EDataLensSystemOp` | `Set`, `Add`, `Sub`, `Mul`, `Min`, `Max` |
| `EDataLensCompareOp` | matches Core's comparison set for predicated Systems |
| `EDataLensCurveType` | `Linear`, `Power`, `Smoothstep`, `Threshold` |
| `EDataLensColumnType` | the typed column kinds `FDataLensColumn` can declare |

Each has a `...BP` (`UENUM(BlueprintType)`) twin in `DataLensBlueprintLibrary.h`, positionally mirroring the core enum via `static_cast` at the boundary — the same convention the Core's own `c_api.h` uses.

-----

## License

Apache 2.0 — see [`LICENSE`](LICENSE). Heathen Engineering Limited, Irish Registered Company #556277.

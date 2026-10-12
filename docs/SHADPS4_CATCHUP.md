# shadPS4 catch-up (branch `feature/shadps4-catchup`)

Our renderer was taken from shadPS4 `d1cb90db` (1 October 2026) and about 100 of its 460 files
carry bbport changes, so upstream commits are ported one by one, in order, onto those changes.
This branch is **not merged**: it changes the shader recompiler and the pipeline cache format,
and waits until the shader capture for the pre-build is finished (docs/ROADMAP.md, item 4).

## Ported (each commit keeps its upstream author)

| shadPS4 | What |
|---|---|
| `0f6d1b87` #5212 | Slot vectors reserve their range and never move entries (callers use the new API) |
| `7e778987` #5196 | Mips smaller than a macro tile are sized and detiled micro tiled (also Supermedo #42) |
| `c4952908` #5257 | NaN handling from the shader's DX10 clamp / IEEE modes (sirit `fc2b42d`, OpNClamp) |
| `7721a1ef` #5260 | The instance ID is unbiased for every use, not only v3 |
| `c5dd8b8b` #5197 | Zero/one MIN/MAX blend factors emulated |
| `c679ce60` #5229 | Added depth-overlap subresources initialised |
| `14ada48a` #5283 | Pending fast clears applied on FMASK decompress |
| `dade3afd` #5272 | Image copy/clear fast path with a third flat buffer |
| `945dbc3c` | Subgroup barriers for shared memory dependencies |
| `1c2b8f97` #5346 | V_RCP_CLAMP / V_RSQ_CLAMP / V_RSQ_LEGACY / V_LOG_CLAMP clamping |
| `3a9f5726` #5344 | IMAGE_GATHER4_C_L |
| `8bafaa5a` #5352 | Another sharp patching pattern (BufferResource gains a dw2 shift) |
| `643c7997` #5345 | Vertex and instance IDs unbiased in every indirect draw |
| `69b6041d` | The stencil plane of DepthStencilCopy destinations is linked |

Then one version bump for the pipeline cache: shader binary 8, shader meta 9 (8 belongs to the
pre-build branch), pipeline key 6.

## Deferred, and why

- `8e23388a` #5219 (texture cache speed-ups, 27 files): conflicts in 15 files with the fork's
  texture cache (collector, memo, GPU-written images). `145932bf`, `e7929178` and `995e37e4`
  (CMask metadata) build on its layout. Port as one piece later, with game testing.
- `99e33b7e` (region manager, lock-free check): written against a region manager the fork has
  changed (64 KiB page tracking). Speed-up only.
- `00276961` #5278 (index buffer capacity for indirect draws): upstream keeps the index buffer
  size outside the registers and makes VK_KHR_maintenance6 mandatory. The fork's decoder
  snapshots the registers per draw for its recording thread, and the register block mirrors the
  hardware layout, so the value needs a snapshot field of its own; maintenance6 must stay
  optional (older drivers).
- `235dcabf` #5241 (RCPPS / RSQRTPS on Intel, the dark spots): the game has 5,795 such
  instructions, almost all 4 bytes long, too short for a jump. shadPS4 patches them with its
  function-discovery and relocation pass; the fork's loader has no equivalent yet. A port means
  building that relocation (or a trap-based fallback) for the loader, plus the two AMD lookup
  tables (zstd, from shadPS4's resources).

## Before merging

1. The capture is finished and the pre-build branch merged.
2. One more cache version above both branches' numbers.
3. The capture's catalog (`.meta` v8 records) converted: the shader info is stored as raw
   `InfoPersistent` bytes, and `BufferResource` grew by four bytes here, so every record is
   rewritten with the new layout (a small script beside `scripts/shader_catalog.py`, checked by
   translating the converted catalog with `bb-shader-prebuild`).
4. Testing in play: the Research Hall, shadowed areas, character creation, loading screens,
   and a new-area walk with an empty cache.

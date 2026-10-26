STATUS: DONE

# Bad Piggies Rebooted - managed exception triage (port-caused vs mod bug)

Scope: the three runtime C# problems seen in `UnityPlayer.log` of the Surface 2 (Tegra 4, FL 9_1) run. Static analysis only: no ARM binary was executed,
the game folder / appx / repo source / shader table were not modified (only this file was written). Tooling was ad-hoc Python in the session scratchpad
(IL2CPP metadata v24 reader, Unity-17 serialized-file reader, capstone Thumb-2 disassembler over GameAssembly.dll). The recipes below are enough to redo it.

NOTE: the `E:\BadPiggiesRebooted_x0xr9mv5edyrp` package-data folder went offline mid-analysis. `TempState/UnityPlayer.log` and `AC/l9shim.log` had already been read in full;
`LocalState/*` (Progress.dat, the 640-byte saved `*.contraption`) were only listed, not read.

## TL;DR

| # | Exception | Verdict | Could the 2 missing shaders change it? |
|---|---|---|---|
| 3 | Missing scripts `BackgroundBox` x2, `REConsoleInterface` | **MOD BUG** (certain). 3 broken MonoBehaviours in the whole game, all content defects | No |
| 1 | `ArgumentOutOfRange` in `PartListing.GetGreyMaterial` | **MOD BUG** (certain). New atlas `IngameAtlas4` was never added to `AtlasMaterials`, plus an unsafe loop bound | No |
| 2 | `NullReferenceException` in `MenuContraptionManager.CreateContraption` | **Almost certainly not port-caused / mod-side** (high confidence on "not GPU related"); the exact null is NOT pinned (see section 3) | No |

Our side (shim / shaders / caps) does not need to act for any of the three. Fixes belong to the mod (add IngameAtlas4 to AtlasMaterials lists, fix the pass-2 loop bound, clear 2 dead components, drop the dead console prefab).
Only port-side symptom seen in the same log (not in scope, noted): `d3d11: failed to create 2D texture shader resource view id=41 [80070057]` at engine init, and the 2 shader MISS pairs.

## 0. Log timeline (UnityPlayer.log / l9shim.log)
1. `failed to create 2D texture shader resource view id=41 [80070057]` at init (port-side, separate).
2. `referenced script ... (Game Object 'BackgroundBox') is missing` x2 right after init, before any `Start()`.
3. `D3D shader create error vertex/pixel [80070057]` x2 pairs == the 2 MISS pairs in l9shim.log (VS1408/PS212, VS1508/PS408).
4. `referenced script (REConsoleInterface) ... missing` then `... Game Object 'REConsoleInterface' ...` via `Resources.Load(String)` <- `<Start>d__1:MoveNext` <- SetupCoroutine.
5. NRE `MenuContraptionManager.CreateContraption()` (main menu scene, single frame), the menu keeps working.
6. AOORE `List.get_Item <- PartListing.GetGreyMaterial <- ToGray <- CreatePartIcons <- CreateGUI <- OnEnable <- Instantiate <- PartListing.Create <- BaseGameMode.OnDataLoadedStart <- LevelManager.OnDataLoaded` at level start; the level then loads and plays.

## 1. Missing scripts (exception 3) - MOD BUG

Method: parsed all 80 serialized files in Data/ plus every AssetBundle (14,784 MonoBehaviours), resolved each `m_Script` PPtr to its MonoScript
(1,717 in globalgamemanagers.assets), and checked (namespace, class) against the 2,315 Assembly-CSharp typedefs (typedef 5882..8196) of global-metadata.dat.
**Exactly 3 MonoBehaviours are broken; 14,781 are fine.** They match the log 1:1:

| Object | Location | Defect |
|---|---|---|
| GO `BackgroundBox` (x2) | sharedassets0.assets MB pid 22476 (child of prefab `CrateCrazePopup`, GO 3465) and pid 22914 (child of `CoinSalePopup`, GO 348) | `m_Script` = (fileID 0, pathID 0): **null script baked into the asset**. The MB is a 32-byte husk (go ptr, enabled, null script, empty name, zero fields). The 57 other `BackgroundBox` objects have valid components (BoxCollider + Button/BackgroundMaskPosition or BackgroundScaler/Sprite); only these two popups carry an extra component whose script was deleted before the build. |
| GO `REConsoleInterface` | resources.assets MB pid 30987 (GO 2729) | MonoScript exists (globalgamemanagers.assets pid 1251: class `REConsoleInterface`, ns '', Assembly-CSharp.dll) but the **class is not in the IL2CPP metadata**: no "REConsole" substring anywhere in global-metadata.dat or GameAssembly.dll. |

Why REConsoleInterface is mod content and not us:
- The prefab is a runtime C# code console: Panel > Scroll View > Content > {ConsoleCode InputField, ConsoleOutput InputField, **CompileButton**, **RunButton**}.
  The metadata keeps only `GoggsMessaround.CSharpScriptEngine.ScriptCompilingException/ScriptExecutionException` (the engine itself is gone) and the other, working, command console
  `RECommandInterface` + `RECommandHandler` + `RECommand*` (input/output/execute/help). Runtime C# compilation cannot work under IL2CPP AOT, so the author left the class out of the IL2CPP build and left the prefab in Resources.
- Load chain (all verified): `SplashPlayer.<Start>d__1.MoveNext` (the only `<Start>d__1` in Assembly-CSharp; SplashPlayer is on level0's Main Camera) uses string literal #10314 `Innovation/INInitializer`
  (checked through the metadata usage lists) -> container entry `innovation/ininitializer` -> GO INInitializer (MB INInitializer) -> `m_resourceData` = ScriptableObject `ResourceData` (pid 24853) -> GO `INApplicationInterface`
  (MB `INAppInterface`) -> GO `REConsoleInterface`. The prefab is not Resources-addressable by itself, only as that dependency, which is why the warning appears inside that one `Resources.Load`.
- 204 other MonoScripts have no top-level metadata type; all are enums/interfaces/generic bases/delegates or non-player assemblies (Timeline, Networking, TestRunner, TMP editor), never referenced as `m_Script`.

Effect: harmless. The components are skipped; the in-game C# console does not exist; the two popups (Crate Craze / Coin Sale) lose a no-op husk. Script resolution is managed-loader work and never touches GPU objects,
so the same two warnings appear on any hardware with this appx. Shaders cannot change it.

## 2. `PartListing.GetGreyMaterial` AOORE (exception 1) - MOD BUG

Reverse engineered from GameAssembly.dll: `GetGreyMaterial` = methodIndex 44635 @ VA 0x1020fefc (methodPointers table at 0x1102c898, 50,884 entries; ImageBase 0x10000000; `List<T>.get_Item` @ 0x10524754 =
`if (idx >= _size) ThrowArgumentOutOfRange; return _items[idx]`). Reconstructed C#:

```
Material GetGreyMaterial(Material material) {
  var A = AtlasMaterials.Instance;                 // fields: normalMaterials(+0x14) renderQueueMaterials(+0x1c) grayMaterials(+0x24)
  for (i = 0; i < A.normalMaterials.Count; i++)    // pass 1
      if (material.name == A.normalMaterials[i].name)      { cache in normalMaterials-dict; return A.grayMaterials[i]; }
  for (i = 0; i < A.normalMaterials.Count; i++)    // pass 2: bound is normalMaterials.Count, but it INDEXES renderQueueMaterials
      if (material.name == A.renderQueueMaterials[i].name) { cache; return A.grayMaterials[i]; }
  cache; return this.grayMaterial;                 // fallback (+0x74) - only reachable if pass 2 does not overrun
}
```

Serialized `AtlasMaterials` prefab (resources.assets MB pid 28513): normalMaterials = **5** (IngameAtlas, IngameAtlas2, IngameAtlas3, MenuAtlas, MenuAtlas2),
renderQueueMaterials = **3** (Ingame/Ingame2/Ingame3_PartIconsRenderQueue), dimmedRenderQueue = 3, partQueueZ = 3, grayMaterials = 5, shine = 5, alpha = 5.
So any material whose name is not one of the 5 normal names runs pass 2 up to i=4 over a 3-item list and throws at i=3:
`Index was out of range. Must be non-negative and less than the size of the collection. Parameter name: index` straight out of GetGreyMaterial = the logged frame and message.

Which material: `CreatePartIcons` calls `ToGray(bg, gray = !CustomizationManager.IsPartUnlocked(part))` after re-parenting the part's icon under the frame `bg`; `ToGray` runs `GetGreyMaterial(r.sharedMaterial)` for every Renderer
under it (skipping GameObjects named NewTag / New / KingsFavorite), **only for locked parts**. The 7 frame prefabs in PartListing (regular/common/rare/epic/legendary/empty/selected) all use `MenuAtlas2` (normal[4]) -> fine.
The part icons are the problem: a scan of all `Icon*` objects shows every material matched **except `IngameAtlas4`, used by 38 part icons / 157 renderers**
(Icon_Engine_8, Icon_EngineBig_8, Icon_MetalFrame_131/132, Icon_WoodenFrame_11..13, Icon_Rope_5..8, Icon_PointLight_6..8, Icon_RailGun_1, Icon_Piston_01, Icon_Oscillator_1, Icon_LightningHook_01,
Icon_ForceFieldLight_01, Icon_TungstenCube_01, Icon_Irrational_133/135, Icon_ColoredFrame, ...). `IngameAtlas4` (Material sharedassets0.assets pid 22) is in none of the AtlasMaterials lists, and nothing registers it at runtime
(the only code that touches `AtlasMaterials.Instance` is 9 stock classes: AnimateColor, BackgroundMask, EpisodeButton, MaterialAnimation, ObjectiveSlot, PartCounter, PartListing, RewardView, SpriteShineEffect).
The mod added a 4th ingame atlas + new parts and extended the lists with MenuAtlas/MenuAtlas2 (so 5 vs 3), but never added IngameAtlas4 and never made pass 2 safe.

Why it is not ours: the inputs are CPU-side strings (Material.name) and serialized list lengths. Material/shader creation success cannot change a List.Count or a name. Trigger = locked part with an IngameAtlas4 icon, which is the
state of any fresh/low-progress save on any hardware; a save that has unlocked all those parts takes the `GetNormalMaterial` path instead. (IngameAtlas4 uses `_Custom/PreAlpha_Unlit_ColorTransparent_Geometry`, served by our
programs 46806b06c667 (PS) / a979d1ae7065 (VS) in l9_replacements.bin, like the other atlases; the 2 MISS shaders are unrelated.)
User-visible effect: `CreateGUI` aborts at the first such icon, so the in-game part list is incomplete in every level that lists those parts. Real gameplay bug, not a graphics one.

## 3. `MenuContraptionManager.CreateContraption` NRE (exception 2) - NOT PORT-CAUSED, exact null not pinned

Facts (methodIndex 44016 @ 0x101e81bc; field offsets: m_gameData 0x0c, m_menuContraptionControllerPrefab 0x10, m_contraptionAssets 0x14, m_contraptionIndex 0x18, m_contraption 0x1c, m_contraptionController 0x20, m_cameraTarget 0x2c):
- Reconstructed: `go = Instantiate(m_menuContraptionControllerPrefab.gameObject, transform, true); m_contraptionController = go.GetComponent<MenuContraptionController>();
  m_contraption = go.GetComponent<MenuContraptionController>().CreateContraption(m_contraptionAssets[m_contraptionIndex]); m_cameraTarget = m_contraption.FindPig().gameObject; if (++m_contraptionIndex >= Count) { index = 0; ShuffleContraptions(); }`
- Called from `Start()` as a tail call (`b.w`), which is why the trace is a single frame. A null inside a callee (MenuContraptionController.CreateContraption, BuildContraption, BuildPart, SetPartAt, GetPart/GetCustomPart, LoadContraptionDataset) would have added its own frame, so the null is in one of the manager's own checks.
- Inputs verified statically: serialized `m_gameData` -> GameData (sharedassets0 pid 18503) resolves; `m_menuContraptionControllerPrefab` -> MB MenuContraptionController on GO `MenuContraptionController` (sharedassets1 pid 2502) resolves;
  `Start` fills `m_contraptionAssets` from `Resources.LoadAll("MenuContraptions")` = 32 TextAssets (original XML datasets), **every one contains a pig (`partType="10"`)**; GameData.m_parts (46 base prefabs) has `Part_Pig_01_SET`;
  `m_customParts` has 45 entries, partType 10 has 19 pig variants, every variant MB is `Pig` with serialized partType 10; XmlSerializer interpreter types (`XmlSerializationReaderInterpreter` etc.) and the
  `ContraptionDatasetUnit` [XmlAttribute] fields are present in the build. So no missing-content explanation and nothing logged by the loader.
- Ruled out as the failing site: prefab null (resolves), Instantiate/transform null (cannot), `m_contraptionAssets` null (would NRE in Start, whose frame is absent), controller GetComponent null (prefab has the MB),
  controller.CreateContraption returning null (it NREs inside itself first). That leaves **`Contraption.FindPig()` (= `FindPart(PartType 10)`) returning null, i.e. the contraption was built without a pig / with an empty part list** (probable), with the only
  remaining in-frame alternative being a null GetComponent on the clone.
- Weak supporting hint (not proof): if the failing check is the FindPig one, `m_contraption` is already assigned and only `m_cameraTarget` stays unset. `Update` dereferences `m_contraption` unguarded in at least one state (before `Contraption.IsMovementStopped`, with an explicit null check that raises NRE), and the log shows no `Update` NRE, which fits that case better than a null `m_contraption`; the session may simply never have reached that state, so treat it as a hint only.
- Nothing in the path involves a GPU object: only Object/Transform/GetComponent/Instantiate, List indexing, `Resources.LoadAll`, `XmlSerializer.Deserialize(TextAsset.bytes)` and part-grid logic (plus `Screen.width/height` in the controller). A failed shader/texture/SRV creation leaves a Material/Texture object and an invisible mesh,
  it does not remove a part from `Contraption.m_parts`.
- Cannot be tied to shaders: there is no dependency on the 2 MISS programs; adding them cannot make `FindPig` non-null.

What is unproven: why the built contraption has no pig. Candidates (all mod/content/build side): the mod's menu build pipeline (`GameData.GetCustomPart` random variant replacement + `ContraptionDatasetUnit` got new fields customPartIndex/offsetX/offsetY), an
XmlSerializer-under-IL2CPP deserialisation that yields an empty `ContraptionDatasetList` silently (read-only collection property + protected `[XmlArray]` field), or a build-config issue. None needs the GPU.
Cheapest decisive test (does not need our code): run the same appx on any other ARM32/ARM64 Windows device (the POCO X3 Pro / Win11 phone or any real-GPU box) and look for the same `NullReferenceException ... MenuContraptionManager.CreateContraption` in its UnityPlayer.log;
or check whether the animated pig-vehicle appears behind the main menu (if absent, FindPig/empty-dataset is confirmed). Reading the saved `*.contraption` from the lost E: dump would also show whether XmlSerializer round-trips in this build.

## 4. What our side does / does not have to do
- Exceptions 1, 2, 3: nothing to fix in l9shim / l9_replacements / caps. They are present with any shader coverage. Do not spend shader work on them.
- Mod-side fixes (for the record, not ours): register IngameAtlas4 in AtlasMaterials (normal+gray+shine+alpha+renderQueue+dimmed+partQueueZ) and bound pass 2 by `renderQueueMaterials.Count`; remove or repair the null-script components on `CrateCrazePopup/BackgroundBox` and `CoinSalePopup/BackgroundBox`;
  drop the `REConsoleInterface` prefab (or compile the class in); investigate the menu-contraption build.
- Separate port-side items noticed in the same log: texture SRV creation failure (id=41, E_INVALIDARG) and the 2 shader MISS pairs (VS1408/PS212, VS1508/PS408).

## Appendix: addresses / recipes (GameAssembly.dll, ARM Thumb-2, ImageBase 0x10000000)
- Sections: .text 0x1000, `il2cpp` 0x13b000 (generated code, 0xc7c82c), .rdata 0xdb8000, .data 0x1235000.
- methodPointers: 50,884 Thumb entries at 0x1102c898 (index = metadata method `methodIndex`; many tiny accessors are ICF-folded so reverse name lookup by address is unreliable for 1-3 instruction getters).
- Il2CppMetadataRegistration at 0x10fe3190: types[50888] at 0x11066850 (Il2CppType = {data, bits}; kind = bits>>16 & 0xff; CLASS/VALUETYPE data = typedef index; field attrs = bits & 0xffff).
- Metadata v24 layouts used: typedef stride 100 (nameIndex, nsIndex, byval, byref, declaring, parent, element, rgctxStart, rgctxCount, genericContainer, flags, fieldStart, methodStart, eventStart, propStart, nestedStart, interfacesStart, vtableStart, ifaceOffsetsStart,
  8x u16 counts, bitfield, token); method stride 52; field stride 12 (name, type, token); image stride 40; Assembly-CSharp = image 48 (typedef 5882, 2315 types); string literals = (len, offset) pairs into stringLiteralData.
- Method -> string literals: the `il2cpp_codegen_initialize_method(idx)` argument is loaded from a .rdata word right before the call (movw/movt r3; ldr r0,[r3]); `metadataUsageLists[idx] = (start,count)` into `metadataUsagePairs (dest, enc)`, enc>>29 == 5 is a string literal.
- Key method indexes: PartListing GetGreyMaterial 44635, ToGray 44618, CreatePartIcons 44609, IsGrayMaterial 44636; MenuContraptionManager Start 44012, CreateContraption 44016, Update 44015;
  MenuContraptionController CreateContraption 44003, BuildContraption 44005; Contraption FindPig 38967, FindPart 38968, SetPartAt 39015; GameData GetPart 40621, GetCustomPart 40622; WPFPrefs LoadContraptionDataset 50505; AtlasMaterials get_Instance 37283.

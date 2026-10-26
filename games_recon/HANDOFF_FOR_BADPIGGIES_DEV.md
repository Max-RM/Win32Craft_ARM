# Bad Piggies Rebooted — three real C# bugs (platform-independent) + an optional low-end-GPU note

Hi! While getting *Bad Piggies Rebooted* running on some older hardware, we found three bugs in the shipped build that are worth fixing in your Unity project. **None of them are specific to our setup — they reproduce on any platform**, because every one is plain CPU-side C#/content logic (list lengths, string names, serialized prefabs), with no dependency on the GPU, graphics API, or device.

We only had the compiled IL2CPP build to work from, so we can describe the *logic* with certainty but can't quote your source line numbers. Each item below gives the runtime symptom, the root cause we reconstructed from the decompile, and a concrete fix.

Two of these (#1 and #2) are visible gameplay/UI bugs; #3 is harmless noise but easy to clear.

---

## Bug 1 — `PartListing.GetGreyMaterial` throws `ArgumentOutOfRangeException` (in-game part picker is incomplete)

**Symptom** (at level start, every level that lists the affected parts):

```
ArgumentOutOfRangeException: Index was out of range. Must be non-negative and less than the size of the collection.
Parameter name: index
  at System.Collections.Generic.List`1[T].get_Item (System.Int32 index)
  at PartListing.GetGreyMaterial (UnityEngine.Material material)
  at PartListing.ToGray (UnityEngine.GameObject go, System.Boolean gray)
  at PartListing.CreatePartIcons (...)
  at PartListing.CreateGUI ()
  at PartListing.OnEnable ()
  ...
```

Because `CreateGUI` aborts at the first offending icon, **the in-game part list is left incomplete** in any level that shows those parts.

**Root cause** — two things combine:

1. **An unsafe loop bound.** `GetGreyMaterial` has two search passes. Pass 2 loops with `AtlasMaterials.normalMaterials.Count` as its bound but **indexes `renderQueueMaterials`** inside the loop:

   ```csharp
   // pass 1: search normalMaterials, OK
   for (int i = 0; i < A.normalMaterials.Count; i++)
       if (material.name == A.normalMaterials[i].name) { ...; return A.grayMaterials[i]; }

   // pass 2: bound is normalMaterials.Count (5), but indexes renderQueueMaterials (3) -> throws at i == 3
   for (int i = 0; i < A.normalMaterials.Count; i++)
       if (material.name == A.renderQueueMaterials[i].name) { ...; return A.grayMaterials[i]; }
   ```

   In the shipped `AtlasMaterials`, `normalMaterials` has **5** entries (IngameAtlas, IngameAtlas2, IngameAtlas3, MenuAtlas, MenuAtlas2) while `renderQueueMaterials` has only **3** (Ingame/Ingame2/Ingame3_PartIconsRenderQueue). So any material whose name isn't one of the 5 normal names falls through pass 1, enters pass 2, and throws at `i == 3`.

2. **A material that was never registered.** The atlas that trips it is **`IngameAtlas4`**, used by **38 part icons / 157 renderers** (Icon_Engine_8, Icon_EngineBig_8, Icon_MetalFrame_131/132, Icon_WoodenFrame_11..13, Icon_Rope_5..8, Icon_PointLight_6..8, Icon_RailGun_1, Icon_Piston_01, Icon_Oscillator_1, Icon_LightningHook_01, Icon_ForceFieldLight_01, Icon_TungstenCube_01, Icon_Irrational_133/135, Icon_ColoredFrame, …). `IngameAtlas4` appears in **none** of the `AtlasMaterials` lists, and nothing adds it at runtime. It looks like the Rebooted content added a 4th ingame atlas and new parts, and extended some lists (MenuAtlas/MenuAtlas2 pushed `normalMaterials` to 5), but `IngameAtlas4` was never wired into `AtlasMaterials`.

**When it fires:** only for **locked** parts — `ToGray` is called with `gray = !CustomizationManager.IsPartUnlocked(part)`, and `GetGreyMaterial` runs per renderer only when graying. So it hits any fresh / low-progress save that still has those parts locked. (The 7 frame prefabs all use `MenuAtlas2`, which is in the lists, so frames are fine; it's the icons.)

**Fix** (any one is enough; doing all three is cleanest):

- **Register `IngameAtlas4`** in `AtlasMaterials` across the parallel lists (`normalMaterials` + the matching `grayMaterials`, `shine`, `alpha`, and, if it should also render-queue, `renderQueueMaterials`/`dimmedRenderQueue`/`partQueueZ`). Keeping the lists the same length is what the indexing assumes.
- **Fix the pass-2 bound** to `renderQueueMaterials.Count` (and likewise guard the shared `grayMaterials[i]` return so `i` is valid for it).
- **Make `GetGreyMaterial` fall back gracefully** when a material isn't found in either list (return the default gray material instead of running off the end).

---

## Bug 2 — `MenuContraptionManager.CreateContraption` throws `NullReferenceException` (main menu)

**Symptom** (once, on the main-menu scene; the menu keeps working afterward):

```
NullReferenceException: Object reference not set to an instance of an object.
  at MenuContraptionManager.CreateContraption ()
```

**Root cause (reconstructed):** `CreateContraption` builds the little display contraption shown on the menu and then grabs its pig as the camera target:

```csharp
var go = Instantiate(m_menuContraptionControllerPrefab.gameObject, transform, true);
m_contraptionController = go.GetComponent<MenuContraptionController>();
m_contraption = m_contraptionController.CreateContraption(m_contraptionAssets[m_contraptionIndex]);
m_cameraTarget = m_contraption.FindPig().gameObject;   // <-- NRE if FindPig() returns null
if (++m_contraptionIndex >= m_contraptionAssets.Count) { m_contraptionIndex = 0; ShuffleContraptions(); }
```

It's called as a tail-call from `Start()`, which is why the stack is a single frame. We verified statically that all the serialized inputs resolve (the GameData reference, the controller prefab, and all 32 `MenuContraptions` XML datasets — each of which *does* contain a pig, `partType="10"`). A null inside any callee (`CreateContraption`/`BuildContraption`/`BuildPart`/…) would have added its own stack frame, and none appears — so the null is one of this method's own dereferences. By elimination the most likely culprit is **`m_contraption.FindPig()` returning null**, i.e. the built contraption ended up with no pig / an empty part list.

We could not pin down *why* the contraption has no pig from the binary alone — it points at the menu-build path, which you can see directly in source. Likely candidates: the custom menu-build logic (random variant replacement via `GameData.GetCustomPart`, the extra `ContraptionDatasetUnit` fields like `customPartIndex`/`offsetX`/`offsetY`), or an `XmlSerializer` deserialization that quietly yields an empty dataset under IL2CPP AOT (watch for read-only collection properties / `[XmlArray]` on protected fields, which `XmlSerializer` handles differently under IL2CPP).

**Fix:**

- **Null-guard** `FindPig()` (and the contraption) before using them — skip the camera-target assignment and/or pick the next dataset if the pig is missing, so one bad dataset doesn't throw.
- **Investigate the empty-contraption case**: confirm the menu datasets deserialize correctly under IL2CPP. A quick check: does the animated pig-vehicle actually appear behind the main menu? If it doesn't, the empty-dataset path is confirmed.

---

## Bug 3 — Three "missing script" warnings (harmless, but easy to clear)

**Symptom:**

```
The referenced script on this Behaviour (Game Object 'BackgroundBox') is missing!   (x2)
The referenced script (REConsoleInterface) on this Behaviour is missing!
The referenced script on this Behaviour (Game Object 'REConsoleInterface') is missing!
```

These are the only broken `MonoBehaviour` references in the entire build (we checked all 14,784 — the other 14,781 are fine), and they map 1:1 to the warnings:

**Root cause:**

- **`BackgroundBox` ×2** — two prefabs, **`CrateCrazePopup`** and **`CoinSalePopup`**, each carry a child `BackgroundBox` with an **extra component whose `m_Script` is null (fileID 0, pathID 0)** baked into the prefab — an empty husk left behind when a script was deleted before the build. (The other 57 `BackgroundBox` objects are fine; only these two popups have the stray component.)
- **`REConsoleInterface`** — the prefab has a `MonoScript` entry, but the **class isn't in the IL2CPP build at all**. It's a runtime C#-compile-and-run console (Panel → Scroll View → `ConsoleCode`/`ConsoleOutput` input fields + `CompileButton`/`RunButton`), and runtime C# compilation (CodeDom-style) can't work under IL2CPP AOT — so the class was left out but the prefab stayed in `Resources`. It gets pulled in as a dependency during `SplashPlayer`'s startup coroutine via `Resources.Load("Innovation/INInitializer")` → `INInitializer` → `ResourceData` → `INApplicationInterface` → `REConsoleInterface`. (Your separate command-console `RECommandInterface`/`RECommandHandler` is intact and unaffected.)

**Effect:** harmless — the components are skipped. Worth clearing just to keep the log clean.

**Fix:**

- **Remove the dead null-script components** from `CrateCrazePopup/BackgroundBox` and `CoinSalePopup/BackgroundBox` (select each, delete the "missing script" component, re-save the prefab).
- **Drop the `REConsoleInterface` runtime-console prefab** from the build (or exclude the `Innovation/INInitializer` → `REConsoleInterface` dependency), since the compile/run console can't run under IL2CPP. If you want a console in IL2CPP builds, your existing `RECommand*` command console is the one that works.

---

## Optional: running on low-end / feature-level 9_1 GPUs

Not something you need to change — just context in case you ever want a native build to reach older GPUs:

- We run the game on old Tegra-class tablets (Direct3D **feature level 9_1**) through an external compatibility layer; the engine itself is untouched. Unity 2018.4 requests feature level 10 by default, so a stock native build simply won't start on a 9_1 device — nothing for you to fix unless supporting that class of hardware is a goal.
- Minor, cosmetic: two **SHADOWCASTER** shader programs (from `Legacy Shaders/VertexLit` and `Particles/Standard Unlit`) get compiled at scene load even though the project has shadows disabled and no lights. It's harmless — just a couple of stray shader variants — and not worth chasing.

---

*These findings come from static analysis of the shipped build only (decompiled IL2CPP + serialized assets); we didn't have your source. We're happy to share more detail (exact prefabs, object IDs, atlas/material names, or the reconstructed method logic) if any of it would help you reproduce or fix these faster.*

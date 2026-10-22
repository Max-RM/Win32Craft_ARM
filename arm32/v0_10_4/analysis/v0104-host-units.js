export const meta = {
  name: 'v0104-host-units',
  description: 'Implement the five 0.10.4 host units in parallel (async, app, panel, platform, input), independent review + fix per unit, then full-tree integration and cross-unit review',
  phases: [
    { title: 'Implement', detail: 'one implementer per unit file' },
    { title: 'Review', detail: 'independent adversarial reviewer per unit' },
    { title: 'Fix', detail: 'apply review findings per unit' },
    { title: 'Integrate', detail: 'full tree build, selftest, cross-unit flow trace, ordering/threading + ABI/slot reviews' },
  ],
}

const R = 'D:\\repo\\Win32Craft-main'
const V = R + '\\arm32\\v0_10_4'
const EXE = 'D:\\repo\\win32craft\\0.10.4.40\\Minecraft Pocket Edition v0.10.4.40 [Winphone Iz]\\MCPE.Host.WindowsPhone.exe'
const UNITS = ['async', 'app', 'panel', 'platform', 'input']
const FILE = k => R + '\\win32_host_arm32_0104_' + k + '.inl'

const BASE = [
  'PROJECT: Win32Craft runs Minecraft PE 0.10.4.40 (Windows Phone 8.1 XAML app, ARM32 Thumb-2) as a desktop program on Windows RT 8.1 / Windows 10 ARM32 (Surface RT / Surface 2, D3D feature level 9_1 only). The game own code runs UNMODIFIED against host fakes of the XAML/WinRT objects it touches (faithful architecture, already decided and verified). Repo: ' + R + '. The game exe (read-only, analyse with python ' + R + '\\tools\\arm32\\arm32re.py after setting ARM32_EXE to the path) is ' + EXE + '.',
  'READ FIRST, completely: ' + V + '\\HOST_DESIGN.md (single source of truth: architecture, file ownership, rules R1-R17, threading model, toolkit guide, cross-unit API table in section 8, bootstrap, logging contract, decisions after review), then your unit brief ' + V + '\\units\\<unit>.md, then ' + V + '\\units\\SLOTS.md (generated slot/ABI tables, typedef names X104_FN_*), then the fact notes your brief cites (' + V + '\\facts\\*.md and RECON_SUMMARY.md; fact ids are keys in facts\\<area>.json and .verdicts.json; where a verdict corrects a fact note the verdict wins). Also read ' + R + '\\win32_host_arm32_0104_core.inl (the toolkit you build on), the generated ' + R + '\\win32_host_arm32_0104_iids.inl and ' + R + '\\win32_host_arm32_0104.h (verified RVAs ARM0104_*), and the PORT_RULES at ' + R + '\\arm32\\PORT_RULES.md.',
  'HARD RULES: edit ONLY your own unit file (named below); never edit another unit, the core, the boot, the selftest, win32_host.c, the generated files or anything under D:\\repo\\win32craft\\0.10.4.40. Keep the public function names and signatures of your unit EXACTLY as in the contract (HOST_DESIGN.md section 8 and the stub in ' + V + '\\stubs\\). Everything file-scope that is not contract starts with x104_<unit>_. Register every vtable slot with X104_SLOT / X104_SLOT_NOARGS (compile-time ABI check against the generated typedefs). Use only symbols of core / iids / header / win32_host.c (try_0104.py lints this). ARM only, x86 untouched. Every failure path logs what failed + HRESULT + actual values (x104_logf); no silent returns. Never invoke a game delegate synchronously from inside a fake (rule R1). No placeholders: every behaviour the brief lists must be implemented, or reported as impossible with the reason.',
  'COMPILE LOOP: python ' + R + '\\tools\\arm32\\try_0104.py --unit <your-unit> must end with zero errors and zero warnings in 0104 files, lint clean, link ok, x86 identical. Optional PC-side logic tests as described in HOST_DESIGN.md section 2 rule 5 (new files named after your unit are allowed under tools\\arm32\\). Use the Write/Edit tools for files (no shell heredocs with backslashes).',
].join('\n')

const IMPL_SCHEMA = {
  type: 'object',
  properties: {
    unit: { type: 'string' },
    compileOk: { type: 'boolean' },
    linesWritten: { type: 'integer' },
    slotsImplemented: { type: 'integer' },
    summary: { type: 'string' },
    deviations: { type: 'array', items: { type: 'string' } },
    notVerified: { type: 'array', items: { type: 'string' } },
    contractRequests: { type: 'array', items: { type: 'string' } },
  },
  required: ['unit', 'compileOk', 'linesWritten', 'slotsImplemented', 'summary', 'deviations', 'notVerified', 'contractRequests'],
}
const REVIEW_SCHEMA = {
  type: 'object',
  properties: {
    unit: { type: 'string' },
    findings: {
      type: 'array',
      items: {
        type: 'object',
        properties: { severity: { type: 'string', enum: ['blocker', 'high', 'medium', 'low'] }, where: { type: 'string' }, problem: { type: 'string' }, scenario: { type: 'string' }, fix: { type: 'string' } },
        required: ['severity', 'where', 'problem', 'scenario', 'fix'],
      },
    },
    verifiedClean: { type: 'array', items: { type: 'string' } },
    verdict: { type: 'string' },
  },
  required: ['unit', 'findings', 'verifiedClean', 'verdict'],
}
const FINAL_SCHEMA = {
  type: 'object',
  properties: {
    buildOk: { type: 'boolean' },
    x86Identical: { type: 'boolean' },
    selftestOk: { type: 'boolean' },
    summary: { type: 'string' },
    fixesMade: { type: 'array', items: { type: 'string' } },
    openIssues: { type: 'array', items: { type: 'string' } },
  },
  required: ['buildOk', 'x86Identical', 'selftestOk', 'summary', 'fixesMade', 'openIssues'],
}

phase('Implement')
const results = await pipeline(
  UNITS,
  unit => agent(
    BASE + '\n\nYOUR UNIT: ' + unit + ' - file ' + FILE(unit) + ' (currently a stub; the pristine stub is in ' + V + '\\stubs\\). Implement it completely per ' + V + '\\units\\' + unit + '.md. Think about ordering, threads and the game exact expectations (read the game disassembly with arm32re.py wherever a brief says the behaviour matters: what the game does with each return value and out parameter). When the compile loop is clean, double-check each slot against units\\SLOTS.md and the facts once more, then return the summary object.',
    { label: 'impl:' + unit, phase: 'Implement', schema: IMPL_SCHEMA, effort: 'high' }
  ),
  (impl, unit) => {
    if (!impl) return null
    return agent(
      [
        'You are an independent, adversarial REVIEWER of ONE host unit that a colleague just implemented: ' + unit + ' (file ' + FILE(unit) + '). Do not edit any file except scratch files under ' + R + '\\arm32\\work\\ and ' + V + '\\analysis\\.',
        'Read ' + V + '\\HOST_DESIGN.md completely, the brief ' + V + '\\units\\' + unit + '.md, ' + V + '\\units\\SLOTS.md, the core toolkit ' + R + '\\win32_host_arm32_0104_core.inl and the unit file. Set ARM32_EXE to ' + EXE + ' for arm32re.py.',
        'Verify, independently and concretely: (1) EVERY object/interface/slot the brief lists is implemented with the exact ABI and the exact behaviour the game needs - re-derive the behaviour-critical ones from the game disassembly (what the game does with each HRESULT, out pointer, returned object, the argument pointer it passes without QI) instead of trusting the brief; (2) the rules R1-R17 of HOST_DESIGN.md (no synchronous delegate invocation inside fakes, ordering rules, thread rules, event/token protocol, QI/IAgile, HSTRING ownership, safe-before-init, early exit); (3) thread safety and deadlocks against the game AppMain critical section and the render-loop busy spin; (4) memory/refcount safety, buffer sizes, wsprintfA formats, 64-bit and double/float argument handling on ARM hard-float AAPCS, Thumb bit on function pointers handed to the game; (5) cross-unit calls use the contracted prototypes and semantics; (6) logging per the brief; (7) run python ' + R + '\\tools\\arm32\\try_0104.py --unit ' + unit + ' yourself and report; try the PC-side tests if present. Findings must carry a concrete failure scenario and a concrete fix. Rank blocker/high/medium/low. List what you verified clean.',
        '', 'Implementer report: ' + JSON.stringify(impl).slice(0, 3500),
      ].join('\n'),
      { label: 'review:' + unit, phase: 'Review', schema: REVIEW_SCHEMA, effort: 'high' }
    ).then(rev => ({ unit, impl, rev }))
  },
  (prev, unit) => {
    if (!prev || !prev.rev) return prev
    const worth = prev.rev.findings.filter(f => f.severity !== 'low')
    if (!worth.length) return prev
    return agent(
      BASE + '\n\nYOUR UNIT: ' + unit + ' - file ' + FILE(unit) + ' (you are the implementer again). An independent reviewer found the defects below. Verify each against the code, the facts and the game disassembly (do not trust blindly: if a finding is wrong say why), fix every real blocker/high/medium finding in your unit file, rerun the compile loop until clean, and report. Also fix low findings that are trivial.\nFINDINGS: ' + JSON.stringify(worth),
      { label: 'fix:' + unit, phase: 'Fix', schema: IMPL_SCHEMA, effort: 'high' }
    ).then(fix => Object.assign({}, prev, { fix }))
  }
)
const done = results.filter(Boolean)
log('units finished: ' + done.length + '/' + UNITS.length)

phase('Integrate')
const INTEG = [
  'All five unit files are implemented, reviewed and fixed individually (summaries below). Your job is the FULL TREE: ' + R + '. Read ' + V + '\\HOST_DESIGN.md first.',
  'UNIT SUMMARIES: ' + JSON.stringify(done.map(d => ({ unit: d.unit, impl: d.impl && d.impl.summary, review: d.rev && d.rev.verdict, fix: d.fix && d.fix.summary, requests: (d.impl && d.impl.contractRequests) || [] }))).slice(0, 9000),
].join('\n')

const integ = await agent(
  INTEG + '\n' + [
    'STEPS: (1) python tools\\arm32\\try_0104.py (real tree), python tools\\arm32\\try_0104.py --selftest, cmd /c build_arm32.bat (BUILD/DIST may be left default: it rebuilds dist-arm32\\win32craft.dll), python tools\\arm32\\check_x86.py: all must be clean/identical; fix compile or link problems that appear ONLY in the combined tree (name clashes, cross-unit prototype mismatches) by editing the SMALLEST possible spot in the responsible unit file. (2) Resolve every contract request the implementers listed (decide; core/boot/selftest/win32_host.c may be edited now if truly needed - keep guarded and minimal, record in HOST_DESIGN.md section 14). (3) Trace the whole boot on paper through the REAL unit code: bootstrap -> game WinMain wrapper -> Application::Start (app) -> UI loop (async) -> App2 aggregation -> OnLaunched -> StatusBar -> MainPage ctor (every activation site of startup_flow.md table 2a gets the right object/interface/slot) -> FindName elements (panel + input) -> event registrations -> DeviceResources (panel hook) -> input work items (async + input) -> AppMain/platform/ApplicationData/Package (platform) -> init job -> first SizeChanged (panel, ordering R2) -> ready Tick (async, R3) -> AttachToPanel/HID::Initialize (input events registered on the fake CoreWindow/TextBox) -> StartRenderLoop -> Present stub. For each hop confirm the producing and consuming unit agree on pointers (R10 view rule), threads and state flags (x104.f_*). Fix mismatches. (4) Make sure the selftest (--selftest-fakes, win32_host_arm32_0104_selftest.inl) really exercises the implemented fakes now and compiles; extend it only if a unit check is still a SKIP. (5) Update HOST_DESIGN.md section 14 with the integration decisions and write ' + V + '\\INTEGRATION_NOTES.md: what is implemented, what is NOT verifiable offline (device-only list), the exact log lines that prove each milestone on the device (M1 menu visible), and the test plan for the user. Report.',
  ].join('\n'),
  { label: 'integrate', phase: 'Integrate', schema: FINAL_SCHEMA, effort: 'high' }
)

const lenses = [
  { key: 'ordering', text: 'ORDERING + THREADING lens: walk the game real flow (startup_flow.md sections 1, 1b-1d, 5a; rules R1-R17) through the combined code and hunt deadlocks, lost wake-ups, ordering violations (first SizeChanged vs client+0xC, timer start, ready gate, DR+0x30), re-entrancy into the game critical section, shutdown/early-exit hangs, exceptions escaping work items, UI loop starvation (render loop busy spin, 10 ms pump timer), nested modal loops (window move/size).' },
  { key: 'abi', text: 'ABI + SLOT-COVERAGE lens: for every interface and slot in startup_flow.md table 2a, input_text.md, platform_services.md and frame_render.md, confirm the combined code registers it with the exact ABI (AAPCS-VFP: LONGLONG/TimeSpan r2:r3 alignment, double in d0, Size/Rect pointers, BYTE bools, HSTRING borrowed vs owned, out pointers), correct IIDs for QI, the R10 view rule for every event args pointer, the Thumb bit on every function pointer given to the game, vtable hole filling, and that nothing the game calls through a QI the host fails (QI MISS). Cross-check against game disassembly samples (arm32re.py with ARM32_EXE set to ' + EXE + ').' },
]
const reviews = await parallel(lenses.map(l => () => agent(
  INTEG + '\n' + 'You are an independent reviewer of the integrated tree with this lens: ' + l.text + ' Do not edit source files (scratch under ' + V + '\\analysis\\ only). Run the build/try scripts yourself first. Return findings with concrete scenarios and fixes.',
  { label: 'xreview:' + l.key, phase: 'Integrate', schema: REVIEW_SCHEMA, effort: 'high' }
)))
const xf = []
reviews.filter(Boolean).forEach(r => (r.findings || []).forEach(f => { if (f.severity !== 'low') xf.push(f) }))

let finalFix = null
if (xf.length) {
  finalFix = await agent(
    INTEG + '\n' + 'Two independent cross-unit reviews of the integrated tree found the defects below. Verify each against code/facts, fix the real ones in the responsible files (smallest edits; record cross-cutting decisions in HOST_DESIGN.md section 14 and INTEGRATION_NOTES.md), then rerun try_0104.py (tree + all five --unit modes), --selftest, build_arm32.bat and check_x86.py and report.\nFINDINGS: ' + JSON.stringify(xf).slice(0, 14000),
    { label: 'final-fix', phase: 'Integrate', schema: FINAL_SCHEMA, effort: 'high' }
  )
}
return { units: done.map(d => ({ unit: d.unit, impl: d.impl, reviewVerdict: d.rev && d.rev.verdict, findings: d.rev && d.rev.findings.length, fix: d.fix && d.fix.summary })), integ, crossReviews: reviews.filter(Boolean).map(r => ({ verdict: r.verdict, findings: r.findings.length })), finalFix }

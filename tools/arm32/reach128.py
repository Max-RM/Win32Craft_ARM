#!/usr/bin/env python3
"""Rough static reachability of win32_host.c from the 1.2.8 bootstrap path.

Nodes: top-level function definitions and top-level data statements (static tables / vtables with
initialisers).  Edges: any identifier mention (so address-taken callbacks and vtable members count).
Writes JSON {reachable:[[a,b,name]], unreachable:[[a,b,name]]} (1-indexed inclusive line spans of
functions only)."""
import re, sys, json
from pathlib import Path

src = Path(sys.argv[1] if len(sys.argv) > 1 else "win32_host.c").read_text(encoding="utf-8", errors="replace").split("\n")
n = len(src)
funcs = {}   # name -> [(start, end)]
i = 0
sig_re = re.compile(r'^(?!\s)(?!#)(?!typedef)(?!struct\b)(?!enum\b)(?!union\b)([A-Za-z_][\w\s\*\(\)\[\],]*?)\b([A-Za-z_]\w*)\s*\(')
while i < n:
    line = src[i]
    m = sig_re.match(line)
    if m and not line.rstrip().endswith(';'):
        j = i
        found = None
        while j < min(n, i + 40):
            if src[j].startswith('{') or (src[j].rstrip().endswith('{') and ')' in src[j]):
                found = j
                break
            if src[j].rstrip().endswith(';'):
                break
            j += 1
        if found is not None:
            k = found + 1
            while k < n and not src[k].startswith('}'):
                k += 1
            funcs.setdefault(m.group(2), []).append((i + 1, k + 1))
            i = k + 1
            continue
    i += 1

in_func = [False] * (n + 2)
for spans in funcs.values():
    for (a, b) in spans:
        for k in range(a, b + 1):
            in_func[k] = True

# top-level data statements with initialisers
datastmts = {}
eq_re = re.compile(r'\b([A-Za-z_]\w*)\s*(?:\[[^\]]*\]\s*)*=\s*[\{\(&A-Za-z0-9_"\-]')
i = 0
while i < n:
    ln = src[i]
    if in_func[i + 1] or not ln or ln[0] in ' \t#}/*' or ln.startswith('//'):
        i += 1
        continue
    j = i
    depth = 0
    text = []
    while j < n:
        text.append(src[j])
        depth += src[j].count('{') - src[j].count('}')
        if depth <= 0 and src[j].rstrip().endswith(';'):
            break
        j += 1
        if j - i > 4000:
            break
    t = "\n".join(text)
    m = eq_re.search(t)
    if m:
        datastmts.setdefault(m.group(1), []).append(t)
    i = j + 1

ident = re.compile(r'[A-Za-z_]\w*')
bodies = {}
for name, spans in funcs.items():
    bodies[name] = "\n".join("\n".join(src[a - 1:b]) for (a, b) in spans)
for dn, tl in datastmts.items():
    bodies[dn] = bodies.get(dn, "") + "\n" + "\n".join(tl)
nodes = set(funcs) | set(datastmts)
mentions = {name: (set(ident.findall(body)) & nodes) - {name} for name, body in bodies.items()}

roots = """install_crash_trace install_exception_trace install_create_file2_compat create_user_data
install_activation_redirect install_vccorlib_compat install_d3d_compile_trace
patch_win7_dxgi_device3_query_to_base patch_win7_discard_views install_d3d11_device_compat
install_xbox_tcui_noops_128 patch_xbox_actions_noop patch_chat_textbox_compat
install_128_zip_resource_overrides install_client_state_hook_128 resolve_fmod_delay_imports_128
install_mouse_mode_hooks install_win32_platform_fixes_128 window_proc initialize_game_d3d
initialize_game_app_128 activate_game_app_128 set_game_relative_actual update_mouse_capture
drain_dispatcher_queue refresh_relative_mouse_from_client_128 render_game_target_128
Win32CraftParseHostOptions DllMain D3D11CreateDevice CreateDirect3D11DeviceFromDXGIDevice CreateDXGIFactory2
Win32CraftCleanupCopyrightTemps win32_vectored_exception win32_unhandled_exception log_line
Win32Bootstrap""".split()
roots = [r for r in roots if r in nodes]
export_re = re.compile(r'^(Win32Craft|Ro[A-Z]|Windows[A-Z]|Co[A-Z]|Event[A-Z]|Show[A-Z]|Process|XInput|Js|LoadPackaged|GetRestricted|SetRestricted)')
roots += [nme for nme in funcs if export_re.match(nme) and nme not in roots]

seen = set(roots)
stack = list(roots)
while stack:
    f = stack.pop()
    for g in mentions.get(f, ()):
        if g not in seen:
            seen.add(g)
            stack.append(g)

rf = sorted((funcs[nme][0][0], funcs[nme][0][1], nme) for nme in funcs if nme in seen)
uf = sorted((funcs[nme][0][0], funcs[nme][0][1], nme) for nme in funcs if nme not in seen)
print(f"functions: {len(funcs)}  data nodes: {len(datastmts)}")
print(f"reachable (over-approx): {len(rf)} funcs, {sum(b - a + 1 for a, b, _ in rf)} lines")
print(f"unreachable: {len(uf)} funcs, {sum(b - a + 1 for a, b, _ in uf)} lines")
Path(sys.argv[2] if len(sys.argv) > 2 else "reach128.json").write_text(json.dumps({"reachable": rf, "unreachable": uf}, indent=1))

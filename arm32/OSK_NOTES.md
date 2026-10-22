# Touch keyboard (TabTip) - device facts and the host controller

STATUS: the host controller below was tested by the user on the Surface 2 (Windows 10 ARM32) on 2026-10-17 and the keyboard bug is FIXED there; Windows RT 8.1 is still unmeasured.

Why this exists: on Surface RT / Surface 2 the keyboard "came up but would not close and would not go down" (user report 2026-10-17; the same problem hit TelegArm).
Root causes found by reading the code against a device probe (`tools/kbdprobe`, log `arm32/device_logs/kbdprobe_surface2_win10_15035.log`):

* The first controller decided "keyboard visible" with `IsWindowVisible(IPTip_Main_Window)`. That window is ALWAYS visible and its rectangle never changes
  (probe: `vis=1 rect=(0,360,1280,720)` in both states), so the host (a) never toggled a running TabTip, (b) reported a keyboard rectangle to the game forever
  ("won't go down") and (c) never told the game about Showing/Hiding (the game registers handlers: Showing body 0x57e2e8, Hiding body 0x57e398, both call
  `args->put_EnsuredFocusedElementInView(true)`; the host ignored `add_Showing/add_Hiding`).
* `ITipInvocation::Toggle` is state blind (shows AND hides); launching `TabTip.exe` while it runs does nothing.

## Measured on Windows 10 build 15035 ARM32 (Surface 2), one run, user answers recorded next to every signal
| signal | shown | hidden |
|---|---|---|
| `IFrameworkInputPane::Location` (shell, desktop) | rect (0,540,1920,1080) | (0,0,0,0) |
| WinRT `InputPane::OccludedRect` via `IInputPaneInterop::GetForWindow` | height 106 | all 0 |
| DWM cloak of `IPTip_Main_Window` | 0 | 1 |
| `IsWindowVisible` / window rect | 1 / constant | 1 / constant (useless) |
| `IFrameworkInputPane` Showing / Hiding callbacks | fired on every change (also when the user taps the keyboard's X) | |

Show: `ITipInvocation::Toggle` works; `TabTip.exe` launch only creates the window (hidden); `IInputPane2::TryShow` does NOT show (no text control).
Hide: `IInputPane2::TryHide` works, `SC_CLOSE` posted to the TabTip window works, `ITipInvocation::Toggle` works, `ShowWindow(SW_HIDE)` does nothing, killing `TabTip.exe`
hides it but leaves `IFrameworkInputPane` stale and makes `ITipInvocation` fail with 0x80040154 until TabTip.exe is launched again (never kill it).
The native EDIT of the probe did not make Windows show the keyboard by itself (slate mode 0 on that device).

## Windows RT 8.1 is NOT measured yet
TelegArm found there: InputPane interop unavailable, cloak flag unreliable, X of the keyboard sometimes dead. The controller therefore picks the best signal at runtime and logs
`ARM32 osk: signals: ... primary=<IFrameworkInputPane|InputPane|dwm-cloak|none>`. With no signal at all it does one action per request and assumes the result.

## Host controller (win32_host.c, ARM block "Windows touch keyboard (TabTip) for text boxes", log prefix `ARM32 osk:`)
* State = `IFrameworkInputPane` OR WinRT InputPane (strong), else DWM cloak (weak), else unknown. Polled in `arm32_input_frame` only while a request is pending / the keyboard is ours.
* Show (game `TryShow`, only when touch is the active source or `--osk=on`): if the state says visible do nothing; else TabTip window missing -> launch `TabTip.exe`, window present -> Toggle,
  retry every 0.9 s (1.6 s after a launch) while the state is definitely hidden, max 5; with no signal exactly one action.
* Hide (game `TryHide`, only for a keyboard the host showed - TryHide also runs once at startup): `IInputPane2::TryHide`, then `SC_CLOSE`, then `Toggle` (strong signals only), each verified
  550 ms later; never kill TabTip.
* Game events: the Showing / Hiding delegates the game registered (`add_Showing/add_Hiding` are stored and AddRef'd) are raised from the main loop when the state changes, with a fake
  `InputPaneVisibilityEventArgs`; `OccludedRect` now returns the keyboard rectangle only while the state is "visible", so the chat box moves up and back down.
* The user closes the keyboard with X: detected as visible -> hidden while the host owns it -> `Hiding` raised; the game keeps the text focus. A tap on the game (finger up) while the text
  box still has focus (`text_input_active`) asks for the keyboard again; `TryShow` calls that arrive after the dismissal without a tap are ignored (the game cannot reopen it by itself).
* `kbdprobe` (tools/kbdprobe, `build_kbdprobe_arm32.bat`, zip `dist-arm32/kbdprobe.zip`) is the device lab: re-run it on RT 8.1 to measure the signals there.

## Review hardening (second DLL, 2026-10-17; user tested it on the Surface 2: okay and better than the first)
Toggle is never sent twice within 2 s and never for a request that is already in flight; `TryShow` during a hide is queued, `TryHide` during a show is deferred until the keyboard is visible; a missing / invisible TabTip window always
means "hidden" (the shell state goes stale when TabTip restarts); a shell signal that never changed ("unproven") cannot trigger a second Toggle after a successful one (the host assumes "visible" and logs it); retries: 2.5 s after a
successful Toggle, 400 ms after a failed one, 300 ms polling after a TabTip launch (max 10 attempts).

## 0.10.4 note (2026-10-17, design review)
The controller reads the shared global `text_input_active` (tap-to-reopen after the user closed the keyboard, hardware-key source switch). In 1.2.8 the client-state hook sets it; the 0.10.4 host does not use that hook,
so the 0.10.4 INPUT unit sets it through the core wrapper `x104_set_text_input_active()` on every `EditBox.Focus` (TRUE) / `Button.Focus` (FALSE) / shutdown (FALSE) and logs each transition (`X104 text_input_active a -> b`).
`arm32_input_pane_raise` forwards to `x104_inputpane_raise()` when `host_is_0104` (guarded edit, 1.2.8 unchanged).

## Windows 11 ARM64 (build 22631) - second DLL generation (2026-10-18, from the friend's logs)
Friend's logs (`arm32/device_logs/win11_arm64_*`): a POCO X3 Pro phone (Adreno 640, 2400x1080) running Windows 11 ARM64 with external keyboards (corrected 2026-10-21: the DXGI adapter line names it), both games, the keyboard "came up and went away by itself / did not go down".
What the logs show (host build 2026-10-18 09:42, i.e. the Windows 10 controller above):
* `IFrameworkInputPane` creates and advises fine, the TabTip window class `IPTip_Main_Window` EXISTS but is never visible (the touch keyboard of Windows 11 lives in TextInputHost.exe),
  so the Windows 10 rule "window not visible = hidden" answered "hidden" for ever: `keyboard hidden (signal IFrameworkInputPane, was -1)` and never VISIBLE.
* The shell does call the Showing / Hiding callbacks, ~350 ms after every Toggle (`shell Showing/Hiding callback #N`).  A callback also counted as "the signal is proven", so the controller
  re-sent ITipInvocation::Toggle every 2.5 s, ten times: the keyboard flickered open / shut and ended in an arbitrary state; the hide was ignored (`ours` was never set) - "won't go down".
* `InputPane` interop fails there (0x8000001d / 0x80004005), so there is no TryHide.

Fix (Windows 11 only - `arm32_osk_is_win11()`, build >= 22000; the Windows 8.1 / 10 paths are unchanged, proven by `tools/arm32/test_osk.py`, which runs the same scenarios against the old and the new controller):
* the Showing / Hiding callback IS the state (kind + rectangle stored by the handlers); the IPTip window only has to exist; a Location rectangle that was seen non-empty ("loc_works") overrules a
  Showing callback older than 1.5 s whose rectangle is empty;
* a callback is no longer "proof" for the retry logic; with a working signal at most 4 show attempts (never ten Toggles);
* hide order on Windows 11: Toggle first (only while the keyboard is believed visible, so it can never open it), `SC_CLOSE` as the fallback; a Toggle held back by the 2 s guard is retried after the
  guard instead of being skipped (hide right after show);
* a callback that arrives while the host only ASSUMES a state replaces the assumption;
* log: `ARM32 osk: shell Showing|Hiding callback #N rect=(...)`, `ARM32 osk: snapshot (...)` (every signal on one line: IPTip window visible/cloak/rect, Location, OccludedRect, TextInputHost window,
  callback kind/rect, state) after the first six callbacks and when the controller gives up.  If Windows 11 still misbehaves, the snapshot lines say which signal lies.

PC simulation: `python tools/arm32/test_osk.py [-v]` compiles the REAL controller block of win32_host.c against a fake shell (Win11 and Win10 models) - 11 scenarios (show/hide, hide while coming up,
hide right after show, no callbacks, user closes with X + tap to reopen, keyboard already open, never appears, four chat opens in a row, Windows 10 show/hide, blind signal).  With
`WIN32CRAFT_HOST_C=<old win32_host.c>` it runs against another controller: the 2026-10-18 09:42 controller fails 8 of the 9 Windows 11 scenarios (reproducing the log) and passes both Windows 10 ones.

## Windows 11 measured with the fix (friend, 2026-10-18 23:49, 0.10.4, DLL build Oct 18 11:32:53) and two follow-up fixes (2026-10-21)
* Log `arm32/device_logs/win11_arm64_0104_friend_r2dll.log` (3 sessions): 4 shows and 2 host hides took 6 Toggles in total (old DLL: 30 Toggles + give-ups); Showing callback 343-375 ms after a show Toggle,
  Hiding 47-62 ms after a hide Toggle; no flicker, no ignored hide.  IFrameworkInputPane::Location IS accurate on Windows 11 ((0,540,2400,1080) shown, empty hidden) - only IPTip_Main_Window is useless (0x0, invisible).
  The 1.2.8 logs of the same evening ran the OLD DLL (09:42:51) - the friend's 1.2.8 folder was never updated; 1.2.8 with the fix is still unmeasured.
* Fix 1 (Windows 11 only): `arm32_osk_rect_buffer()` took the keyboard rectangle from the 0x0 IPTip window, so 0.10.4 fell back to a 40% guess and 1.2.8's fake OccludedRect got nothing.
  It now uses Location, else the last Showing rectangle, on Windows 11 (Windows 8.1/10: TabTip window as before).
* Fix 2 (all versions, an improvement): a show request whose Toggle was held back by the 2 s guard (re-tap shortly after the keyboard went down) was retried only 2.5 s later and the code
  then reasoned about a Toggle that never ran (on an unproven signal it could even assume "visible" without sending anything).  Now `arm32_osk_last_show_suppressed` makes the retry
  fire right after the guard (last Toggle + 2.1 s) and skips the assumptions.  1.2.8 Windows 11 log: 2.9 s -> simulated 1.8 s (Windows 11) / 1.2 s (Windows 10).
* `tools/arm32/test_osk.py` now includes the real `arm32_osk_rect_buffer()` and 16 scenarios (new: win11_rect, win11_rect_location, win10_rect, win11/win10_show_right_after_hide).
  Against the 2026-10-18 controller (`WIN32CRAFT_HOST_C=` the win32_host.c of `dist-handoff/r1_superseded/Win32Craft_source_2026-10-18_r2_superseded_by_r3.zip`/r3): the rectangle and reopen
  scenarios fail (2880 ms, exactly the log's 2.9 s), every other scenario - all Windows 10 ones included - gives the same result.
* Device correction: the "Windows 11 machine" is a POCO X3 Pro phone (Snapdragon / Adreno 640) running Windows 11 ARM64, with external keyboards - not a Surface Pro X / Type Cover.
* Open, not fixed: callbacks that arrive while the controller is idle are counted but not logged (numbers jump); the "closed by the user (its X)" line is an attribution, not a measurement.
* 1.2.8 with the 10-18 DLL on Windows 11 (`arm32/device_logs/win11_arm64_128_friend_r2dll.log`, friend: "still has a problem"): shows / hides mostly one Toggle each, BUT at 157.6 s a show request
  1.7 s after a hide hit the 2 s guard and the three retries (160.1 / 162.7 / 165.2 s, Toggle hr=0) got NO shell callback - the controller gave up; four callbacks (#3-#6) then arrived while
  the controller was idle and were neither logged nor processed (stale state; typed keys flipped the game to Mouse mode).  Not explained by "Toggle needs recent user input" (the old-DLL log
  has answered Toggles 3.9 s after any input).  Cause still unknown.
* Fix 3 (Windows 11 only, DLL Oct 21 2026 12:49:11, sha256 08536fe9d662d674b9a066db2113714f715e2df407e14dad9bccbb5b1c65c68c): shell callbacks are now processed and logged even when no request is pending
  ("... callback #N ... while no request was pending, arrived X ms ago"; Hiding lines no longer print a stale rectangle), so the state follows a keyboard Windows shows or hides by itself
  and a later TryShow adopts it instead of toggling it shut.  With fix 2 the retry of that 157.6 s request fires 0.4 s after the tap instead of 2.5 s.  test_osk.py: 18 scenarios
  (new win11/win10_shell_acts_while_idle; Windows 10 unchanged).  The next log should show exactly when the shell reacts.
* Surface 2 (Windows 10 15035) 1.2.8 with the 12:49 DLL (`arm32/device_logs/surface2_win10_128_r4dll_keyboard_flicker.log`, user: "the keyboard still has a problem"): the controller obeyed
  every request, but the GAME asks for odd pairs - after Enter TryHide and 62 ms later TryShow for the same chat box (keyboard closed and reopened 0.5 s later), and a press that
  opens the keyboard before the finger lifts (the game moves the chat box above the keyboard at once, the lift lands outside the text box, TryHide 31 ms after the keyboard came up).
  Fix 4 (all versions): TryHide waits ARM32_OSK_HIDE_DELAY_MS (300 ms); a TryShow inside that time cancels it.  Fix 5 (1.2.8 OccludedRect only, `arm32_osk_rect_for_game`):
  no rectangle while a finger that touched before the keyboard appeared is still down (max 3 s).  Fix 6 (all versions): the 2 s Toggle guard is released as soon as a shell
  callback newer than the last Toggle arrived (the first Toggle has taken effect) - re-open after a hide: ~0.4 s.  test_osk.py: 22 scenarios, invariant "no Toggle within 2 s of an
  UNCONFIRMED Toggle"; the 12:49 controller fails exactly the 4 replays of this log.  DLL Oct 21 2026 13:46:04 (sha256 0a9d1776da03053aca90e2abf323fdbd40ec972eafc0e0a604e04d7ad7e15e69, also has the 0.10.4 Back/Pause button).

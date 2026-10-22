kbdprobe - touch keyboard (TabTip) lab for Surface 2 / Surface RT (ARM32, Windows 10 or Windows RT 8.1)
=======================================================================================================

Purpose: one short device run that tells us (1) which signal really says "the touch keyboard is on screen",
and (2) which way of hiding it really works, for a window with NO native text box (that is how the game
window behaves). The log stores every signal next to your "visible / not visible" answers.
Signals logged on every change: TabTip window state (exists / IsWindowVisible / DWM cloak / rect), TabTip.exe process
count, IFrameworkInputPane (desktop shell: Location + Showing/Hiding events), WinRT InputPane OccludedRect (via
IInputPaneInterop - Windows 10), foreground window and focus window classes.

Run (touch only is fine, the buttons are big; detach the Type Cover if you can, so Windows treats the device as a tablet):
 1. Copy kbdprobe.exe to the device (any folder), start it. It opens maximised and writes kbdprobe.log next to the exe.
 2. Tap the grey SURFACE box (it behaves like the game window: takes focus, has no native EDIT).
 3. Tap "SHOW: launch TabTip.exe". The keyboard should appear.
 4. Tap "Keyboard IS visible" (logs your answer together with all signals at that moment).
 5. Type "ab" on the touch keyboard (the SURFACE box shows what arrives).
 6. Tap the keyboard's X (close). Then tap "Keyboard IS visible" or "Keyboard is NOT visible" - whatever you see.
 7. Tap SURFACE again, tap "SHOW: launch TabTip.exe" again (note if it comes back at all), answer visible / not visible.
 8. With the keyboard visible, try the HIDE buttons ONE AT A TIME, answering visible / not visible after each:
      HIDE: InputPane2::TryHide            (Windows 10 only; "unavailable" in the log is a valid result on 8.1)
      HIDE: SC_CLOSE to tip window
      HIDE: ITipInvocation::Toggle
      HIDE: ShowWindow(SW_HIDE)
      HIDE: blur (SetFocus neutral)
      HIDE: terminate TabTip.exe
    Bring it back between tests with "SHOW: launch TabTip.exe", "SHOW: ITipInvocation::Toggle" or "SHOW: InputPane2::TryShow".
 9. Finally tap the native EDIT box on the right (Windows should show the keyboard by itself), answer visible,
    tap the keyboard's X, answer visible / not visible, tap the SURFACE box, answer again.
10. Close the window and send kbdprobe.log (in the same folder as the exe).

Nothing else is installed or changed; "terminate TabTip.exe" only kills the keyboard process (Windows restarts it on demand).

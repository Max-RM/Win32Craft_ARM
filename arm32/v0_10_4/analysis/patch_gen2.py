p = r'D:\repo\Win32Craft-main\arm32\v0_10_4\analysis\gen_client_init.py'
t = open(p, encoding='utf-8').read()
def rep(a, b):
    global t
    assert a in t, a[:60]
    t = t.replace(a, b)

rep("platform+0x38 = ((rawDpiX+rawDpiY)*0.5)/25.4-style constant;", "platform+0x38 = ((RawDpiX+RawDpiY)*0.5)/25.4f (float constant 25.4 at RVA 0x4B704; /96.0 double at 0x4B6FC);")
rep("dispatches by ev.type (signed byte at ev+0): 0/1 -> pointer/mouse event functions 0x115F84 / 0x116444 (args from short fields at ev+2,+4,+6,+7,+8), 2 -> Keyboard::feed 0x4E568 (key,down), 3 -> Keyboard::feedText 0x4E598 (text), 4 -> back button:",
    "dispatches by ev.type (signed byte at ev+0, tbb table at 0x4B3AE): 0 -> 0x115F84 (pointer/touch sink, args from shorts/bytes at ev+2,+4,+6,+7), 1 -> 0x116444 (mouse-button/wheel-like sink, extra arg byte ev+8), 2 -> Keyboard::feed 0x4E568 (byte ev+1, int ev+4), 3 -> Keyboard::feedText 0x4E598 (string ev+4, byte ev+0x1C), 4 -> back button:")
rep("+0x3C byte paused/\"game paused\" (read by update/tick; writer not found);", "+0x3C byte paused/\"game paused\" (read by update/tick; writer not found); +0x40 camera/game-mode-like object (vtbl[5] ticked by Minecraft::vf31, passed to 0x51740(obj,partial) in render);")
rep("(when +0x40 non-NULL updates +0x130 Gui +0xB0 and +0x108 LevelRenderer +0xC with 0x51740(partial))", "(when +0x40 is non-NULL stores 0x51740(+0x40,partial) into Gui+0xB0 and LevelRenderer+0xC)")
open(p, 'w', encoding='utf-8').write(t)
print('ok')

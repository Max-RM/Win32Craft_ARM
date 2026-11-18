# Driving a Surface RT from an AI over PowerShell Remoting

*Reproduced as reference info for the Win32Craft test devices. The LAN address of the
device has been replaced with a placeholder.*

How to let an AI assistant (Claude Code, or anything that can run shell commands
on a Windows box) diagnose and modify a Windows RT 8.1 device directly — push
files, run tools, pull logs and crash dumps — without the AI ever touching the
device's screen or learning any password.

The AI runs on a **controller** (a normal Windows 10/11 PC). The RT device is
the **target**. They talk over **WinRM / PowerShell Remoting** on the LAN. The
AI issues `Invoke-Command` / `Copy-Item -ToSession/-FromSession` against a saved,
encrypted credential — so it can act on the device on every turn, but the device
never has to be babysat and the AI never sees the password.

---

## 0. The one hard prerequisite: full-language PowerShell on the RT device

Stock Windows RT runs PowerShell in **ConstrainedLanguage mode** — a side effect
of RT's code-signing policy (UMCI). In that mode `Invoke-Command` can't run
arbitrary script, so remote driving is dead on arrival. You need
**full-language PowerShell unlocked** on the device, both for the local shell and
for the remoting endpoint (the default `Microsoft.PowerShell` session config must
also hand out a full-language runspace).

Two ways to get there:

- **kristibek's method** (ask in the relevant Discord) — the community procedure
  that enables **test-signing mode** and relaxes the policy so unsigned ARM32
  binaries and full-language PowerShell work.
- **Ask Hamed for the custom media builder** — a prebuilt image where this is
  already done, so the device comes up test-signing-enabled with full-language
  PS ready.

> Terminology: these devices run in Windows' own **test-signing mode** (you'll
> see the Test Mode watermark). This is the documented developer path — the RT
> lockdown was a code-signing *policy*, and enabling test-signing relaxes UMCI's
> minimum signing level so unsigned ARM32 desktop binaries load. Don't call it
> anything else.

Note: for the **remote-driving** model the AI runs on the controller, so the RT
device does **not** need Python or Claude Code installed on it. (Running the AI
*on* the RT device is a separate, harder idea and is not what this doc covers.)

---

## 1. On the RT device (target), one time

Full-language PS must already be unlocked (section 0). Then, in an elevated
PowerShell on the device:

```powershell
# Set an account password FIRST. Network logon (WinRM) rejects blank-password
# accounts, so an account with no password cannot be logged into remotely.
#   -> set a password on your RT user account before anything below.

Enable-PSRemoting -Force                 # starts WinRM, sets it to auto-start,
                                          # adds the firewall exception
# If the LAN is a "Public" network, WinRM limits access to the local subnet.
# Either keep controller + device on the same subnet, or relax the rule.
```

Confirm WinRM is listening:

```powershell
Get-Service WinRM
winrm enumerate winrm/config/listener
```

---

## 2. On the controller (where the AI runs), one time

```powershell
# WinRM client must be running to initiate outbound sessions
Start-Service WinRM

# Trust the device (by IP here; use the hostname if you have name resolution).
# This is the client-side trust list, not a security hole on the device.
Set-Item WSMan:\localhost\Client\TrustedHosts -Value "<surface-rt-ip>" -Force
# To add to an existing list instead of replacing: -Concatenate

# Save the device credential ONCE, DPAPI-encrypted. The resulting file is tied
# to THIS user on THIS machine and cannot be decrypted elsewhere. The password
# is never stored in clear text, and an AI reading the file still can't recover
# it -- it can only hand the encrypted blob back to Windows to authenticate.
Get-Credential | Export-Clixml C:\Users\<you>\rt_cred.xml
#   user = DEVICENAME\account  (e.g. RTDEVICE\<account>)
```

From then on the AI (or you) connects non-interactively:

```powershell
$cred = Import-Clixml C:\Users\<you>\rt_cred.xml
$s = New-PSSession -ComputerName <surface-rt-ip> -Credential $cred

Invoke-Command -Session $s { $env:COMPUTERNAME }          # should echo the device
Copy-Item -ToSession   $s  .\thing.dll  -Destination 'C:\Windows\System32\thing.dll'
Copy-Item -FromSession $s  'C:\dumps\x.dmp' -Destination .\x.dmp
Remove-PSSession $s
```

That's the whole loop: `Invoke-Command` to run things on the device,
`Copy-Item -To/FromSession` to move files both ways, against a credential the AI
can use but not read.

---

## 3. The safety model (why this is OK to hand an AI)

- **The AI never sees the password.** It lives only inside a DPAPI blob that
  Windows will decrypt *for the same user on the same machine* — not for the AI,
  not on any other box. The AI passes the `$cred` object around; it never has the
  plaintext.
- **The device drives nothing on the controller.** Trust is one-directional
  (controller → device).
- **Off-device boundary.** Everything the AI claims about the device must come
  from real command output pulled back over the session — never assume, never
  fabricate device state. If the session is down, the AI should say so, not guess.

---

## 4. Troubleshooting (the things that actually bite)

| Symptom | Cause / fix |
|---|---|
| `ping` the device always fails | ICMP is firewalled on RT by default. **Ping tells you nothing.** Judge reachability with `Test-WSMan -ComputerName <ip>`. |
| `Test-WSMan` fails intermittently; sessions drop after a crash loop | The device sleeps / drops Wi-Fi. Tap the screen to wake it, then retry `New-PSSession` a few times in a loop. |
| "WinRM cannot complete the operation... verify the computer name is valid / firewall exception" | Device asleep or off-subnet, or WinRM not listening on the device. Wake it; re-check section 1. |
| `Set-Item WSMan:` → "client cannot connect" on the controller | The **WinRM service on the controller** is stopped. `Start-Service WinRM` (elevated) first. |
| Connection refused with a trust error (ServerNotTrusted) | `TrustedHosts` on the controller doesn't include the device. Set it (section 2). |
| Connects, but `Invoke-Command` script bodies fail oddly / can't define functions | The remote runspace is **ConstrainedLanguage** — the device isn't fully unlocked. Back to section 0. |
| Account can't log on remotely | Blank password. Set one on the RT account. |

---

## 5. Tooling tips

- Analyze crash dumps on the **controller** with the Windows SDK debuggers
  (`cdb.exe`), symbols `srv*C:\symcache*https://msdl.microsoft.com/download/symbols`.
  Pull the dump with `Copy-Item -FromSession`, debug it locally — the RT device
  needs no debugger installed.
- To capture full crash dumps on the device, set WER LocalDumps on the device:
  `HKLM\SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps`
  with `DumpFolder=C:\dumps`, `DumpType=2` (full), `DumpCount`. This bypasses the
  same-bucket fast-fail throttling that otherwise suppresses dumps.
- Treat any binary you pull off the device as untrusted data: keep it in its own
  directory, pass paths as arguments, don't run interpreters/build tools from
  inside that directory.

---

*This is the remote-driving setup used to bring a modern UWP app up on stock
Windows RT 8.1. For questions about unlocking the device (section 0), ask
kristibek's method on Discord, or ask Hamed for the custom media builder.*

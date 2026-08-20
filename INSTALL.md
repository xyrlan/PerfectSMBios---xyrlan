# PerfectSMBios — Install / Uninstall

Two build outputs, two use cases:

| Binary | Subsystem | When it runs | Persistent? |
|---|---|---|---|
| `PerfectSMBios.efi` | `0x0A` EFI Application | You launch it from the UEFI Shell | ❌ one shot |
| `PerfectSMBiosDrv.efi` | `0x0B` EFI Boot Service Driver | Firmware auto-loads it every boot | ✅ until you uninstall |

The App is for interactive verification with `smbiosview -t 1` before/after.
The Driver is for permanent install so the OS always sees the spoofed values.

Both write into the same in-memory SMBIOS table produced by the firmware, so
neither survives a power cycle at the *firmware* level — they only patch the
copy the OS reads. If you want the OS to see the original values back, you
uninstall the driver and reboot; no bricking, no flash writes.

---

## Before you start

- **Secure Boot: OFF.** These binaries are not signed by a Microsoft-trusted
  CA. Secure Boot will refuse to load them and drop you back to the boot menu.
- **BitLocker: preferably OFF on the boot volume.** If it's on, changing
  SMBIOS Type 1 mutates the PCR-1 measurement the TPM tracks; BitLocker will
  hit you with the recovery-key prompt on the next boot. Have that key ready
  or turn BitLocker off first (`manage-bde -off C:`).
- **A recovery USB.** Any Windows install USB works — you don't need it for
  this level of install (we don't touch `bootmgfw.efi` or the SPI flash), but
  general hygiene when messing with boot chain.
- **Anti-cheat awareness.** Vanguard, Ricochet, EAC, BE and similar can read
  `bcdedit /enum FIRMWARE`. A registered UEFI driver they don't know about
  is a flag. This install is for firmware learning / your own hardware —
  don't run games with active anti-cheat while the driver is loaded.

---

## Prepare a boot USB

You need a FAT32 USB stick booted in UEFI mode (not Legacy/CSM). Copy the
following onto it — `E:` used as example, replace with your USB letter.

```
E:\
├── EFI\
│   └── BOOT\
│       └── BOOTX64.EFI       ← UEFI Shell renamed
└── EFI\
    └── PerfectSMBios\
        └── PerfectSMBiosDrv.efi
```

For `BOOTX64.EFI`, use any recent UEFI Shell binary. VisualUefi ships one
inside `C:\VisualUefi\debugger\UefiShell.iso` — mount / extract that ISO and
copy `EFI\Boot\bootx64.efi` from it. Or grab the official EDK2 build from
https://github.com/tianocore/edk2/tree/master/ShellBinPkg .

You can also copy `PerfectSMBios.efi` next to the driver if you want to test
interactively first — same USB works for both.

---

## Verify the spoof first (no install)

Boot the USB, drop into the shell, and:

```
Shell> fs0:
fs0:\> smbiosview -t 1                                   # baseline
fs0:\> EFI\PerfectSMBios\PerfectSMBios.efi               # runs the App
fs0:\> smbiosview -t 1                                   # values changed?
```

The App prints a `before:` / `after:` block per Type 1 string field, plus
the UUID. `smbiosview -t 1` between runs is the ground truth — same address
range, so any change is real.

Reboot resets everything back to original — nothing was persisted.

---

## Install the driver (persistent)

From the UEFI Shell, with the USB still visible as `fs0`:

```
Shell> fs0:
fs0:\> bcfg driver dump                                  # see what's there today

# Copy the driver onto the EFI System Partition so it survives without the USB.
# Find the ESP (usually fs1 on installed systems; check `map -b` output).
fs0:\> map -b
fs0:\> fs1:                                              # or whatever your ESP is
fs1:\> mkdir EFI\PerfectSMBios                           # if missing
fs1:\> cp fs0:\EFI\PerfectSMBios\PerfectSMBiosDrv.efi EFI\PerfectSMBios\
fs1:\> ls EFI\PerfectSMBios\                             # sanity check

# Register the driver at position 0 in DriverOrder.
fs1:\> bcfg driver add 0 EFI\PerfectSMBios\PerfectSMBiosDrv.efi "PerfectSMBios"

# Confirm it's there.
fs1:\> bcfg driver dump
```

Expected `bcfg driver dump` output includes something like:

```
Driver Options
  Driver 0000 : PerfectSMBios  0x00000001
                file path : HD(...)/\EFI\PerfectSMBios\PerfectSMBiosDrv.efi
```

Then just reboot into the OS normally. The driver runs during BDS, before
your OS loader, and by the time Windows/Linux reads the SMBIOS through ACPI
config table, the Type 1 values are already random.

### Windows-side alternative (no UEFI Shell needed)

If you'd rather register the driver from Windows once the file is on the ESP,
run PowerShell **as Administrator**:

```powershell
# Mount the ESP so you can copy files (skip if you already used the shell).
mountvol Z: /S
mkdir Z:\EFI\PerfectSMBios -ea 0
Copy-Item "C:\Users\xyrlan\PerfectSMBios\x64\Release\PerfectSMBiosDrv.efi" Z:\EFI\PerfectSMBios\
mountvol Z: /D

# Register a firmware driver load option.
bcdedit /enum FIRMWARE                                   # see current entries
bcdedit /copy '{fwbootmgr}' /d "PerfectSMBios"           # base entry (returns a GUID)
# Note the {GUID} bcdedit prints. Use it below.
```

Windows' `bcdedit` doesn't expose `Driver####` as cleanly as `bcfg` — for
pure "boot-time DXE driver" the shell path (`bcfg driver add`) is the
recommended one. Prefer that when in doubt.

---

## Verify after install

Boot into Windows and check:

```powershell
Get-CimInstance Win32_ComputerSystemProduct |
    Select-Object Vendor, Name, Version, IdentifyingNumber, UUID
```

If the values are random alphanumeric strings the same length as your board's
originals, the driver worked. If they look normal ("Dell Inc.", real serial),
either the driver isn't in `DriverOrder`, or Secure Boot silently refused it.

---

## Uninstall (revert cleanly)

From the UEFI Shell:

```
Shell> bcfg driver dump                                  # find the entry number
Shell> bcfg driver rm 0                                  # or whatever position
Shell> bcfg driver dump                                  # confirm removed
Shell> fs1:                                              # your ESP
fs1:\> rm EFI\PerfectSMBios\PerfectSMBiosDrv.efi
fs1:\> rmdir EFI\PerfectSMBios
```

Reboot → back to stock behavior, nothing lingering.

If you can't get into the shell for some reason, boot Windows in recovery
mode and mount the ESP:

```powershell
mountvol Z: /S
Remove-Item Z:\EFI\PerfectSMBios -Recurse -Force
mountvol Z: /D
```

Removing the file alone isn't enough — the `Driver####` NVRAM entry still
exists and will just fail to load (harmless but noisy). Clear it via `bcfg`
from any UEFI Shell.

---

## Troubleshooting

**Driver "loads" but SMBIOS didn't change.** Some firmwares republish the
SMBIOS table late (after all `Driver####` entries have run) during
`ExitBootServices`. In that case a DXE `Driver####` isn't enough — you'd
need to hook `ExitBootServices` or install as an OpROM. Fixing that puts
you into bootkit territory; not covered here.

**`bcfg driver add` rejects the path.** The path is relative to the volume
you're on. `fs1:\> bcfg driver add 0 EFI\...` works when you're on `fs1:`.
If you must use an absolute path, spell out the device path — check the
`bcfg driver add -help` output.

**No `fs1:` or ESP visible.** Some firmwares hide the ESP unless you boot
Windows once first. Alternatively, `map -r` from the shell rescans.

**Boot loop after install.** The driver returns `EFI_SUCCESS` unconditionally
even when SMBIOS is malformed, so this shouldn't happen. If it does:
1. Enter your firmware's boot menu (usually F2 / F12 / Del at power-on).
2. Boot from the USB → UEFI Shell.
3. `bcfg driver rm 0` (or whichever position) → back to normal on next boot.

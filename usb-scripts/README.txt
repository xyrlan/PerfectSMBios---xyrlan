PerfectSMBios USB - Quick reference
====================================

Boot this USB stick in UEFI mode (Secure Boot OFF).
The shell auto-runs startup.nsh which prints this menu.

DEFAULT commands after 'fs0:':

    test.nsh              One-shot spoof, no persistence.
                          Reboot restores everything.

    install.nsh N         Install persistent driver where N is the
                          Windows ESP fs number (usually 1).
                          Reboot -> Windows sees spoofed SMBIOS
                          forever, until you uninstall.

    uninstall.nsh N       Remove driver + files. N = same ESP fs#.

    smbiosview -t 1       Read current SMBIOS System Information.
                          Great to compare before/after.

    exit                  Back to firmware boot menu.


USB layout:
    /BOOTX64.EFI                      -- the UEFI Shell (optional)
    /EFI/BOOT/BOOTX64.EFI             -- same, for firmwares that
                                         only look in EFI/BOOT/
    /PerfectSMBios.efi                -- one-shot application
    /EFI/PerfectSMBios/PerfectSMBiosDrv.efi   -- persistent driver
    /startup.nsh                      -- auto-runs at shell start
    /test.nsh /install.nsh /uninstall.nsh
    /README.txt                       -- this file


Prerequisites in your firmware settings BEFORE booting this USB:
    - Secure Boot: OFF   (else the .efi files won't load, silent fail)
    - Boot mode:   UEFI  (not Legacy / CSM)
    - Boot order:  USB first (or use the one-time boot menu, F12 / F11 / Esc)

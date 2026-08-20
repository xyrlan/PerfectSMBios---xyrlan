@echo -off
echo ""
echo   === PerfectSMBios USB - UEFI Shell ===
echo ""
echo   You are in the UEFI Shell. Volumes on this system:
echo ""
map -b
echo ""
echo   Typical layout:
echo     - fs0:   this USB stick (has these .nsh scripts)
echo     - fs1:   the Windows ESP (has EFI\Microsoft\Boot\bootmgfw.efi)
echo   Confirm above which is which before running install.
echo ""
echo   Then type:
echo ""
echo     fs0:                              switch to the USB volume
echo     test.nsh                          run one-shot spoof, no changes
echo     install.nsh 1                     install persistent (1 = ESP fs#)
echo     uninstall.nsh 1                   remove and clean up
echo     smbiosview -t 1                   read the current System Info
echo     exit                              back to boot menu
echo ""

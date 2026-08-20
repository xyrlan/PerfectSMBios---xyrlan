@echo -off
if "%1" == "" then
    echo Usage:  uninstall.nsh ^<esp-fs-number^>
    echo ""
    echo Example:  uninstall.nsh 1       if fs1: is your Windows ESP
    goto END
endif

set _ESP fs%1:
echo ""
echo === Uninstalling PerfectSMBiosDrv from %_ESP% ===
echo ""

echo Current DriverOrder BEFORE:
bcfg driver dump

echo ""
echo Removing DriverOrder entry at position 0 ...
echo (If your PerfectSMBios entry is at a different number, run 'bcfg driver rm N' manually.)
bcfg driver rm 0

echo Deleting files ...
if exist %_ESP%\EFI\PerfectSMBios\PerfectSMBiosDrv.efi then
    rm %_ESP%\EFI\PerfectSMBios\PerfectSMBiosDrv.efi
endif
if exist %_ESP%\EFI\PerfectSMBios then
    rm %_ESP%\EFI\PerfectSMBios
endif

echo ""
echo Current DriverOrder AFTER:
bcfg driver dump

echo ""
echo === Uninstall complete ===
:END
set _ESP ""

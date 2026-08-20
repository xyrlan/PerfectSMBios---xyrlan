@echo -off
if "%1" == "" then
    echo Usage:  install.nsh ^<esp-fs-number^>
    echo ""
    echo Example:  install.nsh 1        if fs1: is your Windows ESP
    echo ""
    echo Available volumes (look for the Fixed one ~100-500MB with EFI\Microsoft):
    map -b
    goto END
endif

set _ESP fs%1:
echo ""
echo === Installing PerfectSMBiosDrv into %_ESP% ===
echo ""

if not exist %_ESP%\EFI\Microsoft\Boot\bootmgfw.efi then
    echo WARNING: %_ESP%\EFI\Microsoft\Boot\bootmgfw.efi not found.
    echo %_ESP% probably is NOT the Windows ESP. Aborting to be safe.
    echo Re-check 'map -b' output and pass the correct number.
    goto END
endif

if not exist .\EFI\PerfectSMBios\PerfectSMBiosDrv.efi then
    echo ERROR: .\EFI\PerfectSMBios\PerfectSMBiosDrv.efi not found.
    echo Did you 'fs0:' (or wherever the USB is) before running this?
    goto END
endif

echo Copying driver to %_ESP%\EFI\PerfectSMBios\ ...
if not exist %_ESP%\EFI\PerfectSMBios then
    mkdir %_ESP%\EFI\PerfectSMBios
endif
cp .\EFI\PerfectSMBios\PerfectSMBiosDrv.efi %_ESP%\EFI\PerfectSMBios\

if not exist %_ESP%\EFI\PerfectSMBios\PerfectSMBiosDrv.efi then
    echo ERROR: copy failed. Aborting.
    goto END
endif

echo Registering DriverOrder entry at position 0 ...
bcfg driver add 0 %_ESP%\EFI\PerfectSMBios\PerfectSMBiosDrv.efi "PerfectSMBios"

echo ""
echo === Current DriverOrder ===
bcfg driver dump

echo ""
echo === Install complete ===
echo Reboot into Windows. Verify with:
echo     Get-CimInstance Win32_ComputerSystemProduct
echo Values should be random alphanumerics of same length as originals.
:END
set _ESP ""

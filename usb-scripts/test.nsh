@echo -off
echo ""
echo === One-shot spoof (NO firmware changes) ===
echo ""
echo Reboot will restore original values. Confirm the effect with:
echo     smbiosview -t 1
echo before and after this script.
echo ""
if not exist .\PerfectSMBios.efi then
    echo ERROR: PerfectSMBios.efi not found in current dir.
    echo Did you 'fs0:' before running this?
    goto END
endif

.\PerfectSMBios.efi

echo ""
echo Done. Run 'smbiosview -t 1' to see the spoofed values.
:END

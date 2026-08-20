//
// PerfectSMBiosDrv — persistent (per-boot) DXE-phase SMBIOS Type 1 spoofer.
//
// This is the "driver" flavour of the PerfectSMBios sample: instead of an
// EFI Application you launch from the UEFI Shell, this is a DXE Boot Service
// Driver you register in NVRAM (DriverOrder), so the firmware auto-loads it
// on every boot BEFORE handing control to the OS bootloader.
//
// Install/uninstall (UEFI Shell):
//   fs0:\> bcfg driver add 0 fs0:\EFI\PerfectSMBios\PerfectSMBiosDrv.efi "PSMB"
//   fs0:\> bcfg driver dump                                  ; verify
//   fs0:\> bcfg driver rm  0                                 ; uninstall
//
// The driver:
//   1. Looks up the SMBIOS entry point (v2 or v3) from the EFI System Table.
//   2. Locates the Type 1 (System Information) structure in the table.
//   3. Overwrites Manufacturer, ProductName, Version, SerialNumber, SKUNumber,
//      Family and Uuid with random data of the same length.
//   4. Returns EFI_SUCCESS and stays resident. BDS then loads the OS boot
//      manager, which reads the (now spoofed) SMBIOS via the EFI Config Table.
//
// Notes vs. the App version:
//   - Silent by default. Prints during DXE go to the firmware log (if any)
//     and can visibly slow / mess with graphical boot. Toggle PSMB_VERBOSE
//     to 1 for on-screen output when you're validating.
//   - No `Print` when PSMB_VERBOSE=0 → nothing hits ConOut.
//   - We do NOT install any protocol or hook Boot Services — pure one-shot.
//
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/ShellLib.h>
#include "..\PerfectSMBios\PerfectSMBios\PerfectSMBios.h"

// Compile-time verbosity switch. Leave 0 for a real install; flip to 1 to
// see what the driver did during boot (useful in QEMU / on a test bench).
#ifndef PSMB_VERBOSE
#define PSMB_VERBOSE 0
#endif

#if PSMB_VERBOSE
#  define PSMB_LOG(...) Print(__VA_ARGS__)
#else
#  define PSMB_LOG(...) do { } while (0)
#endif


//
// UefiDriverEntryPoint required globals
//
const UINT32 _gUefiDriverRevision       = 0x200;    // require UEFI 2.0+
const UINT32 _gDxeRevision              = 0x200;
const UINT8  _gDriverUnloadImageCount   = 1;        // we register an unload
CHAR8*       gEfiCallerBaseName         = "PerfectSMBiosDrv";


//
// Deny driver unload. Once we've patched SMBIOS we don't need to be around,
// but we also don't want the firmware unloading us mid-boot for any reason.
//
EFI_STATUS EFIAPI UefiUnload( IN EFI_HANDLE ImageHandle )
{
    return EFI_ACCESS_DENIED;
}


//
// Same string-field spoof helper the App version uses, minus the pretty
// before/after prints. Returns silently on any missing / empty field so a
// single missing string doesn't take the driver down mid-boot.
//
static void SpoofStringField( SMBIOS_STRUCTURE* hdr, UINT8 str_idx )
{
    if ( !hdr || str_idx == 0 ) return;

    CHAR8* ascii = PSMB_GetSMBiosString( hdr, str_idx );
    if ( !ascii || *ascii == '\0' ) return;

    UINTN  len  = AsciiStrLen( ascii );
    CHAR8* rand = PSMB_GenRandASCIIString( len );
    if ( !rand ) return;

    CopyMem( ascii, rand, len );
    FreePool( rand );
}


//
// Fill `len` bytes at `dst` with pseudo-random bytes (TSC-seeded LCG).
//
static void SpoofBytes( UINT8* dst, UINTN len )
{
    for ( UINTN i = 0; i < len; i++ )
    {
        dst[ i ] = (UINT8)PSMB_GenRandNumber( 0, 255 );
    }
}


EFI_STATUS EFIAPI UefiMain( IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE* SystemTable )
{
    void*  table_base = NULL;
    UINTN  table_len  = 0;

    PSMB_LOG( L"[PSMB-DRV] loaded\n" );

    /* Prefer SMBIOS v2 entry, fall back to v3. Same order the App uses. */
    SMBIOS_TABLE_ENTRY_POINT* entry2_0 = PSMB_GetSMBiosTableEntry( SystemTable );
    if ( entry2_0 )
    {
        table_base = ( void* )( ( UINTN )entry2_0->TableAddress );
        table_len  = entry2_0->TableLength;
    }
    else
    {
        SMBIOS_TABLE_3_0_ENTRY_POINT* entry3_0 = PSMB_GetSMBios3TableEntry( SystemTable );
        if ( !entry3_0 )
        {
            PSMB_LOG( L"[PSMB-DRV] no SMBIOS entry point in config table\n" );
            /* Return SUCCESS anyway — an unusable firmware shouldn't wedge the boot. */
            return EFI_SUCCESS;
        }
        table_base = ( void* )( ( UINTN )entry3_0->TableAddress );
        table_len  = entry3_0->TableMaximumSize;
    }

    SMBIOS_TABLE_TYPE1* sys_info = PSMB_GetTargetTable( PSMB_SystemInformation, table_base, table_len );
    if ( !sys_info )
    {
        PSMB_LOG( L"[PSMB-DRV] Type 1 not found\n" );
        return EFI_SUCCESS;
    }

    /* Strings (always safe: SpoofStringField no-ops on idx=0 / empty). */
    SpoofStringField( &sys_info->Hdr, sys_info->Manufacturer );
    SpoofStringField( &sys_info->Hdr, sys_info->ProductName  );
    SpoofStringField( &sys_info->Hdr, sys_info->Version      );
    SpoofStringField( &sys_info->Hdr, sys_info->SerialNumber );

    /* UUID needs SMBIOS 2.1+ (Hdr.Length must cover the field). */
    if ( sys_info->Hdr.Length >= ( OFFSET_OF( SMBIOS_TABLE_TYPE1, Uuid ) + sizeof( sys_info->Uuid ) ) )
    {
        SpoofBytes( (UINT8*)&sys_info->Uuid, sizeof( sys_info->Uuid ) );
    }

    /* SKUNumber / Family need SMBIOS 2.4+. */
    if ( sys_info->Hdr.Length >= 0x1B )
    {
        SpoofStringField( &sys_info->Hdr, sys_info->SKUNumber );
        SpoofStringField( &sys_info->Hdr, sys_info->Family    );
    }

    PSMB_LOG( L"[PSMB-DRV] Type 1 spoofed\n" );
    return EFI_SUCCESS;
}

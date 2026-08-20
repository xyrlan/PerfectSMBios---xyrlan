//
// Basic UEFI Libraries
//
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/ShellLib.h>
#include ".\PerfectSMBios\PerfectSMBios.h"

/* UEFI Boilerplate */
extern CONST UINT32 _gUefiDriverRevision = 1;
CHAR8 *gEfiCallerBaseName = "PerfectSMBios";



//
// UEFI Unload | Won't be called
//
EFI_STATUS EFIAPI UefiUnload ( IN EFI_HANDLE ImageHandle ) { ASSERT(FALSE); }



//
// Local helpers ( kept static — do not touch the public PSMB_* API )
//

/*
 * Print an SMBIOS ASCII string as UEFI wide chars, prefixed by a label.
 * Safe for NULL / empty inputs.
 */
static void PrintSmbiosString( CONST CHAR16* label, CHAR8* ascii )
{
    if ( !ascii || *ascii == '\0' )
    {
        Print( L"    %s: <empty>\n", label );
        return;
    }

    UINTN   len     = AsciiStrLen( ascii );
    CHAR16* unicode = AllocateZeroPool( ( len + 1 ) * sizeof( CHAR16 ) );
    if ( !unicode )
    {
        Print( L"    %s: <alloc failed>\n", label );
        return;
    }

    AsciiStrToUnicodeStr( ascii, unicode );
    Print( L"    %s: %s\n", label, unicode );
    FreePool( unicode );
}


/*
 * Spoof one SMBIOS string field addressed by its 1-based index inside `hdr`.
 * Overwrites in-place with a same-length random ASCII string so the table
 * layout (double-NUL terminator, following structures) is preserved.
 *
 * Silently skips when:
 *   - str_idx == 0                    (SMBIOS "unused" convention)
 *   - the resolved string is empty    (nothing to overwrite)
 *   - alloc fails                     (leaves original intact)
 *
 * Also frees the buffer returned by PSMB_GenRandASCIIString, which the
 * original main.c leaked.
 */
static void SpoofStringField( SMBIOS_STRUCTURE* hdr, UINT8 str_idx, CONST CHAR16* label )
{
    if ( !hdr || str_idx == 0 )
    {
        Print( L"(-) %s: field not present (idx=0)\n", label );
        return;
    }

    CHAR8* ascii = PSMB_GetSMBiosString( hdr, str_idx );
    if ( !ascii || *ascii == '\0' )
    {
        Print( L"(-) %s: empty string, skipping\n", label );
        return;
    }

    UINTN  len  = AsciiStrLen( ascii );
    CHAR8* rand = PSMB_GenRandASCIIString( len );
    if ( !rand )
    {
        Print( L"(-) %s: alloc failed, skipping\n", label );
        return;
    }

    Print( L"(+) %s (len=%u)\n", label, (UINT32)len );
    PrintSmbiosString( L"before", ascii );

    /* In-place overwrite of exactly `len` bytes — trailing '\0' from the
       original string stays where it was, so nothing after it shifts.    */
    CopyMem( ascii, rand, len );

    PrintSmbiosString( L"after ", ascii );
    FreePool( rand );
}


/*
 * Fill `len` bytes at `dst` with pseudo-random bytes.
 *
 * NOTE: PSMB_GenRandNumber is a TSC-seeded LCG — good enough for making
 * a hardware fingerprint "different", not cryptographically random. If you
 * need real entropy, swap in EFI_RNG_PROTOCOL here.
 */
static void SpoofBytes( UINT8* dst, UINTN len )
{
    for ( UINTN i = 0; i < len; i++ )
    {
        dst[ i ] = (UINT8)PSMB_GenRandNumber( 0, 255 );
    }
}


/*
 * Pretty-print a 16-byte UUID in the standard 8-4-4-4-12 form.
 * SMBIOS stores the first three groups little-endian on the wire, but
 * for a quick "before/after" this raw byte dump is enough.
 */
static void PrintUuid( CONST CHAR16* label, UINT8* uuid )
{
    Print( L"    %s: %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
        label,
        uuid[  0 ], uuid[  1 ], uuid[  2 ], uuid[  3 ],
        uuid[  4 ], uuid[  5 ],
        uuid[  6 ], uuid[  7 ],
        uuid[  8 ], uuid[  9 ],
        uuid[ 10 ], uuid[ 11 ], uuid[ 12 ], uuid[ 13 ], uuid[ 14 ], uuid[ 15 ] );
}



//
// Calling the UEFI Main
//
EFI_STATUS EFIAPI UefiMain ( IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE* SystemTable )
{
    /* Vars */
    EFI_STATUS  status      = EFI_SUCCESS;
    void*       table_base  = NULL;
    UINTN       table_len   = 0;


    /* Logs */
    SystemTable->ConOut->ClearScreen( SystemTable->ConOut );
    Print( L"(+) Welcome to PerfectSMBios:\n" );


    /* Getting the table pointer */
    SMBIOS_TABLE_ENTRY_POINT* entry2_0 = PSMB_GetSMBiosTableEntry( SystemTable );
    if ( entry2_0 )
    {
        table_base = ( void* )( ( UINTN )entry2_0->TableAddress );
        table_len  = entry2_0->TableLength;
    }
    else
    {
        /* If it's version 3 this will work otherwise it's unsupported */
        SMBIOS_TABLE_3_0_ENTRY_POINT* entry3_0 = PSMB_GetSMBios3TableEntry( SystemTable );
        if ( !entry3_0 ) return EFI_UNSUPPORTED;

        /* Saving the table base */
        table_base = ( void* )( ( UINTN )entry3_0->TableAddress );
        table_len  = entry3_0->TableMaximumSize;
    }


    /* Logs */
    Print( L"(+) Table: 0x%p (len=%u)\n", table_base, (UINT32)table_len );


    /* Getting the table 1 (System Information) */
    SMBIOS_TABLE_TYPE1* sys_info = PSMB_GetTargetTable( PSMB_SystemInformation, table_base, table_len );
    if ( !sys_info )
    {
        Print( L"(!) Type 1 (System Information) not found\n" );
        return EFI_NOT_FOUND;
    }

    Print( L"(+) Type 1 @ 0x%p (Hdr.Length=%u)\n", sys_info, sys_info->Hdr.Length );


    //
    // ------ Spoof every string field of Type 1 ------
    //
    // Layout (SMBIOS spec):
    //   0x04 Manufacturer   (BYTE, string index)  — SMBIOS 2.0+
    //   0x05 ProductName    (BYTE, string index)  — SMBIOS 2.0+
    //   0x06 Version        (BYTE, string index)  — SMBIOS 2.0+
    //   0x07 SerialNumber   (BYTE, string index)  — SMBIOS 2.0+
    //   0x08 Uuid           (16 bytes)            — SMBIOS 2.1+  (Hdr.Length >= 0x19)
    //   0x18 WakeUpType     (BYTE)                — not spoofed (enum)
    //   0x19 SKUNumber      (BYTE, string index)  — SMBIOS 2.4+  (Hdr.Length >= 0x1B)
    //   0x1A Family         (BYTE, string index)  — SMBIOS 2.4+  (Hdr.Length >= 0x1B)
    //
    SpoofStringField( &sys_info->Hdr, sys_info->Manufacturer, L"Manufacturer" );
    SpoofStringField( &sys_info->Hdr, sys_info->ProductName,  L"ProductName"  );
    SpoofStringField( &sys_info->Hdr, sys_info->Version,      L"Version"      );
    SpoofStringField( &sys_info->Hdr, sys_info->SerialNumber, L"SerialNumber" );

    /* UUID lives inside the fixed area of the struct, not in the string pool.
       Only present when the struct is long enough (SMBIOS >= 2.1).           */
    if ( sys_info->Hdr.Length >= ( OFFSET_OF( SMBIOS_TABLE_TYPE1, Uuid ) + sizeof( sys_info->Uuid ) ) )
    {
        Print( L"(+) Uuid\n" );
        PrintUuid( L"before", (UINT8*)&sys_info->Uuid );
        SpoofBytes( (UINT8*)&sys_info->Uuid, sizeof( sys_info->Uuid ) );
        PrintUuid( L"after ", (UINT8*)&sys_info->Uuid );
    }
    else
    {
        Print( L"(-) Uuid: struct too short (SMBIOS < 2.1), skipping\n" );
    }

    /* SKUNumber / Family only exist on SMBIOS 2.4+ structs. */
    if ( sys_info->Hdr.Length >= 0x1B )
    {
        SpoofStringField( &sys_info->Hdr, sys_info->SKUNumber, L"SKUNumber" );
        SpoofStringField( &sys_info->Hdr, sys_info->Family,    L"Family"    );
    }
    else
    {
        Print( L"(-) SKUNumber/Family: struct too short (SMBIOS < 2.4), skipping\n" );
    }


    Print( L"(+) Type 1 spoof complete\n" );
    return status;
}

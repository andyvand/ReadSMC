/** @file
  This is a test application that demonstrates how to use the C-style entry point
  for a shell application.

  Copyright (c) 2009 - 2011, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/ShellCEntryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include "AppleSMC.h"

#define READ_BUFFER(B, L, I, V, T)											do{if((I) >= (L)) return EFI_BAD_BUFFER_SIZE; (V) = (T)((B)[(I)]); (I) += 1;}while(0)
#define WRITE_BUFFER(B, L, I, V, T)											do{if((I) >= (L) - 1) return EFI_BUFFER_TOO_SMALL; (B)[(I)] = (T)(V); (I) += 1;}while(0)

//
// unicode to utf8
//
EFI_STATUS UnicodeToUtf8(CHAR16 CONST* unicodeBuffer, UINTN unicodeCharCount, UINT8* utf8Buffer, UINTN utf8BufferLength)
{
	UINT32 j																= 0;
	utf8Buffer[utf8BufferLength - 1]										= 0;
    UINT32 i = 0;

	for(i = 0; i < unicodeCharCount; i ++)
	{
		CHAR16 unicodeChar													= unicodeBuffer[i];
		if(unicodeChar < 0x0080)
		{
			WRITE_BUFFER(utf8Buffer, utf8BufferLength, j, unicodeChar, UINT8);
		}
		else if(unicodeChar < 0x0800)
		{
			WRITE_BUFFER(utf8Buffer, utf8BufferLength, j, ((unicodeChar >>  6) & 0x0f) | 0xc0, UINT8);
			WRITE_BUFFER(utf8Buffer, utf8BufferLength, j, ((unicodeChar >>  0) & 0x3f) | 0x80, UINT8);
		}
		else
		{
			WRITE_BUFFER(utf8Buffer, utf8BufferLength, j, ((unicodeChar >> 12) & 0x0f) | 0xe0, UINT8);
			WRITE_BUFFER(utf8Buffer, utf8BufferLength, j, ((unicodeChar >>  6) & 0x3f) | 0x80, UINT8);
			WRITE_BUFFER(utf8Buffer, utf8BufferLength, j, ((unicodeChar >>  0) & 0x3f) | 0x80, UINT8);
		}
	}
    
	if(j < utf8BufferLength - 1)
		WRITE_BUFFER(utf8Buffer, utf8BufferLength, j, 0, UINT8);
    
	return EFI_SUCCESS;
}

CHAR8 *Utf8FromUnicode(CHAR16 CONST* unicodeString, UINTN unicodeCharCount)
{
	UINTN length															= unicodeCharCount == -1 ? StrLen(unicodeString) : unicodeCharCount;
	UINTN utf8BufferLength													= (length * 3 + 1) * sizeof(CHAR8);
	UINT8 *utf8NameBuffer													= (UINT8 *)(AllocateZeroPool(utf8BufferLength));
	if(!utf8NameBuffer)
		return NULL;
    
	if(!EFI_ERROR(UnicodeToUtf8(unicodeString, length, utf8NameBuffer, utf8BufferLength / sizeof(CHAR8))))
		return (CHAR8 *)utf8NameBuffer;
    
	FreePool(utf8NameBuffer);
	return NULL;
}

VOID Usage(IN CHAR16 *Name)
{
    Print(L"AppleSMC Key Reader\n");
    Print(L"Usage: %s <KeyName> <Length>\n", Name);
    Print(L"Copyright (C) 2014 AnV Software\n");
}

/**
  UEFI application entry point which has an interface similar to a
  standard C main function.

  The ShellCEntryLib library instance wrappers the actual UEFI application
  entry point and calls this ShellAppMain function.

  @param[in] Argc     The number of items in Argv.
  @param[in] Argv     Array of pointers to strings.

  @retval  0               The application exited normally.
  @retval  Other           An error occurred.

**/
INTN
EFIAPI
ShellAppMain (
  IN UINTN Argc,
  IN CHAR16 **Argv
  )
{
    EFI_STATUS Status;
    EFI_GUID SMCGUID = APPLE_SMC_PROTOCOL_GUID;
    APPLE_SMC_PROTOCOL *SMCP;
    CHAR8 *SMCK = AllocateZeroPool(6);
    CHAR8 *SMCKR = AllocateZeroPool(6);
    UINT32 SMCKID;
    UINT32 SMCKL;
    UINT32 SMCKLP;
    UINT8 *SMCKD;

    if (Argc != 3)
    {
        Usage(Argv[0]);

        return 1;
    }

    SMCKR = Utf8FromUnicode(Argv[1], StrLen(Argv[1]));
    
    //Reverse string because UEFI integers are little-endian
    //Not doing this would cause SMCKID to be backwards
    int j = 0;
    for(int i = (AsciiStrLen(SMCKR) - 1); i >= 0; i--) {
        SMCK[j] = SMCKR[i];
        j += 1;
    }
    SMCKID = *(UINT32 *)SMCK;
    FreePool(SMCKR);
    SMCKL = (UINT32)StrDecimalToUint64(Argv[2]);

    Status = gBS->LocateProtocol (
                                  &SMCGUID,
                                  NULL,
                                  (VOID **)&SMCP
                                  );

    if (EFI_ERROR(Status))
    {
        Print(L"ERROR: Could not locate Apple SMC Protocol, no SMC read possible\n");

        return -1;
    }

    Print(L"Signature: 0x%llx\n", SMCP->Signature);

    SMCKD = AllocateZeroPool(SMCKL);

    Status = SMCP->ReadData(SMCP, SMCKID, SMCKL, SMCKD);

    if (EFI_ERROR(Status))
    {
        Print(L"ERROR: Could not read SMC key named %s for length %u\n", Argv[1], SMCKL);

        return -1;
    };

    Print(L"%s: [ ", Argv[1]);

    SMCKLP = 0;
    while (SMCKLP < SMCKL)
    {
        Print(L"%.2X ", *SMCKD);

        ++SMCKLP;
        ++SMCKD;
    }

    Print(L"]\n");
    
    FreePool(SMCKD);
    FreePool(SMCK);
    
    return 0;
}

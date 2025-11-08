// =====================================================================================
// Copyright (c) 2025 Dave Bernazzani (wavemotion-dave)
//
// Copying and distribution of this emulator, its source code and associated
// readme files, with or without modification, are permitted in any medium without
// royalty provided this copyright notice is used and wavemotion-dave and Marat
// Fayzullin (Z80 core) are thanked profusely.
//
// The SpeccySE emulator is offered as-is, without any warranty. Please see readme.md
// =====================================================================================
#include <nds.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fat.h>
#include <dirent.h>

#include "SpeccySE.h"
#include "CRC32.h"
#include "cpu/z80/Z80_interface.h"
#include "SpeccyUtils.h"
#include "printf.h"

// -----------------------------------------------------
// Z80 Snapshot v1 is always a 48K game...
// The header is 30 bytes long - most of which will be
// used when we reset the game to set the state of the
// CPU registers, Stack Pointer and Program Counter.
// -----------------------------------------------------
u8 decompress_v1(int romSize)
{
    int offset = 0; // Current offset into memory

    u8 isCompressed = (ROM_Memory[12] & 0x20 ? 1:0); // V1 files are usually compressed

    for (int i = 30; i < romSize; i++)
    {
        if (offset > 0xC000)
        {
            break;
        }

        // V1 headers always end in 00 ED ED 00
        if (ROM_Memory[i] == 0x00 && ROM_Memory[i + 1] == 0xED && ROM_Memory[i + 2] == 0xED && ROM_Memory[i + 3] == 0x00)
        {
            break;
        }

        if (i < romSize - 3)
        {
            if (ROM_Memory[i] == 0xED && ROM_Memory[i + 1] == 0xED && isCompressed)
            {
                i += 2;
                word repeat = ROM_Memory[i++];
                byte value = ROM_Memory[i];
                for (int j = 0; j < repeat; j++)
                {
                    RAM_Memory[0x4000 + offset++] = value;
                }
            }
            else
            {
                RAM_Memory[0x4000 + offset++] = ROM_Memory[i];
            }
        }
        else
        {
            RAM_Memory[0x4000 + offset++] = ROM_Memory[i];
        }
    }

    return 0; // 48K Spectrum
}

// ---------------------------------------------------------------------------------------------
// Z80 Snapshot v2 or v2 could be 48K game but is usually a 128K game. The header will tell us.
// ---------------------------------------------------------------------------------------------
u8 decompress_v2_v3(int romSize)
{
    int offset;

    word extHeaderLen = 30 + ROM_Memory[30] + 2;

    // Uncompress all the data and store into the proper place in our buffers
    int idx = extHeaderLen;
    while (idx < romSize)
    {
        u8 isCompressed = 1;
        word compressedLen = ROM_Memory[idx] | (ROM_Memory[idx+1] << 8);
        if (compressedLen == 0xFFFF) {isCompressed = 0; compressedLen = (16*1024);}
        if (compressedLen > 0x4000) compressedLen = 0x4000;
        u8 pageNum = ROM_Memory[idx+2];
        u8 *UncompressedData = RAM_Memory128 + ((pageNum-3) * 0x4000);
        idx += 3;
        offset = 0x0000;
        for (int i=0; i<compressedLen; i++)
        {
            if (i < compressedLen - 3)
            {
                if (ROM_Memory[idx+i] == 0xED && ROM_Memory[idx+i + 1] == 0xED && isCompressed)
                {
                    i += 2;
                    u16 repeat = ROM_Memory[idx + i++];
                    byte value = ROM_Memory[idx + i];
                    for (int j = 0; j < repeat; j++)
                    {
                        UncompressedData[offset++] = value;
                    }
                }
                else
                {
                    UncompressedData[offset++] = ROM_Memory[idx+i];
                }
            }
            else
            {
                UncompressedData[offset++] = ROM_Memory[idx+i];
            }
        }

        idx += compressedLen;

        if (ROM_Memory[34] >= 3) // 128K mode?
        {
            // Already placed in RAM by default above...
        }
        else // 48K mode
        {
                 if (pageNum == 8) memcpy(RAM_Memory+0x4000, UncompressedData, 0x4000);
            else if (pageNum == 4) memcpy(RAM_Memory+0x8000, UncompressedData, 0x4000);
            else if (pageNum == 5) memcpy(RAM_Memory+0xC000, UncompressedData, 0x4000);
        }
    }

    return ((ROM_Memory[34] >= 3) ? 1:0); // 128K Spectrum or 48K
}

// ----------------------------------------------------------------------
// Assumes .z80 file is in ROM_Memory[] - this will determine if we are
// a version 1, 2 or 3 snapshot and handle the header appropriately to
// decompress the data out into emulation memory.
// ----------------------------------------------------------------------
void speccy_decompress_snapshot(int romSize)
{
    if (speccy_mode == MODE_SNA) // SNA snapshot - only 48K compatible
    {
        memcpy(RAM_Memory + 0x4000, ROM_Memory+27, 0xC000);
        zx_128k_mode = 0;
        return;
    }

    if (speccy_mode == MODE_Z80) // Otherwise we're some kind of Z80 snapshot file
    {
        // V2 or V3 header... possibly 128K Spectrum snapshot
        if ((ROM_Memory[6] == 0x00) && (ROM_Memory[7] == 0x00))
        {
            zx_128k_mode = decompress_v2_v3(romSize);
        }
        else
        {
            // This is going to be 48K only
            zx_128k_mode = decompress_v1(romSize);
        }
        return;
    }
}

void speccy_restore_sna(void)
{
    CPU.I = ROM_Memory[0];

    CPU.HL1.B.l = ROM_Memory[1];
    CPU.HL1.B.h = ROM_Memory[2];

    CPU.DE1.B.l = ROM_Memory[3];
    CPU.DE1.B.h = ROM_Memory[4];

    CPU.BC1.B.l = ROM_Memory[5];
    CPU.BC1.B.h = ROM_Memory[6];

    CPU.AF1.B.l = ROM_Memory[7];
    CPU.AF1.B.h = ROM_Memory[8];

    CPU.HL.B.l = ROM_Memory[9];
    CPU.HL.B.h = ROM_Memory[10];

    CPU.DE.B.l = ROM_Memory[11];
    CPU.DE.B.h = ROM_Memory[12];

    CPU.BC.B.l = ROM_Memory[13];
    CPU.BC.B.h = ROM_Memory[14];

    CPU.IY.B.l = ROM_Memory[15];
    CPU.IY.B.h = ROM_Memory[16];

    CPU.IX.B.l = ROM_Memory[17];
    CPU.IX.B.h = ROM_Memory[18];

    CPU.IFF     = (ROM_Memory[19] ? (IFF_2|IFF_EI) : 0x00);
    CPU.IFF    |= ((ROM_Memory[25] & 3) == 1 ? IFF_IM1 : IFF_IM2);

    CPU.R      = ROM_Memory[20];

    CPU.AF.B.l = ROM_Memory[21];
    CPU.AF.B.h = ROM_Memory[22];

    CPU.SP.B.l = ROM_Memory[23];
    CPU.SP.B.h = ROM_Memory[24];

    // M_RET
    CPU.PC.B.l=RAM_Memory[CPU.SP.W++];
    CPU.PC.B.h=RAM_Memory[CPU.SP.W++];
}

void speccy_restore_z80(void)
{
    CPU.AF.B.h = ROM_Memory[0]; //A
    CPU.AF.B.l = ROM_Memory[1]; //F

    CPU.BC.B.l = ROM_Memory[2]; //C
    CPU.BC.B.h = ROM_Memory[3]; //B

    CPU.HL.B.l = ROM_Memory[4]; //L
    CPU.HL.B.h = ROM_Memory[5]; //H

    CPU.PC.B.l = ROM_Memory[6]; // PC low byte
    CPU.PC.B.h = ROM_Memory[7]; // PC high byte

    CPU.SP.B.l = ROM_Memory[8]; // SP low byte
    CPU.SP.B.h = ROM_Memory[9]; // SP high byte

    CPU.I      = ROM_Memory[10]; // Interrupt register
    CPU.R      = ROM_Memory[11]; // Low 7-bits of Refresh
    CPU.R_HighBit = (ROM_Memory[12] & 1 ? 0x80:0x00); // High bit of refresh

    CPU.DE.B.l  = ROM_Memory[13]; // E
    CPU.DE.B.h  = ROM_Memory[14]; // D

    CPU.BC1.B.l = ROM_Memory[15]; // BC'
    CPU.BC1.B.h = ROM_Memory[16];

    CPU.DE1.B.l = ROM_Memory[17]; // DE'
    CPU.DE1.B.h = ROM_Memory[18];

    CPU.HL1.B.l = ROM_Memory[19]; // HL'
    CPU.HL1.B.h = ROM_Memory[20];

    CPU.AF1.B.h = ROM_Memory[21]; // AF'
    CPU.AF1.B.l = ROM_Memory[22];

    CPU.IY.B.l  = ROM_Memory[23]; // IY
    CPU.IY.B.h  = ROM_Memory[24];

    CPU.IX.B.l  = ROM_Memory[25]; // IX
    CPU.IX.B.h  = ROM_Memory[26];

    CPU.IFF     = (ROM_Memory[27] ? IFF_1 : 0x00);
    CPU.IFF    |= (ROM_Memory[28] ? IFF_2 : 0x00);
    CPU.IFF    |= ((ROM_Memory[29] & 3) == 1 ? IFF_IM1 : IFF_IM2);

    // ------------------------------------------------------------------------------------
    // If the Z80 snapshot indicated we are v2 or v3 - we use the extended header
    // ------------------------------------------------------------------------------------
    if (CPU.PC.W == 0x0000)
    {
        CPU.PC.B.l = ROM_Memory[32]; // PC low byte
        CPU.PC.B.h = ROM_Memory[33]; // PC high byte
        if (zx_128k_mode)
        {
            // Now set the memory map to point to the right banks...
            MemoryMap[1] = RAM_Memory128 + (5 * 0x4000) - 0x4000; // Bank 5
            MemoryMap[2] = RAM_Memory128 + (2 * 0x4000) - 0x8000; // Bank 2

            zx_bank(ROM_Memory[35]);     // Last write to 0x7ffd (banking)

            // ---------------------------------------------------------------------------------------
            // Restore the sound chip exactly as it was... I've seen some cases (Lode Runner) where
            // the AY in Use flag in byte 37 is not set correctly so we also check to see if the
            // last AY index has been set or if any of the A,B,C volumes is non-zero to enable here.
            // ---------------------------------------------------------------------------------------
            if ((ROM_Memory[37] & 0x04) || (ROM_Memory[38] > 0) || (ROM_Memory[39+8] > 0) ||
               (ROM_Memory[39+9] > 0) || (ROM_Memory[39+10] > 0)) // Was the AY enabled?
            {
                zx_AY_enabled = 1;
                for (u8 k=0; k<16; k++)
                {
                    ay38910IndexW(k, &myAY);
                    ay38910DataW(ROM_Memory[39+k], &myAY);
                }
                ay38910IndexW(ROM_Memory[38], &myAY); // Last write to the AY index register
            }
        }
    }
}

// End of file

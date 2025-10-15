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
#include <ctype.h>
#include <dirent.h>

#include "SpeccySE.h"
#include "CRC32.h"
#include "cpu/z80/Z80_interface.h"
#include "SpeccyUtils.h"
#include "printf.h"

// -----------------------------------------------------------------------------
// I've seen a few rare POKEs that are massive - e.g. Jet Set Willy has a near
// re-write of a routine to change the jumping and Jet-Pac RX has almost 13K 
// worth of patches (to a 16K game!) ... To support these ultra large poke
// possibilities, we allocate a large pool of POKs that give the system up to 
// 16K worth of memory pokes. This should be enough for even the biggest POKs.
// -----------------------------------------------------------------------------
#define MAX_POKES                       64      // No more than this numbber of POKEs
#define MAX_POK_MEM                 (16*1024)   // Our big list of POKs can contain up to 16K worth of pokes

u16  pok_mem[MAX_POK_MEM];      // List of memory areas to poke
u8   pok_val[MAX_POK_MEM];      // List of values to poke into pok_mem[] - 256 is special (means ask user)
u8   pok_bank[MAX_POK_MEM];     // List of memory banks (for 128K Spectrum) in which to poke - usually '8' (no bank) but could be a 128K bank. High bit means ask for poke value

typedef struct
{
  char pok_name[31];            // Poke Name - cut off at 31 chars plus NULL
  u8   pok_applied;             // 1 if the poke has been applied already
  u16  poke_idx_start;          // Where is this POK start in our big list?
  u16  poke_idx_end;            // The last index to POK in our big list?
} Poke_t;

Poke_t Pokes[MAX_POKES];        // This holds all our POKEs for the current game

char szLoadFile[256];
char szLine[256];
u32  last_file_crc_poke_read;
u16  last_pok_mem_idx;

inline void WrZ80(word A, byte value)   {if (A & 0xC000) MemoryMap[(A)>>14][A] = value;}

void pok_init()
{
    memset(Pokes,    0x00, sizeof(Pokes));
    memset(pok_mem,  0x00, sizeof(pok_mem));
    memset(pok_val,  0x00, sizeof(pok_val));
    memset(pok_bank, 0x00, sizeof(pok_bank));
    last_pok_mem_idx = 0; // Index into our big 16K table of POKs
    
    last_file_crc_poke_read = 0x00000000;
}

void pok_apply(u8 sel)
{
    Pokes[sel].pok_applied = 1;
    for (u16 j=Pokes[sel].poke_idx_start; j<Pokes[sel].poke_idx_end; j++)
    {
        if (pok_mem[j] != 0)
        {
            u8 bank = pok_bank[j] & 0x7F;
            u8 value = pok_val[j];

            if (pok_bank[j] & 0x80) // Must ask user for value...
            {
                value = 0;
                DSPrint(0,22,0,"ENTER POKE VALUE (0-255): ");
                while (1)
                {
                    if (keysCurrent() & KEY_A)    break;
                    if (keysCurrent() & KEY_UP)   value++;
                    if (keysCurrent() & KEY_DOWN) value--;
                    char tmp[5];
                    sprintf(tmp,"%03d", value);
                    DSPrint(28,22,2,tmp);
                    WAITVBL;
                }

                while ((keysCurrent() & (KEY_UP | KEY_DOWN | KEY_A ))!=0); // Wait for release
                DSPrint(0,22,0,"                                ");
            }
            // If a bank is indicated (rare), we poke directly into the 128K RAM bank
            if (bank < 8)
            {
                RAM_Memory128[(0x4000 * bank) + pok_mem[j]] = value;
            }
            else
            {
                WrZ80(pok_mem[j], value);
            }
        }
    }
}

u8 num_pokes = 0;
u8 pok_readfile(void)
{
    if (last_file_crc_poke_read == file_crc) return num_pokes;

    last_file_crc_poke_read = file_crc;

    // Zero out all pokes before reading file
    memset(Pokes, 0x00, sizeof(Pokes));
    last_pok_mem_idx = 0;
    num_pokes = 0;

    // POK files must be in a ./pok subdirectory
    sprintf(szLoadFile,"pok/%s", initial_file);

    // And has the same base filename as the game loaded with '.POK' extension
    int len = strlen(szLoadFile);
    szLoadFile[len-3] = 'p';
    szLoadFile[len-2] = 'o';
    szLoadFile[len-1] = 'k';

    FILE *infile = fopen(szLoadFile, "rb");

    if (infile)
    {
        do
        {
            fgets(szLine, 255, infile);

            char *ptr = szLine;

            if (szLine[0] == 'N')
            {
                memcpy(Pokes[num_pokes].pok_name, szLine+1, 30);
                Pokes[num_pokes].poke_idx_start = last_pok_mem_idx;
                Pokes[num_pokes].poke_idx_end = last_pok_mem_idx;
            }

            if (szLine[0] == 'M' || szLine[0] == 'Z')
            {
                while (*ptr != ' ') ptr++; while (*ptr == ' ') ptr++; // Skip to next field
                pok_bank[last_pok_mem_idx] = atoi(ptr);
                while (*ptr != ' ') ptr++; while (*ptr == ' ') ptr++; // Skip to next field
                pok_mem[last_pok_mem_idx] = atoi(ptr);
                while (*ptr != ' ') ptr++; while (*ptr == ' ') ptr++; // Skip to next field
                u16 value = atoi(ptr);
                pok_val[last_pok_mem_idx] = value & 0xFF;
                if (value == 256) pok_bank[last_pok_mem_idx] |= 0x80; // Flag to indicate user-defined value
                
                if (last_pok_mem_idx < (MAX_POK_MEM-1)) last_pok_mem_idx++;
                Pokes[num_pokes].poke_idx_end = last_pok_mem_idx;
                
                if (szLine[0] == 'Z')
                {
                    if (num_pokes < (MAX_POKES-1)) num_pokes++; else break; // We only support so many POKs
                }
            }

            if (szLine[0] == 'Y') break;
        } while (!feof(infile));

        fclose(infile);
    }

    return num_pokes;
}

#define POKES_PER_SCREEN  16

// Show the various POK names to the user so they can select the right one...
void pok_select(void)
{
    char tmp[33];
    u8 sel = 0;

    while ((keysCurrent() & (KEY_UP | KEY_DOWN | KEY_A ))!=0);

    BottomScreenOptions();

    u8 max = pok_readfile();

    if (max == 0) // No POKEs found...
    {
        DSPrint(0,8,0, "    NO .POK FILE WAS FOUND      ");
        DSPrint(0,10,0,"ENSURE .POK IS NAMED THE SAME   ");
        DSPrint(0,11,0,"BASE FILENAME AS GAME AND IS    ");
        DSPrint(0,12,0,"IN A SUBDIR NAMED /POK RELATIVE ");
        DSPrint(0,13,0,"TO WHERE THE GAME FILE IS FOUND ");
        WAITVBL;WAITVBL;WAITVBL;
        while (!keysCurrent()) {WAITVBL;currentBrightness = 0; dimDampen = 0;}
    }
    else
    {
        DSPrint(0,23,0,"PRESS A TO APPLY POKE, B TO EXIT");

        u8 screen_max = (max < POKES_PER_SCREEN ? max:POKES_PER_SCREEN);
        u8 offset = 0;
        for (u8 i=0; i < screen_max; i++)
        {
            sprintf(tmp, "%-31s", Pokes[offset+i].pok_name);
            DSPrint(1,4+i,(i==sel) ? 2:0,tmp);
            if (Pokes[offset+i].pok_applied) DSPrint(0,4+i,2,"@"); else DSPrint(0,4+i,0," ");
        }

        while (1)
        {
            currentBrightness = 0; dimDampen = 0;
            u16 keys = keysCurrent();
            if (keys & KEY_A)
            {
                while ((keysCurrent() & (KEY_UP | KEY_DOWN | KEY_A ))!=0); // Wait for release
                DSPrint(0,4+sel,2,"@");
                DSPrint(0,21,0,"      APPLYING MEMORY POKE      ");
                pok_apply(offset+sel);
                WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
                DSPrint(0,21,0,"                                ");
            }
            
            if (keys & KEY_B) {break;}

            if (keys & KEY_DOWN)
            {
                if (sel < (screen_max-1))
                {
                    sprintf(tmp, "%-31s", Pokes[offset+sel].pok_name);
                    DSPrint(1,4+sel,0,tmp);
                    sel++;
                    sprintf(tmp, "%-31s", Pokes[offset+sel].pok_name);
                    DSPrint(1,4+sel,2,tmp);
                    WAITVBL;WAITVBL;
                }
                else
                {
                    if ((offset + screen_max) < max)
                    {
                        offset += POKES_PER_SCREEN;
                        screen_max = ((max-offset) < POKES_PER_SCREEN ? (max-offset):POKES_PER_SCREEN);
                        sel = 0;
                        for (u8 i=0; i < POKES_PER_SCREEN; i++)
                        {
                            if (i < screen_max)
                            {
                                sprintf(tmp, "%-31s", Pokes[offset+i].pok_name);
                                DSPrint(1,4+i,(i==sel) ? 2:0,tmp);
                                if (Pokes[offset+i].pok_applied) DSPrint(0,4+i,2,"@"); else DSPrint(0,4+i,0," ");
                            }
                            else
                            {
                                DSPrint(1,4+i,0,"                                ");
                                DSPrint(0,4+i,0," ");
                            }
                        }
                        WAITVBL;WAITVBL;WAITVBL;
                    }
                }
            }
            if (keys & KEY_UP)
            {
                if (sel > 0)
                {
                    sprintf(tmp, "%-31s", Pokes[offset+sel].pok_name);
                    DSPrint(1,4+sel,0,tmp);
                    sel--;
                    sprintf(tmp, "%-31s", Pokes[offset+sel].pok_name);
                    DSPrint(1,4+sel,2,tmp);
                    WAITVBL;WAITVBL;
                }
                else
                {
                    if (offset > 0)
                    {
                        offset -= POKES_PER_SCREEN;
                        screen_max = ((max-offset) < POKES_PER_SCREEN ? (max-offset):POKES_PER_SCREEN);
                        sel = POKES_PER_SCREEN-1;
                        for (u8 i=0; i < POKES_PER_SCREEN; i++)
                        {
                            if (i < screen_max)
                            {
                                sprintf(tmp, "%-31s", Pokes[offset+i].pok_name);
                                DSPrint(1,4+i,(i==sel) ? 2:0,tmp);
                                if (Pokes[offset+i].pok_applied) DSPrint(0,4+i,2,"@"); else DSPrint(0,4+i,0," ");
                            }
                            else
                            {
                                DSPrint(0,4+i,0,"                                ");
                                DSPrint(0,4+i,0," ");
                            }
                        }
                        WAITVBL;WAITVBL;WAITVBL;
                    }
                }
            }
        }
    }

    while (keysCurrent())
    {
        WAITVBL;WAITVBL;
    }

    return;
}

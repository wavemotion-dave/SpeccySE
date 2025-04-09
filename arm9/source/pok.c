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

// ----------------------------------------------------------------------------------
// I've seen a few rare POKEs that are massive - e.g. Jet Set Willy has a near 
// re-write of a routine to change the jumping ... We don't support those large 
// POKEs here. Too much wasted memory and for now, we're keeping this very simple. 
// This should handle about 99% of all POKEs out there. Most games use it to
// produce extra lives, invulnerability or weapon upgrades.
// ----------------------------------------------------------------------------------
#define MAX_POKES                       64  // No more than this numbber of POKEs
#define MAX_POK_MEM                     32  // And each POKE can have up to this many memory mods

typedef struct
{
  char pok_name[32];            // Poke Name - cut off at 31 chars plus NULL
  u16  pok_mem[MAX_POK_MEM];    // List of memory areas to poke
  u16  pok_val[MAX_POK_MEM];    // List of values to poke into pok_mem[] - 256 is special (means ask user)
  u8   pok_bank[MAX_POK_MEM];   // List of values to poke into pok_mem[] - usually '8' but could be a 128K bank
} Poke_t;

Poke_t Pokes[MAX_POKES];  // This holds all our POKEs for the current game

inline void WrZ80(word A, byte value)   {if (A & 0xC000) *(MemoryMap[(A)>>13] + ((A)&0x1FFF))=value;}

void pok_apply(u8 sel)
{
    for (u8 j=0; j<MAX_POK_MEM; j++)
    {
        if (Pokes[sel].pok_mem[j] != 0)
        {
            u8 bank = Pokes[sel].pok_bank[j];
            u16 value = Pokes[sel].pok_val[j];
            
            if (value == 256) // Must ask user for value...
            {
                value = 0;
                DSPrint(0,22,0,"ENTER POKE VALUE (0-255): ");
                while (1)
                {
                    if (keysCurrent() & KEY_A)    break;
                    if (keysCurrent() & KEY_UP)   value=(value+1) & 0xFF;
                    if (keysCurrent() & KEY_DOWN) value=(value-1) & 0xFF;
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
                RAM_Memory128[(0x4000 * bank) + Pokes[sel].pok_mem[j]] = (u8)value;
            }
            else
            {
                WrZ80(Pokes[sel].pok_mem[j], (u8)value);
            }
        }
    }
}

char szLoadFile[256];
char szLine[256];
u8 pok_readfile(void)
{
    // Zero out all pokes before reading file
    memset(Pokes, 0x00, sizeof(Pokes));
    
    // POK files must be in a ./pok subdirectory
    sprintf(szLoadFile,"pok/%s", initial_file);

    // And has the same base filename as the game loaded with '.POK' extension
    int len = strlen(szLoadFile);
    szLoadFile[len-3] = 'p';
    szLoadFile[len-2] = 'o';
    szLoadFile[len-1] = 'k';

    FILE *infile = fopen(szLoadFile, "rb");

    u8 num_pokes = 0;
    if (infile)
    {
        u8 mem_idx = 0;
        do
        {
            fgets(szLine, 255, infile);

            char *ptr = szLine;
            
            if (szLine[0] == 'N')
            {
                memcpy(Pokes[num_pokes].pok_name, szLine+1, 30);
                mem_idx=0;
            }

            if (szLine[0] == 'M' || szLine[0] == 'Z')
            {
                while (*ptr != ' ') ptr++; while (*ptr == ' ') ptr++; // Skip to next field
                Pokes[num_pokes].pok_bank[mem_idx] = atoi(ptr);
                while (*ptr != ' ') ptr++; while (*ptr == ' ') ptr++; // Skip to next field
                Pokes[num_pokes].pok_mem[mem_idx] = atoi(ptr);
                while (*ptr != ' ') ptr++; while (*ptr == ' ') ptr++; // Skip to next field
                Pokes[num_pokes].pok_val[mem_idx] = atoi(ptr);                
                if (mem_idx < (MAX_POK_MEM-1)) mem_idx++;
                if (szLine[0] == 'Z')
                { 
                    if (mem_idx < MAX_POK_MEM) num_pokes++;
                    else memset(Pokes[num_pokes].pok_mem, 0x00, sizeof(Pokes[num_pokes].pok_mem));
                    if (num_pokes >= MAX_POKES) break;
                }
            }
            
            if (szLine[0] == 'Y') break;
        } while (!feof(infile));

        fclose(infile);
    }
    
    return num_pokes;
}

#define POKES_PER_SCREEN  16

// Show tape blocks with filenames/descriptions... 
void pok_select(void)
{
    char tmp[33];
    u8 sel = 0;

    while ((keysCurrent() & (KEY_UP | KEY_DOWN | KEY_A ))!=0);

    BottomScreenOptions();

    DSPrint(0,23,0,"PRESS A TO APPLY POKE, B TO EXIT");
    
    u8 max = pok_readfile();
    u8 screen_max = (max < POKES_PER_SCREEN ? max:POKES_PER_SCREEN);
    u8 offset = 0;
    for (u8 i=0; i < screen_max; i++)
    {
        sprintf(tmp, "%-31s", Pokes[offset+i].pok_name);
        DSPrint(1,4+i,(i==sel) ? 2:0,tmp);
    }
    
    while (1)
    {
        u16 keys = keysCurrent();
        if (keys & KEY_A)
        {
            while ((keysCurrent() & (KEY_UP | KEY_DOWN | KEY_A ))!=0); // Wait for release
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
                        }
                        else
                        {
                            DSPrint(1,4+i,0,"                                ");
                        }
                    }
                    WAITVBL;WAITVBL;WAITVBL;WAITVBL;
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
                    sel = 0;
                    for (u8 i=0; i < POKES_PER_SCREEN; i++)
                    {
                        if (i < screen_max)
                        {
                            sprintf(tmp, "%-31s", Pokes[offset+i].pok_name);
                            DSPrint(1,4+i,(i==sel) ? 2:0,tmp);
                        }
                        else
                        {
                            DSPrint(0,4+i,0,"                                ");
                        }
                    }
                    WAITVBL;WAITVBL;WAITVBL;WAITVBL;
                }
            }
        }
    }
    
    while ((keysCurrent() & (KEY_UP | KEY_DOWN | KEY_A ))!=0);
    WAITVBL;WAITVBL;
    
    return;
}

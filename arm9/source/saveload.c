// =====================================================================================
// Copyright (c) 2025 Dave Bernazzani (wavemotion-dave)
//
// Copying and distribution of this emulator, its source code and associated
// readme files, with or without modification, are permitted in any medium without
// royalty provided this copyright notice is used and wavemotion-dave (Phoenix-Edition),
// Alekmaul (original port) and Marat Fayzullin (ColEM core) are thanked profusely.
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

#include "lzav.h"

#define SPECCY_SAVE_VER   0x0009    // Change this if the basic format of the .SAV file changes. Invalidates older .sav files.

// -----------------------------------------------------------------------------------------------------
// Since the main MemoryMap[] can point to differt things (RAM, ROM, BIOS, etc) and since we can't rely
// on the memory being in the same spot on subsequent versions of the emulator... we need to save off
// the type and the offset so that we can patch it back together when we load back a saved state.
// -----------------------------------------------------------------------------------------------------
struct RomOffset
{
    u8   type;
    u32  offset;
};

struct RomOffset Offsets[4];

#define TYPE_ROM        0
#define TYPE_RAM        1
#define TYPE_RAM128     2
#define TYPE_BIOS       3
#define TYPE_BIOS128    4
#define TYPE_OTHER      5

/*********************************************************************************
 * Save the current state - save everything we need to a single .sav file.
 ********************************************************************************/
static char szLoadFile[256];        // We build the filename out of the base filename and tack on .sav, .ee, etc.
static char tmpStr[32];

u8 CompressBuffer[200*1024];        // Big enough to handle compression of even full 128K games - we also steal this memory for screen snapshot use

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"

u8 spare[300];

void spectrumSaveState()
{
    size_t retVal;

    memset(spare, 0x00, sizeof(spare));
    
    // Return to the original path
    chdir(initial_path);

    // Init filename = romname and SAV in place of ROM
    DIR* dir = opendir("sav");
    if (dir) closedir(dir);    // Directory exists... close it out and move on.
    else mkdir("sav", 0777);   // Otherwise create the directory...
    sprintf(szLoadFile,"sav/%s", initial_file);

    int len = strlen(szLoadFile);
    szLoadFile[len-3] = 's';
    szLoadFile[len-2] = 'a';
    szLoadFile[len-1] = 'v';

    strcpy(tmpStr,"SAVING...");
    DSPrint(4,0,0,tmpStr);

    FILE *handle = fopen(szLoadFile, "wb+");
    if (handle != NULL)
    {
        // Write Version
        u16 save_ver = SPECCY_SAVE_VER;
        retVal = fwrite(&save_ver, sizeof(u16), 1, handle);

        // Write Last Directory Path / Tape File
        retVal = fwrite(&last_path, sizeof(last_path), 1, handle);
        retVal = fwrite(&last_file, sizeof(last_file), 1, handle);

        // Write CZ80 CPU
        retVal = fwrite(&CPU, sizeof(CPU), 1, handle);

        // Write AY Chip info
        retVal = fwrite(&myAY, sizeof(myAY), 1, handle);

        // And the Memory Map - we must only save offsets so that this is generic when we change code and memory shifts...
        for (u8 i=0; i<4; i++)
        {
            // -------------------------------------------------------------------------------
            // This is the base address where the Memory Map is pointing to... (the maps are
            // all offset by chunks of 16K to provide faster reading/writing in Z80.c)
            // -------------------------------------------------------------------------------
            u8 *ptr = MemoryMap[i] + (i*0x4000);
            
            if ((ptr >= SpectrumBios) && (ptr < SpectrumBios+sizeof(SpectrumBios)))
            {
                Offsets[i].type = TYPE_BIOS;
                Offsets[i].offset = ptr - SpectrumBios;
            }
            else if ((ptr >= SpectrumBios128) && (ptr < SpectrumBios+sizeof(SpectrumBios128)))
            {
                Offsets[i].type = TYPE_BIOS128;
                Offsets[i].offset = ptr - SpectrumBios128;
            }
            else if ((ptr >= RAM_Memory) && (ptr < RAM_Memory+sizeof(RAM_Memory)))
            {
                Offsets[i].type = TYPE_RAM;
                Offsets[i].offset = ptr - RAM_Memory;
            }
            else if ((ptr >= RAM_Memory128) && (ptr < RAM_Memory128+sizeof(RAM_Memory128)))
            {
                Offsets[i].type = TYPE_RAM128;
                Offsets[i].offset = ptr - RAM_Memory128;
            }
            else if ((ptr >= ROM_Memory) && (ptr < ROM_Memory+sizeof(ROM_Memory)))
            {
                Offsets[i].type = TYPE_ROM;
                Offsets[i].offset = ptr - ROM_Memory;
            }
            else
            {
                Offsets[i].type = TYPE_OTHER;
                Offsets[i].offset =  (u32)MemoryMap[i];
            }
        }
        if (retVal) retVal = fwrite(Offsets, sizeof(Offsets),1, handle);

        // And now a bunch of ZX Spectrum related vars...
        if (retVal) retVal = fwrite(&portFE,                    sizeof(portFE),                     1, handle);
        if (retVal) retVal = fwrite(&portFD,                    sizeof(portFD),                     1, handle);
        if (retVal) retVal = fwrite(&zx_AY_enabled,             sizeof(zx_AY_enabled),              1, handle);
        if (retVal) retVal = fwrite(&flash_timer,               sizeof(flash_timer),                1, handle);
        if (retVal) retVal = fwrite(&bFlash,                    sizeof(bFlash),                     1, handle);
        if (retVal) retVal = fwrite(&zx_128k_mode,              sizeof(zx_128k_mode),               1, handle);
        if (retVal) retVal = fwrite(&zx_ScreenRendering,        sizeof(zx_ScreenRendering),         1, handle);
        if (retVal) retVal = fwrite(&zx_current_line,           sizeof(zx_current_line),            1, handle);
        if (retVal) retVal = fwrite(&emuActFrames,              sizeof(emuActFrames),               1, handle);
        if (retVal) retVal = fwrite(&timingFrames,              sizeof(timingFrames),               1, handle);
        if (retVal) retVal = fwrite(&num_blocks_available,      sizeof(num_blocks_available),       1, handle);
        if (retVal) retVal = fwrite(&current_block,             sizeof(current_block),              1, handle);
        if (retVal) retVal = fwrite(&tape_state,                sizeof(tape_state),                 1, handle);
        if (retVal) retVal = fwrite(&current_block_data_idx,    sizeof(current_block_data_idx),     1, handle);
        if (retVal) retVal = fwrite(&tape_bytes_processed,      sizeof(tape_bytes_processed),       1, handle);
        if (retVal) retVal = fwrite(&header_pulses,             sizeof(header_pulses),              1, handle);
        if (retVal) retVal = fwrite(&current_bit,               sizeof(current_bit),                1, handle);
        if (retVal) retVal = fwrite(&current_bytes_this_block,  sizeof(current_bytes_this_block),   1, handle);
        if (retVal) retVal = fwrite(&handle_last_bits,          sizeof(handle_last_bits),           1, handle);
        if (retVal) retVal = fwrite(&custom_pulse_idx,          sizeof(custom_pulse_idx),           1, handle);
        if (retVal) retVal = fwrite(&bFirstTime,                sizeof(bFirstTime),                 1, handle);
        if (retVal) retVal = fwrite(&loop_counter,              sizeof(loop_counter),               1, handle);
        if (retVal) retVal = fwrite(&loop_block,                sizeof(loop_block),                 1, handle);
        if (retVal) retVal = fwrite(&last_edge,                 sizeof(last_edge),                  1, handle);
        if (retVal) retVal = fwrite(&give_up_counter,           sizeof(give_up_counter),            1, handle);
        if (retVal) retVal = fwrite(&next_edge1,                sizeof(next_edge1),                 1, handle);
        if (retVal) retVal = fwrite(&next_edge2,                sizeof(next_edge2),                 1, handle);
        if (retVal) retVal = fwrite(&tape_play_skip_frame,      sizeof(tape_play_skip_frame),       1, handle);
        if (retVal) retVal = fwrite(&rom_special_bank,          sizeof(rom_special_bank),           1, handle);
        if (retVal) retVal = fwrite(&dandanator_cmd,            sizeof(dandanator_cmd),             1, handle);
        if (retVal) retVal = fwrite(&dandanator_data1,          sizeof(dandanator_data1),           1, handle);
        if (retVal) retVal = fwrite(&dandanator_data2,          sizeof(dandanator_data2),           1, handle);
        if (retVal) retVal = fwrite(&zx_ula_plus_enabled,       sizeof(zx_ula_plus_enabled),        1, handle);
        if (retVal) retVal = fwrite(&zx_ula_plus_group,         sizeof(zx_ula_plus_group),          1, handle);
        if (retVal) retVal = fwrite(&zx_ula_plus_palette_reg,   sizeof(zx_ula_plus_palette_reg),    1, handle);
        if (retVal) retVal = fwrite(&zx_ula_plus_palette,       sizeof(zx_ula_plus_palette),        1, handle);
        if (retVal) retVal = fwrite(spare,                      300,                                1, handle);

        // Save Z80 Memory Map... either 48K or 128K
        u8 *ptr = (zx_128k_mode ? RAM_Memory128 : (RAM_Memory+0x4000));
        u32 mem_size = (zx_128k_mode ? 0x20000 : 0xC000);

        // -------------------------------------------------------------------
        // Compress the RAM data using 'high' compression ratio... it's
        // still quite fast for such small memory buffers and often shrinks
        // 48K games down under 32K and 128K games down closer to 64K.
        // -------------------------------------------------------------------
        int max_len = lzav_compress_bound_hi( mem_size );
        int comp_len = lzav_compress_hi( ptr, CompressBuffer, mem_size, max_len );

        if (retVal) retVal = fwrite(&comp_len,          sizeof(comp_len), 1, handle);
        if (retVal) retVal = fwrite(&CompressBuffer,    comp_len,         1, handle);

        strcpy(tmpStr, (retVal ? "OK ":"ERR"));
        DSPrint(13,0,0,tmpStr);
        WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
        DSPrint(4,0,0,"             ");
        DisplayStatusLine(true);
    }
    else
    {
        strcpy(tmpStr,"Error opening SAV file ...");
    }
    fclose(handle);
}


/*********************************************************************************
 * Load the current state - read everything back from the .sav file.
 ********************************************************************************/
void spectrumLoadState()
{
    size_t retVal;

    // Return to the original path
    chdir(initial_path);

    // Init filename = romname and .SAV in place of ROM
    sprintf(szLoadFile,"sav/%s", initial_file);
    int len = strlen(szLoadFile);

    szLoadFile[len-3] = 's';
    szLoadFile[len-2] = 'a';
    szLoadFile[len-1] = 'v';

    FILE* handle = fopen(szLoadFile, "rb");
    if (handle != NULL)
    {
         strcpy(tmpStr,"LOADING...");
         DSPrint(4,0,0,tmpStr);

        // Read Version
        u16 save_ver = 0xBEEF;
        retVal = fread(&save_ver, sizeof(u16), 1, handle);

        if (save_ver == SPECCY_SAVE_VER)
        {
            // Read Last Directory Path / Tape File
            if (retVal) retVal = fread(&last_path, sizeof(last_path), 1, handle);
            if (retVal) retVal = fread(&last_file, sizeof(last_file), 1, handle);

            // ----------------------------------------------------------------
            // If the last known file was a tap file (.tap or .tzx) we want to
            // reload that as the user might have swapped tapes to side 2, etc.
            // ----------------------------------------------------------------
            if ( (strcasecmp(strrchr(last_file, '.'), ".tap") == 0) || (strcasecmp(strrchr(last_file, '.'), ".tzx") == 0) )
            {
                chdir(last_path);
                CassetteInsert(last_file);
            }

            // Load CZ80 CPU
            if (retVal) retVal = fread(&CPU, sizeof(CPU), 1, handle);

            // Load AY Chip info
            if (retVal) retVal = fread(&myAY, sizeof(myAY), 1, handle);

            // Load back the Memory Map - these were saved as offsets so we must reconstruct actual pointers
            if (retVal) retVal = fread(Offsets, sizeof(Offsets),1, handle);
            for (u8 i=0; i<4; i++)
            {
                if (Offsets[i].type == TYPE_BIOS)
                {
                    MemoryMap[i] = (u8 *) (SpectrumBios + Offsets[i].offset - (i*0x4000));
                }
                else if (Offsets[i].type == TYPE_BIOS128)
                {
                    MemoryMap[i] = (u8 *) (SpectrumBios128 + Offsets[i].offset - (i*0x4000));
                }
                else if (Offsets[i].type == TYPE_RAM)
                {
                    MemoryMap[i] = (u8 *) (RAM_Memory + Offsets[i].offset - (i*0x4000));
                }
                else if (Offsets[i].type == TYPE_RAM128)
                {
                    MemoryMap[i] = (u8 *) (RAM_Memory128 + Offsets[i].offset - (i*0x4000));
                }
                else if (Offsets[i].type == TYPE_ROM)
                {
                    MemoryMap[i] = (u8 *) (ROM_Memory + Offsets[i].offset - (i*0x4000));
                }
                else // TYPE_OTHER - this is just a pointer to memory
                {
                    MemoryMap[i] = (u8 *) (Offsets[i].offset);
                }
            }
        }
        else retVal = 0;

        // And now a bunch of ZX Spectrum related vars...
        if (retVal) retVal = fread(&portFE,                    sizeof(portFE),                     1, handle);
        if (retVal) retVal = fread(&portFD,                    sizeof(portFD),                     1, handle);
        if (retVal) retVal = fread(&zx_AY_enabled,             sizeof(zx_AY_enabled),              1, handle);
        if (retVal) retVal = fread(&flash_timer,               sizeof(flash_timer),                1, handle);
        if (retVal) retVal = fread(&bFlash,                    sizeof(bFlash),                     1, handle);
        if (retVal) retVal = fread(&zx_128k_mode,              sizeof(zx_128k_mode),               1, handle);
        if (retVal) retVal = fread(&zx_ScreenRendering,        sizeof(zx_ScreenRendering),         1, handle);
        if (retVal) retVal = fread(&zx_current_line,           sizeof(zx_current_line),            1, handle);
        if (retVal) retVal = fread(&emuActFrames,              sizeof(emuActFrames),               1, handle);
        if (retVal) retVal = fread(&timingFrames,              sizeof(timingFrames),               1, handle);
        if (retVal) retVal = fread(&num_blocks_available,      sizeof(num_blocks_available),       1, handle);
        if (retVal) retVal = fread(&current_block,             sizeof(current_block),              1, handle);
        if (retVal) retVal = fread(&tape_state,                sizeof(tape_state),                 1, handle);
        if (retVal) retVal = fread(&current_block_data_idx,    sizeof(current_block_data_idx),     1, handle);
        if (retVal) retVal = fread(&tape_bytes_processed,      sizeof(tape_bytes_processed),       1, handle);
        if (retVal) retVal = fread(&header_pulses,             sizeof(header_pulses),              1, handle);
        if (retVal) retVal = fread(&current_bit,               sizeof(current_bit),                1, handle);
        if (retVal) retVal = fread(&current_bytes_this_block,  sizeof(current_bytes_this_block),   1, handle);
        if (retVal) retVal = fread(&handle_last_bits,          sizeof(handle_last_bits),           1, handle);
        if (retVal) retVal = fread(&custom_pulse_idx,          sizeof(custom_pulse_idx),           1, handle);
        if (retVal) retVal = fread(&bFirstTime,                sizeof(bFirstTime),                 1, handle);
        if (retVal) retVal = fread(&loop_counter,              sizeof(loop_counter),               1, handle);
        if (retVal) retVal = fread(&loop_block,                sizeof(loop_block),                 1, handle);
        if (retVal) retVal = fread(&last_edge,                 sizeof(last_edge),                  1, handle);
        if (retVal) retVal = fread(&give_up_counter,           sizeof(give_up_counter),            1, handle);
        if (retVal) retVal = fread(&next_edge1,                sizeof(next_edge1),                 1, handle);
        if (retVal) retVal = fread(&next_edge2,                sizeof(next_edge2),                 1, handle);
        if (retVal) retVal = fread(&tape_play_skip_frame,      sizeof(tape_play_skip_frame),       1, handle);
        if (retVal) retVal = fread(&rom_special_bank,          sizeof(rom_special_bank),           1, handle);
        if (retVal) retVal = fread(&dandanator_cmd,            sizeof(dandanator_cmd),             1, handle);
        if (retVal) retVal = fread(&dandanator_data1,          sizeof(dandanator_data1),           1, handle);
        if (retVal) retVal = fread(&dandanator_data2,          sizeof(dandanator_data2),           1, handle);
        if (retVal) retVal = fread(&zx_ula_plus_enabled,       sizeof(zx_ula_plus_enabled),        1, handle);
        if (retVal) retVal = fread(&zx_ula_plus_group,         sizeof(zx_ula_plus_group),          1, handle);
        if (retVal) retVal = fread(&zx_ula_plus_palette_reg,   sizeof(zx_ula_plus_palette_reg),    1, handle);
        if (retVal) retVal = fread(&zx_ula_plus_palette,       sizeof(zx_ula_plus_palette),        1, handle);
        if (retVal) retVal = fread(spare,                      300,                                1, handle);

        if (zx_ula_plus_enabled)
        {
            apply_ula_plus_palette();
        }

        // Load Z80 Memory Map... either 48K or 128K
        int comp_len = 0;
        if (retVal) retVal = fread(&comp_len,          sizeof(comp_len), 1, handle);
        if (retVal) retVal = fread(&CompressBuffer,    comp_len,         1, handle);

        if (retVal)
        {
            u8 *dest_memory = (zx_128k_mode ? RAM_Memory128 : (RAM_Memory+0x4000));
            u32 mem_size = (zx_128k_mode ? 0x20000 : 0xC000);

            // ------------------------------------------------------------------
            // Decompress the previously compressed RAM and put it back into the
            // right memory location... this is quite fast all things considered.
            // ------------------------------------------------------------------
            (void)lzav_decompress( CompressBuffer, dest_memory, comp_len, mem_size );
        }

        strcpy(tmpStr, (retVal ? "OK ":"ERR"));
        DSPrint(13,0,0,tmpStr);

        WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
        DSPrint(4,0,0,"             ");
        DisplayStatusLine(true);
      }
      else
      {
        DSPrint(4,0,0,"NO SAVED GAME");
        WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
        DSPrint(4,0,0,"             ");
      }

    fclose(handle);
}

#pragma GCC diagnostic pop

// End of file

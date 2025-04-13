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

#define SPECCY_SAVE_VER   0x0005       // Change this if the basic format of the .SAV file changes. Invalidates older .sav files.

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
u8  spare[512] = {0x00};            // We keep some spare bytes so we can use them in the future without changing the structure

static char szLoadFile[256];        // We build the filename out of the base filename and tack on .sav, .ee, etc.
static char tmpStr[32];

static u8 CompressBuffer[150*1024];
void spectrumSaveState()
{
  u32 spare = 0;
  size_t retVal;

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
        if ((MemoryMap[i] >= SpectrumBios) && (MemoryMap[i] < SpectrumBios+(sizeof(SpectrumBios))))
        {
            Offsets[i].type = TYPE_BIOS;
            Offsets[i].offset = MemoryMap[i] - SpectrumBios;
        }
        else if ((MemoryMap[i] >= SpectrumBios128) && (MemoryMap[i] < SpectrumBios128+(sizeof(SpectrumBios128))))
        {
            Offsets[i].type = TYPE_BIOS128;
            Offsets[i].offset = MemoryMap[i] - SpectrumBios;
        }
        else if ((MemoryMap[i] >= RAM_Memory) && (MemoryMap[i] < RAM_Memory+(sizeof(RAM_Memory))))
        {
            Offsets[i].type = TYPE_RAM;
            Offsets[i].offset = MemoryMap[i] - RAM_Memory;
        }
        else if ((MemoryMap[i] >= RAM_Memory128) && (MemoryMap[i] < RAM_Memory128+(sizeof(RAM_Memory128))))
        {
            Offsets[i].type = TYPE_RAM128;
            Offsets[i].offset = MemoryMap[i] - RAM_Memory128;
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
    if (retVal) retVal = fwrite(&spare,                     sizeof(spare),                      1, handle);
    if (retVal) retVal = fwrite(&spare,                     sizeof(spare),                      1, handle);
    if (retVal) retVal = fwrite(&spare,                     sizeof(spare),                      1, handle);
    if (retVal) retVal = fwrite(&spare,                     sizeof(spare),                      1, handle);


    // Save Z80 Memory Map... either 48K for 128K
    u8 *ptr = RAM_Memory+0x4000;
    u32 mem_size = 0xC000;
    if (zx_128k_mode)
    {
        ptr = RAM_Memory128;
        mem_size = 0x20000;
    }
    
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
  else {
    strcpy(tmpStr,"Error opening SAV file ...");
  }
  fclose(handle);
}


/*********************************************************************************
 * Load the current state - read everything back from the .sav file.
 ********************************************************************************/
void spectrumLoadState()
{
    u32 spare = 0;
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
                    MemoryMap[i] = (u8 *) (SpectrumBios + Offsets[i].offset);
                }
                else if (Offsets[i].type == TYPE_BIOS128)
                {
                    MemoryMap[i] = (u8 *) (SpectrumBios128 + Offsets[i].offset);
                }
                else if (Offsets[i].type == TYPE_RAM)
                {
                    MemoryMap[i] = (u8 *) (RAM_Memory + Offsets[i].offset);
                }
                else if (Offsets[i].type == TYPE_RAM128)
                {
                    MemoryMap[i] = (u8 *) (RAM_Memory128 + Offsets[i].offset);
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
        if (retVal) retVal = fread(&spare,                     sizeof(spare),                      1, handle);
        if (retVal) retVal = fread(&spare,                     sizeof(spare),                      1, handle);
        if (retVal) retVal = fread(&spare,                     sizeof(spare),                      1, handle);
        if (retVal) retVal = fread(&spare,                     sizeof(spare),                      1, handle);

        // Load Z80 Memory Map... either 48K for 128K
        int comp_len = 0;
        if (retVal) retVal = fread(&comp_len,          sizeof(comp_len), 1, handle);
        if (retVal) retVal = fread(&CompressBuffer,    comp_len,         1, handle);

        u8 *dest_memory = RAM_Memory+0x4000;
        u32 mem_size = 0xC000;
        if (zx_128k_mode)
        {
            dest_memory = RAM_Memory128;
            mem_size = 0x20000;
        }

        // ------------------------------------------------------------------
        // Decompress the previously compressed RAM and put it back into the
        // right memory location... this is quite fast all things considered.
        // ------------------------------------------------------------------
        (void)lzav_decompress( CompressBuffer, dest_memory, comp_len, mem_size );        

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

// End of file

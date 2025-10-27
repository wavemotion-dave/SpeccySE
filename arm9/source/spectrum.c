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

u8  portFE                  __attribute__((section(".dtcm"))) = 0x00;
u8  portFD                  __attribute__((section(".dtcm"))) = 0x00;
u8  zx_AY_enabled           __attribute__((section(".dtcm"))) = 0;
u8  zx_AY_index_written     __attribute__((section(".dtcm"))) = 0;
u32 flash_timer             __attribute__((section(".dtcm"))) = 0;
u8  bFlash                  __attribute__((section(".dtcm"))) = 0;
u8  zx_128k_mode            __attribute__((section(".dtcm"))) = 0;
u8  zx_ScreenRendering      __attribute__((section(".dtcm"))) = 0;
u8  zx_force_128k_mode      __attribute__((section(".dtcm"))) = 0;
u32 zx_current_line         __attribute__((section(".dtcm"))) = 0;
u8  zx_special_key          __attribute__((section(".dtcm"))) = 0;
u32 last_file_size          __attribute__((section(".dtcm"))) = 0;
u8  isCompressed            __attribute__((section(".dtcm"))) = 1;
u8  tape_play_skip_frame    __attribute__((section(".dtcm"))) = 0;
u8  rom_special_bank        __attribute__((section(".dtcm"))) = 0;
u8  zx_ula_plus_enabled     __attribute__((section(".dtcm"))) = 0;
u8  accurate_emulation      __attribute__((section(".dtcm"))) = 0;
u8  last_line_drawn         __attribute__((section(".dtcm"))) = 0;

u32  pre_render_lookup[16][16][16];

u8  zx_ula_plus_palette[64] = {0};
u8  zx_ula_plus_group = 0x00;
u8  zx_ula_plus_palette_reg = 0x00;

u8  backgroundRenderScreen = 0;
u8  bRenderSkipOnce        = 1;

extern u8 dandy_disabled;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"

ITCM_CODE unsigned char cpu_readport_speccy(register unsigned short Port)
{
    static u8 bNonSpecialKeyWasPressed = 0;
    
    if ((Port & 1) == 0) // Any Even Address will cause the ULA to respond
    {
         // ----------------------------------------------------------------------------------------
         // When we are in the accurate emulation mode, we must deal with ULA memory contention.
         // ----------------------------------------------------------------------------------------
         if (accurate_emulation)
         {
             if (myConfig.machine) // 128K
             {
                 if (ContendMap[Port>>14])
                 {
                     CPU.TStates += cpu_contended_delay_128[(((CPU.TStates))+0) % CYCLES_PER_SCANLINE_128];
                     CPU.TStates += cpu_contended_delay_128[(((CPU.TStates))+1) % CYCLES_PER_SCANLINE_128];
                 }
                 else
                 {
                     CPU.TStates += cpu_contended_delay_128[(((CPU.TStates))+1) % CYCLES_PER_SCANLINE_128];
                 }
             }
             else // 48K
             {
                 if (ContendMap[Port>>14])
                 {
                     CPU.TStates += cpu_contended_delay_48[(((CPU.TStates))+0) % CYCLES_PER_SCANLINE_48];
                     CPU.TStates += cpu_contended_delay_48[(((CPU.TStates))+1) % CYCLES_PER_SCANLINE_48];
                 }
                 else
                 {
                     CPU.TStates += cpu_contended_delay_48[(((CPU.TStates))+1) % CYCLES_PER_SCANLINE_48];
                 }
             }
         }
         
        // --------------------------------------------------------
        // If we are not playing the tape but we got a hit on the
        // loader we can start the tape in motion - auto play...
        // --------------------------------------------------------
        if (!tape_state)
        {
            if (myConfig.autoLoad)
            {
                if (PatchLookup[CPU.PC.W]) tape_play();
            }
        }

        // --------------------------------------------------------
        // If we are playing the tape, get the result back as fast
        // as possible without keys/joystick press.
        // --------------------------------------------------------
        if (tape_state)
        {
            // ----------------------------------------------------------------
            // See if this read is patched... for faster tape edge detection.
            // ----------------------------------------------------------------
            if (PatchLookup[CPU.PC.W])
            {
                return PatchLookup[CPU.PC.W]();
            }
            zx_special_key = 0; 
            return ~tape_pulse();
        }
        
        if (accurate_emulation)  CPU.TStates += 4;

        // -----------------------------
        // Otherwise normal handling...
        // -----------------------------
        u8 key = (portFE & 0x18) ? 0x00 : 0x40;

        for (u8 i=0; i< kbd_keys_pressed; i++) // We may have more than one key pressed...
        {
            kbd_key = kbd_keys[i];
            word inv = ~Port;
            if (inv & 0x0800) // 12345 row
            {
                if (kbd_key)
                {
                    if (kbd_key == '1')           key  |= 0x01;
                    if (kbd_key == '2')           key  |= 0x02;
                    if (kbd_key == '3')           key  |= 0x04;
                    if (kbd_key == '4')           key  |= 0x08;
                    if (kbd_key == '5')           key  |= 0x10;
                }
            }

            if (inv & 0x1000) // 09876 row
            {
                if (kbd_key)
                {
                    if (kbd_key == '0')           key  |= 0x01;
                    if (kbd_key == '9')           key  |= 0x02;
                    if (kbd_key == '8')           key  |= 0x04;
                    if (kbd_key == '7')           key  |= 0x08;
                    if (kbd_key == '6')           key  |= 0x10;
                }
            }

            if (inv & 0x4000) // ENTER LKJH row
            {
                if (kbd_key)
                {
                    if (kbd_key == KBD_KEY_RET)   key  |= 0x01;
                    if (kbd_key == 'L')           key  |= 0x02;
                    if (kbd_key == 'K')           key  |= 0x04;
                    if (kbd_key == 'J')           key  |= 0x08;
                    if (kbd_key == 'H')           key  |= 0x10;
                }
            }

            if (inv & 0x8000) // SPACE SYMBOL MNB
            {
                if (zx_special_key == 2)   key |= 0x02;  // Symbol was previously pressed

                if (kbd_key)
                {
                    if (kbd_key == KBD_KEY_SYMDIR) key  |= 0x02;
                    if (kbd_key == ' ')            key  |= 0x01;
                    if (kbd_key == KBD_KEY_SYMBOL){key  |= 0x02; zx_special_key = 2;}
                    if (kbd_key == 'M')            key  |= 0x04;
                    if (kbd_key == 'N')            key  |= 0x08;
                    if (kbd_key == 'B')            key  |= 0x10;
                }
            }

            if (inv & 0x0200) // ASDFG
            {
                if (kbd_key)
                {
                    if (kbd_key == 'A')           key  |= 0x01;
                    if (kbd_key == 'S')           key  |= 0x02;
                    if (kbd_key == 'D')           key  |= 0x04;
                    if (kbd_key == 'F')           key  |= 0x08;
                    if (kbd_key == 'G')           key  |= 0x10;
                }
            }

            if (inv & 0x2000) // POIUY
            {
                if (kbd_key)
                {
                    if (kbd_key == 'P')           key  |= 0x01;
                    if (kbd_key == 'O')           key  |= 0x02;
                    if (kbd_key == 'I')           key  |= 0x04;
                    if (kbd_key == 'U')           key  |= 0x08;
                    if (kbd_key == 'Y')           key  |= 0x10;
                }
            }

            if (inv & 0x0100) // Shift, ZXCV row
            {
                if (zx_special_key == 1)     key |= 0x01; // Shift was previously pressed
                if (kbd_key)
                {
                    if (kbd_key == KBD_KEY_SFTDIR) key  |= 0x01;
                    if (kbd_key == KBD_KEY_SHIFT) {key  |= 0x01; zx_special_key = 1;}
                    if (kbd_key == 'Z')            key  |= 0x02;
                    if (kbd_key == 'X')            key  |= 0x04;
                    if (kbd_key == 'C')            key  |= 0x08;
                    if (kbd_key == 'V')            key  |= 0x10;
                }
            }

            if (inv & 0x0400) // QWERT row
            {
                if (kbd_key)
                {
                    if (kbd_key == 'Q')           key  |= 0x01;
                    if (kbd_key == 'W')           key  |= 0x02;
                    if (kbd_key == 'E')           key  |= 0x04;
                    if (kbd_key == 'R')           key  |= 0x08;
                    if (kbd_key == 'T')           key  |= 0x10;
                }
            }

            // Handle the Symbol and Shift keys which are special "modifier keys"
            if (((kbd_key >= 'A') && (kbd_key <= 'Z')) || ((kbd_key >= '0') && (kbd_key <= '9')) || (kbd_key == ' '))
            {
                bNonSpecialKeyWasPressed = 1;
            }
        }

        // If no key was pressed but we had a special key press previously... clear it.
        if (bNonSpecialKeyWasPressed && !kbd_keys_pressed)
        {
            zx_special_key = 0;
            bNonSpecialKeyWasPressed = 0;
            DisplayStatusLine(false);
        }

        return (u8)~key;
    }
    else
    {
        if (accurate_emulation)  CPU.TStates += 4;
        
        if ((Port & 0x3F) == 0x1F)  // Kempston Joystick interface... (only A5 driven low)
        {
            u8 joy1 = 0x00;

            if (JoyState & JST_RIGHT) joy1 |= 0x01;
            if (JoyState & JST_LEFT)  joy1 |= 0x02;
            if (JoyState & JST_DOWN)  joy1 |= 0x04;
            if (JoyState & JST_UP)    joy1 |= 0x08;
            if (JoyState & JST_FIRE)  joy1 |= 0x10;
            if (JoyState & JST_FIRE2) joy1 |= 0x20;
            
            return joy1;
        }
        else
        if ((Port & 0xc002) == 0xc000) // AY input
        {
            return ay38910DataR(&myAY);
        }
        else
        if ((Port & 0xBFFF) == 0xBF3B) // ULA+
        {
            // 0xBF3B is Register Port - write only (no read-back)
            // 0xFF3B is the Data Port - read/write
            if (myConfig.ULAplus)
            {
                if (Port == 0xFF3B)
                {
                    if ((zx_ula_plus_group >> 6) == 0x00) // Palette Group
                    {
                        return zx_ula_plus_palette[zx_ula_plus_palette_reg];
                    }
                    else if ((zx_ula_plus_group >> 6) == 0x01) // Mode Group
                    {
                        return zx_ula_plus_enabled;
                    }
                }
            }
         }
     }

    // ---------------------------------------------------------------------------------------------
    // Poor Man's floating bus. Very few games use this - so we basically handle it a bit roughly.
    // If we are not drawing the screen - the ULA will be idle and we will return 0xFF (below).
    // If we are rending the screen... we will return the Attribute byte mid-scanline which is
    // good enough for games like Sidewine and Short Circuit and Cobra, etc.
    // ---------------------------------------------------------------------------------------------
    if (myConfig.machine) // 128K
    {
        if (CPU.TStates >= CONTENTION_START_CYCLE_128 && CPU.TStates < (CONTENTION_START_CYCLE_128+(192*CYCLES_PER_SCANLINE_128)))
        {
            u8 *floatBusPtr;

            // For the ZX 128K, we might be using page 7 for video display... it's rare, but possible...
            if (zx_128k_mode) floatBusPtr = RAM_Memory128 + (((portFD & 0x08) ? 7:5) * 0x4000) + 0x1800;
            else floatBusPtr = RAM_Memory + 0x5800;

            u8 *attrPtr = &floatBusPtr[(last_line_drawn/8)*32];

            if (((CPU.TStates-CONTENTION_START_CYCLE_128) % CYCLES_PER_SCANLINE_128) < 128)
            {
                return attrPtr[((CPU.TStates-CONTENTION_START_CYCLE_128) % CYCLES_PER_SCANLINE_128) / 4];
            }
        }
    }
    else // 48K
    {
        if (CPU.TStates >= CONTENTION_START_CYCLE_48 && CPU.TStates < (CONTENTION_START_CYCLE_48+(192*CYCLES_PER_SCANLINE_48)))
        {
            u8 *floatBusPtr = RAM_Memory + 0x5800;
            u8 *attrPtr = &floatBusPtr[(last_line_drawn/8)*32];

            if (((CPU.TStates-CONTENTION_START_CYCLE_48) % CYCLES_PER_SCANLINE_48) < 128)
            {
                return attrPtr[((CPU.TStates-CONTENTION_START_CYCLE_48) % CYCLES_PER_SCANLINE_48) / 4];
            }
        }
    }

    return 0xFF;  // Unused port returns 0xFF when ULA is idle
}

// --------------------------------------------------------------------------------------
// For the ZX Spectrum 128K this is the banking routine that will swap the BIOS ROM and
// swap out the bank of memory that will be visible at 0xC000 in CPU address space.
// --------------------------------------------------------------------------------------
ITCM_CODE void zx_bank(u8 new_bank)
{
    if (portFD & 0x20) return; // Lock out - no more bank swaps allowed

    // Map in the correct bios segment... make sure this isn't a diagnostic ROM
    if ((speccy_mode != MODE_ZX81))
    {
        if (rom_special_bank)
        {
            MemoryMap[0] = ROM_Memory + (0x4000 * (rom_special_bank-1));
        }
        else
        {
            MemoryMap[0] = SpectrumBios128 + ((new_bank & 0x10) ? 0x4000 : 0x0000);
        }
    }

    // Map in the correct page of banked memory to 0xC000
    MemoryMap[3] = RAM_Memory128 + ((new_bank & 0x07) * 0x4000) - 0xC000;
    
    portFD = new_bank;

    // Set the upper bank of memory to 'contended' if we are swapping in an 'odd' 128K bank
    ContendMap[3] = (zx_128k_mode && (portFD & 1));
}

// A fast look-up table when we are rendering background pixels
u16 zx_border_colors[8] __attribute__((section(".dtcm"))) =
{
  (u16)RGB15(0x00,0x00,0x00),   // Black
  (u16)RGB15(0x00,0x00,0xD8),   // Blue
  (u16)RGB15(0xD8,0x00,0x00),   // Red
  (u16)RGB15(0xD8,0x00,0xD8),   // Magenta
  (u16)RGB15(0x00,0xD8,0x00),   // Green
  (u16)RGB15(0x00,0xD8,0xD8),   // Cyan
  (u16)RGB15(0xD8,0xD8,0x00),   // Yellow
  (u16)RGB15(0xFD,0xFD,0xFD),   // White
};


ITCM_CODE void cpu_writeport_speccy(register unsigned short Port,register unsigned char Value)
{
    if ((Port & 1) == 0) // Any even port (usually 0xFE) is our ULA and beeper output
    {
        // Change the background color as needed...
        if ((portFE & 0x07) != (Value & 0x07))
        {
             BG_PALETTE_SUB[1] = zx_border_colors[Value & 0x07];
        }

        // -------------------------------------------------------------------------------
        // For rapid pulsing... Mojon Twin games and games like Multidude will hit the 
        // speaker hard in the intro screens so we just respond as fast as we can here...
        // -------------------------------------------------------------------------------
        if ((portFE ^ Value) & 0x10)
        {
            if (portFE & 0x10) beeper_on_pulses++; else beeper_off_pulses++;
        }

        portFE = Value;
        
        if (accurate_emulation)
        {
             if (myConfig.machine) // 128K
             {
                 if (ContendMap[Port>>14])
                 {
                     CPU.TStates += cpu_contended_delay_128[(((CPU.TStates))+0) % CYCLES_PER_SCANLINE_128];
                     CPU.TStates += cpu_contended_delay_128[(((CPU.TStates))+1) % CYCLES_PER_SCANLINE_128];
                 }
                 else
                 {
                     CPU.TStates += cpu_contended_delay_128[(((CPU.TStates))+1) % CYCLES_PER_SCANLINE_128];
                 }
             }
             else // 48K
             {
                if (ContendMap[Port>>14])
                 {
                     CPU.TStates += cpu_contended_delay_48[(((CPU.TStates))+0) % CYCLES_PER_SCANLINE_48];
                     CPU.TStates += cpu_contended_delay_48[(((CPU.TStates))+1) % CYCLES_PER_SCANLINE_48];
                 }
                 else
                 {
                     CPU.TStates += cpu_contended_delay_48[(((CPU.TStates))+1) % CYCLES_PER_SCANLINE_48];
                 }
             }
        }
    }
    else
    {
         if (accurate_emulation)
         {
              if (ContendMap[Port>>14])
              {
                  if (myConfig.machine) // 128K
                  {
                      CPU.TStates += cpu_contended_delay_128[(((CPU.TStates))+0) % CYCLES_PER_SCANLINE_128];
                      CPU.TStates += cpu_contended_delay_128[(((CPU.TStates))+1) % CYCLES_PER_SCANLINE_128];
                      CPU.TStates += cpu_contended_delay_128[(((CPU.TStates))+2) % CYCLES_PER_SCANLINE_128];
                      CPU.TStates += cpu_contended_delay_128[(((CPU.TStates))+3) % CYCLES_PER_SCANLINE_128];
                  }
                  else // 48K
                  {
                      CPU.TStates += cpu_contended_delay_48[(((CPU.TStates))+0) % CYCLES_PER_SCANLINE_48];
                      CPU.TStates += cpu_contended_delay_48[(((CPU.TStates))+1) % CYCLES_PER_SCANLINE_48];
                      CPU.TStates += cpu_contended_delay_48[(((CPU.TStates))+2) % CYCLES_PER_SCANLINE_48];
                      CPU.TStates += cpu_contended_delay_48[(((CPU.TStates))+3) % CYCLES_PER_SCANLINE_48];
                  }
              }
         }
    }
    
    if (accurate_emulation)  CPU.TStates += 4;

    if (zx_128k_mode && ((Port & 0x8002) == 0x0000)) // 128K Bankswitch
    {
        zx_bank(Value);
    }
    else
    if ((Port & 0xc002) == 0xc000) // AY Register Select
    {
        ay38910IndexW(Value&0xF, &myAY);
        zx_AY_index_written = 1;
    }
    else if ((Port & 0xc002) == 0x8000) // AY Data Write
    {
        ay38910DataW(Value, &myAY);
        if (zx_AY_index_written) zx_AY_enabled = 1;
    }
    else
    if ((Port & 0xBFFF) == 0xBF3B) // ULA+
    {
        if (myConfig.ULAplus)
        {
            // 0xBF3B is Register Port
            // 0xFF3B is the Data Port            
            if (Port == 0xBF3B)
            {
                zx_ula_plus_group = Value;
                if ((zx_ula_plus_group >> 6) == 0x00) // Palette Group
                {
                    zx_ula_plus_palette_reg = (zx_ula_plus_group & 0x3F);
                }
                else
                {
                    // Mode Group selected... we don't support the Timex screen extensions.
                }
            }
            else if (Port == 0xFF3B)
            {
                if ((zx_ula_plus_group >> 6) == 0x00) // Palette Group
                {
                    u8 r = (Value >> 2) & 7;
                    u8 g = (Value >> 5) & 7;
                    u8 b = (Value & 3) << 1 | ((Value & 3) ? 1:0);
                    r = (r << 5) | (r << 2) | (r >> 1);
                    g = (g << 5) | (g << 2) | (g >> 1);
                    b = (b << 5) | (b << 2) | (b >> 1);
                    SPRITE_PALETTE[0x80|zx_ula_plus_palette_reg] = RGB15(r,g,b);
                    BG_PALETTE[0x80|zx_ula_plus_palette_reg]     = RGB15(r,g,b);
                    zx_ula_plus_palette[zx_ula_plus_palette_reg] = Value;
                }
                else if ((zx_ula_plus_group >> 6) == 0x01) // Mode Group
                {
                    zx_ula_plus_enabled = (Value & 1);
                }
            }
        }
    }
}

// --------------------------------------------------------------------------------------
// After we load a saved state, we need to re-apply the saved palette back to the DS LCD
// --------------------------------------------------------------------------------------
void apply_ula_plus_palette(void)
{
    for (int reg=0; reg<64; reg++)
    {
        u8 r = (zx_ula_plus_palette[reg] >> 2) & 7;
        u8 g = (zx_ula_plus_palette[reg] >> 5) & 7;
        u8 b = (zx_ula_plus_palette[reg] & 3) << 1 | ((zx_ula_plus_palette[reg] & 3) ? 1:0);
        r = (r << 5) | (r << 2) | (r >> 1);
        g = (g << 5) | (g << 2) | (g >> 1);
        b = (b << 5) | (b << 2) | (b >> 1);
        SPRITE_PALETTE[0x80|reg] = RGB15(r,g,b);
        BG_PALETTE[0x80|reg]     = RGB15(r,g,b);
    }
}

// ----------------------------------------------------------------------------
// ULA Plus render line - this repurposes the BRIGHT and FLASH bits as 
// palette select (0-3). Ink gets 8 colors and Paper gets 8 other colors
// and this provides 4 palette 'banks' of 16 colors for a total of 64 colors.
// ----------------------------------------------------------------------------
ITCM_CODE void speccy_render_screen_line_ula_plus(u32 *vidBuf, u8* attrPtr, u8*pixelPtr)
{
    // ---------------------------------------------------------------------
    // With 8 pixels per byte, there are 32 bytes of horizontal screen data
    // ---------------------------------------------------------------------
    for (int x=0; x<32; x++)
    {
        u8 attr = *attrPtr++;           // The color attribute for ULA+
        u8 pixel = *pixelPtr++;         // And here is 8 pixels to draw

        u8 ink   = 0x80 | (((attr >> 6) * 16) + (attr & 0x07));            // Ink Color is the foreground
        u8 paper = 0x80 | (((attr >> 6) * 16) + ((attr >> 3) & 0x07) + 8); // Paper is the background
        
        // --------------------------------------------------------------------------
        // And now the pixel drawing... We try to speed this up as much as possible.
        // --------------------------------------------------------------------------
        if (pixel) // Is at least one pixel on?
        {
            // ------------------------------------------------------------------------------------------------------------------------
            // I've tried look-up tables here, but nothing was as fast as checking the bit and shifting ink/paper to the right spot...
            // ------------------------------------------------------------------------------------------------------------------------
            *vidBuf++ = (((pixel & 0x80) ? ink:paper)) | (((pixel & 0x40) ? ink:paper) << 8) | (((pixel & 0x20) ? ink:paper) << 16) | (((pixel & 0x10) ? ink:paper) << 24);
            *vidBuf++ = (((pixel & 0x08) ? ink:paper)) | (((pixel & 0x04) ? ink:paper) << 8) | (((pixel & 0x02) ? ink:paper) << 16) | (((pixel & 0x01) ? ink:paper) << 24);
        }
        else // Just drawing all background which is common...
        {
            // --------------------------------------------------------------------------------------
            // Draw background directly to the screen - this is slightly faster than a lookup table.
            // --------------------------------------------------------------------------------------
            *vidBuf++ = (paper << 24) | (paper << 16) | (paper << 8) | paper;
            *vidBuf++ = (paper << 24) | (paper << 16) | (paper << 8) | paper;
        }
    }
}

// ----------------------------------------------------------------------------
// Render one screen line of pixels. This is called on every visible scanline
// and is heavily optimized to draw as fast as possible. Since the screen is
// often drawing background (paper vs ink), that's handled via look-up table.
// ----------------------------------------------------------------------------
ITCM_CODE void speccy_render_screen_line(u8 line)
{
    u32 *vidBuf;
    u8 *zx_ScreenPage = 0;

    if (line == 0) // At start of each new frame, handle the flashing 'timer'
    {
        if (isDSiMode() && !tape_is_playing()) // For the DSi we can double-buffer and draw the screen in the background
        {
            if (bRenderSkipOnce) bRenderSkipOnce=0;
            else backgroundRenderScreen = 0x80 | (flash_timer & 1);
        }
        tape_play_skip_frame++;
        if (++flash_timer & 0x10) {flash_timer=0; bFlash ^= 1;} // Same timing as real ULA - 16 frames on and 16 frames off
    }

    if (isDSiMode() && !tape_is_playing())
    {
        vidBuf = (u32*) ((flash_timer & 1 ? 0x06820000:0x06830000) + (line << 8));    // Video buffer... write 32-bits at a time for maximum speed
    }
    else // For the DS-Lite/Phat we direct render for speed - also when tape is loading...
    {
        vidBuf = (u32*)(0x06000000 + (line << 8));    // Video buffer... write 32-bits at a time for maximum speed
        bRenderSkipOnce = 1; // When we stop the tape, we want to allow the first frame to re-draw before rendering
    }

    // -----------------------------------------------------------------------------
    // Only draw one out of every 16 frames when we are loading tape. We want to
    // give as much horsepower to the emulation CPU as possible here - this is
    // good enough to let the screen draw slowly in the background as we have time.
    // -----------------------------------------------------------------------------
    if (tape_is_playing())
    {
        if (tape_play_skip_frame & 0x0F) return;
    }

    // -----------------------------------------------------------
    // For DS-Lite/Phat, we draw every other frame to gain speed.
    // -----------------------------------------------------------
    if (!isDSiMode() && (flash_timer & 1)) return;

    // -----------------------------------------------------------------------
    // Render the current line into our NDS video memory. For the ZX 128K, we
    // might be using page 7 for video display... it's rare, but possible...
    // -----------------------------------------------------------------------
    if (zx_128k_mode) zx_ScreenPage = RAM_Memory128 + ((portFD & 0x08) ? 7:5) * 0x4000;
    else zx_ScreenPage = RAM_Memory + 0x4000;

    // ----------------------------------------------------------------
    // The color attribute is stored independently from the pixel data
    // ----------------------------------------------------------------
    u8 *attrPtr = &zx_ScreenPage[0x1800 + ((line/8)*32)];
    word offset = ((line&0x07) << 8) | ((line&0x38) << 2) | ((line&0xC0) << 5);
    u8 *pixelPtr = zx_ScreenPage+offset;
    
    if (zx_ula_plus_enabled) speccy_render_screen_line_ula_plus(vidBuf, attrPtr, pixelPtr);
    else
    // ---------------------------------------------------------------------
    // With 8 pixels per byte, there are 32 bytes of horizontal screen data
    // ---------------------------------------------------------------------
    for (int x=0; x<32; x++)
    {
        u8 attr = *attrPtr++;           // The color attribute and possible flashing
        u8 paper = ((attr>>3) & 0x0F);  // Paper is the background
        u8 pixel = *pixelPtr++;         // And here is 8 pixels to draw

        if (attr & 0x80) // Flashing swaps pen/ink
        {
            if (bFlash) pixel = ~pixel; // Faster to just invert the pixel itself...
        }

        // --------------------------------------------------------------------------
        // And now the pixel drawing... We try to speed this up as much as possible.
        // --------------------------------------------------------------------------
        if (pixel) // Is at least one pixel on?
        {
            u8 ink   = (attr & 0x07);       // Ink Color is the foreground
            if (attr & 0x40) ink |= 0x08;   // Brightness

            // ------------------------------------------------------------------------------------------------------------------------
            // Bit shifting was faster in a few cases, but generally this lookup table in this specific order is generally fastest.
            // The paper color changes least frequently so it improves the cache density if the lookup is in this specific order.
            // ------------------------------------------------------------------------------------------------------------------------
            *vidBuf++ = pre_render_lookup[paper][ink][pixel>>4];
            *vidBuf++ = pre_render_lookup[paper][ink][pixel&0xF];
        }
        else // Just drawing all background which is common...
        {
            // --------------------------------------------------------------------------------------
            // Draw background directly to the screen - this is slightly faster than a lookup table.
            // --------------------------------------------------------------------------------------
            *vidBuf++ = (paper << 24) | (paper << 16) | (paper << 8) | paper;
            *vidBuf++ = (paper << 24) | (paper << 16) | (paper << 8) | paper;
        }
    }
}


// -----------------------------------------------------
// Z80 Snapshot v1 is always a 48K game...
// The header is 30 bytes long - most of which will be
// used when we reset the game to set the state of the
// CPU registers, Stack Pointer and Program Counter.
// -----------------------------------------------------
u8 decompress_v1(int romSize)
{
    int offset = 0; // Current offset into memory

    isCompressed = (ROM_Memory[12] & 0x20 ? 1:0); // V1 files are usually compressed

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
        isCompressed = 1;
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
void speccy_decompress_z80(int romSize)
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



// ----------------------------------------------------------------------
// Reset the emulation. Freshly decompress the contents of RAM memory
// and setup the CPU registers exactly as the snapshot indicates. Then
// we can start the emulation at exactly the point it left off... This
// works fine so long as the game does not need to go back out to the
// tape to load another segment of the game.
// ----------------------------------------------------------------------
void speccy_reset(void)
{
    tape_reset();
    tape_patch();
    pok_init();
    
    for (int pixel=0; pixel<16; pixel++)
        for (int paper=0; paper<16; paper++)
            for (int ink=0; ink<16; ink++)
            {
                pre_render_lookup[paper][ink][pixel] = (((pixel & 0x08) ? ink:paper)) | (((pixel & 0x04) ? ink:paper) << 8) | (((pixel & 0x02) ? ink:paper) << 16) | (((pixel & 0x01) ? ink:paper) << 24);
            }
    
    ContendMap[3] = 0; // Assume no contention on upper memory until 128K odd bank mapped in
    
    // Restore the original palette (in case ULA+ changed it)
    spectrumSetPalette();

    // Default to a simplified memory map - remap as needed below
    MemoryMap[0] = RAM_Memory + 0x0000;
    MemoryMap[1] = RAM_Memory + 0x4000 - 0x4000;    // Yes, this looks silly but we do this
    MemoryMap[2] = RAM_Memory + 0x8000 - 0x8000;    // to illustrate that we offset each
    MemoryMap[3] = RAM_Memory + 0xC000 - 0xC000;    // MemoryMap[] by 16K for fast indexing in Z80.c

    CPU.PC.W            = 0x0000;
    portFE              = 0x00;
    portFD              = 0x00;
    zx_AY_enabled       = 0;
    zx_AY_index_written = 0;
    zx_special_key      = 0;

    zx_128k_mode        = 0;   // Assume 48K until told otherwise
    rom_special_bank    = 0;   // Assume no special ROM in SLOT0 until proven otherwise below
    
    accurate_emulation  = 0;   // Set to 1 when we need to handle more accurate TState accounting / Contended Memory
    
    zx_ula_plus_enabled     = 0;   // Assume no ULA+ (normal Spectrum ULA)
    zx_ula_plus_group       = 0x00;
    zx_ula_plus_palette_reg = 0x00;
    memset(zx_ula_plus_palette, 0x00, sizeof(zx_ula_plus_palette));

    backgroundRenderScreen = 0;
    bRenderSkipOnce        = 1;

    // ------------------------------------------------
    // Handle parsing of the .tap or .tzx tape formats
    // into easy to manipulate block buffers.
    // ------------------------------------------------
    if (speccy_mode < MODE_SNA)
    {
        tape_parse_blocks(last_file_size);
        strcpy(last_file, initial_file);
        strcpy(last_path, initial_path);
    }
    else if (speccy_mode < MODE_ROM)
    {
        // -------------------------------------------------------------------
        // If we are one of the snapshot formats (Z80, SNA), decompress it...
        // -------------------------------------------------------------------
        speccy_decompress_z80(last_file_size);
    }

    // ---------------------------------------------------------------------------
    // Handle putting the various snapshot formats back into memory. We want to
    // ensure that the memory is exactly as it 'was' when the snapshot was taken.
    // ---------------------------------------------------------------------------

    if (speccy_mode == MODE_SNA) // SNA snapshot
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
    else if (speccy_mode == MODE_ROM) // Diagnostic ROM or Dandanator ROM - always launch in ZX 128K mode
    {
        // Move the ROM into memory...
        MemoryMap[0] = ROM_Memory;   // Load ROM into place (for Dandanator, this will be bank 0)
        rom_special_bank = 1;        // And ensure this gets handled in zx_bank()
        dandy_disabled   = (last_file_size <= (16*1024)) ? 1:0;  // Dandanator starts in enabled mode if > 16K

        if (zx_force_128k_mode || myConfig.machine)
        {
            zx_128k_mode = 1;
            myConfig.machine = 1;

            // Now set the memory map to point to the right banks...
            MemoryMap[1] = RAM_Memory128 + (5 * 0x4000) - 0x4000; // Bank 5
            MemoryMap[2] = RAM_Memory128 + (2 * 0x4000) - 0x8000; // Bank 2
            MemoryMap[3] = RAM_Memory128 + (0 * 0x4000) - 0xC000; // Bank 0
        }
    }
    else if (speccy_mode == MODE_ZX81) // Special handling for Paul Farrow's Interface 2 ROMs
    {
        // Move the ZX81 Emulator ROM into memory where the BIOS would normally be...
        memcpy(RAM_Memory, ZX81Emulator, 0x4000);

        // And force 128K mode needed for ZX81 emulation
        zx_128k_mode = 1;
        myConfig.machine = 1;

        // Now set the memory map to point to the right banks...
        MemoryMap[1] = RAM_Memory128 + (5 * 0x4000) - 0x4000; // Bank 5
        MemoryMap[2] = RAM_Memory128 + (2 * 0x4000) - 0x8000; // Bank 2
        MemoryMap[3] = RAM_Memory128 + (0 * 0x4000) - 0xC000; // Bank 0
    }
    else if (speccy_mode < MODE_SNA) // TAP or TZX file - 48K or 128K
    {
        // BIOS will be loaded further below...
        if (zx_force_128k_mode || myConfig.machine)
        {
            zx_128k_mode = 1;
            myConfig.machine = 1;

            // Now set the memory map to point to the right banks...
            MemoryMap[1] = RAM_Memory128 + (5 * 0x4000) - 0x4000; // Bank 5
            MemoryMap[2] = RAM_Memory128 + (2 * 0x4000) - 0x8000; // Bank 2
            MemoryMap[3] = RAM_Memory128 + (0 * 0x4000) - 0xC000; // Bank 0
        }
    }
    else // Z80 snapshot
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

    // And put the BIOS into place into the memory map...
    if ((speccy_mode != MODE_ROM) && (speccy_mode != MODE_ZX81))
    {
        MemoryMap[0] = (zx_128k_mode ? SpectrumBios128 : SpectrumBios);
    }
}


// -----------------------------------------------------------------------------
// Run the emulation for exactly 1 scanline and handle the VDP interrupt if
// the emulation has executed the last line of the frame.  This also handles
// direct beeper and possibly AY sound emulation as well. Crude but effective.
// -----------------------------------------------------------------------------
ITCM_CODE u32 speccy_run(void)
{
    ++zx_current_line; // This is the pixel line we're working on...
    int starting_line = (myConfig.machine ? 63:64)+myConfig.ULAtiming; // And this is the first line we draw (48K machines start 1 line later)

    // ----------------------------------------------
    // Execute 1 scanline worth of CPU instructions.
    //
    // We break this up into pieces in order to
    // get more chances to render the audio beeper
    // which requires a fairly high sample rate...
    // -----------------------------------------------
    if (tape_state)
    {
        // If we are playing back the tape - just run the emulation as fast as possible
        zx_ScreenRendering = 0;
        ExecZ80_Speccy(CPU.TStates + (zx_128k_mode ? CYCLES_PER_SCANLINE_128:CYCLES_PER_SCANLINE_48));

        if (CPU.TStates > 0xFFFE0000) // Too close to the wrap point, should never happen but trap it out so we don't crash the emulation
        {
            CPU.TStates = 0;
            last_edge = 0;
            tape_stop();
        }
    }
    else
    {
        ExecZ80_Speccy(CPU.TStates + 128); // Execute CPU for the visible portion of the scanline

        zx_ScreenRendering = 0; // On this final chunk we are drawing border and doing a horizontal sync... no contention

        ExecZ80_Speccy((zx_128k_mode ? CYCLES_PER_SCANLINE_128:CYCLES_PER_SCANLINE_48) * zx_current_line); // This puts us exactly where we should be for the scanline
        
        // Grab 4 samples worth of AY sound to mix with the beeper
        processDirectAudio();

        // -----------------------------------------------------------------------
        // If we are not playing the tape, we want to reset the TStates counter
        // on every new frame to help us with the somewhat complex handling of
        // the memory contention which is heavily dependent on CPU Cycle counts.
        // -----------------------------------------------------------------------
        if (zx_current_line == (zx_128k_mode ? SCANLINES_PER_FRAME_128:SCANLINES_PER_FRAME_48))
        {
            CPU.TStates = 0;
            last_edge = 0;
        }
    }

    // -----------------------------------------------------------
    // Render one line if we're in the visible area of the screen
    // This is scalines 64 (first visible scanline) to 255 (last
    // visible scanline for a total of 192 scanlines).
    // -----------------------------------------------------------
    accurate_emulation = 0;
    if ((zx_current_line >= starting_line) && (zx_current_line < 192+starting_line))
    {
        // Render one scanline...
        speccy_render_screen_line(zx_current_line - starting_line);
        last_line_drawn++;
        zx_ScreenRendering = 1;
        accurate_emulation = (tape_state ? 0 : 1); // If tape playing, skip accurate emulation
    } else last_line_drawn = 0;

    // ------------------------------------------
    // Generate an interrupt only at end of frame
    // ------------------------------------------
    if (zx_current_line == (zx_128k_mode ? SCANLINES_PER_FRAME_128:SCANLINES_PER_FRAME_48))
    {
        zx_current_line = 0;
        zx_ScreenRendering = 0;
        CPU.IRequest = INT_RST38;
        CPU.TStates_IRequest = CPU.TStates;
        IntZ80(&CPU, CPU.IRequest);
        return 0; // End of frame
    }

    return 1; // Not end of frame
}

#pragma GCC diagnostic pop

// End of file

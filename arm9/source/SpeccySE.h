// =====================================================================================
// Copyright (c) 2025 Dave Bernazzani (wavemotion-dave)
//
// Copying and distribution of this emulator, its source code and associated
// readme files, with or without modification, are permitted in any medium without
// royalty provided this copyright notice is used and wavemotion-dave and Marat
// Fayzullin (ColEM core) are thanked profusely.
//
// The SpeccySE emulator is offered as-is, without any warranty. Please see readme.md
// =====================================================================================
#ifndef _SPECCYSE_H_
#define _SPECCYSE_H_

#include <nds.h>
#include <string.h>

extern u32 debug[0x10];
extern u32 DX, DY;

// These are the various special icons/menu operations
#define MENU_CHOICE_NONE        0x00
#define MENU_CHOICE_RESET_GAME  0x01
#define MENU_CHOICE_END_GAME    0x02
#define MENU_CHOICE_SAVE_GAME   0x03
#define MENU_CHOICE_LOAD_GAME   0x04
#define MENU_CHOICE_HI_SCORE    0x05
#define MENU_CHOICE_DEFINE_KEYS 0x06
#define MENU_CHOICE_POKE_MEMORY 0x07
#define MENU_CHOICE_CASSETTE    0x08
#define MENU_CHOICE_MENU        0xFF        // Special brings up a mini-menu of choices

// ------------------------------------------------------------------------------
// Joystick UP, RIGHT, LEFT, DOWN and the FIRE button for the Kempston Joystick.
// Designed specifically so each has its own bit so we can press more than one
// direction/fire at the same time.  Keyboard keys are grafted onto this below.
// ------------------------------------------------------------------------------
#define JST_UP              0x0001
#define JST_RIGHT           0x0002
#define JST_DOWN            0x0004
#define JST_LEFT            0x0008
#define JST_FIRE            0x0010

// -----------------------------------------------------------------------------------
// And these are meta keys for mapping NDS keys to keyboard keys (many of the computer
// games don't use joystick inputs and so need to map to keyboard keys...)
// -----------------------------------------------------------------------------------
#define META_KBD_A          0xF001
#define META_KBD_B          0xF002
#define META_KBD_C          0xF003
#define META_KBD_D          0xF004
#define META_KBD_E          0xF005
#define META_KBD_F          0xF006
#define META_KBD_G          0xF007
#define META_KBD_H          0xF008
#define META_KBD_I          0xF009
#define META_KBD_J          0xF00A
#define META_KBD_K          0xF00B
#define META_KBD_L          0xF00C
#define META_KBD_M          0xF00D
#define META_KBD_N          0xF00E
#define META_KBD_O          0xF00F
#define META_KBD_P          0xF010
#define META_KBD_Q          0xF011
#define META_KBD_R          0xF012
#define META_KBD_S          0xF013
#define META_KBD_T          0xF014
#define META_KBD_U          0xF015
#define META_KBD_V          0xF016
#define META_KBD_W          0xF017
#define META_KBD_X          0xF018
#define META_KBD_Y          0xF019
#define META_KBD_Z          0xF01A

#define META_KBD_0          0xF01B
#define META_KBD_1          0xF01C
#define META_KBD_2          0xF01D
#define META_KBD_3          0xF01E
#define META_KBD_4          0xF01F
#define META_KBD_5          0xF020
#define META_KBD_6          0xF021
#define META_KBD_7          0xF022
#define META_KBD_8          0xF023
#define META_KBD_9          0xF024

#define META_KBD_SHIFT      0xF025
#define META_KBD_SYMBOL     0xF026
#define META_KBD_SPACE      0xF029
#define META_KBD_RETURN     0xF02A

#define MAX_KEY_OPTIONS     45

// -----------------------------
// For the Full Keyboard...
// -----------------------------
#define KBD_KEY_SYMBOL      1
#define KBD_KEY_SHIFT       2
#define KBD_KEY_SFTDIR      3
#define KBD_KEY_SYMDIR      4
#define KBD_KEY_RET         13

// What format is the input file?
#define MODE_TAP            1
#define MODE_TZX            2
#define MODE_RES1           3
#define MODE_RES2           4
#define MODE_SNA            5
#define MODE_Z80            6
#define MODE_BIOS           7
#define MODE_ZX81           8

#define WAITVBL swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank();

// -------------------------------------------------------------------------
// This massive patch table consumes 256K (64 x 4 byte function pointers)
// to allow us faster access to patch routines for tape edge detection.
// We put it in LCD VRAM as this is slightly faster access on the DS/DSi.
// 99% of this massive array will be zeroes but we don't have another use
// for it and it does help speed up the patch lookup - so why not?!
// -------------------------------------------------------------------------
typedef u8 (*patchFunc)(void);
#define PatchLookup ((patchFunc*)0x06860000)

extern u8 speccy_mode;
extern u8 kbd_keys_pressed;
extern u8 kbd_keys[12];
extern u16 emuFps;
extern u16 emuActFrames;
extern u16 timingFrames;
extern char initial_file[];
extern char initial_path[];
extern u16 nds_key;
extern u8  kbd_key;
extern u16 vusCptVBL;
extern u16 keyCoresp[MAX_KEY_OPTIONS];
extern u8 soundEmuPause;
extern int bg0, bg1, bg0b, bg1b;
extern u32 last_file_size;
extern u8  zx_special_key;
extern u32 zx_current_line;
extern u8  zx_contend_delay;
extern u16 num_blocks_available;
extern u16 current_block;
extern u8  tape_state;
extern u32 current_block_data_idx;
extern u32 tape_bytes_processed;
extern u32 header_pulses;
extern u16 current_bit;
extern u32 current_bytes_this_block;
extern u8  custom_pulse_idx;
extern u8  handle_last_bits;
extern u16 loop_counter;
extern u16 loop_block;
extern u32 next_edge1;
extern u32 next_edge2;
extern u8 give_up_counter;
extern u32 last_edge;
extern u8 bottom_screen;
extern char *loader_type;
extern u8 bZX81EmuFound;
extern u8 backgroundRenderScreen;

extern void BottomScreenOptions(void);
extern void BottomScreenCassette(void);
extern void BottomScreenKeyboard(void);
extern void PauseSound(void);
extern void UnPauseSound(void);
extern void ResetStatusFlags(void);
extern void ReadFileCRCAndConfig(void);
extern void DisplayStatusLine(bool bForce);
extern void CassetteInsert(char *filename);
extern void ResetSpectrum(void);
extern void processDirectAudio(void);
extern u8   speccyTapePosition(void);
extern void tape_frame(void);
extern void debug_init();
extern void debug_save();
extern void debug_printf(const char * str, ...);

#endif // _SPECCYSE_H_

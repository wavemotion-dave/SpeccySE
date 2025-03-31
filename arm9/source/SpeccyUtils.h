// =====================================================================================
// Copyright (c) 2025 Dave Bernazzani (wavemotion-dave)
//
// Copying and distribution of this emulator, its source code and associated
// readme files, with or without modification, are permitted in any medium without
// royalty provided this copyright notice is used and wavemotion-dave and Marat 
// Fayzullin (ColEM core) are thanked profusely.
//
// The SpeccyDS emulator is offered as-is, without any warranty. Please see readme.md
// =====================================================================================
#ifndef _SPECCY_UTILS_H_
#define _SPECCY_UTILS_H_
#include <nds.h>
#include "SpeccyDS.h"
#include "cpu/z80/Z80_interface.h"
#include "cpu/ay38910/AY38910.h"

#define MAX_ROMS                    2048
#define MAX_ROM_NAME                160
#define MAX_CART_SIZE               (512*1024) // 512K is big enough for any .TAP/.TZX or Snapshot 
            
#define MAX_CONFIGS                 1024
#define CONFIG_VER                  0x0000
            
#define SPECCY_FILE                 0x01
#define DIRECTORY                   0x02
            
#define ID_SHM_CANCEL               0x00
#define ID_SHM_YES                  0x01
#define ID_SHM_NO                   0x02
            
#define DPAD_NORMAL                 0
#define DPAD_DIAGONALS              1


typedef struct {
  char szName[MAX_ROM_NAME+1];
  u8 uType;
  u32 uCrc;
} FISpeccy;


struct __attribute__((__packed__)) GlobalConfig_t
{
    u16 config_ver;
    u32 bios_checksums;
    char szLastRom[MAX_ROM_NAME+1];
    char szLastPath[MAX_ROM_NAME+1];
    char reserved1[MAX_ROM_NAME+1];
    char reserved2[MAX_ROM_NAME+1];
    u8  showFPS;
    u8  emuText;
    u8  global_01;
    u8  global_02;
    u8  global_03;
    u8  global_04;
    u8  global_05;
    u8  global_06;
    u8  global_07;
    u8  global_08;
    u8  global_09;
    u8  global_10;
    u8  global_11;
    u8  global_12;
    u8  debugger;
    u32 config_checksum;
};

struct __attribute__((__packed__)) Config_t
{
    u32 game_crc;
    u8  keymap[12];
    u8  frameSkip;
    u8  frameBlend;
    u8  autoFire;
    u8  tapeSpeed;
    u8  dpad;
    u8  gameSpeed;
    u8  reserved1;
    u8  reserved2;
    u8  reserved3;
    u8  reserved4;
    u8  reserved5;
    u8  reserved6;
    u8  reserved7;
    u8  reserved8;
    u8  reserved9;
};
 

extern struct Config_t       myConfig;
extern struct GlobalConfig_t myGlobalConfig;

extern u8 last_special_key;
extern u8 last_special_key_dampen;

extern u16 JoyState;                    // Joystick / Paddle management

extern u32 file_crc;
extern u8 ctc_enabled;

extern void ProcessBufferedKeys(void);
extern u8 BufferedKeys[32];
extern u8 BufferedKeysWriteIdx;
extern u8 BufferedKeysReadIdx;
extern u16 keyboard_interrupt;
extern u16 joystick_interrupt;
extern u8 zx_force_128k_mode;
extern u8 bFlash;
extern u32 flash_timer;

extern u8 portFE, portFD;
extern u8 zx_AY_enabled;
extern u8 zx_128k_mode;
extern u8 zx_ScreenRendering;

extern u8 SpectrumBios[];
extern u8 SpectrumBios128[];

extern u8 ROM_Memory[MAX_CART_SIZE];
extern u8 RAM_Memory[0x10000];
extern u8 RAM_Memory128[0x20000];

extern u8 *MemoryMap[8];
extern AY38910 myAY;

extern FISpeccy gpFic[MAX_ROMS];  
extern int uNbRoms;
extern int ucGameAct;
extern int ucGameChoice;

extern void LoadConfig(void);
extern u8 showMessage(char *szCh1, char *szCh2);
extern void speccyDSFindFiles(u8 bTapeOnly);
extern void speccyDSChangeOptions(void);
extern void DSPrint(int iX,int iY,int iScr,char *szMessage);
extern unsigned int crc32 (unsigned int crc, const unsigned char *buf, unsigned int len);

extern void FadeToColor(unsigned char ucSens, unsigned short ucBG, unsigned char ucScr, unsigned char valEnd, unsigned char uWait);
extern u8 speccyDSLoadFile(u8 bTapeOnly);
extern void DisplayFileName(void);
extern u32 ReadFileCarefully(char *filename, u8 *buf, u32 buf_size, u32 buf_offset);

extern u8 loadrom(const char *path);

extern u8 spectrumInit(char *szGame);
extern void spectrumSetPalette(void);
extern void spectrumRun(void);
extern void tape_search_for_loader(void);

extern unsigned char cpu_readport_speccy(register unsigned short Port);
extern void cpu_writeport_speccy(register unsigned short Port,register unsigned char Value);
extern void speccy_decompress_z80(int romSize);
extern void speccy_reset(void);
extern u32  speccy_run(void);
extern u8   tape_pulse(void);
extern void tape_reset(void);
extern void tape_patch(void);
extern void tape_stop(void);
extern void tape_play(void);
extern u8 tape_is_playing(void);
extern void tape_parse_blocks(int tapeSize);
extern u32 tape_bytes_processed;
extern void getfile_crc(const char *path);

extern void spectrumLoadState();
extern void spectrumSaveState();

extern void intro_logo(void);
extern void BufferKey(u8 key);

#endif // _SPECCY_UTILS_H_

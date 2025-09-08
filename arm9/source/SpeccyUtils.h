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
#ifndef _SPECCY_UTILS_H_
#define _SPECCY_UTILS_H_
#include <nds.h>
#include "SpeccySE.h"
#include "cpu/z80/Z80_interface.h"
#include "cpu/ay38910/AY38910.h"

#define MAX_FILES                   2048
#define MAX_FILENAME_LEN            160
#define MAX_TAPE_SIZE               (640*1024) // 640K is big enough for any .TAP/.TZX or Snapshot

#define MAX_CONFIGS                 4096
#define CONFIG_VERSION              0x0004

#define SPECCY_FILE                 0x01
#define DIRECTORY                   0x02

#define ID_SHM_CANCEL               0x00
#define ID_SHM_YES                  0x01
#define ID_SHM_NO                   0x02

#define DPAD_NORMAL                 0
#define DPAD_DIAGONALS              1
#define DPAD_SLIDE_N_GLIDE          2

extern char last_path[MAX_FILENAME_LEN];
extern char last_file[MAX_FILENAME_LEN];

typedef struct {
  char szName[MAX_FILENAME_LEN+1];
  u8 uType;
  u32 uCrc;
} FISpeccy;


extern u32 file_size;

typedef struct
{
    char description[33];
    u16  block_id;
} TapePositionTable_t;

extern TapePositionTable_t TapePositionTable[];

struct __attribute__((__packed__)) GlobalConfig_t
{
    u16 config_ver;
    u32 bios_checksums;
    char szLastFile[MAX_FILENAME_LEN+1];
    char szLastPath[MAX_FILENAME_LEN+1];
    char reserved1[MAX_FILENAME_LEN+1];
    char reserved2[MAX_FILENAME_LEN+1];
    u8  showFPS;
    u8  lastDir;
    u8  defMachine;
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
    u8  compressed;
    u8  debugger;
    u32 config_checksum;
};

struct __attribute__((__packed__)) Config_t
{
    u32 game_crc;
    u8  keymap[12];
    u8  contention;
    u8  autoStop;
    u8  autoFire;
    u8  tapeSpeed;
    u8  dpad;
    u8  autoLoad;
    u8  machine;
    u8  gameSpeed;
    u8  reserved3;
    u8  reserved4;
    u8  reserved5;
    u8  reserved6;
    u8  reserved7;
    u8  reserved8;
    u8  reserved9;
    u8  reserved10;
};

extern struct Config_t       myConfig;
extern struct GlobalConfig_t myGlobalConfig;

extern u8 last_special_key;
extern u8 last_special_key_dampen;

extern u16 JoyState;

extern u32 file_crc;
extern u8 bFirstTime;
extern u8 show_tape_counter;

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
extern u8 tape_play_skip_frame;

extern u8 SpectrumBios[0x4000];
extern u8 SpectrumBios128[0x8000];
extern u8 ZX81Emulator[0x4000];

extern u8 ROM_Memory[MAX_TAPE_SIZE];
extern u8 RAM_Memory[0x10000];
extern u8 RAM_Memory128[0x20000];

extern u8 *MemoryMap[4];
extern AY38910 myAY;

extern FISpeccy gpFic[MAX_FILES];
extern int uNbRoms;
extern int ucGameAct;
extern int ucGameChoice;
extern u8 CompressBuffer[];


extern void LoadConfig(void);
extern u8   showMessage(char *szCh1, char *szCh2);
extern void speccySEFindFiles(u8 bTapeOnly);
extern void speccySEChangeOptions(void);
extern void DSPrint(int iX,int iY,int iScr,char *szMessage);
extern u32  crc32 (unsigned int crc, const unsigned char *buf, unsigned int len);
extern void FadeToColor(unsigned char ucSens, unsigned short ucBG, unsigned char ucScr, unsigned char valEnd, unsigned char uWait);
extern u8   speccySELoadFile(u8 bTapeOnly);
extern void DisplayFileName(void);
extern void DisplayFileNameCassette(void);
extern u32  ReadFileCarefully(char *filename, u8 *buf, u32 buf_size, u32 buf_offset);
extern u8   loadgame(const char *path);
extern u8   spectrumInit(char *szGame);
extern void spectrumSetPalette(void);
extern void spectrumRun(void);
extern void tape_search_for_loader(void);
extern void tape_detect_loading(void);
extern u8   cpu_readport_speccy(register unsigned short Port);
extern void cpu_writeport_speccy(register unsigned short Port,register unsigned char Value);
extern void speccy_decompress_z80(int romSize);
extern void speccy_reset(void);
extern u32  speccy_run(void);
extern u8   tape_pulse(void);
extern void tape_reset(void);
extern void tape_patch(void);
extern void tape_stop(void);
extern void tape_play(void);
extern void tape_position(u8 newPos);
extern u8   tape_find_positions(void);
extern u8   tape_is_playing(void);
extern void tape_parse_blocks(int tapeSize);
extern void getfile_crc(const char *path);
extern void spectrumLoadState();
extern void spectrumSaveState();
extern void intro_logo(void);
extern void BufferKey(u8 key);
extern void ProcessBufferedKeys(void);
extern void SpeccySEChangeKeymap(void);
extern void pok_select(void);
extern void pok_init();

#endif // _SPECCY_UTILS_H_

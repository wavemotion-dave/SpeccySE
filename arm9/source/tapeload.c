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

#define MAX_TAPE_BLOCKS                 2048

#define BLOCK_ID_STANDARD               0x10
#define BLOCK_ID_TURBO                  0x11
#define BLOCK_ID_PURE_TONE              0x12
#define BLOCK_ID_PULSE_SEQ              0x13
#define BLOCK_ID_PURE_DATA              0x14
#define BLOCK_ID_PAUSE_STOP             0x20
#define BLOCK_ID_GROUP_START            0x21
#define BLOCK_ID_GROUP_END              0x22
#define BLOCK_ID_LOOP_START             0x24
#define BLOCK_ID_LOOP_END               0x25
#define BLOCK_ID_STOP_IF_48K            0x2A
#define BLOCK_ID_TEXT                   0x30


#define TAPE_STOP                       0x00
#define TAPE_START                      0x01
#define TAPE_NEXT_BLOCK                 0x02
#define BLOCK_PILOT_TONE                0x03
#define SYNC_PULSE                      0x04
#define SEND_DATA_BYTES                 0x05
#define TAPE_DELAY_AFTER                0x06
#define CUSTOM_PULSE_SEQ                0x07

// Yes, this is special. It happens frequently enough we trap on the high bit here...
#define SEND_DATA_BITS                  0x80


// Some defaults mostly for the .TAP files as the .TZX
// will override some/many of these...
#define DEFAULT_PILOT_LENGTH            2168
#define DEFAULT_DATA_ZERO_PULSE_WIDTH    855
#define DEFAULT_DATA_ONE_PULSE_WIDTH    1710
#define DEFAULT_SYNC_PULSE1_WIDTH        667
#define DEFAULT_SYNC_PULSE2_WIDTH        735
#define DEFAULT_TAPE_GAP_DELAY_MS       1000
#define DEFAULT_HEADER_PULSE_TOGGLES    8063
#define DEFAULT_DATA_PULSE_TOGGLES      3223
#define DEFAULT_LAST_USED_BITS             8

typedef struct
{
  u8   id;                      // The block ID (STANDARD, TURBO, DELAY, etc)
  u8   block_flag;              // The 0x00=Header, 0xFF=DATA flag
  u16  pilot_length;            // Length of the pilot pulse {2168} edge-to-edge
  u16  pilot_pulses;            // Number of pilot pulses {8063 with header flag, 3223 with data flag}
  u16  sync1_width;             // Length of SYNC first pulse {667}
  u16  sync2_width;             // Length of SYNC second pulse {735}
  u16  data_zero_width;         // How wide the zero '0' bit pulse is {855}
  u16  data_zero_widthX2;       // How wide the zero '0' bit pulse is {855*2}
  u16  data_one_width;          // How wide the one '1' bit pulse is {1710}
  u16  data_one_widthX2;        // How wide the one '1' bit pulse is {1710*2}
  u8   last_bits_used;          // The number of bits used in the last byte
  u16  gap_delay_after;         // How many milliseconds delay after this block {1000}
  u16  loop_counter;            // For Loops... how many times to iterate
  u32  block_data_idx;          // Where does the block data start (after header stuff is parsed)
  u32  block_data_len;          // How many bytes are in the data stream for this block?
  char description[31];         // For text / meta / description / group blocks (they can be larger, but this is all we will show)
  char block_filename[11];      // For the filename in a header block
  u16  custom_pulse_len[255];   // For the BLOCK_ID_PULSE_SEQ type of block
} TapeBlock_t;

TapeBlock_t TapeBlocks[MAX_TAPE_BLOCKS];  // The .TAP or .TZX will be parsed and this will be filled in.
u8  tape_state                  __attribute__((section(".dtcm"))) = TAPE_STOP;
u16 num_blocks_available        __attribute__((section(".dtcm"))) = 0;
u16 current_block               __attribute__((section(".dtcm"))) = 0;
u32 current_block_data_idx      __attribute__((section(".dtcm"))) = 0;
u32 tape_bytes_processed        __attribute__((section(".dtcm"))) = 0;
u32 header_pulses               __attribute__((section(".dtcm"))) = 0;
u16 current_bit                 __attribute__((section(".dtcm"))) = 0x100;
u32 current_bytes_this_block    __attribute__((section(".dtcm"))) = 0;
u8  handle_last_bits            __attribute__((section(".dtcm"))) = 0;
u8  custom_pulse_idx            __attribute__((section(".dtcm"))) = 0;
u16 loop_counter                __attribute__((section(".dtcm"))) = 0;
u16 loop_block                  __attribute__((section(".dtcm"))) = 0;
u32 last_edge                   __attribute__((section(".dtcm"))) = 0;

u32 next_edge1                  __attribute__((section(".dtcm"))) = 0;
u32 next_edge2                  __attribute__((section(".dtcm"))) = 0;

u8 give_up_counter = 0;
char *loader_type = "STANDARD";
extern patchFunc *PatchLookup;
u8 tape_sample_standard(void);

inline byte OpZ80(word A)  {return *(MemoryMap[(A)>>13] + ((A)&0x1FFF));}

TapePositionTable_t TapePositionTable[255];
extern char strcasestr (const char *big, const char *little);

u8 tape_find_positions(void)
{
    memset(TapePositionTable, 0x00, sizeof(TapePositionTable));

    u8 pos_idx = 0;

    // Always have a Start of Tape (Rewind)
    strcpy(TapePositionTable[pos_idx].description, "START OF TAPE");
    TapePositionTable[pos_idx].block_id = 0;
    pos_idx++;

    for (u16 i=0; i < num_blocks_available; i++)
    {
        if ((strlen(TapeBlocks[i].description) > 2) && (strcasestr(TapeBlocks[i].description, "CREATED WITH") == 0))
        {
            strcpy(TapePositionTable[pos_idx].description, TapeBlocks[i].description);
            TapePositionTable[pos_idx].block_id = i;
            pos_idx++;
        }
        else if (strlen(TapeBlocks[i].block_filename) > 2)
        {
            u8 bad=0;
            for (u8 j=0; j<10; j++)
            {
                if (!isprint((int)TapeBlocks[i].block_filename[j])) bad=1;
            }
            if (!bad)
            {
                strcpy(TapePositionTable[pos_idx].description, TapeBlocks[i].block_filename);
                TapePositionTable[pos_idx].block_id = i;
                pos_idx++;
            }
        }
    }
    return pos_idx;
}


// -----------------------------------------------------------------------------
// Many of the loaders have a pre-edge detect delay that chews up a lot of
// emulation time. So we trap on this and eliminate the entire delay by
// "fast-forwarding" the CPU.TStates as if the entire delay went by in a flash.
// -----------------------------------------------------------------------------
// 05E7  LDA BYTE {+7}
// 05E8  +16
// 05E9  DEC A {+4}
// 05EA  JR,NZ {+12/7}
// 05EB  +/-XX
// 05EC  AND A {+4}
ITCM_CODE u8 tape_predelay_accel(void)
{
    // We trapped on the LD,A BYTE and we've read the byte already... so now we're on the DEC A
    u32 loop_TStates = (CPU.AF.B.h * 16) - 5;   // Each pass around the loop is 16 cycles... except the last pass is only 11 cycles (5 less)
    CPU.TStates += loop_TStates;                // Account for the cycles it would have taken
    CPU.AF.B.h = 0x00;                          // Skip the loop entirely
    CPU.PC.W += 3;                              // Jump to the AND A in the standard ROM loader

    return CPU.AF.B.h;
}

// -----------------------------------------------
// This traps out the tape loader main routine...
// -----------------------------------------------
void tape_patch(void)
{
    // Reset the patch table to all zeros
    memset(PatchLookup, 0x00, 256*1024);

    if (myConfig.tapeSpeed)
    {
        PatchLookup[0x05F3] = tape_sample_standard;
        PatchLookup[0x05E9] = tape_predelay_accel;  // LD A, BYTE (actually DEC A as the PC will have advanced)
        loader_type = "STANDARD";
    }
}

// -----------------------------------------------------------------------------------
// Based on .TAP or .TZX we parse out the loader blocks into our internal structure
// so we can "play back" the tape into the emulation who is mainly looking for edges
// to sort out the ones/zeroes bits. The .TZX also has some metadata we save off.
// -----------------------------------------------------------------------------------
void tape_parse_blocks(int tapeSize)
{
    u32 block_len   = 0;
    u16 gap_len     = 0;
    u8  block_flag  = 0;

    num_blocks_available = 0;
    current_block = 0;

    memset(TapeBlocks, 0x00, sizeof(TapeBlocks));

    // ---------------------------------------------------------------
    // All tape files start with a block of 750ms 'gap' silence...
    // ---------------------------------------------------------------
    TapeBlocks[num_blocks_available].id = BLOCK_ID_PAUSE_STOP;
    TapeBlocks[num_blocks_available].gap_delay_after = 750;
    num_blocks_available++;

    // -----------------------------------------------------------------------
    // TAP files are always 'standard load' - no other information in them...
    // -----------------------------------------------------------------------
    if (speccy_mode == MODE_TAP)
    {
        int idx = 0;
        while (idx < tapeSize)
        {
            block_len  = ROM_Memory[idx] | (ROM_Memory[idx+1] << 8);
            block_flag = ROM_Memory[idx+2];

            // Put the standard block of data into our list
            TapeBlocks[num_blocks_available].id              = BLOCK_ID_STANDARD;
            TapeBlocks[num_blocks_available].gap_delay_after = DEFAULT_TAPE_GAP_DELAY_MS;
            TapeBlocks[num_blocks_available].pilot_length    = DEFAULT_PILOT_LENGTH;
            TapeBlocks[num_blocks_available].pilot_pulses    = ((block_flag < 128) ? DEFAULT_HEADER_PULSE_TOGGLES : DEFAULT_DATA_PULSE_TOGGLES);
            TapeBlocks[num_blocks_available].sync1_width     = DEFAULT_SYNC_PULSE1_WIDTH;
            TapeBlocks[num_blocks_available].sync2_width     = DEFAULT_SYNC_PULSE2_WIDTH;
            TapeBlocks[num_blocks_available].data_one_width  = DEFAULT_DATA_ONE_PULSE_WIDTH;
            TapeBlocks[num_blocks_available].data_zero_width = DEFAULT_DATA_ZERO_PULSE_WIDTH;
            TapeBlocks[num_blocks_available].last_bits_used  = DEFAULT_LAST_USED_BITS;

            // Precompute the X2 values of the one/zero pulse width to speed up edge detection
            TapeBlocks[num_blocks_available].data_one_widthX2  = TapeBlocks[num_blocks_available].data_one_width << 1;
            TapeBlocks[num_blocks_available].data_zero_widthX2 = TapeBlocks[num_blocks_available].data_zero_width << 1;

            TapeBlocks[num_blocks_available].block_data_idx  = idx+2;
            TapeBlocks[num_blocks_available].block_data_len  = block_len;
            TapeBlocks[num_blocks_available].block_flag      = block_flag;

            if ((block_flag < 128) || (block_len == 19)) // Header
            {
                memcpy(TapeBlocks[num_blocks_available].block_filename, &ROM_Memory[idx+4], 10);
            }
            num_blocks_available++;

            idx += (block_len + 2); // The two bytes of meta-data length plus the data
        }
    }
    // -----------------------------------------------------------------
    // TZX files have more metadata to help us with the tape layout...
    // -----------------------------------------------------------------
    else if (speccy_mode == MODE_TZX)
    {
        u16 pilot_length, pilot_pulses, sync1, sync2, zero, one, last_bits;

        int idx = 10;   // Skip past TZX header

        while (idx < tapeSize)
        {
            u8  block_id  = ROM_Memory[idx++];

            // Every block has a Block ID so we store that here...
            TapeBlocks[num_blocks_available].id = block_id;

            switch (block_id)
            {
                case BLOCK_ID_STANDARD: // Standard Load
                    gap_len    = ROM_Memory[idx+0] | (ROM_Memory[idx+1] << 8);
                    block_len  = ROM_Memory[idx+2] | (ROM_Memory[idx+3] << 8);
                    block_flag = ROM_Memory[idx+4];

                    TapeBlocks[num_blocks_available].gap_delay_after = gap_len;
                    TapeBlocks[num_blocks_available].pilot_length    = DEFAULT_PILOT_LENGTH;
                    TapeBlocks[num_blocks_available].pilot_pulses    = (block_flag ? DEFAULT_DATA_PULSE_TOGGLES : DEFAULT_HEADER_PULSE_TOGGLES);
                    TapeBlocks[num_blocks_available].sync1_width     = DEFAULT_SYNC_PULSE1_WIDTH;
                    TapeBlocks[num_blocks_available].sync2_width     = DEFAULT_SYNC_PULSE2_WIDTH;
                    TapeBlocks[num_blocks_available].data_one_width  = DEFAULT_DATA_ONE_PULSE_WIDTH;
                    TapeBlocks[num_blocks_available].data_zero_width = DEFAULT_DATA_ZERO_PULSE_WIDTH;
                    TapeBlocks[num_blocks_available].last_bits_used  = DEFAULT_LAST_USED_BITS;
                    TapeBlocks[num_blocks_available].block_data_idx  = idx+4;
                    TapeBlocks[num_blocks_available].block_data_len  = block_len;
                    TapeBlocks[num_blocks_available].block_flag      = block_flag;
                    // Precompute the X2 values of the one/zero pulse width to speed up edge detection
                    TapeBlocks[num_blocks_available].data_one_widthX2  = TapeBlocks[num_blocks_available].data_one_width << 1;
                    TapeBlocks[num_blocks_available].data_zero_widthX2 = TapeBlocks[num_blocks_available].data_zero_width << 1;

                    if ((block_flag == 0x00) || (block_len == 19)) // Header
                    {
                        memcpy(TapeBlocks[num_blocks_available].block_filename, &ROM_Memory[idx+4+2], 10);
                    }

                    num_blocks_available++;
                    idx += (block_len + 4);
                    break;

                case BLOCK_ID_TURBO: // Turbo Speed Block
                    pilot_length = ROM_Memory[idx+0]  | (ROM_Memory[idx+1]  << 8);
                    sync1        = ROM_Memory[idx+2]  | (ROM_Memory[idx+3]  << 8);
                    sync2        = ROM_Memory[idx+4]  | (ROM_Memory[idx+5]  << 8);
                    zero         = ROM_Memory[idx+6]  | (ROM_Memory[idx+7]  << 8);
                    one          = ROM_Memory[idx+8]  | (ROM_Memory[idx+9]  << 8);
                    pilot_pulses = ROM_Memory[idx+10] | (ROM_Memory[idx+11] << 8);
                    last_bits    = ROM_Memory[idx+12];
                    gap_len      = ROM_Memory[idx+13] | (ROM_Memory[idx+14] << 8);
                    block_len    = ROM_Memory[idx+15] | (ROM_Memory[idx+16] << 8) | (ROM_Memory[idx+17] << 16);
                    block_flag   = ROM_Memory[idx+18];

                    TapeBlocks[num_blocks_available].gap_delay_after = gap_len;
                    TapeBlocks[num_blocks_available].pilot_length    = pilot_length;
                    TapeBlocks[num_blocks_available].pilot_pulses    = pilot_pulses;
                    TapeBlocks[num_blocks_available].sync1_width     = sync1;
                    TapeBlocks[num_blocks_available].sync2_width     = sync2;
                    TapeBlocks[num_blocks_available].data_one_width  = one;
                    TapeBlocks[num_blocks_available].data_zero_width = zero;
                    TapeBlocks[num_blocks_available].last_bits_used  = last_bits;
                    TapeBlocks[num_blocks_available].block_data_idx  = idx+18;
                    TapeBlocks[num_blocks_available].block_data_len  = block_len;
                    TapeBlocks[num_blocks_available].block_flag      = block_flag;
                    // Precompute the X2 values of the one/zero pulse width to speed up edge detection
                    TapeBlocks[num_blocks_available].data_one_widthX2  = TapeBlocks[num_blocks_available].data_one_width << 1;
                    TapeBlocks[num_blocks_available].data_zero_widthX2 = TapeBlocks[num_blocks_available].data_zero_width << 1;

                    if ((block_flag == 0x00) || (block_len == 19)) // Header
                    {
                        memcpy(TapeBlocks[num_blocks_available].block_filename, &ROM_Memory[idx+18+2], 10);
                    }

                    num_blocks_available++;
                    idx += (block_len + 18);
                    break;

                case BLOCK_ID_PURE_TONE:
                    pilot_length = ROM_Memory[idx+0]  | (ROM_Memory[idx+1]  << 8);
                    pilot_pulses = ROM_Memory[idx+2]  | (ROM_Memory[idx+3]  << 8);
                    TapeBlocks[num_blocks_available].pilot_length    = pilot_length;
                    TapeBlocks[num_blocks_available].pilot_pulses    = pilot_pulses;
                    num_blocks_available++;
                    idx += 4;
                    break;

                case BLOCK_ID_PULSE_SEQ:
                    pilot_pulses = ROM_Memory[idx++];
                    for (u16 i=0; i < pilot_pulses; i++)
                    {
                        pilot_length = ROM_Memory[idx+0]  | (ROM_Memory[idx+1]  << 8);
                        idx += 2;
                        TapeBlocks[num_blocks_available].custom_pulse_len[i] = pilot_length;
                    }
                    TapeBlocks[num_blocks_available].pilot_length    = 0;
                    TapeBlocks[num_blocks_available].pilot_pulses    = pilot_pulses;
                    num_blocks_available++;
                    break;

                case BLOCK_ID_PURE_DATA:
                    zero         = ROM_Memory[idx+0]  | (ROM_Memory[idx+1]  << 8);
                    one          = ROM_Memory[idx+2]  | (ROM_Memory[idx+3]  << 8);
                    last_bits    = ROM_Memory[idx+4];
                    gap_len      = ROM_Memory[idx+5] | (ROM_Memory[idx+6] << 8);
                    block_len    = ROM_Memory[idx+7] | (ROM_Memory[idx+8] << 8) | (ROM_Memory[idx+9] << 16);
                    block_flag   = ROM_Memory[idx+10];

                    TapeBlocks[num_blocks_available].gap_delay_after = gap_len;
                    TapeBlocks[num_blocks_available].data_one_width  = one;
                    TapeBlocks[num_blocks_available].data_zero_width = zero;
                    TapeBlocks[num_blocks_available].last_bits_used  = last_bits;
                    TapeBlocks[num_blocks_available].block_data_idx  = idx+10;
                    TapeBlocks[num_blocks_available].block_data_len  = block_len;
                    TapeBlocks[num_blocks_available].block_flag      = block_flag;
                    TapeBlocks[num_blocks_available].sync1_width = 0;
                    TapeBlocks[num_blocks_available].sync2_width = 0;
                    // Precompute the X2 values of the one/zero pulse width to speed up edge detection
                    TapeBlocks[num_blocks_available].data_one_widthX2  = TapeBlocks[num_blocks_available].data_one_width << 1;
                    TapeBlocks[num_blocks_available].data_zero_widthX2 = TapeBlocks[num_blocks_available].data_zero_width << 1;
                    num_blocks_available++;
                    idx += (block_len + 10);
                    break;

                case BLOCK_ID_PAUSE_STOP:     // Pause / Stop the Tape
                    TapeBlocks[num_blocks_available].gap_delay_after = ROM_Memory[idx] | (ROM_Memory[idx+1] << 8);
                    num_blocks_available++;
                    idx += 2;
                    break;

                case BLOCK_ID_STOP_IF_48K: // Stop the Tape only if 48K mode
                    TapeBlocks[num_blocks_available].gap_delay_after = 0;
                    num_blocks_available++;
                    idx += 4;
                    break;

                case BLOCK_ID_GROUP_START: // Group Start
                    block_len = ROM_Memory[idx + 0];
                    memcpy(TapeBlocks[num_blocks_available].description, &ROM_Memory[idx+1], (block_len < 26 ? block_len:26));
                    num_blocks_available++;
                    idx += (block_len + 1);
                    break;

                case BLOCK_ID_GROUP_END: // Group End - skip this for now. Group start is more useful.
                    idx += 0;
                    break;

                case BLOCK_ID_LOOP_START: // Loop Start
                    TapeBlocks[num_blocks_available].loop_counter = (ROM_Memory[idx + 0] << 0) | (ROM_Memory[idx + 1] << 8);
                    num_blocks_available++;
                    idx += 2;
                    break;

                case BLOCK_ID_LOOP_END: // Loop End
                    num_blocks_available++;
                    idx += 0;
                    break;

                case BLOCK_ID_TEXT: // Text Description
                    block_len = ROM_Memory[idx + 0];
                    memcpy(TapeBlocks[num_blocks_available].description, &ROM_Memory[idx+1], (block_len < 26 ? block_len:26));
                    num_blocks_available++;
                    idx += (block_len + 1);
                    break;

                case 0x2B: // Set signal level
                    idx += 5;
                    break;

                case 0x31: // Message Block
                    block_len = ROM_Memory[idx + 1];
                    idx += (block_len + 2);
                    break;

                case 0x32: // Archive Info
                    block_len  = (ROM_Memory[idx + 0] << 0) | (ROM_Memory[idx + 1] << 8);
                    idx += (block_len + 2);
                    break;

                case 0x33: // Machine Info
                    block_len = ROM_Memory[idx + 0];
                    idx += (block_len + 1);
                    break;

                case 0x35: // Custom Info Block
                    block_len  = (ROM_Memory[idx + 0x10] << 0) | (ROM_Memory[idx + 0x11] << 8) | (ROM_Memory[idx + 0x12] << 16) | (ROM_Memory[idx + 0x13] << 24);
                    idx += (block_len + 20);
                    break;

                case 0x5A: // Glue Block
                    idx += 9;
                    break;
#if 0
                default:
                    printf("Unknown ID = %02Xh\n", id);
                    idx = size;
                    break;
#endif
            }
        }

        // -----------------------------------------------------------------------------------------
        // Sometimes the final block will have a long gap - but it's not needed as the tape is done
        // playing at that point... so we cut this short which helps the emulator stop the tape.
        // -----------------------------------------------------------------------------------------
        TapeBlocks[num_blocks_available-1].gap_delay_after = 0;
    }
}

u8 tape_is_playing(void)
{
    return (tape_state != TAPE_STOP);
}

void tape_reset(void)
{
    tape_state = TAPE_STOP;
    current_block_data_idx = 0;
    current_block = 0;
    tape_bytes_processed = 0;
    custom_pulse_idx = 0;
    give_up_counter = 0;
    last_edge = 0;
    next_edge1 = next_edge2 = 0;
}

void tape_stop(void)
{
    tape_state = TAPE_STOP;
    DisplayStatusLine(false);
}

void tape_play(void)
{
    tape_state = TAPE_START;
    DisplayStatusLine(false);
}

void tape_position(u8 newPos)
{
    current_block = TapePositionTable[newPos].block_id;
}

// Called every frame
void tape_frame(void)
{
    // Not sure if we need this yet... called after every 1 frame of timing in the main loop
}

// ----------------------------------------------------------------
// This is called when the Spectrum ULA reads from port 0xFE
// It will sift and sort the current tape block data and return
// the appropriate bit to the caller who will put it into the port
// read return byte.
// ----------------------------------------------------------------
ITCM_CODE u8 tape_pulse(void)
{
    u32 pilot_pulse = 0;
#if 0
    debug[0]  = OpZ80(CPU.PC.W-12);
    debug[1]  = OpZ80(CPU.PC.W-11);
    debug[2]  = OpZ80(CPU.PC.W-10);
    debug[3]  = OpZ80(CPU.PC.W-9);
    debug[4]  = OpZ80(CPU.PC.W-8);
    debug[5]  = OpZ80(CPU.PC.W-7);
    debug[6]  = OpZ80(CPU.PC.W-6);
    debug[7]  = OpZ80(CPU.PC.W-5);
    debug[8]  = OpZ80(CPU.PC.W-4);
    debug[9]  = OpZ80(CPU.PC.W-3);
    debug[10] = OpZ80(CPU.PC.W-2);
    debug[11] = OpZ80(CPU.PC.W-1);
    debug[12] = OpZ80(CPU.PC.W+0);
    debug[13] = OpZ80(CPU.PC.W+1);
    debug[14] = OpZ80(CPU.PC.W+2);
    debug[15] = OpZ80(CPU.PC.W+3);
#endif

    // Don't return from the state machine until we have a bit value to return
    while (1)
    {
        switch (tape_state)
        {
            case TAPE_STOP:
                return 0x00;
                break;

            case TAPE_START:
                last_edge = 0;
                next_edge1 = next_edge2 = 0;
                tape_state = TAPE_NEXT_BLOCK;
                break;

            case TAPE_NEXT_BLOCK:
                // -------------------------------------------------------------
                // If we've exhausted all of the tape blocks, we stop the
                // tape and set the current block back to the start of the tape.
                // -------------------------------------------------------------
                if (current_block >= num_blocks_available)
                {
                    tape_state = TAPE_STOP; // Stop the playback
                    current_block = 0;      // Wrap back around
                    break;                  // And move DIRECTORYly to the STOP state
                }

                // ----------------------------------------------------------------
                // When we start a new block, we start the CPU timer at the top
                // And we set the block data index back to the start of the block.
                // ----------------------------------------------------------------
                last_edge = CPU.TStates;
                give_up_counter = 0;
                current_block_data_idx = TapeBlocks[current_block].block_data_idx;

                // ------------------------------------------------
                // Now... let's see what magic this block holds...
                // ------------------------------------------------
                switch (TapeBlocks[current_block].id)
                {
                    case BLOCK_ID_STANDARD:  // Standard Play Block
                    case BLOCK_ID_TURBO:     // Turbo Load Block
                    case BLOCK_ID_PURE_TONE: // Pilot Tone Only Block
                        tape_state = BLOCK_PILOT_TONE;
                        break;

                    case BLOCK_ID_PULSE_SEQ:    // Custom pulse sequence
                        custom_pulse_idx = 0;
                        tape_state = CUSTOM_PULSE_SEQ;
                        break;

                    case BLOCK_ID_PURE_DATA: // Pure Data Block
                        tape_state = SYNC_PULSE;    // We've set the sync1/sync2 both to zero so this will immediately go into data send
                        break;

                    case BLOCK_ID_PAUSE_STOP: // Delay/Pause/Stop the Tape
                        tape_state = TAPE_DELAY_AFTER;
                        break;

                    case BLOCK_ID_STOP_IF_48K: // Stop if 48K
                        if (!zx_128k_mode) // If we are 48K Spectrum
                        {
                            tape_state = TAPE_STOP;
                        }
                        else // For 128K we can move to the next block
                        {
                            current_block++;
                            tape_state = TAPE_NEXT_BLOCK;
                        }
                        break;

                    case BLOCK_ID_LOOP_START:
                        loop_counter = TapeBlocks[current_block].loop_counter;
                        current_block++;
                        loop_block = current_block;
                        tape_state = TAPE_NEXT_BLOCK;
                        break;

                    case BLOCK_ID_LOOP_END:
                        if (--loop_counter) // If not done with loop, go back to the block after the loop started
                        {
                            current_block = loop_block;
                        }
                        else // Done with loop... move along
                        {
                            current_block++;
                        }
                        tape_state = TAPE_NEXT_BLOCK;
                        break;

                    case BLOCK_ID_GROUP_START:
                    case BLOCK_ID_TEXT:
                        current_block++;
                        break;

                    default: //TODO: add more block IDs for TZX and trap ones we don't handle...
                        tape_state = TAPE_STOP;
                        break;
                }
                break;

            case BLOCK_PILOT_TONE:
                pilot_pulse = ((CPU.TStates-last_edge) / TapeBlocks[current_block].pilot_length); // How many pulses are we into this thing...

                // Always end the pilot tone on a high bit sent to simplify the logic on the SYNC pulse below
                if ((pilot_pulse < TapeBlocks[current_block].pilot_pulses) || (pilot_pulse & 1)) // Are we still in the pilot tone send?
                {
                    if (pilot_pulse & 1) return 0x40; else return 0x00;  // Send the pulse bit
                }
                else  // We're done with the pilot tone... get ready to send the SYNC PULSE
                {
                    last_edge = CPU.TStates;
                    if (TapeBlocks[current_block].id == BLOCK_ID_PURE_TONE) // If pure tone... we're done.
                    {
                        current_block++;
                        tape_state = TAPE_NEXT_BLOCK;
                    }
                    else // Otherwise move on to the Sync Pulse
                    {
                        tape_state = SYNC_PULSE;
                    }
                }
                break;

            case CUSTOM_PULSE_SEQ:
                if ((CPU.TStates-last_edge) < TapeBlocks[current_block].custom_pulse_len[custom_pulse_idx])
                {
                    if (custom_pulse_idx & 1) return 0x40; else return 0x00;  // Send the pulse bit
                }
                else  // Check if we're done with the custom pulse sequence...
                {
                    last_edge = CPU.TStates;
                    if (++custom_pulse_idx >= TapeBlocks[current_block].pilot_pulses)
                    {
                        current_block++;
                        tape_state = TAPE_NEXT_BLOCK;
                    }
                }
                break;

            case SYNC_PULSE:
                if ((CPU.TStates-last_edge) < TapeBlocks[current_block].sync1_width) return 0x00;
                else if ((CPU.TStates-last_edge) < (TapeBlocks[current_block].sync1_width + TapeBlocks[current_block].sync2_width)) return 0x40;
                else
                {
                    last_edge = CPU.TStates;
                    current_bit = 0x100;    // So when we shift it down we'll be looking at the high (7th) bit of data
                    current_bytes_this_block = 0;
                    tape_state = SEND_DATA_BYTES;
                    handle_last_bits = 0x00;
                }
                break;

            case SEND_DATA_BYTES:
                current_bit = current_bit>>1;

                // --------------------------------------------------------------------------
                // If slow bit reads are happening (arbitrarily above 10000 CPU ticks), we
                // increment a "give up" counter... if this reaches critical mass, we simply
                // stop the tape as it no longer looks like we are trying to load anything.
                // --------------------------------------------------------------------------
                if ((CPU.TStates-last_edge) > 10000) // Slow bit reads happening?
                {
                    if (myConfig.autoStop)
                    {
                        if (++give_up_counter > 5)
                        {
                            tape_stop();
                            return 0x00;
                        }
                    }
                }

                if (current_bit == handle_last_bits) // Are we done sending this byte?
                {
                    // ---------------------------------------------------------------------------------
                    // See if it's been a "long" time between bytes.. if so, we probably aren't in
                    // byte load / edge detection handling anymore... we can consider STOPing the tape.
                    // ---------------------------------------------------------------------------------
                    tape_bytes_processed++;
                    current_block_data_idx++;
                    if (++current_bytes_this_block >= TapeBlocks[current_block].block_data_len)
                    {
                        last_edge = CPU.TStates;
                        tape_state = TAPE_DELAY_AFTER; // We're done with this block... delay after
                        return 0x00; // But make at least one transition back to 'low'
                    }
                    else  // We've got another byte to process...
                    {
                        current_bit = 0x100;

                        // Check if we are on the very last byte... some Turbo Loaders don't send all the bits
                        if ((current_bytes_this_block+1) >= TapeBlocks[current_block].block_data_len)
                        {
                             // ----------------------------------------------------------------------------------
                             // If this loader does not pulse out all the final bits, we need to set up a
                             // mask other than 0x00 so that we know when to terminate the last byte transmission.
                             // ----------------------------------------------------------------------------------
                             handle_last_bits = 0x80 >> TapeBlocks[current_block].last_bits_used;
                        }
                        tape_state = SEND_DATA_BYTES;
                    }
                }
                else
                {
                    // We need to send one bit of data...
                    last_edge = CPU.TStates;
                    tape_state = SEND_DATA_BITS;
                    if (ROM_Memory[current_block_data_idx] & current_bit)
                    {
                        next_edge1 = last_edge + TapeBlocks[current_block].data_one_width;
                        next_edge2 = last_edge + TapeBlocks[current_block].data_one_widthX2;
                    }
                    else
                    {
                        next_edge1 = last_edge + TapeBlocks[current_block].data_zero_width;
                        next_edge2 = last_edge + TapeBlocks[current_block].data_zero_widthX2;
                    }
                }
                break;

            case SEND_DATA_BITS:
                if      (CPU.TStates <= next_edge1) return 0x00;
                else if (CPU.TStates <= next_edge2) return 0x40;
                else
                {
                    tape_state = SEND_DATA_BYTES;
                }
                break;

            case TAPE_DELAY_AFTER: // Normally ~1 second but can be different for custom tapes
                if ((CPU.TStates-last_edge) <= (TapeBlocks[current_block].gap_delay_after * 3500)) return 0x00;
                else
                {
                    // A delay of zero is not special unless we are the BLOCK_ID_PAUSE_STOP block type...
                    if ((TapeBlocks[current_block].id == BLOCK_ID_PAUSE_STOP) && (TapeBlocks[current_block].gap_delay_after == 0))
                    {
                        current_block++;
                        tape_state = TAPE_STOP;     // To Pause/Stop the tape
                    }
                    else
                    {
                        current_block++;
                        tape_state = TAPE_NEXT_BLOCK;    // And back to play if we have more tape

                        tape_search_for_loader();
                    }
                }
                break;
        }
    }
}


// ----------------------------------------------------------------------------------------
// Used in our edge-detection accelerated loaders below. This routine will invert and shift
// down the bits so as to provide a very fast interface to edge detection when clocking in
// data Zeros or Ones which is quite common when loading tapes.
// ----------------------------------------------------------------------------------------
u8 inline __attribute__((always_inline)) tape_pulse_fast(void)
{
    if      (CPU.TStates <= next_edge1) return ~0x00; // Inverted and shifted down
    else if (CPU.TStates <= next_edge2) return ~0x20; // So loader does less work
    else
    {
         // Experimentally, this happens about 16x less frequently than the bit returns above (makes sense as there are 2x edges per bit in the byte)
         tape_state = SEND_DATA_BYTES;
         return ~tape_pulse() >> 1; // Invert and shift down
    }
}


// ---------------------------------------------------------------------------------------------------------------
// And these are the loaders used by most of the ZX Spectrum tapes... we accelerate the edge detection loops
// as much as possible while still preserving the original timing and keeping the loader moving. This generally
// results in a 3-4x speedup in the loading process. Some emulators optimize most of this away but we're not
// ready/willing to do that. At least not for now... Loading screens are part of the charm of the ZX Speccy!
// ---------------------------------------------------------------------------------------------------------------

// STANDARD Loader:
// 05ED {1} LD-SAMPLE INC  B        [+4]    Count each pass
// 05EE {1}           RET  Z        [+11/5] Return carry reset & zero set if 'time-up'.
// 05EF {2}           LD   A,+7F    [+7]    Read from port +7FFE
// 05F1 {2}           IN   A,(+FE)  [+11]   i.e. BREAK and EAR    <== This is where the PC Trap is
// 05F3 {1}           RRA           [+4]    Shift the byte
// 05F4 {1}           RET  NC       [+11/5] Return carry reset & zero reset if BREAK was pressed
// 05F5 {1}           XOR  C        [+4]    Now test the byte against the 'last edge-type'
// 05F6 {2}           AND  +20      [+7]    Mask off just the bit we care about (normally 0x40 but it's been shifted down)
// 05F8 {3}           JR   Z,05ED   [+12/5] Jump back to LD-SAMPLE unless it has changed

//PC will be 0x05F3 when we get here from the standard BIOS but were are being location agnostic
//so that we can use this same routine when the standard loader is used in other memory locations.
ITCM_CODE u8 tape_sample_standard(void)
{
    if (!tape_state) tape_state = TAPE_START; // If we aren't playing the tape, may as well do so as we're trying to find an edge

    int B = 255-CPU.BC.B.h;     // Very slight speedups to take these into local stack vars
    const u8 C = CPU.BC.B.l;    // Very slight speedups to take these into local stack vars
ld_sample:
    u8 A;
    if (tape_state & 0x80)      // One or Zero... do it FAST!
    {
        A = (tape_pulse_fast()) ^ C;    // This version is already shifted down and inverted...
    }
    else
    {
        A = (~tape_pulse() >> 1) ^ C;   // Normal version must be inverted and shifted down...
    }

    if (A & 0x20)                       // Edge detected. We can exit the loop.
    {
        CPU.TStates += 25;              // 25 cycles from the IN to past the JR Z,05ED
        CPU.AF.B.h = A & 0x20;          // This is what the result would have been...
        CPU.AF.B.l = H_FLAG;            // Set the appropriate flags for the AND +20
        CPU.BC.B.h = (255-B);           // Let the caller know how close we got to the timeout
        CPU.PC.W += 7;                  // Jump past the JR Z,05ED check
    }
    else                                // Edge not detected - we will do another pass or timeout
    {
        // -----------------------------------------------------------------------
        // For the standard loader if we're at least 4 counts away from the
        // timeout, we can accelerate the timing a bit and still not miss an edge.
        // -----------------------------------------------------------------------
        if (B & 0xFC)
        {
            CPU.TStates += 3*59;        // Three times the normal loop time - faster edge detection
            B-=3;                       // Reduce counter by 3
            goto ld_sample;             // Take another sample.
        }
        CPU.TStates += 59;              // It takes 59 total cycles when we take another pass around the loop
        if (--B) goto ld_sample;        // If no time-out... take another sample.

        // -----------------------------------------------------------------
        // We have timed out when B wraps back to zero... handle this here.
        // -----------------------------------------------------------------
        CPU.TStates -= 27;              // Give back the time we accounted for with the INCB/RETZ/LDA/INA instructions
        CPU.BC.B.h = 0xFF;              // Set this up so that we WILL timeout when returning
        CPU.AF.W = 0x0000;              // Clear flags (mainly Carry Reset) and A register will be clear
        CPU.PC.W -= 6;                  // And return to the INC B counter and allow the timeout
    }

    return CPU.AF.B.h;
}

// SPEEDLOCK Loader:
//
// LD-SAMPLE  INC  B        [+4]    Count each pass
//            RET  Z        [+11/5] Return carry reset & zero set if 'time-up'.
//            LD   A,+7F    [+7]    Read from port +7FFE   <== This is where the PC Trap is
//            IN   A,(+FE)  [+11]   i.e. BREAK and EAR
//            RRA           [+4]    Shift the byte
//            XOR  C        [+4]    Now test the byte against the 'last edge-type'
//            AND  +20      [+7]    Mask off just the bit we care about (normally 0x40 but it's been shifted down)
//            JR   Z,05ED   [+12/5] Jump back to LD-SAMPLE unless it has changed

ITCM_CODE u8 tape_sample_speedlock(void)
{
    if (!tape_state) tape_state = TAPE_START;

    // The CyclesED[] table will consume the 18 cycles that the LDA, INA would have taken here...
    int B = 255-CPU.BC.B.h;
    const u8 C = CPU.BC.B.l;
ld_sample:
    u8 A;
    if (tape_state & 0x80) // One or Zero... do it FAST!
    {
        // This version is already shifted down and inverted...
        A = (tape_pulse_fast()) ^ C;
    }
    else
    {
        A = (~tape_pulse() >> 1) ^ C;
    }

    if (A & 0x20)                       // Edge detected. We can exit the loop.
    {
        CPU.TStates += 20;              // 20 cycles from the IN to past the JR Z,05ED
        CPU.AF.B.h = A & 0x20;          // This is what the result would have been...
        CPU.AF.B.l = H_FLAG;            // Set the appropriate flags for the AND +20
        CPU.BC.B.h = (255-B);           // Let the caller know how close we got to the timeout
        CPU.PC.W += 6;                  // Jump past the JR Z,05ED check
    }
    else                                // Edge not detected - we will do another pass or timeout
    {
        CPU.TStates += 54;              // It takes 54 total cycles when we take another pass around the loop
        if (--B) goto ld_sample;        // If no time-out... take another sample.

        // -----------------------------------------------------------------
        // We have timed out when B wraps back to zero... handle this here.
        // -----------------------------------------------------------------
        CPU.TStates -= 27;              // Give back the time we accounted for with the INCB/RETZ/LDA/INA instructions
        CPU.BC.B.h = 0xFF;              // Set this up so that we WILL timeout when returning
        CPU.AF.W = 0x0000;              // Clear flags (mainly Carry Reset) and A register will be clear
        CPU.PC.W -= 6;                  // And return to the INC B counter and allow the timeout
    }

    return CPU.AF.B.h;
}

// ALKATRAZ Loader:
//
// LD-SAMP{1} INC B               [+4]    Count each pass
//        {2} JR NZ,LD-SAMPLE2    [+12/7] Jump to LD-SAMPLE2
//        {3} JP <somewhere else> [+10]   Trap the timeout - jump out
// LD-SAMP2:
//        {2} IN A,(FE)           [+11]   Read from port +xxFE   <== This is where the PC Trap is
//        {1} RRA                 [+4]    Shift the byte
//        {1} RET Z               [+11/5] Return carry reset & zero set if 'time-up'.
//        {1} XOR C               [+4]    Now test the byte against the 'last edge-type'
//        {2} AND 20              [+7]    Mask off just the bit we care about (normally 0x40 but it's been shifted down)
//        {2} JR Z,LD-SAMP        [+12/5] Jump back to LD-SAMP unless it has changed
ITCM_CODE u8 tape_sample_alkatraz(void)
{
    if (!tape_state) tape_state = TAPE_START;

    // The CyclesED[] table will consume the 11 cycles that the IN A, (+FE) would have taken here...
    int B = 255-CPU.BC.B.h;
    const u8 C = CPU.BC.B.l;
ld_sample:
    u8 A;
    if (tape_state & 0x80) // One or Zero... do it FAST!
    {
        // This version is already shifted down and inverted...
        A = (tape_pulse_fast()) ^ C;
    }
    else
    {
        A = (~tape_pulse() >> 1) ^ C;
    }

    if (A & 0x20)                       // Edge detected. We can exit the loop.
    {
        CPU.TStates += 25;              // 25 cycles from the IN to past the JR Z,LD-SAMP
        CPU.AF.B.h = A & 0x20;          // This is what the result would have been...
        CPU.AF.B.l = H_FLAG;            // Set the appropriate flags for the AND +20
        CPU.BC.B.h = (255-B);           // Let the caller know how close we got to the timeout
        CPU.PC.W += 7;                  // Jump past the JR Z,LD-SAMP check
    }
    else                                // Edge not detected - we will do another pass or timeout
    {
        CPU.TStates += 59;              // It takes 59 total cycles when we take another pass around the loop to the IN A,(FE)
        if (--B) goto ld_sample;        // If no time-out... take another sample.

        // -----------------------------------------------------------------
        // We have timed out when B wraps back to zero... handle this here.
        // -----------------------------------------------------------------
        CPU.TStates -= 27;              // Give back the time we accounted for with the INCB/RETZ/LDA/INA instructions
        CPU.BC.B.h = 0xFF;              // Set this up so that we WILL timeout when returning
        CPU.AF.W = 0x0000;              // Clear flags (mainly Carry Reset) and A register will be clear
        CPU.PC.W -= 8;                  // And return to the INC B counter and allow the timeout
    }

    return CPU.AF.B.h;
}


// MICROSPHERE Loader:
//
// LD-SAMPLE  INC  B        [+4]    Count each pass
//            RET  Z        [+11/5] Return carry reset & zero set if 'time-up'.
//        {2} LD   A,+7F    [+7]    Read from port +7FFE   <== This is where the PC Trap is
//        {2} IN   A,(+FE)  [+11]   i.e. BREAK and EAR
//        {1} RRA           [+4]    Shift the byte
//        {1} AND  A        [+4]    Essentially same as a NOP in this context
//        {1} XOR  C        [+4]    Now test the byte against the 'last edge-type'
//        {2} AND  +20      [+7]    Mask off just the bit we care about (normally 0x40 but it's been shifted down)
//        {2} JR   Z,05ED   [+12/5] Jump back to LD-SAMPLE unless it has changed
//
// BLEEPLOAD Loader:
//
// LD-SAMPLE  INC  B        [+4]    Count each pass
//        {1} RET  Z        [+11/5] Return carry reset & zero set if 'time-up'.
//        {2} LD   A,+7F    [+7]    Read from port +7FFE   <== This is where the PC Trap is
//        {2} IN   A,(+FE)  [+11]   i.e. BREAK and EAR
//        {1} RRA           [+4]    Shift the byte
//        {1} NOP           [+4]    NOP in place of the usual 'RET NC' check for keys pressed
//        {1} XOR  C        [+4]    Now test the byte against the 'last edge-type'
//        {2} AND  +20      [+7]    Mask off just the bit we care about (normally 0x40 but it's been shifted down)
//        {2} JR   Z,05ED   [+12/5] Jump back to LD-SAMPLE unless it has changed

ITCM_CODE u8 tape_sample_microsphere_bleepload(void)
{
    if (!tape_state) tape_state = TAPE_START;

    // The CyclesED[] table will consume the 18 cycles that the LDA, INA would have taken here...
    int B = 255-CPU.BC.B.h;
    const u8 C = CPU.BC.B.l;
ld_sample:
    u8 A;
    if (tape_state & 0x80) // One or Zero... do it FAST!
    {
        // This version is already shifted down and inverted...
        A = (tape_pulse_fast()) ^ C;
    }
    else
    {
        A = (~tape_pulse() >> 1) ^ C;
    }

    if (A & 0x20)                       // Edge detected. We can exit the loop.
    {
        CPU.TStates += 24;              // 24 cycles from the IN to past the JR Z,LD-SAMP
        CPU.AF.B.h = A & 0x20;          // This is what the result would have been...
        CPU.AF.B.l = H_FLAG;            // Set the appropriate flags for the AND +20
        CPU.BC.B.h = (255-B);           // Let the caller know how close we got to the timeout
        CPU.PC.W += 7;                  // Jump past the JR Z,LD-SAMP check
    }
    else                                // Edge not detected - we will do another pass or timeout
    {
        CPU.TStates += 58;              // It takes 58 total cycles when we take another pass around the loop
        if (--B) goto ld_sample;        // If no time-out... take another sample.

        // -----------------------------------------------------------------
        // We have timed out when B wraps back to zero... handle this here.
        // -----------------------------------------------------------------
        CPU.TStates -= 27;              // Give back the time we accounted for with the INCB/RETZ/LDA/INA instructions
        CPU.BC.B.h = 0xFF;              // Set this up so that we WILL timeout when returning
        CPU.AF.W = 0x0000;              // Clear flags (mainly Carry Reset) and A register will be clear
        CPU.PC.W -= 6;                  // And return to the INC B counter and allow the timeout
    }

    return CPU.AF.B.h;
}


// ---------------------------------------------------------------------------------
// After every new block is settled into memory, we look to see if we can find one
// of the popular loaders. We might be able to patch the loader for faster access.
// ---------------------------------------------------------------------------------
ITCM_CODE void tape_search_for_loader(void)
{
    if (myConfig.tapeSpeed == 0) return;

    for (int addr = 0x4000; addr < 0xFFFE; addr++)
    {
        // -----------------------------------------------------------------------
        // All of our loaders have the ubiquitous IN A,+FE to read the tape input
        // -----------------------------------------------------------------------
        if ((OpZ80(addr+0) == 0xDB) && (OpZ80(addr+1) == 0xFE))
        {
            // A crap-ton of loaders are 3E, 7F, DB, FE
            if ((OpZ80(addr-2) == 0x3E) && (OpZ80(addr-1) == 0x7F))
            {
                // Standard Loader just moved in memory
                if (OpZ80(addr+2) == 0x1F)
                  if (OpZ80(addr+3) == 0xD0)
                    if (OpZ80(addr+4) == 0xA9)
                      if (OpZ80(addr+5) == 0xE6)
                        if (OpZ80(addr+6) == 0x20)
                          if (OpZ80(addr+7) == 0x28)
                          {
                              loader_type = "STANDARD";
                              PatchLookup[addr+2]  = tape_sample_standard;
                              if (OpZ80(addr-10) == 0x3E) PatchLookup[addr-8] = tape_predelay_accel;
                          }

                // Speedlock Loader (omits the check for SPACE=break)
                if (OpZ80(addr+2) == 0x1F)
                  if (OpZ80(addr+3) == 0xA9)
                    if (OpZ80(addr+4) == 0xE6)
                      if (OpZ80(addr+5) == 0x20)
                        if (OpZ80(addr+6) == 0x28)
                        {
                            loader_type = "SPEEDLOCK";
                            PatchLookup[addr+2] = tape_sample_speedlock;
                            if (OpZ80(addr-10) == 0x3E) PatchLookup[addr-8] = tape_predelay_accel;
                        }

                // Owens Loader
                if (OpZ80(addr+2) == 0x1F)
                  if (OpZ80(addr+3) == 0xC8) // RET Z
                    if (OpZ80(addr+4) == 0xA9)
                      if (OpZ80(addr+5) == 0xE6)
                        if (OpZ80(addr+6) == 0x20)
                          if (OpZ80(addr+7) == 0x28)
                          {
                              loader_type = "OWENS";
                              PatchLookup[addr+2] = tape_sample_standard; // Since this has the same cycle count - we can use the standard loader
                              if (OpZ80(addr-10) == 0x3E) PatchLookup[addr-8] = tape_predelay_accel;
                          }

                // Dinaload Loader
                if (OpZ80(addr+2) == 0x1F)
                  if (OpZ80(addr+3) == 0xD0) // RET NC
                    if (OpZ80(addr+4) == 0xA9)
                      if (OpZ80(addr+5) == 0xE6)
                        if (OpZ80(addr+6) == 0x20)
                          if (OpZ80(addr+7) == 0x28)
                          {
                              loader_type = "DINALOAD";
                              PatchLookup[addr+2] = tape_sample_standard; // Since this has the same cycle count - we can use the standard loader
                              if (OpZ80(addr-10) == 0x3E) PatchLookup[addr-8] = tape_predelay_accel;
                          }

                // Microsphere Loader
                if (OpZ80(addr+2) == 0x1F)
                  if (OpZ80(addr+3) == 0xA7) // AND A (NOP Equivilent)
                    if (OpZ80(addr+4) == 0xA9)
                      if (OpZ80(addr+5) == 0xE6)
                        if (OpZ80(addr+6) == 0x20)
                          if (OpZ80(addr+7) == 0x28)
                          {
                              loader_type = "MICROSPHERE";
                              PatchLookup[addr+2] = tape_sample_microsphere_bleepload;
                              if (OpZ80(addr-10) == 0x3E) PatchLookup[addr-8] = tape_predelay_accel;
                          }

                // Bleepload Loader
                if (OpZ80(addr+2) == 0x1F)
                  if (OpZ80(addr+3) == 0x00)  // NOP
                    if (OpZ80(addr+4) == 0xA9)
                      if (OpZ80(addr+5) == 0xE6)
                        if (OpZ80(addr+6) == 0x20)
                          if (OpZ80(addr+7) == 0x28)
                          {
                              loader_type = "BLEEPLOAD";
                              PatchLookup[addr+2] = tape_sample_microsphere_bleepload;
                              if (OpZ80(addr-10) == 0x3E) PatchLookup[addr-8] = tape_predelay_accel;
                          }
             }
             else // Now the odd-balls
             {
                // Alkatraz
                if (OpZ80(addr+0) == 0xDB) // INA
                  if (OpZ80(addr+1) == 0xFE) // +FE
                    if (OpZ80(addr+2) == 0x1F) // RRA
                      if (OpZ80(addr+3) == 0xC8) // RETZ
                        if (OpZ80(addr+4) == 0xA9) // XORC
                          if (OpZ80(addr+5) == 0xE6) // AND
                            if (OpZ80(addr+6) == 0x20) // +20
                              if (OpZ80(addr+7) == 0x28) // JRZ
                              {
                                  loader_type = "ALKATRAZ";
                                  PatchLookup[addr+2] = tape_sample_alkatraz;
                                  if (OpZ80(addr-12) == 0x3E) PatchLookup[addr-10] = tape_predelay_accel;
                              }
             }
        }
    }
}

// End of file

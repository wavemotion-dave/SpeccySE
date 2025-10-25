/******************************************************************************
*  SpeccySE Z80 CPU
*
* Note: Most of this file is from the ColEm emulator core by Marat Fayzullin
*       but heavily modified for specific NDS use. If you want to use this
*       code, you are advised to seek out the much more portable ColEm core
*       and contact Marat.
*
******************************************************************************/

/** Z80: portable Z80 emulator *******************************/
/**                                                         **/
/**                           Z80.c                         **/
/**                                                         **/
/** This file contains implementation for Z80 CPU. Don't    **/
/** forget to provide RdZ80(), WrZ80(), InZ80(), OutZ80(),  **/
/** LoopZ80(), and PatchZ80() functions to accomodate the   **/
/** emulated machine's architecture.                        **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994-2021                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/
#include <nds.h>
#include "Z80.h"
#include "Tables.h"
#include <stdio.h>
#include "../../../printf.h"
#include "../../../SpeccyUtils.h"

extern Z80 CPU;

extern u32 debug[];
extern u32 DX,DY;
extern u8 zx_ScreenRendering, zx_128k_mode, portFD;
extern void EI_Enable(void);
void ExecOneInstruction(void);
void ResetZ80(Z80 *R);
void ExecZ80_Speccy_a(u32 RunToCycles);
void EI_Enable_a(void);

#define T_INC(X)
#define PhantomRdZ80(A)
#define J_ADJ   CPU.TStates -= 5;
#define R_ADJ   CPU.TStates += 6;
#define C_ADJ   CPU.TStates += 7;

#define INLINE static inline

/** System-Dependent Stuff ***********************************/
/** This is system-dependent code put here to speed things  **/
/** up. It has to stay inlined to be fast.                  **/
/*************************************************************/
extern u8 *MemoryMap[4];

typedef u8 (*patchFunc)(void);
#define PatchLookup ((patchFunc*)0x06860000)

// ------------------------------------------------------
// These defines and inline functions are to map maximum
// speed/efficiency onto the memory system we have.
// ------------------------------------------------------
extern unsigned char cpu_readport_speccy(register unsigned short Port);
extern void cpu_writeport_speccy(register unsigned short Port,register unsigned char Value);

// ------------------------------------------------------------------------------
// This is how we access the Z80 memory. We indirect through the MemoryMap[] to
// allow for easy mapping by the 128K machines. It's slightly slower than direct
// RAM access but much faster than having to move around chunks of bank memory.
//
// Note that the MemoryMap[] here has each of the 4 segments offset by the
// segment * 16K. This allows us to not have to mask the Address with 0x3FFF
// and can instead just index directly knowing that MemoryMap[] has been offset
// properly when it was setup (e.g. on a 48K machine, each MemoryMap[] segment
// would point to the start of RAM_Memory[] such that this just turns into a 
// simple index by the address without having to mask. This buys us 10% speed.
// ------------------------------------------------------------------------------
inline byte OpZ80(word A)
{
    return MemoryMap[(A)>>14][A];
}

#define RdZ80       OpZ80 // Nothing unique about a memory read - same as an OpZ80 opcode fetch
#define RdZ80_noc   OpZ80 // Nothing unique about a memory read - same as an OpZ80 opcode fetch

// -------------------------------------------------------------------------------------------------------------
// ZX-Dandanator support is fairly basic - we're not supporting the full set of Dandanator functionality, only
// enough that we can do basic BANK swapping into the 512K ROM area and some basic handling on resets and
// disables via the 'special command 40'
// We use a command settle time of 35 T-States which is borrowed from ZEsarUX and a peek at the PIC code
// for the dandanator. This seems to work fine for the majority of Dandanator ROMs out there - mainly this is
// going to be used for Sword of Ianna and maybe Castlevania - Spectral Interlude plus a few compilation carts.
// -------------------------------------------------------------------------------------------------------------

#define DANDANATOR_COMMAND_TIME  35     // 35 T-States. Value doesn't have to be exact, but some delay must be used.

u8 dandanator_cmd   = 0x00;
u8 dandanator_data1 = 0x00;
u8 dandanator_data2 = 0x00;
u8 dan_latched_cmd  = 0x00;
u8 dandy_disabled   = 0;
u8 dandy_locked     = 0;
u8 dan_counter      = 0;
u8 dan_state        = 0;    // 0=Idle/Ready, 1=Command/Processing

// -----------------------------------------------------------------------------
// We use 'rom_special_bank' below to indicate that the Dandanator has control
// of the lower 16K of memory and if the normal 128K Spectrum zx_bank() routine
// is called, we will swap back the Dandanator ROM as needed.
// -----------------------------------------------------------------------------
void dandanator_switch_banks(u8 bank)
{
    // --------------------------------------------------------
    // Banks are 0-31 but are represented by bank numbers 1-32
    // --------------------------------------------------------
    if ((bank > 0) && (bank <= 32))
    {
        MemoryMap[0] = ROM_Memory + (0x4000 * (bank-1));
        rom_special_bank = bank;
    }
    else if ((bank == 33) || (bank == 34)) // Both represent the main Spectrum BIOS should be mapped in
    {
        if (myConfig.machine) // If we are 128K machine
        {
            MemoryMap[0] = SpectrumBios128 + ((portFD & 0x10) ? 0x4000 : 0x0000);
        }
        else // Otherwise original Spectrum 48K
        {
            MemoryMap[0] = SpectrumBios;
        }
        rom_special_bank = 0;
    }
}

__attribute__((noinline)) void dandanator_flash_write(word A, byte value)
{
    if (speccy_mode != MODE_ROM) return; // Make sure we are a ROM load or else a flash write is simply ignored...

    if (dandy_disabled) return;  // If the Dandanator has been turned off, no more commands are allowed...

    switch (A)
    {
        // Command Confirm
        case 0x00:
            dandy_disabled=1;
            ExecZ80_Speccy(CPU.TStates + DANDANATOR_COMMAND_TIME);
            dandy_disabled=0;

            // --------------------------------------------------------------------
            // Special command... data1 is the bank to swap in, data2 has some
            // bits that allow us to disable the dandanator or reset the CPU, etc.
            // --------------------------------------------------------------------
            if (dandanator_cmd == 40)
            {
                if (dandanator_data1 && (dandanator_data1 <= 33)) dandanator_switch_banks(dandanator_data1); // Bank 35 means 'keep previous bank'

                if (dandanator_data2 & 0x08) dandy_disabled = 1;
                if (dandanator_data2 & 0x04) dandy_locked = 1;  // We don't handle this yet... we assume programs are well behaved.
                if (dandanator_data2 & 0x02) IntZ80(&CPU, INT_NMI);
                if (dandanator_data2 & 0x01) ResetZ80(&CPU);
            }
            else if (dandanator_cmd && (dandanator_cmd <= 34))
            {
                dandanator_switch_banks(dandanator_cmd);
            }
            else if (dandanator_cmd == 36)
            {
                ResetZ80(&CPU);
            }
            else if (dandanator_cmd == 46)
            {
                DY++;  // This command is mainly used to lock/unlock Dandanator access. Not used in emulation.
            }
            dan_state = 0;
            break;

        // Command
        case 0x01:
            dandanator_cmd = value;
            if (dan_state == 0)
            {
                dan_latched_cmd = value;
                dan_state = 1;
                dan_counter = 0;
            }

            // --------------------------------------------------------------------------------------------------
            // The Dandanator is a strange beast... each time the command latch value is written, the Dandanator
            // seems to track an internal counter and if the counter value of 'write pulses' reaches the command
            // value, the Dandanator will handle the command immediately - in this way, it's simple to write a
            // command to switch to Bank 0 (command 1) with a single write (and Bank 1 with two writes, etc).
            // --------------------------------------------------------------------------------------------------
            if (dan_state == 1)
            {
                dan_counter++;
                if ((dandanator_cmd < 40) && (dan_counter == dan_latched_cmd))
                {
                    // ---------------------------------------------------------------
                    // Poor-mans way of handling the delay between the command and
                    // the dandanator finishing execution - we simply allow the CPU
                    // to run a few instructions - temporarily disabling re-entrancy.
                    // ---------------------------------------------------------------
                    dandy_disabled=1;
                    ExecZ80_Speccy(CPU.TStates + DANDANATOR_COMMAND_TIME);
                    dandy_disabled=0;

                    if (dan_latched_cmd && (dan_latched_cmd <= 34))
                    {
                        dandanator_switch_banks(dan_latched_cmd);
                    }
                    else if (dan_latched_cmd == 36)
                    {
                        ResetZ80(&CPU);
                    }
                    dan_state = 0;
                }
                else
                {
                    // See if the command changed... if so, restart count (probably isn't needed for well-behaved carts)
                    if (dan_latched_cmd != value)
                    {
                        dan_state = 1;
                        dan_counter = 1;
                        dan_latched_cmd = value;
                    }
                }
            }
            break;

        // Data 1 latch
        case 0x02:
            dandanator_data1 = value;
            break;

        // Data 2 latch
        case 0x03:
            dandanator_data2 = value;
            break;
    }
}

// -------------------------------------------------------------------------------------------
// The only extra protection we have in writes is to ensure we don't write into the ROM area.
// We support the possibility of a Dandanator ROM which writes to the first few addresses
// of the ROM space ($0000 to $0003) and that's handled by dandanator_flash_write().
// -------------------------------------------------------------------------------------------
static void WrZ80(word A, byte value)   {if (A & 0xC000) MemoryMap[(A)>>14][A] = value; else dandanator_flash_write(A,value);}

// -------------------------------------------------------------------
// And these two macros will give us access to the Z80 I/O ports...
// -------------------------------------------------------------------
#define OutZ80(P,V)     cpu_writeport_speccy(P,V)
#define InZ80(P)        cpu_readport_speccy(P)


/** Macros for use through the CPU subsystem */
#define S(Fl)        CPU.AF.B.l|=Fl
#define R(Fl)        CPU.AF.B.l&=~(Fl)
#define FLAGS(Rg,Fl) CPU.AF.B.l=Fl|ZSTable[Rg]
#define INCR(N)      CPU.R++       // Faster to just increment this odd 7-bit RAM Refresh counter here and mask off and OR the high bit back in when asked for in CodesED.h

#define M_RLC(Rg)      \
  CPU.AF.B.l=Rg>>7;Rg=(Rg<<1)|CPU.AF.B.l;CPU.AF.B.l|=PZSTable[Rg]
#define M_RRC(Rg)      \
  CPU.AF.B.l=Rg&0x01;Rg=(Rg>>1)|(CPU.AF.B.l<<7);CPU.AF.B.l|=PZSTable[Rg]
#define M_RL(Rg)       \
  if(Rg&0x80)          \
  {                    \
    Rg=(Rg<<1)|(CPU.AF.B.l&C_FLAG); \
    CPU.AF.B.l=PZSTable[Rg]|C_FLAG; \
  }                    \
  else                 \
  {                    \
    Rg=(Rg<<1)|(CPU.AF.B.l&C_FLAG); \
    CPU.AF.B.l=PZSTable[Rg];        \
  }
#define M_RR(Rg)       \
  if(Rg&0x01)          \
  {                    \
    Rg=(Rg>>1)|(CPU.AF.B.l<<7);     \
    CPU.AF.B.l=PZSTable[Rg]|C_FLAG; \
  }                    \
  else                 \
  {                    \
    Rg=(Rg>>1)|(CPU.AF.B.l<<7);     \
    CPU.AF.B.l=PZSTable[Rg];        \
  }

#define M_SLA(Rg)      \
  CPU.AF.B.l=Rg>>7;Rg<<=1;CPU.AF.B.l|=PZSTable[Rg]
#define M_SRA(Rg)      \
  CPU.AF.B.l=Rg&C_FLAG;Rg=(Rg>>1)|(Rg&0x80);CPU.AF.B.l|=PZSTable[Rg]

#define M_SLL(Rg)      \
  CPU.AF.B.l=Rg>>7;Rg=(Rg<<1)|0x01;CPU.AF.B.l|=PZSTable[Rg]
#define M_SRL(Rg)      \
  CPU.AF.B.l=Rg&0x01;Rg>>=1;CPU.AF.B.l|=PZSTable[Rg]

#define M_BIT(Bit,Rg)  \
  CPU.AF.B.l=(CPU.AF.B.l&C_FLAG)|PZSHTable_BIT[Rg&(1<<Bit)]

#define M_SET(Bit,Rg) Rg|=1<<Bit
#define M_RES(Bit,Rg) Rg&=~(1<<Bit)

#define M_POP(Rg)   CPU.Rg.B.l=RdZ80(CPU.SP.W++);CPU.Rg.B.h=RdZ80(CPU.SP.W++)
#define M_PUSH(Rg)  WrZ80(--CPU.SP.W,CPU.Rg.B.h);WrZ80(--CPU.SP.W,CPU.Rg.B.l)

#define M_CALL         \
  J.B.l=RdZ80(CPU.PC.W++);J.B.h=RdZ80(CPU.PC.W++); T_INC(1); \
  WrZ80(--CPU.SP.W,CPU.PC.B.h);WrZ80(--CPU.SP.W,CPU.PC.B.l); \
  CPU.PC.W=J.W; \
  JumpZ80(J.W)

#define M_JP  CPU.PC.W = (u32)RdZ80(CPU.PC.W) | ((u32)RdZ80(CPU.PC.W+1) << 8);
#define M_JR  CPU.PC.W+=(offset)RdZ80(CPU.PC.W)+1;JumpZ80(CPU.PC.W)
#define M_RET CPU.PC.B.l=RdZ80(CPU.SP.W++);CPU.PC.B.h=RdZ80(CPU.SP.W++);JumpZ80(CPU.PC.W)
#define M_RST(Ad)    WrZ80(--CPU.SP.W,CPU.PC.B.h);WrZ80(--CPU.SP.W,CPU.PC.B.l);CPU.PC.W=Ad;JumpZ80(Ad)
#define M_LDWORD(Rg) CPU.Rg.B.l=RdZ80(CPU.PC.W++);CPU.Rg.B.h=RdZ80(CPU.PC.W++)

#define M_ADD(Rg)      \
  J.W=CPU.AF.B.h+Rg;    \
  CPU.AF.B.l=           \
    (~(CPU.AF.B.h^Rg)&(Rg^J.B.l)&0x80? V_FLAG:0)| \
    J.B.h|ZSTable[J.B.l]|                        \
    ((CPU.AF.B.h^Rg^J.B.l)&H_FLAG);               \
  CPU.AF.B.h=J.B.l

#define M_SUB(Rg)      \
  J.W=CPU.AF.B.h-Rg;    \
  CPU.AF.B.l=           \
    ((CPU.AF.B.h^Rg)&(CPU.AF.B.h^J.B.l)&0x80? V_FLAG:0)| \
    N_FLAG|-J.B.h|ZSTable[J.B.l]|                      \
    ((CPU.AF.B.h^Rg^J.B.l)&H_FLAG);                     \
  CPU.AF.B.h=J.B.l

#define M_ADC(Rg)      \
  J.W=CPU.AF.B.h+Rg+(CPU.AF.B.l&C_FLAG); \
  CPU.AF.B.l=                           \
    (~(CPU.AF.B.h^Rg)&(Rg^J.B.l)&0x80? V_FLAG:0)| \
    J.B.h|ZSTable[J.B.l]|              \
    ((CPU.AF.B.h^Rg^J.B.l)&H_FLAG);     \
  CPU.AF.B.h=J.B.l

#define M_SBC(Rg)      \
  J.W=CPU.AF.B.h-Rg-(CPU.AF.B.l&C_FLAG); \
  CPU.AF.B.l=                           \
    ((CPU.AF.B.h^Rg)&(CPU.AF.B.h^J.B.l)&0x80? V_FLAG:0)| \
    N_FLAG|-J.B.h|ZSTable[J.B.l]|      \
    ((CPU.AF.B.h^Rg^J.B.l)&H_FLAG);     \
  CPU.AF.B.h=J.B.l

#define M_CP(Rg)       \
  J.W=CPU.AF.B.h-Rg;    \
  CPU.AF.B.l=           \
    ((CPU.AF.B.h^Rg)&(CPU.AF.B.h^J.B.l)&0x80? V_FLAG:0)| \
    N_FLAG|-J.B.h|ZSTable[J.B.l]|                      \
    ((CPU.AF.B.h^Rg^J.B.l)&H_FLAG)

#define M_AND(Rg) CPU.AF.B.h&=Rg;CPU.AF.B.l=H_FLAG|PZSTable[CPU.AF.B.h]
#define M_OR(Rg)  CPU.AF.B.h|=Rg;CPU.AF.B.l=PZSTable[CPU.AF.B.h]
#define M_XOR(Rg) CPU.AF.B.h^=Rg;CPU.AF.B.l=PZSTable[CPU.AF.B.h]

#define M_IN(Rg)        \
  Rg=InZ80(CPU.BC.W);  \
  CPU.AF.B.l=PZSTable[Rg]|(CPU.AF.B.l&C_FLAG)

#define M_INC(Rg)       \
  Rg++;                 \
  CPU.AF.B.l=(CPU.AF.B.l&C_FLAG)|ZSTable_INC[Rg];

#define M_DEC(Rg)       \
  Rg--;                 \
  CPU.AF.B.l= (CPU.AF.B.l&C_FLAG)|ZSTable_DEC[Rg];

#define M_ADDW(Rg1,Rg2) \
  J.W=(CPU.Rg1.W+CPU.Rg2.W)&0xFFFF;                        \
  CPU.AF.B.l=                                             \
    (CPU.AF.B.l&~(H_FLAG|N_FLAG|C_FLAG))|                 \
    ((CPU.Rg1.W^CPU.Rg2.W^J.W)&0x1000? H_FLAG:0)|          \
    (((long)CPU.Rg1.W+(long)CPU.Rg2.W)&0x10000? C_FLAG:0); \
  CPU.Rg1.W=J.W

#define M_ADCW(Rg)      \
  I=CPU.AF.B.l&C_FLAG;J.W=(CPU.HL.W+CPU.Rg.W+I)&0xFFFF;           \
  CPU.AF.B.l=                                                   \
    (((long)CPU.HL.W+(long)CPU.Rg.W+(long)I)&0x10000? C_FLAG:0)| \
    (~(CPU.HL.W^CPU.Rg.W)&(CPU.Rg.W^J.W)&0x8000? V_FLAG:0)|       \
    ((CPU.HL.W^CPU.Rg.W^J.W)&0x1000? H_FLAG:0)|                  \
    (J.W? 0:Z_FLAG)|(J.B.h&S_FLAG);                            \
  CPU.HL.W=J.W

#define M_SBCW(Rg)      \
  I=CPU.AF.B.l&C_FLAG;J.W=(CPU.HL.W-CPU.Rg.W-I)&0xFFFF;           \
  CPU.AF.B.l=                                                   \
    N_FLAG|                                                    \
    (((long)CPU.HL.W-(long)CPU.Rg.W-(long)I)&0x10000? C_FLAG:0)| \
    ((CPU.HL.W^CPU.Rg.W)&(CPU.HL.W^J.W)&0x8000? V_FLAG:0)|        \
    ((CPU.HL.W^CPU.Rg.W^J.W)&0x1000? H_FLAG:0)|                  \
    (J.W? 0:Z_FLAG)|(J.B.h&S_FLAG);                            \
  CPU.HL.W=J.W


extern void Trap_Bad_Ops(char *, byte, word);

/** ResetZ80() ***********************************************/
/** This function can be used to reset the register struct  **/
/** before starting execution with Z80(). It sets the       **/
/** registers to their supposed initial values.             **/
/*************************************************************/
void ResetZ80(Z80 *R)
{
  CPU.PC.W     = 0x0000;
  CPU.SP.W     = 0xF000;
  CPU.AF.W     = 0x0000;
  CPU.BC.W     = 0x0000;
  CPU.DE.W     = 0x0000;
  CPU.HL.W     = 0x0000;
  CPU.AF1.W    = 0x0000;
  CPU.BC1.W    = 0x0000;
  CPU.DE1.W    = 0x0000;
  CPU.HL1.W    = 0x0000;
  CPU.IX.W     = 0x0000;
  CPU.IY.W     = 0x0000;
  CPU.I        = 0x00;
  CPU.R        = 0x00;
  CPU.R_HighBit= 0x00;
  CPU.IFF      = 0x00;
  CPU.IRequest = INT_NONE;
  CPU.EI_Delay = 0;
  CPU.Trace    = 0;
  CPU.TrapBadOps = 1;
  CPU.IAutoReset = 1;
  CPU.TStates_IRequest = 0;
  CPU.TStates = 0;

  JumpZ80(CPU.PC.W);
}


/** IntZ80() *************************************************/
/** This function will generate interrupt of given vector.  **/
/*************************************************************/
ITCM_CODE void IntZ80(Z80 *R,word Vector)
{
    /* If HALTed, take CPU off HALT instruction */
    if(CPU.IFF&IFF_HALT) { CPU.PC.W++;CPU.IFF&=~IFF_HALT; }

    if((CPU.IFF&IFF_1)||(Vector==INT_NMI))
    {
      CPU.TStates += 19; // Z80 takes 19 cycles to acknowledge interrupt, setup stack and read vector

      /* Save PC on stack */
      M_PUSH(PC);

      /* Automatically reset IRequest */
      CPU.IRequest=INT_NONE;

      /* If it is NMI... */
      if(Vector==INT_NMI)
      {
          /* Clear IFF1 */
          CPU.IFF&=~(IFF_1|IFF_EI);
          /* Jump to hardwired NMI vector */
          CPU.PC.W=0x0066;
          JumpZ80(0x0066);
          /* Done */
          return;
      }

      /* Further interrupts off */
      CPU.IFF&=~(IFF_1|IFF_2|IFF_EI);

      /* If in IM2 mode... */
      if(CPU.IFF&IFF_IM2)
      {
          /* Make up the vector address - technically the Vector is whatever is on the data bus but is usually 0xFF */
          Vector=(0xFF)|((word)(CPU.I)<<8);
          /* Read the vector */
          CPU.PC.B.l=RdZ80(Vector++);
          CPU.PC.B.h=RdZ80(Vector);

          JumpZ80(CPU.PC.W);

          /* Done */
          return;
      }

      /* If in IM1 mode, just jump to hardwired IRQ vector */
      if(CPU.IFF&IFF_IM1) { CPU.PC.W=0x0038; JumpZ80(0x0038); return; }

      /* If in IM0 mode... Not used on ZX Spectrum but handled here anyway */

      /* Jump to a vector */
      switch(Vector)
      {
          case INT_RST00: CPU.PC.W=0x0000;JumpZ80(0x0000);break;
          case INT_RST08: CPU.PC.W=0x0008;JumpZ80(0x0008);break;
          case INT_RST10: CPU.PC.W=0x0010;JumpZ80(0x0010);break;
          case INT_RST18: CPU.PC.W=0x0018;JumpZ80(0x0018);break;
          case INT_RST20: CPU.PC.W=0x0020;JumpZ80(0x0020);break;
          case INT_RST28: CPU.PC.W=0x0028;JumpZ80(0x0028);break;
          case INT_RST30: CPU.PC.W=0x0030;JumpZ80(0x0030);break;
          case INT_RST38: CPU.PC.W=0x0038;JumpZ80(0x0038);break;
      }
    }
}


static void CodesCB_Speccy(void)
{
  register byte I;

  /* Read opcode and count cycles */
  I=OpZ80(CPU.PC.W++);
  CPU.TStates += CyclesCB[I];

  /* R register incremented on each M1 cycle */
  INCR(1);

  switch(I)
  {
#include "CodesCB.h"
    default:
      if(CPU.TrapBadOps)  Trap_Bad_Ops(" CB ", I, CPU.PC.W-2);
  }
}

static void CodesDDCB_Speccy(void)
{
  register pair J;
  register byte I;

#define XX IX
  /* Get offset, read opcode and count cycles */
  J.W=CPU.XX.W+(offset)OpZ80(CPU.PC.W++);
  I=OpZ80(CPU.PC.W++);
  CPU.TStates += CyclesXXCB[I];

  switch(I)
  {
#include "CodesXCB.h"
    default:
      if(CPU.TrapBadOps)  Trap_Bad_Ops("DDCB", I, CPU.PC.W-4);
  }
#undef XX
}

static void CodesFDCB_Speccy(void)
{
  register pair J;
  register byte I;

#define XX IY
  /* Get offset, read opcode and count cycles */
  J.W=CPU.XX.W+(offset)OpZ80(CPU.PC.W++);
  I=OpZ80(CPU.PC.W++);
  CPU.TStates += CyclesXXCB[I];

  switch(I)
  {
#include "CodesXCB.h"
    default:
      if(CPU.TrapBadOps)  Trap_Bad_Ops("FDCB", I, CPU.PC.W-4);
  }
#undef XX
}

ITCM_CODE static void CodesED_Speccy(void)
{
  register byte I;
  register pair J;

  /* Read opcode and count cycles */
  I=OpZ80(CPU.PC.W++);
  CPU.TStates += CyclesED[I];

  /* R register incremented on each M1 cycle */
  INCR(1);

  switch(I)
  {
#include "CodesED.h"
    case PFX_ED:
      CPU.PC.W--;break;
    default:
      if(CPU.TrapBadOps) Trap_Bad_Ops(" ED ", I, CPU.PC.W-4);
  }
}

static void CodesDD_Speccy(void)
{
  register byte I,K;
  register pair J;

#define XX IX
  /* Read opcode and count cycles */
  I=OpZ80(CPU.PC.W++);
  CPU.TStates += CyclesXX[I];

  /* R register incremented on each M1 cycle */
  INCR(1);

  switch(I)
  {
#include "CodesXX.h"
    case PFX_FD:
    case PFX_DD:
      CPU.PC.W--;break;
    case PFX_CB:
      CodesDDCB_Speccy();break;
    default:
      if(CPU.TrapBadOps)  Trap_Bad_Ops(" DD ", I, CPU.PC.W-2);
  }
#undef XX
}

static void CodesFD_Speccy(void)
{
  register byte I,K;
  register pair J;

#define XX IY
  /* Read opcode and count cycles */
  I=OpZ80(CPU.PC.W++);
  CPU.TStates += CyclesXX[I];

  /* R register incremented on each M1 cycle */
  INCR(1);

  switch(I)
  {
#include "CodesXX.h"
    case PFX_FD:
    case PFX_DD:
      CPU.PC.W--;break;
    case PFX_CB:
      CodesFDCB_Speccy();break;
    default:
        if(CPU.TrapBadOps)  Trap_Bad_Ops(" FD ", I, CPU.PC.W-2);
  }
#undef XX
}

// ------------------------------------------------------------------------------
// Almost 15K wasted space... but needed so we can keep the Enable Interrupt
// instruction out of the main fast Z80 instruction loop. When the EI instruction
// is issued, the interrupts are not enabled until one instruction later. This
// function let's us execute that one instruction - somewhat more slowly but
// interrupts are enabled very infrequently (often enabled and left that way).
// ------------------------------------------------------------------------------
void ExecOneInstruction(void)
{
  register byte I;
  register pair J;
  u32 RunToCycles = CPU.TStates+1;

  I=OpZ80(CPU.PC.W++);
  CPU.TStates += Cycles_NoM1Wait[I];

  /* R register incremented on each M1 cycle */
  INCR(1);

  /* Interpret opcode */
  switch(I)
  {
#include "Codes.h"
    case PFX_CB: CodesCB_Speccy();break;
    case PFX_ED: CodesED_Speccy();break;
    case PFX_FD: CodesFD_Speccy();break;
    case PFX_DD: CodesDD_Speccy();break;
  }
}

// ------------------------------------------------------------------------
// The Enable Interrupt is delayed 1 M1 instruction and we must also check
// to see if we are within the 32 TState period where the ZX Spectrum ULA
// would hold the Interrupt Request pulse...
// ------------------------------------------------------------------------
ITCM_CODE void EI_Enable(void)
{
   ExecOneInstruction();
   CPU.IFF=(CPU.IFF&~IFF_EI)|IFF_1;
   if (CPU.IRequest != INT_NONE)
   {
       if ((CPU.TStates - CPU.TStates_IRequest) < 32) IntZ80(&CPU, CPU.IRequest); // Fire the interrupt
       else CPU.IRequest = INT_NONE; // We missed the interrupt...
   }
}

// -----------------------------------------------------------------------------------
// The main Z80 instruction loop. We put this 15K chunk into fast memory as we
// want to make the Z80 run as quickly as possible - this is the heart of the system.
// -----------------------------------------------------------------------------------
ITCM_CODE void ExecZ80_Speccy(u32 RunToCycles)
{
  if (accurate_emulation) return ExecZ80_Speccy_a(RunToCycles);
  
  register byte I;
  register pair J;
  u8 render = zx_ScreenRendering;   // Slightly faster access from stack

  while (CPU.TStates < RunToCycles)
  {
      // ----------------------------------------------------------------------------------------
      // If we are in contended memory - add penalty. This is not cycle accurate but we want to
      // at least make an attempt to get closer on the cycle timing. So we simply use an 'average'
      // penalty of 3 cycles if we are in contended memory while the screen is rendering. It's
      // rough but gets us close enough to play games. We can improve this later...
      // ----------------------------------------------------------------------------------------
      if (render)
      {
          if (CPU.PC.W & 0x4000) // Either 0x4000 or 0xC000
          {
              if (CPU.PC.W & 0x8000) // Must be 0xC000
              {
                  // For the ZX 128K bank, we contend if the bank is odd (1,3,5,7)
                  if (zx_128k_mode && (portFD & 1)) CPU.TStates += AVERAGE_CONTEND_DELAY;
              }
              else // Must be 0x4000 - we contend on any video access (both 48K and 128K)
              {
                  CPU.TStates += AVERAGE_CONTEND_DELAY;
              }
          }
      }

      I=OpZ80(CPU.PC.W++);
      CPU.TStates += Cycles_NoM1Wait[I];

      /* R register incremented on each M1 cycle */
      INCR(1);

      /* Interpret opcode */
      switch(I)
      {
#include "Codes.h"
        case PFX_CB: CodesCB_Speccy();break;
        case PFX_ED: CodesED_Speccy();break;
        case PFX_FD: CodesFD_Speccy();break;
        case PFX_DD: CodesDD_Speccy();break;
      }
  }
}

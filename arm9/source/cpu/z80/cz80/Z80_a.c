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
extern u8 zx_128k_mode, portFD;
extern void EI_Enable(void);
void ExecOneInstruction(void);
void ResetZ80(Z80 *R);
void dandanator_flash_write(word A, byte value);

#define INLINE static inline

/** System-Dependent Stuff ***********************************/
/** This is system-dependent code put here to speed things  **/
/** up. It has to stay inlined to be fast.                  **/
/*************************************************************/
extern u8 *MemoryMap[4];
u8 ContendMap[4] __attribute__((section(".dtcm"))) = {0,1,0,0};

typedef u8 (*patchFunc)(void);
#define PatchLookup ((patchFunc*)0x06860000)
extern void IntZ80(Z80 *R,word Vector);

// ------------------------------------------------------
// These defines and inline functions are to map maximum
// speed/efficiency onto the memory system we have.
// ------------------------------------------------------
extern unsigned char cpu_readport_speccy(register unsigned short Port);
extern void cpu_writeport_speccy(register unsigned short Port,register unsigned char Value);

// The contended delay table is specific for the 48K Spectrum vs the 128K Spectrum.
// This table is offset to provide the best timing I can muster with imperfect emulation.
u8 cpu_contended_delay_128[228] __attribute__((section(".dtcm"))) = 
{
    0,0,
    6,5,4,3,2,1,0,0,
    6,5,4,3,2,1,0,0,
    6,5,4,3,2,1,0,0,
    6,5,4,3,2,1,0,0,
    6,5,4,3,2,1,0,0,
    6,5,4,3,2,1,0,0,
    6,5,4,3,2,1,0,0,

    6,5,4,3,2,1,0,0,
    6,5,4,3,2,1,0,0,
    6,5,4,3,2,1,0,0,
    6,5,4,3,2,1,0,0,
    6,5,4,3,2,1,0,0,
    6,5,4,3,2,1,0,0,
    6,5,4,3,2,1,0,0,
    6,5,4,3,2,1,0,0,

    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,

    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,6,5,4,3,
    2,1,
};

u8 cpu_contended_delay_48[224] __attribute__((section(".dtcm"))) = 
{
    4,3,2,1,0,0,
    6,5,4,3,2,1,0,0,
    6,5,4,3,2,1,0,0,
    6,5,4,3,2,1,0,0,
    6,5,4,3,2,1,0,0,
    6,5,4,3,2,1,0,0,
    6,5,4,3,2,1,0,0,
    6,5,4,3,2,1,0,0,

    6,5,4,3,2,1,0,0,
    6,5,4,3,2,1,0,0,
    6,5,4,3,2,1,0,0,
    6,5,4,3,2,1,0,0,
    6,5,4,3,2,1,0,0,
    6,5,4,3,2,1,0,0,
    6,5,4,3,2,1,0,0,
    6,5,4,3,2,1,0,0,

    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,

    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    6,5
};

#define T_INC(X)    CPU.TStates +=(X);
#define J_ADJ       
#define R_ADJ
#define C_ADJ
#define PhantomRdZ80(A) RdZ80(A)

// ------------------------------------------------------------------------------------------
// This happens only 5-10% of the time so we don't inline to provide better ARM code density
// ------------------------------------------------------------------------------------------
ITCM_CODE __attribute__((noinline)) void ContendMemory(void)
{
    CPU.TStates += cpu_contended_delay_128[(CPU.TStates) % CYCLES_PER_SCANLINE_128];
}

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
inline __attribute__((always_inline)) byte OpZ80(word A)
{
    if (ContendMap[(A)>>14]) ContendMemory();
    CPU.TStates += 4;  // OpCode reads and process are 4 cycles    
    return MemoryMap[(A)>>14][A];
}

inline __attribute__((always_inline)) static byte RdZ80(word A)
{
    if (ContendMap[(A)>>14]) ContendMemory();
    CPU.TStates += 3; // Memory reads are 3 cycles
    return MemoryMap[(A)>>14][A];
}

inline __attribute__((always_inline)) static byte RdZ80_noc(word A)
{
    CPU.TStates += 3; // Memory reads are 3 cycles
    return MemoryMap[(A)>>14][A];
}

// -------------------------------------------------------------------------------------------
// The only extra protection we have in writes is to ensure we don't write into the ROM area.
// We support the possibility of a Dandanator ROM which writes to the first few addresses
// of the ROM space ($0000 to $0003) and that's handled by dandanator_flash_write().
// -------------------------------------------------------------------------------------------
inline __attribute__((always_inline)) void WrZ80(word A, byte value)
{
    if (A & 0xC000)
    {
        if (ContendMap[(A)>>14]) CPU.TStates += cpu_contended_delay_128[(CPU.TStates) % CYCLES_PER_SCANLINE_128];
        MemoryMap[(A)>>14][A] = value; 
    }
    else dandanator_flash_write(A,value);
    
    CPU.TStates += 3; // Memory writes are 3 cycles
}

// ----------------------------------------------------------------------
// For Stack Writes, we are assuming there would be no possibility of a 
// flash/dandanator write (otherwise something really bad has happened).
// ----------------------------------------------------------------------
inline __attribute__((always_inline)) void WrZ80_fast(word A, byte value)
{
    if (ContendMap[(A)>>14]) CPU.TStates += cpu_contended_delay_128[(CPU.TStates) % CYCLES_PER_SCANLINE_128];
    MemoryMap[(A)>>14][A] = value; 
    CPU.TStates += 3; // Memory writes are 3 cycles
}


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

#define M_SLA(Rg)      CPU.AF.B.l=Rg>>7;Rg<<=1;CPU.AF.B.l|=PZSTable[Rg]
#define M_SRA(Rg)      CPU.AF.B.l=Rg&C_FLAG;Rg=(Rg>>1)|(Rg&0x80);CPU.AF.B.l|=PZSTable[Rg]

#define M_SLL(Rg)      CPU.AF.B.l=Rg>>7;Rg=(Rg<<1)|0x01;CPU.AF.B.l|=PZSTable[Rg]
#define M_SRL(Rg)      CPU.AF.B.l=Rg&0x01;Rg>>=1;CPU.AF.B.l|=PZSTable[Rg]

#define M_BIT(Bit,Rg)  CPU.AF.B.l=(CPU.AF.B.l&C_FLAG)|PZSHTable_BIT[Rg&(1<<Bit)]

#define M_SET(Bit,Rg)  Rg|=1<<Bit
#define M_RES(Bit,Rg)  Rg&=~(1<<Bit)


#define M_CALL         \
  J.B.l=RdZ80(CPU.PC.W++);J.B.h=RdZ80(CPU.PC.W++); T_INC(1); \
  WrZ80_fast(--CPU.SP.W,CPU.PC.B.h);WrZ80_fast(--CPU.SP.W,CPU.PC.B.l); \
  CPU.PC.W=J.W; \
  JumpZ80(J.W)

#define M_JP         CPU.PC.W = (u32)RdZ80(CPU.PC.W) | ((u32)RdZ80(CPU.PC.W+1) << 8);
#define M_JR         CPU.PC.W+=(offset)RdZ80(CPU.PC.W)+1;JumpZ80(CPU.PC.W)

#define M_RET        CPU.PC.B.l=RdZ80(CPU.SP.W++);CPU.PC.B.h=RdZ80_noc(CPU.SP.W++);JumpZ80(CPU.PC.W) // TBD: HACK!! Remove noc() when fixed.
#define M_POP(Rg)    CPU.Rg.B.l=RdZ80(CPU.SP.W++);CPU.Rg.B.h=RdZ80_noc(CPU.SP.W++);                  // TBD: HACK!! Remove noc() when fixed.

#define M_PUSH(Rg)   WrZ80_fast(--CPU.SP.W,CPU.Rg.B.h);WrZ80_fast(--CPU.SP.W,CPU.Rg.B.l)
#define M_RST(Ad)    WrZ80_fast(--CPU.SP.W,CPU.PC.B.h);WrZ80_fast(--CPU.SP.W,CPU.PC.B.l);CPU.PC.W=Ad;JumpZ80(Ad)

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
extern void ResetZ80(Z80 *R);

void    EI_Enable_128(void);
void    EI_Enable_48(void);
#define EI_Enable   EI_Enable_128

static void CodesCB_Speccy_128(void)
{
  register byte I;

  /* Read opcode and count cycles */
  I=OpZ80(CPU.PC.W++);

  /* R register incremented on each M1 cycle */
  INCR(1);

  switch(I)
  {
#include "CodesCB.h"
    default:
      if(CPU.TrapBadOps)  Trap_Bad_Ops(" CB ", I, CPU.PC.W-2);
  }
}

static void CodesDDCB_Speccy_128(void)
{
  register pair J;
  register byte I;

#define XX IX
  /* Get offset, read opcode and count cycles */
  J.W=CPU.XX.W+(offset)RdZ80(CPU.PC.W++);
  I=OpZ80(CPU.PC.W++);

  switch(I)
  {
#include "CodesXCB.h"
    default:
      if(CPU.TrapBadOps)  Trap_Bad_Ops("DDCB", I, CPU.PC.W-4);
  }
#undef XX
}

static void CodesFDCB_Speccy_128(void)
{
  register pair J;
  register byte I;

#define XX IY
  /* Get offset, read opcode and count cycles */
  J.W=CPU.XX.W+(offset)RdZ80(CPU.PC.W++);
  I=OpZ80(CPU.PC.W++);

  switch(I)
  {
#include "CodesXCB.h"
    default:
      if(CPU.TrapBadOps)  Trap_Bad_Ops("FDCB", I, CPU.PC.W-4);
  }
#undef XX
}

static void CodesED_Speccy_128(void)
{
  register byte I;
  register pair J;

  /* Read opcode and count cycles */
  I=OpZ80(CPU.PC.W++);
  
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

static void CodesDD_Speccy_128(void)
{
  register byte I,K;
  register pair J;

#define XX IX
  /* Read opcode and count cycles */
  I=OpZ80(CPU.PC.W++);

  /* R register incremented on each M1 cycle */
  INCR(1);

  switch(I)
  {
#include "CodesXX.h"
    case PFX_FD:
    case PFX_DD:
      CPU.PC.W--;break;
    case PFX_CB:
      CodesDDCB_Speccy_128();break;
    default:
      if(CPU.TrapBadOps)  Trap_Bad_Ops(" DD ", I, CPU.PC.W-2);
  }
#undef XX
}

static void CodesFD_Speccy_128(void)
{
  register byte I,K;
  register pair J;

#define XX IY
  /* Read opcode and count cycles */
  I=OpZ80(CPU.PC.W++);

  /* R register incremented on each M1 cycle */
  INCR(1);

  switch(I)
  {
#include "CodesXX.h"
    case PFX_FD:
    case PFX_DD:
      CPU.PC.W--;break;
    case PFX_CB:
      CodesFDCB_Speccy_128();break;
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
void ExecOneInstruction_128(void)
{
  register byte I;
  register pair J;
  u32 RunToCycles = CPU.TStates+1;

  I=OpZ80(CPU.PC.W++);

  /* R register incremented on each M1 cycle */
  INCR(1);

  /* Interpret opcode */
  switch(I)
  {
#include "Codes.h"
    case PFX_CB: CodesCB_Speccy_128();break;
    case PFX_ED: CodesED_Speccy_128();break;
    case PFX_FD: CodesFD_Speccy_128();break;
    case PFX_DD: CodesDD_Speccy_128();break;
  }
}

// ------------------------------------------------------------------------
// The Enable Interrupt is delayed 1 M1 instruction and we must also check
// to see if we are within the 32 TState period where the ZX Spectrum ULA
// would hold the Interrupt Request pulse...
// ------------------------------------------------------------------------
void EI_Enable_128(void)
{
   ExecOneInstruction_128();
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
void ExecZ80_Speccy_128(u32 RunToCycles)
{
  register byte I;
  register pair J;

  while (CPU.TStates < RunToCycles)
  {
      I=OpZ80(CPU.PC.W++);

      /* R register incremented on each M1 cycle */
      INCR(1);

      /* Interpret opcode */
      switch(I)
      {
#include "Codes.h"
        case PFX_CB: CodesCB_Speccy_128();break;
        case PFX_ED: CodesED_Speccy_128();break;
        case PFX_FD: CodesFD_Speccy_128();break;
        case PFX_DD: CodesDD_Speccy_128();break;
      }
  }
}


// ---------------------------------------------------------------------------------------
// And yet ANOTHER copy of the core Z80 but this time optimized for 48K memory contention.
// ---------------------------------------------------------------------------------------
#define OpZ80       OpZ80_48
#define RdZ80       RdZ80_48
#define WrZ80       WrZ80_48
#define WrZ80_fast  WrZ80_48
#undef  EI_Enable
#define EI_Enable   EI_Enable_48

ITCM_CODE __attribute__((noinline)) void ContendMemory_48(void)
{
    CPU.TStates += cpu_contended_delay_48[(CPU.TStates) % CYCLES_PER_SCANLINE_48];
}


inline __attribute__((always_inline)) byte OpZ80_48(word A)
{
    if (ContendMap[(A)>>14]) ContendMemory_48();
    CPU.TStates += 4;  // OpCode reads and process are 4 cycles
    return MemoryMap[(A)>>14][A];
}

inline __attribute__((always_inline)) static byte RdZ80_48(word A)
{
    if (ContendMap[(A)>>14]) ContendMemory_48();
    CPU.TStates += 3; // Memory reads are 3 cycles
    return MemoryMap[(A)>>14][A];
}

inline __attribute__((always_inline)) void WrZ80_48(word A, byte value)
{
    if (A & 0xC000)
    {
        if (ContendMap[(A)>>14]) ContendMemory_48();
        MemoryMap[(A)>>14][A] = value; 
    }
    else dandanator_flash_write(A,value);
    
    CPU.TStates += 3; // Memory writes are 3 cycles
}



static void CodesCB_Speccy_48(void)
{
  register byte I;

  /* Read opcode and count cycles */
  I=OpZ80(CPU.PC.W++);

  /* R register incremented on each M1 cycle */
  INCR(1);

  switch(I)
  {
#include "CodesCB.h"
    default:
      if(CPU.TrapBadOps)  Trap_Bad_Ops(" CB ", I, CPU.PC.W-2);
  }
}

static void CodesDDCB_Speccy_48(void)
{
  register pair J;
  register byte I;

#define XX IX
  /* Get offset, read opcode and count cycles */
  J.W=CPU.XX.W+(offset)RdZ80(CPU.PC.W++);
  I=OpZ80(CPU.PC.W++);

  switch(I)
  {
#include "CodesXCB.h"
    default:
      if(CPU.TrapBadOps)  Trap_Bad_Ops("DDCB", I, CPU.PC.W-4);
  }
#undef XX
}

static void CodesFDCB_Speccy_48(void)
{
  register pair J;
  register byte I;

#define XX IY
  /* Get offset, read opcode and count cycles */
  J.W=CPU.XX.W+(offset)RdZ80(CPU.PC.W++);
  I=OpZ80(CPU.PC.W++);

  switch(I)
  {
#include "CodesXCB.h"
    default:
      if(CPU.TrapBadOps)  Trap_Bad_Ops("FDCB", I, CPU.PC.W-4);
  }
#undef XX
}

static void CodesED_Speccy_48(void)
{
  register byte I;
  register pair J;

  /* Read opcode and count cycles */
  I=OpZ80(CPU.PC.W++);
  
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

static void CodesDD_Speccy_48(void)
{
  register byte I,K;
  register pair J;

#define XX IX
  /* Read opcode and count cycles */
  I=OpZ80(CPU.PC.W++);

  /* R register incremented on each M1 cycle */
  INCR(1);

  switch(I)
  {
#include "CodesXX.h"
    case PFX_FD:
    case PFX_DD:
      CPU.PC.W--;break;
    case PFX_CB:
      CodesDDCB_Speccy_48();break;
    default:
      if(CPU.TrapBadOps)  Trap_Bad_Ops(" DD ", I, CPU.PC.W-2);
  }
#undef XX
}

static void CodesFD_Speccy_48(void)
{
  register byte I,K;
  register pair J;

#define XX IY
  /* Read opcode and count cycles */
  I=OpZ80(CPU.PC.W++);

  /* R register incremented on each M1 cycle */
  INCR(1);

  switch(I)
  {
#include "CodesXX.h"
    case PFX_FD:
    case PFX_DD:
      CPU.PC.W--;break;
    case PFX_CB:
      CodesFDCB_Speccy_48();break;
    default:
        if(CPU.TrapBadOps)  Trap_Bad_Ops(" FD ", I, CPU.PC.W-2);
  }
#undef XX
}

void ExecOneInstruction_48(void)
{
  register byte I;
  register pair J;
  u32 RunToCycles = CPU.TStates+1;

  I=OpZ80(CPU.PC.W++);

  /* R register incremented on each M1 cycle */
  INCR(1);

  /* Interpret opcode */
  switch(I)
  {
#include "Codes.h"
    case PFX_CB: CodesCB_Speccy_48();break;
    case PFX_ED: CodesED_Speccy_48();break;
    case PFX_FD: CodesFD_Speccy_48();break;
    case PFX_DD: CodesDD_Speccy_48();break;
  }
}

// ------------------------------------------------------------------------
// The Enable Interrupt is delayed 1 M1 instruction and we must also check
// to see if we are within the 32 TState period where the ZX Spectrum ULA
// would hold the Interrupt Request pulse...
// ------------------------------------------------------------------------
void EI_Enable_48(void)
{
   ExecOneInstruction_48();
   CPU.IFF=(CPU.IFF&~IFF_EI)|IFF_1;
   if (CPU.IRequest != INT_NONE)
   {
       if ((CPU.TStates - CPU.TStates_IRequest) < 32) IntZ80(&CPU, CPU.IRequest); // Fire the interrupt
       else CPU.IRequest = INT_NONE; // We missed the interrupt...
   }
}

void ExecZ80_Speccy_48(u32 RunToCycles)
{
  register byte I;
  register pair J;

  while (CPU.TStates < RunToCycles)
  {
      I=OpZ80(CPU.PC.W++);

      /* R register incremented on each M1 cycle */
      INCR(1);

      /* Interpret opcode */
      switch(I)
      {
#include "Codes.h"
        case PFX_CB: CodesCB_Speccy_48();break;
        case PFX_ED: CodesED_Speccy_48();break;
        case PFX_FD: CodesFD_Speccy_48();break;
        case PFX_DD: CodesDD_Speccy_48();break;
      }
  }
}

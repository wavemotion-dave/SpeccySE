#ifndef _Z80_INTERFACE_H_
#define _Z80_INTERFACE_H_

#include <nds.h>

#include "./cz80/Z80.h"

#define word u16
#define byte u8

extern Z80 CPU;

extern void Trap_Bad_Ops(char *prefix, byte I, word W);

#endif

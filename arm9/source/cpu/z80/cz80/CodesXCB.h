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
/**                         CodesXCB.h                      **/
/**                                                         **/
/** This file contains implementation for FD/DD-CB tables   **/
/** of Z80 commands. It is included from Z80.c.             **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994-2021                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/

case BIT0_B: case BIT0_C: case BIT0_D: case BIT0_E:
case BIT0_H: case BIT0_L: case BIT0_A:
case BIT0_xHL: T_INC(1); I=RdZ80(J.W); T_INC(1); M_BIT(0,I);break;  // 20:44354

case BIT1_B: case BIT1_C: case BIT1_D: case BIT1_E:
case BIT1_H: case BIT1_L: case BIT1_A:
case BIT1_xHL: T_INC(1); I=RdZ80(J.W); T_INC(1); M_BIT(1,I);break;  // 20:44354

case BIT2_B: case BIT2_C: case BIT2_D: case BIT2_E:
case BIT2_H: case BIT2_L: case BIT2_A:
case BIT2_xHL: T_INC(1); I=RdZ80(J.W); T_INC(1); M_BIT(2,I);break;  // 20:44354

case BIT3_B: case BIT3_C: case BIT3_D: case BIT3_E:
case BIT3_H: case BIT3_L: case BIT3_A:
case BIT3_xHL: T_INC(1); I=RdZ80(J.W); T_INC(1); M_BIT(3,I);break;  // 20:44354

case BIT4_B: case BIT4_C: case BIT4_D: case BIT4_E:
case BIT4_H: case BIT4_L: case BIT4_A:
case BIT4_xHL: T_INC(1); I=RdZ80(J.W); T_INC(1); M_BIT(4,I);break;  // 20:44354

case BIT5_B: case BIT5_C: case BIT5_D: case BIT5_E:
case BIT5_H: case BIT5_L: case BIT5_A:
case BIT5_xHL: T_INC(1); I=RdZ80(J.W); T_INC(1); M_BIT(5,I);break;  // 20:44354

case BIT6_B: case BIT6_C: case BIT6_D: case BIT6_E:
case BIT6_H: case BIT6_L: case BIT6_A:
case BIT6_xHL: T_INC(1); I=RdZ80(J.W); T_INC(1); M_BIT(6,I);break;  // 20:44354

case BIT7_B: case BIT7_C: case BIT7_D: case BIT7_E:
case BIT7_H: case BIT7_L: case BIT7_A:
case BIT7_xHL: T_INC(1); I=RdZ80(J.W); T_INC(1); M_BIT(7,I);break;  // 20:44354

case RLC_xHL: T_INC(1);  I=RdZ80(J.W);   M_RLC(I);   T_INC(1);   WrZ80(J.W,I);break;  //23:443543
case RRC_xHL: T_INC(1);  I=RdZ80(J.W);   M_RRC(I);   T_INC(1);   WrZ80(J.W,I);break;  //23:443543
case RL_xHL:  T_INC(1);  I=RdZ80(J.W);   M_RL(I);    T_INC(1);   WrZ80(J.W,I);break;  //23:443543
case RR_xHL:  T_INC(1);  I=RdZ80(J.W);   M_RR(I);    T_INC(1);   WrZ80(J.W,I);break;  //23:443543
case SLA_xHL: T_INC(1);  I=RdZ80(J.W);   M_SLA(I);   T_INC(1);   WrZ80(J.W,I);break;  //23:443543
case SRA_xHL: T_INC(1);  I=RdZ80(J.W);   M_SRA(I);   T_INC(1);   WrZ80(J.W,I);break;  //23:443543
case SLL_xHL: T_INC(1);  I=RdZ80(J.W);   M_SLL(I);   T_INC(1);   WrZ80(J.W,I);break;  //23:443543
case SRL_xHL: T_INC(1);  I=RdZ80(J.W);   M_SRL(I);   T_INC(1);   WrZ80(J.W,I);break;  //23:443543

case RES0_xHL: T_INC(1);  I=RdZ80(J.W);  M_RES(0,I); T_INC(1);   WrZ80(J.W,I);break;  //23:443543
case RES1_xHL: T_INC(1);  I=RdZ80(J.W);  M_RES(1,I); T_INC(1);   WrZ80(J.W,I);break;  //23:443543
case RES2_xHL: T_INC(1);  I=RdZ80(J.W);  M_RES(2,I); T_INC(1);   WrZ80(J.W,I);break;  //23:443543
case RES3_xHL: T_INC(1);  I=RdZ80(J.W);  M_RES(3,I); T_INC(1);   WrZ80(J.W,I);break;  //23:443543
case RES4_xHL: T_INC(1);  I=RdZ80(J.W);  M_RES(4,I); T_INC(1);   WrZ80(J.W,I);break;  //23:443543
case RES5_xHL: T_INC(1);  I=RdZ80(J.W);  M_RES(5,I); T_INC(1);   WrZ80(J.W,I);break;  //23:443543
case RES6_xHL: T_INC(1);  I=RdZ80(J.W);  M_RES(6,I); T_INC(1);   WrZ80(J.W,I);break;  //23:443543  
case RES7_xHL: T_INC(1);  I=RdZ80(J.W);  M_RES(7,I); T_INC(1);   WrZ80(J.W,I);break;  //23:443543
                                       
case SET0_xHL: T_INC(1);  I=RdZ80(J.W);  M_SET(0,I); T_INC(1);   WrZ80(J.W,I);break;  //23:443543
case SET1_xHL: T_INC(1);  I=RdZ80(J.W);  M_SET(1,I); T_INC(1);   WrZ80(J.W,I);break;  //23:443543
case SET2_xHL: T_INC(1);  I=RdZ80(J.W);  M_SET(2,I); T_INC(1);   WrZ80(J.W,I);break;  //23:443543
case SET3_xHL: T_INC(1);  I=RdZ80(J.W);  M_SET(3,I); T_INC(1);   WrZ80(J.W,I);break;  //23:443543
case SET4_xHL: T_INC(1);  I=RdZ80(J.W);  M_SET(4,I); T_INC(1);   WrZ80(J.W,I);break;  //23:443543
case SET5_xHL: T_INC(1);  I=RdZ80(J.W);  M_SET(5,I); T_INC(1);   WrZ80(J.W,I);break;  //23:443543
case SET6_xHL: T_INC(1);  I=RdZ80(J.W);  M_SET(6,I); T_INC(1);   WrZ80(J.W,I);break;  //23:443543
case SET7_xHL: T_INC(1);  I=RdZ80(J.W);  M_SET(7,I); T_INC(1);   WrZ80(J.W,I);break;  //23:443543

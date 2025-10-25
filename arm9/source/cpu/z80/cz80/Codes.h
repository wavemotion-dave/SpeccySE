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
/**                          Codes.h                        **/
/**                                                         **/
/** This file contains implementation for the main table of **/
/** Z80 commands. It is included from Z80.c.                **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994-2021                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/


// ----------------------------------------------------------------------------------------
// For the jump instructions, the Cycle[] table builds in assuming the jump WILL be taken
// which is true about 95% of the time. If the jump is not taken, we compensate ICount.
// ----------------------------------------------------------------------------------------
case JR_NZ:   if(CPU.AF.B.l&Z_FLAG) {J_ADJ; RdZ80(CPU.PC.W++);} else { M_JR; T_INC(5); } break;  //12:435, 7:43
case JR_NC:   if(CPU.AF.B.l&C_FLAG) {J_ADJ; RdZ80(CPU.PC.W++);} else { M_JR; T_INC(5); } break;  //12:435, 7:43
case JR_Z:    if(CPU.AF.B.l&Z_FLAG) { M_JR; T_INC(5); } else {J_ADJ; RdZ80(CPU.PC.W++);} break;  //12:435, 7:43
case JR_C:    if(CPU.AF.B.l&C_FLAG) { M_JR; T_INC(5); } else {J_ADJ; RdZ80(CPU.PC.W++);} break;  //12:435, 7:43

case JP_NZ:   if(CPU.AF.B.l&Z_FLAG) { PhantomRdZ80(CPU.PC.W);PhantomRdZ80(CPU.PC.W); CPU.PC.W+=2; } else { M_JP; } break;    //10:433
case JP_NC:   if(CPU.AF.B.l&C_FLAG) { PhantomRdZ80(CPU.PC.W);PhantomRdZ80(CPU.PC.W); CPU.PC.W+=2; } else { M_JP; } break;    //10:433
case JP_PO:   if(CPU.AF.B.l&P_FLAG) { PhantomRdZ80(CPU.PC.W);PhantomRdZ80(CPU.PC.W); CPU.PC.W+=2; } else { M_JP; } break;    //10:433
case JP_P:    if(CPU.AF.B.l&S_FLAG) { PhantomRdZ80(CPU.PC.W);PhantomRdZ80(CPU.PC.W); CPU.PC.W+=2; } else { M_JP; } break;    //10:433
case JP_Z:    if(CPU.AF.B.l&Z_FLAG) { M_JP; } else { PhantomRdZ80(CPU.PC.W);PhantomRdZ80(CPU.PC.W); CPU.PC.W+=2; } break;    //10:433
case JP_C:    if(CPU.AF.B.l&C_FLAG) { M_JP; } else { PhantomRdZ80(CPU.PC.W);PhantomRdZ80(CPU.PC.W); CPU.PC.W+=2; } break;    //10:433
case JP_PE:   if(CPU.AF.B.l&P_FLAG) { M_JP; } else { PhantomRdZ80(CPU.PC.W);PhantomRdZ80(CPU.PC.W); CPU.PC.W+=2; } break;    //10:433
case JP_M:    if(CPU.AF.B.l&S_FLAG) { M_JP; } else { PhantomRdZ80(CPU.PC.W);PhantomRdZ80(CPU.PC.W); CPU.PC.W+=2; } break;    //10:433

// -----------------------------------------------------------------------------------------
// For the RET instructions, the Cycle[] table builds in assuming the return will NOT be
// taken and so we must consume the additional cycles if the condition proves to be TRUE...
// -----------------------------------------------------------------------------------------
case RET_NZ:  T_INC(1); if(!(CPU.AF.B.l&Z_FLAG)) { R_ADJ;M_RET; } break;    //11:533, 5:5
case RET_NC:  T_INC(1); if(!(CPU.AF.B.l&C_FLAG)) { R_ADJ;M_RET; } break;    //11:533, 5:5
case RET_PO:  T_INC(1); if(!(CPU.AF.B.l&P_FLAG)) { R_ADJ;M_RET; } break;    //11:533, 5:5
case RET_P:   T_INC(1); if(!(CPU.AF.B.l&S_FLAG)) { R_ADJ;M_RET; } break;    //11:533, 5:5
case RET_Z:   T_INC(1); if(CPU.AF.B.l&Z_FLAG)    { R_ADJ;M_RET; } break;    //11:533, 5:5
case RET_C:   T_INC(1); if(CPU.AF.B.l&C_FLAG)    { R_ADJ;M_RET; } break;    //11:533, 5:5
case RET_PE:  T_INC(1); if(CPU.AF.B.l&P_FLAG)    { R_ADJ;M_RET; } break;    //11:533, 5:5
case RET_M:   T_INC(1); if(CPU.AF.B.l&S_FLAG)    { R_ADJ;M_RET; } break;    //11:533, 5:5

case CALL_NZ: if(CPU.AF.B.l&Z_FLAG) {PhantomRdZ80(CPU.PC.W);PhantomRdZ80(CPU.PC.W); CPU.PC.W+=2;} else { C_ADJ;M_CALL; } break;  //17:43433, 10:433
case CALL_NC: if(CPU.AF.B.l&C_FLAG) {PhantomRdZ80(CPU.PC.W);PhantomRdZ80(CPU.PC.W); CPU.PC.W+=2;} else { C_ADJ;M_CALL; } break;  //17:43433, 10:433
case CALL_PO: if(CPU.AF.B.l&P_FLAG) {PhantomRdZ80(CPU.PC.W);PhantomRdZ80(CPU.PC.W); CPU.PC.W+=2;} else { C_ADJ;M_CALL; } break;  //17:43433, 10:433
case CALL_P:  if(CPU.AF.B.l&S_FLAG) {PhantomRdZ80(CPU.PC.W);PhantomRdZ80(CPU.PC.W); CPU.PC.W+=2;} else { C_ADJ;M_CALL; } break;  //17:43433, 10:433
case CALL_Z:  if(CPU.AF.B.l&Z_FLAG) { C_ADJ;M_CALL; } else {PhantomRdZ80(CPU.PC.W);PhantomRdZ80(CPU.PC.W); CPU.PC.W+=2;} break;  //17:43433, 10:433
case CALL_C:  if(CPU.AF.B.l&C_FLAG) { C_ADJ;M_CALL; } else {PhantomRdZ80(CPU.PC.W);PhantomRdZ80(CPU.PC.W); CPU.PC.W+=2;} break;  //17:43433, 10:433
case CALL_PE: if(CPU.AF.B.l&P_FLAG) { C_ADJ;M_CALL; } else {PhantomRdZ80(CPU.PC.W);PhantomRdZ80(CPU.PC.W); CPU.PC.W+=2;} break;  //17:43433, 10:433
case CALL_M:  if(CPU.AF.B.l&S_FLAG) { C_ADJ;M_CALL; } else {PhantomRdZ80(CPU.PC.W);PhantomRdZ80(CPU.PC.W); CPU.PC.W+=2;} break;  //17:43433, 10:433

case ADD_B:    M_ADD(CPU.BC.B.h);break;  //4:4
case ADD_C:    M_ADD(CPU.BC.B.l);break;  //4:4
case ADD_D:    M_ADD(CPU.DE.B.h);break;  //4:4
case ADD_E:    M_ADD(CPU.DE.B.l);break;  //4:4
case ADD_H:    M_ADD(CPU.HL.B.h);break;  //4:4
case ADD_L:    M_ADD(CPU.HL.B.l);break;  //4:4
case ADD_A:    M_ADD(CPU.AF.B.h);break;  //4:4
case ADD_xHL:  I=RdZ80(CPU.HL.W);M_ADD(I);break;    //7:43
case ADD_BYTE: I=RdZ80(CPU.PC.W++);M_ADD(I);break;  //7:43

case SUB_B:    M_SUB(CPU.BC.B.h);break;  //4:4
case SUB_C:    M_SUB(CPU.BC.B.l);break;  //4:4
case SUB_D:    M_SUB(CPU.DE.B.h);break;  //4:4
case SUB_E:    M_SUB(CPU.DE.B.l);break;  //4:4
case SUB_H:    M_SUB(CPU.HL.B.h);break;  //4:4
case SUB_L:    M_SUB(CPU.HL.B.l);break;  //4:4
case SUB_A:    CPU.AF.B.h=0;CPU.AF.B.l=N_FLAG|Z_FLAG;break; //4:4
case SUB_xHL:  I=RdZ80(CPU.HL.W);M_SUB(I);break;    //7:43
case SUB_BYTE: I=RdZ80(CPU.PC.W++);M_SUB(I);break;  //7:43

case AND_B:    M_AND(CPU.BC.B.h);break;  //4:4
case AND_C:    M_AND(CPU.BC.B.l);break;  //4:4
case AND_D:    M_AND(CPU.DE.B.h);break;  //4:4
case AND_E:    M_AND(CPU.DE.B.l);break;  //4:4
case AND_H:    M_AND(CPU.HL.B.h);break;  //4:4
case AND_L:    M_AND(CPU.HL.B.l);break;  //4:4
case AND_A:    M_AND(CPU.AF.B.h);break;  //4:4
case AND_xHL:  I=RdZ80(CPU.HL.W);M_AND(I);break;    //7:43
case AND_BYTE: I=RdZ80(CPU.PC.W++);M_AND(I);break;  //7:43

case OR_B:     M_OR(CPU.BC.B.h);break;  //4:4
case OR_C:     M_OR(CPU.BC.B.l);break;  //4:4
case OR_D:     M_OR(CPU.DE.B.h);break;  //4:4
case OR_E:     M_OR(CPU.DE.B.l);break;  //4:4
case OR_H:     M_OR(CPU.HL.B.h);break;  //4:4
case OR_L:     M_OR(CPU.HL.B.l);break;  //4:4
case OR_A:     M_OR(CPU.AF.B.h);break;  //4:4
case OR_xHL:   I=RdZ80(CPU.HL.W);M_OR(I);break;    //7:43
case OR_BYTE:  I=RdZ80(CPU.PC.W++);M_OR(I);break;  //7:43

case ADC_B:    M_ADC(CPU.BC.B.h);break;  //4:4
case ADC_C:    M_ADC(CPU.BC.B.l);break;  //4:4
case ADC_D:    M_ADC(CPU.DE.B.h);break;  //4:4
case ADC_E:    M_ADC(CPU.DE.B.l);break;  //4:4
case ADC_H:    M_ADC(CPU.HL.B.h);break;  //4:4
case ADC_L:    M_ADC(CPU.HL.B.l);break;  //4:4
case ADC_A:    M_ADC(CPU.AF.B.h);break;  //4:4
case ADC_xHL:  I=RdZ80(CPU.HL.W);M_ADC(I);break;    //7:43
case ADC_BYTE: I=RdZ80(CPU.PC.W++);M_ADC(I);break;  //7:43

case SBC_B:    M_SBC(CPU.BC.B.h);break;  //4:4
case SBC_C:    M_SBC(CPU.BC.B.l);break;  //4:4
case SBC_D:    M_SBC(CPU.DE.B.h);break;  //4:4
case SBC_E:    M_SBC(CPU.DE.B.l);break;  //4:4
case SBC_H:    M_SBC(CPU.HL.B.h);break;  //4:4
case SBC_L:    M_SBC(CPU.HL.B.l);break;  //4:4
case SBC_A:    M_SBC(CPU.AF.B.h);break;  //4:4
case SBC_xHL:  I=RdZ80(CPU.HL.W);M_SBC(I);break;   //7:43
case SBC_BYTE: I=RdZ80(CPU.PC.W++);M_SBC(I);break; //7:43

case XOR_B:    M_XOR(CPU.BC.B.h);break;  //4:4
case XOR_C:    M_XOR(CPU.BC.B.l);break;  //4:4
case XOR_D:    M_XOR(CPU.DE.B.h);break;  //4:4
case XOR_E:    M_XOR(CPU.DE.B.l);break;  //4:4
case XOR_H:    M_XOR(CPU.HL.B.h);break;  //4:4
case XOR_L:    M_XOR(CPU.HL.B.l);break;  //4:4
case XOR_A:    CPU.AF.B.h=0;CPU.AF.B.l=P_FLAG|Z_FLAG;break; //4:4
case XOR_xHL:  I=RdZ80(CPU.HL.W);M_XOR(I);break;            //7:43
case XOR_BYTE: I=RdZ80(CPU.PC.W++);M_XOR(I);break;          //7:43

case CP_B:     M_CP(CPU.BC.B.h);break;  //4:4
case CP_C:     M_CP(CPU.BC.B.l);break;  //4:4
case CP_D:     M_CP(CPU.DE.B.h);break;  //4:4
case CP_E:     M_CP(CPU.DE.B.l);break;  //4:4
case CP_H:     M_CP(CPU.HL.B.h);break;  //4:4
case CP_L:     M_CP(CPU.HL.B.l);break;  //4:4
case CP_A:     CPU.AF.B.l=N_FLAG|Z_FLAG;break;    //4:4
case CP_xHL:   I=RdZ80(CPU.HL.W);M_CP(I);break;   //7:43
case CP_BYTE:  I=RdZ80(CPU.PC.W++);M_CP(I);break; //7:43
               
case LD_BC_WORD: M_LDWORD(BC);break;    //10:433
case LD_DE_WORD: M_LDWORD(DE);break;    //10:433
case LD_HL_WORD: M_LDWORD(HL);break;    //10:433
case LD_SP_WORD: M_LDWORD(SP);break;    //10:433

case LD_PC_HL: CPU.PC.W=CPU.HL.W;JumpZ80(CPU.PC.W);break; //4:4
case LD_SP_HL: CPU.SP.W=CPU.HL.W; T_INC(2); break; //6:6
case LD_A_xBC: CPU.AF.B.h=RdZ80(CPU.BC.W);break;   //7:43
case LD_A_xDE: CPU.AF.B.h=RdZ80(CPU.DE.W);break;   //7:43

case ADD_HL_BC:  M_ADDW(HL,BC);T_INC(7);break; //11:443
case ADD_HL_DE:  M_ADDW(HL,DE);T_INC(7);break; //11:443
case ADD_HL_HL:  M_ADDW(HL,HL);T_INC(7);break; //11:443
case ADD_HL_SP:  M_ADDW(HL,SP);T_INC(7);break; //11:443

case DEC_BC:   CPU.BC.W--;  T_INC(2); break;  // 6:6
case DEC_DE:   CPU.DE.W--;  T_INC(2); break;  // 6:6
case DEC_HL:   CPU.HL.W--;  T_INC(2); break;  // 6:6
case DEC_SP:   CPU.SP.W--;  T_INC(2); break;  // 6:6

case INC_BC:   CPU.BC.W++;  T_INC(2); break;  // 6:6
case INC_DE:   CPU.DE.W++;  T_INC(2); break;  // 6:6
case INC_HL:   CPU.HL.W++;  T_INC(2); break;  // 6:6
case INC_SP:   CPU.SP.W++;  T_INC(2); break;  // 6:6

case DEC_B:    M_DEC(CPU.BC.B.h); break; //4:4
case DEC_C:    M_DEC(CPU.BC.B.l); break; //4:4
case DEC_D:    M_DEC(CPU.DE.B.h); break; //4:4
case DEC_E:    M_DEC(CPU.DE.B.l); break; //4:4
case DEC_H:    M_DEC(CPU.HL.B.h); break; //4:4
case DEC_L:    M_DEC(CPU.HL.B.l); break; //4:4
case DEC_A:                              //4:4
    if (PatchLookup[CPU.PC.W]) (void)PatchLookup[CPU.PC.W](); // Tape pre-delay speedup...
    else { M_DEC(CPU.AF.B.h); }
    break;
case DEC_xHL:  I=RdZ80(CPU.HL.W);M_DEC(I);T_INC(1);WrZ80(CPU.HL.W,I);break; //11:443

case INC_B:    M_INC(CPU.BC.B.h); break; //4:4
case INC_C:    M_INC(CPU.BC.B.l); break; //4:4
case INC_D:    M_INC(CPU.DE.B.h); break; //4:4
case INC_E:    M_INC(CPU.DE.B.l); break; //4:4
case INC_H:    M_INC(CPU.HL.B.h); break; //4:4
case INC_L:    M_INC(CPU.HL.B.l); break; //4:4
case INC_A:    M_INC(CPU.AF.B.h); break; //4:4
case INC_xHL:  I=RdZ80(CPU.HL.W);M_INC(I);T_INC(1);WrZ80(CPU.HL.W,I);break;  //11:443

case RLCA: // 4:4
  I=CPU.AF.B.h&0x80? C_FLAG:0;
  CPU.AF.B.h=(CPU.AF.B.h<<1)|I;
  CPU.AF.B.l=(CPU.AF.B.l&~(C_FLAG|N_FLAG|H_FLAG))|I;
  break;
case RLA: // 4:4
  I=CPU.AF.B.h&0x80? C_FLAG:0;
  CPU.AF.B.h=(CPU.AF.B.h<<1)|(CPU.AF.B.l&C_FLAG);
  CPU.AF.B.l=(CPU.AF.B.l&~(C_FLAG|N_FLAG|H_FLAG))|I;
  break;
case RRCA: // 4:4
  I=CPU.AF.B.h&0x01;
  CPU.AF.B.h=(CPU.AF.B.h>>1)|(I? 0x80:0);
  CPU.AF.B.l=(CPU.AF.B.l&~(C_FLAG|N_FLAG|H_FLAG))|I; 
  break;
case RRA: // 4:4
  I=CPU.AF.B.h&0x01;
  CPU.AF.B.h=(CPU.AF.B.h>>1)|(CPU.AF.B.l&C_FLAG? 0x80:0);
  CPU.AF.B.l=(CPU.AF.B.l&~(C_FLAG|N_FLAG|H_FLAG))|I;
  break;

case RST00:    T_INC(1); M_RST(0x0000);break;  //11:533
case RST08:    T_INC(1); M_RST(0x0008);break;  //11:533
case RST10:    T_INC(1); M_RST(0x0010);break;  //11:533
case RST18:    T_INC(1); M_RST(0x0018);break;  //11:533
case RST20:    T_INC(1); M_RST(0x0020);break;  //11:533
case RST28:    T_INC(1); M_RST(0x0028);break;  //11:533
case RST30:    T_INC(1); M_RST(0x0030);break;  //11:533
case RST38:    T_INC(1); M_RST(0x0038);break;  //11:533

case PUSH_BC:  T_INC(1); M_PUSH(BC);break;    //11:533
case PUSH_DE:  T_INC(1); M_PUSH(DE);break;    //11:533
case PUSH_HL:  T_INC(1); M_PUSH(HL);break;    //11:533
case PUSH_AF:  T_INC(1); M_PUSH(AF);break;    //11:533

case POP_BC:   M_POP(BC);break;    //10:433
case POP_DE:   M_POP(DE);break;    //10:433
case POP_HL:   M_POP(HL);break;    //10:433
case POP_AF:   M_POP(AF);break;    //10:433

case DJNZ:  // 13:535, 8:53
  if (PatchLookup[CPU.PC.W]) (void)PatchLookup[CPU.PC.W](); // Tape pre-load speedup...
  T_INC(1);  
  if(--CPU.BC.B.h) { M_JR; T_INC(5); } else {J_ADJ; PhantomRdZ80(CPU.PC.W); CPU.PC.W++;} break;

case JP:   M_JP; break;                                     //10:433
case JR:   M_JR; T_INC(5); break;                           //12:435
case CALL: M_CALL; break;                                   //17:43433
case RET:  M_RET; break;                                    //10:433
case SCF:  S(C_FLAG);R(N_FLAG|H_FLAG);break;                //4:4
case CPL:  CPU.AF.B.h=~CPU.AF.B.h;S(N_FLAG|H_FLAG);break;   //4:4
case NOP:  break;                                           //4:4
case OUTA: I=RdZ80(CPU.PC.W++);OutZ80(I|(CPU.AF.W&0xFF00),CPU.AF.B.h);break; //11:434
case INA:  I=RdZ80(CPU.PC.W++);CPU.AF.B.h=InZ80(I|(CPU.AF.W&0xFF00));break;  //11:434

case HALT: //4:4
  CPU.TStates = RunToCycles;  // We're just waiting for an interrupt... so just skip ahead. This is often how a ZX game waits for the next frame.
  CPU.PC.W--;
  CPU.IFF|=IFF_HALT;
  break;

case DI: //4:4
  CPU.IFF&=~(IFF_1|IFF_2|IFF_EI);
  break;

case EI: //4:4
  if(!(CPU.IFF&(IFF_1|IFF_EI)))
  {
    CPU.IFF|=IFF_2|IFF_EI;
    EI_Enable();
  }
  break;

case CCF: //4:4
  CPU.AF.B.l^=C_FLAG;R(N_FLAG|H_FLAG);
  CPU.AF.B.l|=CPU.AF.B.l&C_FLAG? 0:H_FLAG;
  break;

case EXX: //4:4
  J.W=CPU.BC.W;CPU.BC.W=CPU.BC1.W;CPU.BC1.W=J.W;
  J.W=CPU.DE.W;CPU.DE.W=CPU.DE1.W;CPU.DE1.W=J.W;
  J.W=CPU.HL.W;CPU.HL.W=CPU.HL1.W;CPU.HL1.W=J.W;
  break;

case EX_DE_HL: J.W=CPU.DE.W;CPU.DE.W=CPU.HL.W;CPU.HL.W=J.W;break;     //4:4
case EX_AF_AF: J.W=CPU.AF.W;CPU.AF.W=CPU.AF1.W;CPU.AF1.W=J.W;break;   //4:4
  
case LD_B_B:   CPU.BC.B.h=CPU.BC.B.h;break; //4:4
case LD_C_B:   CPU.BC.B.l=CPU.BC.B.h;break; //4:4
case LD_D_B:   CPU.DE.B.h=CPU.BC.B.h;break; //4:4
case LD_E_B:   CPU.DE.B.l=CPU.BC.B.h;break; //4:4
case LD_H_B:   CPU.HL.B.h=CPU.BC.B.h;break; //4:4
case LD_L_B:   CPU.HL.B.l=CPU.BC.B.h;break; //4:4
case LD_A_B:   CPU.AF.B.h=CPU.BC.B.h;break; //4:4
case LD_xHL_B: WrZ80(CPU.HL.W,CPU.BC.B.h);break;  //7:43

case LD_B_C:   CPU.BC.B.h=CPU.BC.B.l;break; //4:4
case LD_C_C:   CPU.BC.B.l=CPU.BC.B.l;break; //4:4
case LD_D_C:   CPU.DE.B.h=CPU.BC.B.l;break; //4:4
case LD_E_C:   CPU.DE.B.l=CPU.BC.B.l;break; //4:4
case LD_H_C:   CPU.HL.B.h=CPU.BC.B.l;break; //4:4
case LD_L_C:   CPU.HL.B.l=CPU.BC.B.l;break; //4:4
case LD_A_C:   CPU.AF.B.h=CPU.BC.B.l;break; //4:4
case LD_xHL_C: WrZ80(CPU.HL.W,CPU.BC.B.l);break;  //7:43

case LD_B_D:   CPU.BC.B.h=CPU.DE.B.h;break; //4:4
case LD_C_D:   CPU.BC.B.l=CPU.DE.B.h;break; //4:4
case LD_D_D:   CPU.DE.B.h=CPU.DE.B.h;break; //4:4
case LD_E_D:   CPU.DE.B.l=CPU.DE.B.h;break; //4:4
case LD_H_D:   CPU.HL.B.h=CPU.DE.B.h;break; //4:4
case LD_L_D:   CPU.HL.B.l=CPU.DE.B.h;break; //4:4
case LD_A_D:   CPU.AF.B.h=CPU.DE.B.h;break; //4:4
case LD_xHL_D: WrZ80(CPU.HL.W,CPU.DE.B.h);break;  //7:43

case LD_B_E:   CPU.BC.B.h=CPU.DE.B.l;break; //4:4
case LD_C_E:   CPU.BC.B.l=CPU.DE.B.l;break; //4:4
case LD_D_E:   CPU.DE.B.h=CPU.DE.B.l;break; //4:4
case LD_E_E:   CPU.DE.B.l=CPU.DE.B.l;break; //4:4
case LD_H_E:   CPU.HL.B.h=CPU.DE.B.l;break; //4:4
case LD_L_E:   CPU.HL.B.l=CPU.DE.B.l;break; //4:4
case LD_A_E:   CPU.AF.B.h=CPU.DE.B.l;break; //4:4
case LD_xHL_E: WrZ80(CPU.HL.W,CPU.DE.B.l);break; //7:43

case LD_B_H:   CPU.BC.B.h=CPU.HL.B.h;break; //4:4
case LD_C_H:   CPU.BC.B.l=CPU.HL.B.h;break; //4:4
case LD_D_H:   CPU.DE.B.h=CPU.HL.B.h;break; //4:4
case LD_E_H:   CPU.DE.B.l=CPU.HL.B.h;break; //4:4
case LD_H_H:   CPU.HL.B.h=CPU.HL.B.h;break; //4:4
case LD_L_H:   CPU.HL.B.l=CPU.HL.B.h;break; //4:4
case LD_A_H:   CPU.AF.B.h=CPU.HL.B.h;break; //4:4
case LD_xHL_H: WrZ80(CPU.HL.W,CPU.HL.B.h);break; //7:43

case LD_B_L:   CPU.BC.B.h=CPU.HL.B.l;break; //4:4
case LD_C_L:   CPU.BC.B.l=CPU.HL.B.l;break; //4:4
case LD_D_L:   CPU.DE.B.h=CPU.HL.B.l;break; //4:4
case LD_E_L:   CPU.DE.B.l=CPU.HL.B.l;break; //4:4
case LD_H_L:   CPU.HL.B.h=CPU.HL.B.l;break; //4:4
case LD_L_L:   CPU.HL.B.l=CPU.HL.B.l;break; //4:4
case LD_A_L:   CPU.AF.B.h=CPU.HL.B.l;break; //4:4
case LD_xHL_L: WrZ80(CPU.HL.W,CPU.HL.B.l);break; //7:43

case LD_B_A:   CPU.BC.B.h=CPU.AF.B.h;break;  //4:4
case LD_C_A:   CPU.BC.B.l=CPU.AF.B.h;break;  //4:4
case LD_D_A:   CPU.DE.B.h=CPU.AF.B.h;break;  //4:4
case LD_E_A:   CPU.DE.B.l=CPU.AF.B.h;break;  //4:4
case LD_H_A:   CPU.HL.B.h=CPU.AF.B.h;break;  //4:4
case LD_L_A:   CPU.HL.B.l=CPU.AF.B.h;break;  //4:4
case LD_A_A:   CPU.AF.B.h=CPU.AF.B.h;break;  //4:4
case LD_xHL_A: WrZ80(CPU.HL.W,CPU.AF.B.h);break; //7:43

case LD_xBC_A: WrZ80(CPU.BC.W,CPU.AF.B.h);break; //7:43
case LD_xDE_A: WrZ80(CPU.DE.W,CPU.AF.B.h);break; //7:43

case LD_B_xHL:    CPU.BC.B.h=RdZ80(CPU.HL.W);break; //7:43
case LD_C_xHL:    CPU.BC.B.l=RdZ80(CPU.HL.W);break; //7:43
case LD_D_xHL:    CPU.DE.B.h=RdZ80(CPU.HL.W);break; //7:43
case LD_E_xHL:    CPU.DE.B.l=RdZ80(CPU.HL.W);break; //7:43
case LD_H_xHL:    CPU.HL.B.h=RdZ80(CPU.HL.W);break; //7:43
case LD_L_xHL:    CPU.HL.B.l=RdZ80(CPU.HL.W);break; //7:43
case LD_A_xHL:    CPU.AF.B.h=RdZ80(CPU.HL.W);break; //7:43

case LD_B_BYTE:   CPU.BC.B.h=RdZ80(CPU.PC.W++);break; //7:43
case LD_C_BYTE:   CPU.BC.B.l=RdZ80(CPU.PC.W++);break; //7:43
case LD_D_BYTE:   CPU.DE.B.h=RdZ80(CPU.PC.W++);break; //7:43
case LD_E_BYTE:   CPU.DE.B.l=RdZ80(CPU.PC.W++);break; //7:43
case LD_H_BYTE:   CPU.HL.B.h=RdZ80(CPU.PC.W++);break; //7:43
case LD_L_BYTE:   CPU.HL.B.l=RdZ80(CPU.PC.W++);break; //7:43
case LD_A_BYTE:   CPU.AF.B.h=RdZ80(CPU.PC.W++);break; //7:43
case LD_xHL_BYTE: WrZ80(CPU.HL.W,RdZ80(CPU.PC.W++));break; //10:433

case LD_xWORD_HL:           //16:43333
  J.B.l=RdZ80(CPU.PC.W++);
  J.B.h=RdZ80(CPU.PC.W++);
  WrZ80(J.W++,CPU.HL.B.l);
  WrZ80(J.W,CPU.HL.B.h);
  break;

case LD_HL_xWORD:           //16:43333
  J.B.l=RdZ80(CPU.PC.W++);
  J.B.h=RdZ80(CPU.PC.W++);
  CPU.HL.B.l=RdZ80(J.W++);
  CPU.HL.B.h=RdZ80(J.W);
  break;

case LD_A_xWORD:            //13:4333
  J.B.l=RdZ80(CPU.PC.W++);
  J.B.h=RdZ80(CPU.PC.W++); 
  CPU.AF.B.h=RdZ80(J.W);
  break;

case LD_xWORD_A:            //13:4333
  J.B.l=RdZ80(CPU.PC.W++);
  J.B.h=RdZ80(CPU.PC.W++);
  WrZ80(J.W,CPU.AF.B.h);
  break;

case EX_HL_xSP:             //19:43435
  J.B.l=RdZ80(CPU.SP.W);WrZ80(CPU.SP.W++,CPU.HL.B.l);T_INC(1);
  J.B.h=RdZ80(CPU.SP.W);WrZ80(CPU.SP.W--,CPU.HL.B.h);T_INC(2);
  CPU.HL.W=J.W;
  break;

case DAA:                   //4:4
  J.W=CPU.AF.B.h;
  if(CPU.AF.B.l&C_FLAG) J.W|=256;
  if(CPU.AF.B.l&H_FLAG) J.W|=512;
  if(CPU.AF.B.l&N_FLAG) J.W|=1024;
  CPU.AF.W=DAATable[J.W];
  break;

default:
  if(CPU.TrapBadOps) Trap_Bad_Ops("Z80", I, CPU.PC.W-1);
  break;

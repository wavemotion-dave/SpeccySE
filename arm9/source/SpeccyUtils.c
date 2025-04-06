// =====================================================================================
// Copyright (c) 2025 Dave Bernazzani (wavemotion-dave)
//
// Copying and distribution of this emulator, its source code and associated
// readme files, with or without modification, are permitted in any medium without
// royalty provided this copyright notice is used and wavemotion-dave and Marat
// Fayzullin (Z80 core) are thanked profusely.
//
// The SpeccyDS emulator is offered as-is, without any warranty. Please see readme.md
// =====================================================================================
#include <nds.h>

#include <stdlib.h>
#include <stdio.h>
#include <fat.h>
#include <dirent.h>
#include <unistd.h>
#include <maxmod9.h>

#include "SpeccyDS.h"
#include "SpeccyUtils.h"
#include "topscreen.h"
#include "mainmenu.h"
#include "soundbank.h"
#include "pdev_tbg0.h"
#include "pdev_bg0.h"
#include "printf.h"

#include "CRC32.h"
#include "printf.h"

int         countCV=0;
int         ucGameAct=0;
int         ucGameChoice = -1;
FISpeccy    gpFic[MAX_ROMS];
char        szName[256];
char        szFile[256];
u32         file_size = 0;
char        strBuf[40];

struct Config_t AllConfigs[MAX_CONFIGS];
struct Config_t myConfig __attribute((aligned(4))) __attribute__((section(".dtcm")));
struct GlobalConfig_t myGlobalConfig;
extern u32 file_crc;

u16 *pVidFlipBuf  = (u16*) (0x06000000);    // Video flipping buffer

// -----------------------------------------------------------------------
// Used by our system to map into 8K memory chunks which allows for very
// rapid banking of memory - mostly useful for the ZX Spectrum 128K
// -----------------------------------------------------------------------
u8 *MemoryMap[8]        __attribute__((section(".dtcm"))) = {0,0,0,0,0,0,0,0};

// ------------------------------------------------------------------------
// The Z80 Processor! Put the entire CPU state into fast memory for speed!
// ------------------------------------------------------------------------
Z80 CPU __attribute__((section(".dtcm")));

u32 file_crc __attribute__((section(".dtcm")))  = 0x00000000;  // Our global file CRC32 to uniquiely identify this game

// -----------------------------------------------------------
// The AY sound chip is used for the ZX 128K machines
// -----------------------------------------------------------
AY38910 myAY   __attribute__((section(".dtcm")));

u16 JoyState   __attribute__((section(".dtcm"))) = 0;           // Joystick State and Key Bits

u8 option_table=0;

const char szKeyName[MAX_KEY_OPTIONS][16] = {
  "KEMPSTON UP",
  "KEMPSTON DOWN",
  "KEMPSTON LEFT",
  "KEMPSTON RIGHT",
  "KEMPSTON FIRE",

  "KEYBOARD A", //5
  "KEYBOARD B",
  "KEYBOARD C",
  "KEYBOARD D",
  "KEYBOARD E",
  "KEYBOARD F",
  "KEYBOARD G",
  "KEYBOARD H",
  "KEYBOARD I",
  "KEYBOARD J",
  "KEYBOARD K",
  "KEYBOARD L",
  "KEYBOARD M",
  "KEYBOARD N",
  "KEYBOARD O",
  "KEYBOARD P",
  "KEYBOARD Q",
  "KEYBOARD R",
  "KEYBOARD S",
  "KEYBOARD T",
  "KEYBOARD U",
  "KEYBOARD V",
  "KEYBOARD W",
  "KEYBOARD X",
  "KEYBOARD Y",
  "KEYBOARD Z", // 30

  "KEYBOARD 1", // 31
  "KEYBOARD 2",
  "KEYBOARD 3",
  "KEYBOARD 4",
  "KEYBOARD 5",
  "KEYBOARD 6",
  "KEYBOARD 7",
  "KEYBOARD 8",
  "KEYBOARD 9",
  "KEYBOARD 0", // 40

  "KEYBOARD SHIFT",
  "KEYBOARD SYMBOL",
  "KEYBOARD SPACE",
  "KEYBOARD RETURN", // 44
};


/*********************************************************************************
 * Show A message with YES / NO
 ********************************************************************************/
u8 showMessage(char *szCh1, char *szCh2) 
{
  u16 iTx, iTy;
  u8 uRet=ID_SHM_CANCEL;
  u8 ucGau=0x00, ucDro=0x00,ucGauS=0x00, ucDroS=0x00, ucCho = ID_SHM_YES;

  BottomScreenOptions();

  DSPrint(16-strlen(szCh1)/2,10,6,szCh1);
  DSPrint(16-strlen(szCh2)/2,12,6,szCh2);
  DSPrint(8,14,6,("> YES <"));
  DSPrint(20,14,6,("  NO   "));
  while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_A ))!=0);

  while (uRet == ID_SHM_CANCEL)
  {
    WAITVBL;
    if (keysCurrent() & KEY_TOUCH) {
      touchPosition touch;
      touchRead(&touch);
      iTx = touch.px;
      iTy = touch.py;
      if ( (iTx>8*8) && (iTx<8*8+7*8) && (iTy>14*8-4) && (iTy<15*8+4) ) {
        if (!ucGauS) {
          DSPrint(8,14,6,("> YES <"));
          DSPrint(20,14,6,("  NO   "));
          ucGauS = 1;
          if (ucCho == ID_SHM_YES) {
            uRet = ucCho;
          }
          else {
            ucCho  = ID_SHM_YES;
          }
        }
      }
      else
        ucGauS = 0;
      if ( (iTx>20*8) && (iTx<20*8+7*8) && (iTy>14*8-4) && (iTy<15*8+4) ) {
        if (!ucDroS) {
          DSPrint(8,14,6,("  YES  "));
          DSPrint(20,14,6,("> NO  <"));
          ucDroS = 1;
          if (ucCho == ID_SHM_NO) {
            uRet = ucCho;
          }
          else {
            ucCho = ID_SHM_NO;
          }
        }
      }
      else
        ucDroS = 0;
    }
    else {
      ucDroS = 0;
      ucGauS = 0;
    }

    if (keysCurrent() & KEY_LEFT){
      if (!ucGau) {
        ucGau = 1;
        if (ucCho == ID_SHM_YES) {
          ucCho = ID_SHM_NO;
          DSPrint(8,14,6,("  YES  "));
          DSPrint(20,14,6,("> NO  <"));
        }
        else {
          ucCho  = ID_SHM_YES;
          DSPrint(8,14,6,("> YES <"));
          DSPrint(20,14,6,("  NO   "));
        }
        WAITVBL;
      }
    }
    else {
      ucGau = 0;
    }
    if (keysCurrent() & KEY_RIGHT) {
      if (!ucDro) {
        ucDro = 1;
        if (ucCho == ID_SHM_YES) {
          ucCho  = ID_SHM_NO;
          DSPrint(8,14,6,("  YES  "));
          DSPrint(20,14,6,("> NO  <"));
        }
        else {
          ucCho  = ID_SHM_YES;
          DSPrint(8,14,6,("> YES <"));
          DSPrint(20,14,6,("  NO   "));
        }
        WAITVBL;
      }
    }
    else {
      ucDro = 0;
    }
    if (keysCurrent() & KEY_A) {
      uRet = ucCho;
    }
  }
  while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_A ))!=0);

  BottomScreenKeyboard();

  return uRet;
}

void SpeccyDSModeNormal(void) {
  REG_BG3CNT = BG_BMP8_256x256;
  REG_BG3PA = (1<<8);
  REG_BG3PB = 0;
  REG_BG3PC = 0;
  REG_BG3PD = (1<<8);
  REG_BG3X = 0;
  REG_BG3Y = 0;
}

//*****************************************************************************
// Put the top screen in refocused bitmap mode
//*****************************************************************************
void SpeccyDSInitScreenUp(void) {
  videoSetMode(MODE_5_2D | DISPLAY_BG3_ACTIVE);
  vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
  vramSetBankB(VRAM_B_MAIN_SPRITE);
  SpeccyDSModeNormal();
}

/*********************************************************************************
 * Show The 14 games on the list to allow the user to choose a new game.
 ********************************************************************************/
static char szName2[40];
void dsDisplayFiles(u16 NoDebGame, u8 ucSel)
{
  u16 ucBcl,ucGame;
  u8 maxLen;

  DSPrint(31,5,0,(NoDebGame>0 ? "<" : " "));
  DSPrint(31,22,0,(NoDebGame+14<countCV ? ">" : " "));

  for (ucBcl=0;ucBcl<18; ucBcl++)
  {
    ucGame= ucBcl+NoDebGame;
    if (ucGame < countCV)
    {
      maxLen=strlen(gpFic[ucGame].szName);
      strcpy(szName,gpFic[ucGame].szName);
      if (maxLen>30) szName[30]='\0';
      if (gpFic[ucGame].uType == DIRECTORY)
      {
        szName[28] = 0; // Needs to be 2 chars shorter with brackets
        sprintf(szName2, "[%s]",szName);
        sprintf(szName,"%-30s",szName2);
        DSPrint(1,5+ucBcl,(ucSel == ucBcl ? 2 :  0),szName);
      }
      else
      {
        sprintf(szName,"%-30s",strupr(szName));
        DSPrint(1,5+ucBcl,(ucSel == ucBcl ? 2 : 0 ),szName);
      }
    }
    else
    {
        DSPrint(1,5+ucBcl,(ucSel == ucBcl ? 2 : 0 ),"                              ");
    }
  }
}


// -------------------------------------------------------------------------
// Standard qsort routine for the games - we sort all DIRECTORYory
// listings first and then a case-insenstive sort of all games.
// -------------------------------------------------------------------------
int Filescmp (const void *c1, const void *c2)
{
  FISpeccy *p1 = (FISpeccy *) c1;
  FISpeccy *p2 = (FISpeccy *) c2;

  if (p1->szName[0] == '.' && p2->szName[0] != '.')
      return -1;
  if (p2->szName[0] == '.' && p1->szName[0] != '.')
      return 1;
  if ((p1->uType == DIRECTORY) && !(p2->uType == DIRECTORY))
      return -1;
  if ((p2->uType == DIRECTORY) && !(p1->uType == DIRECTORY))
      return 1;
  return strcasecmp (p1->szName, p2->szName);
}

/*********************************************************************************
 * Find files (TAP/TZX/Z80/SNA) available - sort them for display.
 ********************************************************************************/
void speccyDSFindFiles(u8 bTapeOnly)
{
  u32 uNbFile;
  DIR *dir;
  struct dirent *pent;

  uNbFile=0;
  countCV=0;

  dir = opendir(".");
  while (((pent=readdir(dir))!=NULL) && (uNbFile<MAX_ROMS))
  {
    strcpy(szFile,pent->d_name);

    if(pent->d_type == DT_DIR)
    {
      if (!((szFile[0] == '.') && (strlen(szFile) == 1)))
      {
        // Do not include the [sav] DIRECTORYory
        if (strcasecmp(szFile, "sav") != 0)
        {
            strcpy(gpFic[uNbFile].szName,szFile);
            gpFic[uNbFile].uType = DIRECTORY;
            uNbFile++;
            countCV++;
        }
      }
    }
    else {
      if ((strlen(szFile)>4) && (strlen(szFile)<(MAX_ROM_NAME-4)) && (szFile[0] != '.') && (szFile[0] != '_'))  // For MAC don't allow files starting with an underscore
      {
        if (!bTapeOnly) // If we're loading tape files only, exclude .z80 and .sna snapshots
        {
            if ( (strcasecmp(strrchr(szFile, '.'), ".z80") == 0) )  {
              strcpy(gpFic[uNbFile].szName,szFile);
              gpFic[uNbFile].uType = SPECCY_FILE;
              uNbFile++;
              countCV++;
            }
            if ( (strcasecmp(strrchr(szFile, '.'), ".sna") == 0) )  {
              strcpy(gpFic[uNbFile].szName,szFile);
              gpFic[uNbFile].uType = SPECCY_FILE;
              uNbFile++;
              countCV++;
            }
            if ( (strcasecmp(strrchr(szFile, '.'), ".rom") == 0) )  {
              strcpy(gpFic[uNbFile].szName,szFile);
              gpFic[uNbFile].uType = SPECCY_FILE;
              uNbFile++;
              countCV++;
            }
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".tap") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = SPECCY_FILE;
          uNbFile++;
          countCV++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".tzx") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = SPECCY_FILE;
          uNbFile++;
          countCV++;
        }
      }
    }
  }
  closedir(dir);

  // ----------------------------------------------
  // If we found any files, go sort the list...
  // ----------------------------------------------
  if (countCV)
  {
    qsort (gpFic, countCV, sizeof(FISpeccy), Filescmp);
  }
}

// ----------------------------------------------------------------
// Let the user select a new game (rom) file and load it up!
// ----------------------------------------------------------------
u8 speccyDSLoadFile(u8 bTapeOnly)
{
  bool bDone=false;
  u16 ucHaut=0x00, ucBas=0x00,ucSHaut=0x00, ucSBas=0x00, romSelected= 0, firstRomDisplay=0,nbRomPerPage, uNbRSPage;
  s16 uLenFic=0, ucFlip=0, ucFlop=0;

  // Show the menu...
  while ((keysCurrent() & (KEY_TOUCH | KEY_START | KEY_SELECT | KEY_A | KEY_B))!=0);

  BottomScreenOptions();

  DSPrint(1,3,0,"A=LOAD 48K, B=EXIT, Y=128K");

  speccyDSFindFiles(bTapeOnly);

  ucGameChoice = -1;

  nbRomPerPage = (countCV>=18 ? 18 : countCV);
  uNbRSPage = (countCV>=5 ? 5 : countCV);

  if (ucGameAct>countCV-nbRomPerPage)
  {
    firstRomDisplay=countCV-nbRomPerPage;
    romSelected=ucGameAct-countCV+nbRomPerPage;
  }
  else
  {
    firstRomDisplay=ucGameAct;
    romSelected=0;
  }

  if (romSelected >= countCV) romSelected = 0; // Just start at the top

  dsDisplayFiles(firstRomDisplay,romSelected);

  // -----------------------------------------------------
  // Until the user selects a file or exits the menu...
  // -----------------------------------------------------
  while (!bDone)
  {
    if (keysCurrent() & KEY_UP)
    {
      if (!ucHaut)
      {
        ucGameAct = (ucGameAct>0 ? ucGameAct-1 : countCV-1);
        if (romSelected>uNbRSPage) { romSelected -= 1; }
        else {
          if (firstRomDisplay>0) { firstRomDisplay -= 1; }
          else {
            if (romSelected>0) { romSelected -= 1; }
            else {
              firstRomDisplay=countCV-nbRomPerPage;
              romSelected=nbRomPerPage-1;
            }
          }
        }
        ucHaut=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else {

        ucHaut++;
        if (ucHaut>10) ucHaut=0;
      }
      uLenFic=0; ucFlip=-50; ucFlop=0;
    }
    else
    {
      ucHaut = 0;
    }
    if (keysCurrent() & KEY_DOWN)
    {
      if (!ucBas) {
        ucGameAct = (ucGameAct< countCV-1 ? ucGameAct+1 : 0);
        if (romSelected<uNbRSPage-1) { romSelected += 1; }
        else {
          if (firstRomDisplay<countCV-nbRomPerPage) { firstRomDisplay += 1; }
          else {
            if (romSelected<nbRomPerPage-1) { romSelected += 1; }
            else {
              firstRomDisplay=0;
              romSelected=0;
            }
          }
        }
        ucBas=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else
      {
        ucBas++;
        if (ucBas>10) ucBas=0;
      }
      uLenFic=0; ucFlip=-50; ucFlop=0;
    }
    else {
      ucBas = 0;
    }

    // -------------------------------------------------------------
    // Left and Right on the D-Pad will scroll 1 page at a time...
    // -------------------------------------------------------------
    if (keysCurrent() & KEY_RIGHT)
    {
      if (!ucSBas)
      {
        ucGameAct = (ucGameAct< countCV-nbRomPerPage ? ucGameAct+nbRomPerPage : countCV-nbRomPerPage);
        if (firstRomDisplay<countCV-nbRomPerPage) { firstRomDisplay += nbRomPerPage; }
        else { firstRomDisplay = countCV-nbRomPerPage; }
        if (ucGameAct == countCV-nbRomPerPage) romSelected = 0;
        ucSBas=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else
      {
        ucSBas++;
        if (ucSBas>10) ucSBas=0;
      }
      uLenFic=0; ucFlip=-50; ucFlop=0;
    }
    else {
      ucSBas = 0;
    }

    // -------------------------------------------------------------
    // Left and Right on the D-Pad will scroll 1 page at a time...
    // -------------------------------------------------------------
    if (keysCurrent() & KEY_LEFT)
    {
      if (!ucSHaut)
      {
        ucGameAct = (ucGameAct> nbRomPerPage ? ucGameAct-nbRomPerPage : 0);
        if (firstRomDisplay>nbRomPerPage) { firstRomDisplay -= nbRomPerPage; }
        else { firstRomDisplay = 0; }
        if (ucGameAct == 0) romSelected = 0;
        if (romSelected > ucGameAct) romSelected = ucGameAct;
        ucSHaut=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else
      {
        ucSHaut++;
        if (ucSHaut>10) ucSHaut=0;
      }
      uLenFic=0; ucFlip=-50; ucFlop=0;
    }
    else {
      ucSHaut = 0;
    }

    // -------------------------------------------------------------------------
    // They B key will exit out of the ROM selection without picking a new game
    // -------------------------------------------------------------------------
    if ( keysCurrent() & KEY_B )
    {
      bDone=true;
      while (keysCurrent() & KEY_B);
    }

    // -------------------------------------------------------------------
    // Any of these keys will pick the current ROM and try to load it...
    // -------------------------------------------------------------------
    if (keysCurrent() & KEY_A || keysCurrent() & KEY_Y || keysCurrent() & KEY_X)
    {
      if (gpFic[ucGameAct].uType != DIRECTORY)
      {
        if (keysCurrent() & KEY_Y)
        {
            zx_force_128k_mode = 1;
        }
        else
        {
            zx_force_128k_mode = 0;
        }
        bDone=true;
        ucGameChoice = ucGameAct;
        WAITVBL;
      }
      else
      {
        chdir(gpFic[ucGameAct].szName);
        speccyDSFindFiles(bTapeOnly);
        ucGameAct = 0;
        nbRomPerPage = (countCV>=14 ? 14 : countCV);
        uNbRSPage = (countCV>=5 ? 5 : countCV);
        if (ucGameAct>countCV-nbRomPerPage) {
          firstRomDisplay=countCV-nbRomPerPage;
          romSelected=ucGameAct-countCV+nbRomPerPage;
        }
        else {
          firstRomDisplay=ucGameAct;
          romSelected=0;
        }
        dsDisplayFiles(firstRomDisplay,romSelected);
        while (keysCurrent() & KEY_A);
      }
    }

    // --------------------------------------------
    // If the filename is too long... scroll it.
    // --------------------------------------------
    if (strlen(gpFic[ucGameAct].szName) > 30)
    {
      ucFlip++;
      if (ucFlip >= 25)
      {
        ucFlip = 0;
        uLenFic++;
        if ((uLenFic+30)>strlen(gpFic[ucGameAct].szName))
        {
          ucFlop++;
          if (ucFlop >= 15)
          {
            uLenFic=0;
            ucFlop = 0;
          }
          else
            uLenFic--;
        }
        strncpy(szName,gpFic[ucGameAct].szName+uLenFic,30);
        szName[30] = '\0';
        DSPrint(1,5+romSelected,2,szName);
      }
    }
    swiWaitForVBlank();
  }

  // Wait for some key to be pressed before returning
  while ((keysCurrent() & (KEY_TOUCH | KEY_START | KEY_SELECT | KEY_A | KEY_B | KEY_R | KEY_L | KEY_UP | KEY_DOWN))!=0);

  return 0x01;
}


// ---------------------------------------------------------------------------
// Write out the SpeccyDS.DAT configuration file to capture the settings for
// each game.  This one file contains global settings ~1000 game settings.
// ---------------------------------------------------------------------------
void SaveConfig(bool bShow)
{
    FILE *fp;
    int slot = 0;

    if (bShow) DSPrint(6,23,0, (char*)"SAVING CONFIGURATION");

    // Set the global configuration version number...
    myGlobalConfig.config_ver = CONFIG_VER;

    // If there is a game loaded, save that into a slot... re-use the same slot if it exists
    myConfig.game_crc = file_crc;

    // Find the slot we should save into...
    for (slot=0; slot<MAX_CONFIGS; slot++)
    {
        if (AllConfigs[slot].game_crc == myConfig.game_crc)  // Got a match?!
        {
            break;
        }
        if (AllConfigs[slot].game_crc == 0x00000000)  // Didn't find it... use a blank slot...
        {
            break;
        }
    }

    // --------------------------------------------------------------------------
    // Copy our current game configuration to the main configuration database...
    // --------------------------------------------------------------------------
    if (myConfig.game_crc != 0x00000000)
    {
        memcpy(&AllConfigs[slot], &myConfig, sizeof(struct Config_t));
    }

    // --------------------------------------------------
    // Now save the config file out o the SD card...
    // --------------------------------------------------
    DIR* dir = opendir("/data");
    if (dir)
    {
        closedir(dir);  // DIRECTORYory exists.
    }
    else
    {
        mkdir("/data", 0777);   // Doesn't exist - make it...
    }
    fp = fopen("/data/SpeccyDS.DAT", "wb+");
    if (fp != NULL)
    {
        fwrite(&myGlobalConfig, sizeof(myGlobalConfig), 1, fp); // Write the global config
        fwrite(&AllConfigs, sizeof(AllConfigs), 1, fp);         // Write the array of all configurations
        fclose(fp);
    } else DSPrint(4,23,0, (char*)"ERROR SAVING CONFIG FILE");

    if (bShow)
    {
        WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
        DSPrint(4,23,0, (char*)"                        ");
    }
}

void MapPlayer1(void)
{
    myConfig.keymap[0]   = 0;    // NDS D-Pad mapped to Kempston Joystick UP
    myConfig.keymap[1]   = 1;    // NDS D-Pad mapped to Kempston Joystick DOWN
    myConfig.keymap[2]   = 2;    // NDS D-Pad mapped to Kempston Joystick LEFT
    myConfig.keymap[3]   = 3;    // NDS D-Pad mapped to Kempston Joystick RIGHT
    myConfig.keymap[4]   = 4;    // NDS A Button mapped to Kempston Fire

    myConfig.keymap[5]   = 0;    // NDS B Button mapped to Kempston Joystick UP (jump)
    myConfig.keymap[6]   = 43;   // NDS X Button mapped to SPACE
    myConfig.keymap[7]   = 44;   // NDS Y Button mapped to RETURN
    myConfig.keymap[8]   = 41;   // NDS R Button mapped to SHIFT
    myConfig.keymap[9]   = 42;   // NDS L Button mapped to SYMBOL
    myConfig.keymap[10]  = 40;   // NDS START mapped to '0'
    myConfig.keymap[11]  = 31;   // NDS SELECT mapped to '1'
}


void MapQAOP(void)
{
    myConfig.keymap[0]   = 21;   // Q
    myConfig.keymap[1]   =  5;   // A
    myConfig.keymap[2]   = 19;   // O
    myConfig.keymap[3]   = 20;   // P
    myConfig.keymap[4]   = 43;   // Space
    myConfig.keymap[5]   = 43;   // Space
    myConfig.keymap[6]   = 30;   // Z
    myConfig.keymap[7]   = 30;   // Z
    myConfig.keymap[8]   = 41;   // NDS R Button mapped to SHIFT
    myConfig.keymap[9]   = 42;   // NDS L Button mapped to SYMBOL
    myConfig.keymap[10]  = 40;   // NDS START mapped to '0'
    myConfig.keymap[11]  = 31;   // NDS SELECT mapped to '1'
}

void MapWASD(void)
{
    myConfig.keymap[0]   = 27;    // W
    myConfig.keymap[1]   =  5;    // A
    myConfig.keymap[2]   = 23;    // S
    myConfig.keymap[3]   =  8;    // D
    myConfig.keymap[4]   = 43;   // Space
    myConfig.keymap[5]   = 43;   // Space
    myConfig.keymap[6]   = 30;   // Z
    myConfig.keymap[7]   = 30;   // Z
    myConfig.keymap[8]   = 41;   // NDS R Button mapped to SHIFT
    myConfig.keymap[9]   = 42;   // NDS L Button mapped to SYMBOL
    myConfig.keymap[10]  = 40;   // NDS START mapped to '0'
    myConfig.keymap[11]  = 31;   // NDS SELECT mapped to '1'
}

void MapZXSpace(void)
{
    myConfig.keymap[0]   = 43;   // Space
    myConfig.keymap[1]   = 18;   // N
    myConfig.keymap[2]   = 30;   // Z
    myConfig.keymap[3]   = 28;   // X
    myConfig.keymap[4]   = 43;   // Space
    myConfig.keymap[5]   = 43;   // Space
    myConfig.keymap[6]   = 44;   // Return
    myConfig.keymap[7]   = 44;   // Return
    myConfig.keymap[8]   = 41;   // NDS R Button mapped to SHIFT
    myConfig.keymap[9]   = 42;   // NDS L Button mapped to SYMBOL
    myConfig.keymap[10]  = 40;   // NDS START mapped to '0'
    myConfig.keymap[11]  = 31;   // NDS SELECT mapped to '1'
}


// 6 (left), 7 (right), 8 (down), 9 (up) and 0 (fire) for Sinclair 1
// 1 (left), 2 (right), 3 (down), 4 (up) and 5 (fire) for Sinclair 2
void Sinclair1(void)
{
    myConfig.keymap[0]   = 39;   // UP
    myConfig.keymap[1]   = 38;   // DOWN
    myConfig.keymap[2]   = 36;   // LEFT
    myConfig.keymap[3]   = 37;   // RIGHT
    myConfig.keymap[4]   = 40;   // FIRE
    myConfig.keymap[5]   = 43;   // Space
    myConfig.keymap[6]   = 43;   // Space
    myConfig.keymap[7]   = 43;   // Space
    myConfig.keymap[8]   = 41;   // NDS R Button mapped to SHIFT
    myConfig.keymap[9]   = 42;   // NDS L Button mapped to SYMBOL
    myConfig.keymap[10]  = 40;   // NDS START mapped to '0'
    myConfig.keymap[11]  = 31;   // NDS SELECT mapped to '1'
}


void SetDefaultGlobalConfig(void)
{
    // A few global defaults...
    memset(&myGlobalConfig, 0x00, sizeof(myGlobalConfig));
    myGlobalConfig.showFPS        = 0;    // Don't show FPS counter by default
    myGlobalConfig.emuText        = 1;    // Default is to show Emulator Text
    myGlobalConfig.debugger       = 0;    // Debugger is not shown by default
}

void SetDefaultGameConfig(void)
{
    myConfig.game_crc    = 0;    // No game in this slot yet

    MapPlayer1();                // Default to Player 1 mapping

    myConfig.autoStop    = 1;                           // Normally detect STOP tape
    myConfig.tapeSpeed   = 1;                           // Normally accelerated
    myConfig.autoFire    = 0;                           // Default to no auto-fire on either button
    myConfig.dpad        = DPAD_NORMAL;                 // Normal DPAD use - mapped to joystick
    myConfig.autoLoad    = 1;                           // Default is to to auto-load TAP and TZX games
    myConfig.loadAs      = 0;                           // Default load is 48K
    myConfig.reserved2   = 0;
    myConfig.reserved3   = 0;
    myConfig.reserved4   = 0;
    myConfig.reserved5   = 0;
    myConfig.reserved6   = 0;
    myConfig.reserved7   = 0;
    myConfig.reserved8   = 0xA5;    // So it's easy to spot on an "upgrade" and we can re-default it
    myConfig.reserved9   = 0xA5;    // So it's easy to spot on an "upgrade" and we can re-default it
}

// ----------------------------------------------------------
// Load configuration into memory where we can use it.
// The configuration is stored in SpeccyDS.DAT
// ----------------------------------------------------------
void LoadConfig(void)
{
    // -----------------------------------------------------------------
    // Start with defaults.. if we find a match in our config database
    // below, we will fill in the config with data read from the file.
    // -----------------------------------------------------------------
    SetDefaultGameConfig();

    if (ReadFileCarefully("/data/SpeccyDS.DAT", (u8*)&myGlobalConfig, sizeof(myGlobalConfig), 0))  // Read Global Config
    {
        ReadFileCarefully("/data/SpeccyDS.DAT", (u8*)&AllConfigs, sizeof(AllConfigs), sizeof(myGlobalConfig)); // Read the full game array of configs

        if (myGlobalConfig.config_ver != CONFIG_VER)
        {
            memset(&AllConfigs, 0x00, sizeof(AllConfigs));
            SetDefaultGameConfig();
            SetDefaultGlobalConfig();
            SaveConfig(FALSE);
        }
    }
    else    // Not found... init the entire database...
    {
        memset(&AllConfigs, 0x00, sizeof(AllConfigs));
        SetDefaultGameConfig();
        SetDefaultGlobalConfig();
        SaveConfig(FALSE);
    }}

// -------------------------------------------------------------------------
// Try to match our loaded game to a configuration my matching CRCs
// -------------------------------------------------------------------------
void FindConfig(void)
{
    // -----------------------------------------------------------------
    // Start with defaults.. if we find a match in our config database
    // below, we will fill in the config with data read from the file.
    // -----------------------------------------------------------------
    SetDefaultGameConfig();

    for (u16 slot=0; slot<MAX_CONFIGS; slot++)
    {
        if (AllConfigs[slot].game_crc == file_crc)  // Got a match?!
        {
            memcpy(&myConfig, &AllConfigs[slot], sizeof(struct Config_t));
            break;
        }
    }
}


// ------------------------------------------------------------------------------
// Options are handled here... we have a number of things the user can tweak
// and these options are applied immediately. The user can also save off
// their option choices for the currently running game into the NINTV-DS.DAT
// configuration database. When games are loaded back up, NINTV-DS.DAT is read
// to see if we have a match and the user settings can be restored for the game.
// ------------------------------------------------------------------------------
struct options_t
{
    const char  *label;
    const char  *option[37];
    u8          *option_val;
    u8           option_max;
};

const struct options_t Option_Table[2][20] =
{
    // Game Specific Configuration
    {
        {"AUTO PLAY",      {"NO", "YES"},                                              &myConfig.autoLoad,          2},
        {"AUTO STOP",      {"NO", "YES"},                                              &myConfig.autoStop,          2},
        {"AUTO FIRE",      {"OFF", "ON"},                                              &myConfig.autoFire,          2},
        {"NDS D-PAD",      {"NORMAL", "DIAGONALS", "CHUCKIE"},                         &myConfig.dpad,              3},
        {"TAPE SPEED",     {"NORMAL", "ACCELERATED"},                                  &myConfig.tapeSpeed,         2},
        {"LOAD AS",        {"48K SPECTRUM", "128K SPECTRUM"},                          &myConfig.loadAs,            2},
        {"BUS CONTEND",    {"NORMAL", "LIGHT", "HEAVY"},                               &myConfig.contention,        3},
        {NULL,             {"",      ""},                                              NULL,                        1},
    },
    // Global Options
    {
        {"FPS",            {"OFF", "ON", "ON FULLSPEED"},                              &myGlobalConfig.showFPS,     3},
        {"EMU TEXT",       {"OFF",  "ON"},                                             &myGlobalConfig.emuText,     2},
        {"DEBUGGER",       {"OFF", "BAD OPS", "DEBUG", "FULL DEBUG"},                  &myGlobalConfig.debugger,    4},
        {NULL,             {"",      ""},                                              NULL,                        1},
    }
};


// ------------------------------------------------------------------
// Display the current list of options for the user.
// ------------------------------------------------------------------
u8 display_options_list(bool bFullDisplay)
{
    s16 len=0;

    DSPrint(1,21, 0, (char *)"                              ");
    if (bFullDisplay)
    {
        while (true)
        {
            sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][len].label, Option_Table[option_table][len].option[*(Option_Table[option_table][len].option_val)]);
            DSPrint(1,5+len, (len==0 ? 2:0), strBuf); len++;
            if (Option_Table[option_table][len].label == NULL) break;
        }

        // Blank out rest of the screen... option menus are of different lengths...
        for (int i=len; i<16; i++)
        {
            DSPrint(1,5+i, 0, (char *)"                               ");
        }
    }

    DSPrint(1,22, 0, (char *)" B=EXIT, X=GLOBAL, START=SAVE  ");
    return len;
}


//*****************************************************************************
// Change Game Options for the current game
//*****************************************************************************
void SpeccyDSGameOptions(bool bIsGlobal)
{
    u8 optionHighlighted;
    u8 idx;
    bool bDone=false;
    int keys_pressed;
    int last_keys_pressed = 999;

    option_table = (bIsGlobal ? 1:0);

    idx=display_options_list(true);
    optionHighlighted = 0;
    while (keysCurrent() != 0)
    {
        WAITVBL;
    }
    while (!bDone)
    {
        keys_pressed = keysCurrent();
        if (keys_pressed != last_keys_pressed)
        {
            last_keys_pressed = keys_pressed;
            if (keysCurrent() & KEY_UP) // Previous option
            {
                sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,5+optionHighlighted,0, strBuf);
                if (optionHighlighted > 0) optionHighlighted--; else optionHighlighted=(idx-1);
                sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,5+optionHighlighted,2, strBuf);
            }
            if (keysCurrent() & KEY_DOWN) // Next option
            {
                sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,5+optionHighlighted,0, strBuf);
                if (optionHighlighted < (idx-1)) optionHighlighted++;  else optionHighlighted=0;
                sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,5+optionHighlighted,2, strBuf);
            }

            if (keysCurrent() & KEY_RIGHT)  // Toggle option clockwise
            {
                *(Option_Table[option_table][optionHighlighted].option_val) = (*(Option_Table[option_table][optionHighlighted].option_val) + 1) % Option_Table[option_table][optionHighlighted].option_max;
                sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,5+optionHighlighted,2, strBuf);
            }
            if (keysCurrent() & KEY_LEFT)  // Toggle option counterclockwise
            {
                if ((*(Option_Table[option_table][optionHighlighted].option_val)) == 0)
                    *(Option_Table[option_table][optionHighlighted].option_val) = Option_Table[option_table][optionHighlighted].option_max -1;
                else
                    *(Option_Table[option_table][optionHighlighted].option_val) = (*(Option_Table[option_table][optionHighlighted].option_val) - 1) % Option_Table[option_table][optionHighlighted].option_max;
                sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,5+optionHighlighted,2, strBuf);
            }
            if (keysCurrent() & KEY_START)  // Save Options
            {
                SaveConfig(TRUE);
            }
            if (keysCurrent() & (KEY_X)) // Toggle Table
            {
                option_table ^= 1;
                idx=display_options_list(true);
                optionHighlighted = 0;
                while (keysCurrent() != 0)
                {
                    WAITVBL;
                }
            }
            if ((keysCurrent() & KEY_B) || (keysCurrent() & KEY_A))  // Exit options
            {
                option_table = 0;   // Reset for next time
                break;
            }
        }
        swiWaitForVBlank();
    }

    // Give a third of a second time delay...
    for (int i=0; i<20; i++)
    {
        swiWaitForVBlank();
    }

    return;
}

//*****************************************************************************
// Change Keymap Options for the current game
//*****************************************************************************
char szCha[34];
void DisplayKeymapName(u32 uY)
{
  sprintf(szCha," PAD UP    : %-17s",szKeyName[myConfig.keymap[0]]);
  DSPrint(1, 6,(uY==  6 ? 2 : 0),szCha);
  sprintf(szCha," PAD DOWN  : %-17s",szKeyName[myConfig.keymap[1]]);
  DSPrint(1, 7,(uY==  7 ? 2 : 0),szCha);
  sprintf(szCha," PAD LEFT  : %-17s",szKeyName[myConfig.keymap[2]]);
  DSPrint(1, 8,(uY==  8 ? 2 : 0),szCha);
  sprintf(szCha," PAD RIGHT : %-17s",szKeyName[myConfig.keymap[3]]);
  DSPrint(1, 9,(uY== 9 ? 2 : 0),szCha);
  sprintf(szCha," KEY A     : %-17s",szKeyName[myConfig.keymap[4]]);
  DSPrint(1,10,(uY== 10 ? 2 : 0),szCha);
  sprintf(szCha," KEY B     : %-17s",szKeyName[myConfig.keymap[5]]);
  DSPrint(1,11,(uY== 11 ? 2 : 0),szCha);
  sprintf(szCha," KEY X     : %-17s",szKeyName[myConfig.keymap[6]]);
  DSPrint(1,12,(uY== 12 ? 2 : 0),szCha);
  sprintf(szCha," KEY Y     : %-17s",szKeyName[myConfig.keymap[7]]);
  DSPrint(1,13,(uY== 13 ? 2 : 0),szCha);
  sprintf(szCha," KEY R     : %-17s",szKeyName[myConfig.keymap[8]]);
  DSPrint(1,14,(uY== 14 ? 2 : 0),szCha);
  sprintf(szCha," KEY L     : %-17s",szKeyName[myConfig.keymap[9]]);
  DSPrint(1,15,(uY== 15 ? 2 : 0),szCha);
  sprintf(szCha," START     : %-17s",szKeyName[myConfig.keymap[10]]);
  DSPrint(1,16,(uY== 16 ? 2 : 0),szCha);
  sprintf(szCha," SELECT    : %-17s",szKeyName[myConfig.keymap[11]]);
  DSPrint(1,17,(uY== 17 ? 2 : 0),szCha);
}

u8 keyMapType = 0;
void SwapKeymap(void)
{
    keyMapType = (keyMapType+1) % 5;
    switch (keyMapType)
    {
        case 0: MapPlayer1();  DSPrint(10,4,0,("KEMPSTON P1")); break;
        case 1: MapQAOP();     DSPrint(10,4,0,("   QAOP    ")); break;
        case 2: MapWASD();     DSPrint(10,4,0,("   WASD    "));break;
        case 3: MapZXSpace();  DSPrint(10,4,0,(" ZX SPACE  "));break;
        case 4: Sinclair1();   DSPrint(10,4,0,("SINCLAIR P1"));break;
    }
    WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
    DSPrint(10,4,0,("           "));
}


// ------------------------------------------------------------------------------
// Allow the user to change the key map for the current game and give them
// the option of writing that keymap out to a configuration file for the game.
// ------------------------------------------------------------------------------
void SpeccyDSChangeKeymap(void)
{
  u32 ucHaut=0x00, ucBas=0x00,ucL=0x00,ucR=0x00,ucY= 6, bOK=0, bIndTch=0;

  // ------------------------------------------------------
  // Clear the screen so we can put up Key Map infomation
  // ------------------------------------------------------
  unsigned short dmaVal =  *(bgGetMapPtr(bg0b) + 24*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*19*2);

  // --------------------------------------------------
  // Give instructions to the user...
  // --------------------------------------------------
  DSPrint(1 ,19,0,("   D-PAD : CHANGE KEY MAP    "));
  DSPrint(1 ,20,0,("       B : RETURN MAIN MENU  "));
  DSPrint(1 ,21,0,("       X : SWAP KEYMAP TYPE  "));
  DSPrint(1 ,22,0,("   START : SAVE KEYMAP       "));
  DisplayKeymapName(ucY);

  // -----------------------------------------------------------------------
  // Clear out any keys that might be pressed on the way in - make sure
  // NDS keys are not being pressed. This prevents the inadvertant A key
  // that enters this menu from also being acted on in the keymap...
  // -----------------------------------------------------------------------
  while ((keysCurrent() & (KEY_TOUCH | KEY_B | KEY_A | KEY_X | KEY_UP | KEY_DOWN))!=0)
      ;
  WAITVBL;

  while (!bOK) {
    if (keysCurrent() & KEY_UP) {
      if (!ucHaut) {
        DisplayKeymapName(32);
        ucY = (ucY == 6 ? 17 : ucY -1);
        bIndTch = myConfig.keymap[ucY-6];
        ucHaut=0x01;
        DisplayKeymapName(ucY);
      }
      else {
        ucHaut++;
        if (ucHaut>10) ucHaut=0;
      }
    }
    else {
      ucHaut = 0;
    }
    if (keysCurrent() & KEY_DOWN) {
      if (!ucBas) {
        DisplayKeymapName(32);
        ucY = (ucY == 17 ? 6 : ucY +1);
        bIndTch = myConfig.keymap[ucY-6];
        ucBas=0x01;
        DisplayKeymapName(ucY);
      }
      else {
        ucBas++;
        if (ucBas>10) ucBas=0;
      }
    }
    else {
      ucBas = 0;
    }

    if (keysCurrent() & KEY_START)
    {
        SaveConfig(true); // Save options
    }

    if (keysCurrent() & KEY_B)
    {
      bOK = 1;  // Exit menu
    }

    if (keysCurrent() & KEY_LEFT)
    {
        if (ucL == 0) {
          bIndTch = (bIndTch == 0 ? (MAX_KEY_OPTIONS-1) : bIndTch-1);
          ucL=1;
          myConfig.keymap[ucY-6] = bIndTch;
          DisplayKeymapName(ucY);
        }
        else {
          ucL++;
          if (ucL > 7) ucL = 0;
        }
    }
    else
    {
        ucL = 0;
    }

    if (keysCurrent() & KEY_RIGHT)
    {
        if (ucR == 0)
        {
          bIndTch = (bIndTch == (MAX_KEY_OPTIONS-1) ? 0 : bIndTch+1);
          ucR=1;
          myConfig.keymap[ucY-6] = bIndTch;
          DisplayKeymapName(ucY);
        }
        else
        {
          ucR++;
          if (ucR > 7) ucR = 0;
        }
    }
    else
    {
        ucR=0;
    }

    // Swap Player 1 and Player 2 keymap
    if (keysCurrent() & KEY_X)
    {
        SwapKeymap();
        bIndTch = myConfig.keymap[ucY-6];
        DisplayKeymapName(ucY);
        while (keysCurrent() & KEY_X)
            ;
        WAITVBL
    }
    swiWaitForVBlank();
  }
  while (keysCurrent() & KEY_B);
}


// -----------------------------------------------------------------------------------------
// At the bottom of the main screen we show the currently selected filename, size and CRC32
// -----------------------------------------------------------------------------------------
void DisplayFileName(void)
{
    sprintf(szName, "[%d K] [CRC: %08X]", file_size/1024, file_crc);
    DSPrint((16 - (strlen(szName)/2)),19,0,szName);

    sprintf(szName,"%s",gpFic[ucGameChoice].szName);
    for (u8 i=strlen(szName)-1; i>0; i--) if (szName[i] == '.') {szName[i]=0;break;}
    if (strlen(szName)>30) szName[30]='\0';
    DSPrint((16 - (strlen(szName)/2)),21,0,szName);
    if (strlen(gpFic[ucGameChoice].szName) >= 35)   // If there is more than a few characters left, show it on the 2nd line
    {
        if (strlen(gpFic[ucGameChoice].szName) <= 60)
        {
            sprintf(szName,"%s",gpFic[ucGameChoice].szName+30);
        }
        else
        {
            sprintf(szName,"%s",gpFic[ucGameChoice].szName+strlen(gpFic[ucGameChoice].szName)-30);
        }

        if (strlen(szName)>30) szName[30]='\0';
        DSPrint((16 - (strlen(szName)/2)),22,0,szName);
    }
}

void DisplayFileNameCassette(void)
{
    sprintf(szName,"%s",gpFic[ucGameChoice].szName);
    for (u8 i=strlen(szName)-1; i>0; i--) if (szName[i] == '.') {szName[i]=0;break;}
    if (strlen(szName)>28) szName[28]='\0';
    DSPrint((16 - (strlen(szName)/2)),16,0,szName);
    if (strlen(gpFic[ucGameChoice].szName) >= 33)   // If there is more than a few characters left, show it on the 2nd line
    {
        if (strlen(gpFic[ucGameChoice].szName) <= 58)
        {
            sprintf(szName,"%s",gpFic[ucGameChoice].szName+28);
        }
        else
        {
            sprintf(szName,"%s",gpFic[ucGameChoice].szName+strlen(gpFic[ucGameChoice].szName)-30);
        }

        if (strlen(szName)>28) szName[28]='\0';
        DSPrint((16 - (strlen(szName)/2)),17,0,szName);
    }
}

//*****************************************************************************
// Display info screen and change options "main menu"
//*****************************************************************************
void dispInfoOptions(u32 uY)
{
    DSPrint(2, 5,(uY== 5 ? 2 : 0),("         LOAD  GAME         "));
    DSPrint(2, 7,(uY== 7 ? 2 : 0),("         PLAY  GAME         "));
    DSPrint(2, 9,(uY== 9 ? 2 : 0),("     REDEFINE  KEYS         "));
    DSPrint(2,11,(uY==11 ? 2 : 0),("         GAME  OPTIONS      "));
    DSPrint(2,13,(uY==13 ? 2 : 0),("       GLOBAL  OPTIONS      "));
    DSPrint(2,15,(uY==15 ? 2 : 0),("         QUIT  EMULATOR     "));
}

// --------------------------------------------------------------------
// Some main menu selections don't make sense without a game loaded.
// --------------------------------------------------------------------
void NoGameSelected(u32 ucY)
{
    unsigned short dmaVal = *(bgGetMapPtr(bg1b)+24*32);
    while (keysCurrent()  & (KEY_START | KEY_A));
    dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*18*2);
    DSPrint(5,10,0,("   NO GAME SELECTED   "));
    DSPrint(5,12,0,("  PLEASE, USE OPTION  "));
    DSPrint(5,14,0,("      LOAD  GAME      "));
    while (!(keysCurrent()  & (KEY_START | KEY_A)));
    while (keysCurrent()  & (KEY_START | KEY_A));
    dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*18*2);
    dispInfoOptions(ucY);
}


void ReadFileCRCAndConfig(void)
{
    // Reset the mode related vars...
    keyMapType = 0;

    // ----------------------------------------------------------------------------------
    // Clear the entire ROM buffer[] - fill with 0xFF to emulate non-responsive memory
    // ----------------------------------------------------------------------------------
    memset(ROM_Memory, 0xFF, MAX_CART_SIZE);

    // Grab the all-important file CRC - this also loads the file into ROM_Memory[]
    getfile_crc(gpFic[ucGameChoice].szName);

    if (strstr(gpFic[ucGameChoice].szName, ".z80") != 0) speccy_mode = MODE_Z80;
    if (strstr(gpFic[ucGameChoice].szName, ".Z80") != 0) speccy_mode = MODE_Z80;
    if (strstr(gpFic[ucGameChoice].szName, ".sna") != 0) speccy_mode = MODE_SNA;
    if (strstr(gpFic[ucGameChoice].szName, ".SNA") != 0) speccy_mode = MODE_SNA;
    if (strstr(gpFic[ucGameChoice].szName, ".tap") != 0) speccy_mode = MODE_TAP;
    if (strstr(gpFic[ucGameChoice].szName, ".TAP") != 0) speccy_mode = MODE_TAP;
    if (strstr(gpFic[ucGameChoice].szName, ".tzx") != 0) speccy_mode = MODE_TZX;
    if (strstr(gpFic[ucGameChoice].szName, ".TZX") != 0) speccy_mode = MODE_TZX;
    if (strstr(gpFic[ucGameChoice].szName, ".rom") != 0) speccy_mode = MODE_BIOS;
    if (strstr(gpFic[ucGameChoice].szName, ".ROM") != 0) speccy_mode = MODE_BIOS;

    FindConfig();    // Try to find keymap and config for this file...
}


// ----------------------------------------------------------------------
// Read file twice and ensure we get the same CRC... if not, do it again
// until we get a clean read. Return the filesize to the caller...
// ----------------------------------------------------------------------
u32 ReadFileCarefully(char *filename, u8 *buf, u32 buf_size, u32 buf_offset)
{
    u32 crc1 = 0;
    u32 crc2 = 1;
    u32 fileSize = 0;

    // --------------------------------------------------------------------------------------------
    // I've seen some rare issues with reading files from the SD card on a DSi so we're doing
    // this slow and careful - we will read twice and ensure that we get the same CRC both times.
    // --------------------------------------------------------------------------------------------
    do
    {
        // Read #1
        crc1 = 0xFFFFFFFF;
        FILE* file = fopen(filename, "rb");
        if (file)
        {
            if (buf_offset) fseek(file, buf_offset, SEEK_SET);
            fileSize = fread(buf, 1, buf_size, file);
            crc1 = getCRC32(buf, buf_size);
            fclose(file);
        }

        // Read #2
        crc2 = 0xFFFFFFFF;
        FILE* file2 = fopen(filename, "rb");
        if (file2)
        {
            if (buf_offset) fseek(file2, buf_offset, SEEK_SET);
            fread(buf, 1, buf_size, file2);
            crc2 = getCRC32(buf, buf_size);
            fclose(file2);
        }
   } while (crc1 != crc2); // If the file couldn't be read, file_size will be 0 and the CRCs will both be 0xFFFFFFFF

   return fileSize;
}

// --------------------------------------------------------------------
// Let the user select new options for the currently loaded game...
// --------------------------------------------------------------------
void speccyDSChangeOptions(void)
{
  u16 ucHaut=0x00, ucBas=0x00,ucA=0x00,ucY= 5, bOK=0;

  // Upper Screen Background
  videoSetMode(MODE_0_2D | DISPLAY_BG0_ACTIVE | DISPLAY_BG1_ACTIVE | DISPLAY_SPR_1D_LAYOUT | DISPLAY_SPR_ACTIVE);
  vramSetBankA(VRAM_A_MAIN_BG);
  vramSetBankB(VRAM_B_MAIN_SPRITE_0x06400000);
  bg0 = bgInit(0, BgType_Text8bpp, BgSize_T_256x512, 31,0);
  bg1 = bgInit(1, BgType_Text8bpp, BgSize_T_256x512, 29,0);
  bgSetPriority(bg0,1);bgSetPriority(bg1,0);
  decompress(topscreenTiles, bgGetGfxPtr(bg0), LZ77Vram);
  decompress(topscreenMap, (void*) bgGetMapPtr(bg0), LZ77Vram);
  dmaCopy((void*) topscreenPal,(void*) BG_PALETTE,256*2);
  unsigned short dmaVal =  *(bgGetMapPtr(bg0) + 51*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1),32*24*2);

  // Lower Screen Background
  BottomScreenOptions();

  dispInfoOptions(ucY);

  if (ucGameChoice != -1)
  {
      DisplayFileName();
  }

  while (!bOK) {
    if (keysCurrent()  & KEY_UP) {
      if (!ucHaut) {
        dispInfoOptions(32);
        ucY = (ucY == 5 ? 15 : ucY -2);
        ucHaut=0x01;
        dispInfoOptions(ucY);
      }
      else {
        ucHaut++;
        if (ucHaut>10) ucHaut=0;
      }
    }
    else {
      ucHaut = 0;
    }
    if (keysCurrent()  & KEY_DOWN) {
      if (!ucBas) {
        dispInfoOptions(32);
        ucY = (ucY == 15 ? 5 : ucY +2);
        ucBas=0x01;
        dispInfoOptions(ucY);
      }
      else {
        ucBas++;
        if (ucBas>10) ucBas=0;
      }
    }
    else {
      ucBas = 0;
    }
    if (keysCurrent()  & KEY_A) {
      if (!ucA) {
        ucA = 0x01;
        switch (ucY) {
          case 5 :      // LOAD GAME
            speccyDSLoadFile(0);
            dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*19*2);
            BottomScreenOptions();
            if (ucGameChoice != -1)
            {
                ReadFileCRCAndConfig(); // Get CRC32 of the file and read the config/keys
                DisplayFileName();    // And put up the filename on the bottom screen
            }
            ucY = 7;
            dispInfoOptions(ucY);
            break;
          case 7 :     // PLAY GAME
            if (ucGameChoice != -1)
            {
                bOK = 1;
            }
            else
            {
                NoGameSelected(ucY);
            }
            break;
          case 9 :     // REDEFINE KEYS
            if (ucGameChoice != -1)
            {
                SpeccyDSChangeKeymap();
                BottomScreenOptions();
                dispInfoOptions(ucY);
                DisplayFileName();
            }
            else
            {
                NoGameSelected(ucY);
            }
            break;
          case 11 :     // GAME OPTIONS
            if (ucGameChoice != -1)
            {
                SpeccyDSGameOptions(false);
                BottomScreenOptions();
                dispInfoOptions(ucY);
                DisplayFileName();
            }
            else
            {
               NoGameSelected(ucY);
            }
            break;

          case 13 :     // GLOBAL OPTIONS
            SpeccyDSGameOptions(true);
            BottomScreenOptions();
            dispInfoOptions(ucY);
            DisplayFileName();
            break;

          case 15 :     // QUIT EMULATOR
            exit(1);
            break;
        }
      }
    }
    else
      ucA = 0x00;
    if (keysCurrent()  & KEY_START) {
      if (ucGameChoice != -1)
      {
        bOK = 1;
      }
      else
      {
        NoGameSelected(ucY);
      }
    }
    swiWaitForVBlank();
  }
  while (keysCurrent()  & (KEY_START | KEY_A));
}

//*****************************************************************************
// Displays a message on the screen
//*****************************************************************************
void DSPrint(int iX,int iY,int iScr,char *szMessage)
{
  u16 *pusScreen,*pusMap;
  u16 usCharac;
  char *pTrTxt=szMessage;

  pusScreen=(u16*) (iScr != 1 ? bgGetMapPtr(bg1b) : bgGetMapPtr(bg1))+iX+(iY<<5);
  pusMap=(u16*) (iScr != 1 ? (iScr == 6 ? bgGetMapPtr(bg0b)+24*32 : (iScr == 0 ? bgGetMapPtr(bg0b)+24*32 : bgGetMapPtr(bg0b)+26*32 )) : bgGetMapPtr(bg0)+51*32 );

  while((*pTrTxt)!='\0' )
  {
    char ch = *pTrTxt++;
    if (ch >= 'a' && ch <= 'z') ch -= 32;   // Faster than strcpy/strtoupper

    if (((ch)<' ') || ((ch)>'_'))
      usCharac=*(pusMap);                   // Will render as a vertical bar
    else if((ch)<'@')
      usCharac=*(pusMap+(ch)-' ');          // Number from 0-9 or punctuation
    else
      usCharac=*(pusMap+32+(ch)-'@');       // Character from A-Z
    *pusScreen++=usCharac;
  }
}

/******************************************************************************
* Routine FadeToColor :  Fade from background to black or white
******************************************************************************/
void FadeToColor(unsigned char ucSens, unsigned short ucBG, unsigned char ucScr, unsigned char valEnd, unsigned char uWait) {
  unsigned short ucFade;
  unsigned char ucBcl;

  // Fade-out to black
  if (ucScr & 0x01) REG_BLDCNT=ucBG;
  if (ucScr & 0x02) REG_BLDCNT_SUB=ucBG;
  if (ucSens == 1) {
    for(ucFade=0;ucFade<valEnd;ucFade++) {
      if (ucScr & 0x01) REG_BLDY=ucFade;
      if (ucScr & 0x02) REG_BLDY_SUB=ucFade;
      for (ucBcl=0;ucBcl<uWait;ucBcl++) {
        swiWaitForVBlank();
      }
    }
  }
  else {
    for(ucFade=16;ucFade>valEnd;ucFade--) {
      if (ucScr & 0x01) REG_BLDY=ucFade;
      if (ucScr & 0x02) REG_BLDY_SUB=ucFade;
      for (ucBcl=0;ucBcl<uWait;ucBcl++) {
        swiWaitForVBlank();
      }
    }
  }
}


/*********************************************************************************
 * Keyboard Key Buffering Engine...
 ********************************************************************************/
u8 BufferedKeys[32];
u8 BufferedKeysWriteIdx=0;
u8 BufferedKeysReadIdx=0;
void BufferKey(u8 key)
{
    BufferedKeys[BufferedKeysWriteIdx] = key;
    BufferedKeysWriteIdx = (BufferedKeysWriteIdx+1) % 32;
}

// Buffer a whole string worth of characters...
void BufferKeys(char *str)
{
    for (int i=0; i<strlen(str); i++)  BufferKey((u8)str[i]);
}

// ---------------------------------------------------------------------------------------
// Called every frame... so 1/50th or 1/60th of a second. We will virtually 'press' and
// hold the key for roughly a tenth of a second and be smart about shift keys...
// ---------------------------------------------------------------------------------------
void ProcessBufferedKeys(void)
{
    static u8 next_dampen_time = 10;
    static u8 dampen = 0;
    static u8 buf_held = 0;

    if (++dampen >= next_dampen_time) // Roughly 50ms... experimentally good enough for all systems.
    {
        if (BufferedKeysReadIdx != BufferedKeysWriteIdx)
        {
            buf_held = BufferedKeys[BufferedKeysReadIdx];
            BufferedKeysReadIdx = (BufferedKeysReadIdx+1) % 32;
            if (buf_held == 255) {buf_held = 0; next_dampen_time=60;} else next_dampen_time = 10;
        } else buf_held = 0;
        dampen = 0;
    }

    // See if the shift key should be virtually pressed along with this buffered key...
    if (buf_held) {kbd_keys[kbd_keys_pressed++] = buf_held;}
}


/*********************************************************************************
 * Init Spectrum Engine for that game
 ********************************************************************************/
u8 spectrumInit(char *szGame)
{
  u8 RetFct,uBcl;
  u16 uVide;

  // We've got some debug data we can use for development... reset these.
  memset(debug, 0x00, sizeof(debug));

  // -----------------------------------------------------------------
  // Change graphic mode to initiate emulation.
  // Here we can claim back 128K of VRAM which is otherwise unused
  // but we can use it for fast memory swaps and look-up-tables.
  // -----------------------------------------------------------------
  videoSetMode(MODE_5_2D | DISPLAY_BG3_ACTIVE);
  vramSetBankA(VRAM_A_MAIN_BG_0x06000000);      // This is our top emulation screen (where the game is played)
  vramSetBankB(VRAM_B_LCD);                     // 128K of Video Memory mapped at 0x6820000
  REG_BG3CNT = BG_BMP8_256x256;
  REG_BG3PA = (1<<8);
  REG_BG3PB = 0;
  REG_BG3PC = 0;
  REG_BG3PD = (1<<8);
  REG_BG3X = 0;
  REG_BG3Y = 0;

  // Init the page flipping buffer...
  for (uBcl=0;uBcl<192;uBcl++)
  {
     uVide=(uBcl/12);
     dmaFillWords(uVide | (uVide<<16),pVidFlipBuf+uBcl*128,256);
  }

  RetFct = loadrom(szGame);      // Load up the Spectrum game/tap/tzx

  ResetSpectrum();

  // Return with result
  return (RetFct);
}

/*********************************************************************************
 * Run the emul
 ********************************************************************************/
void spectrumRun(void)
{
  ResetZ80(&CPU);                       // Reset the CZ80 core CPU
  speccy_reset();                       // Ensure the Spectrum Emulation is ready
  BottomScreenKeyboard();                 // Show the game-related screen with keypad / keyboard
}

u8 ZX_Spectrum_palette[16*3]   = {
  0x00,0x00,0x00,   // Black
  0x00,0x00,0xD8,   // Blue
  0xD8,0x00,0x00,   // Red
  0xD8,0x00,0xD8,   // Magenta
  0x00,0xD8,0x00,   // Green
  0x00,0xD8,0xD8,   // Cyan
  0xD8,0xD8,0x00,   // Yellow
  0xD8,0xD8,0xD8,   // White
  0x00,0x00,0x00,   // Bright Black
  0x00,0x00,0xFF,   // Bright Blue
  0xFF,0x00,0x00,   // Bright Red
  0xFF,0x00,0xFF,   // Bright Magenta
  0x00,0xFF,0x00,   // Bright Green
  0x00,0xFF,0xFF,   // Bright Cyan
  0xFF,0xFF,0x00,   // Bright Yellow
  0xFF,0xFF,0xFF,   // Bright White
};

/*********************************************************************************
 * Set Spectrum color palette... 8 colors in 2 intensities
 ********************************************************************************/
void spectrumSetPalette(void)
{
  u8 uBcl,r,g,b;

  for (uBcl=0;uBcl<16;uBcl++)
  {
    r = (u8) ((float) ZX_Spectrum_palette[uBcl*3+0]*0.121568f);
    g = (u8) ((float) ZX_Spectrum_palette[uBcl*3+1]*0.121568f);
    b = (u8) ((float) ZX_Spectrum_palette[uBcl*3+2]*0.121568f);

    SPRITE_PALETTE[uBcl] = RGB15(r,g,b);
    BG_PALETTE[uBcl] = RGB15(r,g,b);
  }
}


/*******************************************************************************
 * Compute the file CRC - this will be our unique identifier for the game
 * for saving HI SCORES and Configuration / Key Mapping data.
 *******************************************************************************/
void getfile_crc(const char *filename)
{
    DSPrint(11,13,6, "LOADING...");

    file_crc = getFileCrc(filename);        // The CRC is used as a unique ID to save out High Scores and Configuration...

    DSPrint(11,13,6, "          ");
}


/** loadrom() ******************************************************************/
/* Open a rom file from file system and load it into the ROM_Memory[] buffer   */
/*******************************************************************************/
u8 loadrom(const char *filename)
{
  u8 bOK = 0;
  int romSize = 0;

  FILE* handle = fopen(filename, "rb");
  if (handle != NULL)
  {
    // Save the initial filename and file - we need it for save/restore of state
    strcpy(initial_file, filename);
    getcwd(initial_path, MAX_ROM_NAME);

    // Get file size the 'fast' way - use fstat() instead of fseek() or ftell()
    struct stat stbuf;
    (void)fstat(fileno(handle), &stbuf);
    romSize = stbuf.st_size;
    fclose(handle); // We only need to close the file - the game ROM is now sitting in ROM_Memory[] from the getFileCrc() handler

    last_file_size = (u32)romSize;
  }

  return bOK;
}

void vblankIntro()
{
  vusCptVBL++;
}

// --------------------------------------------------------------
// Intro with portabledev logo and new PHEONIX-EDITION version
// --------------------------------------------------------------
void intro_logo(void)
{
  bool bOK;

  // Init graphics
  videoSetMode(MODE_0_2D | DISPLAY_BG0_ACTIVE );
  videoSetModeSub(MODE_0_2D | DISPLAY_BG0_ACTIVE );
  vramSetBankA(VRAM_A_MAIN_BG); vramSetBankC(VRAM_C_SUB_BG);
  irqSet(IRQ_VBLANK, vblankIntro);
  irqEnable(IRQ_VBLANK);

  // Init BG
  int bg1 = bgInit(0, BgType_Text8bpp, BgSize_T_256x256, 31,0);

  // Init sub BG
  int bg1s = bgInitSub(0, BgType_Text8bpp, BgSize_T_256x256, 31,0);

  REG_BLDCNT = BLEND_FADE_BLACK | BLEND_SRC_BG0 | BLEND_DST_BG0; REG_BLDY = 16;
  REG_BLDCNT_SUB = BLEND_FADE_BLACK | BLEND_SRC_BG0 | BLEND_DST_BG0; REG_BLDY_SUB = 16;

  mmEffect(SFX_MUS_INTRO);

  // Show portabledev
  decompress(pdev_tbg0Tiles, bgGetGfxPtr(bg1), LZ77Vram);
  decompress(pdev_tbg0Map, (void*) bgGetMapPtr(bg1), LZ77Vram);
  dmaCopy((void *) pdev_tbg0Pal,(u16*) BG_PALETTE,256*2);

  decompress(pdev_bg0Tiles, bgGetGfxPtr(bg1s), LZ77Vram);
  decompress(pdev_bg0Map, (void*) bgGetMapPtr(bg1s), LZ77Vram);
  dmaCopy((void *) pdev_bg0Pal,(u16*) BG_PALETTE_SUB,256*2);

  FadeToColor(0,BLEND_FADE_BLACK | BLEND_SRC_BG0 | BLEND_DST_BG0,3,0,3);

  bOK=false;
  while (!bOK) { if ( !(keysCurrent() & 0x1FFF) ) bOK=true; } // 0x1FFF = key or touch screen
  vusCptVBL=0;bOK=false;
  while (!bOK && (vusCptVBL<3*60)) { if (keysCurrent() & 0x1FFF ) bOK=true; }
  bOK=false;
  while (!bOK) { if ( !(keysCurrent() & 0x1FFF) ) bOK=true; }

  FadeToColor(1,BLEND_FADE_WHITE | BLEND_SRC_BG0 | BLEND_DST_BG0,3,16,3);
}

// -------------------------------------------------------------------------
// For arious machines, we have patched the BIOS so that we trap calls
// to various I/O routines: namely cassette access. We handle that here.
// -------------------------------------------------------------------------
void PatchZ80(register Z80 *r)
{
}

// -----------------------------------------------------------------
// Trap and report illegal opcodes to the SpeccyDS debugger...
// -----------------------------------------------------------------
void Trap_Bad_Ops(char *prefix, byte I, word W)
{
    if (myGlobalConfig.debugger)
    {
        char tmp[32];
        sprintf(tmp, "ILLOP: %s %02X %04X", prefix, I, W);
        DSPrint(0,0,6, tmp);
    }
}


void _putchar(char character) {}

// End of file


//{{BLOCK(mainmenu)

//======================================================================
//
//	mainmenu, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 349 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 6140 + 976 = 7628
//
//	Time-stamp: 2025-04-10, 07:54:42
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_MAINMENU_H
#define GRIT_MAINMENU_H

#define mainmenuTilesLen 6140
extern const unsigned int mainmenuTiles[1535];

#define mainmenuMapLen 976
extern const unsigned short mainmenuMap[488];

#define mainmenuPalLen 512
extern const unsigned short mainmenuPal[256];

#endif // GRIT_MAINMENU_H

//}}BLOCK(mainmenu)

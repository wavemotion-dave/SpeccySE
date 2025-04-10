
//{{BLOCK(pdev_bg0)

//======================================================================
//
//	pdev_bg0, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 577 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 8800 + 1484 = 10796
//
//	Time-stamp: 2025-04-10, 07:54:43
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_PDEV_BG0_H
#define GRIT_PDEV_BG0_H

#define pdev_bg0TilesLen 8800
extern const unsigned int pdev_bg0Tiles[2200];

#define pdev_bg0MapLen 1484
extern const unsigned short pdev_bg0Map[742];

#define pdev_bg0PalLen 512
extern const unsigned short pdev_bg0Pal[256];

#endif // GRIT_PDEV_BG0_H

//}}BLOCK(pdev_bg0)

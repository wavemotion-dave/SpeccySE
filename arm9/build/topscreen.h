
//{{BLOCK(topscreen)

//======================================================================
//
//	topscreen, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 585 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 23288 + 1436 = 25236
//
//	Time-stamp: 2025-04-10, 07:54:43
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_TOPSCREEN_H
#define GRIT_TOPSCREEN_H

#define topscreenTilesLen 23288
extern const unsigned int topscreenTiles[5822];

#define topscreenMapLen 1436
extern const unsigned short topscreenMap[718];

#define topscreenPalLen 512
extern const unsigned short topscreenPal[256];

#endif // GRIT_TOPSCREEN_H

//}}BLOCK(topscreen)

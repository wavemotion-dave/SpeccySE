
//{{BLOCK(speccy_kbd)

//======================================================================
//
//	speccy_kbd, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 494 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 8096 + 1536 = 10144
//
//	Time-stamp: 2025-04-10, 07:54:42
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_SPECCY_KBD_H
#define GRIT_SPECCY_KBD_H

#define speccy_kbdTilesLen 8096
extern const unsigned int speccy_kbdTiles[2024];

#define speccy_kbdMapLen 1536
extern const unsigned short speccy_kbdMap[768];

#define speccy_kbdPalLen 512
extern const unsigned short speccy_kbdPal[256];

#endif // GRIT_SPECCY_KBD_H

//}}BLOCK(speccy_kbd)

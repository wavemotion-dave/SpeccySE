
//{{BLOCK(cassette)

//======================================================================
//
//	cassette, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 246 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 4668 + 776 = 5956
//
//	Time-stamp: 2025-04-10, 07:54:42
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_CASSETTE_H
#define GRIT_CASSETTE_H

#define cassetteTilesLen 4668
extern const unsigned int cassetteTiles[1167];

#define cassetteMapLen 776
extern const unsigned short cassetteMap[388];

#define cassettePalLen 512
extern const unsigned short cassettePal[256];

#endif // GRIT_CASSETTE_H

//}}BLOCK(cassette)

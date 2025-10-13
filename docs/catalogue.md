Sprite ID | Use Count | Description
0x04 | 5 | 
0x05 | 3 | 
0x06 | 4 | 
0x07 | 68 | 
0x08 | 12 | 
0x0b | 24 | 
0x0c | 19 | 
0x0d | 1 | 
0x0e | 6 | 
0x0f | 3 | 
0x10 | 1 | 
0x11 | 19 | 
0x15 | 41 | 
0x17 | 9 | 
0x18 | 4 | 
0x19 | 13 | 
0x1a | 4 | 
0x1b | 6 | 
0x1c | 5 | 
0x1e | 23 | 
0x1f | 43 | 
0x20 | 15 | 
0x21 | 24 | 
0x22 | 10 | 
0x23 | 2 | 
0x26 | 23 | 
0x28 | 14 | 
0x2a | 21 | 
0x2c | 3 | 
0x2d | 8 | 
0x2e | 6 | 
0x2f | 3 | 
0x30 | 5 | 
0x31 | 4 | 
0x32 | 2 | 
0x33 | 1 | 
0x34 | 5 | 
0x37 | 2 | 
0x38 | 1 | 
0x39 | 1 | 
0x3a | 1 | 
0x3b | 4 | Spring Guardian
0x3c | 2 | 
0x3d | 1 | 
0x3f | 1 | 
0x40 | 1 | 
0x41 | 1 | 
0x42 | 2 | 
0x44 | 2 | 
0x45 | 4 | 
0x47 | 38 | 
0x48 | 3 | 
0x49 | 1 | 
0x4a | 1 | 
0x4b | 3 | 
0x4c | 12 | 
0x4d | 2 | 
0x4e | 2 | 
0x4f | 1 | 
0x50 | 2 | 
0x52 | 1 | 
0x55 | 6 | 
0x56 | 5 | 
0x57 | 1 | 
0x58 | 1 | 
0x59 | 1 | 
0x5a | 1 | 
0x5b | 1 | 
0x5c | 1 | 
0x5d | 3 | 
0x5e | 2 | 
0x5f | 2 | 
0x60 | 5 | 
0x61 | 1 | Tower Spring
0x62 | 1 | Sky Spring
0x63 | 1 | Dungeon Spring

Text ID | Use Count | Description
0x00 | 1 | 
0x01 | 1 | 
0x03 | 1 | 
0x04 | 1 | 
0x05 | 1 | 
0x06 | 1 | 
0x07 | 1 | 
0x08 | 1 | 
0x09 | 1 | 
0x0a | 1 | 
0x0b | 1 | 
0x0c | 1 | 
0x0d | 1 | 
0x0e | 1 | 
0x0f | 1 | 
0x10 | 1 | 
0x11 | 1 | 
0x19 | 1 | 
0x1a | 1 | 
0x20 | 1 | 
0x21 | 1 | 
0x25 | 2 | Sky Spring dialogue (also added to one enemy, by mistake probably)
0x26 | 1 | 
0x27 | 1 | 
0x2a | 1 | Added to enemy by mistake probably
0x30 | 1 | 
0x40 | 1 | 
0x50 | 1 | 
0x52 | 1 | 
0x60 | 1 | 




Inter-world transitions
Chunk=3,Screen=12,Dest screen=22,Palette ID=6
Chunk=3,Screen=22,Dest screen=12,Palette ID=6





Intra-world transitions

// from mist to towns
Chunk=1,Screen=9,Dest chunk=2,Dest screen=4,Palette ID=27
Chunk=1,Screen=12,Dest chunk=2,Dest screen=5,Palette ID=27
Chunk=1,Screen=34,Dest chunk=2,Dest screen=6,Palette ID=27
Chunk=1,Screen=37,Dest chunk=2,Dest screen=7,Palette ID=27

// error config? screen 3,27 is a copy of this screen and has messed up scrolling bytes
Chunk=3,Screen=0,Dest chunk=2,Dest screen=2,Palette ID=27

// to apolune
Chunk=3,Screen=7,Dest chunk=2,Dest screen=0,Palette ID=27
Chunk=3,Screen=8,Dest chunk=2,Dest screen=1,Palette ID=27

// to forepaw
Chunk=3,Screen=26,Dest chunk=2,Dest screen=2,Palette ID=27
Chunk=3,Screen=29,Dest chunk=2,Dest screen=3,Palette ID=27

// to branch town #1
Chunk=4,Screen=13,Dest chunk=2,Dest screen=8,Palette ID=27

// daybreak
Chunk=4,Screen=35,Dest chunk=2,Dest screen=10,Palette ID=27
Chunk=4,Screen=36,Dest chunk=2,Dest screen=11,Palette ID=27
// to dartmoor town
Chunk=5,Screen=3,Dest chunk=2,Dest screen=12,Palette ID=27

// ------- TOWNS ----------
// apolune
Chunk=2,Screen=0,Dest chunk=3,Dest screen=7,Palette ID=6
Chunk=2,Screen=1,Dest chunk=3,Dest screen=8,Palette ID=6
// forepaw
Chunk=2,Screen=2,Dest chunk=3,Dest screen=26,Palette ID=6
Chunk=2,Screen=3,Dest chunk=3,Dest screen=29,Palette ID=6
// from towns to mist
Chunk=2,Screen=4,Dest chunk=1,Dest screen=9,Palette ID=10
Chunk=2,Screen=5,Dest chunk=1,Dest screen=12,Palette ID=10
Chunk=2,Screen=6,Dest chunk=1,Dest screen=34,Palette ID=10
Chunk=2,Screen=7,Dest chunk=1,Dest screen=37,Palette ID=10
// branch town #1 (one entry and exit)
Chunk=2,Screen=8,Dest chunk=4,Dest screen=13,Palette ID=8
//daybreak
Chunk=2,Screen=10,Dest chunk=4,Dest screen=35,Palette ID=8
Chunk=2,Screen=11,Dest chunk=4,Dest screen=36,Palette ID=8
// dartmoor town (one entry and exit)
Chunk=2,Screen=12,Dest chunk=5,Dest screen=3,Palette ID=12

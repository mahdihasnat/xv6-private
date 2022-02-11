// defincation of VPA_Swap entry  [ virtual page address]
// [20 bit for virtual page address][12 bit for flags|]

#define 	SWAP_FLAGS(x)  	((uint)(x) &  0xFFF)
#define 	SWAP_ADDR(x)  	((uint)(x) & ~0xFFF)

// swap flags
// bit 11    page is occupied
// bit 10-0	 not used , zero by default

#define 	SWAP_P	0x800 // 
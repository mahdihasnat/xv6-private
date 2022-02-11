// defincation of VPA_Swap entry  [ virtual page address]
// [20 bit for virtual page address][12 bit for flags|]

#define 	SWAP_FLAGS(x)  	((uint)(x) &  0xFFF)
#define 	SWAP_ADDR(x)  	((uint)(x) & ~0xFFF)

#define 	MEM_FLAGS(x)  	((uint)(x) &  0xFFF)
#define 	MEM_ADDR(x)  	((uint)(x) & ~0xFFF)

// swap flags
// bit 11    page is occupied
// bit 10-0	 not used , zero by default [ for fifo]

#define 	SWAP_P	0x800 // 

// mem flags
// bit 11    page is occupied				[ for nfu ]
// bit 10-0  counter ,zero by default 		[ for nfu ]
// bit 10-0  not used, zero by default 		[ for fifo ]

#define 	MEM_P	0x800 //

#ifdef 		NFU_SWAP

#define 	NFU_MEM_COUNTER(x)  ((uint)(x) &  0x7FF)
#define 	NFU_COUNTER_MASK	0x7FF

#endif
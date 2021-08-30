#ifndef NGPHEADER
#define NGPHEADER

#include <nds.h>

//NgpHeader
typedef struct
{
	uint8	licence[28];		// 0x00 - 0x1B
	uint32	startPC;			// 0x1C - 0x1F
	uint16	catalog;			// 0x20 - 0x21
	uint8	subCatalog;			// 0x22
	uint8	mode;				// 0x23, 0x00 = B&W, 0x10 = Color.
	uint8	name[12];			// 0x24 - 0x2F

	uint32	reserved1;			// 0x30 - 0x33
	uint32	reserved2;			// 0x34 - 0x37
	uint32	reserved3;			// 0x38 - 0x3B
	uint32	reserved4;			// 0x3C - 0x3F
} NgpHeader;

#endif	// NGPHEADER

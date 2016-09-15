#include "debug.h"

#if defined(CONFIG_DISABLE_UART)
#define DEFAULT_DEBUG_LEVEL             PRN_NONE
#else
#ifdef CONFIG_FPGA
#define	DEFAULT_DEBUG_LEVEL		PRN_RES
#else
//#define	DEFAULT_DEBUG_LEVEL		PRN_NOTICE
// KEITH
#define	DEFAULT_DEBUG_LEVEL		PRN_RES
#endif
#endif

static int dbg_lvl = DEFAULT_DEBUG_LEVEL;

void set_dbg_level(int lvl)
{
	dbg_lvl = lvl;
}

inline int get_dbg_level()
{
	return dbg_lvl;
}

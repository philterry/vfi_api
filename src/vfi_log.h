
#ifndef VFI_LOG_H
#define VFI_LOG_H

#include <syslog.h>
#include <stdarg.h>
#include <assert.h>

/* Debug level is treated as a four-bit level integer, when, and a 28-bit mask, who_what */

/* The when level follows syslog conventions: */
#define	VFI_LOG_EMERG	        LOG_EMERG	/* system is unusable			*/
#define	VFI_LOG_ALERT	        LOG_ALERT	/* action must be taken immediately	*/
#define	VFI_LOG_CRIT	        LOG_CRIT	/* critical conditions			*/
#define	VFI_LOG_ERR	        LOG_ERR	        /* error conditions			*/
#define	VFI_LOG_WARNING	        LOG_WARNING	/* warning conditions			*/
#define	VFI_LOG_NOTICE          LOG_NOTICE	/* normal but significant condition	*/
#define	VFI_LOG_INFO	        LOG_INFO	/* informational			*/
#define	VFI_LOG_DEBUG	        LOG_DEBUG	/* debug-level messages			*/
#define VFI_DBG_WHEN            0xF             /* Mask for when field                  */

#define VFI_DBG_WHO_WHAT        ~VFI_DBG_WHEN   /* Mask for who and what field          */

/* The top-bits of who_what select who, a subsystem, component, type etc. */
#define VFI_LOG_LOG             0x80000000
#define VFI_LOG_FRMWRK          0x40000000
#define VFI_LOG_API             0x20000000
#define VFI_LOG_WHO             0xfffffe00

/* The bottom bits of who_what select a functional area, what to log */
#define VFI_LOG_FUNCALL         0x00000010
#define VFI_LOG_TRACE           0x00000020
#define VFI_LOG_WHATEVER        0x00000040
#define VFI_LOG_ERROR           0x00000080
#define VFI_LOG_WHAT            (~VFI_LOG_WHO & VFI_LOG_WHO_WHAT)

/* So from msb to lsb log_level can be thought of as defining who, what, when */
/* eg Location operators, function calls, when info is wanted */
/* VFI_DBG_LOCATION | VFI_DBG_FUNCALL | VFI_DBG_INFO */
#define VFI_LOG_EVERYONE        VFI_LOG_WHO
#define VFI_LOG_EVERYTHING      VFI_LOG_WHAT
#define VFI_LOG_ALWAYS          VFI_LOG_WHEN
#define VFI_LOG_ALL             (VFI_LOG_EVERYONE | VFI_LOG_EVERYTHING | VFI_LOG_ALWAYS)
#define VFI_LOG_DEFAULT         (VFI_LOG_EVERYONE | VFI_LOG_EVERYTHING | VFI_LOG_ERR)

/* VFI logger */
//static void vfi_debug(char *format, ...) __attribute__((unused,format(printf,1,2)));
static void vfi_log(int level, const char *format, ...)
{
	va_list args;
	va_start(args,format);
	vsyslog(LOG_USER|(level&VFI_DBG_WHEN), format, args);
	va_end(args);
}

#ifdef VFI_DEBUG
//extern unsigned int vfi_dbg_level;

#define VFI_DEBUG(l,f, arg...) if ((((l) & VFI_DBG_WHEN) <= (vfi_debug_level & VFI_DBG_WHEN)) && \
				     ((((l) & VFI_DBG_WHO_WHAT) & vfi_debug_level ) == ((l) & VFI_DBG_WHO_WHAT)) ) \
                                     vfi_log((l) & VFI_DBG_WHEN, ## arg)
#define VFI_DEBUG_SAFE(l,c,f,arg...) if ((c)) VFI_DEBUG((l),f, ## arg)
#define VFI_RESULT(x) ({int _myx = (x); if (_myx) VFI_DEBUG(MY_ERROR,"%s:%d:%s returns error %d\n",__FILE__,__LINE__, __FUNCTION__,_myx) ; _myx;});


#else
#define VFI_DEBUG(l,f,arg...) do {} while (0)
#define VFI_DEBUG_SAFE(l,c,f,arg...) do {} while (0)
#define VFI_RESULT(x) (x)
#endif

#endif

#ifdef _WIN32

/* We define this in OS-specific code which include's config.h  */
#ifndef _SWISH_PORT
#  include "win32/dirent.h"
#  include "win32/regex.h"
#endif

#include <stdlib.h>			/* _sleep() */
#include <process.h>		/* _getpid() */
#include <io.h>				/* _umask(), _mktemp, etc */
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

/* ifdef logic  */
#define NO_GETTOD			/* Win32 has no Get Time Of Day  */
#define NO_SYMBOLIC_FILE_LINKS		/* Win32 has no symbolic links */
#undef INDEXPERMS			/* Win32 has no chmod() */

/* Macros which rewrite values  */
#define SWISH_VERSION "2.1-dev-22"	/* Should we find a better way to handle this */
#define TMPDIR "c:\\windows\\temp"	/* Should this be taken from the environment at runtime */
#define DIRDELIMITER '\\'		/* Does this work right?  Might explain dir problems which have reported */

/* Internal SWISH-E File Access Modes */
#define FILEMODE_READ		"rb"	/* Read only */
#define FILEMODE_WRITE		"wb"	/* Write only */
#define FILEMODE_READWRITE	"rb+"	/* Read Write */

/* External POSIX File Access Modes */
#define O_RDWR _O_RDWR
#define O_CREAT _O_CREAT
#define O_EXCL _O_EXCL

/* Type definitions */
typedef int pid_t;			/* process ID */
typedef int mode_t;         /* file permission mode ID */

/* Rewrite ANSI functions to Win32 equivalents */
#define popen		_popen
#define pclose		_pclose
#define strcasecmp	stricmp
#define strncasecmp	strnicmp
#define sleep		_sleep
#define getpid		_getpid
#define umask       _umask
#define stat        _stat

#endif
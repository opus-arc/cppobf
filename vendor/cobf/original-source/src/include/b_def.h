// 
// BLIB                                                                   
// Global Defines
// Author: BB
// History:
// 1992-10-15	Created
// 1995-01-15	1.0
// 2005-12-19   Update to ANSI-C++
//


#if !defined __BDEFINE_H

#define __BDEFINE_H

#if defined unix
#define __CHECK		0
#else
#define __CHECK		2
#endif
#define __NOFREE	0
#define __MEMCHECK	0


//#define min(a, b) (((a) < (b)) ? (a) : (b))
//#define max(a, b) (((a) > (b)) ? (a) : (b))
//#define __MINMAX_DEFINED /* needed for BC 4.5 */

//typedef enum { false, true } bool;
//#define false 0
//#define true 1

#ifdef OLD_CPLUSPLUS
typedef int bool;
#endif

#if defined unix || defined __STDC__
#define cdecl
#endif

#define far
#define huge

#ifdef unix
#define PATH_SEPARATOR "/"
#define PATH_SEPARATOR_CHAR '/'
#else
#define PATH_SEPARATOR "\\"
#define PATH_SEPARATOR_CHAR '\\'
#endif

#endif

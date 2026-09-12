// 
// BLIB                                                                   
// Interface Memory Management
// Author: BB
// History:
// 1993-08-20	Created
// 1995-01-15	1.0
// 2005-12-19   Update to ANSI-C++
//


#if !defined __NEWALLOC_H

#define __NEWALLOC_H

#if !defined __MEMCHECK
#error __MEMCHECK undefined!
#endif

class BNew
{
public:
	BNew			();
	void setNewHandler	(std::new_handler pNewHandler);
};

extern BNew theBNew;

#if __MEMCHECK > 0

extern void  * cdecl operator new(size_t  a_size);
void checkPointer(void *);
void checkMemList(void);
void viewMemList(void);

#endif
#if __MEMCHECK > 0 || _NOFREE > 0
extern void cdecl operator delete(void *adr);
#endif

#endif


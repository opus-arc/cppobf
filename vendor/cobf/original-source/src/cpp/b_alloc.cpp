// 
// BLIB                                                                   
// Implementation Memory Management
// Author: BB
// History:
// 1993-08-20	Created
// 1995-01-15	1.0
// 2005-12-29   Update to ANSI-C++
//

#include <cstdio>
#include <cstdlib>

#include "b_obj.h"

//#include <new>

void bnew_handler(void);

using namespace std;


BNew::BNew()
{
	setNewHandler(bnew_handler);
}

void BNew::setNewHandler(new_handler pNewHandler)
{
	set_new_handler(pNewHandler);
}

BNew theBNew;

#if __MEMCHECK != 0

#define ALIGN sizeof (void *)
#define MAGIC 0x88

#define N_CHECK  40
//#define N_CHECK  1

#ifdef unix
#define MAX_MALLOCS 20000
#else
#define MAX_MALLOCS 4600
#endif

typedef struct mem_info
{
	bool	valid;	// valid Entry
	size_t	size;	// size of allocated memory block (incl. debug information)
	size_t	a_size; // allocated size (< size)
	struct mem_entry_head *meh;  // address of the memory block
} MemInfo;

typedef struct mem_entry_head
{
	MemInfo *mi; // back link to MemInfo entry
	struct mem_entry_tail *met;
	unsigned char magic[sizeof(int)];
} MemEntryHead;

typedef struct mem_entry_tail
{
	unsigned char magic[sizeof(int)];
} MemEntryTail;

//static MemInfo memInfoList[MAX_MALLOCS];
static MemInfo *memInfoList = NULL;

static int actMemIndex = 0;
static int usedEntries = 0;
static int maxMemIndex = 0;

#define MI_NO(p) (int) ((p) - &memInfoList[0])

#if 1
static int checktab_blocknr[] = { 2306, -1};
static int checktab_blocksiz[]= { 24, -1};
#else
static int checktab_blocknr[] = { 18, /*33,*/ -1};
static int checktab_blocksiz[]= { 12, /* 4,*/ -1};
#endif

static void internalViewMemList(void);

static void initMem()
{
	if (memInfoList != NULL) return;

	memInfoList = (MemInfo *)calloc(MAX_MALLOCS, sizeof(MemInfo));
	if (memInfoList == NULL)
	{
		fprintf(stderr, "initMem: no memory!\n");
		_exit(1);
	}
}

static void resetMem(void *p, size_t size)
{
	unsigned char huge *pAdr = (unsigned char huge *)p;
	for (size_t i = 0; i < size; ++i)
		  *pAdr++ = MAGIC;
}

/*
#ifdef unix
unsigned long coreleft(void)
{
	return 0;
}
#endif
*/
void *cdecl operator new(size_t  a_size)
{

	initMem();

//	cout << "new " << a_size << " Bytes\n";
	static int count;
	++count;

	if (count == N_CHECK)
	{
		checkMemList();
		count = 0;
	}
	MemInfo *pMemInfo;

	int i;
	for (i = 0; i < MAX_MALLOCS; ++i)
	{
		pMemInfo = &memInfoList[actMemIndex];
		if (!pMemInfo->valid) break;
		if (++actMemIndex == MAX_MALLOCS)
			actMemIndex = 0;
	}
	if (pMemInfo->valid)
	{
		fprintf(stderr, "new: no free entry (increase MAX_MALLOCS)!\n");
		babort(1);
	}
	if (actMemIndex > maxMemIndex)
		maxMemIndex = actMemIndex;
	for (i = 0; ; ++i)
	{
		if (checktab_blocknr[i] == -1) break;
		if (checktab_blocknr[i] == actMemIndex && checktab_blocksiz[i] == (int)a_size)
		{
			fprintf(stderr, "Memory nlock %d (%ld Bytes) used!\n", MI_NO(pMemInfo), (long)a_size);
			fprintf(stderr, "");
		}
	 }
	size_t size = (a_size + ALIGN - 1) / ALIGN * ALIGN;
	size += sizeof(MemEntryHead) + sizeof(MemEntryTail);
	MemEntryHead *pMEH;
	if ((pMEH = (MemEntryHead *)calloc(1, size)) == 0)
	{
		fprintf(stderr, "new: No memory!\n");
		babort (1);
	}
	++usedEntries;

	resetMem(pMEH, size);
	pMEH->mi = pMemInfo;
	pMEH->met = (MemEntryTail *)((unsigned char *) pMEH + size - sizeof(MemEntryTail));

	pMemInfo->meh = pMEH;
	pMemInfo->size = size;
	pMemInfo->a_size = a_size;
	pMemInfo->valid = true;

	return (void *)(pMEH + 1);
}

void checkMemInfo(MemInfo *pMemInfo)
{
	MemEntryHead *pMEH = pMemInfo->meh;
	for (int i = 0; i < sizeof(pMEH->magic); ++i)
		if (pMEH->magic[i] != MAGIC || pMEH->met->magic[i] != MAGIC)
		{
			fprintf(stderr, "checkMemInfo: At No. %d written behind the alloacated buffer!\n",
				MI_NO(pMemInfo));
			internalViewMemList();
			babort (1);
		 }
}

//#pragma argsused

void cdecl operator delete(void *adr)
{
	if (aborting)
		return;

//	cout << "delete\n";
	if (adr == NULL)
	{
		cout << "Warning: deleting null pointer!\n";
		return;
	}
#if __NOFREE < 2
	static int count;
	++count;
	if (count == N_CHECK)
	{
		checkMemList();
		count = 0;
	}
	MemEntryHead *pMEH = (MemEntryHead *)adr;
	--pMEH;
	MemInfo *pMemInfo = pMEH->mi;
	if (pMemInfo->meh == pMEH && pMemInfo->valid)
	{
		checkMemInfo(pMemInfo);
		pMemInfo->valid = false;
#if __NOFREE < 1
		resetMem(pMEH, pMemInfo->size);
		free((char *)pMEH);
#endif
		--usedEntries;
//		fprintf(stdout, "delete Block Nr. %d (%ld Bytes)\n", MI_NO(pMemInfo), (long) pMemInfo->a_size);
		return;
	}
	fprintf(stderr, "delete block No. %d (%ld Bytes)\n", MI_NO(pMemInfo), (long) pMemInfo->a_size);
	fprintf(stderr, "delete: Can't free memory!\n");
	babort (1);
#endif
}

void checkMemList()
{

	initMem();

	size_t n_bytes = 0;
	MemInfo *pMemInfo;
	for (int i = 0; i < MAX_MALLOCS; ++i)
	{
		pMemInfo = &memInfoList[i];
		if (pMemInfo->valid)
		{
			checkMemInfo(pMemInfo);
			n_bytes += pMemInfo->a_size;
		}
	 }
	fprintf(stderr, "checkMemList: %ld Bytes in %d Entries (maxEntry %d)\n",
			n_bytes, usedEntries, maxMemIndex);
}

bool testPotentialBObject(void *adr)
{
	BObject *pBObject = (BObject *)adr;
	return pBObject->isValidBObject();
}

void checkBObject(BObject *pBObject)
{
	char answer[80];
	if (!pBObject->isValidBObject())
	{
		printf("For sure not a valid BObject!\n");
		return;
	}
	printf("Might be an BObject!\n");
	printf("Check type (y/n)?");
	fflush(stdout);
	scanf("%s", answer);
	if (*answer == 'y')
		printf("%s\n", pBObject->nameOf());
	printf("Print BObject (y/n)? ");
	fflush(stdout);
	scanf("%s", answer);
	if (*answer == 'y')
	{
		cout << *pBObject << endl;
	}
}

void printMemInfo(MemInfo *pMemInfo)
{
	unsigned char huge *adr;  // address of the memory block
	if (!pMemInfo->valid)
	{
		printf("Entry currently not used!\n");
		return;
	}
	printf("Size (in Bytes): %ld\n", (long) pMemInfo->a_size);
	adr = (unsigned char huge *)pMemInfo->meh + sizeof(MemEntryHead);
	printf("Address: %p ", adr);
	int i;
	for (i = 0; i < 10; ++i)
		printf("%2X ", adr[i]);
	printf("\t\t");
	unsigned char c;
	for (i = 0; i < 10; ++i)
	{
		c = adr[i];
		if (c < 32 || c == 127 || c == 255) c = '.';
		printf("%c", c);
	}
	printf("\n");
	checkBObject((BObject *) adr);
}

void printUsedMemEntryNumbers()
{
	MemInfo *pMemInfo;
	char *bobjectflag = "";
	int count = 0;

	for (int i = 0; i < MAX_MALLOCS; ++i)
	{
		bobjectflag="";
		pMemInfo = &memInfoList[i];
		if (pMemInfo->valid)
		{
			void *adr;
			adr = (unsigned char huge *)pMemInfo->meh + sizeof(MemEntryHead);
			if (testPotentialBObject(adr))
				bobjectflag = "B";
			printf("%4d[%1s%2d]", i, bobjectflag, (int) pMemInfo->a_size);
			++count;
			if (count == 7)
			{
				printf("\n");
				count = 0;
			}
			else
				printf("  ");
		}
	}
	printf("\n");
}

static void internalViewMemList()
{
	char answer[80];
	MemInfo *pMemInfo;
	int noEntry = -1;
	while (1)
	{
		printf("<number> for entry, 'x' for exit, 'u' for numbers of all used entries:\n");
		scanf("%s", answer);
		if (*answer == 'x')
			break;
		if (*answer == 'u')
		{
			printUsedMemEntryNumbers();
			continue;
		}
		sscanf(answer, "%d", &noEntry);
		printf("Entry No. %d\n", noEntry);
		if (noEntry < 0 || noEntry >= MAX_MALLOCS)
		{
			printf("Entry not found!\n"
				"maximum entry: %d (maximal %d)\tCount used: %d\n", maxMemIndex, MAX_MALLOCS, usedEntries);
		}
		else
		{
			pMemInfo = &memInfoList[noEntry];
			printMemInfo(pMemInfo);
		}
	}
}

void viewMemList()
{
	checkMemList();
	internalViewMemList();
}
#endif


#if __MEMCHECK == 0 && __NOFREE > 1
#pragma argsused

void cdecl operator delete(void *adr)
{
}

#endif

void bnew_handler (void)
{
	cerr << endl <<
		"fatal error: no memory!\n" << endl;
	babort(1);
}


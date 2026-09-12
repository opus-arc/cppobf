// 
// BLIB                                                                   
// Implementation BObject
// Author: BB
// History:
// 1993-08-06	Created
// 1995-01-15	1.0
// 2005-12-19   Update to ANSI-C++
//

#include <cstdio>
#include <cstdlib>

#include "b_obj.h"

using namespace std;

bool operator ==  ( const TypeBObjectId &aTypeBObjectId1, const TypeBObjectId &aTypeBObjectId2 )
{
	return strcmp(aTypeBObjectId1, aTypeBObjectId2) == 0;
}

bool operator !=  ( const TypeBObjectId &TypeBObjectId1, const TypeBObjectId &TypeBObjectId2 )
{
	return (bool) !(TypeBObjectId1 == TypeBObjectId2);
}

//DefineBObjectClass(BObject)

BObject Dummy_BObject;

BObject::BObject(const BObject &)
{
	cerr << "\ninternal error: BObject::BObject(const BObject &) called!\n";
	babort (1);
}

BObject::~BObject()
{
#if __MEMCHECK > 0
	check("BOBject::~BObject()");
	magic = 0;
#endif
}

#if __MEMCHECK > 0
void BObject::check( const char *message)
{
	if (!isValidBObject())
	{
		cerr << "\ninternal error: '" << message << "' failed!\n";
		babort(1);
	}
}
#endif

void BObject::notImplemented(const char *fname) const
{
	cerr << "internal error: " << nameOf() << "::" <<
		fname << "() not implemented!\n";
	babort(1);
}

const char *BObject::getKey() const
{
#if __MEMCHECK == 0
	notImplemented("getKey");
	return NULL;
#else
	return nameOf(); // Damit bessere Debug-Moeglichkeiten!
#endif
}

ostream &BObject::printOut(ostream &aOstream)
{
	aOstream << nameOf() << " [" << *this << "]";
	return aOstream;
}

bool BObject::hasKey(const char *name) const
{
	return (bool) (strcmp(getKey(), name) == 0);
}

bool BObject::hasKeyPrefix(const char *name) const
{
	return (bool) (strncmp(getKey(), name, (int) strlen(name)) == 0);
}

bool BObject::isEqual(BObject &aBObject)
{
	return hasKey(aBObject.getKey());
}

// Hash-Funktion nach Kernighan/Ritchie "Programmieren in C" S. 139

unsigned int BObject::getStringHashVal(const char *name)
{
	unsigned hashval;
	for (hashval = 0; *name != '\0'; name++)
		hashval = *name + 31 * hashval;
	return hashval;
}

bool operator == (BObject &aBObject1, BObject &aBObject2 )
{
	if (aBObject1.isA() == aBObject2.isA())
		return aBObject1.isEqual(aBObject2);
	return false;
}

bool operator != (BObject &aBObject1, BObject &aBObject2 )
{
	return (bool) !(aBObject1 == aBObject2);
}

ostream &operator <<(ostream &aOstream, BObject &aBObject)
{
	aBObject.print(aOstream);
	return aOstream;
}

int BObject::compareKey(BObject *pBObject1, BObject *pBObject2)
{
	return strcmp(pBObject1->getKey(),
		      pBObject2->getKey()) > 0;
}

StringObject::StringObject(const char *aKey)
{
	key = saveString(aKey);
}


int StringWithCount::compareCountReverse(BObject *pBObject1, BObject *pBObject2)
{
	PRECONDITION(pBObject1->isKindOf(Id(StringWithCount)) && pBObject2->isKindOf(Id(StringWithCount)));
	return ((StringWithCount *)pBObject1)->getCount() <  ((StringWithCount *) pBObject2)->getCount();
}

int StringWithCount::compareCount(BObject *pBObject1, BObject *pBObject2)
{
	PRECONDITION(pBObject1->isKindOf(Id(StringWithCount)) && pBObject2->isKindOf(Id(StringWithCount)));
	return ((StringWithCount *)pBObject1)->getCount() > ((StringWithCount *) pBObject2)->getCount();
}


NumObject::NumObject(long aKey)
{
	 keynum = aKey;
	 getNumHashString(keynum, keynumstring);
}

void NumObject::getNumHashString(long key, char *keystring)
{
	int i;
	for (i = 0; i < (int) sizeof(long); ++i)
	{
		char c = (char) (((char*)&key)[i] + 17);
		if (c == 0) c = 3;
		keystring[i] = c;
	}
	keystring[i] = '\0';
}

char *saveString2(const char *s1, const char *s2)
{
	char *newstr;
	int len1 = strlen(s1), len2= strlen(s2);
	newstr = new char [len1+len2+1];
	strcpy(newstr, s1);
	strcat(newstr, s2);
	return newstr;
}

bool aborting = false;

void babort(int exitcode)
{
	cout.flush();
	cerr.flush();
	fprintf(stderr, "program aborted (exitcode %d)\n", exitcode);
	fflush(stdout);
	fflush(stderr);
	aborting = true;
	exit (1);
}


#if __CHECK > 0

void AssertFail(const char *text, const char *expr, const char *file, int line)
{
	cerr << endl << text << ": " << expr << "\nin file " << file <<
		", line " << line << endl;
	babort(2);
}

#endif


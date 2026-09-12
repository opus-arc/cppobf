
// 
// BLIB                                                                   
// Interface BObject 
// Author: BB
// History:
// 1992-08-06	Created
// 1996-03-28	1.0
// 2005-12-19   Update to ANSI-C++
//


#if !defined __BOBJECT_H

#define __BOBJECT_H

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstring>



#include "b_def.h"
#include "b_alloc.h"

//#ifdef unix		// quick&dirty hack .....
//#include <memory.h>
//#define memmove(dst, src, lg) memcpy(dst, src, lg)
//#endif

#if __CHECK < 1
#define PRECONDITION(p)   ((void)0)
#else
#define PRECONDITION(p)   ((p) ? (void)0 : (void) AssertFail(		\
		    "Precondition violated", #p, __FILE__, __LINE__))
#endif


#if __CHECK < 2
#define CHECK(p)    ((void)0)
#else
#define CHECK(p)    ((p) ? (void)0 : (void) AssertFail(			\
		    "Check failed", #p, __FILE__, __LINE__))
#endif

#if __CHECK > 0
void AssertFail(const char *text, const char *expr, const char *file, int lineno);
#endif

class BObject;

class TypeBObjectId
{
	const char *typeId;
public:
	TypeBObjectId(const char *aTypeId) { typeId = aTypeId; }

	operator const char *	() const
							{ return typeId; }

	friend bool operator ==  ( const TypeBObjectId &aTypeBObjectId1, const TypeBObjectId &aTypeBObjectId2 );

	friend bool operator !=  ( const TypeBObjectId &TypeBObjectId1, const TypeBObjectId &TypeBObjectId2 );

};


#define Id(Class) #Class

class BObject {
public:
	BObject				(const BObject &);

	static const char *classId	()
								{ return "BObject"; }

	virtual ~BObject	(); // to make all destructors virtual ...

	virtual TypeBObjectId	isA	() const
							{ return TypeBObjectId(BObject::classId()); }

	const char *nameOf	() const
						{ return isA(); }

	bool isMemberOf		( const TypeBObjectId aBObjectId) const
						{ return isA() == aBObjectId; }

	virtual bool isKindOf	( const TypeBObjectId aBObjectId) const
							{ return  TypeBObjectId(BObject::classId()) == aBObjectId; }

	void notImplemented		(const char *fname) const;

	virtual bool isEqual	(BObject &aBObject);	// Attention: shouldn't be overloaded ... 
	virtual void print		(std::ostream &aOstream)
							{ aOstream << getKey(); }

							std::ostream &printOut( std::ostream & );

	virtual const char *getKey	() const;

	bool hasKey		(const char *name) const;

	bool hasKeyPrefix	(const char *name) const;

	static unsigned int getStringHashVal(const char *name);

	unsigned int getHashVal		() const
								{ return getStringHashVal(getKey()); }

	operator const char *	() const
							{ return getKey(); }

	friend bool operator ==	( BObject &, BObject &);
	friend bool operator !=	( BObject &, BObject &);
	friend std::ostream &operator <<	( std::ostream &, BObject &);


	static int compareKey(BObject *pBObject1, BObject *pBObject2);

#if __MEMCHECK > 0
protected:
	enum { BOBJECT_MAGIC = 0x4B4B };
	unsigned int magic;
public:
	BObject		()
				{ magic = BOBJECT_MAGIC; }
	bool isValidBObject()
						{ return magic == BOBJECT_MAGIC; }
	void check( const char *message);
#else
	void check( const char *) {}
	BObject		(){}
#endif
};


typedef int (*CmpFunction) (BObject *, BObject *);

#define RTTIBObject(Classname, BaseClassname) \
public:\
	static const char *classId	() \
				{ return #Classname; } \
\
	virtual TypeBObjectId	isA() const \
				{ return TypeBObjectId(Classname::classId()); }\
\
	virtual bool isKindOf(const TypeBObjectId aBObjectId) const \
			{\
			  return (bool)  (TypeBObjectId(Classname::classId()) == aBObjectId ||\
			  BaseClassname::isKindOf(aBObjectId));\
			}

class StringRefObject: public BObject
{
	RTTIBObject(StringRefObject, BObject)
protected:
	char *key;
public:
	StringRefObject	(char *aKey)
					{ key = aKey; }

	const char *getKey	() const
						{ return key; }
};

class StringObject: public BObject
{
	RTTIBObject(StringObject, BObject)
protected:
	char *key;
public:
	StringObject		(const char *aKey);

	~StringObject		()
						{ delete [] key; }

	const char *getKey	() const
						{ return key; }
};

class StringWithCount: public StringObject
{

	RTTIBObject(StringWithCount, StringObject)

	unsigned long count;
public:
	StringWithCount(const char *name):StringObject(name) { count = 1;}
	StringWithCount(const char *name, unsigned long inc):StringObject(name) { count = inc;}

	void resetCount()			{ count = 0; }
	void incCount()				{ ++count; }
	void addCount(unsigned long inc)	{ count += inc; }
	void setCount(unsigned long val)	{ count = val; }
	unsigned long getCount() const		{ return count; }

	static int compareCount(BObject *pBObject1, BObject *pBObject2);
	static int compareCountReverse(BObject *pBObject1, BObject *pBObject2);

};

class NumObject: public BObject
{

	RTTIBObject(NumObject, BObject)

	long keynum;
	char keynumstring[sizeof(long)+1];

public:
	NumObject		(long aKey);

	virtual void print	(std::ostream &aOstream)
						{ aOstream << keynum; }

	virtual const char *getKey	() const
								{ return keynumstring; }

	long getNum			() const
						{ return keynum; }

	static void getNumHashString	(long key, char *keystring);
};

extern bool aborting;
void babort(int exitcode);

char *saveString2(const char *s1, const char *s2);
inline char *saveString(const char *s)
{
	return saveString2(s, "");
}

#endif

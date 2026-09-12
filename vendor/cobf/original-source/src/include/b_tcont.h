// 
// BLIB                                                                   
// Interface Template Container Classes 
// Author: RB+BB
// History:
// 1992-08-11	Created
// 1995-01-15	1.0
// 2005-12-20   Update to ANSI-C++
//


#if !defined __BTCOLL_H

#define __BTCOLL_H

#include "b_set.h"


template <class T>
class Set: public XHashTable
{
public:
	Set	( int initSize = INIT_SIZE):
		XHashTable (initSize)
		{}

	Set	(T &aT, int initSize = INIT_SIZE1):
		XHashTable (aT, initSize)
		{}

	Set	(Set &aSet):
		XHashTable (aSet)
		{}

	void add	(T &aT)
			{ XHashTable::addElem(aT); }

	bool addTest	(T &aT, int delflag = false)
			{ return XHashTable::addElemTest(aT, delflag); }

	T *get		(const char *name)
			{ return (T*) XHashTable::getElem(name); }

	T &getSafe	(const char *name)
			{ return (T&) XHashTable::getSafeElem(name); }

	T	&operator[]	( int i )
				{ return (T&) XHashTable::operator[]( i ); }
};

template <class T>
class List: public XArray
{
public:
	List		(int size = INIT_SIZE):
			XArray(size)
			{}

	List	(T &aT,  int initSize = INIT_SIZE1):
		XArray (aT, initSize)
		{}

	List	(List &aList):
		XArray (aList)
		{}

	void add	(T &aT)
			{ XArray::addElem(aT); }

	void add	(T &aT, int i)
			{ XArray::insertAtIndex(aT, i); }

	bool addTest	(T &aT, int delflag = false)
			{ return XArray::addElemTest(aT, delflag); }


	T *get		(const char *name)
			{ return (T*) XArray::getElem(name); }

	T &getSafe	(const char *name)
			{ return (T&) XArray::getSafeElem(name); }

	T	&operator[]	( int i )
				{ return (T&) XArray::operator[]( i ); }
};

class StringDictionary: public Set <StringWithCount>
{
	RTTIBObject(StringDictionary, Set <StringWithCount>)

public:
	StringDictionary( int initsize= INIT_SIZE):
		      Set<StringWithCount>(initsize)
		      { ownsElements(true); }
	const char *registerString(const char *str);
};

#endif

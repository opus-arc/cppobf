// 
// BLIB                                                                   
// Implementation XHashTable
// Author: RB+BB
// History:
// 1993-02		Created
// 1996-04-01	1.0
// 2005-12-19   Update to ANSI-C++
//


#include <limits.h>
#include "b_tcont.h"
#include <cstring>

using namespace std;

extern BObject Dummy_BObject;
#define DELETED &Dummy_BObject

#define SORTED_OUTPUT 1

void XHashTable::reset()
{
	if (ownsElementsFlag == true)
		for (int i = 0; i < size(); ++i)
			delete &(*this)[i];

	if (hashTableSize > 0)
		  delete[] hashTable;
	clearSize();
	init(0);
}


void XHashTable::init (int initSize)
{
	countDelElements = 0;

	if (initSize == 0)
		hashTableSize = 0;
	else
	{
		// Aufrunden zur naechsten Zweierpotenz:
		// x & (x - 1) loescht das niedrigstwertige 1er-Bit von x
		// => wenn x & (x - 1) == 0, dann ist x Zweierpotenz

		if (initSize < 8)
			hashTableSize = 8; // aus Effizienzgruenden
		else
		{
			initSize = (initSize << 1) - 1;
			CHECK(initSize > 0);
			while (initSize &= (hashTableSize = initSize) - 1)
				;
			CHECK(hashTableSize * sizeof(BObject *) < INT_MAX);
		}
		hashTable = new BObject *[ hashTableSize ];

		int i;
		for (i = 0; i < hashTableSize; ++i)
			hashTable[i] = NULL;

	}
	cursor = 0;
	indexCountOfCursor = -1; // undefiniert
}

// moveCursorTo liefert
// 	true, falls vorhanden. hashTable[cursor] zeigt auf das Element.
// 	false sonst. Falls !isFull(), zeigt *cursor auf den ersten freien Platz.

bool XHashTable::moveCursorTo(const char *name)
{
	PRECONDITION(name != NULL);

	if (hashTableSize == 0)
		return false;
	unsigned int mask = hashTableSize-1;
	indexCountOfCursor = -1;
	if (hashTableSize <= 1)
		cursor = 0;
	else
		cursor = BObject::getStringHashVal(name) & mask;

	int count = 0;
	while (count < hashTableSize && hashTable[cursor] != NULL)
	{
		++count;
		if (hashTable[cursor] != DELETED && hashTable[cursor]->hasKey(name))
		{
			hashTable[cursor]->check("XHashTable::moveCursorTo");
			return true; // "name" gefunden
		}
		cursor = (cursor + count)  &  mask;
//		cout << "cursor fuer " << name << ": " << cursor << endl;
	}
//	cout << "return: cursor fuer " << name << ": " << cursor << endl;
	return false; // leerer Platz gefunden - oder voll
}


void XHashTable::rehash (unsigned int newSize)
{
	BObject **oldHashTable = hashTable;
	unsigned int oldSize = hashTableSize;

	init (newSize);
	if (oldSize > 0)
	{
		BObject *pBObject;

		unsigned int i;
		for (i = 0; i < oldSize; ++i)
		{
			pBObject = oldHashTable[i];
			if (pBObject != NULL && pBObject != DELETED)
				if (!moveCursorTo(pBObject->getKey()))
					hashTable[cursor]= pBObject;
		}
		delete[] oldHashTable;
	}
	indexCountOfCursor = -1;

}

XHashTable &XHashTable::operator= (XHashTable &aXHashTable)
{
	reset();
	init (aXHashTable.size());
	(*this) += aXHashTable;
	return *this;
}


BObject *XHashTable::getElem (const char *name)
{
	if (moveCursorTo(name))
		return hashTable[cursor];
	return NULL;
}


bool XHashTable::del(const char *name)
{
	if (moveCursorTo(name))
	{
		delAtCursor ();
		return false;
	}
	return true;
}

void XHashTable::addElem(BObject &aBObject)
{
//	cout << "add(" << aBObject << ") ";
	indexCountOfCursor = -1;
	if (hashTableSize == 0)
		init(1);
	else
		if (isFull ())
			rehash (hashTableSize << 1);

	CHECK(hashTableSize >= size());

	if (!moveCursorTo(aBObject.getKey()))
	{
		hashTable[cursor]= &aBObject;
		incSize();
	}
#if 0
	indexCountOfCursor = -1;
	cout << "XHashTab::add(" << aBObject << ") = " << *this << endl;
	indexCountOfCursor = -1;
#endif
	return;
}


BObject &XHashTable::getObjectAtIndex(int index)
{
	PRECONDITION (index >= 0 && index < size());

	if (indexCountOfCursor < 0 || index < indexCountOfCursor)
	{
		indexCountOfCursor = -1;
		cursor = -1;
	}

	while (indexCountOfCursor < index)
	{
		BObject *p;

		++cursor;
		while ((p = hashTable[cursor]) == NULL || p == DELETED)
			++cursor;
		++indexCountOfCursor;
	}
	hashTable[cursor]->check("XHashTable::getObjectAtIndex");
	return *hashTable[cursor];
}

//#include <iomanip.h>

void XHashTable::print(ostream &aOstream)
{
#if 0
	for (unsigned int i= 0; i < hashTableSize; ++i)
	{
		BObject *pBObject = hashTable[i];
		cout << setw(4) << i << setw(0) << ": " << pBObject <<" ";
		if (pBObject == DELETED)
			cout << "(DELETED)";
		if (pBObject != NULL && pBObject != DELETED)
		{
			pBObject->check("XHashTable::print");
			cout << "(" << *pBObject << ")";
		}
		cout << "\n";
	}
#endif
#if SORTED_OUTPUT > 0
	List<BObject> sortlist;
	sortlist.setSeparator(separator);
	sortlist += *this;
	sortlist.sort();
	sortlist.print(aOstream);
#else
	this->print(aOstream);
#endif
}

bool XHashTable::isEqual (BObject &x)
{
	XHashTable &aXHashTable = (XHashTable &)x;

	if (size() != aXHashTable.size())
		return false;

	for (int i= 0; i < aXHashTable.size(); ++i)
		if (!isIn ((aXHashTable[i]).getKey ()))
			return false;
	return true;
}


void XHashTable::delAtCursor ()
{
	if (ownsElementsFlag)
		delete hashTable[cursor];
	hashTable[cursor] = DELETED;
	decSize();
	++countDelElements;
	indexCountOfCursor = -1;
}


bool XHashTable::isFull () const
{
	return (bool) (size() + countDelElements >= hashTableSize / 2);
}

XHashTable &XHashTable::operator -= (XContainer &aXContainer)
{
//	cout << "Differnzmenge\n" << *this << "\n - \n" << aXContainer << "\nist:\n";
	if (size() < aXContainer.size())
	{
		XHashTable tmpXHashTable;
		for (int i = 0; i < size(); ++i)
			if (!aXContainer.isIn((*this)[i]))
				tmpXHashTable.addElem((*this)[i]);
		reset();
		(*this) += tmpXHashTable;
	}
	else
	{
		for (int i = 0; i < aXContainer.size(); ++i)
			del(aXContainer[i]);
	}
//	cout << *this << "\n";
	return *this;
}

XHashTable &XHashTable::operator *= (XContainer &aXContainer)
{
	XHashTable tmpXHashTable;
//	cout << "Schnittmenge\n" << *this << "\n * \n" << aXContainer << "\nist:\n";
//	XHashTable *seekContainer, *testContainer;
	if (size() < aXContainer.size())
	{
		for (int i = 0; i < size(); ++i)
			if (aXContainer.isIn((*this)[i]))
				tmpXHashTable.addElem((*this)[i]);
	}
	else
	{
		for (int i = 0; i < aXContainer.size(); ++i)
			if (isIn(aXContainer[i]))
				tmpXHashTable.addElem(aXContainer[i]);
	}
	reset();
	(*this) += tmpXHashTable;
//	cout << *this << "\n";
	return *this;
}

///////////////////////////////////////////////////////////////////////


const char *StringDictionary::registerString(const char *str)
{
	StringWithCount *pStringObject;
	if ((pStringObject = get(str)) == NULL)
	{
		pStringObject = new StringWithCount(str);
		add(*pStringObject);
	}
	else
		pStringObject->incCount();
	return pStringObject->getKey();
}


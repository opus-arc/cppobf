// 
// BLIB                                                                   
// Implementation XArray
// Author: BB
// History:
// 1993-08-17	Created
// 1995-01-15	1.0
// 2005-12-19   Update to ANSI-C++
//


#include <climits>

#include "b_cont.h"

using namespace std;

void XArray::init(int initSize)
{
	PRECONDITION(initSize * sizeof(BObject *) < INT_MAX / 2);

	if (initSize == 0)
		arraySize = 0;
	else
	{
		// Aufrufen zur naechsten Zweierpotenz:
		// x & (x - 1) loescht das niedrigstwertige Bit von x
		// => wenn x & (x - 1) == 0, dann ist x Zweiterpotenz

		initSize = (initSize << 1) - 1;
		while (initSize &= (arraySize = initSize) - 1)
			;
		array = new BObject *[ arraySize ];

		for (int i= 0; i < arraySize; ++i)
			array[i] = NULL;
	}
}

void XArray::reset()
{
#if __MEMCHECK > 0
	if (ownsElementsFlag)
		cout << "XArray::reset(): " << *this << "\n";
	// ansonsten konnen die Referenzen schon ungueltig sein!
#endif
	for (int i = 0; i < size(); ++i)
	{
		if (ownsElementsFlag == true)
			delete &(*this)[i];
		array[i] = NULL;
	}

	if (arraySize > 0)
	{
		delete[] array;
		arraySize = 0;
	}

	clearSize();
}

void XArray::reorg(int newsize)
{
	BObject **oldarray = array;
	int oldsize = arraySize;

	init(newsize);

	if (oldsize > 0)
	{
		for (int i = 0; i < oldsize; ++i)
			array[i] = oldarray[i];
		delete[] oldarray;
	}
}

XArray &XArray::operator= (XArray &aArray)
{
	reset();
	init (aArray.size());
	*this += aArray;
	return *this;
}

BObject &XArray::getObjectAtIndex(int i)
{
	PRECONDITION(i >= 0 && i < size());
	array[i]->check("XArray::getObjectAtIndex");
	return *array[i];
}

void XArray::addElem(BObject &aBObject)
{
	if (arraySize == 0)
		init(1);
	else
		if (size() + 1 >= arraySize)
			reorg(arraySize << 1);
	array[size()] = &aBObject;
	incSize();
#if 0
	cout << "Xarray::add(" << aBObject << ") = " << *this << endl;
#endif
}

void XArray::removeLast()
{
	PRECONDITION(size() > 0);

	if (ownsElementsFlag == true)
		delete array[size()-1];
	decSize();
}

void XArray::delAtIndex(int i)
{
	PRECONDITION(i >= 0 && i < size());
	if (ownsElementsFlag == true)
		delete array[i];
	memmove(&array[i], &array[i+1], (size() - i) * sizeof(&array[0]));
	decSize();
}

void XArray::swap(int index1, int index2)
{
	PRECONDITION(index1 >= 0 && index1 < size());
	PRECONDITION(index2 >= 0 && index2 < size());
	BObject *temp = array[index1];
	array[index1] = array[index2];
	array[index2] = temp;
}

void XArray::insertAtIndex(BObject &aBObject, int i)
{
	PRECONDITION(i >= 0 && i <= size());
	addElem(aBObject);
	BObject *temp = array[size()-1];
	memmove(&array[i+1], &array[i], (size() - i - 1) * sizeof(&array[0]));
	array[i] = temp;
}

void XArray::sort(CmpFunction cmp)
{
	int i;
	for (i = 0; i < size(); ++i)
		heapInsert(i, array[i], cmp);
	for (i = size(); i >= 1; --i)
		heapDeleteTop(i, cmp);
}

#define PARENT(pos)	(((pos) - 1) / 2)
#define CHILD(pos)	(2*(pos) + 1)

void XArray::heapInsert(int actHeapSize, BObject *pElement, CmpFunction cmp)
{
	BObject *pChild, *pParent;	/* Pointer auf Elemente im Heap */
	unsigned int nChildPos, nParentPos;
				/* Positionen im Array (=linearisierter Baum) */


/* an letzter Stelle einfgen */
	array[nChildPos = actHeapSize++] = pElement;

/* solange aktuelles Element nicht erstes/oben (nChildPos > 0)  UND */
/* der Vergleich des aktuellen Elements mit dem Parent TRUE ergibt */

	while (nChildPos &&
		(*cmp)(pChild = array[nChildPos],
				pParent = array[nParentPos = PARENT(nChildPos)]))
	{

	/*
		vertausche die beiden  UND
		mach mit dem Parent als neues aktuelles Element weiter
	*/

		array[nChildPos] = pParent;
		array[nChildPos = nParentPos] = pChild;
	}
}

void XArray::heapDeleteTop(int actHeapSize, CmpFunction cmp)
{
	 int nParentPos = 0;
	int nChildPos;
	BObject *pParent = array[0]; /* merke Spitze */
	BObject *pChild;


/* kopiere letztes Element an die Spitze und verringere unActHeapSize */

	array[0] = array[--actHeapSize];

/* bringe gemerkte Spitze in den sortierten Teil des Arrays */
	array[actHeapSize] = pParent;

/* 'wackele' noch unpassende Spitze nach unten */
	while ((nChildPos = CHILD(nParentPos)) < actHeapSize)
	{
		pParent = array[nParentPos];
		pChild = array[nChildPos];

/* gibt es ein zweites Child  UND  ist es kleines als erstes Child? */
		if (nChildPos + 1 < actHeapSize &&
			(*cmp)(array[nChildPos + 1], pChild))
			pChild = array[++nChildPos];

/* ist weiter nach unten zu 'wackeln'? */
		if ((*cmp)(pChild, pParent))
		{
/* tauschen und neue Position festlegen */
			array[nParentPos] = pChild;
			array[nParentPos = nChildPos] = pParent;
		}
		else	/* richtige Lage gefunden */
			break;
	}
}

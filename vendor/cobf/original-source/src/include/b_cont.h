// 
// BLIB                                                                   
// Interface XContainer, XArray
// Author: BB
// History:
// 1993-08-10	Created
// 1995-01-15	1.0
// 2005-12-19   Update to ANSI-C++
//


#if !defined __BCOLLECT_H

#define __BCOLLECT_H

#include "b_obj.h"

#define INIT_SIZE	16
#define INIT_SIZE1      16

///////////////////////////////////////////////////////////////////////
// Interface abstrakte Klasse Container
///////////////////////////////////////////////////////////////////////

// Klasse definiert allgemeine Container-Eigenschaften.
// Abgeleitete Subklassen muessen die Funktion reset() definieren,
// die den Container in einen Zustand analog unmittelbar nach seinr
// Konstruktion ueberfuehrt.

// Copy-Konstruktoren und Assignment-Operatoren muessen in
// abgeleiteten instanziierbaren Klassen definiert werden!!
class XContainer: public BObject
{

	RTTIBObject(XContainer, BObject)

	const char *separator;
	int elementsInContainer;

protected:
	bool ownsElementsFlag;
	void decSize	()
				{
				 PRECONDITION(elementsInContainer > 0);
				 --elementsInContainer;
				}

	void incSize	()
				{
				  ++elementsInContainer;
				}

	void clearSize		()
				{
					elementsInContainer = 0;
				}
	void init		();

public:
	XContainer		()
					{ init(); }

	virtual void reset	() = 0;       		// setze Container zu einem Zustand
											// unmittelbar nach der
											// Konstruktion des Containers
	XContainer &append	( XContainer & );	// fuege weiteren Container an

	virtual void print	( std::ostream &);

	void ownsElements	(bool aFlag)
						{ ownsElementsFlag = aFlag; }

	void setSeparator		(const char *aString)
					{ separator = aString; }

	int  size		() const
					{ return elementsInContainer; }

	bool isEmpty	() const
					{ return (bool) (size() == 0); }

	virtual void addElem	( BObject &aBObject ) = 0;
	bool addElemTest		(BObject &aBObject, int delflag = false);


	virtual BObject &getObjectAtIndex
					( int index ) = 0;


	virtual BObject *getElem	( const char *name );
	BObject &getSafeElem		( const char *name );

	virtual bool isIn			( const char *name )
						{ return getFirstIndexOfObject(name) >= 0; }


	BObject &operator []		( int index )
					{ return getObjectAtIndex( index ); }

	XContainer &operator= 		(XContainer &aXContainer);

	XContainer	&operator += 	( XContainer &aContainer )
					{ return append(aContainer); }

	void getObjectsKindOf	(TypeBObjectId aBObjectId, XContainer &retContainer);

	int getFirstIndexOfObject	(BObject &);
	int getFirstIndexOfObject	(const char *name);
};

class XArray: public XContainer
{

	RTTIBObject(XArray, XContainer)

	int arraySize;
	BObject **array;

	void init		(int initSize);
	void reorg		(int newSize);

	void swap		(int index1, int index2);

	void heapInsert(int actHeapSize, BObject *pElement, CmpFunction cmp);
	void heapDeleteTop(int actHeapSize, CmpFunction cmp);

public:
	XArray 			(int initsize = 0)
				{ init(initsize); }

	XArray			(BObject &aBObject, int initsize = 1)
				{ init(initsize); addElem(aBObject); }

	XArray			(XArray &aXArray)
				{ init(aXArray.size()); append(aXArray); }

	~XArray			()
				{ reset(); }


	XArray &operator=	(XArray &aXArray);

	virtual void reset	();

	virtual void addElem	( BObject &aBObject );

	virtual BObject &getObjectAtIndex	(int i);

	void insertAtIndex		(BObject &aBObject, int i);

	void delAtIndex		(int i);

	void removeLast		();

	void sort	(CmpFunction cmp = BObject::compareKey);
};

#endif

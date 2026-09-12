// 
// BLIB                                                                   
// Implementation XContainer
// Author: BB
// History:
// 1993-08-10	Created
// 1995-01-15	1.0
// 2005-12-19   Update to ANSI-C++
//

#include "b_cont.h"

using namespace std;

////////////////////////////////////////////////////////////////////
// Implementierung Container
////////////////////////////////////////////////////////////////////


void XContainer::init()
{
	elementsInContainer = 0;
	separator = ", ";
	ownsElementsFlag = false;
}

XContainer &XContainer::append(  XContainer &aContainer )
{
	// not allowed that both containers owns their elements!!
	PRECONDITION(
		(ownsElementsFlag == true && aContainer.ownsElementsFlag == true)
		? false : true);

	for (int index = 0; index < aContainer.size(); ++index)
	    addElem(aContainer[index]);
	return *this;
}

void XContainer::print( ostream &aOstream)
{
	for (int i = 0; i < size(); ++i)
	{
		if (i > 0)
			aOstream << separator;
		aOstream << (*this)[i];
#if 0
		aOstream << "[" << (*this)[i].nameOf() << "]";
#endif
	}
}

void XContainer::getObjectsKindOf(TypeBObjectId aBObjectId, XContainer &retContainer)
{
	for (int i = 0; i < size(); ++i)
		if ((*this)[i].isKindOf(aBObjectId))
			retContainer.addElem((*this)[i]);
#if 0
	cout << "Vom Typ " << (*aBObjectId)()  << " gefunden : {" << retContainer << "}\n";
#endif
}

bool XContainer::addElemTest(BObject &aBObject, int delflag)
{
	if (!isIn (aBObject.getKey ()))
	{
		addElem(aBObject);
		return false;		// Element noch nicht vorhanden
	}
	if (delflag)
		delete &aBObject;
	return true;			// Element schon vorhanden
}

BObject *XContainer::getElem (const char *name)
{
	int index = getFirstIndexOfObject(name);
	if (index >= 0)
		return &(*this)[index];
	return NULL;

}

BObject &XContainer::getSafeElem (const char *name)
{
	BObject *pBObject;
	if ((pBObject = getElem(name)) == NULL)
	{
		cerr << "internal error: XContainer::getSafeElem() failed!\n";
		babort(1);
	}
	return *pBObject;
}


int XContainer::getFirstIndexOfObject( BObject &aBObject)
{
	for ( int index = 0; index < size(); ++index )
		if ( (*this)[ index ] == aBObject )
			return index;
	return -1;
}

int XContainer::getFirstIndexOfObject( const char *name)
{
	PRECONDITION(name != NULL);
	for ( int index = 0; index < size(); ++index )
		if ( strcmp((*this)[ index ], name)  == 0)
			return index;
	return -1;
}

XContainer &XContainer::operator= (XContainer &aXContainer)
{
	reset();
	*this += aXContainer;
	return *this;
}



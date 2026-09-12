// 
// BLIB                                                                   
// Interface XHashTable 
// Author: RB+BB
// History:
// 1993-02		Created
// 1995-01-15	1.0
// 2005-12-20   Update to ANSI-C++
//


#if !defined __HASHTABLE_H
#define __HASHTABLE_H


#include "b_cont.h"

//
// XHashTable
//
// XHashtable nimmt Instanzen von BObject (und abgeleitete Subklassen)
// Alle diese Instanzen stellen die Funktion "getKey" zur
// Verfuegung, anhand derer sie eindeutig identifizierbar sind. Deshalb
// greifen die Funktionen von XHashTable auch nicht auf eine Vergleichs-
// operation der Elemente zurueck, sondern vergleichen direkt die Namen der
// Elemente via "getKey".
//

class XHashTable: public XContainer
{

	RTTIBObject (XHashTable, XContainer)

protected:
	int	hashTableSize;
	BObject	**hashTable;
	int		cursor;
	int		indexCountOfCursor;
	int		countDelElements;

	void init		(int initsize);
	bool moveCursorTo	(const char *name);
	void delAtCursor	();
	void addElem		(BObject &aBObject);
	bool isFull		() const;
	void rehash		(unsigned int newSize);

public:

// Wird eine explizite Groesse der Hashtable beim Konstruktor
// angegeben, so wird diese auf die naechste Zweierpotenz erhoeht, um die
// Performance der Operationen zu erhoehen und eine Implementierung gemaess
// [Knuth], Band 3, Kap.6.4, Aufgabe 20 zu ermoeglichen.

	XHashTable	(unsigned int initsize = INIT_SIZE)
			{ init (initsize); }

	XHashTable	(BObject &aBObject, unsigned int initsize = INIT_SIZE1)
			{ init (initsize); addElem(aBObject); }

	XHashTable	(XHashTable &aXHashTable)
			{ init (aXHashTable.size()); append(aXHashTable); }

	~XHashTable	()
			{ reset(); }

	virtual void reset	();

	XHashTable &operator=	( XHashTable &aXHashTable );

	XHashTable &operator -=	( XContainer &aXContainer );
	XHashTable &operator *=	( XContainer &aXContainer );

	virtual bool isIn	( const char *name )
				{ return moveCursorTo(name); }

	virtual BObject *getElem	( const char *name );

	virtual BObject &getObjectAtIndex	(int index);

	bool del		( const char *name );

	bool del		(BObject &aBObject)
					{ return del(aBObject.getKey()); }

   // "Iteration": Der Aufrufer stellt einen "int" zur Verfuegung,
   // der die einzelnen Elemente quasi aufzaehlt. Die Anzahl der Elemente
   // in der HashTable kann mit "size()" abgefragt werden. Der Aufrufer
   // ist dafuer verantwortlich, dass die Menge waehrend der Iteration nicht
   // geaendert wird.

	virtual void print	(std::ostream &);

	virtual bool isEqual	(BObject &);

};

#endif


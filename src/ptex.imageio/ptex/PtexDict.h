/* 
PTEX SOFTWARE
Copyright 2009 Disney Enterprises, Inc.  All rights reserved

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.

  * The names "Disney", "Walt Disney Pictures", "Walt Disney Animation
    Studios" or the names of its contributors may NOT be used to
    endorse or promote products derived from this software without
    specific prior written permission from Walt Disney Pictures.

Disclaimer: THIS SOFTWARE IS PROVIDED BY WALT DISNEY PICTURES AND
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE, NONINFRINGEMENT AND TITLE ARE DISCLAIMED.
IN NO EVENT SHALL WALT DISNEY PICTURES, THE COPYRIGHT HOLDER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND BASED ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
*/
/**
  @file PtexDict.h
  @brief Contains PtexDict, a string-keyed hash table.
*/

#ifndef PtexDict_h
#define PtexDict_h

/**
   @class PtexDict
   @brief A string-keyed dictionary template, using a hash table.

   <P>
   An efficient string dictionary template.  A hash table is used
   that automatically doubles in size when it is more than 50%
   full.  It is hard-coded to use string keys for efficiency.

   <P>
   The interface is compatible with std::hash_map<>, though not all methods
   are provided.  Methods provided:

   <UL>
   <LI>iterator begin();
   <LI>const_iterator begin() const;
   <LI>iterator end();
   <LI>const_iterator end() const;
   <LI>T& operator[] (const char*);
   <LI>iterator find(const char* key);
   <LI>const_iterator find(const char* key) const;
   <LI>bool erase(const char* key);
   <LI>iterator erase(iterator);
   <LI>void clear();
   <LI>int size() const;
   </UL>

   <P>
   Unlike std::hash_map<>, PtexDict doesn't have to construct a string
   object to do a lookup.  As a result, it is about 4-10 times faster
   depending on the length of the keys.  And when compiling
   non-optimized, it is 6-10 times faster.

   <P>
   We have decided NOT to follow the coding standard's naming comvention for
   this class, since it needed to remain generic, and compatible with the
   STL std::hash_map<> class. 
   
   @author brentb
   @author longson
   
   @version <B>1.0 brentb  11/01/2000:</B> Initial version
   @version <B>1.1 longson 06/26/2001:</B> Added file and class comment headers
   @version <B>2.0 longson 01/16/2002:</B> Updated to support most of the coding standards, except for the naming conventions.  Added const_iterator to provide const-safe access.  Fixed problem with iterators and not allowing modification.
   
*/

/**
   @struct PtexDict::value_type
   @brief Internal class used to provide a return value for the value type

   All data members and member functions are declared public by design
   Default Copy and Assignment operators are sufficient..

   <P>
   We have decided NOT to follow the coding standard's naming comvention for
   this class, since it needed to remain compatible with the STL std::pair<>
   class. 

   @author brentb
   @author longson

   @version <B>1.0 brentb  11/01/2000:</B> Initial version
   @version <B>1.1 longson 06/26/2001:</B> Added file and class comment headers
      
*/

template<class T>
class PtexDict
{
    class Entry; ///< forward declared private class
    
public: // Public Types   
    class iterator;        ///< forward declared class
    class const_iterator;  ///< forward declared class
    friend class iterator;
    friend class const_iterator;
    
    typedef const char*    key_type;       ///< This is the lookup type
    typedef T              mapped_type;    ///< The data type stored

    struct value_type {
    public:
	/// Default constructor references default_value, with a 0 for first
	value_type(): first(0), second() {}
	/// Creation constructor
	value_type(key_type f, const T& s): first(f), second(s) {}

	const key_type first;  ///< Reference to the key type
	T second;              ///< Reference to the data 
    };

public:  // Public Member Interfce
    
    /// Default contructor initializes the dictionary.
    PtexDict() :  _numEntries(0), _numBuckets(0), _bucketMask(0), _buckets(0) {}
    /// clears the entries in the dictionary
    virtual ~PtexDict() { clear(); }

    /// Locates an entry, creating a new one if necessary.
    /** operator[] will look up an entry and return the value.  A new entry
	will be created (using the default ctor for T) if one doesn't exist.
    */
    T& operator[](const char* key);

    /// Returns an iterator referencing the beginning of the table
    iterator begin()
    {
	iterator iter;
	iter._d = this;
	for (iter._b = 0; iter._b < _numBuckets; iter._b++) {
	    iter._e = &_buckets[iter._b];
	    if (*iter._e) return iter;
	}
	iter._e = 0;
	return iter;
    }

    /// Returns an iterator referencing the end of the table.
    inline iterator end() { return iterator( 0, this, 0 ); }

    /// Const access to the beginning of the list
    const_iterator begin() const
    {
	const_iterator iter;
	iter._d = this;
	for (iter._b = 0; iter._b < _numBuckets; iter._b++) {
	    iter._e = &_buckets[iter._b];
	    if (*iter._e) return iter;
	}
	iter._e = 0;
	return iter;
    }

    /// Const access to the end of the list
    inline const_iterator end() const { return const_iterator( 0, this, 0 ); }
    
    /// Locates an entry, without creating a new one.
    /** find will locate an entry, but won't create a new one.  The result is
	returned as a pair of key and value.  The returned key points to the
	internal key string and will remain valid until the entry is deleted. 
	If the key is not found, the returned iterator will be equal to the
	value returned by end(), and the iterator will be equal to false. O(1)
    */
    iterator find(const char* key);
    /// works the same as find above, but returns a constant iterator.
    const_iterator find(const char* key) const;
    
    /// Will remove an entry.  It will return TRUE if an entry was found.
    bool erase(const char* key);
    /// Removes the entry referenced by the iterator, from the dictionary.
    /** It will return a iterator to the next element, or will equal the
	return value of end() if there is nothing else to erase.  O(1) */
    iterator erase(iterator iter);

    /// clear will remove all entries from the dictionary. O(n)
    void clear();

    /// Returns the number of entries in the dictionary. O(1) time to call.
    int size() const { return _numEntries; }

private: // Private Member Interface
       
    /// @brief This internal structure is used to store the dictionary elements
    class Entry {
    public: // Public Member Interface
	/// Default constructor initiaizes val with the defaul value
	Entry() : _next(0), _hashval(0), _keylen(0),
		  _val(_u._key,T())
        { _u._pad = 0; }	
        ~Entry() {}
    private: // Private Member Interface	
	/// Copy constructor prohibited by design.
	Entry(const Entry&);
	/// Assignment operator prohibited by design.
	Entry& operator=(const Entry&);
	
    public:
	Entry*     _next;    ///< Pointer to the next element in the structure
	int        _hashval; ///< cached hashval of key
	int        _keylen;  ///< cached length of key
	value_type _val;     ///< The stored value of the hash table
	union {
	    int  _pad;	 ///< for integer align of _key, for fast compares
	    char _key[1];///< 1 is dummy length - actual size will be allocated
	} _u;
    };

    /// Copy constructor prohibited by design.
    PtexDict(const PtexDict&);
    /// Assignment operator prohibited by design.
    PtexDict& operator=(const PtexDict&);

    /// Returns the integer hash index for the key and length of the key.
    int hash(const char* key, int& keylen) const
    {
	// this is similar to perl's hash function
	int hashval = 0;
	const char* cp = key;
	char c;
	while ((c = *cp++)) hashval = hashval * 33 + c;
	keylen = int(cp-key)-1;
	return hashval;
    }

    /// Returns a pointer to the desired entry, based on the key. 
    Entry** locate(const char* key, int& keylen, int& hashval) const
    {
	hashval = hash(key, keylen);
	if (!_buckets) return 0;
	for (Entry** e = &_buckets[hashval & _bucketMask]; *e; e=&(*e)->_next)
	    if ((*e)->_hashval == hashval && (*e)->_keylen == keylen &&
		streq(key, (*e)->_u._key, keylen)) 
		return e;
	return 0;
    }

    /// Used for string compares, much faster then strcmp
    /** This is MUCH faster than strcmp and even memcmp, partly because
	it is inline and partly because it can do 4 chars at a time
    */
    static inline bool streq(const char* s1, const char* s2, int len)
    {
	// first make sure s1 is quad-aligned (s2 is always aligned)
	if (((intptr_t)s1 & 3) == 0) {  
	    int len4 = len >> 2;
	    while (len4--) {
		if (*(int*)s1 != *(int*)s2) return 0;
		s1 += 4; s2 += 4;
	    }
	    len &= 3;
	}
	while (len--) if (*s1++ != *s2++) return 0;
	return 1;
    }

    /// Used to increase the size of the table if necessary
    void grow();

private:  // Private Member data

    int _numEntries;  ///< The number of entries in the dictionary
    int _numBuckets;  ///< The number of buckets in use
    int _bucketMask;  ///< The mask for the buckets
    Entry** _buckets; ///< The pointer to the bucket structure
};


/**
   @class PtexDict::iterator
   @brief Internal class used to provide iteration through the dictionary

   This works on non-const types, and provides type safe modification access
  
   @author  brentb
   @author  longson
   
   @version <B>1.0 brentb  11/01/2000:</B> Initial version
   @version <B>1.1 longson 06/26/2001:</B> Added file and class comment headers
   @version <B>1.2 longson 01/16/2002:</B> Made const-safe with const_iterator
      
*/
template<class T>
class PtexDict<T>::iterator {
public:
    /// Default Constructor
    iterator() : _d(0), _e(0), _b(0) {}
    
    /// Proper copy constructor implementation
    iterator(const iterator& iter) :
	_d(iter._d), _e(iter._e), _b(iter._b) {}
    /// Proper assignment operator
    inline iterator& operator=(const iterator& iter)
	{ _e = iter._e; _d = iter._d; _b = iter._b; return *this; }   
    
    /// Operator for obtaining the value that the iterator references
    inline value_type& operator*() const { return getValue(); }
    /// Pointer reference operator
    inline value_type* operator->() const { return &getValue(); }

    /// For determining whether or not an iterator is valid
    inline operator bool() { return _e != 0; }
    
    /// For comparing equality of iterators
    inline bool operator==(const iterator& iter) const
	{ return iter._e == _e; }
    /// For comparing inequality of iterators
    inline bool operator!=(const iterator& iter) const
	{ return iter._e != _e; }
    /// For comparing equality of iterators
    inline bool operator==(const const_iterator& iter) const
	{ return iter._e == _e; }
    /// For comparing inequality of iterators
    inline bool operator!=(const const_iterator& iter) const
	{ return iter._e != _e; }
    /// For advancing the iterator to the next element
    iterator& operator++(int);
    
private:  // Private interface

    /// Constructor Helper for inline creation.
    iterator( Entry** e, const PtexDict* d, int b): _d(d), _e(e), _b(b) {}
    
    /// simple helper function for retrieving the value from the Entry
    inline value_type& getValue() const{
	if (_e) return (*_e)->_val;
	else   return _defaultVal;
    }
    
    friend class PtexDict;
    friend class const_iterator;
    const PtexDict* _d;  ///< dictionary back reference
    Entry** _e;      ///< pointer to entry in table this iterator refs
    int _b;          ///< bucket number this references

    static value_type _defaultVal; ///< Default value
};

// define the static type for the iterator
template<class T> typename PtexDict<T>::value_type PtexDict<T>::iterator::_defaultVal;

/**
   @class PtexDict::const_iterator
   @brief Internal class used to provide iteration through the dictionary

   This works on const data types, and provides const safe access.
   This class can also be created from a PtexDict::iterator class instance.
   
   @author  longson
   
   @version <B>1.2 longson 01/16/2002:</B> Initial version based on iterator
      
*/
template<class T>
class PtexDict<T>::const_iterator {
public:
    /// Default Constructor
    const_iterator() : _d(0), _e(0), _b(0) {}

    /// Proper copy constructor implementation for const_iterator
    const_iterator(const const_iterator& iter) :
	_d(iter._d), _e(iter._e), _b(iter._b) {}
    /// Conversion constructor for iterator
    const_iterator(const iterator& iter) :
	_d(iter._d), _e(iter._e), _b(iter._b) {}
    /// Proper assignment operator for const_iterator
    inline const_iterator& operator=(const const_iterator& iter)
	{ _e = iter._e; _d = iter._d; _b = iter._b; return *this; }
    /// Proper assignment operator for iterator
    inline const_iterator& operator=(iterator& iter)
	{ _e = iter._e; _d = iter._d; _b = iter._b; return *this; }
    
    /// Operator for obtaining the value that the const_iterator references
    inline const value_type& operator*() const { return getValue(); }
    /// Pointer reference operator
    inline const value_type* operator->() const { return &getValue(); }
    
    /// For determining whether or not an iterator is valid
    inline operator bool() { return _e != 0; }
    
    /// For comparing equality of iterators
    inline bool operator==(const iterator& iter) const
	{ return iter._e == _e; }
    /// For comparing inequality of iterators
    inline bool operator!=(const iterator& iter) const
	{ return iter._e != _e; }
    /// For comparing equality of const_iterators
    inline bool operator==(const const_iterator& iter) const
	{ return iter._e == _e; }
    /// For comparing inequality of iterators
    inline bool operator!=(const const_iterator& iter) const
	{ return iter._e != _e; }
    /// For advancing the iterator to the next element
    const_iterator& operator++(int);
    
private:  // Private interface
    
    /// Constructor Helper for inline creation.
    const_iterator( Entry** e, const PtexDict* d, int b): _d(d),_e(e),_b(b) {}

    /// simple helper function for retrieving the value from the Entry
    inline const value_type& getValue() const{
	if (_e) return (*_e)->_val;
	else   return _defaultVal;
    }
    
    friend class PtexDict;
    friend class iterator;
    const PtexDict* _d;  ///< dictionary back reference
    Entry** _e;      ///< pointer to entry in table this iterator refs
    int _b;          ///< bucket number this references

    static value_type _defaultVal; ///< Default value
};

// define the static type for the iterator
template<class T> typename PtexDict<T>::value_type PtexDict<T>::const_iterator::_defaultVal;

template<class T>
typename PtexDict<T>::iterator& PtexDict<T>::iterator::operator++(int)
{
    if (_e) {
	// move to next entry
	_e = &(*_e)->_next;
	if (!*_e) {
	    // move to next non-empty bucket
	    for (_b++; _b < _d->_numBuckets; _b++) {
		_e = &_d->_buckets[_b];
		if (*_e) return *this;
	    }
	    _e = 0;
	}
    }
    return *this;
}

template<class T>
typename PtexDict<T>::const_iterator& PtexDict<T>::const_iterator::operator++(int)
{
    if (_e) {
	// move to next entry
	_e = &(*_e)->_next;
	if (!*_e) {
	    // move to next non-empty bucket
	    for (_b++; _b < _d->_numBuckets; _b++) {
		_e = &_d->_buckets[_b];
		if (*_e) return *this;
	    }
	    _e = 0;
	}
    }
    return *this;
}

template<class T>
typename PtexDict<T>::iterator PtexDict<T>::find(const char* key)
{
    int keylen, hashval;
    Entry** e = locate(key, keylen, hashval);

    /// return a valid iterator if we found an entry, else return end()
    if (e) return iterator( e, this, hashval & _bucketMask );
    else   return end();
}

template<class T>
typename PtexDict<T>::const_iterator PtexDict<T>::find(const char* key) const
{
    int keylen, hashval;
    Entry** e = locate(key, keylen, hashval);

    /// return a valid iterator if we found an entry, else return end()
    if (e) return const_iterator( e, this, hashval & _bucketMask );
    else   return end();
}

template<class T>
T& PtexDict<T>::operator[](const char* key)
{
    int keylen, hashval;
    Entry** e = locate(key, keylen, hashval);
    if (e) return (*e)->_val.second;

    // create a new entry
    _numEntries++;
    if (_numEntries*2 >= _numBuckets) grow();

    // allocate a buffer big enough to hold Entry + (the key length )
    // Note: the NULL character is already accounted for by Entry::_key's size
    void* ebuf = malloc( sizeof(Entry) + (keylen) * sizeof(char) );
    Entry* ne = new(ebuf) Entry; // note: placement new 

    // Store the values in the Entry structure
    Entry** slot = &_buckets[hashval & _bucketMask];
    ne->_next = *slot; *slot = ne;
    ne->_hashval = hashval;
    ne->_keylen = keylen;
    
    // copy the string given into the new location
    memcpy(ne->_u._key, key, keylen);
    ne->_u._key[keylen] = '\0';
    return ne->_val.second;
}


template<class T>
void PtexDict<T>::grow()
{
    if (!_buckets) {
	_numBuckets = 16;
	_bucketMask = _numBuckets - 1;
	_buckets = (Entry**) calloc(_numBuckets, sizeof(Entry*));
    } else {
	int newsize = _numBuckets * 2;
	_bucketMask = newsize - 1;
	Entry** newbuckets = (Entry**) calloc(newsize, sizeof(Entry*));
	for (int i = 0; i < _numBuckets; i++) {
	    for (Entry* e = _buckets[i]; e;) {
		Entry* _next = e->_next;
		Entry** slot = &newbuckets[e->_hashval & _bucketMask];
		e->_next = *slot; *slot = e;
		e = _next;
	    }
	}
	free(_buckets);
	_buckets = newbuckets;
	_numBuckets = newsize;
    }
}


template<class T>
bool PtexDict<T>::erase(const char* key)
{
    iterator iter = find(key);
    if (!iter) return false;

    erase(iter);
    return true;  // valid entry to remove
}


template<class T>
typename PtexDict<T>::iterator PtexDict<T>::erase(iterator iter)
{
    Entry** eptr = iter._e;
    if (!eptr) return iter;

    // patch around deleted entry
    Entry* e = *eptr;
    Entry* next = e->_next;
    if (!next) iter++;  // advance iterator if at end of chain
    *eptr = next;

    // destroy entry.  This is a strange destroy but is necessary because of
    // the way Entry() is allocated by using malloc above.
    e->~Entry(); // note: explicit dtor call
    free(e);     // free memory allocated.
    _numEntries--;

    return iter;
}


template<class T>
void PtexDict<T>::clear()
{
    for (iterator i=begin(); i != end(); )
        i = erase(i);
    free(_buckets);
    _buckets = 0;
    _numEntries = 0;
    _numBuckets = 0;
}

#endif //PtexDict_h

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
  @file PtexHashMap.h
  @brief Contains PtexHashMap, a lightweight hash table.
*/

#ifndef PtexHashMap_h
#define PtexHashMap_h

/**
   @class PtexHashMap
   @brief A lightweight hash table.
   
   <P>
   The hash table automatically doubles in size when it is more than 50% full.

   <P>
   The interface is mostely compatible with std::hash_map<>, though not all methods
   are provided.  Methods provided:

   <UL>
   <LI>iterator begin();
   <LI>iterator end();
   <LI>DataType& operator[] (KeyType key);
   <LI>iterator find(KeyType key) const;
   <LI>bool erase(KeyType key);
   <LI>iterator erase(iterator);
   <LI>void clear();
   <LI>int size() const;
   </UL>

   @author  brentb
   
   @version <B>1.0 brentb  11/01/2000:</B> Initial version
   @version <B>1.1 longson 06/26/2001:</B> Added file and class comment headers
   @version <B>1.2 brentb 105/2006:</B> Generalized IntDict into general hash map
   
*/

/**
   @class PtexHashMap::iterator
   @brief Internal class used to provide iteration through the hash table
  
   @author  brentb
   
   @version <B>1.0 brentb  11/01/2000:</B> Initial version
   @version <B>1.1 longson 06/26/2001:</B> Added file and class comment headers
      
*/

template<typename KeyType, typename DataType, typename HashFn>
class PtexHashMap
{
    struct Entry; // forward decl

public:
    typedef KeyType key_type;
    typedef DataType data_type;
    typedef HashFn hash_fn;

    struct value_type {
	key_type first;
	data_type second;
	value_type() : first(), second() {}
	value_type(const KeyType& first, const DataType& second)
	    : first(first), second(second) {}
    };

    class iterator {
    public:
	/// Default Constructor
	iterator() : e(0), h(0), b(0) {}
	
	/// Proper copy constructor implementation
	iterator(const iterator& iter) : e(iter.e), h(iter.h), b(iter.b) {}

	/// Operator for obtaining the value that the iterator references
	value_type operator*() const { 
	    if (e) return value_type((*e)->key, (*e)->val);
	    return value_type();
	}

	/// Pointer reference operator
	value_type* operator->() const {
	    static value_type v;
	    v = operator*();
	    return &v;
	}

	/// Proper assignment operator
	iterator& operator=(const iterator& iter) {
	    e = iter.e; h = iter.h; b = iter.b; return *this;
	}

	/// For comparing equality of iterators
	bool operator==(const iterator& iter) const { return iter.e == e; }
	/// For comparing inequality of iterators
	bool operator!=(const iterator& iter) const { return iter.e != e; }
	/// For advancing the iterator to the next element
	iterator& operator++(int);

    private:
	friend class PtexHashMap;
	Entry** e; // pointer to prev entry or hash entry
	const PtexHashMap* h;
	int b; // bucket number
    };
    
    /// Returns an iterator referencing the beginning of the table
    iterator begin() const
    {
	iterator iter;
	iter.h = this;
	for (iter.b = 0; iter.b < _numBuckets; iter.b++) {
	    iter.e = &_buckets[iter.b];
	    if (*iter.e) return iter;
	}
	iter.e = 0;
	return iter;
    }

    /// Returns an iteraot referencing the location just beyond the table.
    iterator end() const
    {
	iterator iter;
	iter.h = this; iter.b = 0; iter.e = 0; 
	return iter;
    }


    /// Default contructor initializes the hash table.
     PtexHashMap() : _numEntries(0), _numBuckets(0), _bucketMask(0), _buckets(0) {}
    /// Non-Virtual destructor by design, clears the entries in the hash table
    ~PtexHashMap() { clear(); }

    /// Locates an entry, creating a new one if necessary.
    /** operator[] will look up an entry and return the value.  A new entry
       will be created (using the default ctor for DataType) if one doesn't exist.
    */
    DataType& operator[](const KeyType& key);

    /// Locates an entry, without creating a new one.
    /** find will locate an entry, but won't create a new one.  The result is
       returned as a pair of key and value.  The returned key points to the
       internal key string and will remain valid until the entry is deleted. 
       If the key is not found, both the returned key and value pointers will
       be NULL.
    */
    iterator find(const KeyType& key) const;

    /// Will remove an entry.  It will return TRUE if an entry was found.
    bool erase(const KeyType& key);
    /// Will remove an entry.  It will return a iterator to the next element
    iterator erase(iterator iter);

    /// clear will remove all entries and free the table
    void clear();

    /// number of entries in the hash
    int size() const { return _numEntries; }

private:
    /// Copy constructor prohibited by design.
    PtexHashMap(const PtexHashMap&); // disallow
    /// Assignment operator prohibited by design.
    bool operator=(const PtexHashMap&); // disallow
    
    friend class iterator;
    
    struct Entry {
	Entry() : val() {}
	Entry* next;
	KeyType key;
	DataType val;
    };

    /// Returns a pointer to the desired entry, based on the key. 
    Entry** locate(const KeyType& key) const
    {
	if (!_buckets) return 0;
 	for (Entry** e = &_buckets[hash(key) & _bucketMask]; *e; e = &(*e)->next)
 	    if ((*e)->key == key)
 		return e;
	return 0;
    }

    /// Used to increase the size of the table if necessary
    void grow();

    int _numEntries;
    int _numBuckets;
    int _bucketMask;
    Entry** _buckets;
    HashFn hash;
};


template<class KeyType, class DataType, class HashFn>
typename PtexHashMap<KeyType, DataType, HashFn>::iterator&
PtexHashMap<KeyType, DataType, HashFn>::iterator::operator++(int)
{
    if (e) {
	// move to next entry
	e = &(*e)->next;
	if (!*e) {
	    // move to next non-empty bucket
	    for (b++; b < h->_numBuckets; b++) {
		e = &h->_buckets[b];
		if (*e) return *this;
	    }
	    e = 0;
	}
    }
    return *this;
}


template<class KeyType, class DataType, class HashFn>
typename PtexHashMap<KeyType, DataType, HashFn>::iterator
PtexHashMap<KeyType, DataType, HashFn>::find(const KeyType& key) const
{
    iterator iter;
    Entry** e = locate(key);
    if (e) {
	iter.h = this;
	iter.e = e;
	iter.b = hash(key) & _bucketMask;
    } else iter = end();
    return iter;
}

template<class KeyType, class DataType, class HashFn>
DataType& PtexHashMap<KeyType, DataType, HashFn>::operator[](const KeyType& key)
{
    Entry** e = locate(key);
    if (e) return (*e)->val;

    // create a new entry
    _numEntries++;
    if (_numEntries*2 >= _numBuckets) grow();
    void* ebuf = malloc(sizeof(Entry));
    Entry* ne = new(ebuf) Entry; // note: placement new
    Entry** slot = &_buckets[hash(key) & _bucketMask];
    ne->next = *slot; *slot = ne;
    ne->key = key;
    return ne->val;
}


template<class KeyType, class DataType, class HashFn>
void PtexHashMap<KeyType, DataType, HashFn>::grow()
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
		Entry* next = e->next;
		Entry** slot = &newbuckets[hash(e->key) & _bucketMask];
		e->next = *slot; *slot = e;
		e = next;
	    }
	}
	free(_buckets);
	_buckets = newbuckets;
	_numBuckets = newsize;
    }
}


template<class KeyType, class DataType, class HashFn>
bool PtexHashMap<KeyType, DataType, HashFn>::erase(const KeyType& key)
{
    iterator iter = find(key);
    if (iter == end()) return 0;
    erase(iter);
    return 1;
}


template<class KeyType, class DataType, class HashFn>
typename PtexHashMap<KeyType, DataType, HashFn>::iterator
PtexHashMap<KeyType, DataType, HashFn>::erase(iterator iter)
{
    Entry** eptr = iter.e;
    if (!eptr) return iter;

    // patch around deleted entry
    Entry* e = *eptr;
    Entry* next = e->next;
    if (!next) iter++;  // advance iterator if at end of chain
    *eptr = next;


    // destroy entry
    e->~Entry(); // note: explicit dtor call
    free(e);
    _numEntries--;

    return iter;
}


template<class KeyType, class DataType, class HashFn>
void PtexHashMap<KeyType, DataType, HashFn>::clear()
{
    for (iterator i = begin(); i != end(); )
        i = erase(i);
    free(_buckets);
    _buckets = 0;
    _numEntries = 0;
    _numBuckets = 0;
}

#endif

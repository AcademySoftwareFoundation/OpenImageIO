/*
  Copyright 2008 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the software's owners nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  (This is the Modified BSD License)
*/

#include <cstdio>
#include <string>
#include <vector>
#include <map>

#include "OpenImageIO/export.h"
#include "OpenImageIO/thread.h"
#include "OpenImageIO/strutil.h"
#include "OpenImageIO/dassert.h"
#include "OpenImageIO/ustring.h"
#include "OpenImageIO/unordered_map_concurrent.h"

#include <boost/unordered_map.hpp>

OIIO_NAMESPACE_ENTER
{

#if 0
// Use regular mutex
typedef mutex ustring_mutex_t;
typedef lock_guard ustring_read_lock_t;
typedef lock_guard ustring_write_lock_t;
#elif 0
// Use spin locks
typedef spin_mutex ustring_mutex_t;
typedef spin_lock ustring_read_lock_t;
typedef spin_lock ustring_write_lock_t;
#elif 1
// Use rw spin locks
typedef spin_rw_mutex ustring_mutex_t;
typedef spin_rw_read_lock ustring_read_lock_t;
typedef spin_rw_write_lock ustring_write_lock_t;
#else
// Use null locks
typedef null_mutex ustring_mutex_t;
typedef null_lock<null_mutex> ustring_read_lock_t;
typedef null_lock<null_mutex> ustring_write_lock_t;
#endif


#if defined(__i386__) && !defined(__clang__) && !defined(_MSC_VER)
#if ((10000*__GNUC__ + 100*__GNUC_MINOR__ + __GNUC_PATCHLEVEL__) < 40300)
// On a 32bit build using gcc4.2, make_unique() seg faults with the
// concurrent map enabled, so turn it off. More recent gcc seems ok. That
// old a gcc on 32 bit systems is a pretty rare combination, so we're not
// going to sweat the lower performance of turning off the concurrent map
// for that increasingly rare case.
#define USE_CONCURRENT_MAP 0
#endif
#endif

#ifndef USE_CONCURRENT_MAP
#define USE_CONCURRENT_MAP 1
#endif

#if USE_CONCURRENT_MAP
typedef unordered_map_concurrent <string_view, ustring::TableRep *, Strutil::StringHash, Strutil::StringEqual, 8> UstringTable;
#else
typedef boost::unordered_map <string_view, ustring::TableRep *, Strutil::StringHash, Strutil::StringEqual> UstringTable;
#endif

std::string ustring::empty_std_string ("");


namespace { // anonymous

#if USE_CONCURRENT_MAP
static OIIO_CACHE_ALIGN atomic_ll ustring_stats_memory;
static OIIO_CACHE_ALIGN atomic_ll ustring_stats_constructed;
static OIIO_CACHE_ALIGN atomic_ll ustring_stats_unique;
#else
static OIIO_CACHE_ALIGN long long ustring_stats_memory = 0;
static OIIO_CACHE_ALIGN long long ustring_stats_constructed = 0;
static OIIO_CACHE_ALIGN long long ustring_stats_unique = 0;
#endif


#if !USE_CONCURRENT_MAP
// Wrap our static mutex in a function to guarantee it exists when we
// need it, regardless of module initialization order.
static ustring_mutex_t & ustring_mutex ()
{
    static OIIO_CACHE_ALIGN ustring_mutex_t the_real_mutex;
    return the_real_mutex;
}
#endif


static UstringTable & ustring_table ()
{
    static OIIO_CACHE_ALIGN UstringTable table;
    return table;
}

}           // end anonymous namespace


// Put a ustring in the global scope to force at least one call to
// make_unique to happen before main(), i.e. before threads are launched,
// in order to eliminate any possible thread collosion on construction of
// the ustring_table statically declared within make_unique.
namespace pvt {
static ustring ustring_force_make_unique_call("");
}



namespace {
// Definitions to let us access libc++ string internals.
// See libc++ <string> file for details.

#ifdef _LIBCPP_ALTERNATE_STRING_LAYOUT
struct libcpp_string__long {
    std::string::pointer   __data_;
    std::string::size_type __size_;
    std::string::size_type __cap_;
};
#if _LIBCPP_BIG_ENDIAN
    enum {libcpp_string__long_mask  = 0x1ul};
#else  // _LIBCPP_BIG_ENDIAN
    enum {libcpp_string__long_mask  = ~(std::string::size_type(~0) >> 1)};
#endif  // _LIBCPP_BIG_ENDIAN
#else
struct libcpp_string__long {
    std::string::size_type __cap_;
    std::string::size_type __size_;
    std::string::pointer   __data_;
};
#if _LIBCPP_BIG_ENDIAN
    enum {libcpp_string__long_mask  = ~(std::string::size_type(~0) >> 1)};
#else  // _LIBCPP_BIG_ENDIAN
    enum {libcpp_string__long_mask  = 0x1ul};
#endif  // _LIBCPP_BIG_ENDIAN
#endif

enum {libcpp_string__min_cap = (sizeof(libcpp_string__long) - 1)/sizeof(std::string::value_type) > 2 ?
                               (sizeof(libcpp_string__long) - 1)/sizeof(std::string::value_type) : 2};

}



ustring::TableRep::TableRep (string_view strref)
    : hashed(Strutil::strhash(strref))
{
    length = strref.length();
    memcpy ((char *)c_str(), strref.data(), length);
    ((char *)c_str())[length] = 0;

    // We don't want the internal 'std::string str' to redundantly store the
    // chars, along with our own allocation.  So we use our knowledge of the
    // internal structure of std::string (for certain compilers) to force
    // the std::string to make it point to our chars!  In such a case, the
    // destructor will be careful not to allow a deallocation.

#if defined(__GNUC__) && !defined(_LIBCPP_VERSION) && defined(_GLIBCXX_USE_CXX11_ABI) && _GLIBCXX_USE_CXX11_ABI
    // NEW gcc ABI
    // FIXME -- do something smart with this.

#elif defined(__GNUC__) && !defined(_LIBCPP_VERSION)
    // OLD gcc ABI
    // It turns out that the first field of a gcc std::string is a pointer
    // to the characters within the basic_string::_Rep.  We merely redirect
    // that pointer, though for std::string to function properly, the chars
    // must be preceeded immediately in memory by the rest of
    // basic_string::_Rep, consisting of length, capacity and refcount
    // fields.  And we have designed our TableRep to do just that!  So now
    // we redirect the std::string's pointer to our own characters and its
    // mocked-up _Rep.
    //
    // See /usr/include/c++/VERSION/bits/basic_string.h for the details of
    // gcc's std::string implementation.
    dummy_capacity = length;
    dummy_refcount = 1;   // so it never frees
    *(const char **)&str = c_str();
    DASSERT (str.c_str() == c_str() && str.size() == length);
    return;

#elif defined(_LIBCPP_VERSION)
    // libc++ uses a different std::string representation than gcc.  For
    // long char sequences, it's two size_t's (capacity & length) followed
    // by the pointer to allocated characters. (Gory detail: see the
    // definitions above for how it varies slightly with endianness and
    // _LIBCPP_ALTERNATE_STRING_LAYOUT.)  For short enough sequences, it's a
    // single byte length followed immediately by the chars (the total being
    // the same size as the long string).  There's no savings of space or
    // allocations to be had for short strings, so we just let those behave
    // as normal.  But if it's going to make a long string (we can tell from
    // the length), we construct it ourselves, forcing the pointer to point
    // to the charcters in the TableRep we allocated.
    if (length >= libcpp_string__min_cap /* it'll be a "long string" */) {
        ((libcpp_string__long *)&str)->__cap_ = libcpp_string__long_mask | (length+1);
        ((libcpp_string__long *)&str)->__size_ = length;
        ((libcpp_string__long *)&str)->__data_ = (char *)c_str();
        DASSERT (str.c_str() == c_str() && str.size() == length);
        return;
    }
#endif

    // Remaining cases - just assign the internal string.  This may result
    // in double allocation for the chars.  If you care about that, do
    // something special for your platform, much like we did for gcc and
    // libc++ above. (Windows users, I'm talking to you.)
    str = strref;
}



ustring::TableRep::~TableRep ()
{
    if (str.c_str() == c_str()) {
        // This is one of those cases where we've carefully doctored the
        // string to point to our allocated characters.  To make a safe
        // string destroy, now force it to look like an empty string.
        std::string empty;
        memcpy (&str, &empty, sizeof(std::string));
    }
}



#if USE_CONCURRENT_MAP

const char *
ustring::make_unique (string_view strref)
{
    UstringTable &table (ustring_table());

    // Eliminate NULLs
    if (! strref.data())
        strref = string_view("", 0);

    // Check the ustring table to see if this string already exists.  If so,
    // use its canonical representation.
    ustring_stats_constructed += 1;
    {
        ustring::TableRep *tr;
        if (table.retrieve (strref, tr))
            return tr->c_str();
    }

    // This string is not yet in the ustring table.  Create a new entry.
    size_t len = strref.length();
    size_t size = sizeof(ustring::TableRep) + len + 1;
    ustring::TableRep *rep = (ustring::TableRep *) malloc (size);
    new (rep) ustring::TableRep (strref);

    // Lock the table and add the entry if it's not already there
    const char *result = rep->c_str();
    bool added = table.insert (string_view(result,len), rep);
    if (added) {
        if (result != rep->str.c_str())
            size += len+1;  // chars are replicated
        ustring_stats_unique += 1;
        ustring_stats_memory += size;
        return result;
    } 

    // Somebody else added this string to the table in that interval
    // when we were unlocked and constructing the rep.  Don't use the
    // new one!  Use the one in the table and disregard the one we
    // speculatively built.
    rep->~TableRep ();  // destructor
    free (rep);         // because it was malloced
    UstringTable::iterator found = table.find (strref);
    return found->second->c_str();
}


#else

const char *
ustring::make_unique (string_view strref)
{
    UstringTable &table (ustring_table());

    // Eliminate NULLs
    if (! strref.data())
        strref = string_view("", 0);

    // Check the ustring table to see if this string already exists.  If so,
    // construct from its canonical representation.
    {
        // Grab a read lock on the table.  Hopefully, the string will
        // already be present, and we can immediately return its rep.
        // Lots of threads may do this simultaneously, as long as they
        // are all in the table.
        const char *result = NULL;  // only non-NULL if it was found
        {
            ustring_read_lock_t read_lock (ustring_mutex());
            UstringTable::const_iterator found = table.find (strref);
            if (found != table.end())
                result = found->second->c_str();
        }
        // atomically increment the stat, since we're outside the lock
        atomic_exchange_and_add (&ustring_stats_constructed, 1);
        if (result)
            return result;
    }

    // This string is not yet in the ustring table.  Create a new entry.
    // Note that we are speculatively releasing the lock and building the
    // string locally.  Then we'll lock again to put it in the table.
    size_t len = strref.length();
    size_t size = sizeof(ustring::TableRep) + len + 1;
    ustring::TableRep *rep = (ustring::TableRep *) malloc (size);
    new (rep) ustring::TableRep (strref);

    const char *result = rep->c_str(); // start assuming new one
    {
        // Now grab a write lock on the table.  This will prevent other
        // threads from even reading.  Just in case another thread has
        // already entered this thread while we were unlocked and
        // constructing its rep, check the table one more time.  If it's
        // still empty, add it.
        ustring_write_lock_t write_lock (ustring_mutex());
        UstringTable::const_iterator found = table.find (strref);
        if (found == table.end()) {
            // add the one we just created to the table
            table[string_view(result,len)] = rep;
            ++ustring_stats_unique;
            ustring_stats_memory += size;
            if (rep->c_str() != rep->str.c_str())
                ustring_stats_memory += len+1;  // chars are replicated
            return result;
        } else {
            // use the one in the table, and we'll delete the new one we
            // created at the end of the function
            result = found->second->c_str();
        }
    }
    // Somebody else added this string to the table in that interval
    // when we were unlocked and constructing the rep.  Don't use the
    // new one!  Use the one in the table and disregard the one we
    // speculatively built.  Note that we've already released the lock
    // on the table at this point.
    rep->~TableRep ();  // destructor
    free (rep);         // because it was malloced
    return result;
}

#endif



std::string
ustring::getstats (bool verbose)
{
#if ! USE_CONCURRENT_MAP
    ustring_read_lock_t read_lock (ustring_mutex());
#endif
    std::ostringstream out;
    if (verbose) {
        out << "ustring statistics:\n";
        out << "  ustring requests: " << ustring_stats_constructed
            << ", unique " << ustring_stats_unique << "\n";
        out << "  ustring memory: " << Strutil::memformat(ustring_stats_memory)
            << "\n";
    } else {
        out << "requests: " << ustring_stats_constructed
            << ", unique " << ustring_stats_unique
            << ", " << Strutil::memformat(ustring_stats_memory);
    }
#ifndef NDEBUG
    // See if our hashing is pathological by checking if there are multiple
    // strings that ended up with the same hash.
    UstringTable &table (ustring_table());
    std::map<size_t,int> hashes;
    int collisions = 0;
    int collision_max = 0;
    size_t most_common_hash = 0;
    for (UstringTable::iterator s = table.begin(), e = table.end();
         s != e;  ++s) {
        // Pretend the string_view pointer in the table is a ustring (it is!)
        const char *chars = s->first.data();
        ustring us = *((ustring *)&chars);
        bool init = (hashes.find(us.hash()) == hashes.end());
        int &c (hashes[us.hash()]);  // Find/create the count for this hash
        if (init)
            c = 0;
        if (++c > 1) {               // Increment it, and if it's shared...
            ++collisions;            //     register a collision
            if (c > collision_max) { //     figure out the largest number
                collision_max = c;   //         of shared collisions
                most_common_hash = us.hash();
            }
        }
    }
    out << (verbose ? "  " : ", ") << collisions << " hash collisions (max " 
        << collision_max << (verbose ? ")\n" : ")");

    // DEBUG renders only -- reveal the strings sharing the most common hash
    if (collision_max > 2) {
        out << (verbose ? "" : "\n") << "  Most common hash " 
            << most_common_hash << " was shared by:\n";
        for (UstringTable::iterator s = table.begin(), e = table.end();
             s != e;  ++s) {
            const char *chars = s->first.data();
            ustring us = *((ustring *)&chars);
            if (us.hash() == most_common_hash)
                out << "      \"" << us << "\"\n";
        }
    }
#endif

    return out.str();
}




size_t
ustring::memory ()
{
#if ! USE_CONCURRENT_MAP
    ustring_read_lock_t read_lock (ustring_mutex());
#endif
    return ustring_stats_memory;
}

}
OIIO_NAMESPACE_EXIT

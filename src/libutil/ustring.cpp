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

#include <string>

#include <OpenImageIO/export.h>
#include <OpenImageIO/thread.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/dassert.h>
#include <OpenImageIO/ustring.h>

OIIO_NAMESPACE_BEGIN

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


// NOTE: BASE_CAPACITY must be a power of 2
template <unsigned BASE_CAPACITY = 1 << 20, unsigned POOL_SIZE = 4 << 20>
struct TableRepMap {
    TableRepMap() :
        mask(BASE_CAPACITY - 1),
        entries(static_cast<ustring::TableRep**>(calloc(BASE_CAPACITY, sizeof(ustring::TableRep*)))),
        num_entries(0),
        pool(static_cast<char*>(malloc(POOL_SIZE))),
        pool_offset(0),
        memory_usage(sizeof(*this) + POOL_SIZE + sizeof(ustring::TableRep*) * BASE_CAPACITY),
        num_lookups(0) {}

    ~TableRepMap() { /* just let memory leak */ }

    size_t get_memory_usage() {
        ustring_read_lock_t lock(mutex);
        return memory_usage;
    }

    size_t get_num_entries() {
        ustring_read_lock_t lock(mutex);
        return num_entries;
    }

    size_t get_num_lookups() {
        ustring_read_lock_t lock(mutex);
        return num_lookups;
    }

    const char* lookup(string_view str, size_t hash) {
        ustring_read_lock_t lock(mutex);
#if 0
        // NOTE: this simple increment adds a substantial amount of overhead
        // so keep it off by default, unless the user really wants it
        // NOTE2: note that in debug, asserts like the one in ustring::from_unique
        // can skew the number of lookups compared to release builds
        ++num_lookups;
#endif
        size_t pos = hash & mask, dist = 0;
        for (;;) {
            if (entries[pos] == 0) return 0;
            if (entries[pos]->hashed == hash &&
                entries[pos]->length == str.length() &&
                strncmp(entries[pos]->c_str(), str.data(), str.length()) == 0)
                return entries[pos]->c_str();
            ++dist;
            pos = (pos + dist) & mask; // quadratic probing
        }
    }

    const char* insert(string_view str, size_t hash) {
        ustring_write_lock_t lock(mutex);
        size_t pos = hash & mask, dist = 0;
        for (;;) {
            if (entries[pos] == 0) break; // found insert pos
            if (entries[pos]->hashed == hash &&
                entries[pos]->length == str.length() &&
                strncmp(entries[pos]->c_str(), str.data(), str.length()) == 0)
                return entries[pos]->c_str(); // same string is already inserted, return the one that is already in the table
            ++dist;
            pos = (pos + dist) & mask; // quadratic probing
        }

        ustring::TableRep* rep = make_rep(str, hash);
        entries[pos] = rep;
        ++num_entries;
        if (2 * num_entries > mask) grow(); // maintain 0.5 load factor
        return rep->c_str();                // rep is now in the table
    }

private:
    void grow() {
        size_t new_mask = mask * 2 + 1;

        // NOTE: only increment by half because we are doubling the entries and freeing the old
        memory_usage += (mask + 1) * sizeof(ustring::TableRep*);

        ustring::TableRep** new_entries = static_cast<ustring::TableRep**>(calloc(new_mask + 1, sizeof(ustring::TableRep*)));
        size_t to_copy = num_entries;
        for (size_t i = 0; to_copy != 0; i++) {
            if (entries[i] == 0)  continue;
            size_t pos = entries[i]->hashed & new_mask, dist = 0;
            for (;;) {
                if (new_entries[pos] == 0)
                    break;
                ++dist;
                pos = (pos + dist) & new_mask; // quadratic probing
            }
            new_entries[pos] = entries[i];
            to_copy--;
        }

        free(entries);
        entries = new_entries;
        mask    = new_mask;
    }

    ustring::TableRep* make_rep(string_view str, size_t hash) {
        char* repmem = pool_alloc(sizeof(ustring::TableRep) + str.length() + 1);
        return new (repmem) ustring::TableRep (str, hash);;
    }

    char* pool_alloc(size_t len) {
        // round up to nearest multiple of pointer size to guarentee proper alignment of TableRep objects
        len = (len + alignof(ustring::TableRep) - 1) & ~(alignof(ustring::TableRep) - 1);

        if (len >= POOL_SIZE) {
            memory_usage += len;
            return (char*) malloc(len); // no need to try and use the pool
        }
        if (pool_offset + len > POOL_SIZE) {
            // NOTE: old pool will leak - this is ok because ustrings cannot be freed
            memory_usage += POOL_SIZE;
            pool = (char*) malloc(POOL_SIZE);
            pool_offset = 0;
        }
        char* result = pool + pool_offset;
        pool_offset += len;
        return result;
    }

    size_t mask;
    ustring::TableRep** entries;
    size_t num_entries;
    char* pool;
    size_t pool_offset;
    size_t memory_usage;
    size_t num_lookups;
    ustring_mutex_t mutex;
};

#if 0
// Naive map with a single lock for the whole table
typedef TableRepMap<1 << 20, 4 << 20> UstringTable;
#else
// Optimized map broken up into chunks by the top bits of the hash.
// This helps reduce the amount of contention for locks.
struct UstringTable {
    const char* lookup(string_view str, size_t hash) {
        return whichbin(hash).lookup(str, hash);
    }

    const char* insert(string_view str, size_t hash) {
        return whichbin(hash).insert(str, hash);
    }

    size_t get_memory_usage() {
        size_t mem = 0;
        for (auto& bin : bins)
            mem += bin.get_memory_usage();
        return mem;
    }

    size_t get_num_entries() {
        size_t num = 0;
        for (auto& bin : bins)
            num += bin.get_num_entries();
        return num;
    }

    size_t get_num_lookups() {
        size_t num = 0;
        for (auto& bin : bins)
            num += bin.get_num_lookups();
        return num;
    }

private:
    enum {
        BIN_SHIFT = 5,
        NUM_BINS = 1 << BIN_SHIFT, // NOTE: this guarentees NUM_BINS is a power of 2
        TOP_SHIFT = 8 * sizeof(size_t) - BIN_SHIFT
    };

    typedef TableRepMap<(1 << 20) / NUM_BINS, (4 << 20) / NUM_BINS> Bin;

    Bin bins[NUM_BINS];

    Bin& whichbin(size_t hash) {
        // use the top bits of the hash to pick a bin
        // (lower bits choose position within the table)
        return bins[(hash >> TOP_SHIFT) % NUM_BINS];
    }
};
#endif

// This string is here so that we can return sensible values of str when the ustring's pointer is NULL
std::string ustring::empty_std_string;


namespace { // anonymous

static UstringTable & ustring_table ()
{
    static OIIO_CACHE_ALIGN UstringTable table;
    return table;
}

}           // end anonymous namespace


// Put a ustring in the global scope to force at least one call to
// make_unique to happen before main(), i.e. before threads are launched,
// in order to eliminate any possible thread collision on construction of
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



ustring::TableRep::TableRep (string_view strref, size_t hash)
    : hashed(hash)
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

const char *
ustring::make_unique (string_view strref)
{
    UstringTable &table (ustring_table());
    // Eliminate NULLs
    if (! strref.data())
        strref = string_view("", 0);

    size_t hash = Strutil::strhash(strref);

    // Check the ustring table to see if this string already exists.  If so,
    // construct from its canonical representation.
    // NOTE: all locking is performed internally to the table implementation
    const char* result = table.lookup(strref, hash);
    return result ? result : table.insert(strref, hash);
}

std::string
ustring::getstats (bool verbose)
{
    UstringTable &table (ustring_table());
    std::ostringstream out;
    out.imbue (std::locale::classic());  // Force "C" locale with '.' decimal
    size_t n_l = table.get_num_lookups();
    size_t n_e = table.get_num_entries();
    size_t mem = table.get_memory_usage();
    if (verbose) {
        out << "ustring statistics:\n";
        if (n_l) // NOTE: see #if 0 above
        out << "  ustring requests: " << n_l << ", unique " << n_e << "\n";
        else
        out << "  unique strings: " << n_e << "\n";
        out << "  ustring memory: " << Strutil::memformat(mem)
            << "\n";
    } else {
        if (n_l) // NOTE: see #if 0 above
        out << "requests: " << n_l << ", ";
        out << "unique " << n_e
            << ", " << Strutil::memformat(mem);
    }
    return out.str();
}

size_t
ustring::memory ()
{
    UstringTable &table (ustring_table());
    return table.get_memory_usage();
}

OIIO_NAMESPACE_END

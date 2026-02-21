// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <string>
#include <unordered_map>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/export.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/thread.h>
#include <OpenImageIO/unordered_map_concurrent.h>
#include <OpenImageIO/ustring.h>

OIIO_NAMESPACE_3_1_BEGIN

// Use rw spin locks
typedef spin_rw_mutex ustring_mutex_t;
typedef spin_rw_read_lock ustring_read_lock_t;
typedef spin_rw_write_lock ustring_write_lock_t;


#define PREVENT_HASH_COLLISIONS 1

// Explanation of ustring hash non-collision guarantees:
//
// The gist is that the ustring::strhash(str) function is modified to
// strip out the MSB from Strutil::strhash.  The rep entry is filed in
// the ustring table based on this hash.  So effectively, the computed
// hash is 63 bits, not 64.
//
// But rep->hashed field consists of the lower 63 bits being the computed
// hash, and the MSB indicates whether this is the 2nd (or more) entry in
// the table that had the same 63 bit hash.
//
// ustring::hash() then is modified as follows: If the MSB is 0, the
// computed hash is the hash. If the MSB is 1, though, we DON'T use that
// hash, and instead we use the pointer to the unique characters, but
// with the MSB set (that's an invalid address by itself). Note that the
// computed hashes never have MSB set, and the char*+MSB always have MSB
// set, so therefore ustring::hash() will never have the same value for
// two different ustrings.
//
// But -- please note! -- that ustring::strhash(str) and
// ustring(str).hash() will only match (and also be the same value on
// every execution) if the ustring is the first to receive that hash,
// which should be approximately always. Probably always, in practice.
//
// But in the very improbable case of a hash collision, one of them (the
// second to be turned into a ustring) will be using the alternate hash
// based on the character address, which is both not the same as
// ustring::strhash(chars), nor is it expected to be the same constant on
// every program execution.


template<class T> struct identity {
    constexpr T operator()(T val) const noexcept { return val; }
};


namespace {
atomic_ll total_ustring_hash_collisions(0);
}


// #define USTRING_TRACK_NUM_LOOKUPS


template<unsigned BASE_CAPACITY, unsigned POOL_SIZE> struct TableRepMap {
    static_assert((BASE_CAPACITY & (BASE_CAPACITY - 1)) == 0,
                  "BASE_CAPACITY must be a power of 2");

    TableRepMap()
        : entries(static_cast<ustring::TableRep**>(
            calloc(BASE_CAPACITY, sizeof(ustring::TableRep*))))
        , pool(static_cast<char*>(malloc(POOL_SIZE)))
        , memory_usage(sizeof(*this) + POOL_SIZE
                       + sizeof(ustring::TableRep*) * BASE_CAPACITY)
    {
    }

    ~TableRepMap()
    { /* just let memory leak */
    }

    size_t get_memory_usage()
    {
        ustring_read_lock_t lock(mutex);
        return memory_usage;
    }

    size_t get_num_entries()
    {
        ustring_read_lock_t lock(mutex);
        return num_entries;
    }

#ifdef USTRING_TRACK_NUM_LOOKUPS
    size_t get_num_lookups()
    {
        ustring_read_lock_t lock(mutex);
        return num_lookups;
    }
#endif

    const char* lookup(string_view str, uint64_t hash)
    {
        if (OIIO_UNLIKELY(hash & ustring::duplicate_bit)) {
            // duplicate bit is set -- the hash is related to the chars!
            return OIIO::bitcast<const char*>(hash & ustring::hash_mask);
        }
        ustring_read_lock_t lock(mutex);
#ifdef USTRING_TRACK_NUM_LOOKUPS
        // NOTE: this simple increment adds a substantial amount of overhead
        // so keep it off by default, unless the user really wants it
        // NOTE2: note that in debug, asserts like the one in ustring::from_unique
        // can skew the number of lookups compared to release builds
        ++num_lookups;
#endif
        size_t pos = hash & mask, dist = 0;
        for (;;) {
            ustring::TableRep* e = entries[pos];
            if (e == 0)
                return 0;
            if (e->hashed == hash && e->length == str.length()
                && !strncmp(e->c_str(), str.data(), str.length()))
                return e->c_str();
            ++dist;
            pos = (pos + dist) & mask;  // quadratic probing
        }
    }

    // Look up based on hash only. Return nullptr if not found. Note that if
    // the hash is not unique, this will return the first entry that matches
    // the hash.
    const char* lookup(uint64_t hash)
    {
        if (OIIO_UNLIKELY(hash & ustring::duplicate_bit)) {
            // duplicate bit is set -- the hash is related to the chars!
            return OIIO::bitcast<const char*>(hash & ustring::hash_mask);
        }
        ustring_read_lock_t lock(mutex);
#ifdef USTRING_TRACK_NUM_LOOKUPS
        // NOTE: this simple increment adds a substantial amount of overhead
        // so keep it off by default, unless the user really wants it
        // NOTE2: note that in debug, asserts like the one in ustring::from_unique
        // can skew the number of lookups compared to release builds
        ++num_lookups;
#endif
        size_t pos = hash & mask, dist = 0;
        for (;;) {
            ustring::TableRep* e = entries[pos];
            if (e == 0)
                return 0;
            if (e->hashed == hash)
                return e->c_str();
            ++dist;
            pos = (pos + dist) & mask;  // quadratic probing
        }
    }

    const char* insert(string_view str, uint64_t hash)
    {
        OIIO_ASSERT((hash & ustring::duplicate_bit) == 0);  // can't happen?
        ustring_write_lock_t lock(mutex);
        size_t pos = hash & mask, dist = 0;
        bool duplicate_hash = false;
        for (;;) {
            ustring::TableRep* e = entries[pos];
            if (e == 0)
                break;  // found insert pos
            if (e->hashed == hash) {
                duplicate_hash = true;
                if (e->length == str.length()
                    && !strncmp(e->c_str(), str.data(), str.length())) {
                    // same string is already inserted, return the one that is
                    // already in the table
                    return e->c_str();
                }
            }
            ++dist;
            pos = (pos + dist) & mask;  // quadratic probing
        }

        ustring::TableRep* rep = make_rep(str, hash);

        // If we encountered another ustring with the same hash (if one
        // exists, it would have hashed to the same address so we would have
        // seen it), set the duplicate bit in the rep's hashed field.
        if (duplicate_hash) {
            ++total_ustring_hash_collisions;
#if PREVENT_HASH_COLLISIONS
            rep->hashed |= ustring::duplicate_bit;
#endif
#if !defined(NDEBUG)
            print("DUPLICATE ustring '{}' hash {:x} c_str {:p} strhash {:x}\n",
                  rep->c_str(), rep->hashed, rep->c_str(),
                  ustring::strhash(str));
#endif
        }

        entries[pos] = rep;
        ++num_entries;
        if (2 * num_entries > mask)
            grow();  // maintain 0.5 load factor
        // ensure low bit clear
        OIIO_DASSERT((size_t(rep->c_str()) & 1) == 0);
        // ensure low bit clear
        OIIO_DASSERT((size_t(rep->c_str()) & ustring::duplicate_bit) == 0);
        return rep->c_str();  // rep is now in the table
    }

private:
    void grow()
    {
        size_t new_mask = mask * 2 + 1;

        // NOTE: only increment by half because we are doubling the entries and freeing the old
        memory_usage += (mask + 1) * sizeof(ustring::TableRep*);

        ustring::TableRep** new_entries = static_cast<ustring::TableRep**>(
            calloc(new_mask + 1, sizeof(ustring::TableRep*)));
        size_t to_copy = num_entries;
        for (size_t i = 0; to_copy != 0; i++) {
            if (entries[i] == 0)
                continue;
            size_t pos = entries[i]->hashed & new_mask, dist = 0;
            for (;;) {
                if (new_entries[pos] == 0)
                    break;
                ++dist;
                pos = (pos + dist) & new_mask;  // quadratic probing
            }
            new_entries[pos] = entries[i];
            to_copy--;
        }

        free(entries);
        entries = new_entries;
        mask    = new_mask;
    }

    ustring::TableRep* make_rep(string_view str, uint64_t hash)
    {
        char* repmem = pool_alloc(sizeof(ustring::TableRep) + str.length() + 1);
        return new (repmem) ustring::TableRep(str, hash);
    }

    char* pool_alloc(size_t len)
    {
        // round up to nearest multiple of pointer size to guarantee proper alignment of TableRep objects
        len = (len + alignof(ustring::TableRep) - 1)
              & ~(alignof(ustring::TableRep) - 1);

        if (len >= POOL_SIZE) {
            memory_usage += len;
            return (char*)malloc(len);  // no need to try and use the pool
        }
        if (pool_offset + len > POOL_SIZE) {
            // NOTE: old pool will leak - this is ok because ustrings cannot be freed
            memory_usage += POOL_SIZE;
            pool        = (char*)malloc(POOL_SIZE);
            pool_offset = 0;
        }
        char* result = pool + pool_offset;
        pool_offset += len;
        return result;
    }

    OIIO_CACHE_ALIGN mutable ustring_mutex_t mutex;
    size_t mask = BASE_CAPACITY - 1;
    ustring::TableRep** entries;
    size_t num_entries = 0;
    char* pool;
    size_t pool_offset = 0;
    size_t memory_usage;
#ifdef USTRING_TRACK_NUM_LOOKUPS
    size_t num_lookups = 0;
#endif
};

#if 0
// Naive map with a single lock for the whole table
typedef TableRepMap<1 << 20, 16 << 20> UstringTable;
#else
// Optimized map broken up into chunks by the top bits of the hash.
// This helps reduce the amount of contention for locks.
struct UstringTable {
    using hash_t = ustring::hash_t;

    const char* lookup(string_view str, hash_t hash)
    {
        return whichbin(hash).lookup(str, hash);
    }

    const char* lookup(hash_t hash) { return whichbin(hash).lookup(hash); }

    const char* insert(string_view str, uint64_t hash)
    {
        return whichbin(hash).insert(str, hash);
    }

    size_t get_memory_usage()
    {
        size_t mem = 0;
        for (auto& bin : bins)
            mem += bin.get_memory_usage();
        return mem;
    }

    size_t get_num_entries()
    {
        size_t num = 0;
        for (auto& bin : bins)
            num += bin.get_num_entries();
        return num;
    }

#    ifdef USTRING_TRACK_NUM_LOOKUPS
    size_t get_num_lookups()
    {
        size_t num = 0;
        for (auto& bin : bins)
            num += bin.get_num_lookups();
        return num;
    }
#    endif

private:
    enum {
        // NOTE: this guarantees NUM_BINS is a power of 2
        BIN_SHIFT = 12,
        NUM_BINS  = 1 << BIN_SHIFT,
        TOP_SHIFT = 8 * sizeof(size_t) - BIN_SHIFT
    };

    typedef TableRepMap<(1 << 20) / NUM_BINS, (16 << 20) / NUM_BINS> Bin;

    Bin bins[NUM_BINS];

    Bin& whichbin(uint64_t hash)
    {
        // use the top bits of the hash to pick a bin
        // (lower bits choose position within the table)
        return bins[(hash >> TOP_SHIFT) % NUM_BINS];
    }
};
#endif

// This string is here so that we can return sensible values of str when the ustring's pointer is NULL
std::string ustring::empty_std_string;


namespace {  // anonymous

static UstringTable&
ustring_table()
{
    static OIIO_CACHE_ALIGN UstringTable table;
    return table;
}

}  // end anonymous namespace


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

// clang-format off
#ifdef _LIBCPP_VERSION
#ifdef _LIBCPP_ALTERNATE_STRING_LAYOUT
struct libcpp_string__long {
    std::string::pointer __data_;
    std::string::size_type __size_;
    std::string::size_type __cap_;
};
#    ifdef _LIBCPP_BIG_ENDIAN
enum { libcpp_string__long_mask = 0x1ul };
#    else   // _LIBCPP_BIG_ENDIAN
enum { libcpp_string__long_mask = ~(std::string::size_type(~0) >> 1) };
#    endif  // _LIBCPP_BIG_ENDIAN
#else
struct libcpp_string__long {
    std::string::size_type __cap_;
    std::string::size_type __size_;
    std::string::pointer __data_;
};
#    ifdef _LIBCPP_BIG_ENDIAN
enum { libcpp_string__long_mask = ~(std::string::size_type(~0) >> 1) };
#    else   // _LIBCPP_BIG_ENDIAN
enum { libcpp_string__long_mask = 0x1ul };
#    endif  // _LIBCPP_BIG_ENDIAN
#endif

enum {
    libcpp_string__min_cap
    = (sizeof(libcpp_string__long) - 1) / sizeof(std::string::value_type) > 2
          ? (sizeof(libcpp_string__long) - 1) / sizeof(std::string::value_type)
          : 2
};
#endif

// clang-format on
}  // namespace



ustring::TableRep::TableRep(string_view strref, ustring::hash_t hash)
    : hashed(hash)
{
    length = strref.length();
    memcpy((char*)c_str(), strref.data(), length);
    ((char*)c_str())[length] = 0;

    // We don't want the internal 'std::string str' to redundantly store the
    // chars, along with our own allocation.  So we use our knowledge of the
    // internal structure of std::string (for certain compilers) to force
    // the std::string to make it point to our chars!  In such a case, the
    // destructor will be careful not to allow a deallocation.

#if defined(__GNUC__) && !defined(_LIBCPP_VERSION) \
    && defined(_GLIBCXX_USE_CXX11_ABI) && _GLIBCXX_USE_CXX11_ABI
    // NEW gcc ABI
    // FIXME -- do something smart with this.
    str = strref;

#elif defined(__GNUC__) && !defined(_LIBCPP_VERSION)
    // OLD gcc ABI
    // It turns out that the first field of a gcc std::string is a pointer
    // to the characters within the basic_string::_Rep.  We merely redirect
    // that pointer, though for std::string to function properly, the chars
    // must be preceded immediately in memory by the rest of
    // basic_string::_Rep, consisting of length, capacity and refcount
    // fields.  And we have designed our TableRep to do just that!  So now
    // we redirect the std::string's pointer to our own characters and its
    // mocked-up _Rep.
    //
    // See /usr/include/c++/VERSION/bits/basic_string.h for the details of
    // gcc's std::string implementation.
    dummy_capacity      = length;
    dummy_refcount      = 1;  // so it never frees
    *(const char**)&str = c_str();
    OIIO_DASSERT(str.c_str() == c_str() && str.size() == length);

#elif defined(_LIBCPP_VERSION) && !defined(__aarch64__)
    // FIXME -- we seem to do the wrong thing with libcpp on Mac M1. Disable
    // when on aarch64 for now. Come back and fix then when I have easier
    // access to an M1 Mac.
    //
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
    // to the characters in the TableRep we allocated.
    if (length >= libcpp_string__min_cap /* it'll be a "long string" */) {
        ((libcpp_string__long*)&str)->__cap_ = libcpp_string__long_mask
                                               | (length + 1);
        ((libcpp_string__long*)&str)->__size_ = length;
        ((libcpp_string__long*)&str)->__data_ = (char*)c_str();
        OIIO_DASSERT(str.c_str() == c_str() && str.size() == length);
    } else {
        // Short string -- just assign it, since there is no extra allocation.
        str = strref;
    }
#else
    // Remaining cases - just assign the internal string.  This may result
    // in double allocation for the chars.  If you care about that, do
    // something special for your platform, much like we did for gcc and
    // libc++ above. (Windows users, I'm talking to you.)
    str = strref;
#endif
}



ustring::TableRep::~TableRep()
{
    if (str.c_str() == c_str()) {
        // This is one of those cases where we've carefully doctored the
        // string to point to our allocated characters.  To make a safe
        // string destroy, now force it to look like an empty string.
        new (&str) std::string();  // "placement new"
    }
}



const char*
ustring::make_unique(string_view strref)
{
    UstringTable& table(ustring_table());
    // Eliminate nullptr-referred string views
    if (!strref.data())
        strref = string_view("", 0);

    hash_t hash = ustring::strhash(strref);

    // Check the ustring table to see if this string already exists.  If so,
    // construct from its canonical representation.
    // NOTE: all locking is performed internally to the table implementation
    const char* result = table.lookup(strref, hash);
    if (result)
        return result;
    auto nul = strref.find('\0');
    if (nul != string_view::npos) {
        // Strutil::print("ustring::make_unique: string contains nulls @{}/{}: \"{}\"\n",
        //                strref.find('\0'), strref.size(), strref);
        // OIIO_ASSERT(strref.find('\0') == string_view::npos &&
        //             "ustring::make_unique() does not support embedded nulls");
        strref = strref.substr(0, nul);
        hash   = ustring::strhash(strref);
        result = table.lookup(strref, hash);
        if (result)
            return result;
    }
    // Strutil::print("ADDED ustring \"{}\" {:08x}\n", strref, hash);
    return table.insert(strref, hash);
}



ustring
ustring::from_hash(hash_t hash)
{
    UstringTable& table(ustring_table());
    return from_unique(table.lookup(hash));
}



ustring
ustring::concat(string_view s, string_view t)
{
    size_t sl  = s.size();
    size_t tl  = t.size();
    size_t len = sl + tl;
    std::unique_ptr<char[]> heap_buf;
    char local_buf[256];
    char* buf = local_buf;
    if (len > sizeof(local_buf)) {
        heap_buf.reset(new char[len]);
        buf = heap_buf.get();
    }
    memcpy(buf, s.data(), sl);
    memcpy(buf + sl, t.data(), tl);
    return ustring(buf, len);
}



std::string
ustring::getstats(bool verbose)
{
    std::ostringstream out;
    out.imbue(std::locale::classic());  // Force "C" locale with '.' decimal
    size_t n_e = total_ustrings();
    size_t mem = memory();
    if (verbose) {
        print(out, "ustring statistics:\n");
#ifdef USTRING_TRACK_NUM_LOOKUPS
        print(out, "  ustring requests: {}\n",
              ustring_table().get_num_lookups());
#endif
        print(out, "  unique strings: {}\n", n_e);
        print(out, "  ustring memory: {}\n", Strutil::memformat(mem));
        print(out, "  total ustring hash collisions: {}\n",
              (int)total_ustring_hash_collisions);
    } else {
#ifdef USTRING_TRACK_NUM_LOOKUPS
        print(out, "requests: {}, ", ustring_table().get_num_lookups());
#endif
        print(out, "unique {}, {}\n", n_e, Strutil::memformat(mem));
    }
    return out.str();
}



size_t
ustring::hash_collisions(std::vector<ustring>* collisions)
{
    if (collisions) {
        // Disabled for now
        // for (const auto& c : all_hash_collisions)
        //     collisions->emplace_back(ustring::from_unique(c.first));
    }
    return size_t(total_ustring_hash_collisions);
}



size_t
ustring::total_ustrings()
{
    UstringTable& table(ustring_table());
    return table.get_num_entries();
}



size_t
ustring::memory()
{
    UstringTable& table(ustring_table());
    return table.get_memory_usage();
}

OIIO_NAMESPACE_3_1_END

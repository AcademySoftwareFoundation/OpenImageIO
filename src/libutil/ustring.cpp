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

OIIO_NAMESPACE_BEGIN

// Use rw spin locks
typedef spin_rw_mutex ustring_mutex_t;
typedef spin_rw_read_lock ustring_read_lock_t;
typedef spin_rw_write_lock ustring_write_lock_t;


#define PREVENT_HASH_COLLISIONS 1


template<class T> struct identity {
    constexpr T operator()(T val) const noexcept { return val; }
};



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
            if (entries[pos] == 0)
                return 0;
            if (entries[pos]->hashed == hash
                && entries[pos]->length == str.length()
                && strncmp(entries[pos]->c_str(), str.data(), str.length())
                       == 0)
                return entries[pos]->c_str();
            ++dist;
            pos = (pos + dist) & mask;  // quadratic probing
        }
    }

    // Look up based on hash only. Return nullptr if not found. Note that if
    // the hash is not unique, this will return the first entry that matches
    // the hash.
    const char* lookup(uint64_t hash)
    {
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
            if (entries[pos] == 0)
                return 0;
            if (entries[pos]->hashed == hash)
                return entries[pos]->c_str();
            ++dist;
            pos = (pos + dist) & mask;  // quadratic probing
        }
    }

    const char* insert(string_view str, uint64_t hash)
    {
        ustring_write_lock_t lock(mutex);
        size_t pos = hash & mask, dist = 0;
        for (;;) {
            if (entries[pos] == 0)
                break;  // found insert pos
            if (entries[pos]->hashed == hash
                && entries[pos]->length == str.length()
                && !strncmp(entries[pos]->c_str(), str.data(), str.length())) {
                // same string is already inserted, return the one that is
                // already in the table
                return entries[pos]->c_str();
            }
            ++dist;
            pos = (pos + dist) & mask;  // quadratic probing
        }

        ustring::TableRep* rep = make_rep(str, hash);
        entries[pos]           = rep;
        ++num_entries;
        if (2 * num_entries > mask)
            grow();           // maintain 0.5 load factor
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

// The reverse map that lets you look up a string by its initial hash.
using ReverseMap
    = unordered_map_concurrent<uint64_t, const char*, identity<uint64_t>,
                               std::equal_to<uint64_t>, 256 /*bins*/>;


namespace {  // anonymous

static UstringTable&
ustring_table()
{
    static OIIO_CACHE_ALIGN UstringTable table;
    return table;
}


static ReverseMap&
reverse_map()
{
    static OIIO_CACHE_ALIGN ReverseMap rm;
    return rm;
}


// Keep track of any collisions
static std::vector<std::pair<const char*, uint64_t>> all_hash_collisions;
OIIO_CACHE_ALIGN static std::mutex collision_mutex;

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
    return;

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
        return;
    }
#endif

    // Remaining cases - just assign the internal string.  This may result
    // in double allocation for the chars.  If you care about that, do
    // something special for your platform, much like we did for gcc and
    // libc++ above. (Windows users, I'm talking to you.)
    str = strref;
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

    hash_t hash = Strutil::strhash64(strref);
    // This line, if uncommented, lets you force lots of hash collisions:
    // hash &= ~hash_t(0xffffff);

#if !PREVENT_HASH_COLLISIONS
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
        hash   = Strutil::strhash64(strref);
        result = table.lookup(strref, hash);
        if (result)
            return result;
    }
    // Strutil::print("ADDED ustring \"{}\" {:08x}\n", strref, hash);
    return table.insert(strref, hash);

#else
    // Check the ustring table to see if this string already exists with the
    // default hash. If so, we're done. This is by far the common case --
    // most lookups already exist in the table, and hash collisions are
    // extremely rare.
    const char* result = table.lookup(strref, hash);
    if (result)
        return result;

    // ustring doesn't allow strings with embedded nul characters. Before we
    // go any further, trim beyond any nul and rehash.
    auto nul = strref.find('\0');
    if (nul != string_view::npos) {
        // Strutil::print("ustring::make_unique: string contains nulls @{}/{}: \"{}\"\n",
        //                strref.find('\0'), strref.size(), strref);
        // OIIO_ASSERT(strref.find('\0') == string_view::npos &&
        //             "ustring::make_unique() does not support embedded nulls");
        strref = strref.substr(0, nul);
        hash   = Strutil::strhash64(strref);
        result = table.lookup(strref, hash);
        if (result)
            return result;
    }

    // We did not find it. There are two possibilities: (1) the string is in
    // the table but has a different hash because it collided; or (2) the
    // string is not yet in the table.

    // Thread safety by locking reverse_map's bin corresponding to our
    // original hash. This will prevent any potentially colliding ustring
    // from being added to either table. But ustrings whose hashes go to
    // different bins of the reverse map (which by definition cannot clash)
    // are allowed to be added concurrently.
    auto& rm(reverse_map());
    size_t bin = rm.lock_bin(hash);

    hash_t orighash     = hash;
    size_t binmask      = orighash & (~rm.nobin_mask());
    size_t num_rehashes = 0;

    while (1) {
        auto rev = rm.find(hash, false);
        // rev now either holds an iterator into the reverse map for a
        // record that has this hash, or else it's end().
        if (rev == rm.end()) {
            // That hash is unused, insert the string with that hash into
            // the ustring table, and insert the hash with the unique char
            // pointer into the reverse_map.
            result  = table.insert(strref, hash);
            bool ok = rm.insert(hash, result, false);
            // Strutil::print("ADDED \"{}\" {:08x}\n", strref, hash);
            OIIO_ASSERT(ok && "thread safety failure");
            break;
        }
        // Something uses this hash. Is it our string?
        if (!strncmp(rev->second, strref.data(), strref.size())) {
            // It is our string, already in this hash slot!
            result = rev->second;
            break;
        }
        // Rehash, but keep the bin bits identical so we always rehash into
        // the same (locked) bin.
        hash = (hash & binmask)
               | (farmhash::Fingerprint(hash) & rm.nobin_mask());
        ++num_rehashes;
        // Strutil::print("COLLISION \"{}\" {:08x} vs \"{}\"\n",
        //                strref, orighash, rev->second);
        {
            std::lock_guard<std::mutex> lock(collision_mutex);
            all_hash_collisions.emplace_back(rev->second, rev->first);
        }
    }
    rm.unlock_bin(bin);

    if (num_rehashes) {
        std::lock_guard<std::mutex> lock(collision_mutex);
        all_hash_collisions.emplace_back(result, orighash);
    }

    return result;
#endif
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
        out << "ustring statistics:\n";
#ifdef USTRING_TRACK_NUM_LOOKUPS
        out << "  ustring requests: " << ustring_table().get_num_lookups()
            << "\n";
#endif
        out << "  unique strings: " << n_e << "\n";
        out << "  ustring memory: " << Strutil::memformat(mem) << "\n";
#ifndef NDEBUG
        std::vector<ustring> collisions;
        hash_collisions(&collisions);
        if (collisions.size()) {
            out << "  Hash collisions: " << collisions.size() << "\n";
            for (auto c : collisions)
                out << Strutil::fmt::format("    {} \"{}\"\n", c.hash(), c);
        }
#endif
    } else {
#ifdef USTRING_TRACK_NUM_LOOKUPS
        out << "requests: " << ustring_table().get_num_lookups() << ", ";
#endif
        out << "unique " << n_e << ", " << Strutil::memformat(mem);
    }
    return out.str();
}



size_t
ustring::hash_collisions(std::vector<ustring>* collisions)
{
    std::lock_guard<std::mutex> lock(collision_mutex);
    if (collisions)
        for (const auto& c : all_hash_collisions)
            collisions->emplace_back(ustring::from_unique(c.first));
    return all_hash_collisions.size();
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

OIIO_NAMESPACE_END

// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


/// \file
///
/// Define the ParamValue and ParamValueList classes, which are used to
/// store lists of arbitrary name/data pairs for internal storage of
/// parameter lists, attributes, geometric primitive data, etc.


#pragma once

#include <vector>

#include <OpenImageIO/attrdelegate.h>
#include <OpenImageIO/export.h>
#include <OpenImageIO/strongparam.h>
#include <OpenImageIO/typedesc.h>
#include <OpenImageIO/ustring.h>


OIIO_NAMESPACE_BEGIN

/// ParamValue holds a named parameter and typed data. Usually, it owns the
/// data (holding it in the struct itself if small enough, dynamically
/// allocated for larger things), but it can also refer to non-owned data.
///
/// The data is usually a single value of any type described by TypeDesc
/// (including arrays). It may also hold more than one value of the type --
/// this is usually only used in a geometric context, such as storing a value
/// for each vertex in a mesh. Please note the subtle distinction between the
/// value type itself being an array, versus having multiple values as a
/// parameter, versus the type of the value having multiple components (such
/// as a point or color). Any combination of these may be present.
///
/// To clarify, if you have an array of 4 colors for each of 15 mesh
/// vertices, that means:
///  - There are 15 VALUES (one for each vertex)
///  - Each value has an array of 4 ELEMENTS
///  - Each element is a color
///  - A color has 3 COMPONENTS (R, G, B)
/// Thus, it would be constructed as
/// `ParamValue("mycolor", TypeDesc(TypeDesc::COLOR, 4), 15, ptr_to_data)`
///
/// The main constructor is `ParamValue(name, type, nvalues, dataptr)`. It can
/// be confusing at first to remember that the data argument is a pointer to
/// the first values to copy, not the values themselves, even if the values
/// are themselves pointers, and even if the number of values is 1. In other
/// words, it's behaving as if you're always pointing it to an array even if
/// the "array" has only one element. This is extra confusing for strings,
/// because the strings themselves are `char*` (or ustring), so the pointer
/// you need to pass is `char**`. For this reason, there are also convenience
/// constructors for simple types such as a single int, float, or string.
///
/// So here are some examples:
///
///     // Single int:
///     int my_int = 42;
///     ParamValue A("foo", TypeDesc::INT, 1, &my_int);
///     // Three int values (say, one per vertex of a triangle):
///     int my_int_array[3] = { 1, 2, 3 };
///     ParamValue B("foo", TypeDesc::INT, 1, &my_int_array);
///     // A single value which is an array of 3 ints:
///     ParamValue C("foo", TypeDesc(TypeDesc::INT, 3), 1, &my_int_array);
///     // A string -- note the trick about treating it as an array:
///     const char* my_string = "hello";
///     ParamValue D("foo", TypeDesc::STRING, 1, &my_string);
///
///     // The most common cases also have simple "duck-typed" convenience
///     // constructors:
///     ParamValue A("foo", 42);           // single int
///     ParamValue B("foo", 42.0f);        // single float
///     ParamValue C("foo", "forty two");  // single string

class OIIO_UTIL_API ParamValue {
public:
    /// Interpolation types
    ///
    enum Interp {
        INTERP_CONSTANT = 0,  ///< Constant for all pieces/faces
        INTERP_PERPIECE = 1,  ///< Piecewise constant per piece/face
        INTERP_LINEAR   = 2,  ///< Linearly interpolated across each piece/face
        INTERP_VERTEX   = 3   ///< Interpolated like vertices
    };

    OIIO_STRONG_PARAM_TYPE(Copy, bool);
    OIIO_STRONG_PARAM_TYPE(FromUstring, bool);

    ParamValue() noexcept { m_data.ptr = nullptr; }

    ParamValue(const ustring& _name, TypeDesc _type, int _nvalues,
               const void* _value, Copy _copy = Copy(true)) noexcept
    {
        init_noclear(_name, _type, _nvalues, _value, _copy);
    }
    ParamValue(const ustring& _name, TypeDesc _type, int _nvalues,
               Interp _interp, const void* _value,
               Copy _copy = Copy(true)) noexcept
    {
        init_noclear(_name, _type, _nvalues, _interp, _value, _copy);
    }
    ParamValue(string_view _name, TypeDesc _type, int _nvalues,
               const void* _value, Copy _copy = Copy(true)) noexcept
    {
        init_noclear(ustring(_name), _type, _nvalues, _value, _copy);
    }
    ParamValue(string_view _name, TypeDesc _type, int _nvalues, Interp _interp,
               const void* _value, Copy _copy = Copy(true)) noexcept
    {
        init_noclear(ustring(_name), _type, _nvalues, _interp, _value, _copy);
    }

    ParamValue(string_view _name, int value) noexcept
    {
        init_noclear(ustring(_name), TypeDesc::INT, 1, &value);
    }
    ParamValue(string_view _name, float value) noexcept
    {
        init_noclear(ustring(_name), TypeDesc::FLOAT, 1, &value);
    }
    ParamValue(string_view _name, ustring value) noexcept
    {
        init_noclear(ustring(_name), TypeDesc::STRING, 1, &value, Copy(true),
                     FromUstring(true));
    }
    ParamValue(string_view _name, string_view value) noexcept
        : ParamValue(_name, ustring(value))
    {
    }
    ParamValue(string_view _name, ustringhash value) noexcept
    {
        init_noclear(ustring(_name), TypeDesc::USTRINGHASH, 1, &value,
                     Copy(true));
    }

    // Set from string -- parse
    ParamValue(string_view _name, TypeDesc type, string_view value);

    // Copy constructor
    ParamValue(const ParamValue& p) noexcept
    {
        init_noclear(p.name(), p.type(), p.nvalues(), p.interp(), p.data(),
                     Copy(true), FromUstring(true));
    }
    ParamValue(const ParamValue& p, Copy _copy) noexcept
    {
        init_noclear(p.name(), p.type(), p.nvalues(), p.interp(), p.data(),
                     _copy, FromUstring(true));
    }

    // Rvalue (move) constructor
    ParamValue(ParamValue&& p) noexcept
    {
        init_noclear(p.name(), p.type(), p.nvalues(), p.interp(), p.data(),
                     Copy(false), FromUstring(true));
        m_copy       = p.m_copy;
        m_nonlocal   = p.m_nonlocal;
        p.m_data.ptr = nullptr;  // make sure the old one won't free
    }

    ~ParamValue() noexcept { clear_value(); }

    void init(ustring _name, TypeDesc _type, int _nvalues, Interp _interp,
              const void* _value, Copy _copy) noexcept
    {
        clear_value();
        init_noclear(_name, _type, _nvalues, _interp, _value, _copy);
    }
    void init(ustring _name, TypeDesc _type, int _nvalues, const void* _value,
              Copy _copy = Copy(true)) noexcept
    {
        init(_name, _type, _nvalues, INTERP_CONSTANT, _value, _copy);
    }
    void init(string_view _name, TypeDesc _type, int _nvalues,
              const void* _value, Copy _copy = Copy(true)) noexcept
    {
        init(ustring(_name), _type, _nvalues, _value, _copy);
    }
    void init(string_view _name, TypeDesc _type, int _nvalues, Interp _interp,
              const void* _value, Copy _copy = Copy(true)) noexcept
    {
        init(ustring(_name), _type, _nvalues, _interp, _value, _copy);
    }

    // Assignment
    const ParamValue& operator=(const ParamValue& p) noexcept;
    const ParamValue& operator=(ParamValue&& p) noexcept;

    // FIXME -- some time in the future (after more cleanup), we should make
    // name() return a string_view, and use uname() for the rare time when
    // the caller truly requires the ustring.
    const ustring& name() const noexcept { return m_name; }
    const ustring& uname() const noexcept { return m_name; }
    TypeDesc type() const noexcept { return m_type; }
    int nvalues() const noexcept { return m_nvalues; }
    const void* data() const noexcept
    {
        return m_nonlocal ? m_data.ptr : &m_data;
    }
    int datasize() const noexcept
    {
        return m_nvalues * static_cast<int>(m_type.size());
    }
    Interp interp() const noexcept { return (Interp)m_interp; }
    void interp(Interp i) noexcept { m_interp = (unsigned char)i; }
    bool is_nonlocal() const noexcept { return m_nonlocal; }

    friend void swap(ParamValue& a, ParamValue& b) noexcept
    {
        auto tmp = std::move(a);
        a        = std::move(b);
        b        = std::move(tmp);
    }

    // Use with extreme caution! This is just doing a cast. You'd better
    // be really sure you are asking for the right type. Note that for
    // "string" data, you can get<ustring> or get<char*>, but it's not
    // a std::string.
    template<typename T> const T& get(int i = 0) const noexcept
    {
        OIIO_DASSERT(i >= 0 && i < int(m_nvalues * m_type.basevalues())
                     && "OIIO::ParamValue::get() range check");
        return (reinterpret_cast<const T*>(data()))[i];
    }

    /// Retrieve an integer, with conversions from a wide variety of type
    /// cases, including unsigned, short, byte. Not float. It will retrieve
    /// from a string, but only if the string is entirely a valid int
    /// format. Unconvertible types return the default value.
    int get_int(int defaultval = 0) const;
    int get_int_indexed(int index, int defaultval = 0) const;

    /// Retrieve a float, with conversions from a wide variety of type
    /// cases, including integers. It will retrieve from a string, but only
    /// if the string is entirely a valid float format. Unconvertible types
    /// return the default value.
    float get_float(float defaultval = 0) const;
    float get_float_indexed(int index, float defaultval = 0) const;

    /// Convert any type to a string value. An optional maximum number of
    /// elements is also passed. In the case of a single string, just the
    /// string directly is returned. But for an array of strings, the array
    /// is returned as one string that's a comma-separated list of double-
    /// quoted, escaped strings. For an array or aggregate, at most `maxsize`
    /// elements are returned (if `maxsize` is 0, all elements are returned,
    /// no matter how large it is).
    std::string get_string(int maxsize = 64) const;
    std::string get_string_indexed(int index) const;
    /// Convert any type to a ustring value. An optional maximum number of
    /// elements is also passed. Same behavior as get_string, but returning a
    /// ustring. For an array or aggregate, at most `maxsize` elements are
    /// returned (if `maxsize` is 0, all elements are returned, no matter how
    /// large it is).
    ustring get_ustring(int maxsize = 64) const;
    ustring get_ustring_indexed(int index) const;

private:
    ustring m_name;   ///< data name
    TypeDesc m_type;  ///< data type, which may itself be an array
    union {
        char localval[16];
        const void* ptr;
    } m_data;  ///< Our data, either a pointer or small local value
    int m_nvalues          = 0;  ///< number of values of the given type
    unsigned char m_interp = INTERP_CONSTANT;  ///< Interpolation type
    bool m_copy            = false;
    bool m_nonlocal        = false;

    void init_noclear(ustring _name, TypeDesc _type, int _nvalues,
                      const void* _value, Copy _copy = Copy(true),
                      FromUstring _from_ustring = FromUstring(false)) noexcept;
    void init_noclear(ustring _name, TypeDesc _type, int _nvalues,
                      Interp _interp, const void* _value,
                      Copy _copy                = Copy(true),
                      FromUstring _from_ustring = FromUstring(false)) noexcept;
    void clear_value() noexcept;
};



/// Factory for a ParamValue that holds a single value of any type supported
/// by a corresponding ParamValue constructor (such as int, float, string).
template<typename T>
static ParamValue
make_pv(string_view name, const T& val)
{
    return ParamValue(name, val);
}

/// Factory for a ParamValue from a pointer. Passing `char*` or `const char*`
/// will be interpreted as a C string (TypeString), but all other pointer
/// types will just get stored as an opaque pointer (TypePointer).
template<typename T>
static ParamValue
make_pv(string_view name, T* val)
{
    return ParamValue(name, BaseTypeFromC<T*>::value, 1, &val);
}



/// A list of ParamValue entries, that can be iterated over or searched.
/// It's really just a std::vector<ParamValue>, but with a few more handy
/// methods.
class OIIO_UTIL_API ParamValueList : public std::vector<ParamValue> {
public:
    ParamValueList() {}

    /// Add space for one more ParamValue to the list, and return a
    /// reference to its slot.
    reference grow()
    {
        resize(size() + 1);
        return back();
    }

    /// Find the first entry with matching name, and if type != UNKNOWN,
    /// then also with matching type. The name search is case sensitive if
    /// casesensitive == true.
    iterator find(string_view name, TypeDesc type = TypeDesc::UNKNOWN,
                  bool casesensitive = true);
    iterator find(ustring name, TypeDesc type = TypeDesc::UNKNOWN,
                  bool casesensitive = true);
    const_iterator find(string_view name, TypeDesc type = TypeDesc::UNKNOWN,
                        bool casesensitive = true) const;
    const_iterator find(ustring name, TypeDesc type = TypeDesc::UNKNOWN,
                        bool casesensitive = true) const;

    /// Search for the first entry with matching name, etc., and return
    /// a pointer to it, or nullptr if it is not found.
    ParamValue* find_pv(string_view name, TypeDesc type = TypeDesc::UNKNOWN,
                        bool casesensitive = true)
    {
        iterator f = find(name, type, casesensitive);
        return f != end() ? &(*f) : nullptr;
    }
    const ParamValue* find_pv(string_view name,
                              TypeDesc type      = TypeDesc::UNKNOWN,
                              bool casesensitive = true) const
    {
        const_iterator f = find(name, type, casesensitive);
        return f != cend() ? &(*f) : nullptr;
    }

    /// Search for an integer, with default if not found. Automatically will
    /// return an int even if the data is really unsigned, short, or byte, but
    /// not float. It will retrieve from a string, but only if the string is
    /// entirely a valid int format.
    int get_int(string_view name, int defaultval = 0,
                bool casesensitive = false, bool convert = true) const;

    /// Search for a float, with default if not found. Automatically will
    /// return a float even if the data is really double or half. It will
    /// retrieve from a string, but only if the string is entirely a valid
    /// float format.
    float get_float(string_view name, float defaultval = 0,
                    bool casesensitive = false, bool convert = true) const;

    /// Simple way to get a string attribute, with default provided.
    /// If the value is another type, it will be turned into a string.
    string_view get_string(string_view name,
                           string_view defaultval = string_view(),
                           bool casesensitive     = false,
                           bool convert           = true) const;
    ustring get_ustring(string_view name,
                        string_view defaultval = string_view(),
                        bool casesensitive = false, bool convert = true) const;

    /// Remove the named parameter, if it is in the list.
    void remove(string_view name, TypeDesc type = TypeDesc::UNKNOWN,
                bool casesensitive = true);

    /// Does the list contain the named attribute?
    bool contains(string_view name, TypeDesc type = TypeDesc::UNKNOWN,
                  bool casesensitive = true) const;

    // Add the param to the list, replacing in-place any existing one with
    // the same name.
    void add_or_replace(const ParamValue& pv, bool casesensitive = true);
    void add_or_replace(ParamValue&& pv, bool casesensitive = true);

    /// Add (or replace) a value in the list.
    void attribute(string_view name, TypeDesc type, int nvalues,
                   const void* value)
    {
        if (!name.empty())
            add_or_replace(ParamValue(name, type, nvalues, value));
    }

    void attribute(string_view name, TypeDesc type, const void* value)
    {
        attribute(name, type, 1, value);
    }

    /// Set directly from string -- parse if type is non-string.
    void attribute(string_view name, TypeDesc type, string_view value)
    {
        if (!name.empty())
            add_or_replace(ParamValue(name, type, value));
    }

    // Shortcuts for single value of common types.
    void attribute(string_view name, int value)
    {
        attribute(name, TypeInt, 1, &value);
    }
    void attribute(string_view name, unsigned int value)
    {
        attribute(name, TypeUInt, 1, &value);
    }
    void attribute(string_view name, float value)
    {
        attribute(name, TypeFloat, 1, &value);
    }
    void attribute(string_view name, string_view value)
    {
        ustring v(value);
        attribute(name, TypeString, 1, &v);
    }

    void attribute(string_view name, ustring value)
    {
        if (!name.empty())
            add_or_replace(ParamValue(name, value));
    }

    /// Search list for named item, return its type or TypeUnknown if not
    /// found.
    TypeDesc getattributetype(string_view name,
                              bool casesensitive = false) const
    {
        auto p = find(name, TypeUnknown, casesensitive);
        return p != cend() ? p->type() : TypeUnknown;
    }

    /// Retrieve from list: If found its data type is reasonably convertible
    /// to `type`, copy/convert the value into val[...] and return true.
    /// Otherwise, return false and don't modify what val points to.
    bool getattribute(string_view name, TypeDesc type, void* value,
                      bool casesensitive = false) const;
    /// Shortcut for retrieving a single string via getattribute.
    bool getattribute(string_view name, std::string& value,
                      bool casesensitive = false) const;

    /// Retrieve from list: If found its data type is reasonably convertible
    /// to `type`, copy/convert the value into val[...] and return true.
    /// Otherwise, return false and don't modify what val points to.
    bool getattribute_indexed(string_view name, int index, TypeDesc type,
                              void* value, bool casesensitive = false) const;
    /// Shortcut for retrieving a single string via getattribute.
    bool getattribute_indexed(string_view name, int index, std::string& value,
                              bool casesensitive = false) const;

    /// Sort alphabetically, optionally case-insensitively, locale-
    /// independently, and with all the "un-namespaced" items appearing
    /// first, followed by items with "prefixed namespaces" (e.g. "z" comes
    /// before "foo:a").
    void sort(bool casesensitive = true);

    /// Merge items from PVL `other` into `*this`. Note how this differs
    /// from `operator=` : assignment completely replaces the list with
    /// the contents of another. But merge() adds the other items without
    /// erasing any items already in this list.
    ///
    /// @param other
    ///     The ParamValueList whose entries will be merged into this one.
    /// @param override
    ///     If true, `other` attributes will replace any identically-named
    ///     attributes already in this list. If false, only attributes whose
    ///     names are not already in this list will be appended.
    void merge(const ParamValueList& other, bool override = false);

    /// Even more radical than clear, free ALL memory associated with the
    /// list itself.
    void free()
    {
        clear();
        shrink_to_fit();
    }

    /// Array indexing by integer will return a reference to the ParamValue
    /// in that position of the list.
    ParamValue& operator[](int index)
    {
        return std::vector<ParamValue>::operator[](index);
    }
    const ParamValue& operator[](int index) const
    {
        return std::vector<ParamValue>::operator[](index);
    }

    /// Array indexing by string will create a "Delegate" that enables a
    /// convenient shorthand for adding and retrieving values from the list:
    ///
    /// 1. Assigning to the delegate adds a ParamValue to the list:
    ///        ParamValueList list;
    ///        list["foo"] = 42;       // adds integer
    ///        list["bar"] = 39.8f;    // adds float
    ///        list["baz"] = "hello";  // adds string
    ///    Be very careful, the attribute's type will be implied by the C++
    ///    type of what you assign.
    ///
    /// 2. The delegate supports a get<T>() that retrieves an item of type T:
    ///         int i = list["foo"].get<int>();
    ///         std::string s = list["baz"].get<std::string>();
    ///
    AttrDelegate<const ParamValueList> operator[](string_view name) const
    {
        return { this, name };
    }
    AttrDelegate<ParamValueList> operator[](string_view name)
    {
        return { this, name };
    }
};



/// A span of const ParamValue entries, that can be iterated over or searched.
/// It's really just a cspan<ParamValue>, but with a few more handy methods.
/// This is a convenient way to pass the contents of a ParamValueList (or its
/// equivalent of any consecutive subarray of PV's) without any copies.
class OIIO_UTIL_API ParamValueSpan : public cspan<ParamValue> {
public:
    // Note: inherits from cspan:
    //   - size()
    //   - data()
    //   - operator[int]
    //   - begin(), end(), cbegin(), cend()

    ParamValueSpan() = default;

    ParamValueSpan(cspan<ParamValue> p)
        : cspan<ParamValue>(p)
    {
    }

    // Trivially make a ParamValueSpan from a ParamValueList
    ParamValueSpan(const ParamValueList& p)
        : cspan<ParamValue>(p)
    {
    }

    /// Construct a span from an initializer_list.
    constexpr ParamValueSpan(std::initializer_list<ParamValue> il)
        : cspan<ParamValue>(il.begin(), il.size())
    {
    }

    /// Construct from a fixed-length C array.  Template magic automatically
    /// finds the length from the declared type of the array.
    template<size_t N>
    constexpr ParamValueSpan(const ParamValue (&data)[N])
        : cspan<ParamValue>(data, N)
    {
    }

    constexpr reference operator[](size_type idx) const
    {
        return cspan<ParamValue>::operator[](idx);
    }

    const_iterator find(string_view name, TypeDesc type = TypeUnknown,
                        bool casesensitive = false) const;
    const_iterator find(ustring name, TypeDesc type = TypeUnknown,
                        bool casesensitive = false) const;

    /// Search for an integer, with default if not found. Automatically will
    /// return an int even if the data is really unsigned, short, or byte, but
    /// not float. It will retrieve from a string, but only if the string is
    /// entirely a valid int format.
    int get_int(string_view name, int defaultval = 0,
                bool casesensitive = false, bool convert = true) const;
    int get_int(ustring name, int defaultval = 0, bool casesensitive = false,
                bool convert = true) const;

    /// Search for a float, with default if not found. Automatically will
    /// return a float even if the data is really double or half. It will
    /// retrieve from a string, but only if the string is entirely a valid
    /// float format.
    float get_float(string_view name, float defaultval = 0,
                    bool casesensitive = false, bool convert = true) const;
    float get_float(ustring name, float defaultval = 0,
                    bool casesensitive = false, bool convert = true) const;

    /// Simple way to get a string attribute, with default provided.
    /// If the value is another type, it will be turned into a string.
    string_view get_string(string_view name,
                           string_view defaultval = string_view(),
                           bool casesensitive     = false,
                           bool convert           = true) const;
    string_view get_string(ustring name, string_view defaultval = string_view(),
                           bool casesensitive = false,
                           bool convert       = true) const;
    ustring get_ustring(string_view name,
                        string_view defaultval = string_view(),
                        bool casesensitive = false, bool convert = true) const;
    ustring get_ustring(ustring name, string_view defaultval = string_view(),
                        bool casesensitive = false, bool convert = true) const;

    /// Search for the attribute and return its "truth-like" value: false if
    /// it exists but is empty, or is a numeric value equal to 0, or a string
    /// value that is "0", "no", "off", or "false". Otherwise, any non-empty
    /// value returns true.
    bool get_bool(string_view name, bool defaultval = false,
                  bool casesensitive = false) const;
    bool get_bool(ustring name, bool defaultval = false,
                  bool casesensitive = false) const;

    /// Does the span contain the named attribute?
    bool contains(string_view name, TypeDesc type = TypeUnknown,
                  bool casesensitive = false) const
    {
        return (find(name, type, casesensitive) != end());
    }
    bool contains(ustring name, TypeDesc type = TypeUnknown,
                  bool casesensitive = false) const
    {
        return (find(name, type, casesensitive) != end());
    }

    /// Search list for named item, return its type or TypeUnknown if not
    /// found.
    TypeDesc getattributetype(string_view name,
                              bool casesensitive = false) const
    {
        auto p = find(name, TypeUnknown, casesensitive);
        return p != cend() ? p->type() : TypeUnknown;
    }

    /// Retrieve from list: If found and its data type is reasonably convertible
    /// to `type`, copy/convert the value into val[...] and return true.
    /// Otherwise, return false and don't modify what val points to.
    bool getattribute(string_view name, TypeDesc type, void* value,
                      bool casesensitive = false) const;
    /// Shortcut for retrieving a single string via getattribute.
    bool getattribute(string_view name, std::string& value,
                      bool casesensitive = false) const;

    /// Retrieve from list: If found its data type is reasonably convertible
    /// to `type`, copy/convert the value into val[...] and return true.
    /// Otherwise, return false and don't modify what val points to.
    bool getattribute_indexed(string_view name, int index, TypeDesc type,
                              void* value, bool casesensitive = false) const;
    /// Shortcut for retrieving a single string via getattribute.
    bool getattribute_indexed(string_view name, int index, std::string& value,
                              bool casesensitive = false) const;

    // Inherits operator[int] from span

    /// Array indexing by string will create a "Delegate" that enables a
    /// convenient shorthand for retrieving a value:
    ///
    ///     int i = list["foo"].get<int>();
    ///     std::string s = list["baz"].get<std::string>();
    ///
    AttrDelegate<const ParamValueSpan> operator[](string_view name) const
    {
        return { this, name };
    }
};


OIIO_NAMESPACE_END

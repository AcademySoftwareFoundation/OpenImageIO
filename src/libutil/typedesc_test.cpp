// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <limits>
#include <type_traits>

#include <OpenImageIO/Imath.h>

#include <Imath/ImathBox.h>

#include <OpenEXR/ImfKeyCode.h>
#include <OpenEXR/ImfTimeCode.h>

#include <OpenImageIO/typedesc.h>
#include <OpenImageIO/unittest.h>
#include <OpenImageIO/ustring.h>


using namespace OIIO;


struct notype {};



// Several tests for a TypeDesc type. The template parameter `Ctype` is how
// we store the data in C++. `textrep` is the textual representation,
// like "float". `constructed` is the TypeDesc we are testing, constructed
// (like `TypeDesc(TypeDesc::FLOAT)`). `named` is the pre-constructed alias,
// if there is one (like `TypeFloat`). `value` is data in C++ holding that
// type, and `valuerep` is what `value` is expected to look like when
// rendered as a string.
template<typename CType>
void
test_type(string_view textrep, TypeDesc constructed,
          TypeDesc named = TypeUnknown, const CType& value = CType(),
          string_view valuerep = "")
{
    Strutil::print("Testing {}\n", textrep);

    // Make sure constructing by name from string matches the TypeDesc where
    // it was constructed in C++.
    OIIO_CHECK_EQUAL(constructed, TypeDesc(textrep));

    // Make sure that the pre-constructed alias (if passed) matches the
    // fully constructed type.
    if (named != TypeUnknown)
        OIIO_CHECK_EQUAL(constructed, named);

    // Make sure the size() matches the size of the equivalent C++ data.
    OIIO_CHECK_EQUAL(constructed.size(), sizeof(value));

    // Verify that rendering the sample data `value` as a string matches
    // what we expect.
    {
        tostring_formatting fm;
        fm.aggregate_sep = ", ";
        fm.array_sep     = ", ";
        std::string s    = tostring(constructed, &value, fm);
        if (valuerep.size()) {
            OIIO_CHECK_EQUAL(s, valuerep);
            Strutil::print("  {}\n", s);
        }
    }

    {
        tostring_formatting fm(tostring_formatting::STDFORMAT);
        fm.aggregate_sep = ", ";
        fm.array_sep     = ", ";
#if FMT_VERSION < 70100
        fm.float_fmt = "{:g}";
#endif
        std::string s = tostring(constructed, &value, fm);
        if (valuerep.size()) {
            OIIO_CHECK_EQUAL(s, valuerep);
            Strutil::print("  {}\n", s);
        }
    }
}



static void
test_templates()
{
    print("Testing templates\n");
    OIIO_CHECK_EQUAL(BaseTypeFromC<float>::value, TypeDesc::FLOAT);
    OIIO_CHECK_EQUAL(BaseTypeFromC<int>::value, TypeDesc::INT);
    OIIO_CHECK_EQUAL(BaseTypeFromC<char*>::value, TypeDesc::STRING);
    OIIO_CHECK_EQUAL(BaseTypeFromC<ustring>::value, TypeDesc::STRING);
    OIIO_CHECK_EQUAL(BaseTypeFromC<void*>::value, TypeDesc::PTR);
    OIIO_CHECK_EQUAL(BaseTypeFromC<int*>::value, TypeDesc::PTR);

    OIIO_CHECK_EQUAL(TypeDescFromC<float>::value(), TypeFloat);
    OIIO_CHECK_EQUAL(TypeDescFromC<int>::value(), TypeInt);
    OIIO_CHECK_EQUAL(TypeDescFromC<ustring>::value(), TypeString);
    OIIO_CHECK_EQUAL(TypeDescFromC<char*>::value(), TypeString);
    OIIO_CHECK_EQUAL(TypeDescFromC<const char*>::value(), TypeString);
    OIIO_CHECK_EQUAL(TypeDescFromC<void*>::value(), TypePointer);
    OIIO_CHECK_EQUAL(TypeDescFromC<const void*>::value(), TypePointer);
    OIIO_CHECK_EQUAL(TypeDescFromC<int*>::value(), TypePointer);
}



int
main(int /*argc*/, char* /*argv*/[])
{
    std::cout << "TypeDesc size = " << sizeof(TypeDesc) << "\n";
    // We expect a TypeDesc to be the same size as a 64 bit int
    OIIO_CHECK_EQUAL(sizeof(TypeDesc), sizeof(uint64_t));

    test_templates();

    test_type<float>("float", TypeDesc(TypeDesc::FLOAT), TypeFloat, 1.5f,
                     "1.5");
    test_type<half>("half", TypeDesc(TypeDesc::HALF), TypeHalf, half(1.5f),
                    "1.5");
    test_type<double>("double", TypeDesc(TypeDesc::DOUBLE), TypeUnknown, 1.5,
                      "1.5");
    test_type<int>("int", TypeDesc(TypeDesc::INT), TypeInt, 1, "1");
    test_type<unsigned int>("uint", TypeDesc(TypeDesc::UINT), TypeUInt, 1, "1");
    test_type<int32_t>("int", TypeDesc(TypeDesc::INT), TypeInt, 1, "1");
    test_type<uint32_t>("uint", TypeDesc(TypeDesc::UINT), TypeUInt, 1, "1");
    test_type<int64_t>("int64", TypeDesc(TypeDesc::INT64), TypeInt64, 1, "1");
    test_type<uint64_t>("uint64", TypeDesc(TypeDesc::UINT64), TypeUInt64, 1,
                        "1");
    test_type<int16_t>("int16", TypeDesc(TypeDesc::INT16), TypeInt16, 1, "1");
    test_type<uint16_t>("uint16", TypeDesc(TypeDesc::UINT16), TypeUInt16, 1,
                        "1");
    test_type<int8_t>("int8", TypeDesc(TypeDesc::INT8), TypeInt8, 1, "1");
    test_type<uint8_t>("uint8", TypeDesc(TypeDesc::UINT8), TypeUInt8, 1, "1");

    // test_type<>("", TypeDesc(TypeDesc::UNKNOWN));
    test_type<Imath::Color3f>("color",
                              TypeDesc(TypeDesc::FLOAT, TypeDesc::VEC3,
                                       TypeDesc::COLOR),
                              TypeColor, Imath::Color3f(0.0f), "(0, 0, 0)");
    test_type<Imath::V3f>("point",
                          TypeDesc(TypeDesc::FLOAT, TypeDesc::VEC3,
                                   TypeDesc::POINT),
                          TypePoint, Imath::V3f(0.0f), "(0, 0, 0)");
    test_type<Imath::V3f>("vector",
                          TypeDesc(TypeDesc::FLOAT, TypeDesc::VEC3,
                                   TypeDesc::VECTOR),
                          TypeVector, Imath::V3f(0.0f), "(0, 0, 0)");
    test_type<Imath::V3f>("normal",
                          TypeDesc(TypeDesc::FLOAT, TypeDesc::VEC3,
                                   TypeDesc::NORMAL),
                          TypeNormal, Imath::V3f(0.0f), "(0, 0, 0)");
    test_type<Imath::M33f>("matrix33",
                           TypeDesc(TypeDesc::FLOAT, TypeDesc::MATRIX33),
                           TypeMatrix33, {});
    test_type<Imath::M44f>("matrix",
                           TypeDesc(TypeDesc::FLOAT, TypeDesc::MATRIX44),
                           TypeMatrix44, {});
    test_type<Imath::V2f>("float2", TypeDesc(TypeDesc::FLOAT, TypeDesc::VEC2),
                          TypeFloat2, {});
    test_type<Imath::V2f>("vector2",
                          TypeDesc(TypeDesc::FLOAT, TypeDesc::VEC2,
                                   TypeDesc::VECTOR),
                          TypeVector2, {});
    test_type<Imath::V4f>("float4", TypeDesc(TypeDesc::FLOAT, TypeDesc::VEC4),
                          TypeFloat4, {});
    test_type<const char*>("string", TypeDesc(TypeDesc::STRING), TypeString,
                           "hello", "hello");
    test_type<ustringhash>("ustringhash", TypeDesc(TypeDesc::USTRINGHASH),
                           TypeUstringhash, ustringhash("hello"), "hello");
    int i2[2] = { 1, 2 };
    test_type<int[2]>("rational",
                      TypeDesc(TypeDesc::INT, TypeDesc::VEC2,
                               TypeDesc::RATIONAL),
                      TypeRational, i2, "1/2");
    test_type<Imath::Box2f>("box2",
                            TypeDesc(TypeDesc::FLOAT, TypeDesc::VEC2,
                                     TypeDesc::BOX, 2),
                            TypeBox2);
    test_type<Imath::Box3f>("box3",
                            TypeDesc(TypeDesc::FLOAT, TypeDesc::VEC3,
                                     TypeDesc::BOX, 2),
                            TypeBox3);
    test_type<Imath::Box2f>("box2f",  // synonym for box2
                            TypeDesc(TypeDesc::FLOAT, TypeDesc::VEC2,
                                     TypeDesc::BOX, 2),
                            TypeBox2);
    test_type<Imath::Box3f>("box3f",  // synonym for box3
                            TypeDesc(TypeDesc::FLOAT, TypeDesc::VEC3,
                                     TypeDesc::BOX, 2),
                            TypeBox3);
    test_type<Imath::Box2i>("box2i",
                            TypeDesc(TypeDesc::INT, TypeDesc::VEC2,
                                     TypeDesc::BOX, 2),
                            TypeBox2i);
    test_type<Imath::Box3i>("box3i",
                            TypeDesc(TypeDesc::INT, TypeDesc::VEC3,
                                     TypeDesc::BOX, 2),
                            TypeBox3i);
    Imf::TimeCode tc;
    test_type<Imf::TimeCode>("timecode",
                             TypeDesc(TypeDesc::UINT, TypeDesc::SCALAR,
                                      TypeDesc::TIMECODE, 2),
                             TypeTimeCode, tc);
    Imf::KeyCode kc;
    test_type<Imf::KeyCode>("keycode",
                            TypeDesc(TypeDesc::INT, TypeDesc::SCALAR,
                                     TypeDesc::KEYCODE, 7),
                            TypeKeyCode, kc);
    test_type<void*>("pointer", TypeDesc(TypeDesc::PTR), TypePointer,
                     (void*)(1), "0x1");

    return unit_test_failures;
}

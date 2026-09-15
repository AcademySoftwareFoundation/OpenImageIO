// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/tiffutils.h>
#include <OpenImageIO/unittest.h>

using namespace OIIO;

static void
check_exif()
{
    ImageSpec source;
    source.attribute("Make", "Canon");
    source.attribute("Canon:Quality", 3);
    source.attribute("Canon:MacroMode", 1);
    source.attribute("Canon:SerialNumber", 123456);
    source.attribute("Orientation", 6);
    source.attribute("ExposureTime", 0.125f);
    source.attribute("FNumber", 2.8f);
    source.attribute("Exif:ApertureValue", 2.4f);
    source.attribute("Exif:ExposureBiasValue", -0.5f);
    source.attribute("Exif:ExifVersion", "0231");
    source.attribute("Exif:ColorSpace", 1);
    int gps_version[] = { 2, 3, 0, 0 };
    source.attribute("GPS:VersionID", TypeDesc(TypeDesc::INT, 4), gps_version);
    for (auto order : { endian::little, endian::big }) {
        std::vector<char> bytes;
        encode_exif(source, bytes, order);
        for (bool prefix : { false, true }) {
            std::string blob(bytes.data(), bytes.size());
            if (prefix)
                blob.insert(0, "Exif\0\0", 6);
            // Exercise a bounded, unaligned input view as well.
            blob.insert(0, "!");
            string_view input(blob.data() + 1, blob.size() - 1);
            ImageSpec native, alternate;
            attribute("enable_openmeta", 0);
            OIIO_CHECK_ASSERT(decode_exif(input, native));
            attribute("enable_openmeta", 1);
            OIIO_CHECK_ASSERT(decode_exif(input, alternate));
            for (const auto& p : native.extra_attribs) {
                const auto* q = alternate.find_attribute(p.name());
                OIIO_CHECK_ASSERT(q != nullptr);
                if (q) {
                    OIIO_CHECK_EQUAL(p.type(), q->type());
                    OIIO_CHECK_EQUAL(p.get_string(0), q->get_string(0));
                }
            }
        }
    }
}

static void
check_xmp()
{
    const char* packet = R"(<x:xmpmeta xmlns:x="adobe:ns:meta/">
<rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">
<rdf:Description xmlns:xmp="http://ns.adobe.com/xap/1.0/"
 xmlns:xmpMM="http://ns.adobe.com/xap/1.0/mm/"
 xmlns:dc="http://purl.org/dc/elements/1.1/"
 xmlns:custom="https://example.org/test/"
 xmp:Rating="4" xmpMM:DocumentID="test:document" custom:Value="007">
<dc:subject><rdf:Bag><rdf:li>red</rdf:li><rdf:li>green</rdf:li>
<rdf:li>red</rdf:li></rdf:Bag></dc:subject>
</rdf:Description></rdf:RDF></x:xmpmeta>)";
    for (int enabled : { 0, 1 }) {
        attribute("enable_openmeta", enabled);
        ImageSpec spec;
        OIIO_CHECK_ASSERT(decode_xmp(packet, spec));
        OIIO_CHECK_EQUAL(spec.get_int_attribute("IPTC:Rating"), 4);
        OIIO_CHECK_EQUAL(spec.get_string_attribute("IPTC:DocumentID"),
                         "test:document");
        OIIO_CHECK_EQUAL(spec.get_string_attribute("Keywords"), "red; green");
#if OIIO_USE_OPENMETA
        if (enabled) {
            OIIO_CHECK_EQUAL(spec.get_string_attribute(
                                 "xmp:https://example.org/test/:Value"),
                             "007");
            // Invalid input must not commit partially decoded attributes.
            ImageSpec unchanged;
            unchanged.attribute("keep", "original");
            OIIO_CHECK_ASSERT(!decode_exif(string_view("bad"), unchanged));
            OIIO_CHECK_ASSERT(!decode_xmp(string_view("<broken"), unchanged));
            OIIO_CHECK_EQUAL(unchanged.extra_attribs.size(), 1);
            OIIO_CHECK_EQUAL(unchanged.get_string_attribute("keep"),
                             "original");
        }
#endif
    }
}

static void
check_openmeta_details()
{
#if OIIO_USE_OPENMETA
    attribute("enable_openmeta", 1);
    const char* packet
        = R"(<rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">
<rdf:Description xmlns:e="http://ns.adobe.com/exif/1.0/"
 xmlns:d="http://purl.org/dc/elements/1.1/" e:ApertureValue="99/10"
 e:ExposureTime="1/8" e:ExifVersion="0221" e:ColorSpace="1">
<d:title><rdf:Alt><rdf:li xml:lang="fr">Rouge</rdf:li>
<rdf:li xml:lang="x-default">Red &amp; green</rdf:li></rdf:Alt></d:title>
</rdf:Description></rdf:RDF>)";
    ImageSpec spec;
    spec.attribute("Exif:ApertureValue", 2.4f);
    uint16_t color_space = 1;
    spec.attribute("Exif:ColorSpace", TypeUInt16, &color_space);
    OIIO_CHECK_ASSERT(decode_xmp(packet, spec));
    OIIO_CHECK_EQUAL(spec.get_float_attribute("Exif:ApertureValue"), 2.4f);
    OIIO_CHECK_EQUAL(spec.get_float_attribute("ExposureTime"), 0.125f);
    OIIO_CHECK_EQUAL(spec.get_string_attribute("Exif:ExifVersion"), "0221");
    OIIO_CHECK_ASSERT(!spec.find_attribute(
        "xmp:http://ns.adobe.com/exif/1.0/:ColorSpace"));
    OIIO_CHECK_EQUAL(spec.get_string_attribute(
                         "xmp:http://ns.adobe.com/exif/1.0/:ApertureValue"),
                     "99/10");
    OIIO_CHECK_EQUAL(spec.get_string_attribute("IPTC:ObjectName"),
                     "Red & green");
    OIIO_CHECK_EQUAL(
        spec.get_string_attribute(
            "xmp:http://purl.org/dc/elements/1.1/:title[@xml:lang=fr]"),
        "Rouge");
    std::string huge
        = R"(<rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">
<rdf:Description xmlns:x="http://ns.adobe.com/xap/1.0/" x:Label=")";
    huge.append(1024 * 1024 + 1, 'a');
    huge += "\"/></rdf:RDF>";
    auto count = spec.extra_attribs.size();
    OIIO_CHECK_ASSERT(!decode_xmp(huge, spec));
    OIIO_CHECK_EQUAL(spec.extra_attribs.size(), count);
#endif
}

int
main(int, char**)
{
    attribute("enable_openmeta", 0);
    OIIO_CHECK_EQUAL(get_int_attribute("enable_openmeta"), 0);
    attribute("enable_openmeta", 1);
    OIIO_CHECK_EQUAL(get_int_attribute("enable_openmeta"), OIIO_USE_OPENMETA);
    check_exif();
    check_xmp();
    check_openmeta_details();
    attribute("enable_openmeta", 0);
    return unit_test_failures;
}

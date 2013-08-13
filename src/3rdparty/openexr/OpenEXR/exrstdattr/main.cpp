///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2012, Industrial Light & Magic, a division of Lucas
// Digital Ltd. LLC
// 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *       Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *       Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// *       Neither the name of Industrial Light & Magic nor the names of
// its contributors may be used to endorse or promote products derived
// from this software without specific prior written permission. 
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////


//-----------------------------------------------------------------------------
//
//	exrstdattr -- a program that can set the values of most
//	standard attributes in an OpenEXR image file's header.
//
//-----------------------------------------------------------------------------

#include <ImfPartType.h>
#include <ImfMultiPartInputFile.h>
#include <ImfMultiPartOutputFile.h>
#include <ImfStandardAttributes.h>
#include <ImfVecAttribute.h>
#include <ImfIntAttribute.h>
#include <ImfTiledInputPart.h>
#include <ImfTiledOutputPart.h>
#include <ImfInputPart.h>
#include <ImfOutputPart.h>
#include <ImfDeepScanLineInputPart.h>
#include <ImfDeepScanLineOutputPart.h>
#include <ImfDeepTiledInputPart.h>
#include <ImfDeepTiledOutputPart.h>

#include <map>
#include <string>
#include <iostream>
#include <exception>
#include <string.h>
#include <stdlib.h>
#include <vector>

#include <OpenEXRConfig.h>
namespace CustomImf = OPENEXR_IMF_NAMESPACE;
using namespace CustomImf;
using namespace std;
using namespace IMATH_NAMESPACE;


void
usageMessage (const char argv0[], bool verbose = false)
{
    cerr << "usage: " << argv0 << " [options] infile outfile" << endl;

    if (verbose)
    {
        cerr << "\n"
        "Reads an OpenEXR image from infile, sets the values of\n"
        "one or more standard attributes in the image's header,\n"
        "and saves the result in outfile.  Infile and outfile must\n"
        "not refer to the same file (the program cannot edit an\n"
        "image file \"in place\").\n"
        "\n"
        "Options for setting attribute values:\n"
        "\n"
        "  -chromaticities f f f f f f f f\n"
        "        CIE xy chromaticities for the red, green\n"
        "	 and blue primaries, and for the white point\n"
        "        (8 floats)\n"
        "\n"
        "  -whiteLuminance f\n"
        "        white luminance, in candelas per square meter\n"
        "        (float, >= 0.0)\n"
        "\n"
        "  -adoptedNeutral f f\n"
        "        CIE xy coordinates that should be considered\n"
        "        \"neutral\" during color rendering.  Pixels in\n"
        "        the image file whose xy coordinates match the\n"
        "        adoptedNeutral value should be mapped to neutral\n"
        "        values on the display. (2 floats)\n"
        "\n"
        "  -renderingTransform s\n"
        "        name of the CTL rendering transform for this\n"
        "        image (string)\n"
        "\n"
        "  -lookModTransform s\n"
        "        name of the CTL look modification transform for\n"
        "        this image (string)\n"
        "\n"
        "  -xDensity f\n"
        "        horizontal output density, in pixels per inch\n"
        "        (float, >= 0.0)\n"
        "\n"
        "  -owner s\n"
        "        name of the owner of the image (string)\n"
        "\n"
        "  -comments s\n"
        "        additional information about the image (string)\n"
        "\n"
        "  -capDate s\n"
        "        date when the image was created or\n"
        "        captured, in local time (string,\n"
        "        formatted as YYYY:MM:DD hh:mm:ss)\n"
        "\n"
        "  -utcOffset f\n"
        "        offset of local time at capDate from UTC, in\n"
        "        seconds (float, UTC == local time + x)\n"
        "\n"
        "  -longitude f\n"
        "  -latitude f\n"
        "  -altitude f\n"
        "        location where the image was recorded, in\n"
        "        degrees east of Greenwich and north of the\n"
        "        equator, and in meters above sea level\n"
        "        (float)\n"
        "\n"
        "  -focus f\n"
        "        the camera's focus distance, in meters\n"
        "        (float, > 0, or \"infinity\")\n"
        "\n"
        "  -expTime f\n"
        "        exposure time, in seconds (float, >= 0)\n"
        "\n"
        "  -aperture f\n"
        "        lens apterture, in f-stops (float, >= 0)\n"
        "\n"
        "  -isoSpeed f\n"
        "        effective speed of the film or image\n"
        "        sensor that was used to record the image\n"
        "        (float, >= 0)\n"
        "\n"
        "  -envmap s\n"
        "        indicates that the image is an environment map\n"
        "        (string, LATLONG or CUBE)\n"
        "\n"
        "  -framesPerSecond i i\n"
        "        playback frame rate expressed as a ratio of two\n"
        "        integers, n and d (the frame rate is n/d frames\n"
        "        per second)\n"
        "\n"
        "  -keyCode i i i i i i i\n"
        "        key code that uniquely identifies a motion\n"
        "        picture film frame using 7 integers:\n"
        "         * film manufacturer code (0 - 99)\n"
        "         * film type code (0 - 99)\n"
        "         * prefix to identify film roll (0 - 999999)\n"
        "         * count, increments once every perfsPerCount\n"
        "           perforations (0 - 9999)\n"
        "         * offset of frame, in perforations from\n"
        "           zero-frame reference mark (0 - 119)\n"
        "         * number of perforations per frame (1 - 15)\n"
        "         * number of perforations per count (20 - 120)\n"
        "\n"
        "  -timeCode i i\n"
        "        SMPTE time and control code, specified as a pair\n"
        "        of 8-digit base-16 integers.  The first number\n"
        "        contains the time address and flags (drop frame,\n"
        "        color frame, field/phase, bgf0, bgf1, bgf2).\n"
        "        The second number contains the user data and\n"
        "        control codes.\n"
        "\n"
        "  -wrapmodes s\n"
        "        if the image is used as a texture map, specifies\n"
        "        how the image should be extrapolated outside the\n"
        "        zero-to-one texture coordinate range\n"
        "        (string, e.g. \"clamp\" or \"periodic,clamp\")\n"
        "\n"
        "  -pixelAspectRatio f\n"
        "        width divided by height of a pixel\n"
        "        (float, >= 0)\n"
        "\n"
        "  -screenWindowWidth f\n"
        "        width of the screen window (float, >= 0)\n"
        "\n"
        "  -screenWindowCenter f f\n"
        "        center of the screen window (2 floats)\n"
        "\n"
        "  -string s s\n"
        "        custom string attribute\n"
        "        (2 strings, attribute name and value)\n"
        "\n"
        "  -float s f\n"
        "        custom float attribute (string + float,\n"
        "        attribute name and value)\n"
        "\n"
        "  -int s i\n"
        "        custom integer attribute (string + integer,\n"
        "        attribute name and value)\n"
        "\n"
        "Multipart Options:\n"
        "\n"
        "  -part i   part number\n"
        "\n"
        "Other Options:\n"
        "\n"
        "  -h        prints this message\n";

        cerr << endl;
    }

    exit (1);
}


typedef map <string, Attribute *> AttrMap;


void
isNonNegative (const char attrName[], float f)
{
    if (f < 0)
    {
        cerr << "The value for the " << attrName << " attribute "
                "must not be less than zero." << endl;
        exit (1);
    }
}


void
isPositive (const char attrName[], float f)
{
    if (f <= 0)
    {
        cerr << "The value for the " << attrName << " attribute "
                "must be greater than zero." << endl;
        exit (1);
    }
}


void
notValidDate (const char attrName[])
{
    cerr << "The value for the " << attrName << " attribute "
            "is not a valid date of the form \"YYYY:MM:DD yy:mm:ss\"." << endl;
    exit (1);
}


int
strToInt (const char str[], int length)
{
    int x = 0;

    for (int i = 0; i < length; ++i)
    {
        if (str[i] < '0' || str[i] > '9')
            return -1;

        x = x * 10 + (str[i] - '0');
    }

    return x;
}


void
isDate (const char attrName[], const char str[])
{
    //
    // Check that str represents a valid
    // date of the form YYYY:MM:DD hh:mm:ss.
    //

    if (strlen (str) != 19 ||
        str[4] != ':' || str[7] != ':' ||
        str[10] != ' ' ||
        str[13] != ':' || str[16] != ':')
    {
        notValidDate (attrName);
    }

    int Y = strToInt (str + 0, 4);	// year
    int M = strToInt (str + 5, 2);	// month
    int D = strToInt (str + 8, 2);      // day
    int h = strToInt (str + 11, 2);	// hour
    int m = strToInt (str + 14, 2);	// minute
    int s = strToInt (str + 17, 2);	// second

    if (Y < 0 ||
        M < 1 || M > 12 ||
        D < 1 ||
        h < 0 || h > 23 ||
        m < 0 || m > 59 ||
        s < 0 || s > 59)
    {
        notValidDate (attrName);
    }

    if (M == 2)
    {
        bool leapYear = (Y % 4 == 0) && (Y % 100 != 0 || Y % 400 == 0);

        if (D > (leapYear? 29: 28))
            notValidDate (attrName);
    }
    else if (M == 4 || M == 6 || M == 9 || M == 11)
    {
        if (D > 30)
            notValidDate (attrName);
    }
    else
    {
        if (D > 31)
            notValidDate (attrName);
    }
}


void
getFloat (const char attrName[],
          int argc,
          char **argv,
          int &i,
          AttrMap &attrs,
          void (*check) (const char attrName[], float f) = 0)
{
    if (i > argc - 2)
        usageMessage (argv[0]);

    float f = strtod (argv[i + 1], 0);

    if (check)
        check (attrName, f);

    attrs[attrName] = new FloatAttribute (f);
    i += 2;
}


void
getPosFloatOrInf (const char attrName[],
                  int argc,
                  char **argv,
                  int &i,
                  AttrMap &attrs)
{
    if (i > argc - 2)
        usageMessage (argv[0]);

    float f;

    if (!strcmp (argv[i + 1], "inf") || !strcmp (argv[i + 1], "infinity"))
    {
        f = float (half::posInf());
    }
    else
    {
        f = strtod (argv[i + 1], 0);

        if (f <= 0)
        {
            cerr << "The value for the " << attrName << " attribute "
                    "must be greater than zero, or \"infinity\"." << endl;
            exit (1);
        }
    }

    attrs[attrName] = new FloatAttribute (f);
    i += 2;
}


void
getV2f (const char attrName[],
        int argc,
        char **argv,
        int &i,
        AttrMap &attrs,
        void (*check) (const char attrName[], const V2f &v) = 0)
{
    if (i > argc - 3)
        usageMessage (argv[0]);

    V2f v (strtod (argv[i + 1], 0), strtod (argv[i + 2], 0));

    if (check)
        check (attrName, v);

    attrs[attrName] = new V2fAttribute (v);
    i += 3;
}


void
getRational (const char attrName[],
             int argc,
             char **argv,
             int &i,
             AttrMap &attrs,
             void (*check) (const char attrName[], const Rational &r) = 0)
{
    if (i > argc - 3)
        usageMessage (argv[0]);

    Rational r (strtol (argv[i + 1], 0, 0), strtol (argv[i + 2], 0, 0));

    if (check)
        check (attrName, r);

    attrs[attrName] = new RationalAttribute (r);
    i += 3;
}


void
getString (const char attrName[],
           int argc,
           char **argv,
           int &i,
           AttrMap &attrs,
           void (*check) (const char attrName[], const char str[]) = 0)
{
    if (i > argc - 2)
        usageMessage (argv[0]);

    const char *str = argv[i + 1];

    if (check)
        check (attrName, str);

    attrs[attrName] = new StringAttribute (str);
    i += 2;
}


void
getNameAndString (int argc,
                  char **argv,
                  int &i,
                  AttrMap &attrs)
{
    if (i > argc - 3)
        usageMessage (argv[0]);

    const char *attrName = argv[i + 1];
    const char *str = argv[i + 2];
    attrs[attrName] = new StringAttribute (str);
    i += 3;
}


void
getNameAndFloat (int argc,
                 char **argv,
                 int &i,
                 AttrMap &attrs)
{
    if (i > argc - 3)
        usageMessage (argv[0]);

    const char *attrName = argv[i + 1];
    float f = strtod (argv[i + 2], 0);
    attrs[attrName] = new FloatAttribute (f);
    i += 3;
}


void
getNameAndInt (int argc,
               char **argv,
               int &i,
               AttrMap &attrs)
{
    if (i > argc - 3)
        usageMessage (argv[0]);

    const char *attrName = argv[i + 1];
    int j = strtol (argv[i + 2], 0, 0);
    attrs[attrName] = new IntAttribute (j);
    i += 3;
}


void
getChromaticities (const char attrName[],
                   int argc,
                   char **argv,
                   int &i,
                   AttrMap &attrs)
{
    if (i > argc - 9)
        usageMessage (argv[0]);

    ChromaticitiesAttribute *a = new ChromaticitiesAttribute;
    attrs[attrName] = a;

    a->value().red.x   = strtod (argv[i + 1], 0);
    a->value().red.y   = strtod (argv[i + 2], 0);
    a->value().green.x = strtod (argv[i + 3], 0);
    a->value().green.y = strtod (argv[i + 4], 0);
    a->value().blue.x  = strtod (argv[i + 5], 0);
    a->value().blue.y  = strtod (argv[i + 6], 0);
    a->value().white.x = strtod (argv[i + 7], 0);
    a->value().white.y = strtod (argv[i + 8], 0);
    i += 9;
}


void
getEnvmap (const char attrName[],
           int argc,
           char **argv,
           int &i,
           AttrMap &attrs)
{
    if (i > argc - 2)
        usageMessage (argv[0]);

    char *str = argv[i + 1];
    Envmap type;

    if (!strcmp (str, "latlong") || !strcmp (str, "LATLONG"))
    {
        type = ENVMAP_LATLONG;
    }
    else if (!strcmp (str, "cube") || !strcmp (str, "CUBE"))
    {
        type = ENVMAP_CUBE;
    }
    else
    {
        cerr << "The value for the " << attrName << " attribute "
                "must be either LATLONG or CUBE." << endl;
        exit (1);
    }

    attrs[attrName] = new EnvmapAttribute (type);
    i += 2;
}


void
getKeyCode (const char attrName[],
            int argc,
            char **argv,
            int &i,
            AttrMap &attrs)
{
    if (i > argc - 8)
        usageMessage (argv[0]);

    KeyCodeAttribute *a = new KeyCodeAttribute;
    attrs[attrName] = a;

    a->value().setFilmMfcCode   (strtol (argv[i + 1], 0, 0));
    a->value().setFilmType      (strtol (argv[i + 2], 0, 0));
    a->value().setPrefix        (strtol (argv[i + 3], 0, 0));
    a->value().setCount         (strtol (argv[i + 4], 0, 0));
    a->value().setPerfOffset    (strtol (argv[i + 5], 0, 0));
    a->value().setPerfsPerFrame (strtol (argv[i + 6], 0, 0));
    a->value().setPerfsPerCount (strtol (argv[i + 7], 0, 0));
    i += 8;
}


void
getTimeCode (const char attrName[],
             int argc,
             char **argv,
             int &i,
             AttrMap &attrs)
{
    if (i > argc - 3)
        usageMessage (argv[0]);

    TimeCodeAttribute *a = new TimeCodeAttribute;
    attrs[attrName] = a;

    a->value().setTimeAndFlags (strtoul (argv[i + 1], 0, 16));
    a->value().setUserData     (strtoul (argv[i + 2], 0, 16));
    i += 3;
}

void
getPartNum (int argc,
            char **argv,
            int &i,
            int *j)
{
    if (i > argc - 2)
        usageMessage (argv[0]);

    *j = strtol (argv[i + 1], 0, 0);
    cout << "part number: "<< *j << endl;
    i += 2;
}

int
main(int argc, char **argv)
{
    //
    // Parse the command line.
    //

    if (argc < 2)
	usageMessage (argv[0], true);

    int exitStatus = 0;

    try
    {
        const char *inFileName = 0;
        const char *outFileName = 0;

        AttrMap attrs;

        int i = 1;
        int partnum = -1;     // part number to change attribute
        bool allparts = true; // flag to change attribute in all parts

        while (i < argc)
        {
            const char *attrName = argv[i] + 1;

            if (!strcmp (argv[i], "-chromaticities"))
            {
                getChromaticities (attrName, argc, argv, i, attrs);
            }
            else if (!strcmp (argv[i], "-whiteLuminance"))
            {
                getFloat (attrName, argc, argv, i, attrs);
            }
            else if (!strcmp (argv[i], "-adoptedNeutral"))
            {
                getV2f (attrName, argc, argv, i, attrs);
            }
            else if (!strcmp (argv[i], "-renderingTransform"))
            {
                getString (attrName, argc, argv, i, attrs);
            }
            else if (!strcmp (argv[i], "-lookModTransform"))
            {
                getString (attrName, argc, argv, i, attrs);
            }
            else if (!strcmp (argv[i], "-xDensity"))
            {
                getFloat (attrName, argc, argv, i, attrs, isPositive);
            }
            else if (!strcmp (argv[i], "-owner"))
            {
                getString (attrName, argc, argv, i, attrs);
            }
            else if (!strcmp (argv[i], "-comments"))
            {
                getString (attrName, argc, argv, i, attrs);
            }
            else if (!strcmp (argv[i], "-capDate"))
            {
                getString (attrName, argc, argv, i, attrs, isDate);
            }
            else if (!strcmp (argv[i], "-utcOffset"))
            {
                getFloat (attrName, argc, argv, i, attrs);
            }
            else if (!strcmp (argv[i], "-longitude"))
            {
                getFloat (attrName, argc, argv, i, attrs);
            }
            else if (!strcmp (argv[i], "-latitude"))
            {
                getFloat (attrName, argc, argv, i, attrs);
            }
            else if (!strcmp (argv[i], "-altitude"))
            {
                getFloat (attrName, argc, argv, i, attrs);
            }
            else if (!strcmp (argv[i], "-focus"))
            {
                getPosFloatOrInf (attrName, argc, argv, i, attrs);
            }
            else if (!strcmp (argv[i], "-expTime"))
            {
                getFloat (attrName, argc, argv, i, attrs, isPositive);
            }
            else if (!strcmp (argv[i], "-aperture"))
            {
                getFloat (attrName, argc, argv, i, attrs, isPositive);
            }
            else if (!strcmp (argv[i], "-isoSpeed"))
            {
                getFloat (attrName, argc, argv, i, attrs, isPositive);
            }
            else if (!strcmp (argv[i], "-envmap"))
            {
                getEnvmap (attrName, argc, argv, i, attrs);
            }
            else if (!strcmp (argv[i], "-framesPerSecond"))
            {
                getRational (attrName, argc, argv, i, attrs);
            }
            else if (!strcmp (argv[i], "-keyCode"))
            {
                getKeyCode (attrName, argc, argv, i, attrs);
            }
            else if (!strcmp (argv[i], "-timeCode"))
            {
                getTimeCode (attrName, argc, argv, i, attrs);
            }
            else if (!strcmp (argv[i], "-wrapmodes"))
            {
                getString (attrName, argc, argv, i, attrs);
            }
            else if (!strcmp (argv[i], "-pixelAspectRatio"))
            {
                getFloat (attrName, argc, argv, i, attrs, isPositive);
            }
            else if (!strcmp (argv[i], "-screenWindowWidth"))
            {
                getFloat (attrName, argc, argv, i, attrs, isNonNegative);
            }
            else if (!strcmp (argv[i], "-screenWindowCenter"))
            {
                getV2f (attrName, argc, argv, i, attrs);
            }
            else if (!strcmp (argv[i], "-string"))
            {
                getNameAndString (argc, argv, i, attrs);
            }
            else if (!strcmp (argv[i], "-float"))
            {
                getNameAndFloat (argc, argv, i, attrs);
            }
            else if (!strcmp (argv[i], "-int"))
            {
                getNameAndInt (argc, argv, i, attrs);
            }
            else if (!strcmp (argv[i], "-h"))
            {
                usageMessage (argv[0], true);
            }
            else if (!strcmp (argv[i], "-part"))
            {
                getPartNum (argc, argv, i, &partnum);
                allparts = false;
            }
            else
            {
                if (inFileName == 0)
                    inFileName = argv[i];
                else
                    outFileName = argv[i];

                i += 1;
            }
        }

        if (inFileName == 0 || outFileName == 0)
            usageMessage (argv[0]);

        if (!strcmp (inFileName, outFileName))
        {
            cerr << "Input and output cannot be the same file." << endl;
            return 1;
        }

	//
	// Load the input file, add the new attributes,
	// and save the result in the output file.
	//

        MultiPartInputFile input (inFileName);
        int parts = input.parts();

        if ( !allparts && (partnum < 0 || partnum >= parts) ){
            cerr << "ERROR: you asked for part " << partnum << " in " << inFileName;
            cerr << ", which only has " << parts << " parts\n";
            exit(1);
        }

	//
	// update headers
	//
        vector<Header> headers;
        for(int p = 0 ; p < parts; p++)
        {
            Header h = input.header (p);
            if (allparts)
            {
                for (AttrMap::const_iterator i = attrs.begin(); i != attrs.end(); ++i)
                    h.insert (i->first.c_str(), *i->second);
            }
            else
            {
                if (p == partnum)
                {
                    for (AttrMap::const_iterator i = attrs.begin(); i != attrs.end(); ++i)
                        h.insert (i->first.c_str(), *i->second);
                }
            }

            headers.push_back (h);
        }

        //
        // copy pixels of parts one by one
        //
        MultiPartOutputFile output (outFileName, &headers[0], parts);

        for (int p = 0 ; p < parts; p++)
        {
            Header header = input.header (p);
            std::string type = header.type();
            if (type == TILEDIMAGE)
            {
                TiledInputPart in (input, p);
                TiledOutputPart out (output, p);
                out.copyPixels (in);
            }
            else if (type == SCANLINEIMAGE)
            {
                InputPart in (input, p);
                OutputPart out (output, p);
                out.copyPixels (in);
            }
            else if (type == DEEPSCANLINE)
            {
                DeepScanLineInputPart in (input,p);
                DeepScanLineOutputPart out (output,p);
                out.copyPixels (in);
            }
            else if (type == DEEPTILE)
            {
                DeepTiledInputPart in (input,p);
                DeepTiledOutputPart out (output,p);
                out.copyPixels (in);
            }
        }
    }
    catch (const exception &e)
    {
        cerr << e.what() << endl;
        exitStatus = 1;
    }

    return exitStatus;
}

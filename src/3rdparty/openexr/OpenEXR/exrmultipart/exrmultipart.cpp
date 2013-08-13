///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2012, Industrial Light & Magic, a division of Lucas
// Digital Ltd. LLC
// Portions contributed and copyright held by others as indicated.
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
//	Utility program to combine or separate multipart image files
//
//-----------------------------------------------------------------------------


#include <ImfMultiPartOutputFile.h>
#include <ImfMultiPartInputFile.h>
#include <ImfStringAttribute.h>
#include <ImfChannelList.h>
#include <ImfTiledInputPart.h>
#include <ImfTiledOutputPart.h>
#include <ImfInputPart.h>
#include <ImfOutputPart.h>
#include <ImfDeepScanLineInputPart.h>
#include <ImfDeepScanLineOutputPart.h>
#include <ImfDeepTiledInputPart.h>
#include <ImfDeepTiledOutputPart.h>
#include <ImfPartHelper.h>
#include <ImfPartType.h>

#include <OpenEXRConfig.h>
#include <Iex.h>

#include <iostream>
#include <vector>
#include <stdlib.h>
#include <sstream>
#include <assert.h>

using std::cerr;
using std::cout;
using std::endl;
using std::vector;
using std::set;
using std::ostringstream;
using std::min;
using std::max;
using std::string;
using IMATH_NAMESPACE::Box2i;

using namespace OPENEXR_IMF_NAMESPACE;

void
copy_tile (MultiPartInputFile & input,
           MultiPartOutputFile & output,
           int inPart, int outPart)
{
    TiledInputPart in (input, inPart);
    TiledOutputPart out (output, outPart);

    out.copyPixels (in);
}

void
copy_tiledeep (MultiPartInputFile & input,
               MultiPartOutputFile & output,
               int inPart, int outPart)
{
    DeepTiledInputPart in (input, inPart);
    DeepTiledOutputPart out (output, outPart);

    out.copyPixels (in);
}

void
copy_scanline (MultiPartInputFile & input,
               MultiPartOutputFile & output,
               int inPart, int outPart)
{
    InputPart in (input, inPart);
    OutputPart out (output, outPart);

    out.copyPixels (in);
}

void
copy_scanlinedeep (MultiPartInputFile & input,
                   MultiPartOutputFile & output,
                   int inPart, int outPart)
{
    DeepScanLineInputPart in (input, inPart);
    DeepScanLineOutputPart out (output, outPart);

    out.copyPixels (in);
}

void
make_unique_names (vector<Header> & headers)
{
    set<string> names;
    for ( size_t i = 0 ; i < headers.size() ; i++ )
    {
        std::string base_name;
        // if no name at all, set it to <type><partnum> (first part is part 1)
        if (!headers[i].hasName())
        {
            ostringstream s;
            s << headers[i].type() << (i + 1);
            base_name = s.str();
            headers[i].setName (base_name);
        }
        else
        {
            base_name = headers[i].name();
        }
        // check name has already been used, if so add a _<number> to it
        if (names.find (base_name) == names.end())
        {
            ostringstream s;
            size_t backup=1;
            do
            {
                s.clear();
                s << headers[i].type() << i << "_" << backup;
                backup++;
            }
            while (names.find(s.str()) != names.end());
            headers[i].setName (s.str());
        }
    }
}

void
filename_check (vector <string> names, const char* aname)
{
    string bname(aname);
    for (size_t i = 0; i < names.size(); i++)
    {
        if (bname.compare (names[i]) == 0)
        {
            cerr << "\n" << "ERROR: "
            "input and output file names cannot be the same." << endl;
            exit (1);
        }
    }
}

void
convert(vector <const char*> in, vector<const char *> views,const char* outname, bool override)
{
    if(in.size()!=1)
    {
        cerr <<"\n" << "ERROR: " 
        "can only convert one file at once - use 'combine' mode for multiple files" << endl;
        exit(1);
    }
    try
    {
        MultiPartInputFile infile(in[0]);
        
        if(infile.parts()!=1)
        {
            cerr <<"\n" << "ERROR: " 
            "can only convert single part EXRs to multipart EXR-2.0 files: use 'split' mode instead" << endl;
            exit(1);
        }
        
        vector<MultiViewChannelName> input_channels;
        
        string hero;
        if(hasMultiView(infile.header(0)))
        {
            StringVector h = multiView(infile.header(0));
            if(h.size()>0)
            {
                hero=h[0];
            }
        }
        
        // retrieve channel names from input file in view-friendly format
        GetChannelsInMultiPartFile(infile,input_channels);
        
        
        vector< MultiViewChannelName > output_channels = input_channels;
        // remap channels to multiple output parts
        int parts = SplitChannels(output_channels.begin(),output_channels.end(),true,hero);
        
        vector<Header> output_headers(parts);
        vector<FrameBuffer> output_framebuffers(parts);
        FrameBuffer input_framebuffer;
        
        //
        // make all output headers the same as the input header but 
        // with no channels
        //
        for(int i=0;i<parts;i++)
        {
          output_headers[i]=infile.header(0);     
          if(hasMultiView(output_headers[i]))
          {
              output_headers[i].erase("multiView");
          }
          output_headers[i].channels()=ChannelList();
        }   
        
        make_unique_names(output_headers);
        
        const ChannelList & in_chanlist = infile.header(0).channels();

        int channel_count = 0;
        for(ChannelList::ConstIterator i=in_chanlist.begin();i!=in_chanlist.end();++i)
        {
            ++channel_count;
        }
        
        Box2i dataWindow = infile.header(0).dataWindow();
        int pixel_count = (dataWindow.size().y+1)*(dataWindow.size().x+1);
        int pixel_width = dataWindow.size().x+1;
        
        
        // offset in pixels between base of array and 0,0
        int pixel_base = dataWindow.min.y*pixel_width+dataWindow.min.x;

        vector< vector<char> > channelstore(channel_count);
        
        
        //
        // insert channels into correct header and framebuffers
        //
        for( size_t i=0 ; i<input_channels.size() ; i++ )
        {
            // read the part we should be writing channel into, insert into header
            int part = output_channels[i].part_number;
            ChannelList::ConstIterator chan = in_chanlist.find(input_channels[i].internal_name);
            output_headers[part].channels().insert(output_channels[i].name,chan.channel());
            
            if( output_channels[i].view!="" )
            {
                output_headers[part].setView( output_channels[i].view );
            }
            
            // compute size of channel
            size_t samplesize=sizeof(float);
            if(chan.channel().type==HALF)
            {
                samplesize=sizeof(half);
            }
            channelstore[i].resize(samplesize*pixel_count);
            
            output_framebuffers[part].insert(output_channels[i].name,
                                             Slice(chan.channel().type,&channelstore[i][0]-pixel_base*samplesize,
                                                   samplesize,pixel_width*samplesize));
            input_framebuffer.insert(input_channels[i].internal_name,
                                              Slice(chan.channel().type,&channelstore[i][0]-pixel_base*samplesize,
                                                    samplesize,pixel_width*samplesize));
                                                    
        }
        
        //
        // create output file
        //
        
        MultiPartOutputFile outfile(outname,&output_headers[0],output_headers.size());
        InputPart inpart(infile,0);
        
        
        //
        // read file
        //
        inpart.setFrameBuffer(input_framebuffer);
        inpart.readPixels(dataWindow.min.y,dataWindow.max.y);
        
        //
        // write each part
        //
        
        for(size_t i=0;i<output_framebuffers.size();i++)
        {
            OutputPart outpart(outfile,i);
            outpart.setFrameBuffer(output_framebuffers[i]);
            outpart.writePixels(dataWindow.max.y+1-dataWindow.min.y);
        }
        
        
    }
    catch (IEX_NAMESPACE::BaseExc &e)
    {
        cerr << "\n" << "ERROR:" << endl;
        cerr << e.what() << endl;
        exit (1);
    }
    
}

void
combine (vector <const char*> in, vector<const char *> views,const char* outname, bool override)
{
    size_t numInputs = in.size();
    int numparts;
    vector<int> partnums;
    vector<MultiPartInputFile *> inputs;
    vector<MultiPartInputFile *> fordelete;
    MultiPartInputFile *infile;
    vector<Header> headers;
    vector<string> fornamecheck;

    //
    // parse all inputs
    //
    for (size_t i = 0 ; i < numInputs; i++)
    {
        // if input is <file>:<partnum> then extract part number,
        // else get all parts
        string filename (in[i]);
        size_t colon = filename.rfind (':');

        if (colon == string::npos)
        {
            fornamecheck.push_back (filename);

            try
            {
                infile = new MultiPartInputFile (filename.c_str());
                fordelete.push_back (infile);
                numparts = infile->parts();

                //copy header from all parts of input to our header array
                for (int j = 0; j < numparts; j++)
                {
                    inputs.push_back (infile);
                    headers.push_back (infile->header(j));
                    if( views[i] != NULL )
                    {
                        headers[headers.size()-1].setView( views[i] );
                    }
                    partnums.push_back (j);
                }
            }
            catch (IEX_NAMESPACE::BaseExc &e)
            {
                cerr << "\n" << "ERROR:" << endl;
                cerr << e.what() << endl;
                exit (1);
            }
        }
        else
        {
            string num = filename.substr (colon + 1);
            numparts = atoi (num.c_str());
            filename = filename.substr (0, colon);

            fornamecheck.push_back (filename);

            try
            {
                infile = new MultiPartInputFile (filename.c_str());
                fordelete.push_back (infile);

                if (numparts >= infile->parts())
                {
                    cerr << "ERROR: you asked for part " << numparts << " in " << in[i];
                    cerr << ", which only has " << infile->parts() << " parts\n";
                    exit (1);
                }
                //copy header from required part of input to our header array
                inputs.push_back (infile);
                headers.push_back (infile->header(numparts));
                if( views[i] != NULL )
                {
                     headers[headers.size()-1].setView( views[i] );
                }                
                partnums.push_back (numparts);
            }
            catch (IEX_NAMESPACE::BaseExc &e)
            {
                cerr << "\n" << "ERROR:" << endl;
                cerr << e.what()<< endl;
                exit (1);
            }
        }
    }

    filename_check (fornamecheck, outname);
    //
    // sort out names - make unique
    //
    if (numInputs>1)
    {
        make_unique_names (headers);
    }

    //
    // do combine
    //
    try
    {
        MultiPartOutputFile temp (outname, &headers[0],
                                  headers.size(), override);
    }
    catch (IEX_NAMESPACE::BaseExc &e)
    {
        cerr << "\n" << "ERROR: " << e.what() << endl;
        exit (1);
    }

    MultiPartOutputFile out (outname, &headers[0], headers.size(), override);

    for (size_t p = 0 ; p < partnums.size();p++)
    {
        Header header = headers[p];
        std::string type = header.type();
        if (type == SCANLINEIMAGE)
        {
            cout << "part " << p << ": "<< "scanlineimage" << endl;
            copy_scanline (*inputs[p], out, partnums[p], p);
        }
        else if (type == TILEDIMAGE)
        {
            cout << "part " << p << ": "<< "tiledimage" << endl;
            copy_tile (*inputs[p], out, partnums[p], p);
        }
        else if (type == DEEPSCANLINE)
        {
            cout << "part " << p << ": "<< "deepscanlineimage" << endl;
            copy_scanlinedeep (*inputs[p], out, partnums[p], p);
        }
        else if (type == DEEPTILE)
        {
            cout << "part " << p << ": "<< "deeptile" << endl;
            copy_tiledeep (*inputs[p], out, partnums[p], p);
        }
    }


    for (size_t k = 0; k < fordelete.size(); k++) {
        delete fordelete[k];
    }

    inputs.clear();
    fordelete.size();

    cout << "\n" << "Combine Success" << endl;
}

void
separate (vector <const char*> in, const char* out, bool override)
{
    if (in.size() > 1)
    {
        cerr << "ERROR: -separate only take one input file\n"
        "syntax: exrmultipart -separate -i infile.exr -o outfileBaseName\n";
        exit (1);
    }

    //
    // parse the multipart input
    //
    string filename (in[0]);
    MultiPartInputFile *inputimage;
    int numOutputs;
    vector<string> fornamecheck;

    // add check for existance of the file
    try
    {
        MultiPartInputFile temp (filename.c_str());
    }
    catch (IEX_NAMESPACE::BaseExc &e)
    {
        cerr << "\n" << "ERROR: " << e.what() << endl;
        exit (1);
    }

    inputimage = new MultiPartInputFile (filename.c_str());
    numOutputs = inputimage->parts();
    cout << "numOutputs: " << numOutputs << endl;

    //
    // set outputs names
    //
    for (int p = 0 ; p <numOutputs;p++)
    {
        string outfilename (out);

        //add number to outfilename
        std::ostringstream oss;
        oss << '.' << p + 1;
        outfilename += oss.str();
        outfilename += ".exr";
        cout << "outputfilename: " << outfilename << endl;
        fornamecheck.push_back (outfilename);
    }

    filename_check (fornamecheck, in[0]);

    //
    // separate outputs
    //
    for (int p = 0 ; p < numOutputs; p++)
    {
        Header header = inputimage->header (p);

        MultiPartOutputFile out (fornamecheck[p].c_str(), &header, 1, override);

        std::string type = header.type();
        if (type == "scanlineimage")
        {
            cout << "scanlineimage" << endl;
            copy_scanline (*inputimage, out, p, 0);
        }
        else if (type == "tiledimage")
        {
            cout << "tiledimage" << endl;
            copy_tile (*inputimage, out, p, 0);
        }
        else if (type == "deepscanline")
        {
            cout << "deepscanline" << endl;
            copy_scanlinedeep (*inputimage, out, p, 0);
        }
        else if (type == "deeptile")
        {
            cout << "deeptile" << endl;
            copy_tiledeep (*inputimage, out, p, 0);
        }
    }

    delete inputimage;
    cout << "\n" << "Seperate Success" << endl;
}

void
usageMessage (const char argv[])
{
    cerr << argv << " handles the combining and splitting of multipart data\n";
    cerr << "\n" << "Usage: "
            "exrmultipart -combine -i input.exr[:partnum] "
            "[input2.exr[:partnum]] [...] -o outfile.exr [options]\n";
    cerr << "   or: exrmultipart -separate -i infile.exr -o outfileBaseName "
            "[options]\n";
    cerr << "   or: exrmultipart -convert -i infile.exr -o outfile.exr "
            "[options]\n";
            cerr << "\n" << "Options:\n";
    cerr << "-override [0/1]      0-do not override conflicting shared "
            "attributes [default]\n"
            "                     1-override conflicting shared attributes\n";

    cerr << "-view name           (after specifying -i) "
            "assign following inputs to view 'name'\n";
    exit (1);
}

int
main (int argc, char * argv[])
{
    if (argc < 6)
    {
        usageMessage (argv[0]);
    }

    vector <const char*> inFiles;
    vector <const char*> views;
    const char* view = 0;
    const char *outFile = 0;
    bool override = false;

    int i = 1;
    int mode = 0; // 0-do not read input, 1-infiles, 2-outfile, 3-override, 4-view

    while (i < argc)
    {
        if (!strcmp (argv[i], "-h"))
        {
            usageMessage (argv[0]);
        }

        if (!strcmp (argv[i], "-i"))
        {
            mode = 1;
        }
        else if (!strcmp (argv[i], "-o"))
        {
            mode = 2;
        }
        else if (!strcmp (argv[i], "-override"))
        {
            mode = 3;
        }
        else if (!strcmp (argv[i], "-view"))
        {
            if(mode !=1 )
            {
                usageMessage (argv[0]);
                return 1;
            }
            mode = 4;
        }
        else
        {
            switch (mode)
            {
            case 1:
                inFiles.push_back (argv[i]);
                views.push_back (view);
                break;
            case 2: outFile = argv[i];
                break;
            case 3: override = atoi (argv[i]);
                break;
            case 4: view = argv[i];
                 mode=1;
                break;
            }
        }
        i++;
    }

    // check input and output files found or not
    if (inFiles.size() == 0)
    {
        cerr << "\n" << "ERROR: found no input files" << endl;
        exit (1);
    }

    cout << "input:" << endl;
    for (size_t i = 0; i < inFiles.size(); i++)
    {
        cout << "      " << inFiles[i];
        if(views[i]) cout << " in view " << views[i];
        cout << endl;
    }

    if (!outFile)
    {
        cerr << "\n"<<"ERROR: found no output file" << endl;
        exit (1);
    }

    cout << "output:\n      " << outFile << endl;
    cout << "override:" << override << "\n" << endl;


    if (!strcmp (argv[1], "-combine"))
    {
        cout << "-combine multipart input " << endl;
        combine (inFiles, views, outFile, override);
    }
    else if (!strcmp(argv[1], "-separate"))
    {
        cout << "-separate multipart input " << endl;
        separate (inFiles, outFile, override);
    }
    else if(!strcmp(argv[1],"-convert"))
    {
        cout << "-convert input to EXR2 multipart" << endl;
        convert (inFiles, views, outFile, override);
    }
    else
    {
        usageMessage (argv[0]);
    }

    return 0;
}


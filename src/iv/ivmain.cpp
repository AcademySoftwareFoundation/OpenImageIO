/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2008 Larry Gritz
// 
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
// 
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 
// (this is the MIT license)
/////////////////////////////////////////////////////////////////////////////


#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <iostream>
#include <iterator>

#include <QtGui>

#include <boost/foreach.hpp>

#include "imageio.h"
using namespace OpenImageIO;
#include "imageviewer.h"
#include "timer.h"
#include "argparse.h"



static bool verbose = false;
static bool foreground_mode = false;
static std::vector<std::string> filenames;



static int
parse_files (int argc, const char *argv[])
{
    for (int i = 0;  i < argc;  i++)
        filenames.push_back (argv[i]);
    return 0;
}



static void
getargs (int argc, char *argv[])
{
    bool help = false;
    ArgParse ap (argc, (const char **)argv);
    if (ap.parse ("Usage:  iv [options] [filename...]",
                  "%*", parse_files, "",
                  "--help", &help, "Print help message",
                  "-v", &verbose, "Verbose status messages",
                  "-F", &foreground_mode, "Foreground mode",
                  NULL) < 0) {
        std::cerr << ap.error_message() << std::endl;
        ap.usage ();
        exit (EXIT_FAILURE);
    }
    if (help) {
        ap.usage ();
        exit (EXIT_FAILURE);
    }
}



/// Try to put the process into the background so it doesn't continue to
/// tie up any shell that it was launched from.  Return true if successful,
/// false if it was unable to do so.
static bool
put_in_background (int argc, char *argv[])
{
    // You would think that this would be sufficient:
    //   pid_t pid = fork ();
    //   if (pid < 0)       // Some kind of error, we were unable to background
    //      return false;
    //   if (pid == 0)
    //       return true;   // This is the child process, so continue with life
    //   // Otherwise, this is the parent process, so terminate
    //   exit (0); 
    // But it's not.  On OS X, it's not safe to fork() if your app is linked
    // against certain libraries or frameworks.  So the only thing that I
    // think is safe is to exec a new process.
    // Another solution is this:
    //    daemon (1, 1);
    // But it suffers from the same problem on OS X, and seems to just be
    // a wrapper for fork.

#ifdef LINUX
    // Simplest case:
    daemon (1, 1);
    return true;
#endif


#ifdef MACOSX
#if 0
    // Another solution -- But I can't seem to make this work!
    std::vector<char *> newargs;
    newargs.push_back ("-F");
    for (int i = 0;  i < argc;  ++i)
        newargs.push_back (argv[i]);
    newargs.push_back (NULL);
    if (fork())
        exit (0);
    execv ("/Users/lg/lg/proj/oiio/dist/macosx/bin/iv" /*argv[0]*/, &newargs[0]);
    exit (0);
#endif

#if 1
    // This one works -- just call system(), but we have to properly
    // quote all the arguments in case filenames have spaces in them.
    std::string newcmd = std::string(argv[0]) + " -F";
    for (int i = 1;  i < argc;  ++i) {
        newcmd += " \"";
        newcmd += argv[i];
        newcmd += "\"";
    }
    newcmd += " &";
    if (system (newcmd.c_str()) != -1)
        exit (0);
    return true;
#endif


#endif

#ifdef WIN32
    // FIXME: How to do this for win32?  Somebody told me that it's not
    // necessary at all, and we just have to rename 'main' to 'winMain'
    // for it to become a backgrounded app.
#endif
}



int
main (int argc, char *argv[])
{
    getargs (argc, argv);

    if (! foreground_mode)
        put_in_background (argc, argv);

    // LG
//    Q_INIT_RESOURCE(iv);
    QApplication app(argc, argv);
    ImageViewer *mainWin = new ImageViewer;
    mainWin->show();

    BOOST_FOREACH (const std::string &s, filenames) {
        mainWin->add_image (s);
    }

    mainWin->current_image (0);

    int r = app.exec();
    // OK to clean up here
    return r;
}

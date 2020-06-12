#!/usr/bin/env python

from __future__ import print_function
from __future__ import absolute_import
import os
import glob
import sys
import platform
import subprocess
import difflib
import filecmp
import shutil

from optparse import OptionParser


#
# Get standard testsuite test arguments: srcdir exepath
#

srcdir = "."
tmpdir = "."
path = "../.."

# Options for the command line
parser = OptionParser()
parser.add_option("-p", "--path", help="add to executable path",
                  action="store", type="string", dest="path", default="")
parser.add_option("--devenv-config", help="use a MS Visual Studio configuration",
                  action="store", type="string", dest="devenv_config", default="")
parser.add_option("--solution-path", help="MS Visual Studio solution path",
                  action="store", type="string", dest="solution_path", default="")
(options, args) = parser.parse_args()

if args and len(args) > 0 :
    srcdir = args[0]
    srcdir = os.path.abspath (srcdir) + "/"
    os.chdir (srcdir)
if args and len(args) > 1 :
    path = args[1]
path = os.path.normpath (path)

tmpdir = "."
tmpdir = os.path.abspath (tmpdir)
redirect = " >> out.txt "

def oiio_relpath (path, start=os.curdir):
    "Wrapper around os.path.relpath which always uses '/' as the separator."
    p = os.path.relpath (path, start)
    return p if sys.platform != "win32" else p.replace ('\\', '/')

# Try to figure out where some key things are. Go by env variables set by
# the cmake tests, but if those aren't set, assume somebody is running
# this script by hand from inside build/PLATFORM/testsuite/TEST and that
# the rest of the tree has the standard layout.
OIIO_TESTSUITE_ROOT = oiio_relpath(os.getenv('OIIO_TESTSUITE_ROOT',
                                             '../../../../testsuite'))
OIIO_TESTSUITE_IMAGEDIR = os.getenv('OIIO_TESTSUITE_IMAGEDIR',
                                    '../../../../../oiio-images')
if OIIO_TESTSUITE_IMAGEDIR:
    OIIO_TESTSUITE_IMAGEDIR = oiio_relpath(OIIO_TESTSUITE_IMAGEDIR)
    # Set it back so test's can use it (python-imagebufalgo)
    os.putenv('OIIO_TESTSUITE_IMAGEDIR', OIIO_TESTSUITE_IMAGEDIR)
refdir = "ref/"
refdirlist = [ refdir ]
mytest = os.path.split(os.path.abspath(os.getcwd()))[-1]
if str(mytest).endswith('.batch') :
    mytest = mytest.split('.')[0]
test_source_dir = os.getenv('OIIO_TESTSUITE_SRC',
                            os.path.join(OIIO_TESTSUITE_ROOT, mytest))
colorconfig_file = os.path.join(OIIO_TESTSUITE_ROOT,
                                "common", "OpenColorIO", "nuke-default", "config.ocio")

# Swap the relative diff lines if the test suite is not being run via Makefile
if OIIO_TESTSUITE_ROOT != "../../../../testsuite":
    def replace_relative(lines):
        imgdir = None
        if OIIO_TESTSUITE_IMAGEDIR:
            imgdir = os.path.basename(OIIO_TESTSUITE_IMAGEDIR)
            if imgdir != "oiio-images":
                oiioimgs = os.path.basename(os.path.dirname(OIIO_TESTSUITE_IMAGEDIR))
                if oiioimgs == "oiio-images":
                    imgdir = "oiio-images/" + imgdir
                imgdir = "../../../../../" + imgdir

        for i in range(len(lines)):
            lines[i] = lines[i].replace("../../../../testsuite", OIIO_TESTSUITE_ROOT)
            if imgdir:
                lines[i] = lines[i].replace(imgdir, OIIO_TESTSUITE_IMAGEDIR)
        return lines
else:
    replace_relative = None


command = ""
outputs = [ "out.txt" ]    # default
failureok = 0
failthresh = 0.004
hardfail = 0.012
failpercent = 0.02
anymatch = False
cleanup_on_success = False
if int(os.getenv('TESTSUITE_CLEANUP_ON_SUCCESS', '0')) :
    cleanup_on_success = True;

image_extensions = [ ".tif", ".tx", ".exr", ".jpg", ".png", ".rla",
                     ".dpx", ".iff", ".psd", ".bmp", ".fits", ".ico",
                     ".jp2", ".sgi", ".tga", ".TGA" ]

# print ("srcdir = " + srcdir)
# print ("tmpdir = " + tmpdir)
# print ("path = " + path)
# print ("refdir = " + refdir)
# print ("test source dir = " + test_source_dir)

if platform.system() == 'Windows' :
    if not os.path.exists("./ref") :
        shutil.copytree (os.path.join (test_source_dir, "ref"), "./ref")
    if os.path.exists (os.path.join (test_source_dir, "src")) and not os.path.exists("./src") :
        shutil.copytree (os.path.join (test_source_dir, "src"), "./src")
    # if not os.path.exists("../data") :
    #     shutil.copytree ("../../../testsuite/data", "..")
    # if not os.path.exists("../common") :
    #     shutil.copytree ("../../../testsuite/common", "..")
else :
    def newsymlink(src, dst):
        print("newsymlink", src, dst)
        # os.path.exists returns False for broken symlinks, so remove if thats the case
        if os.path.islink(dst):
            os.remove(dst)
        os.symlink (src, dst)
    if not os.path.exists("./ref") :
        newsymlink (os.path.join (test_source_dir, "ref"), "./ref")
    if os.path.exists (os.path.join (test_source_dir, "src")) and not os.path.exists("./src") :
        newsymlink (os.path.join (test_source_dir, "src"), "./src")
    if not os.path.exists("./data") :
        newsymlink (test_source_dir, "./data")


# Disable this test on Travis when using leak sanitizer, because the error
# condition makes a leak we can't stop, but that's ok.
import os
if (os.getenv("TRAVIS") and (os.getenv("SANITIZE") in ["leak","address"])
    and os.path.exists(os.path.join (test_source_dir,"TRAVIS_SKIP_LSAN"))) :
    sys.exit (0)

pythonbin = 'python'
if os.getenv("PYTHON_VERSION") :
    pythonbin += os.getenv("PYTHON_VERSION")
#print ("pythonbin = ", pythonbin)


###########################################################################

# Handy functions...

# Compare two text files. Returns 0 if they are equal otherwise returns
# a non-zero value and writes the differences to "diff_file".
# Based on the command-line interface to difflib example from the Python
# documentation
def text_diff (fromfile, tofile, diff_file=None):
    import time
    try:
        fromdate = time.ctime (os.stat (fromfile).st_mtime)
        todate = time.ctime (os.stat (tofile).st_mtime)
        fromlines = open (fromfile, 'r').readlines()
        tolines   = open (tofile, 'r').readlines()
        if replace_relative:
            tolines = replace_relative(tolines)
    except:
        print ("Unexpected error:", sys.exc_info()[0])
        return -1
        
    diff = difflib.unified_diff(fromlines, tolines, fromfile, tofile,
                                fromdate, todate)
    # Diff is a generator, but since we need a way to tell if it is
    # empty we just store all the text in advance
    diff_lines = [l for l in diff]
    if not diff_lines:
        return 0
    if diff_file:
        try:
            open (diff_file, 'w').writelines (diff_lines)

            print ("Diff " + fromfile + " vs " + tofile + " was:\n-------")
#            print (diff)
            print ("".join(diff_lines))
        except:
            print ("Unexpected error:", sys.exc_info()[0])
    return 1


def oiio_app (app):
    if (platform.system () != 'Windows' or options.devenv_config == ""):
        return os.path.join (path, "bin", app) + " "
    else:
        return os.path.join (path, "bin", app) + " "
        # Old... not true any more?
        # When we use Visual Studio, built applications are stored
        # in the app/$(OutDir)/ directory, e.g., Release or Debug.
        # return os.path.join (path, "src", app, options.devenv_config, app) + " "


# Construct a command that will print info for an image, appending output to
# the file "out.txt".  If 'safematch' is nonzero, it will exclude printing
# of fields that tend to change from run to run or release to release.
def info_command (file, extraargs="", safematch=False, hash=True,
                  verbose=True) :
    args = "--info"
    if verbose :
        args += " -v -a"
    if safematch :
        args += " --no-metamatch \"DateTime|Software|OriginatingProgram|ImageHistory\""
    if hash :
        args += " --hash"
    return (oiio_app("oiiotool") + args + " " + extraargs
            + " " + oiio_relpath(file,tmpdir) + redirect + ";\n")


# Construct a command that will compare two images, appending output to
# the file "out.txt".  We allow a small number of pixels to have up to
# 1 LSB (8 bit) error, it's very hard to make different platforms and
# compilers always match to every last floating point bit.
def diff_command (fileA, fileB, extraargs="", silent=False, concat=True) :
    command = (oiio_app("idiff") + "-a"
               + " -fail " + str(failthresh)
               + " -failpercent " + str(failpercent)
               + " -hardfail " + str(hardfail)
               + " -warn " + str(2*failthresh)
               + " -warnpercent " + str(failpercent)
               + " " + extraargs + " " + oiio_relpath(fileA,tmpdir) 
               + " " + oiio_relpath(fileB,tmpdir))
    if not silent :
        command += redirect
    if concat:
        command += " ;\n"
    return command


# Construct a command that will create a texture, appending console
# output to the file "out.txt".
def maketx_command (infile, outfile, extraargs="",
                    showinfo=False, showinfo_extra="",
                    silent=False, concat=True) :
    command = (oiio_app("maketx") 
               + " " + oiio_relpath(infile,tmpdir) 
               + " " + extraargs
               + " -o " + oiio_relpath(outfile,tmpdir) )
    if not silent :
        command += redirect
    if concat:
        command += " ;\n"
    if showinfo:
        command += info_command (outfile, extraargs=showinfo_extra, safematch=1)
    return command



# Construct a command that will test the basic ability to read and write
# an image, appending output to the file "out.txt".  First, iinfo the
# file, including a hash (VERY unlikely not to match if we've read
# correctly).  If testwrite is nonzero, also iconvert the file to make a
# copy (tests writing that format), and then idiff to make sure it
# matches the original.
def rw_command (dir, filename, testwrite=True, use_oiiotool=False, extraargs="",
                preargs="", idiffextraargs="", output_filename="",
                safematch=False, printinfo=True) :
    fn = oiio_relpath (dir + "/" + filename, tmpdir)
    if printinfo :
        cmd = info_command (fn, safematch=safematch)
    else :
        cmd = ""
    if output_filename == "" :
        output_filename = filename
    if testwrite :
        if use_oiiotool :
            cmd = (cmd + oiio_app("oiiotool") + preargs + " " + fn
                   + " " + extraargs + " -o " + output_filename + redirect + ";\n")
        else :
            cmd = (cmd + oiio_app("iconvert") + preargs + " " + fn
                   + " " + extraargs + " " + output_filename + redirect + ";\n")
        cmd = (cmd + oiio_app("idiff") + " -a " + fn
               + " -fail " + str(failthresh)
               + " -failpercent " + str(failpercent)
               + " -hardfail " + str(hardfail)
               + " -warn " + str(2*failthresh)
               + " " + idiffextraargs + " " + output_filename + redirect + ";\n")
    return cmd


# Construct a command that will testtex
def testtex_command (file, extraargs="") :
    cmd = (oiio_app("testtex") + " " + file + " " + extraargs + " " +
           redirect + ";\n")
    return cmd


# Construct a command that will run oiiotool and append its output to out.txt
def oiiotool (args, silent=False, concat=True) :
    cmd = (oiio_app("oiiotool") + " "
           + "-colorconfig " + colorconfig_file + " "
           + args)
    if not silent :
        cmd += redirect
    if concat:
        cmd += " ;\n"
    return cmd



# Check one output file against reference images in a list of reference
# directories. For each directory, it will first check for a match under
# the identical name, and if that fails, it will look for alternatives of
# the form "basename-*.ext" (or ANY match in the ref directory, if anymatch
# is True).
def checkref (name, refdirlist) :
    # Break the output into prefix+extension
    (prefix, extension) = os.path.splitext(name)
    ok = 0
    for ref in refdirlist :
        # We will first compare name to ref/name, and if that fails, we will
        # compare it to everything else that matches ref/prefix-*.extension.
        # That allows us to have multiple matching variants for different
        # platforms, etc.
        defaulttest = os.path.join(ref,name)
        if anymatch :
            pattern = "*.*"
        else :
            pattern = prefix+"-*"+extension+"*"
        for testfile in ([defaulttest] + glob.glob (os.path.join (ref, pattern))) :
            if not os.path.exists(testfile) :
                continue
            # print ("comparing " + name + " to " + testfile)
            if extension in image_extensions :
                # images -- use idiff
                cmpcommand = diff_command (name, testfile, concat=False, silent=True)
                cmpresult = os.system (cmpcommand)
            elif extension == ".txt" :
                cmpresult = text_diff (name, testfile, name + ".diff")
            else :
                # anything else
                cmpresult = 0
                if os.path.exists(testfile) and filecmp.cmp (name, testfile) :
                    cmpresult = 0
                else :
                    cmpresult = 1
            if cmpresult == 0 :
                return (True, testfile)   # we're done
    return (False, defaulttest)



# Run 'command'.  For each file in 'outputs', compare it to the copy
# in 'ref/'.  If all outputs match their reference copies, return 0
# to pass.  If any outputs do not match their references return 1 to
# fail.
def runtest (command, outputs, failureok=0) :
    err = 0
#    print ("working dir = " + tmpdir)
    os.chdir (srcdir)
    open ("out.txt", "w").close()    # truncate out.txt
    open ("out.err.txt", "w").close()    # truncate out.txt
    if os.path.isfile("debug.log") :
        os.remove ("debug.log")

    if options.path != "" :
        sys.path = [options.path] + sys.path
    print ("command = " + command)

    test_environ = None
    if (platform.system () == 'Windows') and (options.solution_path != "") and \
       (os.path.isdir (options.solution_path)):
        test_environ = os.environ
        libOIIO_args = [options.solution_path, "libOpenImageIO"]
        if options.devenv_config != "":
            libOIIO_args.append (options.devenv_config)
        libOIIO_path = os.path.normpath (os.path.join (*libOIIO_args))
        test_environ["PATH"] = libOIIO_path + ';' + test_environ["PATH"]

    for sub_command in [c.strip() for c in command.split(';') if c.strip()]:
        cmdret = subprocess.call (sub_command, shell=True, env=test_environ)
        if cmdret != 0 and failureok == 0 :
            print ("#### Error: this command failed: ", sub_command)
            print ("FAIL")
            err = 1

    for out in outputs :
        (prefix, extension) = os.path.splitext(out)
        (ok, testfile) = checkref (out, refdirlist)

        if ok :
            if extension in image_extensions :
                # If we got a match for an image, save the idiff results
                os.system (diff_command (out, testfile, silent=False))
            print ("PASS: " + out + " matches " + testfile)
        else :
            err = 1
            print ("NO MATCH for " + out)
            print ("FAIL " + out)
            if extension == ".txt" :
                # If we failed to get a match for a text file, print the
                # file and the diff, for easy debugging.
                print ("-----" + out + "----->")
                print (open(out,'r').read() + "<----------")
                print ("-----" + testfile + "----->")
                print (open(testfile,'r').read() + "<----------")
                os.system ("ls -al " +out+" "+testfile)
                print ("Diff was:\n-------")
                print (open (out+".diff", 'r').read())
            if extension in image_extensions :
                # If we failed to get a match for an image, send the idiff
                # results to the console
                os.system (diff_command (out, testfile, silent=False))
            if os.path.isfile("debug.log") and os.path.getsize("debug.log") :
                print ("---   DEBUG LOG   ---\n")
                #flog = open("debug.log", "r")
                # print (flog.read())
                with open("debug.log", "r") as flog :
                    print (flog.read())
                print ("--- END DEBUG LOG ---\n")
    return (err)


##########################################################################



#
# Read the individual run.py file for this test, which will define 
# command and outputs.
#
with open(os.path.join(test_source_dir,"run.py")) as f:
    code = compile(f.read(), "run.py", 'exec')
    exec (code)

# Allow a little more slop for slight pixel differences when in DEBUG
# mode or when running on remote Travis-CI or Appveyor machines.
if (os.getenv('TRAVIS') or os.getenv('APPVEYOR') or os.getenv('DEBUG')) :
    failthresh *= 2.0
    hardfail *= 2.0
    failpercent *= 2.0


# Run the test and check the outputs
ret = runtest (command, outputs, failureok=failureok)

if ret == 0 and cleanup_on_success :
    for ext in image_extensions + [ ".txt", ".diff" ] :
        for f in glob.iglob (srcdir + '/*' + ext) :
            os.remove(f)
            #print('REMOVED ', f)

sys.exit (ret)

#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

import os
import glob
import sys
import platform
import subprocess
import difflib
import filecmp
import shutil
import re
from itertools import chain

from optparse import OptionParser


def make_relpath (path: str, start: str=os.curdir) -> str:
    "Wrapper around os.path.relpath which always uses '/' as the separator."
    p = os.path.relpath (path, start)
    return p if platform.system() != 'Windows' else p.replace ('\\', '/')


#
# Get standard testsuite test arguments: srcdir exepath
#

srcdir = "."
path = "../.."

# Options for the command line
parser = OptionParser()
parser.add_option("-p", "--path", help="add to build area path",
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
OIIO_BUILD_ROOT = path

tmpdir = "."
tmpdir = os.path.abspath (tmpdir)
redirect = " >> out.txt "
wrapper_cmd = ""

# Command names that run_app() will recognize as the first word of a command
# and replace with the full path to the corresponding built app.
oiio_app_list = ("oiiotool", "iinfo", "idiff", "maketx", "iconvert", "igrep", "testtex", "iv")
app_list = oiio_app_list

# Try to figure out where some key things are. Go by env variables set by
# the cmake tests, but if those aren't set, assume somebody is running
# this script by hand from inside build/testsuite/TEST and that
# the rest of the tree has the standard layout.
OIIO_TESTSUITE_ROOT = os.getenv('OIIO_TESTSUITE_ROOT', '')
if OIIO_TESTSUITE_ROOT == '' :
    if os.path.exists('../../../testsuite') :
        OIIO_TESTSUITE_ROOT = '../../../testsuite'
    elif os.path.exists('../../../../testsuite') :
        OIIO_TESTSUITE_ROOT = '../../../../testsuite'
OIIO_TESTSUITE_ROOT = make_relpath(OIIO_TESTSUITE_ROOT)
OIIO_PROJECT_ROOT = make_relpath(OIIO_TESTSUITE_ROOT + "/..")

OIIO_TESTSUITE_IMAGEDIR = os.getenv('OIIO_TESTSUITE_IMAGEDIR', '')
if OIIO_TESTSUITE_IMAGEDIR == '' :
    if os.path.exists('../oiio-images'):
        OIIO_TESTSUITE_IMAGEDIR = '../oiio-images'
OIIO_TESTSUITE_IMAGEDIR = make_relpath(OIIO_TESTSUITE_IMAGEDIR)
# Set it back so tests can use it (python-imagebufalgo)
os.putenv('OIIO_TESTSUITE_IMAGEDIR', OIIO_TESTSUITE_IMAGEDIR)

refdir = "ref/"
refdirlist = [ refdir ]
mytest = os.path.split(os.path.abspath(os.getcwd()))[-1]
if str(mytest).endswith('.batch') or str(mytest).endswith('.nanobind') :
    mytest = mytest.rsplit('.', 1)[0]
test_source_dir = os.getenv('OIIO_TESTSUITE_SRC',
                            os.path.join(OIIO_TESTSUITE_ROOT, mytest))

def oiio_app (app: str) -> str:
    if (platform.system () != 'Windows' or options.devenv_config == ""):
        cmd = os.path.join(OIIO_BUILD_ROOT, "bin", app) + " "
    else:
        cmd = os.path.join(OIIO_BUILD_ROOT, "bin", options.devenv_config, app) + " "
    if wrapper_cmd != "":
        cmd = wrapper_cmd + " " + cmd
    return cmd


# Ask oiiotool what version of OpenColorIO it has embedded
ociover = subprocess.check_output([oiio_app('oiiotool').strip(),
                                   '--echo', '{getattribute(opencolorio_version)}'])
ociover = ociover.strip().decode('utf-8')[0:3]
ociover = os.getenv('OCIO_VERSION_OVERRIDE', ociover)
#print(f"OpenColorIO version = '{ociover}'")

command = ""
outputs = [ "out.txt" ]    # default

# Support for temporarily redirecting a section of a test's commands to a file
# other than the default out.txt. Use redirect_push(filename) before the
# commands that should go to the alternate file, and redirect_pop() right
# after to restore the previous redirect. Push/pop nest freely. The first time
# a given filename is pushed, it's truncated and added to 'outputs' so it gets
# checked against its ref; that same membership in 'outputs' is how we
# recognize a later push of the same filename (e.g. a second section of the
# test meant to append to it) and leave its contents alone rather than
# truncating again.
_redirect_stack: list[str] = []

def redirect_push (filename: str) -> None :
    global redirect
    _redirect_stack.append (redirect)
    if filename not in outputs :
        open (filename, "w").close ()    # truncate, but only the first time
        outputs.append (filename)
    redirect = " >> " + filename + " "

def redirect_pop () -> None :
    global redirect
    if not _redirect_stack :
        raise RuntimeError ("redirect_pop: no matching redirect_push")
    redirect = _redirect_stack.pop ()

# The image comparison thresholds are tricky to remember. Here's the key:
# A test fails if more than `failpercent` of pixel values differ by more
# than `failthresh` AND the difference is more than `failrelative` times the
# correct pixel value, or if even one pixel differs by more than `hardfail`.
failthresh = 0.004         # "Failure" threshold for any pixel value
failpercent = 0.02         # Ok fo this percentage of pixels to "fail"
hardfail = 0.012           # Even one pixel this wrong => hard failure
allowfailures = 0          # Freebie failures
failrelative = 0.001       # Ok to fail up to this amount vs the pixel value

# Some tests are designed for the app running to "fail" (in the sense of
# terminating with an error return code), for example, a test that is designed
# to present an error condition to check that it issues the right error. That
# "failure" is a success of the test! For those cases, set `failureok = 1` to
# indicate that the app having an error is fine, and the full test will pass
# or fail based on comparing the output files.
failureok = 0


anymatch = False
cleanup_on_success = False
if int(os.getenv('TESTSUITE_CLEANUP_ON_SUCCESS', '0')) :
    cleanup_on_success = True

image_extensions = [ ".tif", ".tiff", ".tx", ".exr", ".jpg", ".png", ".rla",
                     ".dpx", ".iff", ".psd", ".bmp", ".fits", ".ico",
                     ".jp2", ".jxl", ".sgi", ".tga", ".TGA", ".zfile" ]

# print ("srcdir = " + srcdir)
# print ("tmpdir = " + tmpdir)
# print ("OIIO_BUILD_ROOT = " + OIIO_BUILD_ROOT)
# print ("OIIO_TESTSUITE_IMAGEDIR = {} ({})".format(OIIO_TESTSUITE_IMAGEDIR, os.path.abspath(OIIO_TESTSUITE_IMAGEDIR)))
# print ("refdir = " + refdir)
# print ("test source dir = " + test_source_dir)

if platform.system() == 'Windows' :
    if not os.path.exists("./ref") :
        shutil.copytree (os.path.join (test_source_dir, "ref"), "./ref")
    if os.path.exists (os.path.join (test_source_dir, "src")) and not os.path.exists("./src") :
        shutil.copytree (os.path.join (test_source_dir, "src"), "./src")
    # if not os.path.exists("../data") :
    #     shutil.copytree ("../../testsuite/data", "..")
    # if not os.path.exists("../common") :
    #     shutil.copytree ("../../testsuite/common", "..")
else :
    def newsymlink(src: str, dst: str):
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


if os.getenv("Python_EXECUTABLE") :
    pythonbin = os.getenv("Python_EXECUTABLE")
else :
    pythonbin = sys.executable
#print ("pythonbin = ", pythonbin)


###########################################################################

# Handy functions...

# Strip trailing spaces/tabs from a line, but leave its line ending (if any)
# alone. Used to make text_diff tolerant of trailing whitespace, which can
# vary by platform (e.g. cmd.exe's `echo` bakes in a trailing space that a
# Unix shell would not) without being a meaningful difference in output.
def _rstrip_line (line: str) -> str:
    ending = line[len (line.rstrip ('\r\n')):]
    return line.rstrip () + ending


# Compare two text files. Returns 0 if they are equal otherwise returns
# a non-zero value and writes the differences to "diff_file".
# Based on the command-line interface to difflib example from the Python
# documentation
def text_diff (fromfile: str, tofile: str, diff_file: str=None, filter_re=None) -> int:
    import time
    try:
        fromdate = time.ctime (os.stat (fromfile).st_mtime)
        todate = time.ctime (os.stat (tofile).st_mtime)
        if filter_re:
            filt = re.compile(filter_re)
            fromlines = [l for l in open (fromfile, 'r').readlines() if filt.match(l) is not None]
            tolines   = [l for l in open (tofile, 'r').readlines() if filt.match(l) is not None]
        else:
            fromlines = open (fromfile, 'r').readlines()
            tolines   = open (tofile, 'r').readlines()
        fromlines = [_rstrip_line(l) for l in fromlines]
        tolines   = [_rstrip_line(l) for l in tolines]
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


def run_app(app: str, silent: bool=False, failureok: bool=False,
            concat: bool=True) -> str:
    cmd = app.strip()
    # If the command starts with the name of an OIIO app, substitute the
    # full path to the built app.
    words = cmd.split(maxsplit=1)
    if not words:
        return ""
    if words[0] in app_list :
        cmd = oiio_app(words[0]).strip() + (" " + words[1] if len(words) > 1 else "")
    if not silent :
        cmd += redirect
    if failureok :
        cmd += " || true "
    if concat:
        cmd += " ;\n"
    return cmd


# Take shell `commands`, split at newlines, adorn each with redirects, etc.,
# then re-join with semicolons to make a single command.
# Note: `failureok` defaults to None, meaning "use the global `failureok`
# value at the time this is called", which a run.py may have set.
def run_commands(commands: str, silent: bool=False,
                 failureok=None, concat: bool=True) -> str :
    if failureok is None :
        failureok = globals()["failureok"]
    result = ""
    for line in commands.splitlines():
        cmd = line.strip()
        # Skip empty lines or comments
        if cmd == "" or cmd.startswith("#"):
            continue
        result += run_app(cmd, silent=silent, failureok=failureok, concat=concat)
    return result


# Construct a command that will print info for an image, appending output to
# the file "out.txt".  If 'safematch' is nonzero, it will exclude printing
# of fields that tend to change from run to run or release to release.
def info_command (file: str, extraargs: str="", safematch: bool=False, hash: bool=True,
                  verbose: bool=True, silent: bool=False, concat: bool=True, failureok: bool=False,
                  info_program: str="oiiotool") -> str:
    args = ""
    if info_program == "oiiotool" :
        args += " --info"
    if verbose :
        args += " -v -a"
    if safematch :
        args += " --no-metamatch \"DateTime|Software|OriginatingProgram|ImageHistory\""
    if hash :
        args += " --hash"
    return run_app(f"{info_program} {args} {extraargs} {make_relpath(file,tmpdir)}",
                   silent=silent, failureok=failureok, concat=concat)


# Construct a command that will compare two images, appending output to
# the file "out.txt".  We allow a small number of pixels to have up to
# 1 LSB (8 bit) error, it's very hard to make different platforms and
# compilers always match to every last floating point bit.
def diff_command (fileA: str, fileB: str, extraargs: str="", silent: bool=False, concat: bool=True) -> str :
    return run_app (f"idiff -a -fail {failthresh} -failpercent {failpercent}"
                    f" -hardfail {hardfail} -allowfailures {allowfailures}"
                    f" -warn {2*failthresh} -warnpercent {failpercent}"
                    f" {extraargs} {make_relpath(fileA,tmpdir)} "
                    f" {make_relpath(fileB,tmpdir)}",
                    silent=silent, concat=concat)


# Construct a command that will create a texture, appending console
# output to the file "out.txt".
def maketx_command (infile: str, outfile: str, extraargs: str="",
                    showinfo: bool=False, showinfo_extra: str="",
                    silent: str=False, concat: str=True) -> str :
    infile_relpath = make_relpath(infile,tmpdir)
    outfile_relpath = make_relpath(outfile,tmpdir)
    command = run_app(f"maketx {infile_relpath} {extraargs} -o {outfile_relpath}",
                      silent=silent, concat=concat)
    if showinfo:
        command += info_command (outfile, extraargs=showinfo_extra, safematch=1)
    return command



# Construct a command that will test the basic ability to read and write
# an image, appending output to the file "out.txt".  First, iinfo the
# file, including a hash (VERY unlikely not to match if we've read
# correctly).  If testwrite is nonzero, also iconvert the file to make a
# copy (tests writing that format), and then idiff to make sure it
# matches the original.
def rw_command (dir: str, filename: str, testwrite: bool=True, use_oiiotool: bool=False, extraargs: str="",
                preargs: str="", idiffextraargs: str="", output_filename: str="",
                safematch: bool=False, printinfo: bool=True) -> str:
    fn = make_relpath (dir + "/" + filename, tmpdir)
    cmd = ""
    if printinfo :
        cmd += info_command (fn, safematch=safematch)
    if output_filename == "" :
        output_filename = filename
    tool = "oiiotool" if use_oiiotool else "iconvert"
    if testwrite :
        cmd += run_app(f"{tool} {preargs} {fn} {extraargs} -o {output_filename}")
        cmd += run_app(f"idiff -a {fn} -fail {failthresh} -failpercent {failpercent}"
                       + f" -hardfail {hardfail} -allowfailures {allowfailures}"
                       + f" -warn {2*failthresh} {idiffextraargs} {output_filename}")
    return cmd


# Construct a command that will testtex
def testtex_command (file: str, extraargs: str="", silent: bool=False, concat: bool=True) -> str:
    return run_app(f"testtex {file} {extraargs}",
                   silent=silent, concat=concat)


# Construct a command that will run iconvert and append its output to out.txt
def iconvert (args: str, silent: bool=False, concat: bool=True,
              failureok: bool=False) -> str:
    return run_app(f"iconvert {args}",
                   silent=silent, failureok=failureok, concat=concat)


# Construct a command that will run oiiotool and append its output to out.txt
def oiiotool (args: str, silent: bool=False, concat: bool=True,
             failureok: bool=False) -> str:
    return run_app(f"oiiotool {args}",
                   silent=silent, failureok=failureok, concat=concat)



# Check one output file against reference images in a list of reference
# directories. For each directory, it will first check for a match under
# the identical name, and if that fails, it will look for alternatives of
# the form "basename-*.ext" (or ANY match in the ref directory, if anymatch
# is True).
def checkref (name: str, refdirlist: list[str]) -> tuple[bool, str]:
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
        print("comparisons are", ([defaulttest] + glob.glob (os.path.join (ref, pattern))))
        for testfile in ([defaulttest] + glob.glob (os.path.join (ref, pattern))) :
            if not os.path.exists(testfile) :
                continue
            print ("comparing " + name + " to " + testfile)
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
def runtest (command: str, outputs: list[str], failureok: int=0) -> int :
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
        if cmdret != 0 and not failureok :
            print ("#### Error: this command failed: ", sub_command)
            print ("FAIL")
            err = 1
            if os.path.isfile("build.txt") :
                print ("---   BUILD LOG   ---\n")
                with open("build.txt", "r") as fbuild :
                    print (fbuild.read())
                print ("--- END BUILD LOG ---\n")

    for out in outputs :
        (prefix, extension) = os.path.splitext(out)
        # On Windows, change line endings of text files to unix style before
        # comparison to reference output.
        if (platform.system() == 'Windows' and os.path.exists(out)
                and extension == '.txt') :
            os.rename (out, "crlf.txt")
            os.system ("tr -d '\\r' < crlf.txt > " + out)
            if os.path.exists('crlf.txt') :
                os.remove('crlf.txt')

        (ok, testfile) = checkref (out, refdirlist)

        if ok :
            if extension in image_extensions :
                # If we got a match for an image, save the idiff results
                os.system (diff_command (out, testfile, silent=False, concat=False))
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
                os.system (diff_command (out, testfile, silent=False, concat=False))
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
# mode or when running on remote CI machines.
if (os.getenv('CI') or os.getenv('DEBUG')) :
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

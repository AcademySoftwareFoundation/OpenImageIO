#!/usr/bin/python 

import os
import glob
import sys
import platform
import subprocess
import difflib
import filecmp

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

refdir = "ref/"
parent = "../../../../../"

command = ""
outputs = [ "out.txt" ]    # default
failureok = 0
failthresh = 0.004
failpercent = 0.02

#print ("srcdir = " + srcdir)
#print ("tmpdir = " + tmpdir)
#print ("path = " + path)
#print ("refdir = " + refdir)

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
        fromlines = open (fromfile, 'rU').readlines()
        tolines   = open (tofile, 'rU').readlines()
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
        except:
            print ("Unexpected error:", sys.exc_info()[0])
    return 1



def oiio_relpath (path, start=os.curdir):
    "Wrapper around os.path.relpath which always uses '/' as the separator."
    p = os.path.relpath (path, start)
    return p if sys.platform != "win32" else p.replace ('\\', '/')


def oiio_app (app):
    # When we use Visual Studio, built applications are stored
    # in the app/$(OutDir)/ directory, e.g., Release or Debug.
    if (platform.system () != 'Windows' or options.devenv_config == ""):
        return os.path.join (path, app, app) + " "
    else:
        return os.path.join (path, app, options.devenv_config, app) + " "


# Construct a command that will compare two images, appending output to
# the file "out.txt".  If 'safematch' is nonzero, it will exclude printing
# of fields that tend to change from run to run or release to release.
def info_command (file, extraargs="", safematch=0) :
    if safematch :
        extraargs += " --no-metamatch \"DateTime|Software|OriginatingProgram|ImageHistory\""
    return (oiio_app("oiiotool") + "--info -v -a --hash " + extraargs
            + " " + oiio_relpath(file,tmpdir) + " >> out.txt ;\n")


# Construct a command that will compare two images, appending output to
# the file "out.txt".  We allow a small number of pixels to have up to
# 1 LSB (8 bit) error, it's very hard to make different platforms and
# compilers always match to every last floating point bit.
def diff_command (fileA, fileB, extraargs="", silent=False, concat=True) :
    command = (oiio_app("idiff") + "-a"
               + " -fail 0"
               + " -failpercent " + str(failpercent)
               + " -hardfail " + str(failthresh)
               + " -warn " + str(2*failthresh)
               + " " + extraargs + " " + oiio_relpath(fileA,tmpdir) 
               + " " + oiio_relpath(fileB,tmpdir))
    if not silent :
        command += " >> out.txt"
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
        command += " >> out.txt"
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
def rw_command (dir, filename, testwrite=1, use_oiiotool=0, extraargs="",
                preargs="", idiffextraargs="") :
    fn = oiio_relpath (dir + "/" + filename, tmpdir)
    cmd = (oiio_app("oiiotool") + " --info -v -a --hash " + fn
           + " >> out.txt ;\n")
    if testwrite :
        if use_oiiotool :
            cmd = (cmd + oiio_app("oiiotool") + preargs + " " + fn
                   + " " + extraargs + " -o " + filename + " >> out.txt ;\n")
        else :
            cmd = (cmd + oiio_app("iconvert") + preargs + " " + fn
                   + " " + extraargs + " " + filename + " >> out.txt ;\n")
        cmd = (cmd + oiio_app("idiff") + " -a " + fn
               + " " + idiffextraargs + " " + filename + " >> out.txt ;\n")
    return cmd


# Construct a command that will test 
def testtex_command (file, extraargs="") :
    cmd = (oiio_app("testtex") + " " + file + " " + extraargs + " " +
           " >> out.txt ;\n")
    return cmd



# Run 'command'.  For each file in 'outputs', compare it to the copy
# in 'ref/'.  If all outputs match their reference copies, return 0
# to pass.  If any outputs do not match their references return 1 to
# fail.
def runtest (command, outputs, failureok=0) :
#    print ("working dir = " + tmpdir)
    os.chdir (srcdir)
    open ("out.txt", "w").close()    # truncate out.txt

    if options.path != "" :
        sys.path = [options.path] + sys.path
    print "command = " + command

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
            print "#### Error: this command failed: ", sub_command
            print "FAIL"
            return (1)

    err = 0
    for out in outputs :
        extension = os.path.splitext(out)[1]
        ok = 0
        # We will first compare out to ref/out, and if that fails, we
        # will compare it to everything else with the same extension in
        # the ref directory.  That allows us to have multiple matching
        # variants for different platforms, etc.
        for testfile in (["ref/"+out] + glob.glob (os.path.join ("ref", "*"+extension))) :
            # print ("comparing " + out + " to " + testfile)
            if extension == ".tif" or extension == ".exr" :
                # images -- use idiff
                cmpcommand = diff_command (out, testfile, concat=False)
                # print "cmpcommand = " + cmpcommand
                cmpresult = os.system (cmpcommand)
            elif extension == ".txt" :
                cmpresult = text_diff (out, testfile, out + ".diff")
            else :
                # anything else
                cmpresult = 0 if filecmp.cmp (out, testfile) else 1
            if cmpresult == 0 :
                print ("PASS: " + out + " matches " + testfile)
                ok = 1
                break      # we're done

        if ok == 0:
            err = 1
            print "NO MATCH for " + out
            print "FAIL " + out

    return (err)


##########################################################################



#
# Read the individual run.py file for this test, which will define 
# command and outputs.
#
execfile ("run.py")

# Run the test and check the outputs
ret = runtest (command, outputs)
sys.exit (ret)

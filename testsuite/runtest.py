#!/usr/bin/python 

import os
import sys
import platform
import subprocess

from optparse import OptionParser


#
# Get standard testsuite test arguments: srcdir exepath
#

srcdir = "."
tmpdir = "."
path = "../.."

if len(sys.argv) > 1 :
    srcdir = sys.argv[1]
    srcdir = os.path.abspath (srcdir) + "/"
    os.chdir (srcdir)
if len(sys.argv) > 2 :
    path = sys.argv[2]

tmpdir = "."
tmpdir = os.path.abspath (tmpdir)

refdir = "ref/"
parent = "../../../../../"

command = ""
outputs = [ "out.txt" ]    # default

#print ("srcdir = " + srcdir)
#print ("tmpdir = " + tmpdir)
#print ("path = " + path)
#print ("refdir = " + refdir)



# Handy functions...

def oiio_app (app):
    # when we use Visual Studio, built applications are stored
    # in the app/$(OutDir)/ directory, e.g., Release or Debug.
    # In that case the special token "$<CONFIGURATION>" which is replaced by
    # the actual configuration if one is specified. "$<CONFIGURATION>" works
    # because on Windows it is a forbidden filename due to the "<>" chars.
    if (platform.system () == 'Windows'):
        return app + "/$<CONFIGURATION>/" + app + " "
    return path + "/" + app + "/" + app + " "



# Construct a command that will compare two images, appending output to
# the file "out.txt".
def info_command (file, extraargs="") :
    return (oiio_app("oiiotool") + "--info -v -a --hash " + extraargs
            + " " + os.path.relpath(file,tmpdir) + " >> out.txt ;\n")


# Construct a command that will compare two images, appending output to
# the file "out.txt".  We allow a small number of pixels to have up to
# 1 LSB (8 bit) error, it's very hard to make different platforms and
# compilers always match to every last floating point bit.
def diff_command (fileA, fileB, extraargs="", silent=0) :
    command = (oiio_app("idiff") + "-a "
               + "-failpercent 0.01 -hardfail 0.004 -warn 0.004 "
               + extraargs + " " + os.path.relpath(fileA,tmpdir) 
               + " " + os.path.relpath(fileB,tmpdir))
    if not silent :
        command += " >> out.txt"
    command += " ;\n"
    return command


# Construct a command that will test the basic ability to read and write
# an image, appending output to the file "out.txt".  First, iinfo the
# file, including a hash (VERY unlikely not to match if we've read
# correctly).  If testwrite is nonzero, also iconvert the file to make a
# copy (tests writing that format), and then idiff to make sure it
# matches the original.
def rw_command (dir, filename, testwrite=1, extraargs="") :
    fn = os.path.relpath (dir + "/" + filename, tmpdir)
    cmd = (oiio_app("oiiotool") + " --info -v -a --hash " + fn
           + " >> out.txt ;\n")
    if testwrite :
        cmd = (cmd + oiio_app("iconvert") + fn
               + " " + extraargs + " " + filename + " >> out.txt ;\n")
        cmd = (cmd + oiio_app("idiff") + " -a " + fn
               + " " + filename + " >> out.txt ;\n")
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
    parser = OptionParser()
    parser.add_option("-p", "--path", help="add to executable path",
                      action="store", type="string", dest="path", default="")
    parser.add_option("--devenv-config", help="use a MS Visual Studio configuration",
                      action="store", type="string", dest="devenv_config", default="")
    parser.add_option("--solution-path", help="MS Visual Studio solution path",
                      action="store", type="string", dest="solution_path", default="")
    (options, args) = parser.parse_args()

#    print ("working dir = " + tmpdir)
    os.chdir (srcdir)
    open ("out.txt", "w").close()    # truncate out.txt

    if options.path != "" :
        sys.path = [options.path] + sys.path
    print "command = " + command

    if (platform.system () == 'Windows'):
        # Replace the /$<CONFIGURATION>/ component added in oiio_app
        oiio_app_replace_str = "/"
        if options.devenv_config != "":
            oiio_app_replace_str = '/' + options.devenv_config + '/'
        command = command.replace ("/$<CONFIGURATION>/", oiio_app_replace_str)

    test_environ = None
    if (platform.system () == 'Windows') and (options.solution_path != "") and \
       (os.path.isdir (options.solution_path)):
        test_environ = os.environ
        libOIIO_path = options.solution_path + "\\libOpenImageIO\\"
        if options.devenv_config != "":
            libOIIO_path = libOIIO_path + '\\' + options.devenv_config
        test_environ["PATH"] = libOIIO_path + ';' + test_environ["PATH"]

    for sub_command in command.split(';'):
        cmdret = subprocess.call (sub_command, shell=True, env=test_environ)
        if cmdret != 0 and failureok == 0 :
            print "#### Error: this command failed: ", sub_command
            print "FAIL"
            return (1)

    err = 0
    for out in outputs :
        extension = os.path.splitext(out)[1]
        if extension == ".tif" or extension == ".exr" :
            # images -- use idiff
            cmpcommand = diff_command (out, refdir + out)
        else :
            # anything else, mainly text files
            if (platform.system () == 'Windows'):
                diff_cmd = "fc "
            else:
                diff_cmd = "diff "
            cmpcommand = (diff_cmd + out + " " + refdir + out)
        # print "cmpcommand = " + cmpcommand
        cmpresult = os.system (cmpcommand)
        if cmpresult == 0 :
            print "\tmatch " + out
        else :
            print "\tNO MATCH " + out
            err = 1

    if err == 0 :
        print "PASS"
    else :
        print "FAIL"
    return (err)





#
# Read the individual run.py file for this test, which will define 
# command and outputs.
#
execfile ("run.py")

# Run the test and check the outputs
ret = runtest (command, outputs)
sys.exit (ret)

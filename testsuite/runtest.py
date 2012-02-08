#!/usr/bin/python 

import os
import sys
import platform
import subprocess

from optparse import OptionParser


def oiio_app (app):
    # when we use Visual Studio, built applications are stored
    # in the app/$(OutDir)/ directory, e.g., Release or Debug.
    # In that case the special token "$<CONFIGURATION>" which is replaced by
    # the actual configuration if one is specified. "$<CONFIGURATION>" works
    # because on Windows it is a forbidden filename due to the "<>" chars.
    if (platform.system () == 'Windows'):
        return app + "/$<CONFIGURATION>/" + app + " "
    return app + "/" + app + " "



# Run 'command'.  For each file in 'outputs', compare it to the copy
# in 'ref/'.  If all outputs match their reference copies, return 0
# to pass.  If any outputs do not match their references return 1 to
# fail.
def runtest (command, outputs, cleanfiles="", failureok=0) :
    parser = OptionParser()
    parser.add_option("-p", "--path", help="add to executable path",
                      action="store", type="string", dest="path", default="")
    parser.add_option("-c", "--clean", help="clean up",
                      action="store_true", dest="clean", default=False)
    parser.add_option("--devenv-config", help="use a MS Visual Studio configuration",
                      action="store", type="string", dest="devenv_config", default="")
    parser.add_option("--solution-path", help="MS Visual Studio solution path",
                      action="store", type="string", dest="solution_path", default="")
    (options, args) = parser.parse_args()

    if options.clean :
        for out in outputs+cleanfiles :
            print "\tremoving " + out
            try :
                cmpresult = os.remove (out)
            except OSError :
                continue
        return (0)

    if options.path != "" :
        sys.path = [options.path] + sys.path
    #print "command = " + command

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

    if (platform.system () == 'Windows'):
       diff_cmd = "fc "
    else:
       diff_cmd = "diff "
    err = 0
    for out in outputs :
        extension = os.path.splitext(out)[1]
        if extension == ".tif" or extension == ".exr" :
            # images -- use idiff
            cmpcommand = (os.path.join (os.environ['IMAGEIOHOME'], "bin", "idiff")
                          + " " + out + " ref/" + out)
        else :
            # anything else, mainly text files
            cmpcommand = diff_cmd + out + " ref/" + out
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

    # if everything passed, get rid of the temporary files
    if err == 0 :
        for out in outputs+cleanfiles :
            print "\tremoving " + out
            try :
                cmpresult = os.remove (out)
            except OSError :
                continue

    return (err)



# Construct a command that will test the basic ability to read and write
# an image, appending output to the file "out.txt".  First, iinfo the
# file, including a hash (VERY unlikely not to match if we've read
# correctly).  If testwrite is nonzero, also iconvert the file to make a
# copy (tests writing that format), and then idiff to make sure it
# matches the original.
def rw_command (dir, filename, cmdpath, testwrite=1, extraargs="") :
    cmd = cmdpath + oiio_app("iinfo") + " -v -a --hash " + dir + "/" + filename + " >> out.txt ; "
    print (cmd)
    if testwrite :
        cmd = cmd + cmdpath + oiio_app("iconvert") + dir + "/" + filename + " " + extraargs + " " + filename + " >> out.txt ; "
        cmd = cmd + cmdpath + oiio_app("idiff") + "-a " + dir + "/" + filename + " " + filename + " >> out.txt "
    return cmd



# Construct a command that will compare two images, appending output to
# the file "out.txt".
def diff_command (fileA, fileB, cmdpath) :
    cmd = cmdpath + oiio_app("idiff") + "-a " + fileA + " " + fileB + " >> out.txt "
    return cmd


#!/usr/bin/python 

import os
import sys
from optparse import OptionParser


# Run 'command'.  For each file in 'outputs', compare it to the copy
# in 'ref/'.  If all outputs match their reference copies, return 0
# to pass.  If any outputs do not match their references return 1 to
# fail.
def runtest (command, outputs, cleanfiles="") :
    parser = OptionParser()
    parser.add_option("-p", "--path", help="add to executable path",
                      action="store", type="string", dest="path", default="")
    parser.add_option("-c", "--clean", help="clean up",
                      action="store_true", dest="clean", default=False)
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

    cmdret = os.system (command)
    # print "cmdret = " + str(cmdret)

    if cmdret != 0 :
        print "FAIL"
        return (1)

    err = 0
    for out in outputs :
        cmpcommand = "diff " + out + " ref/" + out
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



# Construct a command that will test the basic ability to read and write
# an image, appending output to the file "out.txt".  First, iinfo the
# file, including a hash (VERY unlikely not to match if we've read
# correctly).  If testwrite is nonzero, also iconvert the file to make a
# copy (tests writing that format), and then idiff to make sure it
# matches the original.
def rw_command (dir, filename, cmdpath, testwrite=1) :
    cmd = cmdpath + "iinfo/iinfo -v -a --hash " + dir + "/" + filename + " >> out.txt ; "
    if testwrite :
        cmd = cmd + cmdpath + "iconvert/iconvert " + dir + "/" + filename + " " + filename + " >> out.txt ; "
        cmd = cmd + cmdpath + "idiff/idiff -a " + dir + "/" + filename + " " + filename + " >> out.txt "
    return cmd



# Construct a command that will compare two images, appending output to
# the file "out.txt".
def diff_command (fileA, fileB, cmdpath) :
    cmd = cmdpath + "idiff/idiff -a " + fileA + " " + fileB + " >> out.txt "
    return cmd


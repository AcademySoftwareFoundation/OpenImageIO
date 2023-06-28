..
  Copyright Contributors to the OpenImageIO project.
  SPDX-License-Identifier: CC-BY-4.0


Searching Image Metadata With `igrep`
#####################################

The `igrep` program search one or more image files for metadata
that match a string or regular expression.



Using `igrep`
=============

The `igrep` utility is invoked as follows:

    `igrep` [*options*] *pattern* *filename* ...

Where *pattern* is a POSIX.2 regular expression (just like the Unix/Linux
`grep(1)` command), and *filename* (and any following names) specify images
or directories that should be searched. An image file will "match" if any of
its metadata contains values contain substring that are recognized regular
expression.  The image files may be of any format recognized by OpenImageIO
(i.e., for which ImageInput plugins are available).

Example::

    $ igrep Jack *.jpg 
    bar.jpg: Keywords = Carly; Jack
    foo.jpg: Keywords = Jack
    test7.jpg: ImageDescription = Jack on vacation



`igrep` command-line options
============================

.. describe:: --help

    Prints usage information to the terminal.

.. option:: --version

    Prints the version designation of the OIIO library.

.. describe:: -d

    Print directory names as it recurses.  This only happens if the `-r`
    option is also used.

.. describe:: -E

    Interpret the pattern as an extended regular expression (just like
    `egrep` or `grep -E`).

.. describe:: -f

    Match the expression against the filename, as well as the metadata
    within the file.

.. describe:: -i

    Ignore upper/lower case distinctions.  Without this flag, the expression
    matching will be case-sensitive.

.. describe:: -l

    Simply list the matching files by name, suppressing the normal output
    that would include the metadata name and values that matched. For
    example::

        $ igrep Jack *.jpg
        bar.jpg: Keywords = Carly; Jack
        foo.jpg: Keywords = Jack
        test7.jpg: ImageDescription = Jack on vacation

        $ igrep -l Jack *.jpg
        bar.jpg
        foo.jpg
        test7.jpg

.. describe:: -r

    Recurse into directories.  If this flag is present, any files specified
    that are directories will have any image file contained therein to be
    searched for a match (an so on, recursively).

.. describe:: -v

    Invert the sense of matching, to select image files that *do not* match
    the expression.


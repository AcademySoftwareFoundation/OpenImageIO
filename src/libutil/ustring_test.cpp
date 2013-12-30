/*
  Copyright 2009 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the software's owners nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  (This is the Modified BSD License)
*/


#include <iostream>
#include <cstdio>

#include "thread.h"
#include "ustring.h"
#include "strutil.h"

#include "unittest.h"


OIIO_NAMESPACE_USING;

// Test ustring's internal locks by creating a bunch of strings in many
// threads simultaneously.  Hopefully something will crash if the 
// internal table is not being locked properly.

static void
create_lotso_ustrings ()
{
    std::cout << "thread " << boost::this_thread::get_id() << "\n";
    char buf[20];
    const int iterations = 1000000;
    for (int i = 0;  i < iterations;  ++i) {
        sprintf (buf, "%d", i);
        ustring s (buf);
    }
}



void test_ustring_lock ()
{
    std::cout << "hw threads = " << boost::thread::hardware_concurrency() << "\n";

    const int numthreads = 16;
    boost::thread_group threads;
    for (int i = 0;  i < numthreads;  ++i) {
        threads.create_thread (&create_lotso_ustrings);
    }
    std::cout << "Created " << threads.size() << " threads\n";
    threads.join_all ();
    std::cout << "\n" << ustring::getstats() << "\n";
    OIIO_CHECK_ASSERT (true);  // If we make it here without crashing, pass
}



int main (int argc, char *argv[])
{
    test_ustring_lock ();

    return unit_test_failures;
}

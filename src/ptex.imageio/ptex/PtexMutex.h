#ifndef PtexMutex_h
#define PtexMutex_h

/* 
PTEX SOFTWARE
Copyright 2009 Disney Enterprises, Inc.  All rights reserved

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.

  * The names "Disney", "Walt Disney Pictures", "Walt Disney Animation
    Studios" or the names of its contributors may NOT be used to
    endorse or promote products derived from this software without
    specific prior written permission from Walt Disney Pictures.

Disclaimer: THIS SOFTWARE IS PROVIDED BY WALT DISNEY PICTURES AND
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE, NONINFRINGEMENT AND TITLE ARE DISCLAIMED.
IN NO EVENT SHALL WALT DISNEY PICTURES, THE COPYRIGHT HOLDER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND BASED ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
*/

// #define DEBUG_THREADING

/** For internal use only */
namespace PtexInternal {
#ifndef NDEBUG
    template <class T>
    class DebugLock : public T {
     public:
	DebugLock() : _locked(0) {}
	void lock()   { T::lock(); _locked = 1; }
	void unlock() { assert(_locked); _locked = 0; T::unlock(); }
	bool locked() { return _locked != 0; }
     private:
	int _locked;
    };
#endif

    /** Automatically acquire and release lock within enclosing scope. */
    template <class T>
    class AutoLock {
    public:
	AutoLock(T& m) : _m(m) { _m.lock(); }
	~AutoLock()            { _m.unlock(); }
    private:
	T& _m;
    };

#ifndef NDEBUG
    // add debug wrappers to mutex and spinlock
    typedef DebugLock<_Mutex> Mutex;
    typedef DebugLock<_SpinLock> SpinLock;
#else
    typedef _Mutex Mutex;
    typedef _SpinLock SpinLock;
#endif

    typedef AutoLock<Mutex> AutoMutex;
    typedef AutoLock<SpinLock> AutoSpin;
}

#endif

// Hand-crafted version of config/IlmBaseConfig.h for Windows
//
// Define and set to 1 if the target system has POSIX thread support
// and you want IlmBase to use it for multithreaded file I/O.
//

#undef HAVE_PTHREAD

//
// Define and set to 1 if the target system supports POSIX semaphores
// and you want OpenEXR to use them; otherwise, OpenEXR will use its
// own semaphore implementation.
//

#undef HAVE_POSIX_SEMAPHORES


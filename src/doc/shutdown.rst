.. _chap-shutdown:

Shutdown
########

Before exiting an application that utilizes OpenImageIO the `shutdown` function
must be called, which will perform shutdown of any running thread-pools. 
Failing to call `shutdown` could lead to a sporadic dead-lock during 
application shutdown on certain platforms such as Windows. 


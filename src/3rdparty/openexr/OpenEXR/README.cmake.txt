Build IlmBase and OpenEXR on Windows using cmake
------------------

What follows are instructions for generating Visual Studio solution 
files and building those two packages

1. Launch a command window, navigate to the IlmBase folder with 
CMakeLists.txt,and type command:
	setlocal
	del /f CMakeCache.txt
	cmake
      -DCMAKE_INSTALL_PREFIX=<where you want to install the ilmbase builds>
      -G "Visual Studio 10 Win64" 
      ..\ilmbase

2. Navigate to IlmBase folder in Windows Explorer, open ILMBase.sln
and build the solution. When it build successfully, right click 
INSTALL project and build. It will install the output to the path
you set up at the previous step.  

3. Go to http://www.zlib.net and download zlib 
	  
4. Launch a command window, navigate to the OpenEXR folder with 
CMakeLists.txt, and type command:	  
	setlocal
	del /f CMakeCache.txt
	cmake 
      -DZLIB_ROOT=<zlib location>
      -DILMBASE_PACKAGE_PREFIX=<where you installed the ilmbase builds>
      -DCMAKE_INSTALL_PREFIX=<where you want to instal the openexr builds>
      -G "Visual Studio 10 Win64" ^
      ..\openexr

5. Navigate to OpenEXR folder in Windows Explorer, open OpenEXR.sln
and build the solution. When it build successfully, right click 
INSTALL project and build. It will install the output to the path
you set up at the previous step. 
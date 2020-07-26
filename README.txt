Use cmake and MSVC compiller to build it under windows
Building process requires next steps:
1. open cmd 
2. run cmake with corresponding generator. For example for MSVC 2015 command line looks like: 
	cmake -G "Visual Studio 14 2015 Win64"  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE="."
3. run build with msvc compiller. In case MSVC 2015 it looks like:
	MSBuild ALL_BUILD.vcxproj /p:Configuration="Release" /p:Platform="x64"

to build under linux follow next steps:
1. open terminal
2. Go to directory with sources
2. Run

    cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release .
    make

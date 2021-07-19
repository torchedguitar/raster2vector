# raster2vector

Simple cross-platform command-line tool for turning raster images into vector
images in SVG format, where each pixel is a filled polygon object.  This makes
it easy to import low-res sprite images into vector tools like Illustrator or
CorelDraw.  These tools often have an "image trace" feature built in, but the
results are typically smoothed or curved, and for some reason can't be coerced
into simply tracing the rectangular pixel edges exactly.

Run `raster2vector -h` to see the help text, which explains the command line
options.  Note that if no output filename is specified, the default output
filename is the input filename with extension changed to `.svg`.

Tested with MSVC 16.11 and GCC 9.3, but should work with any compiler that
supports C++17 (particularly, std::filesystem).

Compile using CMake 3.16 or newer, and a build system like Ninja.  On Linux:

    mkdir out
    cd out
    cmake ..
    ninja
    ./raster2vector ~/images/sprite.png

On Windows, in a Visual Studio Developer Command Prompt (2019 or newer):

    mkdir out
    cd out
    cmake ..
    ninja
    raster2vector C:\images\sprite.png

Large files may take a while.  After processing one row of pixels, if it
appears the total time will be more than two seconds, an estimate of the
total time is printed.  When testing on Windows, Release builds were much
faster than Debug builds.

# raster2vector

Simple cross-platform command-line tool for turning raster images into vector
images in SVG format, where each pixel is a filled polygon object.  This makes
it easy to import low-res sprite images into vector tools like Illustrator or
CorelDraw.  These tools often have an "image trace" feature built in, but the
results are typically smoothed or curved, and for some reason can't be coerced
into simply tracing the rectangular pixel edges exactly.

Tested with MSVC 16.11 and GCC 9.3.  Requires C++17.

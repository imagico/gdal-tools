
GDAL based C++ tools for geodata processing
===========================================

These are various small tools based on [GDAL](https://gdal.org/) for
geodata processing tasks.

A makefile is included for building and basic tests.

gdal_valscale
-------------

`gdal_valscale` scales values in a raster image that represent quantities
relative to surface area like densities which have originally been determined
with respect to projected coordinates to correct densities with respect
to real world surface area by multiplying with the area scaling function of
the projection.  It takes a single parameter, the image file to process and
requires this to include coordinate system information readable by GDAL
(like in a GeoTIFF file).  It reads the image into memory as a whole and
modifies the file in place.

Building requires GDAL and Proj4 development packages.


gdal_maskbuffer
---------------

`gdal_maskbuffer` buffers a black/white mask image by a certain distance
in real world units.  Technically this is done by variable radius buffering
of the raster mask in projected coodinates using the scaling function of the
projection to modulate the radius.  In addition to the image file name it
takes a second parameter, the buffer radius in units of the image file
coordinate system (usually meters).  The code assumes isotropic scaling and
does not take into account any periodicity of the projection used.

Building requires GDAL and Proj4 development packages as well as
[CImg](http://cimg.eu/).


gdal_maskcompare
----------------

`gdal_maskcompare` performs a weighted comparison of two black/white mask
images.  Differences near the mask edge are weighted less than differences
far away from it.  The purpose of this tool is to perform sanity check for
the OpenStreetMap coastline processing by detecting larger erroneous
modifications but at the same time allowing smaller changes.

Building requires GDAL and Proj4 development packages as well as
[CImg](http://cimg.eu/).

Licensed under GPLv3.



# makefile for gdal-tools
# Copyright (C) 2014-2015 Christoph Hormann <chris_hormann@gmx.de>

CC=gcc
CXX=g++

ARCHFLAGS = -g

LDFLAGS = -lm
LDFLAGS_CIMG = -ltiff
LDFLAGS_GDAL = `gdal-config --libs`
LDFLAGS_PROJ = -lproj

CXXFLAGS = $(ARCHFLAGS)
CXXFLAGS_CIMG = -ltiff
CXXFLAGS_GDAL  = `gdal-config --cflags`

# ---------------------------------------

all: gdal_valscale gdal_maskbuffer

clean:
	rm -f *.o
	rm -f gdal_valscale gdal_maskbuffer

test:
	@echo "This test requires ImageMagick."
	convert -size 1024x1024 xc:gray1 -depth 16 gray_raw.tif
	gdal_translate -a_srs EPSG:3857 -a_ullr -20037508.342789244 20037508.342789244 20037508.342789244 -20037508.342789244 gray_raw.tif gray_3857.tif
	./gdal_valscale gray_3857.tif
	convert -size 1024x1024 xc:black -depth 8 -fill white -draw "rectangle 256,0 767,1023" -alpha off strip_raw.tif
	gdal_translate -a_srs EPSG:3857 -a_ullr -20037508.342789244 20037508.342789244 20037508.342789244 -20037508.342789244 strip_raw.tif strip_3857.tif
	./gdal_maskbuffer strip_3857.tif 100000

# additional test requires OSM files with coastlines extracted from planet and OSMCoastline land polygons
#	gdal_nodedensity -a_srs EPSG:3857 -ot Float32 -ts 1024 1024 -te -20037508.342789244 -20037508.342789244 20037508.342789244 20037508.342789244 osm_coastlines_tmp.osm coast_nodedensity.tif
#	./gdal_valscale coast_nodedensity.tif
#	gdal_rasterize --config GDAL_CACHEMAX 2048 -a_srs EPSG:3857 -ot Byte -ts 1024 1024 -te -20037508.342789244 -20037508.342789244 20037508.342789244 20037508.342789244 -burn 255 -at osm_coastlines_land_3857.shp coast_mask.tif
#	./gdal_maskbuffer coast_mask.tif 100000

# ---------------------------------------

gdal_valscale: gdal_valscale.o
	$(CXX) $(LDFLAGS) gdal_valscale.o -o gdal_valscale $(LDFLAGS_GDAL) $(LDFLAGS_PROJ)

gdal_maskbuffer: gdal_maskbuffer.o
	$(CXX) $(LDFLAGS) gdal_maskbuffer.o -o gdal_maskbuffer $(LDFLAGS_GDAL) $(LDFLAGS_CIMG) $(LDFLAGS_PROJ)

gdal_maskcompare: gdal_maskcompare.o
	$(CXX) $(LDFLAGS) gdal_maskcompare.o -o gdal_maskcompare $(LDFLAGS_GDAL) $(LDFLAGS_CIMG) $(LDFLAGS_PROJ)

gdal_maskcompare_wm: gdal_maskcompare_wm.o
	$(CXX) $(LDFLAGS) gdal_maskcompare_wm.o -o gdal_maskcompare_wm $(LDFLAGS_GDAL) $(LDFLAGS_CIMG) $(LDFLAGS_PROJ)

# ---------------------------------------

gdal_valscale.o: gdal_valscale.cpp
	$(CXX) -c $(CXXFLAGS) $(CXXFLAGS_GDAL) -o gdal_valscale.o gdal_valscale.cpp

gdal_maskbuffer.o: gdal_maskbuffer.cpp
	$(CXX) -c $(CXXFLAGS) $(CXXFLAGS_CIMG) $(CXXFLAGS_GDAL) -o gdal_maskbuffer.o gdal_maskbuffer.cpp

gdal_maskcompare.o: gdal_maskcompare.cpp
	$(CXX) -c $(CXXFLAGS) $(CXXFLAGS_CIMG) $(CXXFLAGS_GDAL) -o gdal_maskcompare.o gdal_maskcompare.cpp

gdal_maskcompare_wm.o: gdal_maskcompare_wm.cpp
	$(CXX) -c $(CXXFLAGS) $(CXXFLAGS_CIMG) $(CXXFLAGS_GDAL) -o gdal_maskcompare_wm.o gdal_maskcompare_wm.cpp

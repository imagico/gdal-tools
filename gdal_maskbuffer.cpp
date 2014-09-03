/* ========================================================================
    File: @(#)gdal_maskbuffer.cpp
   ------------------------------------------------------------------------
    gdal_maskbuffer - buffers a raster mask in projected coordinates by 
                      a distance in real world units
    Copyright (C) 2014 Christoph Hormann <chris_hormann@gmx.de>
   ------------------------------------------------------------------------

    This file is part of gdal_maskbuffer

    gdal_maskbuffer is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    gdal_maskbuffer is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with gdal_maskbuffer.  If not, see <http://www.gnu.org/licenses/>.

    Version history:

      0.1: initial public version, September 2014

   ========================================================================
 */

const char PROGRAM_TITLE[] = "gdal_maskbuffer 0.1";

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <algorithm>

#include <gdal_priv.h>
#include <ogrsf_frmts.h>

#include <projects.h>
#include <proj_api.h>

#define cimg_use_magick 1
#define cimg_use_tiff 1
#define cimg_use_png 1
#define cimg_display 0

#include "CImg.h"

using namespace cimg_library;


int main(int argc,char **argv)
{
	std::fprintf(stderr,"%s\n", PROGRAM_TITLE);
	std::fprintf(stderr,"-------------------------------------------------------\n");
	std::fprintf(stderr,"Copyright (C) 2014 Christoph Hormann\n");
	std::fprintf(stderr,"This program comes with ABSOLUTELY NO WARRANTY;\n");
	std::fprintf(stderr,"This is free software, and you are welcome to redistribute\n");
	std::fprintf(stderr,"it under certain conditions; see COPYING for details.\n");

	// two parameters: file name and buffer radius

	if (argc <= 2)
	{
		std::fprintf(stderr,"  You need to supply an image file name and buffer radius\n\n");
		std::exit(1);
	}

	char *fnm = argv[1];
	float radius = atof(argv[2]);

	GDALAllRegister();

	GDALDataset  *poDataset;

	poDataset = (GDALDataset *) GDALOpen( fnm, GA_Update );
	if( poDataset == NULL )
	{
		std::fprintf(stderr,"  opening file %s failed.\n\n", fnm);
		std::exit(1);
	}

	int nXSize = poDataset->GetRasterXSize();
	int nYSize = poDataset->GetRasterYSize();

	GDALRasterBand  *poBand = poDataset->GetRasterBand( 1 );

	double adfGeoTransform[6];

	if( poDataset->GetProjectionRef()  == NULL )
	{
		std::fprintf(stderr,"  Cannot process images without projection data\n\n");
		std::exit(1);
	}

	if( poDataset->GetGeoTransform( adfGeoTransform ) != CE_None )
	{
		std::fprintf(stderr,"  error reading geotransform\n\n");
		std::exit(1);
	}

	OGRSpatialReference *oSRS = (OGRSpatialReference*)OSRNewSpatialReference(poDataset->GetProjectionRef()); 

	char *str_proj4;
	oSRS->exportToProj4(&str_proj4);

	projPJ Proj = pj_init_plus(str_proj4);

	if (!Proj)
	{
		std::fprintf(stderr,"  Initializing projection in Proj4 failed.\n\n");
		std::exit(1);
	}

	std::fprintf(stderr,"  proj4: %s\n", str_proj4);

	if (!Proj->inv)
	{
		std::fprintf(stderr,"  inverse not known for projection.\n\n");
		std::exit(1);
	}

	std::fprintf(stderr,"  allocating image (%dx%d)...\n", nXSize, nYSize);

	CImg<unsigned char> img = CImg<unsigned char>(nXSize,nYSize,1,1);
	CImg<float> img_dist;

	std::fprintf(stderr,"  reading data...\n");

	poBand->RasterIO( GF_Read, 0, 0, nXSize, nYSize, 
										img.data(), nXSize, nYSize, GDT_Byte, 
										0, 0 );
	
	std::fprintf(stderr,"  generating distance field...\n");

	if (radius > 0)
		img_dist = img.get_distance(255);
	else
		img_dist = img.get_distance(0);

	std::fprintf(stderr,"  buffering...\n");

	double min_scale = 1.0e12;
	double max_scale = -1.0e12;
	size_t cnterr = 0;
	size_t cntmod = 0;

	double pixel_size = 0.5*(std::abs(adfGeoTransform[2])+std::abs(adfGeoTransform[5]));

	for (size_t py = 0; py < nYSize; py++)
	{
		for (size_t px = 0; px < nXSize; px++)
		{
			projUV dat_xy;
			struct FACTORS facs;

			dat_xy.u = adfGeoTransform[0] + adfGeoTransform[1] * (0.5+px) + adfGeoTransform[2] * (0.5+py);
			dat_xy.v = adfGeoTransform[3] + adfGeoTransform[4] * (0.5+px) + adfGeoTransform[5] * (0.5+py);

			projUV dat_ll = pj_inv(dat_xy, Proj);

			int facs_bad = pj_factors(dat_ll, Proj, 0.0, &facs);

			if (!facs_bad)
			{
				double scale = std::max(facs.h,facs.k);
				if (img_dist(px,py) < scale*std::abs(radius)/pixel_size)
				{
					cntmod++;
					if (radius > 0)
						img(px,py) = 255;
					else
						img(px,py) = 0;
				}

				min_scale = std::min(min_scale, scale);
				max_scale = std::max(max_scale, scale);

				//std::fprintf(stderr,"    (%.2f/%.2f) -> (%.2f/%.2f), %.2f, %.2f\n", dat_xy.u, dat_xy.v, dat_ll.u, dat_ll.v, facs.h, facs.k);

			}
			else if (cnterr < 1000)
			{
				std::fprintf(stderr,"    failure to get scaling factor for %.2f/%.2f\n", dat_xy.u, dat_xy.v);
				cnterr++;
				if (cnterr == 1000)
				{
					std::fprintf(stderr,"    more than 1000 errors - not showing further errors.\n");
				}
			}
		}
	}

	std::fprintf(stderr,"    maximum scaling: %.4f, minimum scaling: %.4f\n", max_scale, min_scale);
	std::fprintf(stderr,"    %d pixels changed\n", cntmod);
	std::fprintf(stderr,"  writing data...\n");

	poBand->RasterIO( GF_Write, 0, 0, nXSize, nYSize, 
										img.data(), nXSize, nYSize, GDT_Byte, 
										0, 0 );
}

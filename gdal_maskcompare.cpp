/* ========================================================================
    File: @(#)gdal_maskcompare.cpp
   ------------------------------------------------------------------------
    gdal_maskcompare - compares two mask files and rates differences
    Copyright (C) 2015 Christoph Hormann <chris_hormann@gmx.de>
   ------------------------------------------------------------------------

    This file is part of gdal_maskcompare

    gdal_maskcompare is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    gdal_maskcompare is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with gdal_maskcompare.  If not, see <http://www.gnu.org/licenses/>.

    Version history:

      0.1: initial public version, June 2015
      0.1.1: small change, August 2015

   ========================================================================
 */

const char PROGRAM_TITLE[] = "gdal_maskcompare 0.1.1";

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <algorithm>

#include <gdal_priv.h>
#include <ogrsf_frmts.h>

#include <projects.h>
#include <proj_api.h>

#define cimg_display 0

#include "CImg.h"

using namespace cimg_library;


int main(int argc,char **argv)
{
	std::fprintf(stderr,"%s\n", PROGRAM_TITLE);
	std::fprintf(stderr,"-------------------------------------------------------\n");
	std::fprintf(stderr,"Copyright (C) 2015 Christoph Hormann\n");
	std::fprintf(stderr,"This program comes with ABSOLUTELY NO WARRANTY;\n");
	std::fprintf(stderr,"This is free software, and you are welcome to redistribute\n");
	std::fprintf(stderr,"it under certain conditions; see COPYING for details.\n");

	// two parameters: file name and buffer radius

	if (argc <= 3)
	{
		std::fprintf(stderr,"  You need to supply two image file name and a radius value\n\n");
		std::exit(1);
	}

	char *fnm_ref = argv[1];
	char *fnm = argv[2];
	float radius = atof(argv[3]);

	GDALAllRegister();

	GDALDataset *poDataset_ref;
	GDALDataset *poDataset;

	poDataset_ref = (GDALDataset *) GDALOpen( fnm_ref, GA_ReadOnly );
	if( poDataset_ref == NULL )
	{
		std::fprintf(stderr,"  opening file %s failed.\n\n", fnm_ref);
		std::exit(1);
	}

	poDataset = (GDALDataset *) GDALOpen( fnm, GA_ReadOnly );
	if( poDataset == NULL )
	{
		std::fprintf(stderr,"  opening file %s failed.\n\n", fnm);
		std::exit(1);
	}

	int nXSize = poDataset_ref->GetRasterXSize();
	int nYSize = poDataset_ref->GetRasterYSize();

	if ((nXSize != poDataset->GetRasterXSize()) || (nYSize != poDataset->GetRasterYSize()))
	{
		std::fprintf(stderr,"  image files need to be same size.\n\n");
		std::exit(1);
	}

	GDALRasterBand  *poBand_ref = poDataset_ref->GetRasterBand( 1 );
	GDALRasterBand  *poBand = poDataset->GetRasterBand( 1 );

	double adfGeoTransform[6];

	if( poDataset_ref->GetProjectionRef()  == NULL )
	{
		std::fprintf(stderr,"  Cannot process image without projection data\n\n");
		std::exit(1);
	}

	if( poDataset_ref->GetGeoTransform( adfGeoTransform ) != CE_None )
	{
		std::fprintf(stderr,"  error reading geotransform\n\n");
		std::exit(1);
	}

	OGRSpatialReference *oSRS = (OGRSpatialReference*)OSRNewSpatialReference(poDataset_ref->GetProjectionRef()); 

	char *str_proj4;
	oSRS->exportToProj4(&str_proj4);

	projPJ Proj = pj_init_plus(str_proj4);

	if (!Proj)
	{
		std::fprintf(stderr,"  Initializing projection in Proj4 failed.\n\n");
		std::exit(1);
	}

	double pixel_size = 0.5*(std::abs(adfGeoTransform[1])+std::abs(adfGeoTransform[5]));

	std::fprintf(stderr,"  proj4: %s\n", str_proj4);
	std::fprintf(stderr,"  pixel size: %.2f m\n", pixel_size);

	if (!Proj->inv)
	{
		std::fprintf(stderr,"  inverse not known for projection.\n\n");
		std::exit(1);
	}

	std::fprintf(stderr,"  allocating images (%dx%d)...\n", nXSize, nYSize);

	CImg<unsigned char> img_ref = CImg<unsigned char>(nXSize,nYSize,1,1);
	CImg<unsigned char> img = CImg<unsigned char>(nXSize,nYSize,1,1);
	CImg<float> img_dist1;
	CImg<float> img_dist2;

	std::fprintf(stderr,"  reading data...\n");

	poBand_ref->RasterIO( GF_Read, 0, 0, nXSize, nYSize, 
												img_ref.data(), nXSize, nYSize, GDT_Byte, 
												0, 0 );

	poBand->RasterIO( GF_Read, 0, 0, nXSize, nYSize, 
										img.data(), nXSize, nYSize, GDT_Byte, 
										0, 0 );
	
	std::fprintf(stderr,"  generating distance fields...\n");

	if (radius > 0)
	{
		img_dist1 = img_ref.get_distance(0);
		img_dist2 = img_ref.get_distance(255);
	}
	else
	{
		img_dist1 = img_ref.get_distance(255);
		img_dist2 = img_ref.get_distance(0);
	}

	std::fprintf(stderr,"  analyzing...\n");

	size_t cnterr = 0;
	size_t cnt_l = 0;
	size_t cnt_w = 0;
	size_t cnt_lx = 0;
	size_t cnt_wx = 0;
	double area_l = 0.0;
	double area_w = 0.0;
	double area_lx = 0.0;
	double area_wx = 0.0;
	double area_all = 0.0;

	double min_scale = 1.0e12;
	double max_scale = -1.0e12;

	double min_ascale = 1.0e12;
	double max_ascale = -1.0e12;

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
				double ascale = 0.000001*facs.s; // in sqkm/sqm

				min_ascale = std::min(min_scale, facs.s);
				max_ascale = std::max(max_scale, facs.s);

				min_scale = std::min(min_scale, scale);
				max_scale = std::max(max_scale, scale);

				area_all += ascale*pixel_size*pixel_size;

				if (img(px,py) == 255)
				{
					// new in mask pixel
					if (img_ref(px,py) == 0)
					{
						if (img_dist2(px,py) < scale*std::abs(radius)/pixel_size)
						{
							cnt_l++;
							area_l += ascale*pixel_size*pixel_size;
						}
						else
						{
							cnt_lx++;
							area_lx += ascale*pixel_size*pixel_size;
						}
					}
				}
				else
				{
					// new out of mask pixel
					if (img_ref(px,py) == 255)
					{
						if (img_dist1(px,py) < scale*std::abs(radius)/pixel_size)
						{
							cnt_w++;
							area_w += ascale*pixel_size*pixel_size;
						}
						else
						{
							cnt_wx++;
							area_wx += ascale*pixel_size*pixel_size;
						}
					}
				}
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

	double area_weighted = area_l+area_w+3.0*area_lx+10.0*area_wx;

	std::fprintf(stderr,"maximum area scaling: %.4f, minimum: %.4f\n", max_ascale, min_ascale);
	std::fprintf(stderr,"maximum scaling: %.4f, minimum scaling: %.4f\n", max_scale, min_scale);

	std::fprintf(stderr,"new in mask: %d normal pixel (%.2f sqkm)\n", cnt_l, area_l);
	std::fprintf(stderr,"             %d isolated pixel (%.2f sqkm)\n", cnt_lx, area_lx);
	std::fprintf(stderr,"new out of mask: %d normal pixel (%.2f sqkm)\n", cnt_w, area_w);
	std::fprintf(stderr,"                 %d isolated pixel (%.2f sqkm)\n", cnt_wx, area_wx);
	std::fprintf(stderr,"difference rating: %.8f (%.2f sqkm)\n", area_weighted/area_all, area_weighted);
	std::fprintf(stdout,"short version: %d:%.2f:%d:%.2f:%d:%.2f:%d:%.2f:%.8f:%.2f\n", cnt_l, area_l, cnt_lx, area_lx, cnt_w, area_w, cnt_wx, area_wx, area_weighted/area_all, area_weighted);

}

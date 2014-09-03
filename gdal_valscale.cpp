/* ========================================================================
    File: @(#)gdal_valscale.cpp
   ------------------------------------------------------------------------
    gdal_valscale raster value scaling with actual pixel areas
    Copyright (C) 2014 Christoph Hormann <chris_hormann@gmx.de>
   ------------------------------------------------------------------------

    This file is part of gdal_valscale

    gdal_valscale is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    gdal_valscale is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with gdal_valscale.  If not, see <http://www.gnu.org/licenses/>.

    Version history:

      0.1: initial public version, September 2014

   ========================================================================
 */

const char PROGRAM_TITLE[] = "gdal_valscale 0.1";

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <algorithm>

#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <cpl_conv.h> // for CPLMalloc()

#include <projects.h>
#include <proj_api.h>

int main(int argc,char **argv)
{
	std::fprintf(stderr,"%s\n", PROGRAM_TITLE);
	std::fprintf(stderr,"-------------------------------------------------------\n");
	std::fprintf(stderr,"Copyright (C) 2014 Christoph Hormann\n");
	std::fprintf(stderr,"This program comes with ABSOLUTELY NO WARRANTY;\n");
	std::fprintf(stderr,"This is free software, and you are welcome to redistribute\n");
	std::fprintf(stderr,"it under certain conditions; see COPYING for details.\n");

	// only have one parameter

	if (argc <= 1)
	{
		std::fprintf(stderr,"  You need to supply an image file name as parameter\n\n");
		std::exit(1);
	}

	char *fnm = argv[1];

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

	std::fprintf(stderr,"  proj4: %s/\n", str_proj4);

	if (!Proj->inv)
	{
		std::fprintf(stderr,"  inverse not known for projection.\n\n");
		std::exit(1);
	}

	std::fprintf(stderr,"  allocating memory (%dx%dx%d)...\n", nXSize, nYSize, poDataset->GetRasterCount());

	float * pData = (float *) CPLMalloc(sizeof(float)*nXSize*nYSize*poDataset->GetRasterCount());

	std::fprintf(stderr,"  reading data...\n");

	poDataset->RasterIO( GF_Read, 0, 0, nXSize, nYSize, 
											 pData, nXSize, nYSize, GDT_Float32, 
											 poDataset->GetRasterCount(), NULL, 0, 0, 0);

	std::fprintf(stderr,"  scaling values...\n");

	double min_scale = 1.0e12;
	double max_scale = -1.0e12;

	size_t cnterr = 0;
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
				for (size_t band = 0; band < poDataset->GetRasterCount(); band++)
				{
					size_t i = poDataset->GetRasterCount()*(py*nXSize+px)+band;
					pData[i] *= facs.s;
					min_scale = std::min(min_scale, facs.s);
					max_scale = std::max(max_scale, facs.s);
				}
				//std::fprintf(stderr,"    (%.2f/%.2f) -> (%.2f/%.2f), %.2f\n", dat_xy.u, dat_xy.v, dat_ll.u, dat_ll.v, facs.s);
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
	std::fprintf(stderr,"  writing data...\n");

	poDataset->RasterIO( GF_Write, 0, 0, nXSize, nYSize, 
											 pData, nXSize, nYSize, GDT_Float32, 
											 poDataset->GetRasterCount(), NULL, 0, 0, 0);
}

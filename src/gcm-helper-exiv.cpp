/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <exiv2/image.hpp>
#include <exiv2/exif.hpp>
#include <iostream>
#include <iomanip>

int
main (int argc, char* const argv[])
{
	Exiv2::Image::AutoPtr image;
	Exiv2::ExifData exifData;
	std::string filename;
	std::string make;
	std::string model;
	std::string serial;
	int i;
	int retval = 0;
	const char *temp[] = {
		"Exif.Canon.SerialNumber",
		"Exif.Fujifilm.SerialNumber",
		"Exif.Nikon3.SerialNO",
		"Exif.Nikon3.SerialNumber",
		"Exif.OlympusEq.InternalSerialNumber",
		"Exif.OlympusEq.SerialNumber",
		"Exif.Olympus.SerialNumber2",
		"Exif.Sigma.SerialNumber",
		NULL };

	try {
		/* open file */
		if (argc == 2)
			filename = argv[1];
		if (filename.empty())
			throw Exiv2::Error(1, "No filename specified");
		image = Exiv2::ImageFactory::open(filename);
		image->readMetadata();

		/* get exif data */
		exifData = image->exifData();
		if (exifData.empty()) {
			std::string error(argv[1]);
			error += ": No Exif data found in the file";
			throw Exiv2::Error(1, error);
		}

		/* try to find make, model and serial number */
		make = exifData["Exif.Image.Make"].toString();
		model = exifData["Exif.Image.Model"].toString();
		for (i=0; temp[i] != NULL; i++) {
			if (exifData[temp[i]].idx())
				serial = exifData[temp[i]].toString();
			if (!serial.empty())
				break;
		}
		std::cout << model << "\n";
		std::cout << make << "\n";
		std::cout << serial << "\n";
	} catch (Exiv2::AnyError& e) {
		std::cout << "Failed to load: " << e << "\n";
		retval = -1;
	}
	return retval;
}



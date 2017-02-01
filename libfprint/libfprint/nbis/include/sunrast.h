/*******************************************************************************

License: 
This software was developed at the National Institute of Standards and 
Technology (NIST) by employees of the Federal Government in the course 
of their official duties. Pursuant to title 17 Section 105 of the 
United States Code, this software is not subject to copyright protection 
and is in the public domain. NIST assumes no responsibility  whatsoever for 
its use by other parties, and makes no guarantees, expressed or implied, 
about its quality, reliability, or any other characteristic. 

Disclaimer: 
This software was developed to promote biometric standards and biometric
technology testing for the Federal Government in accordance with the USA
PATRIOT Act and the Enhanced Border Security and Visa Entry Reform Act.
Specific hardware and software products identified in this software were used
in order to perform the software development.  In no case does such
identification imply recommendation or endorsement by the National Institute
of Standards and Technology, nor does it imply that the products and equipment
identified are necessarily the best available for the purpose.  

*******************************************************************************/

#ifndef _SUNRAST_H
#define _SUNRAST_H

/************************************************************/
/*         File Name: Sunrast.h                             */
/*         Package:   Sun Rasterfile I/O                    */
/*         Author:    Michael D. Garris                     */
/*         Date:      8/19/99                               */
/*         Updated:   03/16/2005 by MDG                     */
/*                                                          */
/************************************************************/

/* Contains header information related to Sun Rasterfile images. */

typedef struct sunrasterhdr {
	int	magic;		/* magic number */
	int	width;		/* width (in pixels) of image */
	int	height;		/* height (in pixels) of image */
	int	depth;		/* depth (1, 8, or 24 bits) of pixel */
	int	raslength;	/* length (in bytes) of image */
	int	rastype;	/* type of file; see SUN_* below */
	int	maptype;	/* type of colormap; see MAP_* below */
	int	maplength;	/* length (bytes) of following map */
	/* color map follows for maplength bytes, followed by image */
} SUNHEAD;

#define	SUN_MAGIC	0x59a66a95

	/* Sun supported ras_type's */
#define SUN_STANDARD	1	/* Raw pixrect image in 68000 byte order */
#define SUN_RUN_LENGTH	2	/* Run-length compression of bytes */
#define SUN_FORMAT_RGB	3	/* XRGB or RGB instead of XBGR or BGR */
#define SUN_FORMAT_TIFF	4	/* tiff <-> standard rasterfile */
#define SUN_FORMAT_IFF	5	/* iff (TAAC format) <-> standard rasterfile */

	/* Sun supported maptype's */
#define MAP_RAW		2
#define MAP_NONE	0	/* maplength is expected to be 0 */
#define MAP_EQUAL_RGB	1	/* red[maplength/3],green[],blue[] */

/*
 * NOTES:
 *   Each line of a bitmap image should be rounded out to a multiple
 *   of 16 bits.
 */

/* sunrast.c */
extern int ReadSunRaster(const char *, SUNHEAD **, unsigned char **, int *,
                         unsigned char **, int *, int *, int *, int *);
extern int WriteSunRaster(char *, unsigned char *, const int, const int,
                         const int);

#endif

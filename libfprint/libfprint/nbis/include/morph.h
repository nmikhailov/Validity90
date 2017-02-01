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

#ifndef __MORPH_H__
#define __MORPH_H__

/* Modified 10/26/1999 by MDG to avoid indisciminate erosion of pixels */
/* along the edge of the binary image.                                 */

extern void erode_charimage_2(unsigned char *, unsigned char *,
                     const int, const int);
extern void dilate_charimage_2(unsigned char *, unsigned char *,
                     const int, const int);
extern char get_south8_2(char *, const int, const int, const int, const int);
extern char get_north8_2(char *, const int, const int, const int);
extern char get_east8_2(char *, const int, const int, const int);
extern char get_west8_2(char *, const int, const int);

#endif /* !__MORPH_H__ */

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

/***********************************************************************
      LIBRARY: LFS - NIST Latent Fingerprint System

      FILE:    MORPH.C
      AUTHOR:  Michael D. Garris
      DATE:    10/04/1999
      UPDATED: 10/26/1999 by MDG
               To avoid indisciminate erosion of pixels along
               the edge of the binary image.
      UPDATED: 03/16/2005 by MDG

      Contains general support image morphology routines required by
      the NIST Latent Fingerprint System (LFS).

***********************************************************************
               ROUTINES:
                        erode_charimage_2()
                        dilate_charimage_2()
                        get_south8_2()
                        get_north8_2()
                        get_east8_2()
                        get_west8_2()

***********************************************************************/

#include <morph.h>
#include <string.h>

/*************************************************************************
**************************************************************************
#cat: erode_charimage_2 - Erodes an 8-bit image by setting true pixels to zero
#cat:             if any of their 4 neighbors is zero.  Allocation of the
#cat:             output image is the responsibility of the caller.  The
#cat:             input image remains unchanged.  This routine will NOT
#cat:             erode pixels indiscriminately along the image border.

   Input:
      inp       - input 8-bit image to be eroded
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
   Output:
      out       - contains to the resulting eroded image
**************************************************************************/
void erode_charimage_2(unsigned char *inp, unsigned char *out,
                     const int iw, const int ih)
{
   int row, col;
   unsigned char *itr = inp, *otr = out;
 
   memcpy(out, inp, iw*ih);
 
   /* for true pixels. kill pixel if there is at least one false neighbor */
   for ( row = 0 ; row < ih ; row++ )
      for ( col = 0 ; col < iw ; col++ )
      {  
         if (*itr)      /* erode only operates on true pixels */
         {
            /* more efficient with C's left to right evaluation of     */
            /* conjuctions. E N S functions not executed if W is false */
            if (!(get_west8_2 ((char *)itr, col        , 1 ) &&
                  get_east8_2 ((char *)itr, col, iw    , 1 ) &&
                  get_north8_2((char *)itr, row, iw    , 1 ) &&
                  get_south8_2((char *)itr, row, iw, ih, 1)))
               *otr = 0;
         }
         itr++ ; otr++;
      }  
}

/*************************************************************************
**************************************************************************
#cat: dilate_charimage_2 - Dilates an 8-bit image by setting false pixels to
#cat:             one if any of their 4 neighbors is non-zero.  Allocation
#cat:             of the output image is the responsibility of the caller.
#cat:             The input image remains unchanged.

   Input:
      inp       - input 8-bit image to be dilated
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
   Output:
      out       - contains to the resulting dilated image
**************************************************************************/
void dilate_charimage_2(unsigned char *inp, unsigned char *out,
                      const int iw, const int ih)
{
   int row, col;
   unsigned char *itr = inp, *otr = out;
 
   memcpy(out, inp, iw*ih);
 
   /* for all pixels. set pixel if there is at least one true neighbor */
   for ( row = 0 ; row < ih ; row++ )
      for ( col = 0 ; col < iw ; col++ )
      {  
         if (!*itr)     /* pixel is already true, neighbors irrelevant */
         {
            /* more efficient with C's left to right evaluation of     */
            /* conjuctions. E N S functions not executed if W is false */
            if (get_west8_2 ((char *)itr, col        , 0) ||
                get_east8_2 ((char *)itr, col, iw    , 0) ||
                get_north8_2((char *)itr, row, iw    , 0) ||
                get_south8_2((char *)itr, row, iw, ih, 0))
               *otr = 1;
         }
         itr++ ; otr++;
      }  
}

/*************************************************************************
**************************************************************************
#cat: get_south8_2 - Returns the value of the 8-bit image pixel 1 below the
#cat:                current pixel if defined else it returns  (char)0.

   Input:
      ptr       - points to current pixel in image
      row       - y-coord of current pixel
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
      failcode  - return value if desired pixel does not exist
   Return Code:
      Zero      - if neighboring pixel is undefined
                  (outside of image boundaries)
      Pixel     - otherwise, value of neighboring pixel
**************************************************************************/
char get_south8_2(char *ptr, const int row, const int iw, const int ih,
                  const int failcode)
{
   if (row >= ih-1) /* catch case where image is undefined southwards   */
      return failcode; /* use plane geometry and return code.           */

      return *(ptr+iw);
}

/*************************************************************************
**************************************************************************
#cat: get_north8_2 - Returns the value of the 8-bit image pixel 1 above the
#cat:                current pixel if defined else it returns  (char)0.

   Input:
      ptr       - points to current pixel in image
      row       - y-coord of current pixel
      iw        - width (in pixels) of image
      failcode  - return value if desired pixel does not exist
   Return Code:
      Zero      - if neighboring pixel is undefined
                  (outside of image boundaries)
      Pixel     - otherwise, value of neighboring pixel
**************************************************************************/
char get_north8_2(char *ptr, const int row, const int iw,
                  const int failcode)
{
   if (row < 1)     /* catch case where image is undefined northwards   */
      return failcode; /* use plane geometry and return code.           */

      return *(ptr-iw);
}

/*************************************************************************
**************************************************************************
#cat: get_east8_2 - Returns the value of the 8-bit image pixel 1 right of the
#cat:               current pixel if defined else it returns  (char)0.

   Input:
      ptr       - points to current pixel in image
      col       - x-coord of current pixel
      iw        - width (in pixels) of image
      failcode  - return value if desired pixel does not exist
   Return Code:
      Zero      - if neighboring pixel is undefined
                  (outside of image boundaries)
      Pixel     - otherwise, value of neighboring pixel
**************************************************************************/
char get_east8_2(char *ptr, const int col, const int iw,
                 const int failcode)
{
   if (col >= iw-1) /* catch case where image is undefined eastwards    */
      return failcode; /* use plane geometry and return code.           */

      return *(ptr+ 1);
}

/*************************************************************************
**************************************************************************
#cat: get_west8_2 - Returns the value of the 8-bit image pixel 1 left of the
#cat:              current pixel if defined else it returns  (char)0.

   Input:
      ptr       - points to current pixel in image
      col       - x-coord of current pixel
      failcode  - return value if desired pixel does not exist
   Return Code:
      Zero      - if neighboring pixel is undefined
                  (outside of image boundaries)
      Pixel     - otherwise, value of neighboring pixel
**************************************************************************/
char get_west8_2(char *ptr, const int col, const int failcode)
{
   if (col < 1)     /* catch case where image is undefined westwards     */
      return failcode; /* use plane geometry and return code.            */

      return *(ptr- 1);
}

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

      FILE:    BLOCK.C
      AUTHOR:  Michael D. Garris
      DATE:    03/16/1999
      UPDATED: 10/04/1999 Version 2 by MDG
      UPDATED: 03/16/2005 by MDG

      Contains routines responsible for partitioning arbitrarily-
      sized images into equally-sized blocks as part of the NIST
      Latent Fingerprint System (LFS).

***********************************************************************
               ROUTINES:
                        block_offsets()
                        low_contrast_block()
                        find_valid_block()
                        set_margin_blocks()

***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lfs.h>

/*************************************************************************
**************************************************************************
#cat: block_offsets - Divides an image into mw X mh equally sized blocks,
#cat:       returning a list of offsets to the top left corner of each block.
#cat:       For images that are even multiples of BLOCKSIZE, blocks do not
#cat:       not overlap and are immediately adjacent to each other.  For image
#cat:       that are NOT even multiples of BLOCKSIZE, blocks continue to be
#cat:       non-overlapping up to the last column and/or last row of blocks.
#cat:       In these cases the blocks are adjacent to the edge of the image and
#cat:       extend inwards BLOCKSIZE units, overlapping the neighboring column
#cat:       or row of blocks.  This routine also accounts for image padding
#cat:       which makes things a little more "messy". This routine is primarily
#cat:       responsible providing the ability to processs arbitrarily-sized
#cat:       images.  The strategy used here is simple, but others are possible.

   Input:
      iw        - width (in pixels) of the orginal input image
      ih        - height (in pixels) of the orginal input image
      pad       - the padding (in pixels) required to support the desired
                  range of block orientations for DFT analysis.  This padding
                  is required along the entire perimeter of the input image.
                  For certain applications, the pad may be zero.
      blocksize - the width and height (in pixels) of each image block
   Output:
      optr      - points to the list of pixel offsets to the origin of
                  each block in the "padded" input image
      ow        - the number of horizontal blocks in the input image
      oh        - the number of vertical blocks in the input image
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
int block_offsets(int **optr, int *ow, int *oh,
          const int iw, const int ih, const int pad, const int blocksize)
{
   int *blkoffs, bx, by, bw, bh, bi, bsize;
   int blkrow_start, blkrow_size, offset;
   int lastbw, lastbh;
   int pad2, pw;

   /* Test if unpadded image is smaller than a single block */
   if((iw < blocksize) || (ih < blocksize)){
      fprintf(stderr,
         "ERROR : block_offsets : image must be at least %d by %d in size\n",
              blocksize, blocksize);
      return(-80);
   }

   /* Compute padded width and height of image */
   pad2 = pad<<1;
   pw = iw + pad2;

   /* Compute the number of columns and rows of blocks in the image. */
   /* Take the ceiling to account for "leftovers" at the right and   */
   /* bottom of the unpadded image */
   bw = (int)ceil(iw / (double)blocksize);
   bh = (int)ceil(ih / (double)blocksize);

   /* Total number of blocks in the image */
   bsize = bw*bh;

   /* The index of the last column */
   lastbw = bw - 1;
   /* The index of the last row */
   lastbh = bh - 1;

   /* Allocate list of block offsets */
   blkoffs = (int *)malloc(bsize * sizeof(int));
   if(blkoffs == (int *)NULL){
      fprintf(stderr, "ERROR : block_offsets : malloc : blkoffs\n");
      return(-81);
   }

   /* Current block index */
   bi = 0;

   /* Current offset from top of padded image to start of new row of  */
   /* unpadded image blocks. It is initialize to account for the      */
   /* padding and will always be indented the size of the padding     */
   /* from the left edge of the padded image.                         */
   blkrow_start = (pad * pw) + pad;

   /* Number of pixels in a row of blocks in the padded image */
   blkrow_size = pw * blocksize;  /* row width X block height */

   /* Foreach non-overlapping row of blocks in the image */
   for(by = 0; by < lastbh; by++){
      /* Current offset from top of padded image to beginning of */
      /* the next block */
      offset = blkrow_start;
      /* Foreach non-overlapping column of blocks in the image */
      for(bx = 0; bx < lastbw; bx++){
         /* Store current block offset */
         blkoffs[bi++] = offset;
         /* Bump to the beginning of the next block */
         offset += blocksize;
      }

      /* Compute and store "left-over" block in row.    */
      /* This is the block in the last column of row.   */
      /* Start at far right edge of unpadded image data */
      /* and come in BLOCKSIZE pixels.                  */
      blkoffs[bi++] = blkrow_start + iw - blocksize;
      /* Bump to beginning of next row of blocks */
      blkrow_start += blkrow_size;
   }

   /* Compute and store "left-over" row of blocks at bottom of image */
   /* Start at bottom edge of unpadded image data and come up        */
   /* BLOCKSIZE pixels. This too must account for padding.           */
   blkrow_start = ((pad + ih - blocksize) * pw) + pad;
   /* Start the block offset for the last row at this point */
   offset = blkrow_start;
   /* Foreach non-overlapping column of blocks in last row of the image */
   for(bx = 0; bx < lastbw; bx++){
      /* Store current block offset */
      blkoffs[bi++] = offset;
      /* Bump to the beginning of the next block */
      offset += blocksize;
   }

   /* Compute and store last "left-over" block in last row.      */
   /* Start at right edge of unpadded image data and come in     */
   /* BLOCKSIZE pixels.                                          */
   blkoffs[bi++] = blkrow_start + iw - blocksize;

   *optr = blkoffs;
   *ow = bw;
   *oh = bh;
   return(0);
}

/*************************************************************************
#cat: low_contrast_block - Takes the offset to an image block of specified
#cat:             dimension, and analyzes the pixel intensities in the block
#cat:             to determine if there is sufficient contrast for further
#cat:             processing.

   Input:
      blkoffset - byte offset into the padded input image to the origin of
                  the block to be analyzed
      blocksize - dimension (in pixels) of the width and height of the block
                  (passing separate blocksize from LFSPARMS on purpose)
      pdata     - padded input image data (8 bits [0..256) grayscale)
      pw        - width (in pixels) of the padded input image
      ph        - height (in pixels) of the padded input image
      lfsparms  - parameters and thresholds for controlling LFS
   Return Code:
      TRUE     - block has sufficiently low contrast
      FALSE    - block has sufficiently hight contrast
      Negative - system error
**************************************************************************
**************************************************************************/
int low_contrast_block(const int blkoffset, const int blocksize,
                       unsigned char *pdata, const int pw, const int ph,
                       const LFSPARMS *lfsparms)
{
   int pixtable[IMG_6BIT_PIX_LIMIT], numpix;
   int px, py, pi;
   unsigned char *sptr, *pptr;
   int delta;
   double tdbl;
   int prctmin = 0, prctmax = 0, prctthresh;
   int pixsum, found;

   numpix = blocksize*blocksize;
   memset(pixtable, 0, IMG_6BIT_PIX_LIMIT*sizeof(int));

   tdbl = (lfsparms->percentile_min_max/100.0) * (double)(numpix-1);
   tdbl = trunc_dbl_precision(tdbl, TRUNC_SCALE);
   prctthresh = sround(tdbl);

   sptr = pdata+blkoffset;
   for(py = 0; py < blocksize; py++){
      pptr = sptr;
      for(px = 0; px < blocksize; px++){
         pixtable[*pptr]++;
         pptr++;
      }
      sptr += pw;
   }

   pi = 0;
   pixsum = 0;
   found = FALSE;
   while(pi < IMG_6BIT_PIX_LIMIT){
      pixsum += pixtable[pi];
      if(pixsum >= prctthresh){
         prctmin = pi;
         found = TRUE;
         break;
      }
      pi++;
   }
   if(!found){
      fprintf(stderr,
              "ERROR : low_contrast_block : min percentile pixel not found\n");
      return(-510);
   }

   pi = IMG_6BIT_PIX_LIMIT-1;
   pixsum = 0;
   found = FALSE;
   while(pi >= 0){
      pixsum += pixtable[pi];
      if(pixsum >= prctthresh){
         prctmax = pi;
         found = TRUE;
         break;
      }
      pi--;
   }
   if(!found){
      fprintf(stderr,
              "ERROR : low_contrast_block : max percentile pixel not found\n");
      return(-511);
   }

   delta = prctmax - prctmin;

   if(delta < lfsparms->min_contrast_delta)
      return(TRUE);
   else
      return(FALSE);
}

/*************************************************************************
**************************************************************************
#cat: find_valid_block - Take a Direction Map, Low Contrast Map,
#cat:             Starting block address, a direction and searches the
#cat:             maps in the specified direction until either a block valid
#cat:             direction is encountered or a block flagged as LOW CONTRAST
#cat:             is encountered.  If a valid direction is located, it and the
#cat:             address of the corresponding block are returned with a
#cat:             code of FOUND.  Otherwise, a code of NOT_FOUND is returned.

   Input:
      direction_map    - map of blocks containing directional ridge flows
      low_contrast_map - map of blocks flagged as LOW CONTRAST
      sx        - X-block coord where search starts in maps
      sy        - Y-block coord where search starts in maps
      mw        - number of blocks horizontally in the maps
      mh        - number of blocks vertically in the maps
      x_incr    - X-block increment to direct search
      y_incr    - Y-block increment to direct search
   Output:
      nbr_dir   - valid direction found
      nbr_x     - X-block coord where valid direction found
      nbr_y     - Y-block coord where valid direction found
   Return Code:
      FOUND     - neighboring block with valid direction found
      NOT_FOUND - neighboring block with valid direction NOT found
**************************************************************************/
int find_valid_block(int *nbr_dir, int *nbr_x, int *nbr_y,
                     int *direction_map, int *low_contrast_map,
                     const int sx, const int sy,
                     const int mw, const int mh,
                     const int x_incr, const int y_incr)
{
   int x, y, dir;

   /* Initialize starting block coords. */
   x = sx + x_incr;
   y = sy + y_incr;

   /* While we are not outside the boundaries of the map ... */
   while((x >= 0) && (x < mw) && (y >= 0) && (y < mh)){
      /* Stop unsuccessfully if we encounter a LOW CONTRAST block. */
      if(*(low_contrast_map+(y*mw)+x))
         return(NOT_FOUND);

      /* Stop successfully if we encounter a block with valid direction. */
      if((dir = *(direction_map+(y*mw)+x)) >= 0){
         *nbr_dir = dir;
         *nbr_x = x;
         *nbr_y = y;
         return(FOUND);
      }

      /* Otherwise, advance to the next block in the map. */
      x += x_incr;
      y += y_incr;
   }

   /* If we get here, then we did not find a valid block in the given */
   /* direction in the map.                                           */
   return(NOT_FOUND);
}

/*************************************************************************
**************************************************************************
#cat: set_margin_blocks - Take an image map and sets its perimeter values to
#cat:             the specified value.

   Input:
      map       - map of blocks to be modified
      mw        - number of blocks horizontally in the map
      mh        - number of blocks vertically in the map
      margin_value - value to be assigned to the perimeter blocks
   Output:
      map       - resulting map
**************************************************************************/
void set_margin_blocks(int *map, const int mw, const int mh,
                    const int margin_value)
{
   int x, y;
   int *ptr1, *ptr2;

   ptr1 = map;
   ptr2 = map+((mh-1)*mw);
   for(x = 0; x < mw; x++){
      *ptr1++ = margin_value;
      *ptr2++ = margin_value;
   }

   ptr1 = map + mw;
   ptr2 = map + mw + mw - 1;
   for(y = 1; y < mh-1; y++){
      *ptr1 = margin_value;
      *ptr2 = margin_value;
      ptr1 += mw;
      ptr2 += mw;
   }

}


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

      FILE:    DFT.C
      AUTHOR:  Michael D. Garris
      DATE:    03/16/1999
      UPDATED: 03/16/2005 by MDG

      Contains routines responsible for conducting Discrete Fourier
      Transforms (DFT) analysis on a block of image data as part of
      the NIST Latent Fingerprint System (LFS).

***********************************************************************
               ROUTINES:
                        dft_dir_powers()
                        sum_rot_block_rows()
                        dft_power()
                        dft_power_stats()
                        get_max_norm()
                        sort_dft_waves()
***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <lfs.h>

/*************************************************************************
**************************************************************************
#cat: sum_rot_block_rows - Computes a vector or pixel row sums by sampling
#cat:               the current image block at a given orientation.  The
#cat:               sampling is conducted using a precomputed set of rotated
#cat:               pixel offsets (called a grid) relative to the orgin of
#cat:               the image block.

   Input:
      blkptr    - the pixel address of the origin of the current image block
      grid_offsets - the rotated pixel offsets for a block-sized grid
                  rotated according to a specific orientation
      blocksize - the width and height of the image block and thus the size
                  of the rotated grid
   Output:
      rowsums   - the resulting vector of pixel row sums
**************************************************************************/
static void sum_rot_block_rows(int *rowsums, const unsigned char *blkptr,
                        const int *grid_offsets, const int blocksize)
{
   int ix, iy, gi;

   /* Initialize rotation offset index. */
   gi = 0;

   /* For each row in block ... */
   for(iy = 0; iy < blocksize; iy++){
      /* The sums are accumlated along the rotated rows of the grid, */
      /* so initialize row sum to 0.                                 */
      rowsums[iy] = 0;
      /* Foreach column in block ... */
      for(ix = 0; ix < blocksize; ix++){
         /* Accumulate pixel value at rotated grid position in image */
         rowsums[iy] += *(blkptr + grid_offsets[gi]);
         gi++;
      }
   }
}

/*************************************************************************
**************************************************************************
#cat: dft_power - Computes the DFT power by applying a specific wave form
#cat:             frequency to a vector of pixel row sums computed from a
#cat:             specific orientation of the block image

   Input:
      rowsums - accumulated rows of pixels from within a rotated grid
                overlaying an input image block
      wave    - the wave form (cosine and sine components) at a specific
                frequency
      wavelen - the length of the wave form (must match the height of the
                image block which is the length of the rowsum vector)
   Output:
      power   - the computed DFT power for the given wave form at the
                given orientation within the image block
**************************************************************************/
static void dft_power(double *power, const int *rowsums,
               const DFTWAVE *wave, const int wavelen)
{
   int i;
   double cospart, sinpart;

   /* Initialize accumulators */
   cospart = 0.0;
   sinpart = 0.0;

   /* Accumulate cos and sin components of DFT. */
   for(i = 0; i < wavelen; i++){
      /* Multiply each rotated row sum by its        */
      /* corresponding cos or sin point in DFT wave. */
      cospart += (rowsums[i] * wave->cos[i]);
      sinpart += (rowsums[i] * wave->sin[i]);
   }

   /* Power is the sum of the squared cos and sin components */
   *power = (cospart * cospart) + (sinpart * sinpart);
}

/*************************************************************************
**************************************************************************
#cat: dft_dir_powers - Conducts the DFT analysis on a block of image data.
#cat:         The image block is sampled across a range of orientations
#cat:         (directions) and multiple wave forms of varying frequency are
#cat:         applied at each orientation.  At each orentation, pixels are
#cat:         accumulated along each rotated pixel row, creating a vector
#cat:         of pixel row sums.  Each DFT wave form is then applied
#cat:         individually to this vector of pixel row sums.  A DFT power
#cat:         value is computed for each wave form (frequency0 at each
#cat:         orientaion within the image block.  Therefore, the resulting DFT
#cat:         power vectors are of dimension (N Waves X M Directions).
#cat:         The power signatures derived form this process are used to
#cat:         determine dominant direction flow within the image block.

   Input:
      pdata     - the padded input image.  It is important that the image
                  be properly padded, or else the sampling at various block
                  orientations may result in accessing unkown memory.
      blkoffset - the pixel offset form the origin of the padded image to
                  the origin of the current block in the image
      pw        - the width (in pixels) of the padded input image
      ph        - the height (in pixels) of the padded input image
      dftwaves  - structure containing the DFT wave forms
      dftgrids  - structure containing the rotated pixel grid offsets
   Output:
      powers    - DFT power computed from each wave form frequencies at each
                  orientation (direction) in the current image block
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
int dft_dir_powers(double **powers, unsigned char *pdata,
               const int blkoffset, const int pw, const int ph,
               const DFTWAVES *dftwaves, const ROTGRIDS *dftgrids)
{
   int w, dir;
   int *rowsums;
   unsigned char *blkptr;

   /* Allocate line sum vector, and initialize to zeros */
   /* This routine requires square block (grid), so ERROR otherwise. */
   if(dftgrids->grid_w != dftgrids->grid_h){
      fprintf(stderr, "ERROR : dft_dir_powers : DFT grids must be square\n");
      return(-90);
   }
   rowsums = (int *)malloc(dftgrids->grid_w * sizeof(int));
   if(rowsums == (int *)NULL){
      fprintf(stderr, "ERROR : dft_dir_powers : malloc : rowsums\n");
      return(-91);
   }

   /* Foreach direction ... */
   for(dir = 0; dir < dftgrids->ngrids; dir++){
      /* Compute vector of line sums from rotated grid */
      blkptr = pdata + blkoffset;
      sum_rot_block_rows(rowsums, blkptr,
                         dftgrids->grids[dir], dftgrids->grid_w);

      /* Foreach DFT wave ... */
      for(w = 0; w < dftwaves->nwaves; w++){
         dft_power(&(powers[w][dir]), rowsums,
                   dftwaves->waves[w], dftwaves->wavelen);
      }
   }

   /* Deallocate working memory. */
   free(rowsums);

   return(0);
}

/*************************************************************************
**************************************************************************
#cat: get_max_norm - Analyses a DFT power vector for a specific wave form
#cat:                applied at different orientations (directions) to the
#cat:                current image block.  The routine retuns the maximum
#cat:                power value in the vector, the direction at which the
#cat:                maximum occurs, and a normalized power value.  The
#cat:                normalized power is computed as the maximum power divided
#cat:                by the average power across all the directions.  These
#cat:                simple statistics are fundamental to the selection of
#cat:                a dominant direction flow for the image block.

   Input:
      power_vector - the DFT power values derived form a specific wave form
                     applied at different directions
      ndirs      - the number of directions to which the wave form was applied
   Output:
      powmax     - the maximum power value in the DFT power vector
      powmax_dir - the direciton at which the maximum power value occured
      pownorm    - the normalized power corresponding to the maximum power
**************************************************************************/
static void get_max_norm(double *powmax, int *powmax_dir,
               double *pownorm, const double *power_vector, const int ndirs)
{
   int dir;
   double max_v, powsum;
   int max_i;
   double powmean;

   /* Find max power value and store corresponding direction */
   max_v = power_vector[0];
   max_i = 0;

   /* Sum the total power in a block at a given direction */
   powsum = power_vector[0];

   /* For each direction ... */
   for(dir = 1; dir < ndirs; dir++){
      powsum += power_vector[dir];
      if(power_vector[dir] > max_v){
         max_v = power_vector[dir];
         max_i = dir;
      }
   }

   *powmax = max_v;
   *powmax_dir = max_i;

   /* Powmean is used as denominator for pownorm, so setting  */
   /* a non-zero minimum avoids possible division by zero.    */
   powmean = max(powsum, MIN_POWER_SUM)/(double)ndirs;

   *pownorm = *powmax / powmean;
}

/*************************************************************************
**************************************************************************
#cat: sort_dft_waves - Creates a ranked list of DFT wave form statistics
#cat:                  by sorting on the normalized squared maximum power.

   Input:
      powmaxs  - maximum DFT power for each wave form used to derive
                 statistics
      pownorms - normalized maximum power corresponding to values in powmaxs
      nstats   - number of wave forms used to derive statistics (N Wave - 1)
   Output:
      wis      - sorted list of indices corresponding to the ranked set of
                 wave form statistics.  These indices will be used as
                 indirect addresses when processing the power statistics
                 in descending order of "dominance"
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
static int sort_dft_waves(int *wis, const double *powmaxs, const double *pownorms,
                   const int nstats)
{
   int i;
   double *pownorms2;

   /* Allocate normalized power^2 array */
   pownorms2 = (double *)malloc(nstats * sizeof(double));
   if(pownorms2 == (double *)NULL){
      fprintf(stderr, "ERROR : sort_dft_waves : malloc : pownorms2\n");
      return(-100);
   }

   for(i = 0; i < nstats; i++){
      /* Wis will hold the sorted statistic indices when all is done. */
      wis[i] = i;
      /* This is normalized squared max power. */
      pownorms2[i] = powmaxs[i] * pownorms[i];
   }

   /* Sort the statistic indices on the normalized squared power. */
   bubble_sort_double_dec_2(pownorms2, wis, nstats);

   /* Deallocate the working memory. */
   free(pownorms2);

   return(0);
}

/*************************************************************************
**************************************************************************
#cat: dft_power_stats - Derives statistics from a set of DFT power vectors.
#cat:           Statistics are computed for all but the lowest frequency
#cat:           wave form, including the Maximum power for each wave form,
#cat:           the direction at which the maximum power occured, and a
#cat:           normalized value for the maximum power.  In addition, the
#cat:           statistics are ranked in descending order based on normalized
#cat:           squared maximum power.  These statistics are fundamental
#cat:           to selecting a dominant direction flow for the current
#cat:           input image block.

   Input:
      powers   - DFT power vectors (N Waves X M Directions) computed for
                 the current image block from which the values in the
                 statistics arrays are derived
      fw       - the beginning of the range of wave form indices from which
                 the statistcs are to derived
      tw       - the ending of the range of wave form indices from which
                 the statistcs are to derived (last index is tw-1)
      ndirs    - number of orientations (directions) at which the DFT
                 analysis was conducted
   Output:
      wis      - list of ranked wave form indicies of the corresponding
                 statistics based on normalized squared maximum power. These
                 indices will be used as indirect addresses when processing
                 the power statistics in descending order of "dominance"
      powmaxs  - array holding the maximum DFT power for each wave form
                 (other than the lowest frequecy)
      powmax_dirs - array to holding the direction corresponding to
                  each maximum power value in powmaxs
      pownorms - array to holding the normalized maximum powers corresponding
                 to each value in powmaxs
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
int dft_power_stats(int *wis, double *powmaxs, int *powmax_dirs,
                     double *pownorms, double **powers,
                     const int fw, const int tw, const int ndirs)
{
   int w, i;
   int ret; /* return code */

   for(w = fw, i = 0; w < tw; w++, i++){
      get_max_norm(&(powmaxs[i]), &(powmax_dirs[i]),
                    &(pownorms[i]), powers[w], ndirs);
   }

   /* Get sorted order of applied DFT waves based on normalized power */
   if((ret = sort_dft_waves(wis, powmaxs, pownorms, tw-fw)))
      return(ret);

   return(0);
}


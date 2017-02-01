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

      FILE:    MAPS.C
      AUTHOR:  Michael D. Garris
      DATE:    03/16/1999
      UPDATED: 10/04/1999 Version 2 by MDG
      UPDATED: 10/26/1999 by MDG
               To permit margin blocks to be flagged in
               low contrast and low flow maps.
      UPDATED: 03/16/2005 by MDG

      Contains routines responsible for computing various block-based
      maps (including directional ridge flow maps) from an arbitrarily-
      sized image as part of the NIST Latent Fingerprint System (LFS).

***********************************************************************
               ROUTINES:
                        gen_image_maps()
                        gen_initial_maps()
                        interpolate_direction_map()
                        morph_TF_map()
                        pixelize_map()
                        smooth_direction_map()
                        gen_high_curve_map()
                        gen_initial_imap()
                        primary_dir_test()
                        secondary_fork_test()
                        remove_incon_dirs()
                        test_top_edge()
                        test_right_edge()
                        test_bottom_edge()
                        test_left_edge()
                        remove_dir()
                        average_8nbr_dir()
                        num_valid_8nbrs()
                        smooth_imap()
                        vorticity()
                        accum_nbr_vorticity()
                        curvature()

***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lfs.h>
#include <morph.h>
#include <log.h>

/*************************************************************************
**************************************************************************
#cat: gen_image_maps - Computes a set of image maps based on Version 2
#cat:            of the NIST LFS System.  The first map is a Direction Map
#cat:            which is a 2D vector of integer directions, where each
#cat:            direction represents the dominant ridge flow in a block of
#cat:            the input grayscale image.  The Low Contrast Map flags
#cat:            blocks with insufficient contrast.  The Low Flow Map flags
#cat:            blocks with insufficient ridge flow.  The High Curve Map
#cat:            flags blocks containing high curvature. This routine will
#cat:            generate maps for an arbitrarily sized, non-square, image.

   Input:
      pdata     - padded input image data (8 bits [0..256) grayscale)
      pw        - padded width (in pixels) of the input image
      ph        - padded height (in pixels) of the input image
      dir2rad   - lookup table for converting integer directions
      dftwaves  - structure containing the DFT wave forms
      dftgrids  - structure containing the rotated pixel grid offsets
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      odmap     - points to the created Direction Map
      olcmap    - points to the created Low Contrast Map
      olfmap    - points to the Low Ridge Flow Map
      ohcmap    - points to the High Curvature Map
      omw       - width (in blocks) of the maps
      omh       - height (in blocks) of the maps
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
int gen_image_maps(int **odmap, int **olcmap, int **olfmap, int **ohcmap,
              int *omw, int *omh,
              unsigned char *pdata, const int pw, const int ph,
              const DIR2RAD *dir2rad, const DFTWAVES *dftwaves,
              const ROTGRIDS *dftgrids, const LFSPARMS *lfsparms)
{
   int *direction_map, *low_contrast_map, *low_flow_map, *high_curve_map;
   int mw, mh, iw, ih;
   int *blkoffs;
   int ret; /* return code */

   /* 1. Compute block offsets for the entire image, accounting for pad */
   /* Block_offsets() assumes square block (grid), so ERROR otherwise. */
   if(dftgrids->grid_w != dftgrids->grid_h){
      fprintf(stderr,
              "ERROR : gen_image_maps : DFT grids must be square\n");
      return(-540);
   }
   /* Compute unpadded image dimensions. */
   iw = pw - (dftgrids->pad<<1);
   ih = ph - (dftgrids->pad<<1);
   if((ret = block_offsets(&blkoffs, &mw, &mh, iw, ih,
                        dftgrids->pad, lfsparms->blocksize))){
      return(ret);
   }

   /* 2. Generate initial Direction Map and Low Contrast Map*/
   if((ret = gen_initial_maps(&direction_map, &low_contrast_map,
                              &low_flow_map, blkoffs, mw, mh,
                              pdata, pw, ph, dftwaves, dftgrids, lfsparms))){
      /* Free memory allocated to this point. */
      free(blkoffs);
      return(ret);
   }

   if((ret = morph_TF_map(low_flow_map, mw, mh, lfsparms))){
      return(ret);
   }

   /* 3. Remove directions that are inconsistent with neighbors */
   remove_incon_dirs(direction_map, mw, mh, dir2rad, lfsparms);


   /* 4. Smooth Direction Map values with their neighbors */
   smooth_direction_map(direction_map, low_contrast_map, mw, mh,
                           dir2rad, lfsparms);

   /* 5. Interpolate INVALID direction blocks with their valid neighbors. */
   if((ret = interpolate_direction_map(direction_map, low_contrast_map,
                                       mw, mh, lfsparms))){
      return(ret);
   }

   /* May be able to skip steps 6 and/or 7 if computation time */
   /* is a critical factor.                                    */

   /* 6. Remove directions that are inconsistent with neighbors */
   remove_incon_dirs(direction_map, mw, mh, dir2rad, lfsparms);

   /* 7. Smooth Direction Map values with their neighbors. */
   smooth_direction_map(direction_map, low_contrast_map, mw, mh,
                           dir2rad, lfsparms);

   /* 8. Set the Direction Map values in the image margin to INVALID. */
   set_margin_blocks(direction_map, mw, mh, INVALID_DIR);

   /* 9. Generate High Curvature Map from interpolated Direction Map. */
   if((ret = gen_high_curve_map(&high_curve_map, direction_map, mw, mh,
                                lfsparms))){
      return(ret);
   }

   /* Deallocate working memory. */
   free(blkoffs);

   *odmap = direction_map;
   *olcmap = low_contrast_map;
   *olfmap = low_flow_map;
   *ohcmap = high_curve_map;
   *omw = mw;
   *omh = mh;
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: gen_initial_maps - Creates an initial Direction Map from the given
#cat:             input image.  It very important that the image be properly
#cat:             padded so that rotated grids along the boundary of the image
#cat:             do not access unkown memory.  The rotated grids are used by a
#cat:             DFT-based analysis to determine the integer directions
#cat:             in the map. Typically this initial vector of directions will
#cat:             subsequently have weak or inconsistent directions removed
#cat:             followed by a smoothing process.  The resulting Direction
#cat:             Map contains valid directions >= 0 and INVALID values = -1.
#cat:             This routine also computes and returns 2 other image maps.
#cat:             The Low Contrast Map flags blocks in the image with
#cat:             insufficient contrast.  Blocks with low contrast have a
#cat:             corresponding direction of INVALID in the Direction Map.
#cat:             The Low Flow Map flags blocks in which the DFT analyses
#cat:             could not determine a significant ridge flow.  Blocks with
#cat:             low ridge flow also have a corresponding direction of
#cat:             INVALID in the Direction Map.

   Input:
      blkoffs   - offsets to the pixel origin of each block in the padded image
      mw        - number of blocks horizontally in the padded input image
      mh        - number of blocks vertically in the padded input image
      pdata     - padded input image data (8 bits [0..256) grayscale)
      pw        - width (in pixels) of the padded input image
      ph        - height (in pixels) of the padded input image
      dftwaves  - structure containing the DFT wave forms
      dftgrids  - structure containing the rotated pixel grid offsets
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      odmap     - points to the newly created Direction Map
      olcmap    - points to the newly created Low Contrast Map
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
int gen_initial_maps(int **odmap, int **olcmap, int **olfmap,
                int *blkoffs, const int mw, const int mh,
                unsigned char *pdata, const int pw, const int ph,
                const DFTWAVES *dftwaves, const  ROTGRIDS *dftgrids,
                const LFSPARMS *lfsparms)
{
   int *direction_map, *low_contrast_map, *low_flow_map;
   int bi, bsize, blkdir;
   int *wis, *powmax_dirs;
   double **powers, *powmaxs, *pownorms;
   int nstats;
   int ret; /* return code */
   int dft_offset;
   int xminlimit, xmaxlimit, yminlimit, ymaxlimit;
   int win_x, win_y, low_contrast_offset;

   print2log("INITIAL MAP\n");

   /* Compute total number of blocks in map */
   bsize = mw * mh;

   /* Allocate Direction Map memory */
   direction_map = (int *)malloc(bsize * sizeof(int));
   if(direction_map == (int *)NULL){
      fprintf(stderr,
              "ERROR : gen_initial_maps : malloc : direction_map\n");
      return(-550);
   }
   /* Initialize the Direction Map to INVALID (-1). */
   memset(direction_map, INVALID_DIR, bsize * sizeof(int));

   /* Allocate Low Contrast Map memory */
   low_contrast_map = (int *)malloc(bsize * sizeof(int));
   if(low_contrast_map == (int *)NULL){
      free(direction_map);
      fprintf(stderr,
              "ERROR : gen_initial_maps : malloc : low_contrast_map\n");
      return(-551);
   }
   /* Initialize the Low Contrast Map to FALSE (0). */
   memset(low_contrast_map, 0, bsize * sizeof(int));

   /* Allocate Low Ridge Flow Map memory */
   low_flow_map = (int *)malloc(bsize * sizeof(int));
   if(low_flow_map == (int *)NULL){
      free(direction_map);
      free(low_contrast_map);
      fprintf(stderr,
              "ERROR : gen_initial_maps : malloc : low_flow_map\n");
      return(-552);
   }
   /* Initialize the Low Flow Map to FALSE (0). */
   memset(low_flow_map, 0, bsize * sizeof(int));

   /* Allocate DFT directional power vectors */
   if((ret = alloc_dir_powers(&powers, dftwaves->nwaves, dftgrids->ngrids))){
      /* Free memory allocated to this point. */
      free(direction_map);
      free(low_contrast_map);
      free(low_flow_map);
      return(ret);
   }

   /* Allocate DFT power statistic arrays */
   /* Compute length of statistics arrays.  Statistics not needed   */
   /* for the first DFT wave, so the length is number of waves - 1. */
   nstats = dftwaves->nwaves - 1;
   if((ret = alloc_power_stats(&wis, &powmaxs, &powmax_dirs,
                            &pownorms, nstats))){
      /* Free memory allocated to this point. */
      free(direction_map);
      free(low_contrast_map);
      free(low_flow_map);
      free_dir_powers(powers, dftwaves->nwaves);
      return(ret);
   }

   /* Compute special window origin limits for determining low contrast.  */
   /* These pixel limits avoid analyzing the padded borders of the image. */
   xminlimit = dftgrids->pad;
   yminlimit = dftgrids->pad;
   xmaxlimit = pw - dftgrids->pad - lfsparms->windowsize - 1;
   ymaxlimit = ph - dftgrids->pad - lfsparms->windowsize - 1;

   /* max limits should not be negative */
   xmaxlimit = MAX(xmaxlimit, 0);
   ymaxlimit = MAX(ymaxlimit, 0);

   /* Foreach block in image ... */
   for(bi = 0; bi < bsize; bi++){
      /* Adjust block offset from pointing to block origin to pointing */
      /* to surrounding window origin.                                 */
      dft_offset = blkoffs[bi] - (lfsparms->windowoffset * pw) -
                      lfsparms->windowoffset;

      /* Compute pixel coords of window origin. */
      win_x = dft_offset % pw;
      win_y = (int)(dft_offset / pw);

      /* Make sure the current window does not access padded image pixels */
      /* for analyzing low contrast.                                      */
      win_x = max(xminlimit, win_x);
      win_x = min(xmaxlimit, win_x);
      win_y = max(yminlimit, win_y);
      win_y = min(ymaxlimit, win_y);
      low_contrast_offset = (win_y * pw) + win_x;

      print2log("   BLOCK %2d (%2d, %2d) ", bi, bi%mw, bi/mw);

      /* If block is low contrast ... */
      if((ret = low_contrast_block(low_contrast_offset, lfsparms->windowsize,
                                  pdata, pw, ph, lfsparms))){
         /* If system error ... */
         if(ret < 0){
            free(direction_map);
            free(low_contrast_map);
            free(low_flow_map);
            free_dir_powers(powers, dftwaves->nwaves);
            free(wis);
            free(powmaxs);
            free(powmax_dirs);
            free(pownorms);
            return(ret);
         }

         /* Otherwise, block is low contrast ... */
         print2log("LOW CONTRAST\n");
         low_contrast_map[bi] = TRUE;
         /* Direction Map's block is already set to INVALID. */
      }
      /* Otherwise, sufficient contrast for DFT processing ... */
      else {
         print2log("\n");

         /* Compute DFT powers */
         if((ret = dft_dir_powers(powers, pdata, low_contrast_offset, pw, ph,
                               dftwaves, dftgrids))){
            /* Free memory allocated to this point. */
            free(direction_map);
            free(low_contrast_map);
            free(low_flow_map);
            free_dir_powers(powers, dftwaves->nwaves);
            free(wis);
            free(powmaxs);
            free(powmax_dirs);
            free(pownorms);
            return(ret);
         }

         /* Compute DFT power statistics, skipping first applied DFT  */
         /* wave.  This is dependent on how the primary and secondary */
         /* direction tests work below.                               */
         if((ret = dft_power_stats(wis, powmaxs, powmax_dirs, pownorms, powers,
                                1, dftwaves->nwaves, dftgrids->ngrids))){
            /* Free memory allocated to this point. */
            free(direction_map);
            free(low_contrast_map);
            free(low_flow_map);
            free_dir_powers(powers, dftwaves->nwaves);
            free(wis);
            free(powmaxs);
            free(powmax_dirs);
            free(pownorms);
            return(ret);
         }

#ifdef LOG_REPORT /*vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/
         {  int _w;
            fprintf(logfp, "      Power\n");
            for(_w = 0; _w < nstats; _w++){
               /* Add 1 to wis[w] to create index to original dft_coefs[] */
               fprintf(logfp, "         wis[%d] %d %12.3f %2d %9.3f %12.3f\n",
                    _w, wis[_w]+1, 
                    powmaxs[wis[_w]], powmax_dirs[wis[_w]], pownorms[wis[_w]],
                    powers[0][powmax_dirs[wis[_w]]]);
            }
         }
#endif /*^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

         /* Conduct primary direction test */
         blkdir = primary_dir_test(powers, wis, powmaxs, powmax_dirs,
                                  pownorms, nstats, lfsparms);

         if(blkdir != INVALID_DIR)
            direction_map[bi] = blkdir;
         else{
            /* Conduct secondary (fork) direction test */
            blkdir = secondary_fork_test(powers, wis, powmaxs, powmax_dirs,
                                  pownorms, nstats, lfsparms);
            if(blkdir != INVALID_DIR)
               direction_map[bi] = blkdir;
            /* Otherwise current direction in Direction Map remains INVALID */
            else
               /* Flag the block as having LOW RIDGE FLOW. */
               low_flow_map[bi] = TRUE;
         }

      } /* End DFT */
   } /* bi */

   /* Deallocate working memory */
   free_dir_powers(powers, dftwaves->nwaves);
   free(wis);
   free(powmaxs);
   free(powmax_dirs);
   free(pownorms);

   *odmap = direction_map;
   *olcmap = low_contrast_map;
   *olfmap = low_flow_map;
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: interpolate_direction_map - Take a Direction Map and Low Contrast
#cat:             Map and attempts to fill in INVALID directions in the
#cat:             Direction Map based on a blocks valid neighbors.  The
#cat:             valid neighboring directions are combined in a weighted
#cat:             average inversely proportional to their distance from
#cat:             the block being interpolated.  Low Contrast blocks are
#cat:             used to prempt the search for a valid neighbor in a
#cat:             specific direction, which keeps the process from
#cat:             interpolating directions for blocks in the background and
#cat:             and perimeter of the fingerprint in the image.

   Input:
      direction_map    - map of blocks containing directional ridge flow
      low_contrast_map - map of blocks flagged as LOW CONTRAST
      mw        - number of blocks horizontally in the maps
      mh        - number of blocks vertically in the maps
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      direction_map - contains the newly interpolated results
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
int interpolate_direction_map(int *direction_map, int *low_contrast_map,
                      const int mw, const int mh, const LFSPARMS *lfsparms)
{
   int x, y, new_dir;
   int n_dir, e_dir, s_dir, w_dir;
   int n_dist = 0, e_dist = 0, s_dist = 0, w_dist = 0, total_dist;
   int n_found, e_found, s_found, w_found, total_found;
   int n_delta = 0, e_delta = 0, s_delta = 0, w_delta = 0, total_delta;
   int nbr_x, nbr_y;
   int *omap, *dptr, *cptr, *optr;
   double avr_dir;

   print2log("INTERPOLATE DIRECTION MAP\n");

   /* Allocate output (interpolated) Direction Map. */
   omap = (int *)malloc(mw*mh*sizeof(int));
   if(omap == (int *)NULL){
      fprintf(stderr,
              "ERROR : interpolate_direction_map : malloc : omap\n");
      return(-520);
   }

   /* Set pointers to the first block in the maps. */
   dptr = direction_map;
   cptr = low_contrast_map;
   optr = omap;

   /* Foreach block in the maps ... */
   for(y = 0; y < mh; y++){
      for(x = 0; x < mw; x++){

         /* If image block is NOT LOW CONTRAST and has INVALID direction ... */
         if((!*cptr) && (*dptr == INVALID_DIR)){

            /* Set neighbor accumulators to 0. */
            total_found = 0;
            total_dist = 0;

            /* Find north neighbor. */
            if((n_found = find_valid_block(&n_dir, &nbr_x, &nbr_y,
                                            direction_map, low_contrast_map,
                                            x, y, mw, mh, 0, -1)) == FOUND){
               /* Compute north distance. */
               n_dist = y - nbr_y;
               /* Accumulate neighbor distance. */
               total_dist += n_dist;
               /* Bump number of neighbors found. */
               total_found++;
            }

            /* Find east neighbor. */
            if((e_found = find_valid_block(&e_dir, &nbr_x, &nbr_y,
                                            direction_map, low_contrast_map,
                                            x, y, mw, mh, 1, 0)) == FOUND){
               /* Compute east distance. */
               e_dist = nbr_x - x;
               /* Accumulate neighbor distance. */
               total_dist += e_dist;
               /* Bump number of neighbors found. */
               total_found++;
            }

            /* Find south neighbor. */
            if((s_found = find_valid_block(&s_dir, &nbr_x, &nbr_y,
                                            direction_map, low_contrast_map,
                                            x, y, mw, mh, 0, 1)) == FOUND){
               /* Compute south distance. */
               s_dist = nbr_y - y;
               /* Accumulate neighbor distance. */
               total_dist += s_dist;
               /* Bump number of neighbors found. */
               total_found++;
            }

            /* Find west neighbor. */
            if((w_found = find_valid_block(&w_dir, &nbr_x, &nbr_y,
                                            direction_map, low_contrast_map,
                                            x, y, mw, mh, -1, 0)) == FOUND){
               /* Compute west distance. */
               w_dist = x - nbr_x;
               /* Accumulate neighbor distance. */
               total_dist += w_dist;
               /* Bump number of neighbors found. */
               total_found++;
            }

            /* If a sufficient number of neighbors found (Ex. 2) ... */
            if(total_found >= lfsparms->min_interpolate_nbrs){

               /* Accumulate weighted sum of neighboring directions     */
               /* inversely related to the distance from current block. */
               total_delta = 0.0;
               /* If neighbor found to the north ... */
               if(n_found){
                  n_delta = total_dist - n_dist;
                  total_delta += n_delta;
               }
               /* If neighbor found to the east ... */
               if(e_found){
                  e_delta = total_dist - e_dist;
                  total_delta += e_delta;
               }
               /* If neighbor found to the south ... */
               if(s_found){
                  s_delta = total_dist - s_dist;
                  total_delta += s_delta;
               }
               /* If neighbor found to the west ... */
               if(w_found){
                  w_delta = total_dist - w_dist;
                  total_delta += w_delta;
               }

               avr_dir = 0.0;

               if(n_found){
                  avr_dir += (n_dir*(n_delta/(double)total_delta));
               }
               if(e_found){
                  avr_dir += (e_dir*(e_delta/(double)total_delta));
               }
               if(s_found){
                  avr_dir += (s_dir*(s_delta/(double)total_delta));
               }
               if(w_found){
                  avr_dir += (w_dir*(w_delta/(double)total_delta));
               }

               /* Need to truncate precision so that answers are consistent  */
               /* on different computer architectures when rounding doubles. */
               avr_dir = trunc_dbl_precision(avr_dir, TRUNC_SCALE);

               /* Assign interpolated direction to output Direction Map. */
               new_dir = sround(avr_dir);

               print2log("   Block %d,%d INTERP numnbs=%d newdir=%d\n",
                       x, y, total_found, new_dir);

               *optr = new_dir;
            }
            else{
               /* Otherwise, the direction remains INVALID. */
               *optr = *dptr;
            }
         }
         else{
            /* Otherwise, assign the current direction to the output block. */
            *optr = *dptr;
         }

         /* Bump to the next block in the maps ... */
         dptr++;
         cptr++;
         optr++;
      }
   }

   /* Copy the interpolated directions into the input map. */
   memcpy(direction_map, omap, mw*mh*sizeof(int));
   /* Deallocate the working memory. */
   free(omap);

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: morph_tf_map - Takes a 2D vector of TRUE and FALSE values integers
#cat:               and dialates and erodes the map in an attempt to fill
#cat:               in voids in the map.

   Input:
      tfmap    - vector of integer block values
      mw       - width (in blocks) of the map
      mh       - height (in blocks) of the map
      lfsparms - parameters and thresholds for controlling LFS
   Output:
      tfmap    - resulting morphed map
**************************************************************************/
int morph_TF_map(int *tfmap, const int mw, const int mh,
                 const LFSPARMS *lfsparms)
{
   unsigned char *cimage, *mimage, *cptr;
   int *mptr;
   int i;
   

   /* Convert TRUE/FALSE map into a binary byte image. */
   cimage = (unsigned char *)malloc(mw*mh);
   if(cimage == (unsigned char *)NULL){
      fprintf(stderr, "ERROR : morph_TF_map : malloc : cimage\n");
      return(-660);
   }

   mimage = (unsigned char *)malloc(mw*mh);
   if(mimage == (unsigned char *)NULL){
      fprintf(stderr, "ERROR : morph_TF_map : malloc : mimage\n");
      return(-661);
   }

   cptr = cimage;
   mptr = tfmap;
   for(i = 0; i < mw*mh; i++){
      *cptr++ = *mptr++;
   }

   dilate_charimage_2(cimage, mimage, mw, mh);
   dilate_charimage_2(mimage, cimage, mw, mh);
   erode_charimage_2(cimage, mimage, mw, mh);
   erode_charimage_2(mimage, cimage, mw, mh);

   cptr = cimage;
   mptr = tfmap;
   for(i = 0; i < mw*mh; i++){
      *mptr++ = *cptr++;
   }

   free(cimage);
   free(mimage);

   return(0);
}

/*************************************************************************
**************************************************************************
#cat: pixelize_map - Takes a block image map and assigns each pixel in the
#cat:            image its corresponding block value.  This allows block
#cat:            values in maps to be directly accessed via pixel addresses.

   Input:
      iw        - the width (in pixels) of the corresponding image
      ih        - the height (in pixels) of the corresponding image
      imap      - input block image map
      mw        - the width (in blocks) of the map
      mh        - the height (in blocks) of the map
      blocksize - the dimension (in pixels) of each block
   Output:
      omap      - points to the resulting pixelized map
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
int pixelize_map(int **omap, const int iw, const int ih,
                  int *imap, const int mw, const int mh, const int blocksize)
{
   int *pmap;
   int ret, x, y;
   int *blkoffs, bw, bh, bi;
   int *spptr, *pptr;

   pmap = (int *)malloc(iw*ih*sizeof(int));
   if(pmap == (int *)NULL){
      fprintf(stderr, "ERROR : pixelize_map : malloc : pmap\n");
      return(-590);
   }

   if((ret = block_offsets(&blkoffs, &bw, &bh, iw, ih, 0, blocksize))){
      return(ret);
   }

   if((bw != mw) || (bh != mh)){
      free(blkoffs);
      fprintf(stderr,
         "ERROR : pixelize_map : block dimensions do not match\n");
      return(-591);
   }

   for(bi = 0; bi < mw*mh; bi++){
      spptr = pmap + blkoffs[bi];
      for(y = 0; y < blocksize; y++){
         pptr = spptr;
         for(x = 0; x < blocksize; x++){
            *pptr++ = imap[bi];
         }
         spptr += iw;
      }
   }

   /* Deallocate working memory. */
   free(blkoffs);
   /* Assign pixelized map to output pointer. */
   *omap = pmap;

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: smooth_direction_map - Takes a vector of integer directions and smooths
#cat:               them by analyzing the direction of adjacent neighbors.

   Input:
      direction_map - vector of integer block values
      mw        - width (in blocks) of the map
      mh        - height (in blocks) of the map
      dir2rad   - lookup table for converting integer directions
      lfsparms - parameters and thresholds for controlling LFS
   Output:
      imap      - vector of smoothed input values
**************************************************************************/
void smooth_direction_map(int *direction_map, int *low_contrast_map,
                 const int mw, const int mh,
                 const DIR2RAD *dir2rad, const LFSPARMS *lfsparms)
{
   int mx, my;
   int *dptr, *cptr;
   int avrdir, nvalid;
   double dir_strength;

   print2log("SMOOTH DIRECTION MAP\n");

   /* Assign pointers to beginning of both maps. */
   dptr = direction_map;
   cptr = low_contrast_map;

   /* Foreach block in maps ... */
   for(my = 0; my < mh; my++){
      for(mx = 0; mx < mw; mx++){
         /* If the current block does NOT have LOW CONTRAST ... */
         if(!*cptr){

            /* Compute average direction from neighbors, returning the */
            /* number of valid neighbors used in the computation, and  */
            /* the "strength" of the average direction.                */
            average_8nbr_dir(&avrdir, &dir_strength, &nvalid,
                             direction_map, mx, my, mw, mh, dir2rad);

            /* If average direction strength is strong enough */
            /*    (Ex. thresh==0.2)...                        */
            if(dir_strength >= lfsparms->dir_strength_min){
               /* If Direction Map direction is valid ... */
               if(*dptr != INVALID_DIR){
                  /* Conduct valid neighbor test (Ex. thresh==3)... */
                  if(nvalid >= lfsparms->rmv_valid_nbr_min){

#ifdef LOG_REPORT /*vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/
                     fprintf(logfp, "   BLOCK %2d (%2d, %2d)\n",
                             mx+(my*mw), mx, my);
                     fprintf(logfp, "      Average NBR :   %2d %6.3f %d\n",
                             avrdir, dir_strength, nvalid);
                     fprintf(logfp, "      1. Valid NBR (%d >= %d)\n",
                             nvalid, lfsparms->rmv_valid_nbr_min);
                     fprintf(logfp, "      Valid Direction = %d\n", *dptr);
                     fprintf(logfp, "      Smoothed Direction = %d\n", avrdir);
#endif /*^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

                     /* Reassign valid direction with average direction. */
                     *dptr = avrdir;
                  }
               }
               /* Otherwise direction is invalid ... */
               else{
                  /* Even if DIRECTION_MAP value is invalid, if number of */
                  /* valid neighbors is big enough (Ex. thresh==7)...     */
                  if(nvalid >= lfsparms->smth_valid_nbr_min){

#ifdef LOG_REPORT /*vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/
                     fprintf(logfp, "   BLOCK %2d (%2d, %2d)\n",
                             mx+(my*mw), mx, my);
                     fprintf(logfp, "      Average NBR :   %2d %6.3f %d\n",
                             avrdir, dir_strength, nvalid);
                     fprintf(logfp, "      2. Invalid NBR (%d >= %d)\n",
                             nvalid, lfsparms->smth_valid_nbr_min);
                     fprintf(logfp, "      Invalid Direction = %d\n", *dptr);
                     fprintf(logfp, "      Smoothed Direction = %d\n", avrdir);
#endif /*^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

                     /* Assign invalid direction with average direction. */
                     *dptr = avrdir;
                  }
               }
            }
         }
         /* Otherwise, block has LOW CONTRAST, so keep INVALID direction. */

         /* Bump to next block in maps. */
         dptr++;
         cptr++;
      }
   }
}

/*************************************************************************
**************************************************************************
#cat: gen_high_curve_map - Takes a Direction Map and generates a new map
#cat:            that flags blocks with HIGH CURVATURE.

   Input:
      direction_map - map of blocks containing directional ridge flow
      mw        - the width (in blocks) of the map
      mh        - the height (in blocks) of the map
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      ohcmap    - points to the created High Curvature Map
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
int gen_high_curve_map(int **ohcmap, int *direction_map,
                   const int mw, const int mh, const LFSPARMS *lfsparms)
{
   int *high_curve_map, mapsize;
   int *hptr, *dptr;
   int bx, by;
   int nvalid, cmeasure, vmeasure;

   mapsize = mw*mh;

   /* Allocate High Curvature Map. */
   high_curve_map = (int *)malloc(mapsize * sizeof(int));
   if(high_curve_map == (int *)NULL){
      fprintf(stderr,
              "ERROR: gen_high_curve_map : malloc : high_curve_map\n");
      return(-530);
   }
   /* Initialize High Curvature Map to FALSE (0). */
   memset(high_curve_map, 0, mapsize*sizeof(int));

   hptr = high_curve_map;
   dptr = direction_map;

   /* Foreach row in maps ... */
   for(by = 0; by < mh; by++){
      /* Foreach column in maps ... */
      for(bx = 0; bx < mw; bx++){

         /* Count number of valid neighbors around current block ... */
         nvalid = num_valid_8nbrs(direction_map, bx, by, mw, mh);

         /* If valid neighbors exist ... */
         if(nvalid > 0){
            /* If current block's direction is INVALID ... */
            if(*dptr == INVALID_DIR){
               /* If a sufficient number of VALID neighbors exists ... */
               if(nvalid >= lfsparms->vort_valid_nbr_min){
                  /* Measure vorticity of neighbors. */
                  vmeasure = vorticity(direction_map, bx, by, mw, mh,
                                       lfsparms->num_directions);
                  /* If vorticity is sufficiently high ... */
                  if(vmeasure >= lfsparms->highcurv_vorticity_min)
                     /* Flag block as HIGH CURVATURE. */
                     *hptr = TRUE;
               }
            }
            /* Otherwise block has valid direction ... */
            else{
               /* Measure curvature around the valid block. */
               cmeasure = curvature(direction_map, bx, by, mw, mh,
                                    lfsparms->num_directions);
               /* If curvature is sufficiently high ... */
               if(cmeasure >= lfsparms->highcurv_curvature_min)
                  *hptr = TRUE;
            }
         } /* Else (nvalid <= 0) */

         /* Bump pointers to next block in maps. */
         dptr++;
         hptr++;

      } /* bx */
   } /* by */

   /* Assign High Curvature Map to output pointer. */         
   *ohcmap = high_curve_map;

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: gen_initial_imap - Creates an initial IMAP from the given input image.
#cat:             It very important that the image be properly padded so
#cat:             that rotated grids along the boudary of the image do not
#cat:             access unkown memory.  The rotated grids are used by a
#cat:             DFT-based analysis to determine the integer directions
#cat:             in the IMAP. Typically this initial vector of directions will
#cat:             subsequently have weak or inconsistent directions removed
#cat:             followed by a smoothing process.

   Input:
      blkoffs   - offsets to the pixel origin of each block in the padded image
      mw        - number of blocks horizontally in the padded input image
      mh        - number of blocks vertically in the padded input image
      pdata     - padded input image data (8 bits [0..256) grayscale)
      pw        - width (in pixels) of the padded input image
      ph        - height (in pixels) of the padded input image
      dftwaves  - structure containing the DFT wave forms
      dftgrids  - structure containing the rotated pixel grid offsets
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      optr      - points to the newly created IMAP
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
int gen_initial_imap(int **optr, int *blkoffs, const int mw, const int mh,
                unsigned char *pdata, const int pw, const int ph,
                const DFTWAVES *dftwaves, const  ROTGRIDS *dftgrids,
                const LFSPARMS *lfsparms)
{
   int *imap;
   int bi, bsize, blkdir;
   int *wis, *powmax_dirs;
   double **powers, *powmaxs, *pownorms;
   int nstats;
   int ret; /* return code */

   print2log("INITIAL MAP\n");

   /* Compute total number of blocks in IMAP */
   bsize = mw * mh;

   /* Allocate IMAP memory */
   imap = (int *)malloc(bsize * sizeof(int));
   if(imap == (int *)NULL){
      fprintf(stderr, "ERROR : gen_initial_imap : malloc : imap\n");
      return(-70);
   }

   /* Allocate DFT directional power vectors */
   if((ret = alloc_dir_powers(&powers, dftwaves->nwaves, dftgrids->ngrids))){
      /* Free memory allocated to this point. */
      free(imap);
      return(ret);
   }

   /* Allocate DFT power statistic arrays */
   /* Compute length of statistics arrays.  Statistics not needed   */
   /* for the first DFT wave, so the length is number of waves - 1. */
   nstats = dftwaves->nwaves - 1;
   if((ret = alloc_power_stats(&wis, &powmaxs, &powmax_dirs,
                            &pownorms, nstats))){
      /* Free memory allocated to this point. */
      free(imap);
      free_dir_powers(powers, dftwaves->nwaves);
      return(ret);
   }

   /* Initialize the imap to -1 */
   memset(imap, INVALID_DIR, bsize * sizeof(int));

   /* Foreach block in imap ... */
   for(bi = 0; bi < bsize; bi++){

      print2log("   BLOCK %2d (%2d, %2d)\n", bi, bi%mw, bi/mw);

      /* Compute DFT powers */
      if((ret = dft_dir_powers(powers, pdata, blkoffs[bi], pw, ph,
                            dftwaves, dftgrids))){
         /* Free memory allocated to this point. */
         free(imap);
         free_dir_powers(powers, dftwaves->nwaves);
         free(wis);
         free(powmaxs);
         free(powmax_dirs);
         free(pownorms);
         return(ret);
      }

      /* Compute DFT power statistics, skipping first applied DFT  */
      /* wave.  This is dependent on how the primary and secondary */
      /* direction tests work below.                               */
      if((ret = dft_power_stats(wis, powmaxs, powmax_dirs, pownorms, powers,
                      1, dftwaves->nwaves, dftgrids->ngrids))){
         /* Free memory allocated to this point. */
         free(imap);
         free_dir_powers(powers, dftwaves->nwaves);
         free(wis);
         free(powmaxs);
         free(powmax_dirs);
         free(pownorms);
         return(ret);
      }

#ifdef LOG_REPORT /*vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/
      {  int _w;
         fprintf(logfp, "      Power\n");
         for(_w = 0; _w < nstats; _w++){
            /* Add 1 to wis[w] to create index to original dft_coefs[] */
            fprintf(logfp, "         wis[%d] %d %12.3f %2d %9.3f %12.3f\n",
                    _w, wis[_w]+1, 
                    powmaxs[wis[_w]], powmax_dirs[wis[_w]], pownorms[wis[_w]],
                    powers[0][powmax_dirs[wis[_w]]]);
         }
      }
#endif /*^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

      /* Conduct primary direction test */
      blkdir = primary_dir_test(powers, wis, powmaxs, powmax_dirs,
                               pownorms, nstats, lfsparms);

      if(blkdir != INVALID_DIR)
         imap[bi] = blkdir;
      else{
         /* Conduct secondary (fork) direction test */
         blkdir = secondary_fork_test(powers, wis, powmaxs, powmax_dirs,
                               pownorms, nstats, lfsparms);
         if(blkdir != INVALID_DIR)
            imap[bi] = blkdir;
      }

      /* Otherwise current block direction in IMAP remains INVALID */

   } /* bi */

   /* Deallocate working memory */
   free_dir_powers(powers, dftwaves->nwaves);
   free(wis);
   free(powmaxs);
   free(powmax_dirs);
   free(pownorms);

   *optr = imap;
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: primary_dir_test - Applies the primary set of criteria for selecting
#cat:                    an IMAP integer direction from a set of DFT results
#cat:                    computed from a block of image data

   Input:
      powers      - DFT power computed from each (N) wave frequencies at each
                    rotation direction in the current image block
      wis         - sorted order of the highest N-1 frequency power statistics
      powmaxs     - maximum power for each of the highest N-1 frequencies
      powmax_dirs - directions associated with each of the N-1 maximum powers
      pownorms    - normalized power for each of the highest N-1 frequencies
      nstats      - N-1 wave frequencies (where N is the length of dft_coefs)
      lfsparms    - parameters and thresholds for controlling LFS
   Return Code:
      Zero or Positive - The selected IMAP integer direction
      INVALID_DIR - IMAP Integer direction could not be determined
**************************************************************************/
int primary_dir_test(double **powers, const int *wis,
            const double *powmaxs, const int *powmax_dirs,
            const double *pownorms, const int nstats,
            const LFSPARMS *lfsparms)
{
   int w;

   print2log("      Primary\n");

   /* Look at max power statistics in decreasing order ... */
   for(w = 0; w < nstats; w++){
         /* 1. Test magnitude of current max power (Ex. Thresh==100000)   */
      if((powmaxs[wis[w]] > lfsparms->powmax_min) &&
         /* 2. Test magnitude of normalized max power (Ex. Thresh==3.8)   */
         (pownorms[wis[w]] > lfsparms->pownorm_min) &&
         /* 3. Test magnitude of power of lowest DFT frequency at current */
         /* max power direction and make sure it is not too big.          */
         /* (Ex. Thresh==50000000)                                        */
         (powers[0][powmax_dirs[wis[w]]] <= lfsparms->powmax_max)){

#ifdef LOG_REPORT /*vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/
            /* Add 1 to wis[w] to create index to original dft_coefs[] */
            fprintf(logfp,
                     "         Selected Wave = %d\n", wis[w]+1);
            fprintf(logfp,
                    "         1. Power Magnitude (%12.3f > %12.3f)\n",
                   powmaxs[wis[w]], lfsparms->powmax_min);
            fprintf(logfp,
                    "         2. Norm Power Magnitude (%9.3f > %9.3f)\n",
                    pownorms[wis[w]], lfsparms->pownorm_min);
            fprintf(logfp,
                    "         3. Low Freq Wave Magnitude (%12.3f <= %12.3f)\n",
                    powers[0][powmax_dirs[wis[w]]], lfsparms->powmax_max);
            fprintf(logfp,
                    "         PASSED\n");
            fprintf(logfp,
                    "         Selected Direction = %d\n",
                    powmax_dirs[wis[w]]);
#endif /*^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

            /* If ALL 3 criteria met, return current max power direction. */
            return(powmax_dirs[wis[w]]);

      }
   }

   /* Otherwise test failed. */

#ifdef LOG_REPORT /*vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/
   fprintf(logfp, "         1. Power Magnitude ( > %12.3f)\n",
           lfsparms->powmax_min);
   fprintf(logfp, "         2. Norm Power Magnitude ( > %9.3f)\n",
           lfsparms->pownorm_min);
   fprintf(logfp, "         3. Low Freq Wave Magnitude ( <= %12.3f)\n",
           lfsparms->powmax_max);
   fprintf(logfp, "         FAILED\n");
#endif /*^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

   return(INVALID_DIR);
}

/*************************************************************************
**************************************************************************
#cat: secondary_fork_test - Applies a secondary set of criteria for selecting
#cat:                    an IMAP integer direction from a set of DFT results
#cat:                    computed from a block of image data.  This test
#cat:                    analyzes the strongest power statistics associated
#cat:                    with a given frequency and direction and analyses
#cat:                    small changes in direction to the left and right to
#cat:                    determine if the block contains a "fork".

   Input:
      powers      - DFT power computed from each (N) wave frequencies at each
                    rotation direction in the current image block
      wis         - sorted order of the highest N-1 frequency power statistics
      powmaxs     - maximum power for each of the highest N-1 frequencies
      powmax_dirs - directions associated with each of the N-1 maximum powers
      pownorms    - normalized power for each of the highest N-1 frequencies
      nstats      - N-1 wave frequencies (where N is the length of dft_coefs)
      lfsparms    - parameters and thresholds for controlling LFS
   Return Code:
      Zero or Positive - The selected IMAP integer direction
      INVALID_DIR - IMAP Integer direction could not be determined
**************************************************************************/
int secondary_fork_test(double **powers, const int *wis,
            const double *powmaxs, const int *powmax_dirs,
            const double *pownorms, const int nstats,
            const LFSPARMS *lfsparms)
{
   int ldir, rdir;
   double fork_pownorm_min, fork_pow_thresh;

#ifdef LOG_REPORT
{  int firstpart = 0; /* Flag to determine if passed 1st part ... */
   fprintf(logfp, "      Secondary\n");
#endif

   /* Relax the normalized power threshold under fork conditions. */
   fork_pownorm_min = lfsparms->fork_pct_pownorm * lfsparms->pownorm_min;

      /* 1. Test magnitude of largest max power (Ex. Thresh==100000)   */
   if((powmaxs[wis[0]] > lfsparms->powmax_min) &&
      /* 2. Test magnitude of corresponding normalized power           */
      /*    (Ex. Thresh==2.85)                                         */
      (pownorms[wis[0]] >= fork_pownorm_min) &&
      /* 3. Test magnitude of power of lowest DFT frequency at largest */
      /* max power direction and make sure it is not too big.          */
      /* (Ex. Thresh==50000000)                                        */
      (powers[0][powmax_dirs[wis[0]]] <= lfsparms->powmax_max)){

#ifdef LOG_REPORT /*vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/
      /* First part passed ... */
      firstpart = 1;
      fprintf(logfp,
              "         Selected Wave = %d\n", wis[0]+1);
      fprintf(logfp,
              "         1. Power Magnitude (%12.3f > %12.3f)\n",
             powmaxs[wis[0]], lfsparms->powmax_min);
      fprintf(logfp,
              "         2. Norm Power Magnitude (%9.3f >= %9.3f)\n",
             pownorms[wis[0]], fork_pownorm_min);
      fprintf(logfp,
              "         3. Low Freq Wave Magnitude (%12.3f <= %12.3f)\n",
              powers[0][powmax_dirs[wis[0]]], lfsparms->powmax_max);
#endif /*^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

      /* Add FORK_INTERVALs to current direction modulo NDIRS */
      rdir = (powmax_dirs[wis[0]] + lfsparms->fork_interval) %
                lfsparms->num_directions;

      /* Subtract FORK_INTERVALs from direction modulo NDIRS  */
      /* For example, FORK_INTERVAL==2 & NDIRS==16, then      */
      /*            ldir = (dir - (16-2)) % 16                */
      /* which keeps result in proper modulo range.           */
      ldir = (powmax_dirs[wis[0]] + lfsparms->num_directions -
                 lfsparms->fork_interval) % lfsparms->num_directions;

      print2log("         Left = %d, Current = %d, Right = %d\n",
              ldir, powmax_dirs[wis[0]], rdir);

      /* Set forked angle threshold to be a % of the max directional */
      /* power. (Ex. thresh==0.7*powmax)                             */
      fork_pow_thresh = powmaxs[wis[0]] * lfsparms->fork_pct_powmax;

      /* Look up and test the computed power for the left and right    */
      /* fork directions.s                                             */
      /* The power stats (and thus wis) are on the range [0..nwaves-1) */
      /* as the statistics for the first DFT wave are not included.    */
      /* The original power vectors exist for ALL DFT waves, therefore */
      /* wis indices must be added by 1 before addressing the original */
      /* powers vector.                                                */
      /* LFS permits one and only one of the fork angles to exceed     */
      /* the relative power threshold.                                 */
      if(((powers[wis[0]+1][ldir] <= fork_pow_thresh) ||
          (powers[wis[0]+1][rdir] <= fork_pow_thresh)) &&
         ((powers[wis[0]+1][ldir] > fork_pow_thresh) ||
          (powers[wis[0]+1][rdir] > fork_pow_thresh))){

#ifdef LOG_REPORT /*vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/
         fprintf(logfp,
                 "         4. Left Power Magnitude (%12.3f > %12.3f)\n",
                 powers[wis[0]+1][ldir], fork_pow_thresh);
         fprintf(logfp,
                 "         5. Right Power Magnitude (%12.3f > %12.3f)\n",
                 powers[wis[0]+1][rdir], fork_pow_thresh);
         fprintf(logfp, "         PASSED\n");
         fprintf(logfp,
                 "         Selected Direction = %d\n", powmax_dirs[wis[0]]);
#endif /*^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

         /* If ALL the above criteria hold, then return the direction */
         /* of the largest max power.                                 */
         return(powmax_dirs[wis[0]]);
      }
   }

   /* Otherwise test failed. */

#ifdef LOG_REPORT /*vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/
   if(!firstpart){
      fprintf(logfp,
              "         1. Power Magnitude ( > %12.3f)\n",
              lfsparms->powmax_min);
      fprintf(logfp,
              "         2. Norm Power Magnitude ( > %9.3f)\n",
              fork_pownorm_min);
      fprintf(logfp,
              "         3. Low Freq Wave Magnitude ( <= %12.3f)\n",
              lfsparms->powmax_max);
   }
   else{
      fprintf(logfp, "         4. Left Power Magnitude (%12.3f > %12.3f)\n",
              powers[wis[0]+1][ldir], fork_pow_thresh);
      fprintf(logfp, "         5. Right Power Magnitude (%12.3f > %12.3f)\n",
              powers[wis[0]+1][rdir], fork_pow_thresh);
   }
   fprintf(logfp, "         FAILED\n");
} /* Close scope of firstpart */
#endif /*^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

   return(INVALID_DIR);
}

/*************************************************************************
**************************************************************************
#cat: remove_incon_dirs - Takes a vector of integer directions and removes
#cat:              individual directions that are too weak or inconsistent.
#cat:              Directions are tested from the center of the IMAP working
#cat:              outward in concentric squares, and the process resets to
#cat:              the center and continues until no changes take place during
#cat:              a complete pass.

   Input:
      imap      - vector of IMAP integer directions
      mw        - width (in blocks) of the IMAP
      mh        - height (in blocks) of the IMAP
      dir2rad   - lookup table for converting integer directions
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      imap      - vector of pruned input values
**************************************************************************/
void remove_incon_dirs(int *imap, const int mw, const int mh,
             const DIR2RAD *dir2rad, const LFSPARMS *lfsparms)
{
   int cx, cy;
   int *iptr;
   int nremoved;
   int lbox, rbox, tbox, bbox;

#ifdef LOG_REPORT
{  int numpass = 0;
   fprintf(logfp, "REMOVE MAP\n");
#endif

   /* Compute center coords of IMAP */
   cx = mw>>1;
   cy = mh>>1;

   /* Do pass, while directions have been removed in a pass ... */
   do{

#ifdef LOG_REPORT
      /* Count number of complete passes through IMAP */
      ++numpass;
      fprintf(logfp, "   PASS = %d\n", numpass);
#endif

      /* Reinitialize number of removed directions to 0 */
      nremoved = 0;

      /* Start at center */
      iptr = imap + (cy * mw) + cx;
      /* If valid IMAP direction and test for removal is true ... */
      if((*iptr != INVALID_DIR)&&
         (remove_dir(imap, cx, cy, mw, mh, dir2rad, lfsparms))){

         /* Set to INVALID */
         *iptr = INVALID_DIR;
         /* Bump number of removed IMAP directions */
         nremoved++;
      }

      /* Initialize side indices of concentric boxes */
      lbox = cx-1;
      tbox = cy-1;
      rbox = cx+1;
      bbox = cy+1;

      /* Grow concentric boxes, until ALL edges of imap are exceeded */
      while((lbox >= 0) || (rbox < mw) || (tbox >= 0) || (bbox < mh)){

         /* test top edge of box */
         if(tbox >= 0)
            nremoved += test_top_edge(lbox, tbox, rbox, bbox, imap, mw, mh,
                             dir2rad, lfsparms);

         /* test right edge of box */
         if(rbox < mw)
            nremoved += test_right_edge(lbox, tbox, rbox, bbox, imap, mw, mh,
                             dir2rad, lfsparms);

         /* test bottom edge of box */
         if(bbox < mh)
            nremoved += test_bottom_edge(lbox, tbox, rbox, bbox, imap, mw, mh,
                             dir2rad, lfsparms);

         /* test left edge of box */
         if(lbox >=0)
            nremoved += test_left_edge(lbox, tbox, rbox, bbox, imap, mw, mh,
                             dir2rad, lfsparms);

         /* Resize current box */
         lbox--;
         tbox--;
         rbox++;
         bbox++;
      }
   }while(nremoved);

#ifdef LOG_REPORT
} /* Close scope of numpass */
#endif

}

/*************************************************************************
**************************************************************************
#cat: test_top_edge - Walks the top edge of a concentric square in the IMAP,
#cat:                 testing directions along the way to see if they should
#cat:                 be removed due to being too weak or inconsistent with
#cat:                 respect to their adjacent neighbors.

   Input:
      lbox      - left edge of current concentric square
      tbox      - top edge of current concentric square
      rbox      - right edge of current concentric square
      bbox      - bottom edge of current concentric square
      imap      - vector of IMAP integer directions
      mw        - width (in blocks) of the IMAP
      mh        - height (in blocks) of the IMAP
      dir2rad   - lookup table for converting integer directions
      lfsparms  - parameters and thresholds for controlling LFS
   Return Code:
      Positive - direction should be removed from IMAP
      Zero     - direction should NOT be remove from IMAP
**************************************************************************/
int test_top_edge(const int lbox, const int tbox, const int rbox,
                  const int bbox, int *imap, const int mw, const int mh,
                  const DIR2RAD *dir2rad, const LFSPARMS *lfsparms)
{
   int bx, by, sx, ex;
   int *iptr, *sptr, *eptr;
   int nremoved;

   /* Initialize number of directions removed on edge to 0 */
   nremoved = 0;

   /* Set start pointer to top-leftmost point of box, or set it to */
   /* the leftmost point in the IMAP row (0), whichever is larger. */
   sx = max(lbox, 0);
   sptr = imap + (tbox*mw) + sx;

   /* Set end pointer to either 1 point short of the top-rightmost */
   /* point of box, or set it to the rightmost point in the IMAP   */
   /* row (lastx=mw-1), whichever is smaller.                      */
   ex = min(rbox-1, mw-1);
   eptr = imap + (tbox*mw) + ex;

   /* For each point on box's edge ... */
   for(iptr = sptr, bx = sx, by = tbox;
       iptr <= eptr;
       iptr++, bx++){
      /* If valid IMAP direction and test for removal is true ... */
      if((*iptr != INVALID_DIR)&&
         (remove_dir(imap, bx, by, mw, mh, dir2rad, lfsparms))){
         /* Set to INVALID */
         *iptr = INVALID_DIR;
         /* Bump number of removed IMAP directions */
         nremoved++;
      }
   }

   /* Return the number of directions removed on edge */
   return(nremoved);
}

/*************************************************************************
**************************************************************************
#cat: test_right_edge - Walks the right edge of a concentric square in the
#cat:                 IMAP, testing directions along the way to see if they
#cat:                 should be removed due to being too weak or inconsistent
#cat:                 with respect to their adjacent neighbors.

   Input:
      lbox      - left edge of current concentric square
      tbox      - top edge of current concentric square
      rbox      - right edge of current concentric square
      bbox      - bottom edge of current concentric square
      imap      - vector of IMAP integer directions
      mw        - width (in blocks) of the IMAP
      mh        - height (in blocks) of the IMAP
      dir2rad   - lookup table for converting integer directions
      lfsparms  - parameters and thresholds for controlling LFS
   Return Code:
      Positive - direction should be removed from IMAP
      Zero     - direction should NOT be remove from IMAP
**************************************************************************/
int test_right_edge(const int lbox, const int tbox, const int rbox,
                    const int bbox, int *imap, const int mw, const int mh,
                    const DIR2RAD *dir2rad, const LFSPARMS *lfsparms)
{
   int bx, by, sy, ey;
   int *iptr, *sptr, *eptr;
   int nremoved;

   /* Initialize number of directions removed on edge to 0 */
   nremoved = 0;

   /* Set start pointer to top-rightmost point of box, or set it to */
   /* the topmost point in IMAP column (0), whichever is larger.    */
   sy = max(tbox, 0);
   sptr = imap + (sy*mw) + rbox;

   /* Set end pointer to either 1 point short of the bottom-    */
   /* rightmost point of box, or set it to the bottommost point */
   /* in the IMAP column (lasty=mh-1), whichever is smaller.    */
   ey = min(bbox-1,mh-1);
   eptr = imap + (ey*mw) + rbox;

   /* For each point on box's edge ... */
   for(iptr = sptr, bx = rbox, by = sy;
       iptr <= eptr;
       iptr+=mw, by++){
      /* If valid IMAP direction and test for removal is true ... */
      if((*iptr != INVALID_DIR)&&
         (remove_dir(imap, bx, by, mw, mh, dir2rad, lfsparms))){
         /* Set to INVALID */
         *iptr = INVALID_DIR;
         /* Bump number of removed IMAP directions */
         nremoved++;
      }
   }

   /* Return the number of directions removed on edge */
   return(nremoved);
}

/*************************************************************************
**************************************************************************
#cat: test_bottom_edge - Walks the bottom edge of a concentric square in the
#cat:               IMAP, testing directions along the way to see if they
#cat:               should be removed due to being too weak or inconsistent
#cat:               with respect to their adjacent neighbors.
   Input:
      lbox      - left edge of current concentric square
      tbox      - top edge of current concentric square
      rbox      - right edge of current concentric square
      bbox      - bottom edge of current concentric square
      imap      - vector of IMAP integer directions
      mw        - width (in blocks) of the IMAP
      mh        - height (in blocks) of the IMAP
      dir2rad   - lookup table for converting integer directions
      lfsparms  - parameters and thresholds for controlling LFS
   Return Code:
      Positive - direction should be removed from IMAP
      Zero     - direction should NOT be remove from IMAP
**************************************************************************/
int test_bottom_edge(const int lbox, const int tbox, const int rbox,
                     const int bbox, int *imap, const int mw, const int mh,
                     const DIR2RAD *dir2rad, const LFSPARMS *lfsparms)
{
   int bx, by, sx, ex;
   int *iptr, *sptr, *eptr;
   int nremoved;

   /* Initialize number of directions removed on edge to 0 */
   nremoved = 0;

   /* Set start pointer to bottom-rightmost point of box, or set it to the */
   /* rightmost point in the IMAP ROW (lastx=mw-1), whichever is smaller.  */
   sx = min(rbox, mw-1);
   sptr = imap + (bbox*mw) + sx;

   /* Set end pointer to either 1 point short of the bottom-    */
   /* lefttmost point of box, or set it to the leftmost point   */
   /* in the IMAP row (x=0), whichever is larger.               */
   ex = max(lbox-1, 0);
   eptr = imap + (bbox*mw) + ex;

   /* For each point on box's edge ... */
   for(iptr = sptr, bx = sx, by = bbox;
       iptr >= eptr;
       iptr--, bx--){
      /* If valid IMAP direction and test for removal is true ... */
      if((*iptr != INVALID_DIR)&&
         (remove_dir(imap, bx, by, mw, mh, dir2rad, lfsparms))){
         /* Set to INVALID */
         *iptr = INVALID_DIR;
         /* Bump number of removed IMAP directions */
         nremoved++;
      }
   }

   /* Return the number of directions removed on edge */
   return(nremoved);
}

/*************************************************************************
**************************************************************************
#cat: test_left_edge - Walks the left edge of a concentric square in the IMAP,
#cat:                 testing directions along the way to see if they should
#cat:                 be removed due to being too weak or inconsistent with
#cat:                 respect to their adjacent neighbors.

   Input:
      lbox      - left edge of current concentric square
      tbox      - top edge of current concentric square
      rbox      - right edge of current concentric square
      bbox      - bottom edge of current concentric square
      imap      - vector of IMAP integer directions
      mw        - width (in blocks) of the IMAP
      mh        - height (in blocks) of the IMAP
      dir2rad   - lookup table for converting integer directions
      lfsparms  - parameters and thresholds for controlling LFS
   Return Code:
      Positive - direction should be removed from IMAP
      Zero     - direction should NOT be remove from IMAP
**************************************************************************/
int test_left_edge(const int lbox, const int tbox, const int rbox,
                   const int bbox, int *imap, const int mw, const int mh,
                   const DIR2RAD *dir2rad, const LFSPARMS *lfsparms)
{
   int bx, by, sy, ey;
   int *iptr, *sptr, *eptr;
   int nremoved;

   /* Initialize number of directions removed on edge to 0 */
   nremoved = 0;

   /* Set start pointer to bottom-leftmost point of box, or set it to */
   /* the bottommost point in IMAP column (lasty=mh-1), whichever     */
   /* is smaller.                                                     */
   sy = min(bbox, mh-1);
   sptr = imap + (sy*mw) + lbox;

   /* Set end pointer to either 1 point short of the top-leftmost */
   /* point of box, or set it to the topmost point in the IMAP    */
   /* column (y=0), whichever is larger.                          */
   ey = max(tbox-1, 0);
   eptr = imap + (ey*mw) + lbox;

   /* For each point on box's edge ... */
   for(iptr = sptr, bx = lbox, by = sy;
       iptr >= eptr;
       iptr-=mw, by--){
      /* If valid IMAP direction and test for removal is true ... */
      if((*iptr != INVALID_DIR)&&
         (remove_dir(imap, bx, by, mw, mh, dir2rad, lfsparms))){
         /* Set to INVALID */
         *iptr = INVALID_DIR;
         /* Bump number of removed IMAP directions */
         nremoved++;
      }
   }

   /* Return the number of directions removed on edge */
   return(nremoved);
}

/*************************************************************************
**************************************************************************
#cat: remove_dir - Determines if an IMAP direction should be removed based
#cat:              on analyzing its adjacent neighbors

   Input:
      imap      - vector of IMAP integer directions
      mx        - IMAP X-coord of the current direction being tested
      my        - IMPA Y-coord of the current direction being tested
      mw        - width (in blocks) of the IMAP
      mh        - height (in blocks) of the IMAP
      dir2rad   - lookup table for converting integer directions
      lfsparms  - parameters and thresholds for controlling LFS
   Return Code:
      Positive - direction should be removed from IMAP
      Zero     - direction should NOT be remove from IMAP
**************************************************************************/
int remove_dir(int *imap, const int mx, const int my,
               const int mw, const int mh, const DIR2RAD *dir2rad,
               const LFSPARMS *lfsparms)
{
   int avrdir, nvalid, dist;
   double dir_strength;

   /* Compute average direction from neighbors, returning the */
   /* number of valid neighbors used in the computation, and  */
   /* the "strength" of the average direction.                */
   average_8nbr_dir(&avrdir, &dir_strength, &nvalid, imap, mx, my, mw, mh,
                    dir2rad);

   /* Conduct valid neighbor test (Ex. thresh==3) */
   if(nvalid < lfsparms->rmv_valid_nbr_min){

#ifdef LOG_REPORT /*vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/
      fprintf(logfp, "      BLOCK %2d (%2d, %2d)\n",
              mx+(my*mw), mx, my);
      fprintf(logfp, "         Average NBR :   %2d %6.3f %d\n",
              avrdir, dir_strength, nvalid);
      fprintf(logfp, "         1. Valid NBR (%d < %d)\n",
              nvalid, lfsparms->rmv_valid_nbr_min);
#endif /*^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

      return(1);
   }

   /* If stregnth of average neighbor direction is large enough to */
   /* put credence in ... (Ex. thresh==0.2)                        */
   if(dir_strength >= lfsparms->dir_strength_min){

      /* Conduct direction distance test (Ex. thresh==3) */
      /* Compute minimum absolute distance between current and       */
      /* average directions accounting for wrapping from 0 to NDIRS. */
      dist = abs(avrdir - *(imap+(my*mw)+mx));
      dist = min(dist, dir2rad->ndirs-dist);
      if(dist > lfsparms->dir_distance_max){

#ifdef LOG_REPORT /*vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/
         fprintf(logfp, "      BLOCK %2d (%2d, %2d)\n",
                 mx+(my*mw), mx, my);
         fprintf(logfp, "         Average NBR :   %2d %6.3f %d\n",
                 avrdir, dir_strength, nvalid);
         fprintf(logfp, "         1. Valid NBR (%d < %d)\n",
                 nvalid, lfsparms->rmv_valid_nbr_min);
         fprintf(logfp, "         2. Direction Strength (%6.3f >= %6.3f)\n",
                 dir_strength, lfsparms->dir_strength_min);
         fprintf(logfp, "         Current Dir =  %d, Average Dir = %d\n",
                 *(imap+(my*mw)+mx), avrdir);
         fprintf(logfp, "         3. Direction Distance (%d > %d)\n",
                 dist, lfsparms->dir_distance_max);
#endif /*^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

         return(2);
      }
   }

   /* Otherwise, the strength of the average direciton is not strong enough */
   /* to put credence in, so leave the current block's directon alone.      */

   return(0);
}

/*************************************************************************
**************************************************************************
#cat: average_8nbr_dir - Given an IMAP direction, computes an average
#cat:                    direction from its adjacent 8 neighbors returning
#cat:                    the average direction, its strength, and the
#cat:                    number of valid direction in the neighborhood.

   Input:
      imap      - vector of IMAP integer directions
      mx        - IMAP X-coord of the current direction
      my        - IMPA Y-coord of the current direction
      mw        - width (in blocks) of the IMAP
      mh        - height (in blocks) of the IMAP
      dir2rad   - lookup table for converting integer directions
   Output:
      avrdir    - the average direction computed from neighbors
      dir_strenght - the strength of the average direction
      nvalid    - the number of valid directions used to compute the
                  average
**************************************************************************/
void average_8nbr_dir(int *avrdir, double *dir_strength, int *nvalid,
                      int *imap, const int mx, const int my,
                      const int mw, const int mh,
                      const DIR2RAD *dir2rad)
{
   int *iptr;
   int e,w,n,s;
   double cospart, sinpart;
   double pi2, pi_factor, theta;
   double avr;

   /* Compute neighbor coordinates to current IMAP direction */
   e = mx+1;  /* East */
   w = mx-1;  /* West */
   n = my-1;  /* North */
   s = my+1;  /* South */

   /* Intialize accumulators */
   *nvalid = 0;
   cospart = 0.0;
   sinpart = 0.0;

   /* 1. Test NW */
   /* If NW point within IMAP boudaries ... */
   if((w >= 0) && (n >= 0)){
      iptr = imap + (n*mw) + w;
      /* If valid direction ... */
      if(*iptr != INVALID_DIR){
         /* Accumulate cosine and sine components of the direction */
         cospart += dir2rad->cos[*iptr];
         sinpart += dir2rad->sin[*iptr];
         /* Bump number of accumulated directions */
         (*nvalid)++;
      }
   }

   /* 2. Test N */
   /* If N point within IMAP boudaries ... */
   if(n >= 0){
      iptr = imap + (n*mw) + mx;
      /* If valid direction ... */
      if(*iptr != INVALID_DIR){
         /* Accumulate cosine and sine components of the direction */
         cospart += dir2rad->cos[*iptr];
         sinpart += dir2rad->sin[*iptr];
         /* Bump number of accumulated directions */
         (*nvalid)++;
      }
   }

   /* 3. Test NE */
   /* If NE point within IMAP boudaries ... */
   if((e < mw) && (n >= 0)){
      iptr = imap + (n*mw) + e;
      /* If valid direction ... */
      if(*iptr != INVALID_DIR){
         /* Accumulate cosine and sine components of the direction */
         cospart += dir2rad->cos[*iptr];
         sinpart += dir2rad->sin[*iptr];
         /* Bump number of accumulated directions */
         (*nvalid)++;
      }
   }

   /* 4. Test E */
   /* If E point within IMAP boudaries ... */
   if(e < mw){
      iptr = imap + (my*mw) + e;
      /* If valid direction ... */
      if(*iptr != INVALID_DIR){
         /* Accumulate cosine and sine components of the direction */
         cospart += dir2rad->cos[*iptr];
         sinpart += dir2rad->sin[*iptr];
         /* Bump number of accumulated directions */
         (*nvalid)++;
      }
   }

   /* 5. Test SE */
   /* If SE point within IMAP boudaries ... */
   if((e < mw) && (s < mh)){
      iptr = imap + (s*mw) + e;
      /* If valid direction ... */
      if(*iptr != INVALID_DIR){
         /* Accumulate cosine and sine components of the direction */
         cospart += dir2rad->cos[*iptr];
         sinpart += dir2rad->sin[*iptr];
         /* Bump number of accumulated directions */
         (*nvalid)++;
      }
   }

   /* 6. Test S */
   /* If S point within IMAP boudaries ... */
   if(s < mh){
      iptr = imap + (s*mw) + mx;
      /* If valid direction ... */
      if(*iptr != INVALID_DIR){
         /* Accumulate cosine and sine components of the direction */
         cospart += dir2rad->cos[*iptr];
         sinpart += dir2rad->sin[*iptr];
         /* Bump number of accumulated directions */
         (*nvalid)++;
      }
   }

   /* 7. Test SW */
   /* If SW point within IMAP boudaries ... */
   if((w >= 0) && (s < mh)){
      iptr = imap + (s*mw) + w;
      /* If valid direction ... */
      if(*iptr != INVALID_DIR){
         /* Accumulate cosine and sine components of the direction */
         cospart += dir2rad->cos[*iptr];
         sinpart += dir2rad->sin[*iptr];
         /* Bump number of accumulated directions */
         (*nvalid)++;
      }
   }

   /* 8. Test W */
   /* If W point within IMAP boudaries ... */
   if(w >= 0){
      iptr = imap + (my*mw) + w;
      /* If valid direction ... */
      if(*iptr != INVALID_DIR){
         /* Accumulate cosine and sine components of the direction */
         cospart += dir2rad->cos[*iptr];
         sinpart += dir2rad->sin[*iptr];
         /* Bump number of accumulated directions */
         (*nvalid)++;
      }
   }

   /* If there were no neighbors found with valid direction ... */
   if(*nvalid == 0){
      /* Return INVALID direction. */
      *dir_strength = 0;
      *avrdir = INVALID_DIR;
      return;
   }

   /* Compute averages of accumulated cosine and sine direction components */
   cospart /= (double)(*nvalid);
   sinpart /= (double)(*nvalid);

   /* Compute directional strength as hypotenuse (without sqrt) of average */
   /* cosine and sine direction components.  Believe this value will be on */
   /* the range of [0 .. 1].                                               */
   *dir_strength = (cospart * cospart) + (sinpart * sinpart);
   /* Need to truncate precision so that answers are consistent   */
   /* on different computer architectures when comparing doubles. */
   *dir_strength = trunc_dbl_precision(*dir_strength, TRUNC_SCALE);

   /* If the direction strength is not sufficiently high ... */
   if(*dir_strength < DIR_STRENGTH_MIN){
      /* Return INVALID direction. */
      *dir_strength = 0;
      *avrdir = INVALID_DIR;
      return;
   }

   /* Compute angle (in radians) from Arctan of avarage         */
   /* cosine and sine direction components.  I think this order */
   /* is necessary because 0 direction is vertical and positive */
   /* direction is clockwise.                                   */
   theta = atan2(sinpart, cospart);

   /* Atan2 returns theta on range [-PI..PI].  Adjust theta so that */
   /* it is on the range [0..2PI].                                  */
   pi2 = 2*M_PI;
   theta += pi2;
   theta = fmod(theta, pi2);

   /* Pi_factor sets the period of the trig functions to NDIRS units in x. */
   /* For example, if NDIRS==16, then pi_factor = 2(PI/16) = .3926...      */
   /* Dividing theta (in radians) by this factor ((1/pi_factor)==2.546...) */
   /* will produce directions on the range [0..NDIRS].                     */
   pi_factor = pi2/(double)dir2rad->ndirs; /* 2(M_PI/ndirs) */

   /* Round off the direction and return it as an average direction */
   /* for the neighborhood.                                         */
   avr = theta / pi_factor;
   /* Need to truncate precision so that answers are consistent */
   /* on different computer architectures when rounding doubles. */
   avr = trunc_dbl_precision(avr, TRUNC_SCALE);
   *avrdir = sround(avr);

   /* Really do need to map values > NDIRS back onto [0..NDIRS) range. */
   *avrdir %= dir2rad->ndirs;
}

/*************************************************************************
**************************************************************************
#cat: num_valid_8nbrs - Given a block in an IMAP, counts the number of
#cat:                   immediate neighbors that have a valid IMAP direction.

   Input:
      imap - 2-D vector of directional ridge flows
      mx   - horizontal coord of current block in IMAP
      my   - vertical coord of current block in IMAP
      mw   - width (in blocks) of the IMAP
      mh   - height (in blocks) of the IMAP
   Return Code:
      Non-negative - the number of valid IMAP neighbors
**************************************************************************/
int num_valid_8nbrs(int *imap, const int mx, const int my,
                    const int mw, const int mh)
{
   int e_ind, w_ind, n_ind, s_ind;
   int nvalid;

   /* Initialize VALID IMAP counter to zero. */
   nvalid = 0;

   /* Compute neighbor coordinates to current IMAP direction */
   e_ind = mx+1;  /* East index */
   w_ind = mx-1;  /* West index */
   n_ind = my-1;  /* North index */
   s_ind = my+1;  /* South index */

   /* 1. Test NW IMAP value.  */
   /* If neighbor indices are within IMAP boundaries and it is VALID ... */
   if((w_ind >= 0) && (n_ind >= 0) && (*(imap + (n_ind*mw) + w_ind) >= 0))
      /* Bump VALID counter. */
      nvalid++;

   /* 2. Test N IMAP value.  */
   if((n_ind >= 0) && (*(imap + (n_ind*mw) + mx) >= 0))
      nvalid++;

   /* 3. Test NE IMAP value. */
   if((n_ind >= 0) && (e_ind < mw) && (*(imap + (n_ind*mw) + e_ind) >= 0))
      nvalid++;

   /* 4. Test E IMAP value. */
   if((e_ind < mw) && (*(imap + (my*mw) + e_ind) >= 0))
      nvalid++;

   /* 5. Test SE IMAP value. */
   if((e_ind < mw) && (s_ind < mh) && (*(imap + (s_ind*mw) + e_ind) >= 0))
      nvalid++;

   /* 6. Test S IMAP value. */
   if((s_ind < mh) && (*(imap + (s_ind*mw) + mx) >= 0))
      nvalid++;

   /* 7. Test SW IMAP value. */
   if((w_ind >= 0) && (s_ind < mh) && (*(imap + (s_ind*mw) + w_ind) >= 0))
      nvalid++;

   /* 8. Test W IMAP value. */
   if((w_ind >= 0) && (*(imap + (my*mw) + w_ind) >= 0))
      nvalid++;

   /* Return number of neighbors with VALID IMAP values. */
   return(nvalid);
}

/*************************************************************************
**************************************************************************
#cat: smooth_imap - Takes a vector of integer directions and smooths them
#cat:               by analyzing the direction of adjacent neighbors.

   Input:
      imap      - vector of IMAP integer directions
      mw        - width (in blocks) of the IMAP
      mh        - height (in blocks) of the IMAP
      dir2rad   - lookup table for converting integer directions
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      imap      - vector of smoothed input values
**************************************************************************/
void smooth_imap(int *imap, const int mw, const int mh,
                 const DIR2RAD *dir2rad, const LFSPARMS *lfsparms)
{
   int mx, my;
   int *iptr;
   int avrdir, nvalid;
   double dir_strength;

   print2log("SMOOTH MAP\n");

   iptr = imap;
   for(my = 0; my < mh; my++){
      for(mx = 0; mx < mw; mx++){
         /* Compute average direction from neighbors, returning the */
         /* number of valid neighbors used in the computation, and  */
         /* the "strength" of the average direction.                */
         average_8nbr_dir(&avrdir, &dir_strength, &nvalid,
                          imap, mx, my, mw, mh, dir2rad);

         /* If average direction strength is strong enough */
         /*    (Ex. thresh==0.2)...                        */
         if(dir_strength >= lfsparms->dir_strength_min){
            /* If IMAP direction is valid ... */
            if(*iptr != INVALID_DIR){
               /* Conduct valid neighbor test (Ex. thresh==3)... */
               if(nvalid >= lfsparms->rmv_valid_nbr_min){

#ifdef LOG_REPORT /*vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/
                  fprintf(logfp, "   BLOCK %2d (%2d, %2d)\n",
                          mx+(my*mw), mx, my);
                  fprintf(logfp, "      Average NBR :   %2d %6.3f %d\n",
                          avrdir, dir_strength, nvalid);
                  fprintf(logfp, "      1. Valid NBR (%d >= %d)\n",
                          nvalid, lfsparms->rmv_valid_nbr_min);
                  fprintf(logfp, "      Valid Direction = %d\n", *iptr);
                  fprintf(logfp, "      Smoothed Direction = %d\n", avrdir);
#endif /*^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

                  /* Reassign valid IMAP direction with average direction. */
                  *iptr = avrdir;
               }
            }
            /* Otherwise IMAP direction is invalid ... */
            else{
               /* Even if IMAP value is invalid, if number of valid */
               /* neighbors is big enough (Ex. thresh==7)...        */
               if(nvalid >= lfsparms->smth_valid_nbr_min){

#ifdef LOG_REPORT /*vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/
                  fprintf(logfp, "   BLOCK %2d (%2d, %2d)\n",
                          mx+(my*mw), mx, my);
                  fprintf(logfp, "      Average NBR :   %2d %6.3f %d\n",
                          avrdir, dir_strength, nvalid);
                  fprintf(logfp, "      2. Invalid NBR (%d >= %d)\n",
                          nvalid, lfsparms->smth_valid_nbr_min);
                  fprintf(logfp, "      Invalid Direction = %d\n", *iptr);
                  fprintf(logfp, "      Smoothed Direction = %d\n", avrdir);
#endif /*^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

                  /* Assign invalid IMAP direction with average direction. */
                  *iptr = avrdir;
               }
            }
         }

         /* Bump to next IMAP direction. */
         iptr++;
      }
   }
}

/*************************************************************************
**************************************************************************
#cat: vorticity - Measures the amount of cummulative curvature incurred
#cat:             among the IMAP neighbors of the given block.

   Input:
      imap  - 2D vector of ridge flow directions
      mx    - horizontal coord of current IMAP block
      my    - vertical coord of current IMAP block
      mw    - width (in blocks) of the IMAP
      mh    - height (in blocks) of the IMAP
      ndirs - number of possible directions in the IMAP
   Return Code:
      Non-negative - the measured vorticity among the neighbors
**************************************************************************/
int vorticity(int *imap, const int mx, const int my,
              const int mw, const int mh, const int ndirs)
{
   int e_ind, w_ind, n_ind, s_ind;
   int nw_val, n_val, ne_val, e_val, se_val, s_val, sw_val, w_val;
   int vmeasure;

   /* Compute neighbor coordinates to current IMAP direction */
   e_ind = mx+1;  /* East index */
   w_ind = mx-1;  /* West index */
   n_ind = my-1;  /* North index */
   s_ind = my+1;  /* South index */

   /* 1. Get NW IMAP value.  */
   /* If neighbor indices are within IMAP boundaries ... */
   if((w_ind >= 0) && (n_ind >= 0))
      /* Set neighbor value to IMAP value. */
      nw_val = *(imap + (n_ind*mw) + w_ind);
   else
      /* Otherwise, set the neighbor value to INVALID. */
      nw_val = INVALID_DIR;

   /* 2. Get N IMAP value.  */
   if(n_ind >= 0)
      n_val = *(imap + (n_ind*mw) + mx);
   else
      n_val = INVALID_DIR;

   /* 3. Get NE IMAP value. */
   if((n_ind >= 0) && (e_ind < mw))
      ne_val = *(imap + (n_ind*mw) + e_ind);
   else
      ne_val = INVALID_DIR;

   /* 4. Get E IMAP value. */
   if(e_ind < mw)
      e_val = *(imap + (my*mw) + e_ind);
   else
      e_val = INVALID_DIR;

   /* 5. Get SE IMAP value. */
   if((e_ind < mw) && (s_ind < mh))
      se_val = *(imap + (s_ind*mw) + e_ind);
   else
      se_val = INVALID_DIR;

   /* 6. Get S IMAP value. */
   if(s_ind < mh)
      s_val = *(imap + (s_ind*mw) + mx);
   else
      s_val = INVALID_DIR;

   /* 7. Get SW IMAP value. */
   if((w_ind >= 0) && (s_ind < mh))
      sw_val = *(imap + (s_ind*mw) + w_ind);
   else
      sw_val = INVALID_DIR;

   /* 8. Get W IMAP value. */
   if(w_ind >= 0)
      w_val = *(imap + (my*mw) + w_ind);
   else
      w_val = INVALID_DIR;

   /* Now that we have all IMAP neighbors, accumulate vorticity between */
   /* the neighboring directions.                                       */

   /* Initialize vorticity accumulator to zero. */
   vmeasure = 0;

   /* 1. NW & N */
   accum_nbr_vorticity(&vmeasure, nw_val, n_val, ndirs);

   /* 2. N & NE */
   accum_nbr_vorticity(&vmeasure, n_val, ne_val, ndirs);

   /* 3. NE & E */
   accum_nbr_vorticity(&vmeasure, ne_val, e_val, ndirs);

   /* 4. E & SE */
   accum_nbr_vorticity(&vmeasure, e_val, se_val, ndirs);

   /* 5. SE & S */
   accum_nbr_vorticity(&vmeasure, se_val, s_val, ndirs);

   /* 6. S & SW */
   accum_nbr_vorticity(&vmeasure, s_val, sw_val, ndirs);

   /* 7. SW & W */
   accum_nbr_vorticity(&vmeasure, sw_val, w_val, ndirs);

   /* 8. W & NW */
   accum_nbr_vorticity(&vmeasure, w_val, nw_val, ndirs);

   /* Return the accumulated vorticity measure. */
   return(vmeasure);
}

/*************************************************************************
**************************************************************************
#cat: accum_nbor_vorticity - Accumlates the amount of curvature measures
#cat:                        between neighboring IMAP blocks.

   Input:
      dir1  - first neighbor's integer IMAP direction
      dir2  - second neighbor's integer IMAP direction
      ndirs - number of possible IMAP directions
   Output:
      vmeasure - accumulated vorticity among neighbors measured so far
**************************************************************************/
void accum_nbr_vorticity(int *vmeasure, const int dir1, const int dir2,
                         const int ndirs)
{
   int dist;

   /* Measure difference in direction between a pair of neighboring */
   /* directions.                                                   */
   /* If both neighbors are not equal and both are VALID ... */
   if((dir1 != dir2) && (dir1 >= 0)&&(dir2 >= 0)){
      /* Measure the clockwise distance from the first to the second */
      /* directions.                                                 */
      dist = dir2 - dir1;
      /* If dist is negative, then clockwise distance must wrap around */
      /* the high end of the direction range. For example:             */
      /*              dir1 = 8                                         */
      /*              dir2 = 3                                         */
      /*       and   ndirs = 16                                        */
      /*             3 - 8 = -5                                        */
      /*        so  16 - 5 = 11  (the clockwise distance from 8 to 3)  */
       if(dist < 0)
         dist += ndirs;
      /* If the change in clockwise direction is larger than 90 degrees as */
      /* in total the total number of directions covers 180 degrees.       */
      if(dist > (ndirs>>1))
         /* Decrement the vorticity measure. */
         (*vmeasure)--;
      else
         /* Otherwise, bump the vorticity measure. */
         (*vmeasure)++;
   }
   /* Otherwise both directions are either equal or  */
   /* one or both directions are INVALID, so ignore. */
}

/*************************************************************************
**************************************************************************
#cat: curvature - Measures the largest change in direction between the
#cat:             current IMAP direction and its immediate neighbors.

   Input:
      imap  - 2D vector of ridge flow directions
      mx    - horizontal coord of current IMAP block
      my    - vertical coord of current IMAP block
      mw    - width (in blocks) of the IMAP
      mh    - height (in blocks) of the IMAP
      ndirs - number of possible directions in the IMAP
   Return Code:
      Non-negative - maximum change in direction found (curvature)
      Negative     - No valid neighbor found to measure change in direction
**************************************************************************/
int curvature(int *imap, const int mx, const int my,
              const int mw, const int mh, const int ndirs)
{
   int *iptr;
   int e_ind, w_ind, n_ind, s_ind;
   int nw_val, n_val, ne_val, e_val, se_val, s_val, sw_val, w_val;
   int cmeasure, dist;

   /* Compute neighbor coordinates to current IMAP direction */
   e_ind = mx+1;  /* East index */
   w_ind = mx-1;  /* West index */
   n_ind = my-1;  /* North index */
   s_ind = my+1;  /* South index */

   /* 1. Get NW IMAP value.  */
   /* If neighbor indices are within IMAP boundaries ... */
   if((w_ind >= 0) && (n_ind >= 0))
      /* Set neighbor value to IMAP value. */
      nw_val = *(imap + (n_ind*mw) + w_ind);
   else
      /* Otherwise, set the neighbor value to INVALID. */
      nw_val = INVALID_DIR;

   /* 2. Get N IMAP value.  */
   if(n_ind >= 0)
      n_val = *(imap + (n_ind*mw) + mx);
   else
      n_val = INVALID_DIR;

   /* 3. Get NE IMAP value. */
   if((n_ind >= 0) && (e_ind < mw))
      ne_val = *(imap + (n_ind*mw) + e_ind);
   else
      ne_val = INVALID_DIR;

   /* 4. Get E IMAP value. */
   if(e_ind < mw)
      e_val = *(imap + (my*mw) + e_ind);
   else
      e_val = INVALID_DIR;

   /* 5. Get SE IMAP value. */
   if((e_ind < mw) && (s_ind < mh))
      se_val = *(imap + (s_ind*mw) + e_ind);
   else
      se_val = INVALID_DIR;

   /* 6. Get S IMAP value. */
   if(s_ind < mh)
      s_val = *(imap + (s_ind*mw) + mx);
   else
      s_val = INVALID_DIR;

   /* 7. Get SW IMAP value. */
   if((w_ind >= 0) && (s_ind < mh))
      sw_val = *(imap + (s_ind*mw) + w_ind);
   else
      sw_val = INVALID_DIR;

   /* 8. Get W IMAP value. */
   if(w_ind >= 0)
      w_val = *(imap + (my*mw) + w_ind);
   else
      w_val = INVALID_DIR;

   /* Now that we have all IMAP neighbors, determine largest change in */
   /* direction from current block to each of its 8 VALID neighbors.   */

   /* Initialize pointer to current IMAP value. */
   iptr = imap + (my*mw) + mx;

   /* Initialize curvature measure to negative as closest_dir_dist() */
   /* always returns -1=INVALID or a positive value.                 */
   cmeasure = -1;

   /* 1. With NW */
   /* Compute closest distance between neighboring directions. */
   dist = closest_dir_dist(*iptr, nw_val, ndirs);
   /* Keep track of maximum. */
   if(dist > cmeasure)
      cmeasure = dist;

   /* 2. With N */
   dist = closest_dir_dist(*iptr, n_val, ndirs);
   if(dist > cmeasure)
      cmeasure = dist;

   /* 3. With NE */
   dist = closest_dir_dist(*iptr, ne_val, ndirs);
   if(dist > cmeasure)
      cmeasure = dist;

   /* 4. With E */
   dist = closest_dir_dist(*iptr, e_val, ndirs);
   if(dist > cmeasure)
      cmeasure = dist;

   /* 5. With SE */
   dist = closest_dir_dist(*iptr, se_val, ndirs);
   if(dist > cmeasure)
      cmeasure = dist;

   /* 6. With S */
   dist = closest_dir_dist(*iptr, s_val, ndirs);
   if(dist > cmeasure)
      cmeasure = dist;

   /* 7. With SW */
   dist = closest_dir_dist(*iptr, sw_val, ndirs);
   if(dist > cmeasure)
      cmeasure = dist;

   /* 8. With W */
   dist = closest_dir_dist(*iptr, w_val, ndirs);
   if(dist > cmeasure)
      cmeasure = dist;

   /* Return maximum difference between current block's IMAP direction */
   /* and the rest of its VALID neighbors.                             */
   return(cmeasure);
}

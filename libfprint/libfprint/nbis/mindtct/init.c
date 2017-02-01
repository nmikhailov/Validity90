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

      FILE:    INIT.C
      AUTHOR:  Michael D. Garris
      DATE:    03/16/1999
      UPDATED: 10/04/1999 Version 2 by MDG
      UPDATED: 03/16/2005 by MDG

      Contains routines responsible for allocation and/or initialization
      of memories required by the NIST Latent Fingerprint System.

***********************************************************************
               ROUTINES:
                        init_dir2rad()
                        init_dftwaves()
                        get_max_padding_V2()
                        init_rotgrids()
                        alloc_dir_powers()
                        alloc_power_stats()
***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <lfs.h>

/*************************************************************************
**************************************************************************
#cat: init_dir2rad - Allocates and initializes a lookup table containing
#cat:                cosine and sine values needed to convert integer IMAP
#cat:                directions to angles in radians.

   Input:
      ndirs - the number of integer directions to be defined in a
              semicircle
   Output:
      optr  - points to the allocated/initialized DIR2RAD structure
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
int init_dir2rad(DIR2RAD **optr, const int ndirs)
{
   DIR2RAD *dir2rad;
   int i;
   double theta, pi_factor;
   double cs, sn;

   /* Allocate structure */
   dir2rad = (DIR2RAD *)malloc(sizeof(DIR2RAD));
   if(dir2rad == (DIR2RAD *)NULL){
      fprintf(stderr, "ERROR : init_dir2rad : malloc : dir2rad\n");
      return(-10);
   }

   /* Assign number of directions */
   dir2rad->ndirs = ndirs;

   /* Allocate cosine vector */
   dir2rad->cos = (double *)malloc(ndirs * sizeof(double));
   if(dir2rad->cos == (double *)NULL){
      /* Free memory allocated to this point. */
      free(dir2rad);
      fprintf(stderr, "ERROR : init_dir2rad : malloc : dir2rad->cos\n");
      return(-11);
   }

   /* Allocate sine vector */
   dir2rad->sin = (double *)malloc(ndirs * sizeof(double));
   if(dir2rad->sin == (double *)NULL){
      /* Free memory allocated to this point. */
      free(dir2rad->cos);
      free(dir2rad);
      fprintf(stderr, "ERROR : init_dir2rad : malloc : dir2rad->sin\n");
      return(-12);
   }

   /* Pi_factor sets the period of the trig functions to NDIRS units in x. */
   /* For example, if NDIRS==16, then pi_factor = 2(PI/16) = .3926...      */
   pi_factor = 2.0*M_PI/(double)ndirs;

   /* Now compute cos and sin values for each direction.    */
   for (i = 0; i < ndirs; ++i) {
      theta = (double)(i * pi_factor);
      cs = cos(theta);
      sn = sin(theta);
      /* Need to truncate precision so that answers are consistent */
      /* on different computer architectures. */
      cs = trunc_dbl_precision(cs, TRUNC_SCALE);
      sn = trunc_dbl_precision(sn, TRUNC_SCALE);
      dir2rad->cos[i] = cs;
      dir2rad->sin[i] = sn;
   }

   *optr = dir2rad;
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: init_dftwaves - Allocates and initializes a set of wave forms needed
#cat:                 to conduct DFT analysis on blocks of the input image

   Input:
      dft_coefs - array of multipliers used to define the frequency for
                  each wave form to be computed
      nwaves    - number of wave forms to be computed
      blocksize - the width and height of each block of image data to
                  be DFT analyzed
   Output:
      optr     - points to the allocated/initialized DFTWAVES structure
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
int init_dftwaves(DFTWAVES **optr, const double *dft_coefs,
                  const int nwaves, const int blocksize)
{
   DFTWAVES *dftwaves;
   int i, j;
   double pi_factor, freq, x;
   double *cptr, *sptr;

   /* Allocate structure */
   dftwaves = (DFTWAVES *)malloc(sizeof(DFTWAVES));
   if(dftwaves == (DFTWAVES *)NULL){
      fprintf(stderr, "ERROR : init_dftwaves : malloc : dftwaves\n");
      return(-20);
   }

   /* Set number of DFT waves */
   dftwaves->nwaves = nwaves;
   /* Set wave length of the DFT waves (they all must be the same length) */
   dftwaves->wavelen = blocksize;

   /* Allocate list of wave pointers */
   dftwaves->waves = (DFTWAVE **)malloc(nwaves * sizeof(DFTWAVE *));
   if(dftwaves == (DFTWAVES *)NULL){
      /* Free memory allocated to this point. */
      free(dftwaves);
      fprintf(stderr, "ERROR : init_dftwaves : malloc : dftwaves->waves\n");
      return(-21);
   }

   /* Pi_factor sets the period of the trig functions to BLOCKSIZE units */
   /* in x.  For example, if BLOCKSIZE==24, then                         */
   /*                         pi_factor = 2(PI/24) = .26179...           */
   pi_factor = 2.0*M_PI/(double)blocksize;

   /* Foreach of 4 DFT frequency coef ... */
   for (i = 0; i < nwaves; ++i) {
      /* Allocate wave structure */
      dftwaves->waves[i] = (DFTWAVE *)malloc(sizeof(DFTWAVE));
      if(dftwaves->waves[i] == (DFTWAVE *)NULL){
         /* Free memory allocated to this point. */
         { int _j; for(_j = 0; _j < i; _j++){
            free(dftwaves->waves[_j]->cos);
            free(dftwaves->waves[_j]->sin);
            free(dftwaves->waves[_j]);
         }}
         free(dftwaves->waves);
         free(dftwaves);
         fprintf(stderr,
                 "ERROR : init_dftwaves : malloc : dftwaves->waves[i]\n");
         return(-22);
      }
      /* Allocate cosine vector */
      dftwaves->waves[i]->cos = (double *)malloc(blocksize * sizeof(double));
      if(dftwaves->waves[i]->cos == (double *)NULL){
         /* Free memory allocated to this point. */
         { int _j; for(_j = 0; _j < i; _j++){
            free(dftwaves->waves[_j]->cos);
            free(dftwaves->waves[_j]->sin);
            free(dftwaves->waves[_j]);
         }}
         free(dftwaves->waves[i]);
         free(dftwaves->waves);
         free(dftwaves);
         fprintf(stderr,
                 "ERROR : init_dftwaves : malloc : dftwaves->waves[i]->cos\n");
         return(-23);
      }
      /* Allocate sine vector */
      dftwaves->waves[i]->sin = (double *)malloc(blocksize * sizeof(double));
      if(dftwaves->waves[i]->sin == (double *)NULL){
         /* Free memory allocated to this point. */
         { int _j; for(_j = 0; _j < i; _j++){
            free(dftwaves->waves[_j]->cos);
            free(dftwaves->waves[_j]->sin);
            free(dftwaves->waves[_j]);
         }}
         free(dftwaves->waves[i]->cos);
         free(dftwaves->waves[i]);
         free(dftwaves->waves);
         free(dftwaves);
         fprintf(stderr,
                 "ERROR : init_dftwaves : malloc : dftwaves->waves[i]->sin\n");
         return(-24);
      }

      /* Assign pointer nicknames */
      cptr = dftwaves->waves[i]->cos;
      sptr = dftwaves->waves[i]->sin;

      /* Compute actual frequency */
      freq = pi_factor * dft_coefs[i];

      /* Used as a 1D DFT on a 24 long vector of pixel sums */
      for (j = 0; j < blocksize; ++j) {
         /* Compute sample points from frequency */
         x = freq * (double)j;
         /* Store cos and sin components of sample point */
         *cptr++ = cos(x);
         *sptr++ = sin(x);
      }
   }

   *optr = dftwaves;
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: get_max_padding_V2 - Deterines the maximum amount of image pixel padding
#cat:         required by all LFS (Version 2) processes.  Padding is currently
#cat:         required by the rotated grids used in DFT analyses and in
#cat:         directional binarization.  The NIST generalized code enables
#cat:         the parameters governing these processes to be redefined, so a
#cat:         check at runtime is required to determine which process
#cat:         requires the most padding.  By using the maximum as the padding
#cat:         factor, all processes will run safely with a single padding of
#cat:         the input image avoiding the need to repad for further processes.

   Input:
      map_windowsize  - the size (in pixels) of each window centered about
                        each block in the image used in DFT analyses
      map_windowoffset - the offset (in pixels) from the orgin of the
                        surrounding window to the origin of the block
      dirbin_grid_w   - the width (in pixels) of the rotated grids used in
                        directional binarization
      dirbin_grid_h   - the height (in pixels) of the rotated grids used in
                        directional binarization
   Return Code:
      Non-negative - the maximum padding required for all processes
**************************************************************************/
int get_max_padding_V2(const int map_windowsize, const int map_windowoffset,
                       const int dirbin_grid_w, const int dirbin_grid_h)
{
   int dft_pad, dirbin_pad, max_pad;
   double diag;
   double pad;


   /* 1. Compute pad required for rotated windows used in DFT analyses. */

   /* Explanation of DFT padding:

                    B---------------------
                    |      window        |
                    |                    |
                    |                    |
                    |      A.......______|__________
                    |      :      :      |
                    |<-C-->: block:      |
                 <--|--D-->:      :      | image
                    |      ........      |
                    |      |             |
                    |      |             |
                    |      |             |
                    ----------------------
                           |
                           |
                           |

         Pixel A = Origin of entire fingerprint image
                 = Also origin of first block in image. Each pixel in
                   this block gets the same DFT results computed from
                   the surrounding window.  Note that in general
                   blocks are adjacent and non-overlapping.

         Pixel B = Origin of surrounding window in which DFT
                   analysis is conducted.  Note that this window is not
                   completely contained in the image but extends to the
                   top and to the right.

         Distance C = Number of pixels in which the window extends
                   beyond the image (map_windowoffset).

         Distance D = Amount of padding required to hold the entire
                   rotated window in memory.

   */

   /* Compute pad as difference between the MAP windowsize           */
   /* and the diagonal distance of the window.                       */
   /* (DFT grids are computed with pixel offsets RELATIVE2ORIGIN.)   */
   diag = sqrt((double)(2.0 * map_windowsize * map_windowsize));
   pad = (diag-map_windowsize)/(double)2.0;
   /* Need to truncate precision so that answers are consistent  */
   /* on different computer architectures when rounding doubles. */
   pad = trunc_dbl_precision(pad, TRUNC_SCALE);
   /* Must add the window offset to the rotational padding. */
   dft_pad = sround(pad) + map_windowoffset;

   /* 2. Compute pad required for rotated blocks used in directional  */
   /*    binarization.  Binarization blocks are applied to each pixel */
   /*    in the input image.                                          */
   diag = sqrt((double)((dirbin_grid_w*dirbin_grid_w)+
                        (dirbin_grid_h*dirbin_grid_h)));
   /* Assumption: all grid centers reside in valid/allocated memory. */
   /* (Dirbin grids are computed with pixel offsets RELATIVE2CENTER.) */
   pad = (diag-1)/(double)2.0;
   /* Need to truncate precision so that answers are consistent */
   /* on different computer architectures when rounding doubles. */
   pad = trunc_dbl_precision(pad, TRUNC_SCALE);
   dirbin_pad = sround(pad);

   max_pad = max(dft_pad, dirbin_pad);

   /* Return the maximum of the two required paddings.  This padding will */
   /* be sufficiently large for all purposes, so that padding of the      */
   /* input image will only be required once.                             */
   return(max_pad);
}

/*************************************************************************
**************************************************************************
#cat: init_rotgrids - Allocates and initializes a set of offsets that address
#cat:                 individual rotated pixels within a grid.
#cat:                 These rotated grids are used to conduct DFT analyses
#cat:                 on blocks of input image data, and they are used
#cat:                 in isotropic binarization.

   Input:
      iw        - width (in pixels) of the input image
      ih        - height (in pixels) of the input image
      pad       - designates the number of pixels to be padded to the perimeter
                  of the input image.  May be passed as UNDEFINED, in which
                  case the specific padding required by the rotated grids
                  will be computed and returned in ROTGRIDS.
      start_dir_angle - angle from which rotations are to start
      ndirs     - number of rotations to compute (within a semicircle)
      grid_w    - width of the grid in pixels to be rotated
      grid_h    - height of the grid in pixels to be rotated
      relative2 - designates whether pixel offsets whould be computed
                  relative to the ORIGIN or the CENTER of the grid
   Output:
      optr      - points to the allcated/initialized ROTGRIDS structure
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
int init_rotgrids(ROTGRIDS **optr, const int iw, const int ih, const int ipad,
                  const double start_dir_angle, const int ndirs,
                  const int grid_w, const int grid_h, const int relative2)
{
   ROTGRIDS *rotgrids;
   double pi_offset, pi_incr;
   int dir, ix, iy, grid_size, pw, grid_pad, min_dim;
   int *grid;
   double diag, theta, cs, sn, cx, cy;
   double fxm, fym, fx, fy;
   int ixt, iyt;
   double pad;

   /* Allocate structure */
   rotgrids = (ROTGRIDS *)malloc(sizeof(ROTGRIDS));
   if(rotgrids == (ROTGRIDS *)NULL){
      fprintf(stderr, "ERROR : init_rotgrids : malloc : rotgrids\n");
      return(-30);
   }

   /* Set rotgrid attributes */
   rotgrids->ngrids = ndirs;
   rotgrids->grid_w = grid_w;
   rotgrids->grid_h = grid_h;
   rotgrids->start_angle = start_dir_angle;
   rotgrids->relative2 = relative2;

   /* Compute pad based on diagonal of the grid */
   diag = sqrt((double)((grid_w*grid_w)+(grid_h*grid_h)));
   switch(relative2){
      case RELATIVE2CENTER:
         /* Assumption: all grid centers reside in valid/allocated memory. */
         pad = (diag-1)/(double)2.0;
         /* Need to truncate precision so that answers are consistent */
         /* on different computer architectures when rounding doubles. */
         pad = trunc_dbl_precision(pad, TRUNC_SCALE);
         grid_pad = sround(pad);
         break;
      case RELATIVE2ORIGIN:
         /* Assumption: all grid origins reside in valid/allocated memory. */
         min_dim = min(grid_w, grid_h);
         /* Compute pad as difference between the smallest grid dimension */
         /* and the diagonal distance of the grid. */
         pad = (diag-min_dim)/(double)2.0;
         /* Need to truncate precision so that answers are consistent */
         /* on different computer architectures when rounding doubles. */
         pad = trunc_dbl_precision(pad, TRUNC_SCALE);
         grid_pad = sround(pad);
         break;
      default:
         fprintf(stderr,
                 "ERROR : init_rotgrids : Illegal relative flag : %d\n",
                 relative2);
         free(rotgrids);
         return(-31);
   }

   /* If input padding is UNDEFINED ... */
   if(ipad == UNDEFINED)
      /* Use the padding specifically required by the rotated grids herein. */
      rotgrids->pad = grid_pad;
   else{
      /* Otherwise, input pad was specified, so check to make sure it is */
      /* sufficiently large to handle the rotated grids herein.          */
      if(ipad < grid_pad){
         /* If input pad is NOT large enough, then ERROR. */
         fprintf(stderr, "ERROR : init_rotgrids : Pad passed is too small\n");
         free(rotgrids);
         return(-32);
      }
      /* Otherwise, use the specified input pad in computing grid offsets. */
      rotgrids->pad = ipad;
   }

   /* Total number of points in grid */
   grid_size = grid_w * grid_h;

   /* Compute width of "padded" image */
   pw = iw + (rotgrids->pad<<1);

   /* Center coord of grid (0-oriented). */
   cx = (grid_w-1)/(double)2.0;
   cy = (grid_h-1)/(double)2.0;

   /* Allocate list of rotgrid pointers */
   rotgrids->grids = (int **)malloc(ndirs * sizeof(int *));
   if(rotgrids->grids == (int **)NULL){
      /* Free memory allocated to this point. */
      free(rotgrids);
      fprintf(stderr, "ERROR : init_rotgrids : malloc : rotgrids->grids\n");
      return(-33);
   }

   /* Pi_offset is the offset in radians from which angles are to begin. */
   pi_offset = start_dir_angle;
   pi_incr = M_PI/(double)ndirs;  /* if ndirs == 16, incr = 11.25 degrees */

   /* For each direction to rotate a grid ... */
   for (dir = 0, theta = pi_offset;
        dir < ndirs; dir++, theta += pi_incr) {

      /* Allocate a rotgrid */
      rotgrids->grids[dir] = (int *)malloc(grid_size * sizeof(int));
      if(rotgrids->grids[dir] == (int *)NULL){
         /* Free memory allocated to this point. */
         { int _j; for(_j = 0; _j < dir; _j++){
            free(rotgrids->grids[_j]);
         }}
         free(rotgrids);
         fprintf(stderr,
                 "ERROR : init_rotgrids : malloc : rotgrids->grids[dir]\n");
         return(-34);
      }

      /* Set pointer to current grid */
      grid = rotgrids->grids[dir];

      /* Compute cos and sin of current angle */
      cs = cos(theta);
      sn = sin(theta);

      /* This next section of nested FOR loops precomputes a         */
      /* rotated grid.  The rotation is set up to rotate a GRID_W X  */
      /* GRID_H grid on its center point at C=(Cx,Cy). The current   */
      /* pixel being rotated is P=(Ix,Iy).  Therefore, we have a     */
      /* rotation transformation of point P about pivot point C.     */
      /* The rotation transformation about a pivot point in matrix   */
      /* form is:                                                    */
      /*
            +-                                                       -+
            |             cos(T)                   sin(T)           0 |
  [Ix Iy 1] |            -sin(T)                   cos(T)           0 |
            | (1-cos(T))*Cx + Cy*sin(T)  (1-cos(T))*Cy - Cx*sin(T)  1 |
            +-                                                       -+
      */
      /* Multiplying the 2 matrices and combining terms yeilds the */
      /* equations for rotated coordinates (Rx, Ry):               */
      /*        Rx = Cx + (Ix - Cx)*cos(T) - (Iy - Cy)*sin(T)      */
      /*        Ry = Cy + (Ix - Cx)*sin(T) + (Iy - Cy)*cos(T)      */
      /*                                                           */
      /* Care has been taken to ensure that (for example) when     */
      /* BLOCKSIZE==24 the rotated indices stay within a centered  */
      /* 34X34 area.                                               */
      /* This is important for computing an accurate padding of    */
      /* the input image.  The rotation occurs "in-place" so that  */
      /* outer pixels in the grid are mapped at times from         */
      /* adjoining blocks.  As a result, to keep from accessing    */
      /* "unknown" memory or pixels wrapped from the other side of */
      /* the image, the input image should first be padded by      */
      /* PAD=round((DIAG - BLOCKSIZE)/2.0) where DIAG is the       */
      /* diagonal distance of the grid.                            */
      /* For example, when BLOCKSIZE==24, Dx=34, so PAD=5.         */

      /* Foreach each y coord in block ... */
      for (iy = 0; iy < grid_h; ++iy) {
         /* Compute rotation factors dependent on Iy (include constant) */
         fxm = -1.0 * ((iy - cy) * sn);
         fym = ((iy - cy) * cs);

         /* If offsets are to be relative to the grids origin, then */
         /* we need to subtract CX and CY.                          */
         if(relative2 == RELATIVE2ORIGIN){
            fxm += cx;
            fym += cy;
         }

         /* foreach each x coord in block ... */
         for (ix = 0; ix < grid_w; ++ix) {

             /* Now combine factors dependent on Iy with those of Ix */
             fx = fxm + ((ix - cx) * cs);
             fy = fym + ((ix - cx) * sn);
             /* Need to truncate precision so that answers are consistent */
             /* on different computer architectures when rounding doubles. */
             fx = trunc_dbl_precision(fx, TRUNC_SCALE);
             fy = trunc_dbl_precision(fy, TRUNC_SCALE);
             ixt = sround(fx);
             iyt = sround(fy);

             /* Store the current pixels relative   */
             /* rotated offset.  Make sure to       */
             /* multiply the y-component of the     */
             /* offset by the "padded" image width! */
             *grid++ = ixt + (iyt * pw);
         }/* ix */
      }/* iy */
   }/* dir */

   *optr = rotgrids;
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: alloc_dir_powers - Allocates the memory associated with DFT power
#cat:           vectors.  The DFT analysis is conducted block by block in the
#cat:           input image, and within each block, N wave forms are applied
#cat:           at M different directions.

   Input:
      nwaves - number of DFT wave forms
      ndirs  - number of orientations (directions) used in DFT analysis
   Output:
      opowers - pointer to the allcated power vectors
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
int alloc_dir_powers(double ***opowers, const int nwaves, const int ndirs)
{
   int w;
   double **powers;

   /* Allocate list of double pointers to hold power vectors */
   powers = (double **)malloc(nwaves * sizeof(double*));
   if(powers == (double **)NULL){
      fprintf(stderr, "ERROR : alloc_dir_powers : malloc : powers\n");
      return(-40);
   }
   /* Foreach DFT wave ... */
   for(w = 0; w < nwaves; w++){
      /* Allocate power vector for all directions */
      powers[w] = (double *)malloc(ndirs * sizeof(double));
      if(powers[w] == (double *)NULL){
         /* Free memory allocated to this point. */
         { int _j; for(_j = 0; _j < w; _j++){
            free(powers[_j]);
         }}
         free(powers);
         fprintf(stderr, "ERROR : alloc_dir_powers : malloc : powers[w]\n");
         return(-41);
      }
   }

   *opowers = powers;
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: alloc_power_stats - Allocates memory associated with set of statistics
#cat:             derived from DFT power vectors computed in a block of the
#cat:             input image.  Statistics are not computed for the lowest DFT
#cat:             wave form, so the length of the statistics arrays is 1 less
#cat:             than the number of DFT wave forms used.  The staistics
#cat:             include the Maximum power for each wave form, the direction
#cat:             at which the maximum power occured, and a normalized value
#cat:             for the maximum power.  In addition, the statistics are
#cat:             ranked in descending order based on normalized squared
#cat:             maximum power.

   Input:
      nstats - the number of waves forms from which statistics are to be
               derived (N Waves - 1)
   Output:
      owis      - points to an array to hold the ranked wave form indicies
                  of the corresponding statistics
      opowmaxs  - points to an array to hold the maximum DFT power for each
                  wave form
      opowmax_dirs - points to an array to hold the direction corresponding to
                  each maximum power value
      opownorms - points to an array to hold the normalized maximum power
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
int alloc_power_stats(int **owis, double **opowmaxs, int **opowmax_dirs,
                      double **opownorms, const int nstats)
{
   int *wis, *powmax_dirs;
   double *powmaxs, *pownorms;

   /* Allocate DFT wave index vector */
   wis = (int *)malloc(nstats * sizeof(int));
   if(wis == (int *)NULL){
      fprintf(stderr, "ERROR : alloc_power_stats : malloc : wis\n");
      return(-50);
   }

   /* Allocate max power vector */
   powmaxs = (double *)malloc(nstats * sizeof(double));
   if(powmaxs == (double *)NULL){
      /* Free memory allocated to this point. */
      free(wis);
      fprintf(stderr, "ERROR : alloc_power_stats : malloc : powmaxs\n");
      return(-51);
   }

   /* Allocate max power direction vector */
   powmax_dirs = (int *)malloc(nstats * sizeof(int));
   if(powmax_dirs == (int *)NULL){
      /* Free memory allocated to this point. */
      free(wis);
      free(powmaxs);
      fprintf(stderr, "ERROR : alloc_power_stats : malloc : powmax_dirs\n");
      return(-52);
   }

   /* Allocate normalized power vector */
   pownorms = (double *)malloc(nstats * sizeof(double));
   if(pownorms == (double *)NULL){
      /* Free memory allocated to this point. */
      free(wis);
      free(powmaxs);
      free(pownorms);
      fprintf(stderr, "ERROR : alloc_power_stats : malloc : pownorms\n");
      return(-53);
   }

   *owis = wis;
   *opowmaxs = powmaxs;
   *opowmax_dirs = powmax_dirs;
   *opownorms = pownorms;
   return(0);
}




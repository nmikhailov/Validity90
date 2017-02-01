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

      FILE:    GLOBALS.C
      AUTHOR:  Michael D. Garris
      DATE:    03/16/1999
      UPDATED: 10/04/1999 Version 2 by MDG

      Contains general global variable definitions required by the
      NIST Latent Fingerprint System (LFS).
***********************************************************************/

#include <lfs.h>

/*************************************************************************/
/*        GOBAL DECLARATIONS                                             */
/*************************************************************************/

#ifdef LOG_REPORT
FILE *g_logfp;
#endif

/* Constants (C) for defining 4 DFT frequencies, where  */
/* frequency is defined as C*(PI_FACTOR).  PI_FACTOR    */
/* regulates the period of the function in x, so:       */
/*      1 = one period in range X.                      */
/*      2 = twice the frequency in range X.             */
/*      3 = three times the frequency in reange X.      */
/*      4 = four times the frequency in ranage X.       */
double g_dft_coefs[NUM_DFT_WAVES] = { 1,2,3,4 };

/* Allocate and initialize a global LFS parameters structure. */
LFSPARMS g_lfsparms = {
   /* Image Controls */
   PAD_VALUE,
   JOIN_LINE_RADIUS,

   /* Map Controls */
   IMAP_BLOCKSIZE,
   UNUSED_INT,           /* windowsize */
   UNUSED_INT,           /* windowoffset */
   NUM_DIRECTIONS,
   START_DIR_ANGLE,
   RMV_VALID_NBR_MIN,
   DIR_STRENGTH_MIN,
   DIR_DISTANCE_MAX,
   SMTH_VALID_NBR_MIN,
   VORT_VALID_NBR_MIN,
   HIGHCURV_VORTICITY_MIN,
   HIGHCURV_CURVATURE_MIN,
   UNUSED_INT,           /* min_interpolate_nbrs */
   UNUSED_INT,           /* percentile_min_max   */
   UNUSED_INT,           /* min_contrast_delta   */

   /* DFT Controls */
   NUM_DFT_WAVES,
   POWMAX_MIN,
   POWNORM_MIN,
   POWMAX_MAX,
   FORK_INTERVAL,
   FORK_PCT_POWMAX,
   FORK_PCT_POWNORM,

   /* Binarization Controls */
   DIRBIN_GRID_W,
   DIRBIN_GRID_H,
   ISOBIN_GRID_DIM,
   NUM_FILL_HOLES,

   /* Minutiae Detection Controls */
   MAX_MINUTIA_DELTA,
   MAX_HIGH_CURVE_THETA,
   HIGH_CURVE_HALF_CONTOUR,
   MIN_LOOP_LEN,
   MIN_LOOP_ASPECT_DIST,
   MIN_LOOP_ASPECT_RATIO,

   /* Minutiae Link Controls */
   LINK_TABLE_DIM,
   MAX_LINK_DIST,
   MIN_THETA_DIST,
   MAXTRANS,
   SCORE_THETA_NORM,
   SCORE_DIST_NORM,
   SCORE_DIST_WEIGHT,
   SCORE_NUMERATOR,

   /* False Minutiae Removal Controls */
   MAX_RMTEST_DIST,
   MAX_HOOK_LEN,
   MAX_HALF_LOOP,
   TRANS_DIR_PIX,
   SMALL_LOOP_LEN,
   SIDE_HALF_CONTOUR,
   INV_BLOCK_MARGIN,
   RM_VALID_NBR_MIN,
   UNUSED_INT,         /* max_overlap_dist       */
   UNUSED_INT,         /* max_overlap_join_dist  */
   UNUSED_INT,         /* malformation_steps_1   */
   UNUSED_INT,         /* malformation_steps_2   */
   UNUSED_DBL,         /* min_malformation_ratio */
   UNUSED_INT,         /* max_malformation_dist  */
   PORES_TRANS_R,
   PORES_PERP_STEPS,
   PORES_STEPS_FWD,
   PORES_STEPS_BWD,
   PORES_MIN_DIST2,
   PORES_MAX_RATIO,
   FALSE, /* not removing perimeter points by default */
   PERIMETER_PTS_DISTANCE,

   /* Ridge Counting Controls */
   MAX_NBRS,
   MAX_RIDGE_STEPS
};


/* Allocate and initialize VERSION 2 global LFS parameters structure. */
LFSPARMS g_lfsparms_V2 = {
   /* Image Controls */
   PAD_VALUE,
   JOIN_LINE_RADIUS,

   /* Map Controls */
   MAP_BLOCKSIZE_V2,
   MAP_WINDOWSIZE_V2,
   MAP_WINDOWOFFSET_V2,
   NUM_DIRECTIONS,
   START_DIR_ANGLE,
   RMV_VALID_NBR_MIN,
   DIR_STRENGTH_MIN,
   DIR_DISTANCE_MAX,
   SMTH_VALID_NBR_MIN,
   VORT_VALID_NBR_MIN,
   HIGHCURV_VORTICITY_MIN,
   HIGHCURV_CURVATURE_MIN,
   MIN_INTERPOLATE_NBRS,
   PERCENTILE_MIN_MAX,
   MIN_CONTRAST_DELTA,

   /* DFT Controls */
   NUM_DFT_WAVES,
   POWMAX_MIN,
   POWNORM_MIN,
   POWMAX_MAX,
   FORK_INTERVAL,
   FORK_PCT_POWMAX,
   FORK_PCT_POWNORM,

   /* Binarization Controls */
   DIRBIN_GRID_W,
   DIRBIN_GRID_H,
   UNUSED_INT,          /* isobin_grid_dim */
   NUM_FILL_HOLES,

   /* Minutiae Detection Controls */
   MAX_MINUTIA_DELTA,
   MAX_HIGH_CURVE_THETA,
   HIGH_CURVE_HALF_CONTOUR,
   MIN_LOOP_LEN,
   MIN_LOOP_ASPECT_DIST,
   MIN_LOOP_ASPECT_RATIO,

   /* Minutiae Link Controls */
   UNUSED_INT,          /* link_table_dim     */
   UNUSED_INT,          /* max_link_dist      */
   UNUSED_INT,          /* min_theta_dist     */
   MAXTRANS,            /* used for removing overlaps as well */
   UNUSED_DBL,          /* score_theta_norm   */
   UNUSED_DBL,          /* score_dist_norm    */
   UNUSED_DBL,          /* score_dist_weight  */
   UNUSED_DBL,          /* score_numerator    */

   /* False Minutiae Removal Controls */
   MAX_RMTEST_DIST_V2,
   MAX_HOOK_LEN_V2,
   MAX_HALF_LOOP_V2,
   TRANS_DIR_PIX_V2,
   SMALL_LOOP_LEN,
   SIDE_HALF_CONTOUR,
   INV_BLOCK_MARGIN_V2,
   RM_VALID_NBR_MIN,
   MAX_OVERLAP_DIST,
   MAX_OVERLAP_JOIN_DIST,
   MALFORMATION_STEPS_1,
   MALFORMATION_STEPS_2,
   MIN_MALFORMATION_RATIO,
   MAX_MALFORMATION_DIST,
   PORES_TRANS_R,
   PORES_PERP_STEPS,
   PORES_STEPS_FWD,
   PORES_STEPS_BWD,
   PORES_MIN_DIST2,
   PORES_MAX_RATIO,
   FALSE, /* not removing perimeter points by default */
   PERIMETER_PTS_DISTANCE,

   /* Ridge Counting Controls */
   MAX_NBRS,
   MAX_RIDGE_STEPS
};

/* Variables for conducting 8-connected neighbor analyses. */
/* Pixel neighbor offsets:  0  1  2  3  4  5  6  7  */     /* 7 0 1 */
int g_nbr8_dx[] =          {  0, 1, 1, 1, 0,-1,-1,-1 };      /* 6 C 2 */
int g_nbr8_dy[] =          { -1,-1, 0, 1, 1, 1, 0,-1 };      /* 5 4 3 */

/* The chain code lookup matrix for 8-connected neighbors. */
/* Should put this in globals.                             */
int g_chaincodes_nbr8[]={ 3, 2, 1,
                        4,-1, 0,
                        5, 6, 7};

/* Global array of feature pixel pairs. */
FEATURE_PATTERN g_feature_patterns[]=
                       {{RIDGE_ENDING,  /* a. Ridge Ending (appearing) */
                         APPEARING,
                         {0,0},
                         {0,1},
                         {0,0}},

                        {RIDGE_ENDING,  /* b. Ridge Ending (disappearing) */
                         DISAPPEARING,
                         {0,0},
                         {1,0},
                         {0,0}},

                        {BIFURCATION,   /* c. Bifurcation (disappearing) */
                         DISAPPEARING,
                         {1,1},
                         {0,1},
                         {1,1}},

                        {BIFURCATION,   /* d. Bifurcation (appearing) */
                         APPEARING,
                         {1,1},
                         {1,0},
                         {1,1}},

                        {BIFURCATION,   /* e. Bifurcation (disappearing) */
                         DISAPPEARING,
                         {1,0},
                         {0,1},
                         {1,1}},

                        {BIFURCATION,   /* f. Bifurcation (disappearing) */
                         DISAPPEARING,
                         {1,1},
                         {0,1},
                         {1,0}},

                        {BIFURCATION,   /* g. Bifurcation (appearing) */
                         APPEARING,
                         {1,1},
                         {1,0},
                         {0,1}},

                        {BIFURCATION,   /* h. Bifurcation (appearing) */
                         APPEARING,
                         {0,1},
                         {1,0},
                         {1,1}},

                        {BIFURCATION,   /* i. Bifurcation (disappearing) */
                         DISAPPEARING,
                         {1,0},
                         {0,1},
                         {1,0}},

                        {BIFURCATION,   /* j. Bifurcation (appearing) */
                         APPEARING,
                         {0,1},
                         {1,0},
                         {0,1}}};

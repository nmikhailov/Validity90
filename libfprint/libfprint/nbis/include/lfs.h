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

#ifndef _LFS_H
#define _LFS_H

/***********************************************************************
               PACKAGE: NIST Latent Fingerprint System
               AUTHOR:  Michael D. Garris
               DATE:    03/16/1999
               UPDATED: 10/04/1999 Version 2 by MDG
               UPDATED: 10/26/1999 by MDG
                        Comments added to guide changes to blocksize
                        or number of detected directions.
               UPDATED: 03/11/2005 by MDG

               FILE:    LFS.H

      Contains all custom structure definitions, constant definitions,
      external function definitions, and external global variable
      definitions required by the NIST Latent Fingerprint System (LFS).
***********************************************************************/

#include <math.h>
#include <stdio.h>
#include <fp_internal.h>

/*************************************************************************/
/*        OUTPUT FILE EXTENSIONS                                         */
/*************************************************************************/
#define MIN_TXT_EXT           "min"
#define LOW_CONTRAST_MAP_EXT  "lcm"
#define HIGH_CURVE_MAP_EXT    "hcm"
#define DIRECTION_MAP_EXT     "dm"
#define LOW_FLOW_MAP_EXT      "lfm"
#define QUALITY_MAP_EXT       "qm"
#define AN2K_OUT_EXT          "mdt"
#define BINARY_IMG_EXT        "brw"
#define XYT_EXT               "xyt"

/*************************************************************************/
/*        MINUTIAE XYT REPRESENTATION SCHEMES                            */
/*************************************************************************/
#define NIST_INTERNAL_XYT_REP  0
#define M1_XYT_REP             1

/*************************************************************************/
/*        MACRO DEFINITIONS                                              */
/*************************************************************************/

#define max(a, b)   ((a) > (b) ? (a) : (b))
#define min(a, b)   ((a) < (b) ? (a) : (b))
#define sround(x) ((int) (((x)<0) ? (x)-0.5 : (x)+0.5))
#define trunc_dbl_precision(x, scale) ((double) (((x)<0.0) \
                 ? ((int)(((x)*(scale))-0.5))/(scale) \
                 : ((int)(((x)*(scale))+0.5))/(scale)))

#ifndef M_PI
#define M_PI		3.14159265358979323846	/* pi */
#endif

/*************************************************************************/
/*        STRUCTURE DEFINITIONS                                          */
/*************************************************************************/

/* Lookup tables for converting from integer directions */
/* to angles in radians.                                */
typedef struct dir2rad{
   int ndirs;
   double *cos;
   double *sin;
} DIR2RAD;

/* DFT wave form structure containing both cosine and   */
/* sine components for a specific frequency.            */
typedef struct dftwave{
   double *cos;
   double *sin;
} DFTWAVE;

/* DFT wave forms structure containing all wave forms  */
/* to be used in DFT analysis.                         */
typedef struct dftwaves{
   int nwaves;
   int wavelen;
   DFTWAVE **waves;
}DFTWAVES;

/* Rotated pixel offsets for a grid of specified dimensions */
/* rotated at a specified number of different orientations  */
/* (directions).  This structure used by the DFT analysis   */
/* when generating a Direction Map and also for conducting  */
/* isotropic binarization.                                  */
typedef struct rotgrids{
   int pad;
   int relative2;
   double start_angle;
   int ngrids;
   int grid_w;
   int grid_h;
   int **grids;
} ROTGRIDS;

/*************************************************************************/
/* 10, 2X3 pixel pair feature patterns used to define ridge endings      */
/* and bifurcations.                                                     */
/* 2nd pixel pair is permitted to repeat multiple times in match.        */
#define NFEATURES      10
#define BIFURCATION     0
#define RIDGE_ENDING    1
#define DISAPPEARING    0
#define APPEARING       1

typedef struct fp_minutia MINUTIA;
typedef struct fp_minutiae MINUTIAE;

typedef struct feature_pattern{
   int type;
   int appearing;
   int first[2];
   int second[2];
   int third[2];
} FEATURE_PATTERN;

/* SHAPE structure definitions. */
typedef struct rows{
   int y;         /* Y-coord of current row in shape.                  */
   int *xs;       /* X-coords for shape contour points on current row. */
   int alloc;     /* Number of points allocate for x-coords on row.    */
   int npts;      /* Number of points assigned for x-coords on row.    */
} ROW;

typedef struct shape{
   int ymin;      /* Y-coord of top-most scanline in shape.     */
   int ymax;      /* Y-coord of bottom-most scanline in shape.  */
   ROW **rows;    /* List of row pointers comprising the shape. */
   int alloc;     /* Number of rows allocated for shape.        */
   int nrows;     /* Number of rows assigned to shape.          */
} SHAPE;

/* Parameters used by LFS for setting thresholds and  */
/* defining testing criterion.                        */
typedef struct lfsparms{
   /* Image Controls */
   int    pad_value;
   int    join_line_radius;

   /* Map Controls */
   int    blocksize;       /* Pixel dimension image block.                 */
   int    windowsize;      /* Pixel dimension window surrounding block.    */
   int    windowoffset;    /* Offset in X & Y from block to window origin. */
   int    num_directions;
   double start_dir_angle;
   int    rmv_valid_nbr_min;
   double dir_strength_min;
   int    dir_distance_max;
   int    smth_valid_nbr_min;
   int    vort_valid_nbr_min;
   int    highcurv_vorticity_min;
   int    highcurv_curvature_min;
   int    min_interpolate_nbrs;
   int    percentile_min_max;
   int    min_contrast_delta;

   /* DFT Controls */
   int    num_dft_waves;
   double powmax_min;
   double pownorm_min;
   double powmax_max;
   int    fork_interval;
   double fork_pct_powmax;
   double fork_pct_pownorm;

   /* Binarization Controls */
   int    dirbin_grid_w;
   int    dirbin_grid_h;
   int    isobin_grid_dim;
   int    num_fill_holes;

   /* Minutiae Detection Controls */
   int    max_minutia_delta;
   double max_high_curve_theta;
   int    high_curve_half_contour;
   int    min_loop_len;
   double min_loop_aspect_dist;
   double min_loop_aspect_ratio;

   /* Minutiae Link Controls */
   int    link_table_dim;
   int    max_link_dist;
   int    min_theta_dist;
   int    maxtrans;
   double score_theta_norm;
   double score_dist_norm;
   double score_dist_weight;
   double score_numerator;

   /* False Minutiae Removal Controls */
   int    max_rmtest_dist;
   int    max_hook_len;
   int    max_half_loop;
   int    trans_dir_pix;
   int    small_loop_len;
   int    side_half_contour;
   int    inv_block_margin;
   int    rm_valid_nbr_min;
   int    max_overlap_dist;
   int    max_overlap_join_dist;
   int    malformation_steps_1;
   int    malformation_steps_2;
   double min_malformation_ratio;
   int    max_malformation_dist;
   int    pores_trans_r;
   int    pores_perp_steps;
   int    pores_steps_fwd;
   int    pores_steps_bwd;
   double pores_min_dist2;
   double pores_max_ratio;
   int    remove_perimeter_pts;
   int    min_pp_distance;

   /* Ridge Counting Controls */
   int    max_nbrs;
   int    max_ridge_steps;
} LFSPARMS;

/*************************************************************************/
/*        LFS CONSTANT DEFINITIONS                                       */
/*************************************************************************/

/***** IMAGE CONSTANTS *****/

#ifndef DEFAULT_PPI
#define DEFAULT_PPI            500
#endif

/* Intensity used to fill padded image area */
#define PAD_VALUE              128   /* medium gray @ 8 bits */

/* Intensity used to draw on grayscale images */
#define DRAW_PIXEL             255   /* white in 8 bits */

/* Definitions for 8-bit binary pixel intensities. */
#define WHITE_PIXEL            255
#define BLACK_PIXEL              0

/* Definitions for controlling join_miutia(). */
/* Draw without opposite perimeter pixels.  */
#define NO_BOUNDARY              0

/* Draw with opposite perimeter pixels.     */
#define WITH_BOUNDARY            1

/* Radial width added to join line (not including the boundary pixels). */
#define JOIN_LINE_RADIUS         1


/***** MAP CONSTANTS *****/

/* Map value for not well-defined directions */
#define INVALID_DIR             -1

/* Map value assigned when the current block has no neighbors */
/* with valid direction.                                      */
#define NO_VALID_NBRS           -3

/* Map value designating a block is near a high-curvature */
/* area such as a core or delta.                          */
#define HIGH_CURVATURE          -2

/* This specifies the pixel dimensions of each block in the IMAP */
#define IMAP_BLOCKSIZE          24

/* Pixel dimension of image blocks. The following three constants work */
/* together to define a system of 8X8 adjacent and non-overlapping     */
/* blocks that are assigned results from analyzing a larger 24X24      */
/* window centered about each of the 8X8 blocks.                       */
/* CAUTION: If MAP_BLOCKSIZE_V2 is changed, then the following will    */
/* likely need to be changed:  MAP_WINDOWOFFSET_V2,                    */
/*                             TRANS_DIR_PIX_V2,                       */
/*                             INV_BLOCK_MARGIN_V2                     */
#define MAP_BLOCKSIZE_V2         8

/* Pixel dimension of window that surrounds the block.  The result from    */
/* analyzing the content of the window is stored in the interior block.    */
#define MAP_WINDOWSIZE_V2       24

/* Pixel offset in X & Y from the origin of the block to the origin of */
/* the surrounding window.                                             */
#define MAP_WINDOWOFFSET_V2      8

/* This is the number of integer directions to be used in semicircle. */
/* CAUTION: If NUM_DIRECTIONS is changed, then the following will     */
/* likely need to be changed:  HIGHCURV_VORTICITY_MIN,                */
/*                             HIGHCURV_CURVATURE_MIN,                */
/*                             FORK_INTERVAL                          */
#define NUM_DIRECTIONS          16

/* This is the theta from which integer directions   */
/* are to begin.                                     */
#define START_DIR_ANGLE     (double)(M_PI/2.0)    /* 90 degrees */

/* Minimum number of valid neighbors required for a        */
/* valid block value to keep from being removed.           */
#define RMV_VALID_NBR_MIN        3

/* Minimum strength for a direction to be considered significant. */
#define DIR_STRENGTH_MIN         0.2

/* Maximum distance allowable between valid block direction */
/* and the average direction of its neighbors before the    */
/* direction is removed.                                    */
#define DIR_DISTANCE_MAX         3

/* Minimum number of valid neighbors required for an       */
/* INVALID block direction to receive its direction from   */
/* the average of its neighbors.                           */
#define SMTH_VALID_NBR_MIN       7

/* Minimum number of valid neighbors required for a block  */
/* with an INVALID block direction to be measured for      */
/* vorticity.                                              */
#define VORT_VALID_NBR_MIN       7

/* The minimum vorticity value whereby an INVALID block       */
/* is determined to be high-curvature based on the directions */
/* of it neighbors.                                           */
#define HIGHCURV_VORTICITY_MIN   5

/* The minimum curvature value whereby a VALID direction block is  */
/* determined to be high-curvature based on it value compared with */
/* its neighbors' directions.                                      */
#define HIGHCURV_CURVATURE_MIN   5

/* Minimum number of neighbors with VALID direction for an INVALID         */
/* directon block to have its direction interpolated from those neighbors. */
#define MIN_INTERPOLATE_NBRS     2

/* Definitions for creating a low contrast map. */
/* Percentile cut off for choosing min and max pixel intensities */
/* in a block.                                                   */
#define PERCENTILE_MIN_MAX      10

/* The minimum delta between min and max percentile pixel intensities */
/* in block for block NOT to be considered low contrast.  (Note that  */
/* this value is in terms of 6-bit pixels.)                           */
#define MIN_CONTRAST_DELTA       5


/***** DFT CONSTANTS *****/

/* This specifies the number of DFT wave forms to be applied */
#define NUM_DFT_WAVES            4

/* Minimum total DFT power for any given block  */
/* which is used to compute an average power.   */
/* By setting a non-zero minimum total,possible */
/* division by zero is avoided.  This value was */
/* taken from HO39.                             */
#define MIN_POWER_SUM           10.0

/* Thresholds and factors used by HO39.  Renamed     */
/* here to give more meaning.                        */
                                                     /* HO39 Name=Value */
/* Minimum DFT power allowable in any one direction. */ 
#define POWMAX_MIN          100000.0                 /*     thrhf=1e5f  */

/* Minimum normalized power allowable in any one     */
/* direction.                                        */
#define POWNORM_MIN              3.8                 /*      disc=3.8f  */

/* Maximum power allowable at the lowest frequency   */
/* DFT wave.                                         */
#define POWMAX_MAX        50000000.0                 /*     thrlf=5e7f  */

/* Check for a fork at +- this number of units from  */
/* current integer direction.  For example,          */
/*           2 dir ==> 11.25 X 2 degrees.            */
#define FORK_INTERVAL            2

/* Minimum DFT power allowable at fork angles is     */
/* FORK_PCT_POWMAX X block's max directional power.  */
#define FORK_PCT_POWMAX          0.7

/* Minimum normalized power allowable at fork angles */
/* is FORK_PCT_POWNORM X POWNORM_MIN                 */
#define FORK_PCT_POWNORM         0.75


/***** BINRAIZATION CONSTANTS *****/

/* Directional binarization grid dimensions. */
#define DIRBIN_GRID_W            7
#define DIRBIN_GRID_H            9

/* The pixel dimension (square) of the grid used in isotropic      */
/* binarization.                                                   */
#define ISOBIN_GRID_DIM         11

/* Number of passes through the resulting binary image where holes */
/* of pixel length 1 in horizontal and vertical runs are filled.   */
#define NUM_FILL_HOLES           3


/***** MINUTIAE DETECTION CONSTANTS *****/

/* The maximum pixel translation distance in X or Y within which */
/* two potential minutia points are to be considered similar.    */
#define MAX_MINUTIA_DELTA       10

/* If the angle of a contour exceeds this angle, then it is NOT */
/* to be considered to contain minutiae.                         */
#define MAX_HIGH_CURVE_THETA  (double)(M_PI/3.0)

/* Half the length in pixels to be extracted for a high-curvature contour. */
#define HIGH_CURVE_HALF_CONTOUR 14

/* Loop must be larger than this threshold (in pixels) to be considered */
/* to contain minutiae.                                                  */
#define MIN_LOOP_LEN            20

/* If loop's minimum distance half way across its contour is less than */
/* this threshold, then loop is tested for minutiae.                    */
#define MIN_LOOP_ASPECT_DIST     1.0

/* If ratio of loop's maximum/minimum distances half way across its   */
/* contour is >=  to this threshold, then loop is tested for minutiae. */
#define MIN_LOOP_ASPECT_RATIO    2.25

/* There are 10 unique feature patterns with ID = [0..9] , */
/* so set LOOP ID to 10 (one more than max pattern ID).    */
#define LOOP_ID                 10

/* Definitions for controlling the scanning of minutiae. */
#define SCAN_HORIZONTAL          0
#define SCAN_VERTICAL            1
#define SCAN_CLOCKWISE           0
#define SCAN_COUNTER_CLOCKWISE   1

/* The dimension of the chaincode loopkup matrix. */
#define NBR8_DIM                 3

/* Default minutiae reliability. */
#define DEFAULT_RELIABILITY      0.99

/* Medium minutia reliability. */
#define MEDIUM_RELIABILITY       0.50

/* High minutia reliability. */
#define HIGH_RELIABILITY         0.99


/***** MINUTIAE LINKING CONSTANTS *****/

/* Definitions for controlling the linking of minutiae. */
/* Square dimensions of 2D table of potentially linked minutiae. */
#define LINK_TABLE_DIM          20

/* Distance (in pixels) used to determine if the orthogonal distance  */
/* between the coordinates of 2 minutia points are sufficiently close */
/* to be considered for linking.                                      */
#define MAX_LINK_DIST           20

/* Minimum distance (in pixels) between 2 minutia points that an angle */
/* computed between the points may be considered reliable.             */
#define MIN_THETA_DIST           5

/* Maximum number of transitions along a contiguous pixel trajectory    */
/* between 2 minutia points for that trajectory to be considered "free" */
/* of obstacles.                                                        */
#define MAXTRANS                 2

/* Parameters used to compute a link score between 2 minutiae. */
#define SCORE_THETA_NORM        15.0
#define SCORE_DIST_NORM         10.0
#define SCORE_DIST_WEIGHT        4.0
#define SCORE_NUMERATOR      32000.0


/***** FALSE MINUTIAE REMOVAL CONSTANTS *****/

/* Definitions for removing hooks, islands, lakes, and overlaps. */
/* Distance (in pixels) used to determine if the orthogonal distance  */
/* between the coordinates of 2 minutia points are sufficiently close */
/* to be considered for removal.                                      */
#define MAX_RMTEST_DIST          8

#define MAX_RMTEST_DIST_V2      16

/* Length of pixel contours to be traced and analyzed for possible hooks. */
#define MAX_HOOK_LEN            15

#define MAX_HOOK_LEN_V2         30

/* Half the maximum length of pixel contours to be traced and analyzed */
/* for possible loops (islands/lakes).                                 */
#define MAX_HALF_LOOP           15

#define MAX_HALF_LOOP_V2        30

/* Definitions for removing minutiae that are sufficiently close and */
/* point to a block with invalid ridge flow.                         */
/* Distance (in pixels) in direction opposite the minutia to be */
/* considered sufficiently close to an invalid block.           */
#define TRANS_DIR_PIX            6

#define TRANS_DIR_PIX_V2         4

/* Definitions for removing small holes (islands/lakes).  */
/* Maximum circumference (in pixels) of qualifying loops. */
#define SMALL_LOOP_LEN          15

/* Definitions for removing or adusting side minutiae. */
/* Half the number of pixels to be traced to form a complete contour. */
#define SIDE_HALF_CONTOUR        7

/* Definitions for removing minutiae near invalid blocks. */
/* Maximum orthogonal distance a minutia can be neighboring a block with */
/* invalid ridge flow in order to be removed.                            */
#define INV_BLOCK_MARGIN         6

#define INV_BLOCK_MARGIN_V2      4

/* Given a sufficiently close, neighboring invalid block, if that invalid */
/* block has a total number of neighboring blocks with valid ridge flow   */
/* less than this threshold, then the minutia point is removed.           */
#define RM_VALID_NBR_MIN         7

/* Definitions for removing overlaps. */
/* Maximum pixel distance between 2 points to be tested for overlapping */
/* conditions.                                                          */
#define MAX_OVERLAP_DIST         8

/* Maximum pixel distance between 2 points on opposite sides of an overlap */
/* will be joined.                                                         */
#define MAX_OVERLAP_JOIN_DIST    6

/* Definitions for removing "irregularly-shaped" minutiae. */
/* Contour steps to be traced to 1st measuring point. */
#define MALFORMATION_STEPS_1    10
/* Contour steps to be traced to 2nd measuring point. */
#define MALFORMATION_STEPS_2    20
/* Minimum ratio of distances across feature at the two point to be */
/* considered normal.                                               */
#define MIN_MALFORMATION_RATIO   2.0
/* Maximum distance permitted across feature to be considered normal. */
#define MAX_MALFORMATION_DIST   20

/* Definitions for removing minutiae on pores. */
/* Translation distance (in pixels) from minutia point in opposite direction */
/* in order to get off a valley edge and into the neighboring ridge.         */
#define PORES_TRANS_R            3

/* Number of steps (in pixels) to search for edge of current ridge. */
#define PORES_PERP_STEPS        12

/* Number of pixels to be traced to find forward contour points. */
#define PORES_STEPS_FWD         10

/* Number of pixels to be traced to find backward contour points. */
#define PORES_STEPS_BWD          8

/* Minimum squared distance between points before being considered zero. */
#define PORES_MIN_DIST2          0.5

/* Max ratio of computed distances between pairs of forward and backward */
/* contour points to be considered a pore.                               */
#define PORES_MAX_RATIO          2.25

/* Points which are closer than this distance to scan perimeter will be removed */
#define PERIMETER_PTS_DISTANCE 10


/***** RIDGE COUNTING CONSTANTS *****/

/* Definitions for detecting nearest neighbors and counting ridges. */
/* Maximum number of nearest neighbors per minutia. */
#define MAX_NBRS                 5

/* Maximum number of contour steps taken to validate a ridge crossing. */
#define MAX_RIDGE_STEPS         10

/*************************************************************************/
/*         QUALITY/RELIABILITY DEFINITIONS                               */
/*************************************************************************/
/* Quality map levels */
#define QMAP_LEVELS  5

/* Neighborhood radius in millimeters computed from 11 pixles */
/* scanned at 19.69 pixels/mm. */
#define RADIUS_MM  ((double)(11.0 / 19.69))

/* Ideal Standard Deviation of pixel values in a neighborhood. */
#define IDEALSTDEV  64
/* Ideal Mean of pixel values in a neighborhood. */
#define IDEALMEAN    127

/* Look for neighbors this many blocks away. */
#define NEIGHBOR_DELTA 2

/*************************************************************************/
/*         GENERAL DEFINITIONS                                           */
/*************************************************************************/
#define LFS_VERSION_STR         "NIST_LFS_VER2"

/* This factor converts degrees to radians. */
#ifndef DEG2RAD
#define DEG2RAD             (double)(M_PI/180.0)
#endif

#define NORTH                    0
#define SOUTH                    4
#define EAST                     2
#define WEST                     6

#ifndef TRUE
#define TRUE                     1
#endif
#ifndef FALSE
#define FALSE                    0
#endif

#ifndef FOUND
#define FOUND                 TRUE
#endif
#ifndef NOT_FOUND
#define NOT_FOUND            FALSE
#endif

#define HOOK_FOUND               1
#define LOOP_FOUND               1
#define IGNORE                   2
#define LIST_FULL                3
#define INCOMPLETE               3

/* Pixel value limit in 6-bit image. */
#define IMG_6BIT_PIX_LIMIT      64

/* Maximum number (or reallocated chunks) of minutia to be detected */
/* in an image.                                                     */
#define MAX_MINUTIAE          1000

/* If both deltas in X and Y for a line of specified slope is less than */
/* this threshold, then the angle for the line is set to 0 radians.     */
#define MIN_SLOPE_DELTA          0.5

/* Designates that rotated grid offsets should be relative */
/* to the grid's center.                                   */
#define RELATIVE2CENTER          0

/* Designates that rotated grid offsets should be relative */
/* to the grid's origin.                                   */
#define RELATIVE2ORIGIN          1

/* Truncate floating point precision by multiply, rounding, and then */
/* dividing by this value.  This enables consistant results across   */
/* different computer architectures.                                 */
#define TRUNC_SCALE          16384.0

/* Designates passed argument as undefined. */
#define UNDEFINED               -1

/* Dummy values for unused LFS control parameters. */
#define UNUSED_INT               0
#define UNUSED_DBL               0.0

/*************************************************************************/
/*        EXTERNAL FUNCTION DEFINITIONS                                  */
/*************************************************************************/

/* binar.c */
extern int binarize_V2(unsigned char **, int *, int *,
                     unsigned char *, const int, const int,
                     int *, const int, const int,
                     const ROTGRIDS *, const LFSPARMS *);
extern int binarize_image_V2(unsigned char **, int *, int *,
                     unsigned char *, const int, const int,
                     const int *, const int, const int,
                     const int, const ROTGRIDS *);
extern int dirbinarize(const unsigned char *, const int, const ROTGRIDS *);

/* block.c */
extern int block_offsets(int **, int *, int *, const int, const int,
                     const int, const int);
extern int low_contrast_block(const int, const int,
                     unsigned char *, const int, const int, const LFSPARMS *);
extern int find_valid_block(int *, int *, int *, int *, int *,
                     const int, const int, const int, const int,
                     const int, const int);
extern void set_margin_blocks(int *, const int, const int, const int);

/* contour.c */
int allocate_contour(int **ocontour_x, int **ocontour_y,
                     int **ocontour_ex, int **ocontour_ey, const int ncontour);
extern void free_contour(int *, int *, int *, int *);
extern int get_high_curvature_contour(int **, int **, int **, int **, int *,
                     const int, const int, const int, const int, const int,
                     unsigned char *, const int, const int);
extern int get_centered_contour(int **, int **, int **, int **, int *,
                     const int, const int, const int, const int, const int,
                     unsigned char *, const int, const int);
extern int trace_contour(int **, int **, int **, int **, int *,
                     const int, const int, const int, const int, const int,
                     const int, const int, const int,
                     unsigned char *, const int, const int);
extern int search_contour(const int, const int, const int,
                     const int, const int, const int, const int, const int,
                     unsigned char *, const int, const int);
extern int min_contour_theta(int *, double *, const int, const int *,
                     const int *, const int);
extern void contour_limits(int *, int *, int *, int *, const int *,
                     const int *, const int);
extern void fix_edge_pixel_pair(int *, int *, int *, int *,
                     unsigned char *, const int, const int);

/* detect.c */
extern int get_minutiae(MINUTIAE **, int **, int **, int **,
                 int **, int **, int *, int *,
                 unsigned char **, int *, int *, int *,
                 unsigned char *, const int, const int,
                 const int, const double, const LFSPARMS *);

/* dft.c */
extern int dft_dir_powers(double **, unsigned char *, const int,
                     const int, const int, const DFTWAVES *,
                     const ROTGRIDS *);
extern int dft_power_stats(int *, double *, int *, double *, double **,
                     const int, const int, const int);

/* free.c */
extern void free_dir2rad(DIR2RAD *);
extern void free_dftwaves(DFTWAVES *);
extern void free_rotgrids(ROTGRIDS *);
extern void free_dir_powers(double **, const int);

/* imgutil.c */
extern void bits_6to8(unsigned char *, const int, const int);
extern void bits_8to6(unsigned char *, const int, const int);
extern void gray2bin(const int, const int, const int,
                     unsigned char *, const int, const int);
extern int pad_uchar_image(unsigned char **, int *, int *,
                     unsigned char *, const int, const int, const int,
                     const int);
extern void fill_holes(unsigned char *, const int, const int);
extern int free_path(const int, const int, const int, const int,
                     unsigned char *, const int, const int, const LFSPARMS *);
extern int search_in_direction(int *, int *, int *, int *, const int,
                     const int, const int, const double, const double,
                     const int, unsigned char *, const int, const int);

/* init.c */
extern int init_dir2rad(DIR2RAD **, const int);
extern int init_dftwaves(DFTWAVES **, const double *, const int, const int);
extern int get_max_padding_V2(const int, const int, const int, const int);
extern int init_rotgrids(ROTGRIDS **, const int, const int, const int,
                     const double, const int, const int, const int, const int);
extern int alloc_dir_powers(double ***, const int, const int);
extern int alloc_power_stats(int **, double **, int **, double **, const int);

/* line.c */
extern int line_points(int **, int **, int *,
                     const int, const int, const int, const int);

/* loop.c */
extern int get_loop_list(int **, MINUTIAE *, const int, unsigned char *,
                     const int, const int);
extern int on_loop(const MINUTIA *, const int, unsigned char *, const int,
                     const int);
extern int on_island_lake(int **, int **, int **, int **, int *,
                     const MINUTIA *, const MINUTIA *, const int,
                     unsigned char *, const int, const int);
extern int on_hook(const MINUTIA *, const MINUTIA *, const int,
                     unsigned char *, const int, const int);
extern int is_loop_clockwise(const int *, const int *, const int, const int);
extern int process_loop(MINUTIAE *, const int *, const int *,
                     const int *, const int *, const int,
                     unsigned char *, const int, const int, const LFSPARMS *);
extern int process_loop_V2(MINUTIAE *, const int *, const int *,
                     const int *, const int *, const int,
                     unsigned char *, const int, const int,
                     int *, const LFSPARMS *);
extern int fill_loop(const int *, const int *, const int,
                     unsigned char *, const int, const int);

/* maps.c */
extern int gen_image_maps(int **, int **, int **, int **, int *, int *,
                    unsigned char *, const int, const int,
                    const DIR2RAD *, const DFTWAVES *,
                    const ROTGRIDS *, const LFSPARMS *);
extern int gen_initial_maps(int **, int **, int **,
                    int *, const int, const int,
                    unsigned char *, const int, const int,
                    const DFTWAVES *, const  ROTGRIDS *, const LFSPARMS *);
extern int interpolate_direction_map(int *, int *, const int, const int,
                    const LFSPARMS *);
extern int morph_TF_map(int *, const int, const int, const LFSPARMS *);
extern int pixelize_map(int **, const int, const int,
                     int *, const int, const int, const int);
extern void smooth_direction_map(int *, int *, const int, const int,
                     const DIR2RAD *, const LFSPARMS *);
extern int gen_high_curve_map(int **, int *, const int, const int,
                     const LFSPARMS *);
extern int gen_initial_imap(int **, int *, const int, const int,
                     unsigned char *, const int, const int,
                     const DFTWAVES *, const ROTGRIDS *, const LFSPARMS *);
extern int primary_dir_test(double **, const int *, const double *,
                     const int *, const double *, const int,
                     const LFSPARMS *);
extern int secondary_fork_test(double **, const int *, const double *,
                     const int *, const double *, const int,
                     const LFSPARMS *);
extern void remove_incon_dirs(int *, const int, const int,
                     const DIR2RAD *, const LFSPARMS *);
extern int test_top_edge(const int, const int, const int, const int,
                     int *, const int, const int, const DIR2RAD *,
                     const LFSPARMS *);
extern int test_right_edge(const int, const int, const int, const int,
                     int *, const int, const int, const DIR2RAD *,
                     const LFSPARMS *);
extern int test_bottom_edge(const int, const int, const int, const int,
                     int *, const int, const int, const DIR2RAD *,
                     const LFSPARMS *);
extern int test_left_edge(const int, const int, const int, const int,
                     int *, const int, const int, const DIR2RAD *,
                     const LFSPARMS *);
extern int remove_dir(int *, const int, const int, const int, const int,
                     const DIR2RAD *, const LFSPARMS *);
extern void average_8nbr_dir(int *, double *, int *, int *, const int,
                     const int, const int, const int, const DIR2RAD *);
extern int num_valid_8nbrs(int *, const int, const int, const int, const int);
extern void smooth_imap(int *, const int, const int, const DIR2RAD *,
                     const LFSPARMS *);
extern int vorticity(int *, const int, const int, const int, const int,
                     const int);
extern void accum_nbr_vorticity(int *, const int, const int, const int);
extern int curvature(int *, const int, const int, const int, const int,
                     const int);

/* matchpat.c */
extern int match_1st_pair(unsigned char, unsigned char, int *, int *);
extern int match_2nd_pair(unsigned char, unsigned char, int *, int *);
extern int match_3rd_pair(unsigned char, unsigned char, int *, int *);
extern void skip_repeated_horizontal_pair(int *, const int,
                     unsigned char **, unsigned char **, const int, const int);
extern void skip_repeated_vertical_pair(int *, const int,
                     unsigned char **, unsigned char **, const int, const int);

/* minutia.c */
extern int alloc_minutiae(MINUTIAE **, const int);
extern int realloc_minutiae(MINUTIAE *, const int);
extern int detect_minutiae_V2(MINUTIAE *,
                     unsigned char *, const int, const int,
                     int *, int *, int *, const int, const int,
                     const LFSPARMS *);
extern int update_minutiae(MINUTIAE *, MINUTIA *, unsigned char *,
                     const int, const int, const LFSPARMS *);
extern int update_minutiae_V2(MINUTIAE *, MINUTIA *, const int, const int,
                     unsigned char *, const int, const int,
                     const LFSPARMS *);
extern int sort_minutiae(MINUTIAE *, const int, const int);
extern int sort_minutiae_y_x(MINUTIAE *, const int, const int);
extern int sort_minutiae_x_y(MINUTIAE *, const int, const int);
extern int rm_dup_minutiae(MINUTIAE *);
extern void dump_minutiae(FILE *, const MINUTIAE *);
extern void dump_minutiae_pts(FILE *, const MINUTIAE *);
extern void dump_reliable_minutiae_pts(FILE *, const MINUTIAE *, const double);
extern int create_minutia(MINUTIA **, const int, const int,
                     const int, const int, const int, const double,
                     const int, const int, const int);
extern void free_minutiae(MINUTIAE *);
extern void free_minutia(MINUTIA *);
extern int remove_minutia(const int, MINUTIAE *);
extern int join_minutia(const MINUTIA *, const MINUTIA *, unsigned char *,
                     const int, const int, const int, const int);
extern int minutia_type(const int);
extern int is_minutia_appearing(const int, const int, const int, const int);
extern int choose_scan_direction(const int, const int);
int scan4minutiae(MINUTIAE *, unsigned char *, const int, const int,
                     const int *, const int *, const int, const int,
                     const int, const int, const int, const int,
                     const int, const int, const int, const LFSPARMS *);
extern int scan4minutiae_horizontally(MINUTIAE *, unsigned char *,
                     const int, const int, const int, const int,
                     const int, const int, const int, const int,
                     const LFSPARMS *);
extern int scan4minutiae_horizontally_V2(MINUTIAE *,
                     unsigned char *, const int, const int,
                     int *, int *, int *,
                     const LFSPARMS *);
extern int scan4minutiae_vertically(MINUTIAE *, unsigned char *,
                     const int, const int, const int, const int,
                     const int, const int, const int, const int,
                     const LFSPARMS *);
extern int rescan4minutiae_horizontally(MINUTIAE *, unsigned char *bdata,
                     const int, const int, const int *, const int *,
                     const int, const int, const int, const int,
                     const int, const int, const int, const int,
                     const LFSPARMS *);
extern int scan4minutiae_vertically_V2(MINUTIAE *,
                     unsigned char *, const int, const int,
                     int *, int *, int *, const LFSPARMS *);
extern int rescan4minutiae_vertically(MINUTIAE *, unsigned char *,
                     const int, const int, const int *, const int *,
                     const int, const int, const int, const int,
                     const int, const int, const int, const int,
                     const LFSPARMS *);
extern int rescan_partial_horizontally(const int, MINUTIAE *,
                     unsigned char *, const int, const int,
                     const int *, const int *,
                     const int, const int, const int, const int,
                     const int, const int, const int, const int,
                     const LFSPARMS *);
extern int rescan_partial_vertically(const int, MINUTIAE *,
                     unsigned char *, const int, const int,
                     const int *, const int *,
                     const int, const int, const int, const int,
                     const int, const int, const int, const int,
                     const LFSPARMS *);
extern int get_nbr_block_index(int *, const int, const int, const int,
                     const int, const int);
extern int adjust_horizontal_rescan(const int, int *, int *, int *, int *,
                     const int, const int, const int, const int, const int);
extern int adjust_vertical_rescan(const int, int *, int *, int *, int *,
                     const int, const int, const int, const int, const int);
extern int process_horizontal_scan_minutia(MINUTIAE *, const int, const int,
                     const int, const int,
                     unsigned char *, const int, const int,
                     const int, const int, const LFSPARMS *);
extern int process_horizontal_scan_minutia_V2(MINUTIAE *,
                     const int, const int, const int, const int,
                     unsigned char *, const int, const int,
                     int *, int *, int *, const LFSPARMS *);
extern int process_vertical_scan_minutia(MINUTIAE *, const int, const int,
                     const int, const int,
                     unsigned char *, const int, const int,
                     const int, const int, const LFSPARMS *);
extern int process_vertical_scan_minutia_V2(MINUTIAE *, const int, const int,
                     const int, const int,
                     unsigned char *, const int, const int,
                     int *, int *, int *, const LFSPARMS *);
extern int update_minutiae_V2(MINUTIAE *, MINUTIA *, const int, const int,
                     unsigned char *, const int, const int,
                     const LFSPARMS *);
extern int adjust_high_curvature_minutia(int *, int *, int *, int *, int *,
                     const int, const int, const int, const int,
                     unsigned char *, const int, const int,
                     MINUTIAE *, const LFSPARMS *);
extern int adjust_high_curvature_minutia_V2(int *, int *, int *,
                     int *, int *, const int, const int,
                     const int, const int,
                     unsigned char *, const int, const int,
                     int *, MINUTIAE *, const LFSPARMS *);
extern int get_low_curvature_direction(const int, const int, const int,
                     const int);
void lfs2nist_minutia_XYT(int *ox, int *oy, int *ot,
                          const MINUTIA *minutia, const int iw, const int ih);

/* quality.c */
extern int gen_quality_map(int **, int *, int *, int *, int *,
                     const int, const int);
extern int combined_minutia_quality(MINUTIAE *, int *, const int, const int,
                     const int, unsigned char *, const int, const int,
                     const int, const double);

/* remove.c */
extern int remove_false_minutia(MINUTIAE *,
                  unsigned char *, const int, const int,
                  int *, const int, const int, const LFSPARMS *);
extern int remove_false_minutia_V2(MINUTIAE *,
                  unsigned char *, const int, const int,
                  int *, int *, int *, const int, const int,
                  const LFSPARMS *);

/* ridges.c */
extern int count_minutiae_ridges(MINUTIAE *,
                  unsigned char *, const int, const int,
                  const LFSPARMS *);

/* shape.c */
extern void free_shape(SHAPE *);
extern int shape_from_contour(SHAPE **, const int *, const int *, const int);

/* sort.c */
extern int sort_indices_int_inc(int **, int *, const int);
extern int sort_indices_double_inc(int **, double *, const int);
extern void bubble_sort_int_inc_2(int *, int *, const int);
extern void bubble_sort_double_inc_2(double *, int *, const int);
extern void bubble_sort_double_dec_2(double *, int *,  const int);
extern void bubble_sort_int_inc(int *, const int);

/* util.c */
extern int maxv(const int *, const int);
extern int minv(const int *, const int);
extern int minmaxs(int **, int **, int **, int *, int *,
                  const int *, const int);
extern double distance(const int, const int, const int, const int);
extern double squared_distance(const int, const int, const int, const int);
extern int in_int_list(const int, const int *, const int);
extern int remove_from_int_list(const int, int *, const int);
extern int find_incr_position_dbl(const double, double *, const int);
extern double angle2line(const int, const int, const int, const int);
extern int line2direction(const int, const int, const int, const int,
                     const int);
extern int closest_dir_dist(const int, const int, const int);

/*************************************************************************/
/*        EXTERNAL GLOBAL VARIABLE DEFINITIONS                           */
/*************************************************************************/
extern double g_dft_coefs[];
extern LFSPARMS g_lfsparms;
extern LFSPARMS g_lfsparms_V2;
extern int g_nbr8_dx[];
extern int g_nbr8_dy[];
extern int g_chaincodes_nbr8[];
extern FEATURE_PATTERN g_feature_patterns[];

#endif

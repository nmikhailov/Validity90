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

#ifndef _BOZORTH_H
#define _BOZORTH_H

/* The max number of points in any Probe or Gallery XYT is set to 200; */
/* a pointwise comparison table therefore has a maximum number of:     */
/*		(200^2)/2 = 20000 comparisons. */


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h> /* Needed for type pid_t */
#include <errno.h>

/* If not defined in sys/param.h */
#ifndef MAX
#define	MAX(a,b) (((a)>(b))?(a):(b))
#endif

/**************************************************************************/
/* Math-Related Macros, Definitions & Prototypes */
/**************************************************************************/
#include <math.h>
				/* This macro adjusts angles to the range (-180,180] */
#define IANGLE180(deg)		( ( (deg) > 180 ) ? ( (deg) - 360 ) : ( (deg) <= -180 ? ( (deg) + 360 ) : (deg) ) )

#define SENSE(a,b)		( (a) < (b) ? (-1) : ( ( (a) == (b) ) ? 0 : 1 ) )
#define SENSE_NEG_POS(a,b)	( (a) < (b) ? (-1) : 1 )

#define SQUARED(n)		( (n) * (n) )

#ifdef ROUND_USING_LIBRARY
/* These functions should be declared in math.h:
	extern float  roundf( float  );
	extern double round(  double );
*/
#define ROUND(f) (roundf(f))
#else
#define ROUND(f) ( ( (f) < 0.0F ) ? ( (int) ( (f) - 0.5F ) ) : ( (int) ( (f) + 0.5F ) ) )
#endif

/* PI is used in: bozorth3.c, comp.c */
#ifdef M_PI
#define PI		M_PI
#define PI_SINGLE	( (float) PI )
#else
#define PI		3.14159
#define PI_SINGLE	3.14159F
#endif

/* Provide prototype for atanf() */
extern float atanf( float );

/**************************************************************************/
/* Array Length Definitions */
/**************************************************************************/
#include <bz_array.h>


/**************************************************************************/
/**************************************************************************/
                        /* GENERAL DEFINITIONS */
/**************************************************************************/

#define FPNULL ((FILE *) NULL)
#define CNULL  ((char *) NULL)

#define PROGRAM				"bozorth3"

#define MAX_LINE_LENGTH 1024

#define SCOREFILE_EXTENSION		".scr"

#define MAX_FILELIST_LENGTH		10000

#define DEFAULT_BOZORTH_MINUTIAE	150
#define MAX_BOZORTH_MINUTIAE		200
#define MIN_BOZORTH_MINUTIAE		0
#define MIN_COMPUTABLE_BOZORTH_MINUTIAE	10

#define DEFAULT_MAX_MATCH_SCORE		400
#define ZERO_MATCH_SCORE		0

#define DEFAULT_SCORE_LINE_FORMAT	"s"

#define DM	125
#define FD	5625
#define FDD	500
#define TK	0.05F
#define TXS	121
#define CTXS	121801
#define MSTR	3
#define MMSTR	8
#define WWIM	10

#define QQ_SIZE 4000

#define QQ_OVERFLOW_SCORE QQ_SIZE

/**************************************************************************/
/**************************************************************************/
                          /* MACROS DEFINITIONS */
/**************************************************************************/
#define INT_SET(dst,count,value) { \
		int * int_set_dst   = (dst); \
		int   int_set_count = (count); \
		int   int_set_value = (value); \
		while ( int_set_count-- > 0 ) \
			*int_set_dst++ = int_set_value; \
		}

/* The code that calls it assumed dst gets bumped, so don't assign to a local variable */
#define INT_COPY(dst,src,count) { \
		int * int_copy_src = (src); \
		int int_copy_count = (count); \
		while ( int_copy_count-- > 0 ) \
			*dst++ = *int_copy_src++; \
		}


/**************************************************************************/
/**************************************************************************/
                         /* STRUCTURES & TYPEDEFS */
/**************************************************************************/

/**************************************************************************/
/* In BZ_SORT.C - supports stdlib qsort() and customized quicksort */
/**************************************************************************/

/* Used by call to stdlib qsort() */
struct minutiae_struct {
	int col[4];
};

/* Used by custom quicksort */
#define BZ_STACKSIZE    1000
struct cell {
	int		index;	/* pointer to an array of pointers to index arrays */
	int		item;	/* pointer to an item array */
};

/**************************************************************************/
/* In BZ_IO : Supports the loading and manipulation of XYT data */
/**************************************************************************/
#define MAX_FILE_MINUTIAE       1000 /* bz_load() */

struct xyt_struct {
	int nrows;
	int xcol[     MAX_BOZORTH_MINUTIAE ];
	int ycol[     MAX_BOZORTH_MINUTIAE ];
	int thetacol[ MAX_BOZORTH_MINUTIAE ];
};

#define XYT_NULL ( (struct xyt_struct *) NULL ) /* bz_load() */


/**************************************************************************/
/**************************************************************************/
                           /* GLOBAL VARIABLES */
/**************************************************************************/

/**************************************************************************/
/* In: SRC/BIN/BOZORTH3/BOZORTH3.C */
/**************************************************************************/
/* Globals supporting command line options */
extern int verbose_threshold;

/**************************************************************************/
/* In: BZ_GBLS.C */
/**************************************************************************/
/* Global arrays supporting "core" bozorth algorithm */
extern int colp[ COLP_SIZE_1 ][ COLP_SIZE_2 ];
extern int scols[ SCOLS_SIZE_1 ][ COLS_SIZE_2 ];
extern int fcols[ FCOLS_SIZE_1 ][ COLS_SIZE_2 ];
extern int * scolpt[ SCOLPT_SIZE ];
extern int * fcolpt[ FCOLPT_SIZE ];
extern int sc[ SC_SIZE ];
extern int yl[ YL_SIZE_1 ][ YL_SIZE_2 ];
/* Global arrays supporting "core" bozorth algorithm continued: */
/*    Globals used significantly by sift() */
extern int rq[ RQ_SIZE ];
extern int tq[ TQ_SIZE ];
extern int zz[ ZZ_SIZE ];
extern int rx[ RX_SIZE ];
extern int mm[ MM_SIZE ];
extern int nn[ NN_SIZE ];
extern int qq[ QQ_SIZE ];
extern int rk[ RK_SIZE ];
extern int cp[ CP_SIZE ];
extern int rp[ RP_SIZE ];
extern int rf[RF_SIZE_1][RF_SIZE_2];
extern int cf[CF_SIZE_1][CF_SIZE_2];
extern int y[20000];

/**************************************************************************/
/**************************************************************************/
/* ROUTINE PROTOTYPES */
/**************************************************************************/
/* In: BZ_DRVRS.C */
extern int bozorth_probe_init( struct xyt_struct *);
extern int bozorth_gallery_init( struct xyt_struct *);
extern int bozorth_to_gallery(int, struct xyt_struct *, struct xyt_struct *);
extern int bozorth_main(struct xyt_struct *, struct xyt_struct *);
/* In: BOZORTH3.C */
extern void bz_comp(int, int [], int [], int [], int *, int [][COLS_SIZE_2],
                    int *[]);
extern void bz_find(int *, int *[]);
extern int bz_match(int, int);
extern int bz_match_score(int, struct xyt_struct *, struct xyt_struct *);
extern void bz_sift(int *, int, int *, int, int, int, int *, int *);
/* In: BZ_ALLOC.C */
extern char *malloc_or_exit(int, const char *);
extern char *malloc_or_return_error(int, const char *);
/* In: BZ_IO.C */
extern int parse_line_range(const char *, int *, int *);
extern void set_progname(int, char *, pid_t);
extern void set_probe_filename(char *);
extern void set_gallery_filename(char *);
extern char *get_progname(void);
extern char *get_probe_filename(void);
extern char *get_gallery_filename(void);
extern char *get_next_file(char *, FILE *, FILE *, int *, int *, char *,
			int, char **, int *, int *, int, int);
extern char *get_score_filename(const char *, const char *);
extern char *get_score_line(const char *, const char *, int, int, const char *);
extern struct xyt_struct *bz_load(const char *);
extern int fd_readable(int);
/* In: BZ_SORT.C */
extern int sort_quality_decreasing(const void *, const void *);
extern int sort_x_y(const void *, const void *);
extern int sort_order_decreasing(int [], int, int []);

#endif /* !_BOZORTH_H */

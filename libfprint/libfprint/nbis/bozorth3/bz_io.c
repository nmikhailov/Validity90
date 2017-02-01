/****************************************************************************** 

This file is part of the Export Control subset of the United States NIST
Biometric Image Software (NBIS) distribution:
    http://fingerprint.nist.gov/NBIS/index.html

It is our understanding that this falls within ECCN 3D980, which covers
software associated with the development, production or use of certain
equipment controlled in accordance with U.S. concerns about crime control
practices in specific countries.

Therefore, this file should not be exported, or made available on fileservers,
except as allowed by U.S. export control laws.

Do not remove this notice.

******************************************************************************/

/* NOTE: Despite the above notice (which I have not removed), this file is
 * being legally distributed within libfprint; the U.S. Export Administration
 * Regulations do not place export restrictions upon distribution of
 * "publicly available technology and software", as stated in EAR section
 * 734.3(b)(3)(i). libfprint qualifies as publicly available technology as per
 * the definition in section 734.7(a)(1).
 *
 * For further information, see http://reactivated.net/fprint/US_export_control
 */

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
      LIBRARY: FING - NIST Fingerprint Systems Utilities

      FILE:           BZ_IO.C
      ALGORITHM:      Allan S. Bozorth (FBI)
      MODIFICATIONS:  Michael D. Garris (NIST)
                      Stan Janet (NIST)
      DATE:           09/21/2004

      Contains routines responsible for supporting command line
      processing, file and data input to, and output from the
      Bozorth3 fingerprint matching algorithm.

***********************************************************************

      ROUTINES:
#cat: parse_line_range - parses strings of the form #-# into the upper
#cat:            and lower bounds of a range corresponding to lines in
#cat:            an input file list
#cat: set_progname - stores the program name for the current invocation
#cat: set_probe_filename - stores the name of the current probe file
#cat:            being processed
#cat: set_gallery_filename - stores the name of the current gallery file
#cat:            being processed
#cat: get_progname - retrieves the program name for the current invocation
#cat: get_probe_filename - retrieves the name of the current probe file
#cat:            being processed
#cat: get_gallery_filename - retrieves the name of the current gallery
#cat:            file being processed
#cat: get_next_file - gets the next probe (or gallery) filename to be
#cat:            processed, either from the command line or from a
#cat:            file list
#cat: get_score_filename - returns the filename to which the output line
#cat:            should be written
#cat: get_score_line - formats output lines based on command line options
#cat:            specified
#cat: bz_load -  loads the contents of the specified XYT file into
#cat:            structured memory
#cat: fd_readable - when multiple bozorth processes are being run
#cat:            concurrently and one of the processes determines a
#cat:            has been found, the other processes poll a file
#cat:            descriptor using this function to see if they
#cat:            should exit as well

***********************************************************************/

#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#include <bozorth.h>

static const int verbose_load = 0;
static const int verbose_main = 0;

/***********************************************************************/
int parse_line_range( const char * sb, int * begin, int * end )
{
int ib, ie;
char * se;


if ( ! isdigit(*sb) )
	return -1;
ib = atoi( sb );

se = strchr( sb, '-' );
if ( se != (char *) NULL ) {
	se++;
	if ( ! isdigit(*se) )
		return -2;
	ie = atoi( se );
} else {
	ie = ib;
}

if ( ib <= 0 ) {
	if ( ie <= 0 ) {
		return -3;
	} else {
		return -4;
	}
}

if ( ie <= 0 ) {
	return -5;
}

if ( ib > ie )
	return -6;

*begin = ib;
*end   = ie;

return 0;
}

/***********************************************************************/

/* Used by the following set* and get* routines */
static char program_buffer[ 1024 ];
static char * pfile;
static char * gfile;

/***********************************************************************/
void set_progname( int use_pid, char * basename, pid_t pid )
{
if ( use_pid )
	sprintf( program_buffer, "%s pid %ld", basename, (long) pid );
else
	sprintf( program_buffer, "%s", basename );
}

/***********************************************************************/
void set_probe_filename( char * filename )
{
pfile = filename;
}

/***********************************************************************/
void set_gallery_filename( char * filename )
{
gfile = filename;
}

/***********************************************************************/
char * get_progname( void )
{
return program_buffer;
}

/***********************************************************************/
char * get_probe_filename( void )
{
return pfile;
}

/***********************************************************************/
char * get_gallery_filename( void )
{
return gfile;
}

/***********************************************************************/
char * get_next_file(
		char * fixed_file,
		FILE * list_fp,
		FILE * mates_fp,
		int * done_now,
		int * done_afterwards,
		char * line,
		int argc,
		char ** argv,
		int * optind,

		int * lineno,
		int begin,
		int end
		)
{
char * p;
FILE * fp;



if ( fixed_file != (char *) NULL ) {
	if ( verbose_main )
		fprintf( stderr, "returning fixed filename: %s\n", fixed_file );
	return fixed_file;
}


fp = list_fp;
if ( fp == (FILE *) NULL )
	fp = mates_fp;
if ( fp != (FILE *) NULL ) {
	while (1) {
		if ( fgets( line, MAX_LINE_LENGTH, fp ) == (char *) NULL ) {
			*done_now = 1;
			if ( verbose_main )
				fprintf( stderr, "returning NULL -- reached EOF\n" );
			return (char *) NULL;
		}
		++*lineno;

		if ( begin <= 0 )			/* no line number range was specified */
			break;
		if ( *lineno > end ) {
			*done_now = 1;
			if ( verbose_main )
				fprintf( stderr, "returning NULL -- current line (%d) > end line (%d)\n",
										*lineno, end );
			return (char *) NULL;
		}
		if ( *lineno >= begin ) {
			break;
		}
		/* Otherwise ( *lineno < begin ) so read another line */
	}

	p = strchr( line, '\n' );
	if ( p == (char *) NULL ) {
		*done_now = 1;
		if ( verbose_main )
			fprintf( stderr, "returning NULL -- missing newline character\n" );
		return (char *) NULL;
	}
	*p = '\0';

	p = line;
	if ( verbose_main )
		fprintf( stderr, "returning filename from next line: %s\n", p );
	return p;
}


p = argv[*optind];
++*optind;
if ( *optind >= argc )
	*done_afterwards = 1;
if ( verbose_main )
	fprintf( stderr, "returning next argv: %s [done_afterwards=%d]\n", p, *done_afterwards );
return p;
}

/***********************************************************************/
/* returns CNULL on error */
char * get_score_filename( const char * outdir, const char * listfile )
{
const char * basename;
int baselen;
int dirlen;
int extlen;
char * outfile;

/* These are now exteranlly defined in bozorth.h */
/* extern FILE * stderr; */
/* extern char * get_progname( void ); */



basename = strrchr( listfile, '/' );
if ( basename == CNULL ) {
	basename = listfile;
} else {
	++basename;
}
baselen = strlen( basename );
if ( baselen == 0 ) {
	fprintf( stderr, "%s: ERROR: couldn't find basename of %s\n", get_progname(), listfile );
	return(CNULL);
}
dirlen = strlen( outdir );
if ( dirlen == 0 ) {
	fprintf( stderr, "%s: ERROR: illegal output directory %s\n", get_progname(), outdir );
	return(CNULL);
}

extlen = strlen( SCOREFILE_EXTENSION );
outfile = malloc_or_return_error( dirlen + baselen + extlen + 2, "output filename" );
if ( outfile == CNULL)
	return(CNULL);

sprintf( outfile, "%s/%s%s", outdir, basename, SCOREFILE_EXTENSION );

return outfile;
}

/***********************************************************************/
char * get_score_line(
		const char * probe_file,
		const char * gallery_file,
		int n,
		int static_flag,
		const char * fmt
		)
{
int nchars;
char * bufptr;
static char linebuf[1024];

nchars = 0;
bufptr = &linebuf[0];
while ( *fmt ) {
	if ( nchars++ > 0 )
		*bufptr++ = ' ';
	switch ( *fmt++ ) {
		case 's':
			sprintf( bufptr, "%d", n );
			break;
		case 'p':
			sprintf( bufptr, "%s", probe_file );
			break;
		case 'g':
			sprintf( bufptr, "%s", gallery_file );
			break;
		default:
			return (char *) NULL;
	}
	bufptr = strchr( bufptr, '\0' );
}
*bufptr++ = '\n';
*bufptr   = '\0';

return static_flag ? &linebuf[0] : strdup( linebuf );
}

/************************************************************************
Load a 3-4 column (X,Y,T[,Q]) set of minutiae from the specified file.
Row 3's value is an angle which is normalized to the interval (-180,180].
A maximum of MAX_BOZORTH_MINUTIAE minutiae can be returned -- fewer if
"DEFAULT_BOZORTH_MINUTIAE" is smaller.  If the file contains more minutiae than are
to be returned, the highest-quality minutiae are returned.
*************************************************************************/

/***********************************************************************/
struct xyt_struct * bz_load( const char * xyt_file )
{
int nminutiae;
int j;
int m;
int nargs_expected;
FILE * fp;
struct xyt_struct * s;
int * xptr;
int * yptr;
int * tptr;
int * qptr;
struct minutiae_struct c[MAX_FILE_MINUTIAE];
int	xvals_lng[MAX_FILE_MINUTIAE],	/* Temporary lists to store all the minutaie from a file */
	yvals_lng[MAX_FILE_MINUTIAE],
	tvals_lng[MAX_FILE_MINUTIAE],
	qvals_lng[MAX_FILE_MINUTIAE];
int order[MAX_FILE_MINUTIAE];		/* The ranked order, after sort, for each index */
int	xvals[MAX_BOZORTH_MINUTIAE],	/* Temporary lists to hold input coordinates */
	yvals[MAX_BOZORTH_MINUTIAE],
	tvals[MAX_BOZORTH_MINUTIAE],
	qvals[MAX_BOZORTH_MINUTIAE];
char xyt_line[ MAX_LINE_LENGTH ];

/* This is now externally defined in bozorth.h */
/* extern FILE * stderr; */



#define C1 0
#define C2 1



fp = fopen( xyt_file, "r" );
if ( fp == (FILE *) NULL ) {
	fprintf( stderr, "%s: ERROR: fopen() of minutiae file \"%s\" failed: %s\n",
							get_progname(), xyt_file, strerror(errno) );
	return XYT_NULL;
}

nminutiae = 0;
nargs_expected = 0;
while ( fgets( xyt_line, sizeof xyt_line, fp ) != CNULL ) {

	m = sscanf( xyt_line, "%d %d %d %d",
				&xvals_lng[nminutiae],
				&yvals_lng[nminutiae],
				&tvals_lng[nminutiae],
				&qvals_lng[nminutiae] );
	if ( nminutiae == 0 ) {
		if ( m != 3 && m != 4 ) {
			fprintf( stderr, "%s: ERROR: sscanf() failed on line %u in minutiae file \"%s\"\n",
							get_progname(), nminutiae+1, xyt_file );
			return XYT_NULL;
		}
		nargs_expected = m;
	} else {
		if ( m != nargs_expected ) {
			fprintf( stderr, "%s: ERROR: inconsistent argument count on line %u of minutiae file \"%s\"\n",
							get_progname(), nminutiae+1, xyt_file );
			return XYT_NULL;
		}
	}
	if ( m == 3 )
		qvals_lng[nminutiae] = 1;



	if ( tvals_lng[nminutiae] > 180 )
		tvals_lng[nminutiae] -= 360;

	/*
	if ( C1 ) {
		c[nminutiae].col[0] = xvals_lng[nminutiae];
		c[nminutiae].col[1] = yvals_lng[nminutiae];
		c[nminutiae].col[2] = tvals_lng[nminutiae];
		c[nminutiae].col[3] = qvals_lng[nminutiae];
	}
	*/

	++nminutiae;
	if ( nminutiae == MAX_FILE_MINUTIAE )
		break;
}

if ( fclose(fp) != 0 ) {
	fprintf( stderr, "%s: ERROR: fclose() of minutiae file \"%s\" failed: %s\n",
						get_progname(), xyt_file, strerror(errno) );
	return XYT_NULL;
}




if ( nminutiae > DEFAULT_BOZORTH_MINUTIAE ) {
	if ( verbose_load )
		fprintf( stderr, "%s: WARNING: bz_load(): trimming minutiae to the %d of highest quality\n",
						get_progname(), DEFAULT_BOZORTH_MINUTIAE );

	if ( verbose_load )
		fprintf( stderr, "Before quality sort:\n" );
	if ( sort_order_decreasing( qvals_lng, nminutiae, order )) {
		fprintf( stderr, "%s: ERROR: sort failed and returned on error\n", get_progname());
		return XYT_NULL;
	}

	for ( j = 0; j < nminutiae; j++ ) {

		if ( verbose_load )
			fprintf( stderr, "   %3d: %3d %3d %3d ---> order = %3d\n",
						j, xvals_lng[j], yvals_lng[j], qvals_lng[j], order[j] );

		if ( j == 0 )
			continue;
		if ( qvals_lng[order[j]] > qvals_lng[order[j-1]] ) {
			fprintf( stderr, "%s: ERROR: sort failed: j=%d; qvals_lng[%d] > qvals_lng[%d]\n",
						get_progname(), j, order[j], order[j-1] );
			return XYT_NULL;
		}
	}


	if ( verbose_load )
		fprintf( stderr, "\nAfter quality sort:\n" );
	for ( j = 0; j < DEFAULT_BOZORTH_MINUTIAE; j++ ) {
		xvals[j] = xvals_lng[order[j]];
		yvals[j] = yvals_lng[order[j]];
		tvals[j] = tvals_lng[order[j]];
		qvals[j] = qvals_lng[order[j]];
		if ( verbose_load )
			fprintf( stderr, "   %3d: %3d %3d %3d\n", j, xvals[j], yvals[j], qvals[j] );
	}


	if ( C1 ) {
	if ( verbose_load )
		fprintf( stderr, "\nAfter qsort():\n" );
	qsort( (void *) &c, (size_t) nminutiae, sizeof(struct minutiae_struct), sort_quality_decreasing );
	for ( j = 0; j < nminutiae; j++ ) {

		if ( verbose_load )
			fprintf( stderr, "Q  %3d: %3d %3d %3d\n",
						j, c[j].col[0], c[j].col[1], c[j].col[3] );

		if ( j > 0 && c[j].col[3] > c[j-1].col[3] ) {
			fprintf( stderr, "%s: ERROR: sort failed: c[%d].col[3] > c[%d].col[3]\n",
						get_progname(), j, j-1 );
			return XYT_NULL;
		}
	}
	}

	if ( verbose_load )
		fprintf( stderr, "\n" );

	xptr = xvals;
	yptr = yvals;
	tptr = tvals;
	qptr = qvals;

	nminutiae = DEFAULT_BOZORTH_MINUTIAE;
} else{
	xptr = xvals_lng;
	yptr = yvals_lng;
	tptr = tvals_lng;
	qptr = qvals_lng;
}



for ( j=0; j < nminutiae; j++ ) {
	c[j].col[0] = xptr[j];
	c[j].col[1] = yptr[j];
	c[j].col[2] = tptr[j];
	c[j].col[3] = qptr[j];
}
qsort( (void *) &c, (size_t) nminutiae, sizeof(struct minutiae_struct), sort_x_y );




if ( verbose_load ) {
	fprintf( stderr, "\nSorted on increasing x, then increasing y\n" );
	for ( j = 0; j < nminutiae; j++ ) {
		fprintf( stderr, "%d : %3d, %3d, %3d, %3d\n", j, c[j].col[0], c[j].col[1], c[j].col[2], c[j].col[3] );
		if ( j > 0 ) {
			if ( c[j].col[0] < c[j-1].col[0] ) {
				fprintf( stderr, "%s: ERROR: sort failed: c[%d].col[0]=%d > c[%d].col[0]=%d\n",
							get_progname(),
							j, c[j].col[0], j-1, c[j-1].col[0]
							);
				return XYT_NULL;
			}
			if ( c[j].col[0] == c[j-1].col[0] && c[j].col[1] < c[j-1].col[1] ) {
				fprintf( stderr, "%s: ERROR: sort failed: c[%d].col[0]=%d == c[%d].col[0]=%d; c[%d].col[0]=%d == c[%d].col[0]=%d\n",
							get_progname(),
							j, c[j].col[0], j-1, c[j-1].col[0],
							j, c[j].col[1], j-1, c[j-1].col[1]
							);
				return XYT_NULL;
			}
		}
	}
}



s = (struct xyt_struct *) malloc( sizeof( struct xyt_struct ) );
if ( s == XYT_NULL ) {
	fprintf( stderr, "%s: ERROR: malloc() failure while loading minutiae file \"%s\" failed: %s\n",
							get_progname(),
							xyt_file,
							strerror(errno)
							);
	return XYT_NULL;
}



for ( j = 0; j < nminutiae; j++ ) {
	s->xcol[j]     = c[j].col[0];
	s->ycol[j]     = c[j].col[1];
	s->thetacol[j] = c[j].col[2];
}
s->nrows = nminutiae;




if ( verbose_load )
	fprintf( stderr, "Loaded %s\n", xyt_file );

return s;
}

/***********************************************************************/
#ifdef PARALLEL_SEARCH
int fd_readable( int fd )
{
int retval;
fd_set rfds;
struct timeval tv;


FD_ZERO( &rfds );
FD_SET( fd, &rfds );
tv.tv_sec = 0;
tv.tv_usec = 0;

retval = select( fd+1, &rfds, NULL, NULL, &tv );

if ( retval < 0 ) {
	perror( "select() failed" );
	return 0;
}

if ( FD_ISSET( fd, &rfds ) ) {
	/*fprintf( stderr, "data is available now.\n" );*/
	return 1;
}

/* fprintf( stderr, "no data is available\n" ); */
return 0;
}
#endif

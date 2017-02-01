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

      FILE:           BZ_SORT.C
      ALGORITHM:      Allan S. Bozorth (FBI)
      MODIFICATIONS:  Michael D. Garris (NIST)
                      Stan Janet (NIST)
      DATE:           09/21/2004

      Contains sorting routines responsible for supporting the
      Bozorth3 fingerprint matching algorithm.

***********************************************************************

      ROUTINES:
#cat: sort_quality_decreasing - comparison function passed to stdlib
#cat:            qsort() used to sort minutia qualities
#cat: sort_x_y - comparison function passed to stdlib qsort() used
#cat:            to sort minutia coordinates increasing first on x
#cat:            then on y
#cat: sort_order_decreasing - calls a custom quicksort that sorts
#cat:            a list of integers in decreasing order

***********************************************************************/

#include <stdio.h>
#include <bozorth.h>

/* These are now externally defined in bozorth.h */
/* extern char * get_progname( void ); */

/***********************************************************************/
int sort_quality_decreasing( const void * a, const void * b )
{
struct minutiae_struct * af;
struct minutiae_struct * bf;

af = (struct minutiae_struct *) a;
bf = (struct minutiae_struct *) b;

if ( af->col[3] > bf->col[3] )
	return -1;
if ( af->col[3] < bf->col[3] )
	return 1;
return 0;
}

/***********************************************************************/
int sort_x_y( const void * a, const void * b )
{
struct minutiae_struct * af;
struct minutiae_struct * bf;

af = (struct minutiae_struct *) a;
bf = (struct minutiae_struct *) b;

if ( af->col[0] < bf->col[0] )
	return -1;
if ( af->col[0] > bf->col[0] )
	return 1;

if ( af->col[1] < bf->col[1] )
	return -1;
if ( af->col[1] > bf->col[1] )
	return 1;

return 0;
}

/********************************************************
qsort_decreasing() - quicksort an array of integers in decreasing
                     order [based on multisort.c, by Michael Garris
                     and Ted Zwiesler, 1986]
********************************************************/
/* Used by custom quicksort code below */
static int   stack[BZ_STACKSIZE];
static int * stack_pointer = stack;

/***********************************************************************/
/* return values: 0 == successful, 1 == error */
static int popstack( int *popval )
{
if ( --stack_pointer < stack ) {
	fprintf( stderr, "%s: ERROR: popstack(): stack underflow\n", get_progname() );
	return 1;
}

*popval = *stack_pointer;
return 0;
}

/***********************************************************************/
/* return values: 0 == successful, 1 == error */
static int pushstack( int position )
{
*stack_pointer++ = position;
if ( stack_pointer > ( stack + BZ_STACKSIZE ) ) {
	fprintf( stderr, "%s: ERROR: pushstack(): stack overflow\n", get_progname() );
	return 1;
}
return 0;
}

/***********************************************************************/
/*******************************************************************
select_pivot()
selects a pivot from a list being sorted using the Singleton Method.
*******************************************************************/
static int select_pivot( struct cell v[], int left, int right )
{
int midpoint;


midpoint = ( left + right ) / 2;
if ( v[left].index <= v[midpoint].index ) {
	if ( v[midpoint].index <= v[right].index ) {
		return midpoint;
	} else {
		if ( v[right].index > v[left].index ) {
			return right;
		} else {
			return left;
		}
	}
} else {
	if ( v[left].index < v[right].index ) {
		return left;
	} else {
		if ( v[right].index < v[midpoint].index ) {
			return midpoint;
		} else {
			return right;
		}
	}
}
}

/***********************************************************************/
/********************************************************
partition_dec()
Inputs a pivot element making comparisons and swaps with other elements in a list,
until pivot resides at its correct position in the list.
********************************************************/
static void partition_dec( struct cell v[], int *llen, int *rlen, int *ll, int *lr, int *rl, int *rr, int p, int l, int r )
{
#define iswap(a,b) { int itmp = (a); a = (b); b = itmp; }

*ll = l;
*rr = r;
while ( 1 ) {
	if ( l < p ) {
		if ( v[l].index < v[p].index ) {
			iswap( v[l].index, v[p].index )
			iswap( v[l].item,  v[p].item )
			p = l;
		} else {
			l++;
		}
	} else {
		if ( r > p ) {
			if ( v[r].index > v[p].index ) {
				iswap( v[r].index, v[p].index )
				iswap( v[r].item,  v[p].item )
				p = r;
				l++;
			} else {
				r--;
			}
		} else {
			*lr = p - 1;
			*rl = p + 1;
			*llen = *lr - *ll + 1;
			*rlen = *rr - *rl + 1;
			break;
		}
	}
}
}

/***********************************************************************/
/********************************************************
qsort_decreasing()
This procedure inputs a pointer to an index_struct, the subscript of an index array to be
sorted, a left subscript pointing to where the  sort is to begin in the index array, and a right
subscript where to end. This module invokes a  decreasing quick-sort sorting the index array  from l to r.
********************************************************/
/* return values: 0 == successful, 1 == error */
static int qsort_decreasing( struct cell v[], int left, int right )
{
int pivot;
int llen, rlen;
int lleft, lright, rleft, rright;


if ( pushstack( left  ))
	return 1;
if ( pushstack( right ))
	return 2;
while ( stack_pointer != stack ) {
	if (popstack(&right))
		return 3;
	if (popstack(&left ))
		return 4;
	if ( right - left > 0 ) {
		pivot = select_pivot( v, left, right );
		partition_dec( v, &llen, &rlen, &lleft, &lright, &rleft, &rright, pivot, left, right );
		if ( llen > rlen ) {
			if ( pushstack( lleft  ))
				return 5;
			if ( pushstack( lright ))
				return 6;
			if ( pushstack( rleft  ))
				return 7;
			if ( pushstack( rright ))
				return 8;
		} else{
			if ( pushstack( rleft  ))
				return 9;
			if ( pushstack( rright ))
				return 10;
			if ( pushstack( lleft  ))
				return 11;
			if ( pushstack( lright ))
				return 12;
		}
	}
}
return 0;
}

/***********************************************************************/
/* return values: 0 == successful, 1 == error */
int sort_order_decreasing(
		int values[],		/* INPUT:  the unsorted values themselves */
		int num,		/* INPUT:  the number of values */
		int order[]		/* OUTPUT: the order for each of the values if sorted */
		)
{
int i;
struct cell * cells;


cells = (struct cell *) malloc( num * sizeof(struct cell) );
if ( cells == (struct cell *) NULL ){
	fprintf( stderr, "%s: ERROR: malloc(): struct cell\n", get_progname() );
        return 1;
}

for( i = 0; i < num; i++ ) {
	cells[i].index = values[i];
	cells[i].item  = i;
}

if ( qsort_decreasing( cells, 0, num-1 ) < 0)
	return 2;

for( i = 0; i < num; i++ ) {
	order[i] = cells[i].item;
}

free( (void *) cells );

return 0;
}

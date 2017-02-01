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

      FILE:           BZ_DRVRS.C
      ALGORITHM:      Allan S. Bozorth (FBI)
      MODIFICATIONS:  Michael D. Garris (NIST)
                      Stan Janet (NIST)
      DATE:           09/21/2004

      Contains driver routines responsible for kicking off matches
      using the Bozorth3 fingerprint matching algorithm.

***********************************************************************

      ROUTINES:
#cat: bozorth_probe_init -   creates the pairwise minutia comparison
#cat:                        table for the probe fingerprint
#cat: bozorth_gallery_init - creates the pairwise minutia comparison
#cat:                        table for the gallery fingerprint
#cat: bozorth_to_gallery -   supports the matching scenario where the
#cat:                        same probe fingerprint is matches repeatedly
#cat:                        to multiple gallery fingerprints as in
#cat:                        identification mode
#cat: bozorth_main -         supports the matching scenario where a
#cat:                        single probe fingerprint is to be matched
#cat:                        to a single gallery fingerprint as in
#cat:                        verificaiton mode

***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bozorth.h>

/**************************************************************************/

int bozorth_probe_init( struct xyt_struct * pstruct )
{
int sim;	/* number of pointwise comparisons for Subject's record*/
int msim;	/* Pruned length of Subject's comparison pointer list */



/* Take Subject's points and compute pointwise comparison statistics table and sorted row-pointer list. */
/* This builds a "Web" of relative edge statistics between points. */
bz_comp(
	pstruct->nrows,
	pstruct->xcol,
	pstruct->ycol,
	pstruct->thetacol,
	&sim,
	scols,
	scolpt );

msim = sim;	/* Init search to end of Subject's pointwise comparison table (last edge in Web) */



bz_find( &msim, scolpt );



if ( msim < FDD )	/* Makes sure there are a reasonable number of edges (at least 500, if possible) to analyze in the Web */
	msim = ( sim > FDD ) ? FDD : sim;





return msim;
}

/**************************************************************************/

int bozorth_gallery_init( struct xyt_struct * gstruct )
{
int fim;	/* number of pointwise comparisons for On-File record*/
int mfim;	/* Pruned length of On-File Record's pointer list */


/* Take On-File Record's points and compute pointwise comparison statistics table and sorted row-pointer list. */
/* This builds a "Web" of relative edge statistics between points. */
bz_comp(
	gstruct->nrows,
	gstruct->xcol,
	gstruct->ycol,
	gstruct->thetacol,
	&fim,
	fcols,
	fcolpt );

mfim = fim;	/* Init search to end of On-File Record's pointwise comparison table (last edge in Web) */



bz_find( &mfim, fcolpt );



if ( mfim < FDD )	/* Makes sure there are a reasonable number of edges (at least 500, if possible) to analyze in the Web */
	mfim = ( fim > FDD ) ? FDD : fim;





return mfim;
}

/**************************************************************************/

int bozorth_to_gallery(
		int probe_len,
		struct xyt_struct * pstruct,
		struct xyt_struct * gstruct
		)
{
int np;
int gallery_len;

gallery_len = bozorth_gallery_init( gstruct );
np = bz_match( probe_len, gallery_len );
return bz_match_score( np, pstruct, gstruct );
}

/**************************************************************************/

int bozorth_main(
		struct xyt_struct * pstruct,
		struct xyt_struct * gstruct
		)
{
int ms;
int np;
int probe_len;
int gallery_len;



#ifdef DEBUG
	printf( "PROBE_INIT() called\n" );
#endif
probe_len   = bozorth_probe_init( pstruct );


#ifdef DEBUG
	printf( "GALLERY_INIT() called\n" );
#endif
gallery_len = bozorth_gallery_init( gstruct );


#ifdef DEBUG
	printf( "BZ_MATCH() called\n" );
#endif
np = bz_match( probe_len, gallery_len );


#ifdef DEBUG
	printf( "BZ_MATCH() returned %d edge pairs\n", np );
	printf( "COMPUTE() called\n" );
#endif
ms = bz_match_score( np, pstruct, gstruct );


#ifdef DEBUG
	printf( "COMPUTE() returned %d\n", ms );
#endif


return ms;
}

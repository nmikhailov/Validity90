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

      FILE:           BZ_GBLS.C
      ALGORITHM:      Allan S. Bozorth (FBI)
      MODIFICATIONS:  Michael D. Garris (NIST)
                      Stan Janet (NIST)
      DATE:           09/21/2004

      Contains global variables responsible for supporting the
      Bozorth3 fingerprint matching "core" algorithm.

***********************************************************************
***********************************************************************/

#include <bozorth.h>

/**************************************************************************/
/* General supporting global variables */
/**************************************************************************/

int colp[ COLP_SIZE_1 ][ COLP_SIZE_2 ];		/* Output from match(), this is a sorted table of compatible edge pairs containing: */
						/*	DeltaThetaKJs, Subject's K, J, then On-File's {K,J} or {J,K} depending */
						/* Sorted first on Subject's point index K, */
						/*	then On-File's K or J point index (depending), */
						/*	lastly on Subject's J point index */
int scols[ SCOLS_SIZE_1 ][ COLS_SIZE_2 ];	/* Subject's pointwise comparison table containing: */
						/*	Distance,min(BetaK,BetaJ),max(BetaK,BbetaJ), K,J,ThetaKJ */
int fcols[ FCOLS_SIZE_1 ][ COLS_SIZE_2 ];	/* On-File Record's pointwise comparison table with: */
						/*	Distance,min(BetaK,BetaJ),max(BetaK,BbetaJ),K,J, ThetaKJ */
int * scolpt[ SCOLPT_SIZE ];			/* Subject's list of pointers to pointwise comparison rows, sorted on: */
						/*	Distance, min(BetaK,BetaJ), then max(BetaK,BetaJ) */
int * fcolpt[ FCOLPT_SIZE ];			/* On-File Record's list of pointers to pointwise comparison rows sorted on: */
						/*	Distance, min(BetaK,BetaJ), then max(BetaK,BetaJ) */
int sc[ SC_SIZE ];				/* Flags all compatible edges in the Subject's Web */

int yl[ YL_SIZE_1 ][ YL_SIZE_2 ];


/**************************************************************************/
/* Globals used significantly by sift() */
/**************************************************************************/
int rq[ RQ_SIZE ] = {};
int tq[ TQ_SIZE ] = {};
int zz[ ZZ_SIZE ] = {};

int rx[ RX_SIZE ] = {};
int mm[ MM_SIZE ] = {};
int nn[ NN_SIZE ] = {};

int qq[ QQ_SIZE ] = {};

int rk[ RK_SIZE ] = {};

int cp[ CP_SIZE ] = {};
int rp[ RP_SIZE ] = {};

int rf[RF_SIZE_1][RF_SIZE_2] = {};
int cf[CF_SIZE_1][CF_SIZE_2] = {};

int y[20000] = {};


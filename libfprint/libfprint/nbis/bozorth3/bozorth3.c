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

      FILE:           BOZORTH3.C
      ALGORITHM:      Allan S. Bozorth (FBI)
      MODIFICATIONS:  Michael D. Garris (NIST)
                      Stan Janet (NIST)
      DATE:           09/21/2004

      Contains the "core" routines responsible for supporting the
      Bozorth3 fingerprint matching algorithm.

***********************************************************************

      ROUTINES:
#cat: bz_comp -  takes a set of minutiae (probe or gallery) and
#cat:            compares/measures  each minutia's {x,y,t} with every
#cat:            other minutia's {x,y,t} in the set creating a table
#cat:            of pairwise comparison entries
#cat: bz_find -  trims sorted table of pairwise minutia comparisons to
#cat:            a max distance of 75^2
#cat: bz_match - takes the two pairwise minutia comparison tables (a probe
#cat:            table and a gallery table) and compiles a list of
#cat:            all relatively "compatible" entries between the two
#cat:            tables generating a match table
#cat: bz_match_score - takes a match table and traverses it looking for
#cat:            a sufficiently long path (or a cluster of compatible paths)
#cat:            of "linked" match table entries
#cat:            the accumulation of which results in a match "score"
#cat: bz_sift -  main routine handling the path linking and match table
#cat:            traversal
#cat: bz_final_loop - (declared static) a final postprocess after
#cat:            the main match table traversal which looks to combine
#cat:            clusters of compatible paths

***********************************************************************/

#include <stdio.h>
#include <bozorth.h>

static const int verbose_bozorth = 0;
static const int m1_xyt = 0;

/***********************************************************************/
void bz_comp(
	int npoints,				/* INPUT: # of points */
	int xcol[     MAX_BOZORTH_MINUTIAE ],	/* INPUT: x cordinates */
	int ycol[     MAX_BOZORTH_MINUTIAE ],	/* INPUT: y cordinates */
	int thetacol[ MAX_BOZORTH_MINUTIAE ],	/* INPUT: theta values */

	int * ncomparisons,			/* OUTPUT: number of pointwise comparisons */
	int cols[][ COLS_SIZE_2 ],		/* OUTPUT: pointwise comparison table */
	int * colptrs[]				/* INPUT and OUTPUT: sorted list of pointers to rows in cols[] */
	)
{
int i, j, k;

int b;
int t;
int n;
int l;

int table_index;

int dx;
int dy;
int distance;

int theta_kj;
int beta_j;
int beta_k;

int * c;



c = &cols[0][0];

table_index = 0;
for ( k = 0; k < npoints - 1; k++ ) {
	for ( j = k + 1; j < npoints; j++ ) {


		if ( thetacol[j] > 0 ) {

			if ( thetacol[k] == thetacol[j] - 180 )
				continue;
		} else {

			if ( thetacol[k] == thetacol[j] + 180 )
				continue;
		}


		dx = xcol[j] - xcol[k];
		dy = ycol[j] - ycol[k];
		distance = SQUARED(dx) + SQUARED(dy);
		if ( distance > SQUARED(DM) ) {
			if ( dx > DM )
				break;
			else
				continue;

		}

					/* The distance is in the range [ 0, 125^2 ] */
		if ( dx == 0 )
			theta_kj = 90;
		else {
			double dz;

			if ( m1_xyt )
				dz = ( 180.0F / PI_SINGLE ) * atanf( (float) -dy / (float) dx );
			else
				dz = ( 180.0F / PI_SINGLE ) * atanf( (float) dy / (float) dx );
			if ( dz < 0.0F )
				dz -= 0.5F;
			else
				dz += 0.5F;
			theta_kj = (int) dz;
		}


		beta_k = theta_kj - thetacol[k];
		beta_k = IANGLE180(beta_k);

		beta_j = theta_kj - thetacol[j] + 180;
		beta_j = IANGLE180(beta_j);


		if ( beta_k < beta_j ) {
			*c++ = distance;
			*c++ = beta_k;
			*c++ = beta_j;
			*c++ = k+1;
			*c++ = j+1;
			*c++ = theta_kj;
		} else {
			*c++ = distance;
			*c++ = beta_j;
			*c++ = beta_k;
			*c++ = k+1;
			*c++ = j+1;
			*c++ = theta_kj + 400;

		}






		b = 0;
		t = table_index + 1;
		l = 1;
		n = -1;			/* Init binary search state ... */




		while ( t - b > 1 ) {
			int * midpoint;

			l = ( b + t ) / 2;
			midpoint = colptrs[l-1];




			for ( i=0; i < 3; i++ ) {
				int dd, ff;

				dd = cols[table_index][i];

				ff = midpoint[i];


				n = SENSE(dd,ff);


				if ( n < 0 ) {
					t = l;
					break;
				}
				if ( n > 0 ) {
					b = l;
					break;
				}
			}

			if ( n == 0 ) {
				n = 1;
				b = l;
			}
		} /* END while */

		if ( n == 1 )
			++l;




		for ( i = table_index; i >= l; --i )
			colptrs[i] = colptrs[i-1];


		colptrs[l-1] = &cols[table_index][0];
		++table_index;


		if ( table_index == 19999 ) {
#ifndef NOVERBOSE
			if ( verbose_bozorth )
				printf( "bz_comp(): breaking loop to avoid table overflow\n" );
#endif
			goto COMP_END;
		}

	} /* END for j */

} /* END for k */

COMP_END:
	*ncomparisons = table_index;

}

/***********************************************************************/
void bz_find(
	int * xlim,		/* INPUT:  number of pointwise comparisons in table */
				/* OUTPUT: determined insertion location (NOT ALWAYS SET) */
	int * colpt[]		/* INOUT:  sorted list of pointers to rows in the pointwise comparison table */
	)
{
int midpoint;
int top;
int bottom;
int state;
int distance;



/* binary search to locate the insertion location of a predefined distance in list of sorted distances */


bottom   = 0;
top      = *xlim + 1;
midpoint = 1;
state    = -1;

while ( top - bottom > 1 ) {
	midpoint = ( bottom + top ) / 2;
	distance = *colpt[ midpoint-1 ];
	state = SENSE_NEG_POS(FD,distance);
	if ( state < 0 )
		top = midpoint;
	else {
		bottom = midpoint;
	}
}

if ( state > -1 )
	++midpoint;

if ( midpoint < *xlim )
	*xlim = midpoint;



}

/***********************************************************************/
/* Make room in RTP list at insertion point by shifting contents down the
   list.  Then insert the address of the current ROT row into desired
   location */
/***********************************************************************/
static

void rtp_insert( int * rtp[], int l, int idx, int * ptr )
{
int shiftcount;
int ** r1;
int ** r2;


r1 = &rtp[idx];
r2 = r1 - 1;

shiftcount = ( idx - l ) + 1;
while ( shiftcount-- > 0 ) {
	*r1-- = *r2--;
}
*r1 = ptr;
}

/***********************************************************************/
/* Builds list of compatible edge pairs between the 2 Webs. */
/* The Edge pair DeltaThetaKJs and endpoints are sorted     */
/*	first on Subject's K,                               */
/*	then On-File's J or K (depending),                  */
/*	and lastly on Subject's J point index.              */
/* Return value is the # of compatible edge pairs           */
/***********************************************************************/
int bz_match(
	int probe_ptrlist_len,		/* INPUT:  pruned length of Subject's pointer list */
	int gallery_ptrlist_len		/* INPUT:  pruned length of On-File Record's pointer list */
	)
{
int i;			/* Temp index */
int ii;			/* Temp index */
int edge_pair_index;	/* Compatible edge pair index */
float dz;		/* Delta difference and delta angle stats */
float fi;		/* Distance limit based on factor TK */
int * ss;		/* Subject's comparison stats row */
int * ff;		/* On-File Record's comparison stats row */
int j;			/* On-File Record's row index */
int k;			/* Subject's row index */
int st;			/* Starting On-File Record's row index */
int p1;			/* Adjusted Subject's ThetaKJ, DeltaThetaKJs, K or J point index */
int p2;			/* Adjusted On-File's ThetaKJ, RTP point index */
int n;			/* ThetaKJ and binary search state variable */
int l;			/* Midpoint of binary search */
int b;			/* ThetaKJ state variable, and bottom of search range */
int t;			/* Top of search range */

register int * rotptr;


#define ROT_SIZE_1 20000
#define ROT_SIZE_2 5

static int rot[ ROT_SIZE_1 ][ ROT_SIZE_2 ];


static int * rtp[ ROT_SIZE_1 ];




/* These now externally defined in bozorth.h */
/* extern int * scolpt[ SCOLPT_SIZE ];			 INPUT */
/* extern int * fcolpt[ FCOLPT_SIZE ];			 INPUT */
/* extern int   colp[ COLP_SIZE_1 ][ COLP_SIZE_2 ];	 OUTPUT */
/* extern int verbose_bozorth; */
/* extern FILE * stderr; */
/* extern char * get_progname( void ); */
/* extern char * get_probe_filename( void ); */
/* extern char * get_gallery_filename( void ); */




st = 1;
edge_pair_index = 0;
rotptr = &rot[0][0];

/* Foreach sorted edge in Subject's Web ... */

for ( k = 1; k < probe_ptrlist_len; k++ ) {
	ss = scolpt[k-1];

	/* Foreach sorted edge in On-File Record's Web ... */

	for ( j = st; j <= gallery_ptrlist_len; j++ ) {
		ff = fcolpt[j-1];
		dz = *ff - *ss;

		fi = ( 2.0F * TK ) * ( *ff + *ss );








		if ( SQUARED(dz) > SQUARED(fi) ) {
			if ( dz < 0 ) {

				st = j + 1;

				continue;
			} else
				break;


		}



		for ( i = 1; i < 3; i++ ) {
			float dz_squared;

			dz = *(ss+i) - *(ff+i);
			dz_squared = SQUARED(dz);




			if ( dz_squared > TXS && dz_squared < CTXS )
				break;
		}

		if ( i < 3 )
			continue;






		if ( *(ss+5) >= 220 ) {
			p1 = *(ss+5) - 580;
			n  = 1;
		} else {
			p1 = *(ss+5);
			n  = 0;
		}


		if ( *(ff+5) >= 220 ) {
			p2 = *(ff+5) - 580;
			b  = 1;
		} else {
			p2 = *(ff+5);
			b  = 0;
		}

		p1 -= p2;
		p1 = IANGLE180(p1);
























		if ( n != b ) {

			*rotptr++ = p1;
			*rotptr++ = *(ss+3);
			*rotptr++ = *(ss+4);

			*rotptr++ = *(ff+4);
			*rotptr++ = *(ff+3);
		} else {
			*rotptr++ = p1;
			*rotptr++ = *(ss+3);
			*rotptr++ = *(ss+4);

			*rotptr++ = *(ff+3);
			*rotptr++ = *(ff+4);
		}






		n = -1;
		l = 1;
		b = 0;
		t = edge_pair_index + 1;
		while ( t - b > 1 ) {
			l = ( b + t ) / 2;

			for ( i = 0; i < 3; i++ ) {
				static int ii_table[] = { 1, 3, 2 };

								/*	1 = Subject's Kth, */
								/*	3 = On-File's Jth or Kth (depending), */
								/*	2 = Subject's Jth */

				ii = ii_table[i];
				p1 = rot[edge_pair_index][ii];
				p2 = *( rtp[l-1] + ii );

				n = SENSE(p1,p2);

				if ( n < 0 ) {
					t = l;
					break;
				}
				if ( n > 0 ) {
					b = l;
					break;
				}
			}

			if ( n == 0 ) {
				n = 1;
				b = l;
			}
		} /* END while() for binary search */


		if ( n == 1 )
			++l;

		rtp_insert( rtp, l, edge_pair_index, &rot[edge_pair_index][0] );
		++edge_pair_index;

		if ( edge_pair_index == 19999 ) {
#ifndef NOVERBOSE
			if ( verbose_bozorth )
				fprintf( stderr, "%s: bz_match(): WARNING: list is full, breaking loop early [p=%s; g=%s]\n",
							get_progname(), get_probe_filename(), get_gallery_filename() );
#endif
			goto END;		/* break out if list exceeded */
		}

	} /* END FOR On-File (edge) distance */

} /* END FOR Subject (edge) distance */



END:
{
	int * colp_ptr = &colp[0][0];

	for ( i = 0; i < edge_pair_index; i++ ) {
		INT_COPY( colp_ptr, rtp[i], COLP_SIZE_2 );


	}
}



return edge_pair_index;			/* Return the number of compatible edge pairs stored into colp[][] */
}

/**************************************************************************/
/* These global arrays are declared "static" as they are only used        */
/* between bz_match_score() & bz_final_loop()                             */
/**************************************************************************/
static int ct[ CT_SIZE ];
static int gct[ GCT_SIZE ];
static int ctt[ CTT_SIZE ];
static int ctp[ CTP_SIZE_1 ][ CTP_SIZE_2 ];
static int yy[ YY_SIZE_1 ][ YY_SIZE_2 ][ YY_SIZE_3 ];

static int    bz_final_loop( int );

/**************************************************************************/
int bz_match_score(
	int np,
	struct xyt_struct * pstruct,
	struct xyt_struct * gstruct
	)
{
int kx, kq;
int ftt;
int tot;
int qh;
int tp;
int ll, jj, kk, n, t, b;
int k, i, j, ii, z;
int kz, l;
int p1, p2;
int dw, ww;
int match_score;
int qq_overflow = 0;
float fi;

/* These next 3 arrays originally declared global, but moved here */
/* locally because they are only used herein */
int rr[ RR_SIZE ];
int avn[ AVN_SIZE ];
int avv[ AVV_SIZE_1 ][ AVV_SIZE_2 ];

/* These now externally defined in bozorth.h */
/* extern FILE * stderr; */
/* extern char * get_progname( void ); */
/* extern char * get_probe_filename( void ); */
/* extern char * get_gallery_filename( void ); */






if ( pstruct->nrows < MIN_COMPUTABLE_BOZORTH_MINUTIAE ) {
#ifndef NOVERBOSE
	if ( gstruct->nrows < MIN_COMPUTABLE_BOZORTH_MINUTIAE ) {
		if ( verbose_bozorth )
			fprintf( stderr, "%s: bz_match_score(): both probe and gallery file have too few minutiae (%d,%d) to compute a real Bozorth match score; min. is %d [p=%s; g=%s]\n",
						get_progname(),
						pstruct->nrows, gstruct->nrows, MIN_COMPUTABLE_BOZORTH_MINUTIAE,
						get_probe_filename(), get_gallery_filename() );
	} else {
		if ( verbose_bozorth )
			fprintf( stderr, "%s: bz_match_score(): probe file has too few minutiae (%d) to compute a real Bozorth match score; min. is %d [p=%s; g=%s]\n",
						get_progname(),
						pstruct->nrows, MIN_COMPUTABLE_BOZORTH_MINUTIAE,
						get_probe_filename(), get_gallery_filename() );
	}
#endif
	return ZERO_MATCH_SCORE;
}



if ( gstruct->nrows < MIN_COMPUTABLE_BOZORTH_MINUTIAE ) {
#ifndef NOVERBOSE
	if ( verbose_bozorth )
		fprintf( stderr, "%s: bz_match_score(): gallery file has too few minutiae (%d) to compute a real Bozorth match score; min. is %d [p=%s; g=%s]\n",
						get_progname(),
						gstruct->nrows, MIN_COMPUTABLE_BOZORTH_MINUTIAE,
						get_probe_filename(), get_gallery_filename() );
#endif
	return ZERO_MATCH_SCORE;
}









								/* initialize tables to 0's */
INT_SET( (int *) &yl, YL_SIZE_1 * YL_SIZE_2, 0 );



INT_SET( (int *) &sc, SC_SIZE, 0 );
INT_SET( (int *) &cp, CP_SIZE, 0 );
INT_SET( (int *) &rp, RP_SIZE, 0 );
INT_SET( (int *) &tq, TQ_SIZE, 0 );
INT_SET( (int *) &rq, RQ_SIZE, 0 );
INT_SET( (int *) &zz, ZZ_SIZE, 1000 );				/* zz[] initialized to 1000's */

INT_SET( (int *) &avn, AVN_SIZE, 0 );				/* avn[0...4] <== 0; */





tp  = 0;
p1  = 0;
tot = 0;
ftt = 0;
kx  = 0;
match_score = 0;

for ( k = 0; k < np - 1; k++ ) {
					/* printf( "compute(): looping with k=%d\n", k ); */

	if ( sc[k] )			/* If SC counter for current pair already incremented ... */
		continue;		/*		Skip to next pair */


	i = colp[k][1];
	t = colp[k][3];




	qq[0]   = i;
	rq[t-1] = i;
	tq[i-1] = t;


	ww = 0;
	dw = 0;

	do {
		ftt++;
		tot = 0;
		qh  = 1;
		kx  = k;




		do {









			kz = colp[kx][2];
			l  = colp[kx][4];
			kx++;
			bz_sift( &ww, kz, &qh, l, kx, ftt, &tot, &qq_overflow );
			if ( qq_overflow ) {
				fprintf( stderr, "%s: WARNING: bz_match_score(): qq[] overflow from bz_sift() #1 [p=%s; g=%s]\n",
							get_progname(), get_probe_filename(), get_gallery_filename() );
				return QQ_OVERFLOW_SCORE;
			}

#ifndef NOVERBOSE
			if ( verbose_bozorth )
				printf( "x1 %d %d %d %d %d %d\n", kx, colp[kx][0], colp[kx][1], colp[kx][2], colp[kx][3], colp[kx][4] );
#endif

		} while ( colp[kx][3] == colp[k][3] && colp[kx][1] == colp[k][1] );
			/* While the startpoints of lookahead edge pairs are the same as the starting points of the */
			/* current pair, set KQ to lookahead edge pair index where above bz_sift() loop left off */

		kq = kx;

		for ( j = 1; j < qh; j++ ) {
			for ( i = kq; i < np; i++ ) {

				for ( z = 1; z < 3; z++ ) {
					if ( z == 1 ) {
						if ( (j+1) > QQ_SIZE ) {
							fprintf( stderr, "%s: WARNING: bz_match_score(): qq[] overflow #1 in bozorth3(); j-1 is %d [p=%s; g=%s]\n",
								get_progname(), j-1, get_probe_filename(), get_gallery_filename() );
							return QQ_OVERFLOW_SCORE;
						}
						p1 = qq[j];
					} else {
						p1 = tq[p1-1];

					}






					if ( colp[i][2*z] != p1 )
						break;
				}


				if ( z == 3 ) {
					z = colp[i][1];
					l = colp[i][3];



					if ( z != colp[k][1] && l != colp[k][3] ) {
						kx = i + 1;
						bz_sift( &ww, z, &qh, l, kx, ftt, &tot, &qq_overflow );
						if ( qq_overflow ) {
							fprintf( stderr, "%s: WARNING: bz_match_score(): qq[] overflow from bz_sift() #2 [p=%s; g=%s]\n",
								get_progname(), get_probe_filename(), get_gallery_filename() );
							return QQ_OVERFLOW_SCORE;
						}
					}
				}
			} /* END for i */



			/* Done looking ahead for current j */





			l = 1;
			t = np + 1;
			b = kq;

			while ( t - b > 1 ) {
				l = ( b + t ) / 2;

				for ( i = 1; i < 3; i++ ) {

					if ( i == 1 ) {
						if ( (j+1) > QQ_SIZE ) {
							fprintf( stderr, "%s: WARNING: bz_match_score(): qq[] overflow #2 in bozorth3(); j-1 is %d [p=%s; g=%s]\n",
								get_progname(), j-1, get_probe_filename(), get_gallery_filename() );
							return QQ_OVERFLOW_SCORE;
						}
						p1 = qq[j];
					} else {
						p1 = tq[p1-1];
					}



					p2 = colp[l-1][i*2-1];

					n = SENSE(p1,p2);

					if ( n < 0 ) {
						t = l;
						break;
					}
					if ( n > 0 ) {
						b = l;
						break;
					}
				}

				if ( n == 0 ) {






					/* Locates the head of consecutive sequence of edge pairs all having the same starting Subject and On-File edgepoints */
					while ( colp[l-2][3] == p2 && colp[l-2][1] == colp[l-1][1] )
						l--;

					kx = l - 1;


					do {
						kz = colp[kx][2];
						l  = colp[kx][4];
						kx++;
						bz_sift( &ww, kz, &qh, l, kx, ftt, &tot, &qq_overflow );
						if ( qq_overflow ) {
							fprintf( stderr, "%s: WARNING: bz_match_score(): qq[] overflow from bz_sift() #3 [p=%s; g=%s]\n",
								get_progname(), get_probe_filename(), get_gallery_filename() );
							return QQ_OVERFLOW_SCORE;
						}
					} while ( colp[kx][3] == p2 && colp[kx][1] == colp[kx-1][1] );

					break;
				} /* END if ( n == 0 ) */

			} /* END while */

		} /* END for j */




		if ( tot >= MSTR ) {
			jj = 0;
			kk = 0;
			n  = 0;
			l  = 0;

			for ( i = 0; i < tot; i++ ) {


				int colp_value = colp[ y[i]-1 ][0];
				if ( colp_value < 0 ) {
					kk += colp_value;
					n++;
				} else {
					jj += colp_value;
					l++;
				}
			}


			if ( n == 0 ) {
				n = 1;
			} else if ( l == 0 ) {
				l = 1;
			}



			fi = (float) jj / (float) l - (float) kk / (float) n;

			if ( fi > 180.0F ) {
				fi = ( jj + kk + n * 360 ) / (float) tot;
				if ( fi > 180.0F )
					fi -= 360.0F;
			} else {
				fi = ( jj + kk ) / (float) tot;
			}

			jj = ROUND(fi);
			if ( jj <= -180 )
				jj += 360;



			kk = 0;
			for ( i = 0; i < tot; i++ ) {
				int diff = colp[ y[i]-1 ][0] - jj;
				j = SQUARED( diff );




				if ( j > TXS && j < CTXS )
					kk++;
				else
					y[i-kk] = y[i];
			} /* END FOR i */

			tot -= kk;				/* Adjust the total edge pairs TOT based on # of edge pairs skipped */

		} /* END if ( tot >= MSTR ) */




		if ( tot < MSTR ) {




			for ( i = tot-1 ; i >= 0; i-- ) {
				int idx = y[i] - 1;
				if ( rk[idx] == 0 ) {
					sc[idx] = -1;
				} else {
					sc[idx] = rk[idx];
				}
			}
			ftt--;

		} else {		/* tot >= MSTR */
					/* Otherwise size of TOT group (seq. of TOT indices stored in Y) is large enough to analyze */

			int pa = 0;
			int pb = 0;
			int pc = 0;
			int pd = 0;

			for ( i = 0; i < tot; i++ ) {
				int idx = y[i] - 1;
				for ( ii = 1; ii < 4; ii++ ) {




					kk = ( SQUARED(ii) - ii + 2 ) / 2 - 1;




					jj = colp[idx][kk];

					switch ( ii ) {
					  case 1:
						if ( colp[idx][0] < 0 ) {
							pd += colp[idx][0];
							pb++;
						} else {
							pa += colp[idx][0];
							pc++;
						}
						break;
					  case 2:
						avn[ii-1] += pstruct->xcol[jj-1];
						avn[ii] += pstruct->ycol[jj-1];
						break;
					  default:
						avn[ii] += gstruct->xcol[jj-1];
						avn[ii+1] += gstruct->ycol[jj-1];
						break;
					} /* switch */
				} /* END for ii = [1..3] */

				for ( ii = 0; ii < 2; ii++ ) {
					n = -1;
					l = 1;

					for ( jj = 1; jj < 3; jj++ ) {










						p1 = colp[idx][ 2 * ii + jj ];


						b = 0;
						t = yl[ii][tp] + 1;

						while ( t - b > 1 ) {
							l  = ( b + t ) / 2;
							p2 = yy[l-1][ii][tp];
							n  = SENSE(p1,p2);

							if ( n < 0 ) {
								t = l;
							} else {
								if ( n > 0 ) {
									b = l;
								} else {
									break;
								}
							}
						} /* END WHILE */

						if ( n != 0 ) {
							if ( n == 1 )
								++l;

							for ( kk = yl[ii][tp]; kk >= l; --kk ) {
								yy[kk][ii][tp] = yy[kk-1][ii][tp];
							}

							++yl[ii][tp];
							yy[l-1][ii][tp] = p1;


						} /* END if ( n != 0 ) */

						/* Otherwise, edgepoint already stored in YY */

					} /* END FOR jj in [1,2] */
				} /* END FOR ii in [0,1] */
			} /* END FOR i */

			if ( pb == 0 ) {
				pb = 1;
			} else if ( pc == 0 ) {
				pc = 1;
			}



			fi = (float) pa / (float) pc - (float) pd / (float) pb;
			if ( fi > 180.0F ) {

				fi = ( pa + pd + pb * 360 ) / (float) tot;
				if ( fi > 180.0F )
					fi -= 360.0F;
			} else {
				fi = ( pa + pd ) / (float) tot;
			}

			pa = ROUND(fi);
			if ( pa <= -180 )
				pa += 360;



			avv[tp][0] = pa;

			for ( ii = 1; ii < 5; ii++ ) {
				avv[tp][ii] = avn[ii] / tot;
				avn[ii] = 0;
			}

			ct[tp]  = tot;
			gct[tp] = tot;

			if ( tot > match_score )		/* If current TOT > match_score ... */
				match_score = tot;		/*	Keep track of max TOT in match_score */

			ctt[tp]    = 0;		/* Init CTT[TP] to 0 */
			ctp[tp][0] = tp;	/* Store TP into CTP */

			for ( ii = 0; ii < tp; ii++ ) {
				int found;
				int diff;

				int * avv_tp_ptr = &avv[tp][0];
				int * avv_ii_ptr = &avv[ii][0];
				diff = *avv_tp_ptr++ - *avv_ii_ptr++;
				j = SQUARED( diff );






				if ( j > TXS && j < CTXS )
					continue;









				ll = *avv_tp_ptr++ - *avv_ii_ptr++;
				jj = *avv_tp_ptr++ - *avv_ii_ptr++;
				kk = *avv_tp_ptr++ - *avv_ii_ptr++;
				j  = *avv_tp_ptr++ - *avv_ii_ptr++;

				{
				float tt, ai, dz;

				tt = (float) (SQUARED(ll) + SQUARED(jj));
				ai = (float) (SQUARED(j)  + SQUARED(kk));

				fi = ( 2.0F * TK ) * ( tt + ai );
				dz = tt - ai;


				if ( SQUARED(dz) > SQUARED(fi) )
					continue;
				}



				if ( ll ) {

					if ( m1_xyt )
						fi = ( 180.0F / PI_SINGLE ) * atanf( (float) -jj / (float) ll );
					else
						fi = ( 180.0F / PI_SINGLE ) * atanf( (float) jj / (float) ll );
					if ( fi < 0.0F ) {
						if ( ll < 0 )
							fi += 180.5F;
						else
							fi -= 0.5F;
					} else {
						if ( ll < 0 )
							fi -= 180.5F;
						else
							fi += 0.5F;
					}
					jj = (int) fi;
					if ( jj <= -180 )
						jj += 360;
				} else {

					if ( m1_xyt ) {
						if ( jj > 0 )
							jj = -90;
						else
							jj = 90;
					} else {
						if ( jj > 0 )
							jj = 90;
						else
							jj = -90;
					}
				}



				if ( kk ) {

					if ( m1_xyt )
						fi = ( 180.0F / PI_SINGLE ) * atanf( (float) -j / (float) kk );
					else
						fi = ( 180.0F / PI_SINGLE ) * atanf( (float) j / (float) kk );
					if ( fi < 0.0F ) {
						if ( kk < 0 )
							fi += 180.5F;
						else
							fi -= 0.5F;
					} else {
						if ( kk < 0 )
							fi -= 180.5F;
						else
							fi += 0.5F;
					}
					j = (int) fi;
					if ( j <= -180 )
						j += 360;
				} else {

					if ( m1_xyt ) {
						if ( j > 0 )
							j = -90;
						else
							j = 90;
					} else {
						if ( j > 0 )
							j = 90;
						else
							j = -90;
					}
				}





				pa = 0;
				pb = 0;
				pc = 0;
				pd = 0;

				if ( avv[tp][0] < 0 ) {
					pd += avv[tp][0];
					pb++;
				} else {
					pa += avv[tp][0];
					pc++;
				}

				if ( avv[ii][0] < 0 ) {
					pd += avv[ii][0];
					pb++;
				} else {
					pa += avv[ii][0];
					pc++;
				}

				if ( pb == 0 ) {
					pb = 1;
				} else if ( pc == 0 ) {
					pc = 1;
				}



				fi = (float) pa / (float) pc - (float) pd / (float) pb;

				if ( fi > 180.0F ) {
					fi = ( pa + pd + pb * 360 ) / 2.0F;
					if ( fi > 180.0F )
						fi -= 360.0F;
				} else {
					fi = ( pa + pd ) / 2.0F;
				}

				pb = ROUND(fi);
				if ( pb <= -180 )
					pb += 360;





				pa = jj - j;
				pa = IANGLE180(pa);
				kk = SQUARED(pb-pa);




				/* Was: if ( SQUARED(kk) > TXS && kk < CTXS ) : assume typo */
				if ( kk > TXS && kk < CTXS )
					continue;


				found = 0;
				for ( kk = 0; kk < 2; kk++ ) {
					jj = 0;
					ll = 0;

					do {
						while ( yy[jj][kk][ii] < yy[ll][kk][tp] && jj < yl[kk][ii] ) {

							jj++;
						}




						while ( yy[jj][kk][ii] > yy[ll][kk][tp] && ll < yl[kk][tp] ) {

							ll++;
						}




						if ( yy[jj][kk][ii] == yy[ll][kk][tp] && jj < yl[kk][ii] && ll < yl[kk][tp] ) {
							found = 1;
							break;
						}


					} while ( jj < yl[kk][ii] && ll < yl[kk][tp] );
					if ( found )
						break;
				} /* END for kk */

				if ( ! found ) {			/* If we didn't find what we were searching for ... */
					gct[ii] += ct[tp];
					if ( gct[ii] > match_score )
						match_score = gct[ii];
					++ctt[ii];
					ctp[ii][ctt[ii]] = tp;
				}

			} /* END for ii in [0,TP-1] prior TP group */

			tp++;			/* Bump TP counter */


		} /* END ELSE if ( tot == MSTR ) */



		if ( qh > QQ_SIZE ) {
			fprintf( stderr, "%s: WARNING: bz_match_score(): qq[] overflow #3 in bozorth3(); qh-1 is %d [p=%s; g=%s]\n",
					get_progname(), qh-1, get_probe_filename(), get_gallery_filename() );
			return QQ_OVERFLOW_SCORE;
		}
		for ( i = qh - 1; i > 0; i-- ) {
			n = qq[i] - 1;
			if ( ( tq[n] - 1 ) >= 0 ) {
				rq[tq[n]-1] = 0;
				tq[n]       = 0;
				zz[n]       = 1000;
			}
		}

		for ( i = dw - 1; i >= 0; i-- ) {
			n = rr[i] - 1;
			if ( tq[n] ) {
				rq[tq[n]-1] = 0;
				tq[n]       = 0;
			}
		}

		i = 0;
		j = ww - 1;
		while ( i >= 0 && j >= 0 ) {
			if ( nn[j] < mm[j] ) {
				++nn[j];

				for ( i = ww - 1; i >= 0; i-- ) {
					int rt = rx[i];
					if ( rt < 0 ) {
						rt = - rt;
						rt--;
						z  = rf[i][nn[i]-1]-1;



						if (( tq[z] != (rt+1) && tq[z] ) || ( rq[rt] != (z+1) && rq[rt] ))
							break;


						tq[z]  = rt+1;
						rq[rt] = z+1;
						rr[i]  = z+1;
					} else {
						rt--;
						z = cf[i][nn[i]-1]-1;


						if (( tq[rt] != (z+1) && tq[rt] ) || ( rq[z] != (rt+1) && rq[z] ))
							break;


						tq[rt] = z+1;
						rq[z]  = rt+1;
						rr[i]  = rt+1;
					}
				} /* END for i */

				if ( i >= 0 ) {
					for ( z = i + 1; z < ww; z++) {
						n = rr[z] - 1;
						if ( tq[n] - 1 >= 0 ) {
							rq[tq[n]-1] = 0;
							tq[n]       = 0;
						}
					}
					j = ww - 1;
				}

			} else {
				nn[j] = 1;
				j--;
			}

		}

		if ( tp > 1999 )
			break;

		dw = ww;


	} while ( j >= 0 ); /* END while endpoint group remain ... */


	if ( tp > 1999 )
		break;




	n = qq[0] - 1;
	if ( tq[n] - 1 >= 0 ) {
		rq[tq[n]-1] = 0;
		tq[n]       = 0;
	}

	for ( i = ww-1; i >= 0; i-- ) {
		n = rx[i];
		if ( n < 0 ) {
			n = - n;
			rp[n-1] = 0;
		} else {
			cp[n-1] = 0;
		}

	}

} /* END FOR each edge pair */



if ( match_score < MMSTR ) {
	return match_score;
}

match_score = bz_final_loop( tp );
return match_score;
}


/***********************************************************************/
/* These globals signficantly used by bz_sift () */
/* Now externally defined in bozorth.h */
/* extern int sc[ SC_SIZE ]; */
/* extern int rq[ RQ_SIZE ]; */
/* extern int tq[ TQ_SIZE ]; */
/* extern int rf[ RF_SIZE_1 ][ RF_SIZE_2 ]; */
/* extern int cf[ CF_SIZE_1 ][ CF_SIZE_2 ]; */
/* extern int zz[ ZZ_SIZE ]; */
/* extern int rx[ RX_SIZE ]; */
/* extern int mm[ MM_SIZE ]; */
/* extern int nn[ NN_SIZE ]; */
/* extern int qq[ QQ_SIZE ]; */
/* extern int rk[ RK_SIZE ]; */
/* extern int cp[ CP_SIZE ]; */
/* extern int rp[ RP_SIZE ]; */
/* extern int y[ Y_SIZE ]; */

void bz_sift(
	int * ww,		/* INPUT and OUTPUT; endpoint groups index; *ww may be bumped by one or by two */
	int   kz,		/* INPUT only;       endpoint of lookahead Subject edge */
	int * qh,		/* INPUT and OUTPUT; the value is an index into qq[] and is stored in zz[]; *qh may be bumped by one */
	int   l,		/* INPUT only;       endpoint of lookahead On-File edge */
	int   kx,		/* INPUT only -- index */
	int   ftt,		/* INPUT only */
	int * tot,		/* OUTPUT -- counter is incremented by one, sometimes */
	int * qq_overflow	/* OUTPUT -- flag is set only if qq[] overflows */
	)
{
int n;
int t;

/* These now externally defined in bozorth.h */
/* extern FILE * stderr; */
/* extern char * get_progname( void ); */
/* extern char * get_probe_filename( void ); */
/* extern char * get_gallery_filename( void ); */



n = tq[ kz - 1];	/* Lookup On-File edgepoint stored in TQ at index of endpoint of lookahead Subject edge */
t = rq[ l  - 1];	/* Lookup Subject edgepoint stored in RQ at index of endpoint of lookahead On-File edge */

if ( n == 0 && t == 0 ) {


	if ( sc[kx-1] != ftt ) {
		y[ (*tot)++ ] = kx;
		rk[kx-1] = sc[kx-1];
		sc[kx-1] = ftt;
	}

	if ( *qh >= QQ_SIZE ) {
		fprintf( stderr, "%s: ERROR: bz_sift(): qq[] overflow #1; the index [*qh] is %d [p=%s; g=%s]\n",
						get_progname(),
						*qh, get_probe_filename(), get_gallery_filename() );
		*qq_overflow = 1;
		return;
	}
	qq[ *qh ]  = kz;
	zz[ kz-1 ] = (*qh)++;


				/* The TQ and RQ locations are set, so set them ... */
	tq[ kz-1 ] = l;
	rq[ l-1 ] = kz;

	return;
} /* END if ( n == 0 && t == 0 ) */









if ( n == l ) {

	if ( sc[kx-1] != ftt ) {
		if ( zz[kx-1] == 1000 ) {
			if ( *qh >= QQ_SIZE ) {
				fprintf( stderr, "%s: ERROR: bz_sift(): qq[] overflow #2; the index [*qh] is %d [p=%s; g=%s]\n",
							get_progname(),
							*qh,
							get_probe_filename(), get_gallery_filename() );
				*qq_overflow = 1;
				return;
			}
			qq[*qh]  = kz;
			zz[kz-1] = (*qh)++;
		}
		y[(*tot)++] = kx;
		rk[kx-1] = sc[kx-1];
		sc[kx-1] = ftt;
	}

	return;
} /* END if ( n == l ) */





if ( *ww >= WWIM )	/* This limits the number of endpoint groups that can be constructed */
	return;


{
int b;
int b_index;
register int i;
int notfound;
int lim;
register int * lptr;

/* If lookahead Subject endpoint previously assigned to TQ but not paired with lookahead On-File endpoint ... */

if ( n ) {
	b = cp[ kz - 1 ];
	if ( b == 0 ) {
		b              = ++*ww;
		b_index        = b - 1;
		cp[kz-1]       = b;
		cf[b_index][0] = n;
		mm[b_index]    = 1;
		nn[b_index]    = 1;
		rx[b_index]    = kz;

	} else {
		b_index = b - 1;
	}

	lim = mm[b_index];
	lptr = &cf[b_index][0];
	notfound = 1;

#ifndef NOVERBOSE
	if ( verbose_bozorth ) {
		int * llptr = lptr;
		printf( "bz_sift(): n: looking for l=%d in [", l );
		for ( i = 0; i < lim; i++ ) {
			printf( " %d", *llptr++ );
		}
		printf( " ]\n" );
	}
#endif

	for ( i = 0; i < lim; i++ ) {
		if ( *lptr++ == l ) {
			notfound = 0;
			break;
		}
	}
	if ( notfound ) {		/* If lookahead On-File endpoint not in list ... */
		cf[b_index][i] = l;
		++mm[b_index];
	}
} /* END if ( n ) */


/* If lookahead On-File endpoint previously assigned to RQ but not paired with lookahead Subject endpoint... */

if ( t ) {
	b = rp[ l - 1 ];
	if ( b == 0 ) {
		b              = ++*ww;
		b_index        = b - 1;
		rp[l-1]        = b;
		rf[b_index][0] = t;
		mm[b_index]    = 1;
		nn[b_index]    = 1;
		rx[b_index]    = -l;


	} else {
		b_index = b - 1;
	}

	lim = mm[b_index];
	lptr = &rf[b_index][0];
	notfound = 1;

#ifndef NOVERBOSE
	if ( verbose_bozorth ) {
		int * llptr = lptr;
		printf( "bz_sift(): t: looking for kz=%d in [", kz );
		for ( i = 0; i < lim; i++ ) {
			printf( " %d", *llptr++ );
		}
		printf( " ]\n" );
	}
#endif

	for ( i = 0; i < lim; i++ ) {
		if ( *lptr++ == kz ) {
			notfound = 0;
			break;
		}
	}
	if ( notfound ) {		/* If lookahead Subject endpoint not in list ... */
		rf[b_index][i] = kz;
		++mm[b_index];
	}
} /* END if ( t ) */

}

}

/**************************************************************************/

static int bz_final_loop( int tp )
{
int ii, i, t, b, n, k, j, kk, jj;
int lim;
int match_score;

/* This array originally declared global, but moved here */
/* locally because it is only used herein.  The use of   */
/* "static" is required as the array will exceed the     */
/* stack allocation on our local systems otherwise.      */
static int sct[ SCT_SIZE_1 ][ SCT_SIZE_2 ];

match_score = 0;
for ( ii = 0; ii < tp; ii++ ) {				/* For each index up to the current value of TP ... */

		if ( match_score >= gct[ii] )		/* if next group total not bigger than current match_score.. */
			continue;			/*		skip to next TP index */

		lim = ctt[ii] + 1;
		for ( i = 0; i < lim; i++ ) {
			sct[i][0] = ctp[ii][i];
		}

		t     = 0;
		y[0]  = lim;
		cp[0] = 1;
		b     = 0;
		n     = 1;
		do {					/* looping until T < 0 ... */
			if ( y[t] - cp[t] > 1 ) {
				k = sct[cp[t]][t];
				j = ctt[k] + 1;
				for ( i = 0; i < j; i++ ) {
					rp[i] = ctp[k][i];
				}
				k  = 0;
				kk = cp[t];
				jj = 0;

				do {
					while ( rp[jj] < sct[kk][t] && jj < j )
						jj++;
					while ( rp[jj] > sct[kk][t] && kk < y[t] )
						kk++;
					while ( rp[jj] == sct[kk][t] && kk < y[t] && jj < j ) {
						sct[k][t+1] = sct[kk][t];
						k++;
						kk++;
						jj++;
					}
				} while ( kk < y[t] && jj < j );

				t++;
				cp[t] = 1;
				y[t]  = k;
				b     = t;
				n     = 1;
			} else {
				int tot = 0;

				lim = y[t];
				for ( i = n-1; i < lim; i++ ) {
					tot += ct[ sct[i][t] ];
				}

				for ( i = 0; i < b; i++ ) {
					tot += ct[ sct[0][i] ];
				}

				if ( tot > match_score ) {		/* If the current total is larger than the running total ... */
					match_score = tot;		/*	then set match_score to the new total */
					for ( i = 0; i < b; i++ ) {
						rk[i] = sct[0][i];
					}

					{
					int rk_index = b;
					lim = y[t];
					for ( i = n-1; i < lim; ) {
						rk[ rk_index++ ] = sct[ i++ ][ t ];
					}
					}
				}
				b = t;
				t--;
				if ( t >= 0 ) {
					++cp[t];
					n = y[t];
				}
			} /* END IF */

		} while ( t >= 0 );

} /* END FOR ii */

return match_score;

} /* END bz_final_loop() */

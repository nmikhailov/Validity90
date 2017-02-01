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

      FILE:    MATCHPAT.C
      AUTHOR:  Michael D. Garris
      DATE:    05/11/1999
      UPDATED: 03/16/2005 by MDG

      Contains routines responsible for matching minutia feature
      patterns as part of the NIST Latent Fingerprint System (LFS).

***********************************************************************
               ROUTINES:
                        match_1st_pair()
                        match_2nd_pair()
                        match_3rd_pair()
                        skip_repeated_horizontal_pair()
                        skip_repeated_vertical_pair()
***********************************************************************/

#include <stdio.h>
#include <lfs.h>

/*************************************************************************
**************************************************************************
#cat: match_1st_pair - Determines which of the feature_patterns[] have their
#cat:            first pixel pair match the specified pixel pair.

   Input:
      p1 - first pixel value of pair
      p2 - second pixel value of pair
   Output:
      possible - list of matching feature_patterns[] indices
      nposs    - number of matches
   Return Code:
      nposs    - number of matches
*************************************************************************/
int match_1st_pair(unsigned char p1, unsigned char p2,
                   int *possible, int *nposs)
{
   int i;

   /* Set possibilities to 0 */
   *nposs = 0;

   /* Foreach set of feature pairs ... */
   for(i = 0; i < NFEATURES; i++){
      /* If current scan pair matches first pair for feature ... */
      if((p1==g_feature_patterns[i].first[0]) &&
         (p2==g_feature_patterns[i].first[1])){
         /* Store feature as a possible match. */
         possible[*nposs] = i;
         /* Bump number of stored possibilities. */
         (*nposs)++;
      }
   }

   /* Return number of stored possibilities. */
   return(*nposs);
}

/*************************************************************************
**************************************************************************
#cat: match_2nd_pair - Determines which of the passed feature_patterns[] have
#cat:            their second pixel pair match the specified pixel pair.

   Input:
      p1 - first pixel value of pair
      p2 - second pixel value of pair
      possible - list of potentially-matching feature_patterns[] indices
      nposs    - number of potential matches
   Output:
      possible - list of matching feature_patterns[] indices
      nposs    - number of matches
   Return Code:
      nposs    - number of matches
*************************************************************************/
int match_2nd_pair(unsigned char p1, unsigned char p2,
                   int *possible, int *nposs)
{
   int i;
   int tnposs;

   /* Store input possibilities. */
   tnposs = *nposs;
   /* Reset output possibilities to 0. */
   *nposs = 0;

   /* If current scan pair values are the same ... */
   if(p1 == p2)
      /* Simply return because pair can't be a second feature pair. */
      return(*nposs);

   /* Foreach possible match based on first pair ... */
   for(i = 0; i < tnposs; i++){
      /* If current scan pair matches second pair for feature ... */
      if((p1==g_feature_patterns[possible[i]].second[0]) &&
         (p2==g_feature_patterns[possible[i]].second[1])){
         /* Store feature as a possible match. */
         possible[*nposs] = possible[i];
         /* Bump number of stored possibilities. */
         (*nposs)++;
      }
   }

   /* Return number of stored possibilities. */
   return(*nposs);
}

/*************************************************************************
**************************************************************************
#cat: match_3rd_pair - Determines which of the passed feature_patterns[] have
#cat:            their third pixel pair match the specified pixel pair.

   Input:
      p1 - first pixel value of pair
      p2 - second pixel value of pair
      possible - list of potentially-matching feature_patterns[] indices
      nposs    - number of potential matches
   Output:
      possible - list of matching feature_patterns[] indices
      nposs    - number of matches
   Return Code:
      nposs    - number of matches
*************************************************************************/
int match_3rd_pair(unsigned char p1, unsigned char p2,
                   int *possible, int *nposs)
{
   int i;
   int tnposs;

   /* Store input possibilities. */
   tnposs = *nposs;
   /* Reset output possibilities to 0. */
   *nposs = 0;

   /* Foreach possible match based on first and second pairs ... */
   for(i = 0; i < tnposs; i++){
      /* If current scan pair matches third pair for feature ... */
      if((p1==g_feature_patterns[possible[i]].third[0]) &&
         (p2==g_feature_patterns[possible[i]].third[1])){
         /* Store feature as a possible match. */
         possible[*nposs] = possible[i];
         /* Bump number of stored possibilities. */
         (*nposs)++;
      }
   }

   /* Return number of stored possibilities. */
   return(*nposs);
}

/*************************************************************************
**************************************************************************
#cat: skip_repeated_horizontal_pair - Takes the location of two pixel in
#cat:            adjacent pixel rows within an image region and skips
#cat:            rightward until the either the pixel pair no longer repeats
#cat:            itself or the image region is exhausted.

   Input:
      cx    - current x-coord of starting pixel pair
      ex    - right edge of the image region
      p1ptr - pointer to current top pixel in pair
      p2ptr - pointer to current bottom pixel in pair
      iw    - width (in pixels) of image
      ih    - height (in pixels) of image
   Output:
      cx    - x-coord of where rightward skip terminated
      p1ptr - points to top pixel where rightward skip terminated
      p2ptr - points to bottom pixel where rightward skip terminated
*************************************************************************/
void skip_repeated_horizontal_pair(int *cx, const int ex,
                   unsigned char **p1ptr, unsigned char **p2ptr,
                   const int iw, const int ih)
{
   int old1, old2;

   /* Store starting pixel pair. */
   old1 = **p1ptr;
   old2 = **p2ptr;

   /* Bump horizontally to next pixel pair. */
   (*cx)++;
   (*p1ptr)++;
   (*p2ptr)++;

   /* While not at right of scan region... */
   while(*cx < ex){
      /* If one or the other pixels in the new pair are different */
      /* from the starting pixel pair...                          */
      if((**p1ptr != old1) || (**p2ptr != old2))
         /* Done skipping repreated pixel pairs. */
         return;
      /* Otherwise, bump horizontally to next pixel pair. */
      (*cx)++;
      (*p1ptr)++;
      (*p2ptr)++;
   }
}

/*************************************************************************
**************************************************************************
#cat: skip_repeated_vertical_pair - Takes the location of two pixel in
#cat:            adjacent pixel columns within an image region and skips
#cat:            downward until the either the pixel pair no longer repeats
#cat:            itself or the image region is exhausted.

   Input:
      cy    - current y-coord of starting pixel pair
      ey    - bottom of the image region
      p1ptr - pointer to current left pixel in pair
      p2ptr - pointer to current right pixel in pair
      iw    - width (in pixels) of image
      ih    - height (in pixels) of image
   Output:
      cy    - y-coord of where downward skip terminated
      p1ptr - points to left pixel where downward skip terminated
      p2ptr - points to right pixel where donward skip terminated
*************************************************************************/
void skip_repeated_vertical_pair(int *cy, const int ey,
                   unsigned char **p1ptr, unsigned char **p2ptr,
                   const int iw, const int ih)
{
   int old1, old2;

   /* Store starting pixel pair. */
   old1 = **p1ptr;
   old2 = **p2ptr;

   /* Bump vertically to next pixel pair. */
   (*cy)++;
   (*p1ptr)+=iw;
   (*p2ptr)+=iw;

   /* While not at bottom of scan region... */
   while(*cy < ey){
      /* If one or the other pixels in the new pair are different */
      /* from the starting pixel pair...                          */
      if((**p1ptr != old1) || (**p2ptr != old2))
         /* Done skipping repreated pixel pairs. */
         return;
      /* Otherwise, bump vertically to next pixel pair. */
      (*cy)++;
      (*p1ptr)+=iw;
      (*p2ptr)+=iw;
   }
}


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

      FILE:    QUALITY.C
      AUTHOR:  Michael D. Garris
      DATE:    09/25/2000
      UPDATED: 03/16/2005 by MDG

      Contains routines responsible for assessing minutia quality
      and assigning different reliability measures.  These routines
      are primarily to support the rejection of bad minutiae.

***********************************************************************
               ROUTINES:
                        gen_quality_map()
                        combined_minutia_quality()
                        grayscale_reliability()
                        get_neighborhood_stats()

***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <lfs.h>

/***********************************************************************
************************************************************************
#cat: gen_quality_map - Takes a direction map, low contrast map, low ridge
#cat:              flow map, and high curvature map, and combines them
#cat:              into a single map containing 5 levels of decreasing
#cat:              quality.  This is done through a set of heuristics.

   Code originally written by Austin Hicklin for FBI ATU
   Modified by Michael D. Garris (NIST) Sept. 1, 2000

   Set quality of 0(unusable)..4(good) (I call these grades A..F)
      0/F: low contrast OR no direction
      1/D: low flow OR high curve
           (with low contrast OR no direction neighbor)
           (or within NEIGHBOR_DELTA of edge)
      2/C: low flow OR high curve
           (or good quality with low contrast/no direction neighbor)
      3/B: good quality with low flow / high curve neighbor
      4/A: good quality (none of the above)

   Generally, the features in A/B quality are useful, the C/D quality
   ones are not.

   Input:
      direction_map    - map with blocks assigned dominant ridge flow direction
      low_contrast_map - map with blocks flagged as low contrast
      low_flow_map     - map with blocks flagged as low ridge flow
      high_curve_map   - map with blocks flagged as high curvature
      map_w            - width (in blocks) of the maps
      map_h            - height (in blocks) of the maps
   Output:
      oqmap      - points to new quality map
   Return Code:
      Zero       - successful completion
      Negative   - system error
************************************************************************/
int gen_quality_map(int **oqmap, int *direction_map, int *low_contrast_map,
                    int *low_flow_map, int *high_curve_map,
                    const int map_w, const int map_h)
{

   int *QualMap;
   int thisX, thisY;
   int compX, compY;
   int arrayPos, arrayPos2;
   int QualOffset;

   QualMap = (int *)malloc(map_w * map_h * sizeof(int));
   if(QualMap == (int *)NULL){
      fprintf(stderr, "ERROR : gen_quality_map : malloc : QualMap\n");
      return(-2);
   }

   /* Foreach row of blocks in maps ... */
   for(thisY=0; thisY<map_h; thisY++){
      /* Foreach block in current row ... */
      for(thisX=0; thisX<map_w; thisX++) {
         /* Compute block index. */
         arrayPos=(thisY*map_w)+thisX;
         /* If current block has low contrast or INVALID direction ... */
         if(low_contrast_map[arrayPos] || direction_map[arrayPos]<0)
            /* Set block's quality to 0/F. */
            QualMap[arrayPos]=0;
         else{
            /* Set baseline quality before looking at neighbors    */
            /*     (will subtract QualOffset below)                */
            /* If current block has low flow or high curvature ... */
            if(low_flow_map[arrayPos] || high_curve_map[arrayPos])
               /* Set block's quality initially to 3/B. */
               QualMap[arrayPos] = 3;  /* offset will be -1..-2 */
            /* Otherwise, block is NOT low flow AND NOT high curvature... */
            else
               /* Set block's quality to 4/A. */
               QualMap[arrayPos]=4;    /* offset will be 0..-2 */

            /* If block within NEIGHBOR_DELTA of edge ... */
            if(thisY < NEIGHBOR_DELTA || thisY > map_h - 1 - NEIGHBOR_DELTA ||
               thisX < NEIGHBOR_DELTA || thisX > map_w - 1 - NEIGHBOR_DELTA)
               /* Set block's quality to 1/E. */
               QualMap[arrayPos]=1;
            /* Otherwise, test neighboring blocks ... */
            else{
               /* Initialize quality adjustment to 0. */
               QualOffset=0;
               /* Foreach row in neighborhood ... */
               for(compY=thisY-NEIGHBOR_DELTA;
                   compY<=thisY+NEIGHBOR_DELTA;compY++){
                  /* Foreach block in neighborhood */
                  /*  (including current block)... */
                  for(compX=thisX-NEIGHBOR_DELTA;
                      compX<=thisX+NEIGHBOR_DELTA;compX++) {
                     /* Compute neighboring block's index. */
                     arrayPos2 = (compY*map_w)+compX;
                    /* If neighbor block (which might be itself) has */
                    /* low contrast or INVALID direction .. */
                    if(low_contrast_map[arrayPos2] ||
                        direction_map[arrayPos2]<0) {
                        /* Set quality adjustment to -2. */
                        QualOffset=-2;
                        /* Done with neighborhood row. */
                        break;
                     }
                     /* Otherwise, if neighbor block (which might be */
                     /* itself) has low flow or high curvature ... */
                     else if(low_flow_map[arrayPos2] ||
                             high_curve_map[arrayPos2]) {
                        /* Set quality to -1 if not already -2. */
                        QualOffset=min(QualOffset,-1);
                     }
                  }
               }
               /* Decrement minutia quality by neighborhood adjustment. */
               QualMap[arrayPos]+=QualOffset;
            }
         }
      }
   }

   /* Set output pointer. */
   *oqmap = QualMap;
   /* Return normally. */
   return(0);
}

/***********************************************************************
************************************************************************
#cat: get_neighborhood_stats - Given a minutia point, computes the mean
#cat:              and stdev of the 8-bit grayscale pixels values in a
#cat:              surrounding neighborhood with specified radius.

   Code originally written by Austin Hicklin for FBI ATU
   Modified by Michael D. Garris (NIST) Sept. 25, 2000

   Input:
      minutia    - structure containing detected minutia
      idata      - 8-bit grayscale fingerprint image
      iw         - width (in pixels) of the image
      ih         - height (in pixels) of the image
      radius_pix - pixel radius of surrounding neighborhood
   Output:
      mean       - mean of neighboring pixels
      stdev      - standard deviation of neighboring pixels
************************************************************************/
static void get_neighborhood_stats(double *mean, double *stdev, MINUTIA *minutia,
                     unsigned char *idata, const int iw, const int ih,
                     const int radius_pix)
{
   int i, x, y, rows, cols;
   int n = 0, sumX = 0, sumXX = 0;
   int histogram[256];

   /* Zero out histogram. */
   memset(histogram, 0, 256 * sizeof(int));

   /* Set minutia's coordinate variables. */
   x = minutia->x;
   y = minutia->y;


   /* If minutiae point is within sampleboxsize distance of image border, */
   /* a value of 0 reliability is returned. */
   if ((x < radius_pix) || (x > iw-radius_pix-1) || 
       (y < radius_pix) || (y > ih-radius_pix-1)) {
      *mean = 0.0;
      *stdev = 0.0;
      return;
      
   }

   /* Foreach row in neighborhood ... */
   for(rows = y - radius_pix;
       rows <= y + radius_pix;
       rows++){
      /* Foreach column in neighborhood ... */
      for(cols = x - radius_pix;
          cols <= x + radius_pix;
          cols++){
         /* Bump neighbor's pixel value bin in histogram. */
         histogram[*(idata+(rows * iw)+cols)]++;
      }
   }

   /* Foreach grayscale pixel bin ... */
   for(i = 0; i < 256; i++){
      if(histogram[i]){
         /* Accumulate Sum(X[i]) */
         sumX += (i * histogram[i]);
         /* Accumulate Sum(X[i]^2) */
         sumXX += (i * i * histogram[i]);
         /* Accumulate N samples */
         n += histogram[i];
      }
   }

   /* Mean = Sum(X[i])/N */
   *mean = sumX/(double)n;
   /* Stdev = sqrt((Sum(X[i]^2)/N) - Mean^2) */
   *stdev = sqrt((sumXX/(double)n) - ((*mean)*(*mean)));
}

/***********************************************************************
************************************************************************
#cat: grayscale_reliability - Given a minutia point, computes a reliability
#cat:              measure from the stdev and mean of its pixel neighborhood.

   Code originally written by Austin Hicklin for FBI ATU
   Modified by Michael D. Garris (NIST) Sept. 25, 2000

   GrayScaleReliability - reasonable reliability heuristic, returns
   0.0 .. 1.0 based on stdev and Mean of a localized histogram where
   "ideal" stdev is >=64; "ideal" Mean is 127.  In a 1 ridge radius
   (11 pixels), if the bytevalue (shade of gray) in the image has a
   stdev of >= 64 & a mean of 127,  returns 1.0 (well defined
   light & dark areas in equal proportions).

   Input:
      minutia    - structure containing detected minutia
      idata      - 8-bit grayscale fingerprint image
      iw         - width (in pixels) of the image
      ih         - height (in pixels) of the image
      radius_pix - pixel radius of surrounding neighborhood
   Return Value:
      reliability - computed reliability measure
************************************************************************/
static double grayscale_reliability(MINUTIA *minutia, unsigned char *idata,
                             const int iw, const int ih, const int radius_pix)
{
   double mean, stdev;
   double reliability;

   get_neighborhood_stats(&mean, &stdev, minutia, idata, iw, ih, radius_pix);

   reliability = min((stdev>IDEALSTDEV ? 1.0 : stdev/(double)IDEALSTDEV),
                         (1.0-(fabs(mean-IDEALMEAN)/(double)IDEALMEAN)));

   return(reliability);
}

/***********************************************************************
************************************************************************
#cat: combined_minutia_quality - Combines quality measures derived from
#cat:              the quality map and neighboring pixel statistics to
#cat:              infer a reliability measure on the scale [0...1].

   Input:
      minutiae    - structure contining the detected minutia
      quality_map - map with blocks assigned 1 of 5 quality levels
      map_w       - width (in blocks) of the map
      map_h       - height (in blocks) of the map
      blocksize   - size (in pixels) of each block in the map
      idata       - 8-bit grayscale fingerprint image
      iw          - width (in pixels) of the image
      ih          - height (in pixels) of the image
      id          - depth (in pixels) of the image
      ppmm        - scan resolution of the image in pixels/mm
   Output:
      minutiae    - updated reliability members
   Return Code:
      Zero       - successful completion
      Negative   - system error
************************************************************************/
int combined_minutia_quality(MINUTIAE *minutiae,
             int *quality_map, const int mw, const int mh, const int blocksize,
             unsigned char *idata, const int iw, const int ih, const int id,
             const double ppmm)
{
   int ret, i, index, radius_pix;
   int *pquality_map, qmap_value;
   MINUTIA *minutia;
   double gs_reliability, reliability;

   /* If image is not 8-bit grayscale ... */
   if(id != 8){
      fprintf(stderr, "ERROR : combined_miutia_quality : ");
      fprintf(stderr, "image must pixel depth = %d must be 8 ", id);
      fprintf(stderr, "to compute reliability\n");
      return(-2);
   }

   /* Compute pixel radius of neighborhood based on image's scan resolution. */
   radius_pix = sround(RADIUS_MM * ppmm);

   /* Expand block map values to pixel map. */
   if((ret = pixelize_map(&pquality_map, iw, ih,
                         quality_map, mw, mh, blocksize))){
      return(ret);
   }

   /* Foreach minutiae detected ... */
   for(i = 0; i < minutiae->num; i++){
      /* Assign minutia pointer. */
      minutia = minutiae->list[i];

      /* Compute reliability from stdev and mean of pixel neighborhood. */
      gs_reliability = grayscale_reliability(minutia,
                                             idata, iw, ih, radius_pix);

      /* Lookup quality map value. */
      /* Compute minutia pixel index. */
      index = (minutia->y * iw) + minutia->x;
      /* Switch on pixel's quality value ... */
      qmap_value = pquality_map[index];

      /* Combine grayscale reliability and quality map value. */
      switch(qmap_value){
         /* Quality A : [50..99]% */
         case 4 :
            reliability = 0.50 + (0.49 * gs_reliability);
            break;
         /* Quality B : [25..49]% */
         case 3 :
            reliability = 0.25 + (0.24 * gs_reliability);
            break;
         /* Quality C : [10..24]% */
         case 2 :
            reliability = 0.10 + (0.14 * gs_reliability);
            break;
         /* Quality D : [5..9]% */
         case 1 :
            reliability = 0.05 + (0.04 * gs_reliability);
            break;
         /* Quality E : 1% */
         case 0 :
            reliability = 0.01;
            break;
         /* Error if quality value not in range [0..4]. */
         default:
            fprintf(stderr, "ERROR : combined_miutia_quality : ");
            fprintf(stderr, "unexpected quality map value %d ", qmap_value);
            fprintf(stderr, "not in range [0..4]\n");
            free(pquality_map);
            return(-3);
      }
      minutia->reliability = reliability;
   }

   /* NEW 05-08-2002 */
   free(pquality_map);

   /* Return normally. */
   return(0);
}


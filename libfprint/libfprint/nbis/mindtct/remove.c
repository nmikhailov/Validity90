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

      FILE:    REMOVE.C
      AUTHOR:  Michael D. Garris
      DATE:    08/02/1999
      UPDATED: 10/04/1999 Version 2 by MDG
      UPDATED: 03/16/2005 by MDG

      Contains routines responsible for detecting and removing false
      minutiae as part of the NIST Latent Fingerprint System (LFS).

***********************************************************************
               ROUTINES:
                        remove_false_minutia_V2()
                        remove_holes()
                        remove_hooks()
                        remove_islands_and_lakes()
                        remove_malformations()
                        remove_near_invblock_V2()
                        remove_pointing_invblock_V2()
                        remove_overlaps()
                        remove_pores_V2()
                        remove_or_adjust_side_minutiae_V2()

***********************************************************************/

#include <stdio.h>
#include <lfs.h>
#include <log.h>

/*************************************************************************
**************************************************************************
#cat: remove_holes - Removes minutia points on small loops around valleys.

   Input:
      minutiae  - list of true and false minutiae
      bdata     - binary image data (0==while & 1==black)
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      minutiae  - list of pruned minutiae
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
static int remove_holes(MINUTIAE *minutiae,
                 unsigned char *bdata, const int iw, const int ih,
                 const LFSPARMS *lfsparms)
{
   int i, ret;
   MINUTIA *minutia;

   print2log("\nREMOVING HOLES:\n");

   i = 0;
   /* Foreach minutia remaining in list ... */
   while(i < minutiae->num){
      /* Assign a temporary pointer. */
      minutia = minutiae->list[i];
      /* If current minutia is a bifurcation ... */
      if(minutia->type == BIFURCATION){
         /* Check to see if it is on a loop of specified length (ex. 15). */
         ret = on_loop(minutia, lfsparms->small_loop_len, bdata, iw, ih);
         /* If minutia is on a loop ... or loop test IGNORED */
         if((ret == LOOP_FOUND) || (ret == IGNORE)){

            print2log("%d,%d RM\n", minutia->x, minutia->y);

            /* Then remove the minutia from list. */
            if((ret = remove_minutia(i, minutiae))){
               /* Return error code. */
               return(ret);
            }
            /* No need to advance because next minutia has "slid" */
            /* into position pointed to by 'i'.                   */
         }
         /* If the minutia is NOT on a loop... */
         else if (ret == FALSE){
            /* Simply advance to next minutia in the list. */
            i++;
         }
         /* Otherwise, an ERROR occurred while looking for loop. */
         else{
            /* Return error code. */
            return(ret);
         }
      }
      /* Otherwise, the current minutia is a ridge-ending... */
      else{
         /* Advance to next minutia in the list. */
         i++;
      }
   }

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: remove_hooks - Takes a list of true and false minutiae and
#cat:                attempts to detect and remove those false minutiae that
#cat:                are on a hook (white or black).

   Input:
      minutiae  - list of true and false minutiae
      bdata     - binary image data (0==while & 1==black)
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      minutiae  - list of pruned minutiae
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
static int remove_hooks(MINUTIAE *minutiae,
                 unsigned char *bdata, const int iw, const int ih,
                 const LFSPARMS *lfsparms)
{
   int *to_remove;
   int i, f, s, ret;
   int delta_y, full_ndirs, qtr_ndirs, deltadir, min_deltadir;
   MINUTIA *minutia1, *minutia2;
   double dist;

   print2log("\nREMOVING HOOKS:\n");

   /* Allocate list of minutia indices that upon completion of testing */
   /* should be removed from the minutiae lists.  Note: That using      */
   /* "calloc" initializes the list to FALSE.                          */
   to_remove = (int *)calloc(minutiae->num, sizeof(int));
   if(to_remove == (int *)NULL){
      fprintf(stderr, "ERROR : remove_hooks : calloc : to_remove\n");
      return(-640);
   }

   /* Compute number directions in full circle. */
   full_ndirs = lfsparms->num_directions<<1;
   /* Compute number of directions in 45=(180/4) degrees. */
   qtr_ndirs = lfsparms->num_directions>>2;

   /* Minimum allowable deltadir to consider joining minutia.               */
   /* (The closer the deltadir is to 180 degrees, the more likely the join. */
   /* When ndirs==16, then this value is 11=(3*4)-1 == 123.75 degrees.      */
   /* I chose to parameterize this threshold based on a fixed fraction of   */
   /* 'ndirs' rather than on passing in a parameter in degrees and doing    */
   /* the conversion.  I doubt the difference matters.                      */
   min_deltadir = (3 * qtr_ndirs) - 1;

   f = 0;
   /* Foreach primary (first) minutia (except for last one in list) ... */
   while(f < minutiae->num-1){

      /* If current first minutia not previously set to be removed. */
      if(!to_remove[f]){

         print2log("\n");

         /* Set first minutia to temporary pointer. */
         minutia1 = minutiae->list[f];
         /* Foreach secondary (second) minutia to right of first minutia ... */
         s = f+1;
         while(s < minutiae->num){
            /* Set second minutia to temporary pointer. */
            minutia2 = minutiae->list[s];

            print2log("1:%d(%d,%d)%d 2:%d(%d,%d)%d ",
                      f, minutia1->x, minutia1->y, minutia1->type,
                      s, minutia2->x, minutia2->y, minutia2->type);

            /* The binary image is potentially being edited during each */
            /* iteration of the secondary minutia loop, therefore       */
            /* minutia pixel values may be changed.  We need to catch   */
            /* these events by using the next 2 tests.                  */

            /* If the first minutia's pixel has been previously changed... */
            if(*(bdata+(minutia1->y*iw)+minutia1->x) != minutia1->type){
               print2log("\n");
               /* Then break out of secondary loop and skip to next first. */
               break;
            }

            /* If the second minutia's pixel has been previously changed... */
            if(*(bdata+(minutia2->y*iw)+minutia2->x) != minutia2->type)
               /* Set to remove second minutia. */
               to_remove[s] = TRUE;

            /* If the second minutia not previously set to be removed. */
            if(!to_remove[s]){

               /* Compute delta y between 1st & 2nd minutiae and test. */
               delta_y = minutia2->y - minutia1->y;
               /* If delta y small enough (ex. < 8 pixels) ... */
               if(delta_y <= lfsparms->max_rmtest_dist){

                  print2log("1DY ");

                  /* Compute Euclidean distance between 1st & 2nd mintuae. */
                  dist = distance(minutia1->x, minutia1->y,
                                  minutia2->x, minutia2->y);
                  /* If distance is NOT too large (ex. < 8 pixels) ... */
                  if(dist <= lfsparms->max_rmtest_dist){

                     print2log("2DS ");

                     /* Compute "inner" difference between directions on */
                     /* a full circle and test.                          */
                     if((deltadir = closest_dir_dist(minutia1->direction,
                                    minutia2->direction, full_ndirs)) ==
                                    INVALID_DIR){
                        free(to_remove);
                        fprintf(stderr,
                                "ERROR : remove_hooks : INVALID direction\n");
                        return(-641);
                     }
                     /* If the difference between dirs is large enough ...  */
                     /* (the more 1st & 2nd point away from each other the  */
                     /* more likely they should be joined)                  */
                     if(deltadir > min_deltadir){

                        print2log("3DD ");

                        /* If 1st & 2nd minutiae are NOT same type ... */
                        if(minutia1->type != minutia2->type){
                           /* Check to see if pair on a hook with contour */
                           /* of specified length (ex. 15 pixels) ...     */

                           ret = on_hook(minutia1, minutia2,
                                         lfsparms->max_hook_len,
                                         bdata, iw, ih);

                           /* If hook detected between pair ... */
                           if(ret == HOOK_FOUND){

                              print2log("4HK RM\n");

                              /* Set to remove first minutia. */
                              to_remove[f] = TRUE;
                              /* Set to remove second minutia. */
                              to_remove[s] = TRUE;
                           }
                           /* If hook test IGNORED ... */
                           else if (ret == IGNORE){

                              print2log("RM\n");

                              /* Set to remove first minutia. */
                              to_remove[f] = TRUE;
                              /* Skip to next 1st minutia by breaking out of */
                              /* inner secondary loop.                       */
                              break;
                           }
                           /* If system error occurred during hook test ... */
                           else if (ret < 0){
                              free(to_remove);
                              return(ret);
                           }
                           /* Otherwise, no hook found, so skip to next */
                           /* second minutia.                           */
                           else
                              print2log("\n");
                        }
                        else
                           print2log("\n");
                        /* End different type test. */
                     }/* End deltadir test. */
                     else
                        print2log("\n");
                  }/* End distance test. */
                  else
                     print2log("\n");
               }
               /* Otherwise, current 2nd too far below 1st, so skip to next */
               /* 1st minutia.                                              */
               else{

                  print2log("\n");

                  /* Break out of inner secondary loop. */
                  break;
               }/* End delta-y test. */

            }/* End if !to_remove[s] */
            else
               print2log("\n");

            /* Bump to next second minutia in minutiae list. */
            s++;
         }/* End secondary minutiae loop. */

      }/* Otherwise, first minutia already flagged to be removed. */

      /* Bump to next first minutia in minutiae list. */
      f++;
   }/* End primary minutiae loop. */

   /* Now remove all minutiae in list that have been flagged for removal. */
   /* NOTE: Need to remove the minutia from their lists in reverse       */
   /*       order, otherwise, indices will be off.                       */
   for(i = minutiae->num-1; i >= 0; i--){
      /* If the current minutia index is flagged for removal ... */
      if(to_remove[i]){
         /* Remove the minutia from the minutiae list. */
         if((ret = remove_minutia(i, minutiae))){
            free(to_remove);
            return(ret);
         }
      }
   }

   /* Deallocate flag list. */
   free(to_remove);

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: remove_islands_and_lakes - Takes a list of true and false minutiae and
#cat:                attempts to detect and remove those false minutiae that
#cat:                are either on a common island (filled with black pixels)
#cat:                or a lake (filled with white pixels).
#cat:                Note that this routine edits the binary image by filling
#cat:                detected lakes or islands.

   Input:
      minutiae  - list of true and false minutiae
      bdata     - binary image data (0==while & 1==black)
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      minutiae  - list of pruned minutiae
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
static int remove_islands_and_lakes(MINUTIAE *minutiae,
                      unsigned char *bdata, const int iw, const int ih,
                      const LFSPARMS *lfsparms)
{
   int *to_remove;
   int i, f, s, ret;
   int delta_y, full_ndirs, qtr_ndirs, deltadir, min_deltadir;
   int *loop_x, *loop_y, *loop_ex, *loop_ey, nloop;
   MINUTIA *minutia1, *minutia2;
   double dist;
   int dist_thresh, half_loop;

   print2log("\nREMOVING ISLANDS AND LAKES:\n");

   dist_thresh = lfsparms->max_rmtest_dist;
   half_loop = lfsparms->max_half_loop;

   /* Allocate list of minutia indices that upon completion of testing */
   /* should be removed from the minutiae lists.  Note: That using      */
   /* "calloc" initializes the list to FALSE.                          */
   to_remove = (int *)calloc(minutiae->num, sizeof(int));
   if(to_remove == (int *)NULL){
      fprintf(stderr,
              "ERROR : remove_islands_and_lakes : calloc : to_remove\n");
      return(-610);
   }

   /* Compute number directions in full circle. */
   full_ndirs = lfsparms->num_directions<<1;
   /* Compute number of directions in 45=(180/4) degrees. */
   qtr_ndirs = lfsparms->num_directions>>2;

   /* Minimum allowable deltadir to consider joining minutia.               */
   /* (The closer the deltadir is to 180 degrees, the more likely the join. */
   /* When ndirs==16, then this value is 11=(3*4)-1 == 123.75 degrees.      */
   /* I chose to parameterize this threshold based on a fixed fraction of   */
   /* 'ndirs' rather than on passing in a parameter in degrees and doing    */
   /* the conversion.  I doubt the difference matters.                      */
   min_deltadir = (3 * qtr_ndirs) - 1;

   /* Foreach primary (first) minutia (except for last one in list) ... */
   f = 0;
   while(f < minutiae->num-1){

      /* If current first minutia not previously set to be removed. */
      if(!to_remove[f]){

         print2log("\n");

         /* Set first minutia to temporary pointer. */
         minutia1 = minutiae->list[f];

         /* Foreach secondary minutia to right of first minutia ... */
         s = f+1;
         while(s < minutiae->num){
            /* Set second minutia to temporary pointer. */
            minutia2 = minutiae->list[s];

            /* If the secondary minutia is desired type ... */
            if(minutia2->type == minutia1->type){

               print2log("1:%d(%d,%d)%d 2:%d(%d,%d)%d ",
                         f, minutia1->x, minutia1->y, minutia1->type,
                         s, minutia2->x, minutia2->y, minutia2->type);

               /* The binary image is potentially being edited during   */
               /* each iteration of the secondary minutia loop,         */
               /* therefore minutia pixel values may be changed.  We    */
               /* need to catch these events by using the next 2 tests. */

               /* If the first minutia's pixel has been previously */
               /* changed...                                       */
               if(*(bdata+(minutia1->y*iw)+minutia1->x) != minutia1->type){
                  print2log("\n");
                  /* Then break out of secondary loop and skip to next */
                  /* first.                                            */
                  break;
               }

               /* If the second minutia's pixel has been previously */
               /* changed...                                        */
               if(*(bdata+(minutia2->y*iw)+minutia2->x) != minutia2->type)
                  /* Set to remove second minutia. */
                  to_remove[s] = TRUE;

               /* If the second minutia not previously set to be removed. */
               if(!to_remove[s]){

                  /* Compute delta y between 1st & 2nd minutiae and test. */
                  delta_y = minutia2->y - minutia1->y;
                  /* If delta y small enough (ex. <16 pixels)... */
                  if(delta_y <= dist_thresh){

                     print2log("1DY ");

                     /* Compute Euclidean distance between 1st & 2nd */
                     /* mintuae.                                     */
                     dist = distance(minutia1->x, minutia1->y,
                                     minutia2->x, minutia2->y);

                     /* If distance is NOT too large (ex. <16 pixels)... */
                     if(dist <= dist_thresh){

                        print2log("2DS ");

                        /* Compute "inner" difference between directions */
                        /* on a full circle and test.                    */
                        if((deltadir = closest_dir_dist(minutia1->direction,
                                       minutia2->direction, full_ndirs)) ==
                                       INVALID_DIR){
                           free(to_remove);
                           fprintf(stderr,
                     "ERROR : remove_islands_and_lakes : INVALID direction\n");
                           return(-611);
                        }
                        /* If the difference between dirs is large      */
                        /* enough ...                                   */
                        /* (the more 1st & 2nd point away from each     */
                        /* other the more likely they should be joined) */
                        if(deltadir > min_deltadir){

                           print2log("3DD ");

                           /* Pair is the same type, so test to see */
                           /* if both are on an island or lake.     */

                           /* Check to see if pair on a loop of specified */
                           /* half length (ex. 30 pixels) ...             */
                           ret = on_island_lake(&loop_x, &loop_y,
                                           &loop_ex, &loop_ey, &nloop,
                                           minutia1, minutia2,
                                           half_loop, bdata, iw, ih);
                           /* If pair is on island/lake ... */
                           if(ret == LOOP_FOUND){

                              print2log("4IL RM\n");

                              /* Fill the loop. */
                              if((ret = fill_loop(loop_x, loop_y, nloop,
                                                 bdata, iw, ih))){
                                 free_contour(loop_x, loop_y,
                                              loop_ex, loop_ey);
                                 free(to_remove);
                                 return(ret);
                              }
                              /* Set to remove first minutia. */
                              to_remove[f] = TRUE;
                              /* Set to remove second minutia. */
                              to_remove[s] = TRUE;
                              /* Deallocate loop contour. */
                              free_contour(loop_x,loop_y,loop_ex,loop_ey);
                           }
                           /* If island/lake test IGNORED ... */
                           else if (ret == IGNORE){

                              print2log("RM\n");

                              /* Set to remove first minutia. */
                              to_remove[f] = TRUE;
                              /* Skip to next 1st minutia by breaking out */
                              /* of inner secondary loop.                 */
                              break;
                           }
                           /* If ERROR while looking for island/lake ... */
                           else if (ret < 0){
                              free(to_remove);
                              return(ret);
                           }
                           else
                              print2log("\n");
                        }/* End deltadir test. */
                        else
                           print2log("\n");
                     }/* End distance test. */
                     else
                        print2log("\n");
                  }
                  /* Otherwise, current 2nd too far below 1st, so skip to */
                  /* next 1st minutia.                                    */
                  else{

                     print2log("\n");

                     /* Break out of inner secondary loop. */
                     break;
                  }/* End delta-y test. */
               }/* End if !to_remove[s] */
               else
                  print2log("\n");

            }/* End if 2nd not desired type */

            /* Bump to next second minutia in minutiae list. */
            s++;
         }/* End secondary minutiae loop. */

      }/* Otherwise, first minutia already flagged to be removed. */

      /* Bump to next first minutia in minutiae list. */
      f++;
   }/* End primary minutiae loop. */

   /* Now remove all minutiae in list that have been flagged for removal. */
   /* NOTE: Need to remove the minutia from their lists in reverse       */
   /*       order, otherwise, indices will be off.                       */
   for(i = minutiae->num-1; i >= 0; i--){
      /* If the current minutia index is flagged for removal ... */
      if(to_remove[i]){
         /* Remove the minutia from the minutiae list. */
         if((ret = remove_minutia(i, minutiae))){
            free(to_remove);
            return(ret);
         }
      }
   }

   /* Deallocate flag list. */
   free(to_remove);

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: remove_malformations - Attempts to detect and remove minutia points
#cat:            that are "irregularly" shaped.  Irregularity is measured
#cat:            by measuring across the interior of the feature at
#cat:            two progressive points down the feature's contour.  The
#cat:            test is triggered if a pixel of opposite color from the
#cat:            feture's type is found.  The ratio of the distances across
#cat:            the feature at the two points is computed and if the ratio
#cat:            is too large then the minutia is determined to be malformed.
#cat:            A cursory test is conducted prior to the general tests in
#cat:            the event that the minutia lies in a block with LOW RIDGE
#cat:            FLOW.  In this case, the distance across the feature at
#cat:            the second progressive contour point is measured and if
#cat:            too large, the point is determined to be malformed.

   Input:
      minutiae  - list of true and false minutiae
      bdata     - binary image data (0==while & 1==black)
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
      low_flow_map   - map of image blocks flagged as LOW RIDGE FLOW
      mw        - width in blocks of the map
      mh        - height in blocks of the map
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      minutiae  - list of pruned minutiae
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
static int remove_malformations(MINUTIAE *minutiae,
                         unsigned char *bdata, const int iw, const int ih,
                         int *low_flow_map, const int mw, const int mh,
                         const LFSPARMS *lfsparms)
{
   int i, j, ret;
   MINUTIA *minutia;
   int *contour_x, *contour_y, *contour_ex, *contour_ey, ncontour;
   int ax1, ay1, bx1, by1;
   int ax2, ay2, bx2, by2;
   int *x_list, *y_list, num;
   double a_dist, b_dist, ratio;
   int fmapval, removed;
   int blk_x, blk_y;

   print2log("\nREMOVING MALFORMATIONS:\n");

   for(i = minutiae->num-1; i >= 0; i--){
      minutia = minutiae->list[i];
      ret = trace_contour(&contour_x, &contour_y,
                          &contour_ex, &contour_ey, &ncontour,
                          lfsparms->malformation_steps_2,
                          minutia->x, minutia->y,
                          minutia->x, minutia->y, minutia->ex, minutia->ey,
                          SCAN_COUNTER_CLOCKWISE, bdata, iw, ih);

      /* If system error occurred during trace ... */
      if(ret < 0){
         /* Return error code. */
         return(ret);
      }
      
      /* If trace was not possible OR loop found OR */
      /* contour is incomplete ...                  */
      if((ret == IGNORE) ||
         (ret == LOOP_FOUND) ||
         (ncontour < lfsparms->malformation_steps_2)){
         /* If contour allocated and returned ... */
         if((ret == LOOP_FOUND) ||
            (ncontour < lfsparms->malformation_steps_2))
            /* Deallocate the contour. */
            free_contour(contour_x, contour_y, contour_ex, contour_ey);

         print2log("%d,%d RMA\n", minutia->x, minutia->y);

         /* Then remove the minutia. */
         if((ret = remove_minutia(i, minutiae)))
            /* If system error, return error code. */
            return(ret);
      }
      /* Otherwise, traced contour is complete. */
      else{
         /* Store 'A1' contour point. */
         ax1 = contour_x[lfsparms->malformation_steps_1-1];
         ay1 = contour_y[lfsparms->malformation_steps_1-1];

         /* Store 'B1' contour point. */
         bx1 = contour_x[lfsparms->malformation_steps_2-1];
         by1 = contour_y[lfsparms->malformation_steps_2-1];

         /* Deallocate the contours. */
         free_contour(contour_x, contour_y, contour_ex, contour_ey);

         ret = trace_contour(&contour_x, &contour_y,
                          &contour_ex, &contour_ey, &ncontour,
                          lfsparms->malformation_steps_2,
                          minutia->x, minutia->y,
                          minutia->x, minutia->y, minutia->ex, minutia->ey,
                          SCAN_CLOCKWISE, bdata, iw, ih);

         /* If system error occurred during trace ... */
         if(ret < 0){
            /* Return error code. */
            return(ret);
         }

         /* If trace was not possible OR loop found OR */
         /* contour is incomplete ...                  */
         if((ret == IGNORE) ||
            (ret == LOOP_FOUND) ||
            (ncontour < lfsparms->malformation_steps_2)){
            /* If contour allocated and returned ... */
            if((ret == LOOP_FOUND) ||
               (ncontour < lfsparms->malformation_steps_2))
               /* Deallocate the contour. */
               free_contour(contour_x, contour_y, contour_ex, contour_ey);

            print2log("%d,%d RMB\n", minutia->x, minutia->y);

            /* Then remove the minutia. */
            if((ret = remove_minutia(i, minutiae)))
               /* If system error, return error code. */
               return(ret);
         }
         /* Otherwise, traced contour is complete. */
         else{
            /* Store 'A2' contour point. */
            ax2 = contour_x[lfsparms->malformation_steps_1-1];
            ay2 = contour_y[lfsparms->malformation_steps_1-1];

            /* Store 'B2' contour point. */
            bx2 = contour_x[lfsparms->malformation_steps_2-1];
            by2 = contour_y[lfsparms->malformation_steps_2-1];

            /* Deallocate the contour. */
            free_contour(contour_x, contour_y, contour_ex, contour_ey);

            /* Compute distances along A & B paths. */
            a_dist = distance(ax1, ay1, ax2, ay2);
            b_dist = distance(bx1, by1, bx2, by2);

            /* Compute block coords from minutia's pixel location. */
            blk_x = minutia->x/lfsparms->blocksize;
            blk_y = minutia->y/lfsparms->blocksize;

            removed = FALSE;

            /* Check to see if distances are not zero. */
            if((a_dist == 0.0) || (b_dist == 0.0)){
               /* Remove the malformation minutia. */
               print2log("%d,%d RMMAL1\n", minutia->x, minutia->y);
               if((ret = remove_minutia(i, minutiae)))
                  /* If system error, return error code. */
                  return(ret);
               removed = TRUE;
            }

            if(!removed){
               /* Determine if minutia is in LOW RIDGE FLOW block. */
               fmapval = *(low_flow_map+(blk_y*mw)+blk_x);
               if(fmapval){
                  /* If in LOW RIDGE LFOW, conduct a cursory distance test. */
                  /* Need to test this out!                                 */
                  if(b_dist > lfsparms->max_malformation_dist){
                     /* Remove the malformation minutia. */
                     print2log("%d,%d RMMAL2\n", minutia->x, minutia->y);
                     if((ret = remove_minutia(i, minutiae)))
                        /* If system error, return error code. */
                        return(ret);
                     removed = TRUE;
                  }
               }
            }

            if(!removed){
               /* Compute points on line between the points A & B. */
               if((ret = line_points(&x_list, &y_list, &num,
                                     bx1, by1, bx2, by2)))
                  return(ret);
               /* Foreach remaining point along line segment ... */
               for(j = 0; j < num; j++){
                  /* If B path contains pixel opposite minutia type ... */
                  if(*(bdata+(y_list[j]*iw)+x_list[j]) != minutia->type){
                     /* Compute ratio of A & B path lengths. */
                     ratio = b_dist / a_dist;
                     /* Need to truncate precision so that answers are  */
                     /* consistent on different computer architectures. */
                     ratio = trunc_dbl_precision(ratio, TRUNC_SCALE);
                     /* If the B path is sufficiently longer than A path ... */
                     if(ratio > lfsparms->min_malformation_ratio){
                        /* Remove the malformation minutia. */
                        /* Then remove the minutia. */
                        print2log("%d,%d RMMAL3 (%f)\n",
                                  minutia->x, minutia->y, ratio);
                        if((ret = remove_minutia(i, minutiae))){
                           free(x_list);
                           free(y_list);
                           /* If system error, return error code. */
                           return(ret);
                        }
                        /* Break out of FOR loop. */
                        break;
                     }
                  }
               }

               free(x_list);
               free(y_list);

            }
         }
      }
   }

   return(0);
}

/*************************************************************************
**************************************************************************
#cat: remove_near_invblocks_V2 - Removes minutia points from the given list
#cat:                that are sufficiently close to a block with invalid
#cat:                ridge flow or to the edge of the image.

   Input:
      minutiae  - list of true and false minutiae
      direction_map - map of image blocks containing direction ridge flow
      mw        - width in blocks of the map
      mh        - height in blocks of the map
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      minutiae  - list of pruned minutiae
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
static int remove_near_invblock_V2(MINUTIAE *minutiae, int *direction_map,
                const int mw, const int mh, const LFSPARMS *lfsparms)
{
   int i, ret;
   int ni, nbx, nby, nvalid;
   int ix, iy, sbi, ebi;
   int bx, by, px, py;
   int removed;
   MINUTIA *minutia;
   int lo_margin, hi_margin;

   /* The next 2 lookup tables are indexed by 'ix' and 'iy'. */
   /* When a feature pixel lies within a 6-pixel margin of a */
   /* block, this routine examines neighboring blocks to     */
   /* determine appropriate actions.                         */
   /*    'ix' may take on values:                            */
   /*         0 == x-pixel coord in leftmost margin          */
   /*         1 == x-pixel coord in middle of block          */
   /*         2 == x-pixel coord in rightmost margin         */
   /*    'iy' may take on values:                            */
   /*         0 == y-pixel coord in topmost margin           */
   /*         1 == y-pixel coord in middle of block          */
   /*         2 == y-pixel coord in bottommost margin        */
   /* Given (ix, iy):                                        */
   /*    'startblk[ix][iy]' == starting neighbor index (sbi) */
   /*    'endblk[ix][iy]'   == ending neighbor index (ebi)   */
   /*    so that neighbors begin to be analized from index   */
   /*    'sbi' to 'ebi'.                                     */
   /* Ex. (ix, iy) = (2, 0)                                  */
   /*    ix==2 ==> x-pixel coord in rightmost margin         */
   /*    iy==0 ==> y-pixel coord in topmost margin           */
   /*    X - marks the region in the current block           */
   /*        corresponding to (ix=2, iy=0).                  */
   /*    sbi = 0 = startblk[2][0]                            */
   /*    ebi = 2 = endblk[2][0]                              */
   /*    so neighbors are analized on index range [0..2]     */
   /*                                |                       */
   /*                 nbr block 0    |  nbr block 1          */
   /*      --------------------------+------------           */
   /*           top margin      | X  |                       */
   /*      _._._._._._._._._._._._._.|                       */
   /*                           |    |                       */
   /*          current block    .r  m|  nbr block 2          */
   /*                           |i  a|                       */
   /*                           .g  g|                       */
   /*                           |h  i|                       */
   /*                           .t  n|                       */
   /*                           |    |                       */

   /* LUT for starting neighbor index given (ix, iy).        */
   static int startblk[9] = { 6, 0, 0,
                              6,-1, 2,
                              4, 4, 2 };
   /* LUT for ending neighbor index given (ix, iy).          */
   static int endblk[9] =   { 8, 0, 2,
                              6,-1, 2,
                              6, 4, 4 };

   /* Pixel coord offsets specifying the order in which neighboring */
   /* blocks are searched.  The current block is in the middle of   */
   /* 8 surrounding neighbors.  The following illustrates the order */
   /* of neighbor indices.  (Note that 9 overlaps 1.)               */
   /*                        8                                      */
   /*                      7 0 1                                    */
   /*                      6 C 2                                    */
   /*                      5 4 3                                    */
   /*                                                               */
   /*                       0  1  2  3  4  5  6  7  8                    */
   static int blkdx[9] = {  0, 1, 1, 1, 0,-1,-1,-1, 0 };  /* Delta-X     */
   static int blkdy[9] = { -1,-1, 0, 1, 1, 1, 0,-1,-1 };  /* Delta-Y     */

   print2log("\nREMOVING MINUTIA NEAR INVALID BLOCKS:\n");

   /* If the margin covers more than the entire block ... */
   if(lfsparms->inv_block_margin > (lfsparms->blocksize>>1)){
      /* Then treat this as an error. */
      fprintf(stderr,
        "ERROR : remove_near_invblock_V2 : margin too large for blocksize\n");
      return(-620);
   }

   /* Compute the low and high pixel margin boundaries (ex. 6 pixels wide) */
   /* in the block.                                                        */
   lo_margin = lfsparms->inv_block_margin;
   hi_margin = lfsparms->blocksize - lfsparms->inv_block_margin - 1;

   i = 0;
   /* Foreach minutia remaining in the list ... */
   while(i < minutiae->num){
      /* Assign temporary minutia pointer. */
      minutia = minutiae->list[i];

      /* Compute block coords from minutia's pixel location. */
      bx = minutia->x/lfsparms->blocksize;
      by = minutia->y/lfsparms->blocksize;

      /* Compute pixel offset into the image block corresponding to the */
      /* minutia's pixel location.                                      */
      /* NOTE: The margins used here will not necessarily correspond to */
      /* the actual block boundaries used to compute the map values.    */
      /* This will be true when the image width and/or height is not an */
      /* even multiple of 'blocksize' and we are processing minutia     */
      /* located in the right-most column (or bottom-most row) of       */
      /* blocks.  I don't think this will pose a problem in practice.   */
      px = minutia->x % lfsparms->blocksize;
      py = minutia->y % lfsparms->blocksize;

      /* Determine if x pixel offset into the block is in the margins. */
      /* If x pixel offset is in left margin ... */
      if(px < lo_margin)
         ix = 0;
      /* If x pixel offset is in right margin ... */
      else if(px > hi_margin)
         ix = 2;
      /* Otherwise, x pixel offset is in middle of block. */
      else
         ix = 1;

      /* Determine if y pixel offset into the block is in the margins. */
      /* If y pixel offset is in top margin ... */
      if(py < lo_margin)
         iy = 0;
      /* If y pixel offset is in bottom margin ... */
      else if(py > hi_margin)
         iy = 2;
      /* Otherwise, y pixel offset is in middle of block. */
      else
         iy = 1;

      /* Set remove flag to FALSE. */
      removed = FALSE;

      /* If one of the minutia's pixel offsets is in a margin ... */
      if((ix != 1) || (iy != 1)){

         /* Compute the starting neighbor block index for processing. */
         sbi = *(startblk+(iy*3)+ix);
         /* Compute the ending neighbor block index for processing. */
         ebi = *(endblk+(iy*3)+ix);

         /* Foreach neighbor in the range to be processed ... */
         for(ni = sbi; ni <= ebi; ni++){
            /* Compute the neighbor's block coords relative to */
            /* the block the current minutia is in.            */
            nbx = bx + blkdx[ni];
            nby = by + blkdy[ni];

            /* If neighbor's block coords are outside of map boundaries... */
            if((nbx < 0) || (nbx >= mw) ||
               (nby < 0) || (nby >= mh)){

               print2log("%d,%d RM1\n", minutia->x, minutia->y);

               /* Then the minutia is in a margin adjacent to the edge of */
               /* the image.                                              */
               /* NOTE: This is true when the image width and/or height   */
               /* is an even multiple of blocksize.  When the image is not*/
               /* an even multiple, then some minutia may not be detected */
               /* as being in the margin of "the image" (not the block).  */
               /* In practice, I don't think this will impact performance.*/
               if((ret = remove_minutia(i, minutiae)))
                  /* If system error occurred while removing minutia, */
                  /* then return error code.                          */
                  return(ret);
               /* Set remove flag to TURE. */
               removed = TRUE;
               /* Break out of neighboring block loop. */
               break;
            }
            /* If the neighboring block has INVALID direction ... */
            else if (*(direction_map+(nby*mw)+nbx) == INVALID_DIR){
               /* Count the number of valid blocks neighboring */
               /* the current neighbor.                        */
               nvalid = num_valid_8nbrs(direction_map, nbx, nby, mw, mh);
               /* If the number of valid neighbors is < threshold */
               /* (ex. 7)...                                      */
               if(nvalid < lfsparms->rm_valid_nbr_min){

                  print2log("%d,%d RM2\n", minutia->x, minutia->y);

                  /* Then remove the current minutia from the list. */
                  if((ret = remove_minutia(i, minutiae)))
                     /* If system error occurred while removing minutia, */
                     /* then return error code.                          */
                     return(ret);
                  /* Set remove flag to TURE. */
                  removed = TRUE;
                  /* Break out of neighboring block loop. */
                  break;
               }
               /* Otherwise enough valid neighbors, so don't remove minutia */
               /* based on this neighboring block.                          */
            }
            /* Otherwise neighboring block has valid direction,         */
            /* so don't remove minutia based on this neighboring block. */
         }

      } /* Otherwise not in margin, so skip to next minutia in list. */

      /* If current minutia not removed ... */
      if(!removed)
         /* Advance to the next minutia in the list. */
         i++;
      /* Otherwise the next minutia has slid into the spot where current */
      /* minutia was removed, so don't bump minutia index.               */
   } /* End minutia loop */

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: remove_pointing_invblock_V2 - Removes minutia points that are relatively
#cat:                close in the direction opposite the minutia to a
#cat:                block with INVALID ridge flow.

   Input:
      minutiae  - list of true and false minutiae
      direction_map - map of image blocks containing directional ridge flow
      mw        - width in blocks of the map
      mh        - height in blocks of the map
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      minutiae  - list of pruned minutiae
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
static int remove_pointing_invblock_V2(MINUTIAE *minutiae,
                             int *direction_map, const int mw, const int mh,
                             const LFSPARMS *lfsparms)
{
   int i, ret;
   int delta_x, delta_y, dmapval;
   int nx, ny, bx, by;
   MINUTIA *minutia;
   double pi_factor, theta;
   double dx, dy;

   print2log("\nREMOVING MINUTIA POINTING TO INVALID BLOCKS:\n");

   /* Compute factor for converting integer directions to radians. */
   pi_factor = M_PI / (double)lfsparms->num_directions;

   i = 0;
   /* Foreach minutia remaining in list ... */
   while(i < minutiae->num){
      /* Set temporary minutia pointer. */
      minutia = minutiae->list[i];
      /* Convert minutia's direction to radians. */
      theta = minutia->direction * pi_factor;
      /* Compute translation offsets (ex. 6 pixels). */
      dx = sin(theta) * (double)(lfsparms->trans_dir_pix);
      dy = cos(theta) * (double)(lfsparms->trans_dir_pix);
      /* Need to truncate precision so that answers are consistent */
      /* on different computer architectures when rounding doubles. */
      dx = trunc_dbl_precision(dx, TRUNC_SCALE);
      dy = trunc_dbl_precision(dy, TRUNC_SCALE);
      delta_x = sround(dx);
      delta_y = sround(dy);
      /* Translate the minutia's coords. */
      nx = minutia->x - delta_x;
      ny = minutia->y + delta_y;
      /* Convert pixel coords to block coords. */
      bx = (int)(nx / lfsparms->blocksize);
      by = (int)(ny / lfsparms->blocksize);
      /* The translation could move the point out of image boundaries,    */
      /* and therefore the corresponding block coords can be out of       */
      /* map boundaries, so limit the block coords to within boundaries.  */
      bx = max(0, bx);
      bx = min(mw-1, bx);
      by = max(0, by);
      by = min(mh-1, by);

      /* Get corresponding block's ridge flow direction. */
      dmapval = *(direction_map+(by*mw)+bx);

      /* If the block's direction is INVALID ... */
      if(dmapval == INVALID_DIR){

         print2log("%d,%d RM\n", minutia->x, minutia->y);

         /* Remove the minutia from the minutiae list. */
         if((ret = remove_minutia(i, minutiae))){
            return(ret);
         }
         /* No need to advance because next minutia has slid into slot. */
      }
      else{
         /* Advance to next minutia in list. */
         i++;
      }
   }

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: remove_overlaps - Takes a list of true and false minutiae and
#cat:                attempts to detect and remove those false minutiae that
#cat:                are on opposite sides of an overlap.  Note that this
#cat:                routine does NOT edit the binary image when overlaps
#cat:                are removed.

   Input:
      minutiae  - list of true and false minutiae
      bdata     - binary image data (0==while & 1==black)
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      minutiae  - list of pruned minutiae
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
static int remove_overlaps(MINUTIAE *minutiae,
                    unsigned char *bdata, const int iw, const int ih,
                    const LFSPARMS *lfsparms)
{
   int *to_remove;
   int i, f, s, ret;
   int delta_y, full_ndirs, qtr_ndirs, deltadir, min_deltadir;
   MINUTIA *minutia1, *minutia2;
   double dist;
   int joindir, opp1dir, half_ndirs;

   print2log("\nREMOVING OVERLAPS:\n");

   /* Allocate list of minutia indices that upon completion of testing */
   /* should be removed from the minutiae lists.  Note: That using      */
   /* "calloc" initializes the list to FALSE.                          */
   to_remove = (int *)calloc(minutiae->num, sizeof(int));
   if(to_remove == (int *)NULL){
      fprintf(stderr, "ERROR : remove_overlaps : calloc : to_remove\n");
      return(-650);
   }

   /* Compute number directions in full circle. */
   full_ndirs = lfsparms->num_directions<<1;
   /* Compute number of directions in 45=(180/4) degrees. */
   qtr_ndirs = lfsparms->num_directions>>2;
   /* Compute number of directions in 90=(180/2) degrees. */
   half_ndirs = lfsparms->num_directions>>1;

   /* Minimum allowable deltadir to consider joining minutia.               */
   /* (The closer the deltadir is to 180 degrees, the more likely the join. */
   /* When ndirs==16, then this value is 11=(3*4)-1 == 123.75 degrees.      */
   /* I chose to parameterize this threshold based on a fixed fraction of   */
   /* 'ndirs' rather than on passing in a parameter in degrees and doing    */
   /* the conversion.  I doubt the difference matters.                      */
   min_deltadir = (3 * qtr_ndirs) - 1;

   f = 0;
   /* Foreach primary (first) minutia (except for last one in list) ... */
   while(f < minutiae->num-1){

      /* If current first minutia not previously set to be removed. */
      if(!to_remove[f]){

         print2log("\n");

         /* Set first minutia to temporary pointer. */
         minutia1 = minutiae->list[f];
         /* Foreach secondary (second) minutia to right of first minutia ... */
         s = f+1;
         while(s < minutiae->num){
            /* Set second minutia to temporary pointer. */
            minutia2 = minutiae->list[s];

            print2log("1:%d(%d,%d)%d 2:%d(%d,%d)%d ",
                      f, minutia1->x, minutia1->y, minutia1->type,
                      s, minutia2->x, minutia2->y, minutia2->type);

            /* The binary image is potentially being edited during each */
            /* iteration of the secondary minutia loop, therefore       */
            /* minutia pixel values may be changed.  We need to catch   */
            /* these events by using the next 2 tests.                  */

            /* If the first minutia's pixel has been previously changed... */
            if(*(bdata+(minutia1->y*iw)+minutia1->x) != minutia1->type){
               print2log("\n");
               /* Then break out of secondary loop and skip to next first. */
               break;
            }

            /* If the second minutia's pixel has been previously changed... */
            if(*(bdata+(minutia2->y*iw)+minutia2->x) != minutia2->type)
               /* Set to remove second minutia. */
               to_remove[s] = TRUE;

            /* If the second minutia not previously set to be removed. */
            if(!to_remove[s]){

               /* Compute delta y between 1st & 2nd minutiae and test. */
               delta_y = minutia2->y - minutia1->y;
               /* If delta y small enough (ex. < 8 pixels) ... */
               if(delta_y <= lfsparms->max_overlap_dist){

                  print2log("1DY ");

                  /* Compute Euclidean distance between 1st & 2nd mintuae. */
                  dist = distance(minutia1->x, minutia1->y,
                                  minutia2->x, minutia2->y);
                  /* If distance is NOT too large (ex. < 8 pixels) ... */
                  if(dist <= lfsparms->max_overlap_dist){

                     print2log("2DS ");

                     /* Compute "inner" difference between directions on */
                     /* a full circle and test.                          */
                     if((deltadir = closest_dir_dist(minutia1->direction,
                                    minutia2->direction, full_ndirs)) ==
                                    INVALID_DIR){
                        free(to_remove);
                        fprintf(stderr,
                           "ERROR : remove_overlaps : INVALID direction\n");
                        return(-651);
                     }
                     /* If the difference between dirs is large enough ...  */
                     /* (the more 1st & 2nd point away from each other the  */
                     /* more likely they should be joined)                  */
                     if(deltadir > min_deltadir){

                        print2log("3DD ");

                        /* If 1st & 2nd minutiae are same type ... */
                        if(minutia1->type == minutia2->type){
                           /* Test to see if both are on opposite sides */
                           /* of an overlap.                            */

                           /* Compute direction of "joining" vector.      */
                           /* First, compute direction of line from first */
                           /* to second minutia points.                   */
                           joindir = line2direction(minutia1->x, minutia1->y,
                                                    minutia2->x, minutia2->y,
                                                    lfsparms->num_directions);

                           /* Comptue opposite direction of first minutia. */
                           opp1dir = (minutia1->direction+
                                      lfsparms->num_directions)%full_ndirs;
                           /* Take "inner" distance on full circle between */
                           /* the first minutia's opposite direction and   */
                           /* the joining direction.                       */
                           joindir = abs(opp1dir - joindir);
                           joindir = min(joindir, full_ndirs - joindir);

                           print2log("joindir=%d dist=%f ", joindir,dist);

                           /* If the joining angle is <= 90 degrees OR   */
                           /*    the 2 points are sufficiently close AND */
                           /*    a free path exists between pair ...     */
                           if(((joindir <= half_ndirs) ||
                               (dist <= lfsparms->max_overlap_join_dist)) &&
                               free_path(minutia1->x, minutia1->y,
                                         minutia2->x, minutia2->y,
                                         bdata, iw, ih, lfsparms)){

                              print2log("4OV RM\n");

                              /* Then assume overlap, so ...             */
                              /* Set to remove first minutia. */
                              to_remove[f] = TRUE;
                              /* Set to remove second minutia. */
                              to_remove[s] = TRUE;
                           }
                           /* Otherwise, pair not on an overlap, so skip */
                           /* to next second minutia.                    */
                           else
                              print2log("\n");
                        }
                        else
                           print2log("\n");
                        /* End same type test. */
                     }/* End deltadir test. */
                     else
                        print2log("\n");
                  }/* End distance test. */
                  else
                     print2log("\n");
               }
               /* Otherwise, current 2nd too far below 1st, so skip to next */
               /* 1st minutia.                                              */
               else{

                  print2log("\n");

                  /* Break out of inner secondary loop. */
                  break;
               }/* End delta-y test. */

            }/* End if !to_remove[s] */
            else
               print2log("\n");

            /* Bump to next second minutia in minutiae list. */
            s++;
         }/* End secondary minutiae loop. */

      }/* Otherwise, first minutia already flagged to be removed. */

      /* Bump to next first minutia in minutiae list. */
      f++;
   }/* End primary minutiae loop. */

   /* Now remove all minutiae in list that have been flagged for removal. */
   /* NOTE: Need to remove the minutia from their lists in reverse       */
   /*       order, otherwise, indices will be off.                       */
   for(i = minutiae->num-1; i >= 0; i--){
      /* If the current minutia index is flagged for removal ... */
      if(to_remove[i]){
         /* Remove the minutia from the minutiae list. */
         if((ret = remove_minutia(i, minutiae))){
            free(to_remove);
            return(ret);
         }
      }
   }

   /* Deallocate flag list. */
   free(to_remove);

   /* Return normally. */
   return(0);
}

static void mark_minutiae_in_range(MINUTIAE *minutiae, int *to_remove, int x, int y,
                                   const LFSPARMS *lfsparms)
{
    int i, dist;
    for (i = 0; i < minutiae->num; i++) {
        if (to_remove[i])
            continue;
        dist = (int)sqrt((x - minutiae->list[i]->x) * (x - minutiae->list[i]->x) +
                         (y - minutiae->list[i]->y) * (y - minutiae->list[i]->y));
        if (dist < lfsparms->min_pp_distance) {
            to_remove[i] = 1;
        }
    }
}

/*************************************************************************
**************************************************************************
#cat: remove_perimeter_pts - Takes a list of true and false minutiae and
#cat:                attempts to detect and remove those false minutiae that
#cat:                belong to image edge

   Input:
      minutiae  - list of true and false minutiae
      bdata     - binary image data (0==while & 1==black)
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      minutiae  - list of pruned minutiae
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
static int remove_perimeter_pts(MINUTIAE *minutiae,
                       unsigned char *bdata, const int iw, const int ih,
                       const LFSPARMS *lfsparms)
{
    int i, j, ret, *to_remove;
    int *left, *left_up, *left_down;
    int *right, *right_up, *right_down;
    int removed = 0;
    int left_min, right_max;

    if (!lfsparms->remove_perimeter_pts)
        return(0);

    to_remove = calloc(minutiae->num, sizeof(int));
    left = calloc(ih, sizeof(int));
    left_up = calloc(ih, sizeof(int));
    left_down = calloc(ih, sizeof(int));
    right = calloc(ih, sizeof(int));
    right_up = calloc(ih, sizeof(int));
    right_down = calloc(ih, sizeof(int));

    /* Pass downwards */
    left_min = iw - 1;
    right_max = 0;
    for (i = 0; i < ih; i++) {
        for (j = 0; j < left_min; j++) {
            if ((bdata[i * iw + j] != 0)) {
                left_min = j;
                break;
            }
        }
        if (left_min == (iw - 1))
            left_down[i] = -1;
        else
            left_down[i] = left_min;
        for (j = iw - 1; j >= right_max; j--) {
            if ((bdata[i * iw + j] != 0)) {
                right_max = j;
                break;
            }
        }
        if (right_max == 0)
            right_down[i] = -1;
        else
            right_down[i] = right_max;
    }

    /* Pass upwards */
    left_min = iw - 1;
    right_max = 0;
    for (i = ih - 1; i >= 0; i--) {
        for (j = 0; j < left_min; j++) {
            if ((bdata[i * iw + j] != 0)) {
                left_min = j;
                break;
            }
        }
        if (left_min == (iw - 1))
            left_up[i] = -1;
        else
            left_up[i] = left_min;
        for (j = iw - 1; j >= right_max; j--) {
            if ((bdata[i * iw + j] != 0)) {
                right_max = j;
                break;
            }
        }
        if (right_max == 0)
            right_up[i] = -1;
        else
            right_up[i] = right_max;
    }

    /* Merge */
    left_min = left_down[ih - 1];
    right_max = right_down[ih - 1];
    for (i = 0; i < ih; i++) {
        if (left_down[i] != left_min)
            left[i] = left_down[i];
        else
            left[i] = left_up[i];

        if (right_down[i] != right_max)
            right[i] = right_down[i];
        else
            right[i] = right_up[i];
    }
    free(left_up);
    free(left_down);
    free(right_up);
    free(right_down);

    /* Mark minitiae close to the edge */
    for (i = 0; i < ih; i++) {
        if (left[i] != -1)
            mark_minutiae_in_range(minutiae, to_remove, left[i], i, lfsparms);
        if (right[i] != -1)
            mark_minutiae_in_range(minutiae, to_remove, right[i], i, lfsparms);
    }

    free(left);
    free(right);

    for (i = minutiae->num - 1; i >= 0; i--) {
        /* If the current minutia index is flagged for removal ... */
        if (to_remove[i]){
            removed ++;
            /* Remove the minutia from the minutiae list. */
            if((ret = remove_minutia(i, minutiae))){
                free(to_remove);
                return(ret);
            }
        }
    }

    free(to_remove);

    return (0);
}

/*************************************************************************
**************************************************************************
#cat: remove_pores_V2 - Attempts to detect and remove minutia points located on
#cat:                   pore-shaped valleys and/or ridges.  Detection for
#cat:                   these features are only performed in blocks with
#cat:                   LOW RIDGE FLOW or HIGH CURVATURE.

   Input:
      minutiae  - list of true and false minutiae
      bdata     - binary image data (0==while & 1==black)
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
      direction_map  - map of image blocks containing directional ridge flow
      low_flow_map   - map of image blocks flagged as LOW RIDGE FLOW
      high_curve_map - map of image blocks flagged as HIGH CURVATURE
      mw        - width in blocks of the maps
      mh        - height in blocks of the maps
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      minutiae  - list of pruned minutiae
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
static int remove_pores_V2(MINUTIAE *minutiae,
                    unsigned char *bdata, const int iw, const int ih,
                    int *direction_map, int *low_flow_map,
                    int *high_curve_map, const int mw, const int mh,
                    const LFSPARMS *lfsparms)
{
   int i, ret;
   int removed, blk_x, blk_y;
   int rx, ry;
   int px, py, pex, pey, bx, by, dx, dy;
   int qx, qy, qex, qey, ax, ay, cx, cy;
   MINUTIA *minutia;
   double pi_factor, theta, sin_theta, cos_theta;
   double ab2, cd2, ratio;
   int *contour_x, *contour_y, *contour_ex, *contour_ey, ncontour;
   double drx, dry;

   /*      This routine attempts to locate the following points on all */
   /*      minutia within the feature list.                            */
   /*      1. Compute R 3 pixels opposite the feature direction from   */
   /*         feature point F.                                         */
   /*      2. Find white pixel transitions P & Q within 12 steps from  */
   /*         from R perpendicular to the feature's direction.         */
   /*      3. Find points B & D by walking white edge from P.          */
   /*      4. Find points A & C by walking white edge from Q.          */
   /*      5. Measure squared distances between A-B and C-D.           */
   /*      6. Compute ratio of squared distances and compare against   */
   /*         threshold (2.25).  If A-B sufficiently larger than C-D,  */
   /*         then assume NOT pore, otherwise flag the feature point F.*/
   /*      If along the way, finding any of these points fails, then   */
   /*      assume the feature is a pore and flag it.                   */
   /*                                                                  */
   /*                      A                                           */
   /*                _____._                                           */
   /*                       ----___     Q      C                       */
   /*                ------____    ---_.________.___                   */
   /*                          ---_                                    */
   /*                (valley)    F.\   .R  (ridge)                     */
   /*                          ____/                                   */
   /*                ______----    ___-.--------.---                   */
   /*                       ____---     P      D                       */
   /*                -----.-                                           */
   /*                      B                                           */
   /*                                                                  */
   /*              AB^2/CD^2 <= 2.25  then flag feature                */
   /*                                                                  */


   print2log("\nREMOVING PORES:\n");

   /* Factor for converting integer directions into radians. */
   pi_factor = M_PI/(double)lfsparms->num_directions;

   /* Initialize to the beginning of the minutia list. */
   i = 0;
   /* Foreach minutia remaining in the list ... */
   while(i < minutiae->num){
      /* Set temporary minutia pointer. */
      minutia = minutiae->list[i];

      /* Initialize remove flag to FALSE. */
      removed = FALSE;

      /* Compute block coords from minutia point. */
      blk_x = minutia->x / lfsparms->blocksize;
      blk_y = minutia->y / lfsparms->blocksize;

      /* If minutia in LOW RIDGE FLOW or HIGH CURVATURE block */
      /* with a valid direction ...                           */
      if((*(low_flow_map+(blk_y*mw)+blk_x) ||
          *(high_curve_map+(blk_y*mw)+blk_x)) &&
         (*(direction_map+(blk_y*mw)+blk_x) >= 0)){
         /* Compute radian angle from minutia direction. */
         theta = (double)minutia->direction * pi_factor;
         /* Compute sine and cosine factors of this angle. */
         sin_theta = sin(theta);
         cos_theta = cos(theta);
         /* Translate the minutia point (ex. 3 pixels) in opposite */
         /* direction minutia is pointing.  Call this point 'R'.   */
         drx = (double)minutia->x -
                     (sin_theta * (double)lfsparms->pores_trans_r);
         dry = (double)minutia->y +
                     (cos_theta * (double)lfsparms->pores_trans_r);
         /* Need to truncate precision so that answers are consistent */
         /* on different computer architectures when rounding doubles. */
         drx = trunc_dbl_precision(drx, TRUNC_SCALE);
         dry = trunc_dbl_precision(dry, TRUNC_SCALE);
         rx = sround(drx);
         ry = sround(dry);

         /* If 'R' is opposite color from minutia type ... */
         if(*(bdata+(ry*iw)+rx) != minutia->type){

            /* Search a specified number of steps (ex. 12) from 'R' in a */
            /* perpendicular direction from the minutia direction until  */
            /* the first white pixel is found.  If a white pixel is      */
            /* found within the specified number of steps, then call     */
            /* this point 'P' (storing the point's edge pixel as well).  */
            if(search_in_direction(&px, &py, &pex, &pey,
                                   minutia->type,
                                   rx, ry, -cos_theta, -sin_theta,
                                   lfsparms->pores_perp_steps,
                                   bdata, iw, ih)){
               /* Trace contour from P's edge pixel in counter-clockwise  */
               /* scan and step along specified number of steps (ex. 10). */
               ret = trace_contour(&contour_x, &contour_y,
                                   &contour_ex, &contour_ey, &ncontour,
                                   lfsparms->pores_steps_fwd,
                                   px, py, px, py, pex, pey,
                                   SCAN_COUNTER_CLOCKWISE, bdata, iw, ih);

               /* If system error occurred during trace ... */
               if(ret < 0){
                  /* Return error code. */
                  return(ret);
               }

               /* If trace was not possible OR loop found OR */
               /* contour is incomplete ...                  */
               if((ret == IGNORE) ||
                  (ret == LOOP_FOUND) ||
                  (ncontour < lfsparms->pores_steps_fwd)){
                  /* If contour allocated and returned ... */
                  if((ret == LOOP_FOUND) ||
                     (ncontour < lfsparms->pores_steps_fwd))
                     /* Deallocate the contour. */
                     free_contour(contour_x, contour_y,
                                  contour_ex, contour_ey);

                  print2log("%d,%d RMB\n", minutia->x, minutia->y);

                  /* Then remove the minutia. */
                  if((ret = remove_minutia(i, minutiae)))
                     /* If system error, return error code. */
                     return(ret);
                  /* Set remove flag to TRUE. */
                  removed = TRUE;
               }
               /* Otherwise, traced contour is complete. */
               else{
                  /* Store last point in contour as point 'B'. */
                  bx = contour_x[ncontour-1];
                  by = contour_y[ncontour-1];
                  /* Deallocate the contour. */
                  free_contour(contour_x, contour_y,
                               contour_ex, contour_ey);

                  /* Trace contour from P's edge pixel in clockwise scan */
                  /* and step along specified number of steps (ex. 8).   */
                  ret = trace_contour(&contour_x, &contour_y,
                                      &contour_ex, &contour_ey, &ncontour,
                                      lfsparms->pores_steps_bwd,
                                      px, py, px, py, pex, pey,
                                      SCAN_CLOCKWISE, bdata, iw, ih);

                  /* If system error occurred during trace ... */
                  if(ret < 0){
                     /* Return error code. */
                     return(ret);
                  }

                  /* If trace was not possible OR loop found OR */
                  /* contour is incomplete ...                  */
                  if((ret == IGNORE) ||
                     (ret == LOOP_FOUND) ||
                     (ncontour < lfsparms->pores_steps_bwd)){
                     /* If contour allocated and returned ... */
                     if((ret == LOOP_FOUND) ||
                        (ncontour < lfsparms->pores_steps_bwd))
                        /* Deallocate the contour. */
                        free_contour(contour_x, contour_y,
                                     contour_ex, contour_ey);

                     print2log("%d,%d RMD\n", minutia->x, minutia->y);

                     /* Then remove the minutia. */
                     if((ret = remove_minutia(i, minutiae)))
                        /* If system error, return error code. */
                        return(ret);
                     /* Set remove flag to TRUE. */
                     removed = TRUE;
                  }
                  /* Otherwise, traced contour is complete. */
                  else{
                     /* Store last point in contour as point 'D'. */
                     dx = contour_x[ncontour-1];
                     dy = contour_y[ncontour-1];
                     /* Deallocate the contour. */
                     free_contour(contour_x, contour_y,
                                  contour_ex, contour_ey);
                     /* Search a specified number of steps (ex. 12) from */
                     /* 'R' in opposite direction of that used to find   */
                     /* 'P' until the first white pixel is found.  If a  */
                     /* white pixel is found within the specified number */
                     /* of steps, then call this point 'Q' (storing the  */
                     /* point's edge pixel as well).                     */
                     if(search_in_direction(&qx, &qy, &qex, &qey,
                                            minutia->type,
                                            rx, ry, cos_theta, sin_theta,
                                            lfsparms->pores_perp_steps,
                                            bdata, iw, ih)){
                        /* Trace contour from Q's edge pixel in clockwise */
                        /* scan and step along specified number of steps  */
                        /* (ex. 10).                                      */
                        ret = trace_contour(&contour_x, &contour_y,
                                    &contour_ex, &contour_ey, &ncontour,
                                    lfsparms->pores_steps_fwd,
                                    qx, qy, qx, qy, qex, qey,
                                    SCAN_CLOCKWISE, bdata, iw, ih);

                        /* If system error occurred during trace ... */
                        if(ret < 0){
                           /* Return error code. */
                           return(ret);
                        }

                        /* If trace was not possible OR loop found OR */
                        /* contour is incomplete ...                  */
                        if((ret == IGNORE) ||
                           (ret == LOOP_FOUND) ||
                           (ncontour < lfsparms->pores_steps_fwd)){
                           /* If contour allocated and returned ... */
                           if((ret == LOOP_FOUND) ||
                              (ncontour < lfsparms->pores_steps_fwd))
                              /* Deallocate the contour. */
                              free_contour(contour_x, contour_y,
                                           contour_ex, contour_ey);

                           print2log("%d,%d RMA\n", minutia->x, minutia->y);

                           /* Then remove the minutia. */
                           if((ret = remove_minutia(i, minutiae)))
                              /* If system error, return error code. */
                              return(ret);
                           /* Set remove flag to TRUE. */
                           removed = TRUE;
                        }
                        /* Otherwise, traced contour is complete. */
                        else{
                           /* Store last point in contour as point 'A'. */
                           ax = contour_x[ncontour-1];
                           ay = contour_y[ncontour-1];
                           /* Deallocate the contour. */
                           free_contour(contour_x, contour_y,
                                        contour_ex, contour_ey);

                           /* Trace contour from Q's edge pixel in    */
                           /* counter-clockwise scan and step along a */
                           /* specified number of steps (ex. 8).      */
                           ret = trace_contour(&contour_x, &contour_y,
                                    &contour_ex, &contour_ey, &ncontour, 
                                    lfsparms->pores_steps_bwd,
                                    qx, qy, qx, qy, qex, qey,
                                    SCAN_COUNTER_CLOCKWISE, bdata, iw, ih);

                           /* If system error occurred during scan ... */
                           if(ret < 0){
                              /* Return error code. */
                              return(ret);
                           }

                           /* If trace was not possible OR loop found OR */
                           /* contour is incomplete ...                  */
                           if((ret == IGNORE) ||
                              (ret == LOOP_FOUND) ||
                              (ncontour < lfsparms->pores_steps_bwd)){
                              /* If contour allocated and returned ... */
                              if((ret == LOOP_FOUND) ||
                                 (ncontour < lfsparms->pores_steps_bwd))
                                 /* Deallocate the contour. */
                                 free_contour(contour_x, contour_y,
                                              contour_ex, contour_ey);

                              print2log("%d,%d RMC\n",
                                        minutia->x, minutia->y);

                              /* Then remove the minutia. */
                              if((ret = remove_minutia(i, minutiae)))
                                 /* If system error, return error code. */
                                 return(ret);
                              /* Set remove flag to TRUE. */
                              removed = TRUE;
                           }
                           /* Otherwise, traced contour is complete. */
                           else{
                              /* Store last point in contour as 'C'. */
                              cx = contour_x[ncontour-1];
                              cy = contour_y[ncontour-1];
                              /* Deallocate the contour. */
                              free_contour(contour_x, contour_y,
                                           contour_ex, contour_ey);

                              /* Compute squared distance between points */
                              /* 'A' and 'B'.                            */
                              ab2 = squared_distance(ax, ay, bx, by);
                              /* Compute squared distance between points */
                              /* 'C' and 'D'.                            */
                              cd2 = squared_distance(cx, cy, dx, dy);
                              /* If CD distance is not near zero */
                              /* (ex. 0.5) ...                   */
                              if(cd2 > lfsparms->pores_min_dist2){
                                 /* Compute ratio of squared distances. */
                                 ratio = ab2 / cd2;

                                 /* If ratio is small enough (ex. 2.25)...*/
                                 if(ratio <= lfsparms->pores_max_ratio){

                                    print2log("%d,%d ",
                                              minutia->x, minutia->y);
      print2log("R=%d,%d P=%d,%d B=%d,%d D=%d,%d Q=%d,%d A=%d,%d C=%d,%d ",
              rx, ry, px, py, bx, by, dx, dy, qx, qy, ax, ay, cx, cy);
                                    print2log("RMRATIO %f\n", ratio);

                                    /* Then assume pore & remove minutia. */
                                    if((ret = remove_minutia(i, minutiae)))
                                       /* If system error, return code. */
                                       return(ret);
                                    /* Set remove flag to TRUE. */
                                    removed = TRUE;
                                 }
                                 /* Otherwise, ratio to big, so assume */
                                 /* legitimate minutia.                */
                              } /* Else, cd2 too small. */
                           } /* Done with C. */
                        } /* Done with A. */
                     }
                     /* Otherwise, Q not found ... */
                     else{

                        print2log("%d,%d RMQ\n", minutia->x, minutia->y);

                        /* Then remove the minutia. */
                        if((ret = remove_minutia(i, minutiae)))
                           /* If system error, return error code. */
                           return(ret);
                        /* Set remove flag to TRUE. */
                        removed = TRUE;
                     } /* Done with Q. */
                  } /* Done with D. */
               } /* Done with B. */
            }
            /* Otherwise, P not found ... */
            else{

               print2log("%d,%d RMP\n", minutia->x, minutia->y);

               /* Then remove the minutia. */
               if((ret = remove_minutia(i, minutiae)))
                  /* If system error, return error code. */
                  return(ret);
               /* Set remove flag to TRUE. */
               removed = TRUE;
            }
         } /* Else, R is on pixel the same color as type, so do not */
           /* remove minutia point and skip to next one.            */
      } /* Else block is unreliable or has INVALID direction. */

      /* If current minutia not removed ... */
      if(!removed)
         /* Bump to next minutia in list. */
         i++;
      /* Otherwise, next minutia has slid into slot of current removed one. */

   } /* End While minutia remaining in list. */

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: remove_or_adjust_side_minutiae_V2 - Removes loops or minutia points that
#cat:              are not on complete contours of specified length. If the
#cat:              contour is complete, then the minutia is adjusted based
#cat:              on a minmax analysis of the rotated y-coords of the contour.

   Input:
      minutiae  - list of true and false minutiae
      bdata     - binary image data (0==while & 1==black)
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
      direction_map - map of image blocks containing directional ridge flow
      mw        - width (in blocks) of the map
      mh        - height (in blocks) of the map
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      minutiae  - list of pruned minutiae
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
static int remove_or_adjust_side_minutiae_V2(MINUTIAE *minutiae,
                 unsigned char *bdata, const int iw, const int ih,
                 int *direction_map, const int mw, const int mh,
                 const LFSPARMS *lfsparms)
{
   int i, j, ret;
   MINUTIA *minutia;
   double pi_factor, theta, sin_theta, cos_theta;
   int *contour_x, *contour_y, *contour_ex, *contour_ey, ncontour;
   int *rot_y, minloc;
   int *minmax_val, *minmax_i, *minmax_type, minmax_alloc, minmax_num;
   double drot_y;
   int bx, by;

   print2log("\nADJUSTING SIDE MINUTIA:\n");

   /* Allocate working memory for holding rotated y-coord of a */
   /* minutia's contour.                                       */
   rot_y = (int *)malloc(((lfsparms->side_half_contour<<1)+1) * sizeof(int));
   if(rot_y == (int *)NULL){
      fprintf(stderr,
              "ERROR : remove_or_adjust_side_minutiae_V2 : malloc : rot_y\n");
      return(-630);
   }

   /* Compute factor for converting integer directions to radians. */
   pi_factor = M_PI / (double)lfsparms->num_directions;

   i = 0;
   /* Foreach minutia remaining in list ... */
   while(i < minutiae->num){
      /* Assign a temporary pointer. */
      minutia = minutiae->list[i];

      /* Extract a contour centered on the minutia point (ex. 7 pixels */
      /* in both directions).                                          */
      ret = get_centered_contour(&contour_x, &contour_y,
                         &contour_ex, &contour_ey, &ncontour,
                         lfsparms->side_half_contour,
                         minutia->x, minutia->y, minutia->ex, minutia->ey,
                         bdata, iw, ih);

      /* If system error occurred ... */
      if(ret < 0){
         /* Deallocate working memory. */
         free(rot_y);
         /* Return error code. */
         return(ret);
      }

      /* If we didn't succeed in extracting a complete contour for any */
      /* other reason ...                                              */
      if((ret == LOOP_FOUND) ||
         (ret == IGNORE) ||
         (ret == INCOMPLETE)){

         print2log("%d,%d RM1\n", minutia->x, minutia->y);

         /* Remove minutia from list. */
         if((ret = remove_minutia(i, minutiae))){
            /* Deallocate working memory. */
            free(rot_y);
            /* Return error code. */
            return(ret);
         }
         /* No need to advance because next minutia has "slid" */
         /* into position pointed to by 'i'.                   */
      }
      /* Otherwise, a complete contour was found and extracted ... */
      else{
         /* Rotate contour points by negative angle of feature's direction. */
         /* The contour of a well-formed minutia point will form a bowl     */
         /* shape concaved in the direction of the minutia.  By rotating    */
         /* the contour points by the negative angle of feature's direction */
         /* the bowl will be transformed to be concaved upwards and minima  */
         /* and maxima of the transformed y-coords can be analyzed to       */
         /* determine if the minutia is "well-formed" or not.  If well-     */
         /* formed then the position of the minutia point is adjusted.  If  */
         /* not well-formed, then the minutia point is removed altogether.  */

         /* Normal rotation of T degrees around the origin of */
         /*      the point (x,y):                             */
         /*         rx = x*cos(T) - y*sin(T)                  */
         /*         ry = x*cos(T) + y*sin(T)                  */
         /*      The rotation here is for -T degrees:         */
         /*         rx = x*cos(-T) - y*sin(-T)                */
         /*         ry = x*cos(-T) + y*sin(-T)                */
         /*      which can be written:                        */
         /*         rx = x*cos(T) + y*sin(T)                  */
         /*         ry = x*sin(T) - y*cos(T)                  */

         /* Convert minutia's direction to radians. */
         theta = (double)minutia->direction * pi_factor;
         /* Compute sine and cosine values at theta for rotation. */
         sin_theta = sin(theta);
         cos_theta = cos(theta);

         for(j = 0; j < ncontour; j++){
             /* We only need to rotate the y-coord (don't worry     */
             /* about rotating the x-coord or contour edge pixels). */
             drot_y = ((double)contour_x[j] * sin_theta) -
                               ((double)contour_y[j] * cos_theta);
             /* Need to truncate precision so that answers are consistent */
             /* on different computer architectures when rounding doubles. */
             drot_y = trunc_dbl_precision(drot_y, TRUNC_SCALE);
             rot_y[j] = sround(drot_y);
         }

         /* Locate relative minima and maxima in vector of rotated */
         /* y-coords of current minutia's contour.                 */
         if((ret = minmaxs(&minmax_val, &minmax_type, &minmax_i,
                          &minmax_alloc, &minmax_num,
                          rot_y, ncontour))){
            /* If system error, then deallocate working memories. */
            free(rot_y);
            free_contour(contour_x, contour_y, contour_ex, contour_ey);
            /* Return error code. */
            return(ret);
         }

         /* If one and only one minima was found in rotated y-coord */
         /* of contour ...                                          */
         if((minmax_num == 1) &&
            (minmax_type[0] == -1)){

            print2log("%d,%d ", minutia->x, minutia->y);

            /* Reset loation of minutia point to contour point at minima. */
            minutia->x = contour_x[minmax_i[0]];
            minutia->y = contour_y[minmax_i[0]];
            minutia->ex = contour_ex[minmax_i[0]];
            minutia->ey = contour_ey[minmax_i[0]];

            /* Must check if adjusted minutia is now in INVALID block ... */
            bx = minutia->x/lfsparms->blocksize;
            by = minutia->y/lfsparms->blocksize;
            if(*(direction_map+(by*mw)+bx) == INVALID_DIR){
               /* Remove minutia from list. */
               if((ret = remove_minutia(i, minutiae))){
                  /* Deallocate working memory. */
                  free(rot_y);
                  free_contour(contour_x, contour_y, contour_ex, contour_ey);
                  if(minmax_alloc > 0){
                     free(minmax_val);
                     free(minmax_type);
                     free(minmax_i);
                  }
                  /* Return error code. */
                  return(ret);
               }
               /* No need to advance because next minutia has "slid" */
               /* into position pointed to by 'i'.                   */

               print2log("RM2\n");
            }
            else{
               /* Advance to the next minutia in the list. */
               i++;
               print2log("AD1 %d,%d\n", minutia->x, minutia->y);
            }

         }
         /* If exactly 3 min/max found and they are min-max-min ... */
         else if((minmax_num == 3) &&
                 (minmax_type[0] == -1)){
            /* Choose minima location with smallest rotated y-coord. */
            if(minmax_val[0] < minmax_val[2])
               minloc = minmax_i[0];
            else
               minloc = minmax_i[2];

            print2log("%d,%d ", minutia->x, minutia->y);

            /* Reset loation of minutia point to contour point at minima. */
            minutia->x = contour_x[minloc];
            minutia->y = contour_y[minloc];
            minutia->ex = contour_ex[minloc];
            minutia->ey = contour_ey[minloc];

            /* Must check if adjusted minutia is now in INVALID block ... */
            bx = minutia->x/lfsparms->blocksize;
            by = minutia->y/lfsparms->blocksize;
            if(*(direction_map+(by*mw)+bx) == INVALID_DIR){
               /* Remove minutia from list. */
               if((ret = remove_minutia(i, minutiae))){
                  /* Deallocate working memory. */
                  free(rot_y);
                  free_contour(contour_x, contour_y, contour_ex, contour_ey);
                  if(minmax_alloc > 0){
                     free(minmax_val);
                     free(minmax_type);
                     free(minmax_i);
                  }
                  /* Return error code. */
                  return(ret);
               }
               /* No need to advance because next minutia has "slid" */
               /* into position pointed to by 'i'.                   */

               print2log("RM3\n");
            }
            else{
               /* Advance to the next minutia in the list. */
               i++;
               print2log("AD2 %d,%d\n", minutia->x, minutia->y);
            }
         }
         /* Otherwise, ... */
         else{

            print2log("%d,%d RM4\n", minutia->x, minutia->y);

            /* Remove minutia from list. */
            if((ret = remove_minutia(i, minutiae))){
               /* If system error, then deallocate working memories. */
               free(rot_y);
               free_contour(contour_x, contour_y, contour_ex, contour_ey);
               if(minmax_alloc > 0){
                  free(minmax_val);
                  free(minmax_type);
                  free(minmax_i);
               }
               /* Return error code. */
               return(ret);
            }
            /* No need to advance because next minutia has "slid" */
            /* into position pointed to by 'i'.                   */
         }

         /* Deallocate contour and min/max buffers. */
         free_contour(contour_x, contour_y, contour_ex, contour_ey);
         if(minmax_alloc > 0){
            free(minmax_val);
            free(minmax_type);
            free(minmax_i);
         }
      } /* End else contour extracted. */
   } /* End while not end of minutiae list. */

   /* Deallocate working memory. */
   free(rot_y);

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: remove_false_minutia_V2 - Takes a list of true and false minutiae and
#cat:                attempts to detect and remove the false minutiae based
#cat:                on a series of tests.

   Input:
      minutiae  - list of true and false minutiae
      bdata     - binary image data (0==while & 1==black)
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
      direction_map  - map of image blocks containing directional ridge flow
      low_flow_map   - map of image blocks flagged as LOW RIDGE FLOW
      high_curve_map - map of image blocks flagged as HIGH CURVATURE
      mw        - width in blocks of the maps
      mh        - height in blocks of the maps
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      minutiae  - list of pruned minutiae
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
int remove_false_minutia_V2(MINUTIAE *minutiae,
           unsigned char *bdata, const int iw, const int ih,
           int *direction_map, int *low_flow_map, int *high_curve_map,
           const int mw, const int mh, const LFSPARMS *lfsparms)
{
   int ret;

   /* 1. Sort minutiae points top-to-bottom and left-to-right. */
   if((ret = sort_minutiae_y_x(minutiae, iw, ih))){
      return(ret);
   }

   /* 2. Remove minutiae on lakes (filled with white pixels) and        */
   /*    islands (filled with black pixels), both  defined by a pair of */
   /*    minutia points.                                                */
   if((ret = remove_islands_and_lakes(minutiae, bdata, iw, ih, lfsparms))){
      return(ret);
   }

   /* 3. Remove minutiae on holes in the binary image defined by a */
   /*    single point.                                             */
   if((ret = remove_holes(minutiae, bdata, iw, ih, lfsparms))){
      return(ret);
   }

   /* 4. Remove minutiae that point sufficiently close to a block with */
   /*    INVALID direction.                                            */
   if((ret = remove_pointing_invblock_V2(minutiae, direction_map, mw, mh,
                                        lfsparms))){
      return(ret);
   }

   /* 5. Remove minutiae that are sufficiently close to a block with */
   /*    INVALID direction.                                          */
   if((ret = remove_near_invblock_V2(minutiae, direction_map, mw, mh,
                                    lfsparms))){
      return(ret);
   }

   /* 6. Remove or adjust minutiae that reside on the side of a ridge */
   /*    or valley.                                                   */
   if((ret = remove_or_adjust_side_minutiae_V2(minutiae, bdata, iw, ih,
                                  direction_map, mw, mh, lfsparms))){
      return(ret);
   }

   /* 7. Remove minutiae that form a hook on the side of a ridge or valley. */
   if((ret = remove_hooks(minutiae, bdata, iw, ih, lfsparms))){
      return(ret);
   }

   /* 8. Remove minutiae that are on opposite sides of an overlap. */
   if((ret = remove_overlaps(minutiae, bdata, iw, ih, lfsparms))){
      return(ret);
   }

   /* 9. Remove minutiae that are "irregularly" shaped. */
   if((ret = remove_malformations(minutiae, bdata, iw, ih,
                                 low_flow_map, mw, mh, lfsparms))){
      return(ret);
   }

   /* 10. Remove minutiae that form long, narrow, loops in the */
   /*     "unreliable" regions in the binary image.            */
   if((ret = remove_pores_V2(minutiae,  bdata, iw, ih,
                            direction_map, low_flow_map, high_curve_map,
                            mw, mh, lfsparms))){
      return(ret);
   }

   /* 11. Remove minutiae on image edge */
   if((ret = remove_perimeter_pts(minutiae, bdata, iw, ih, lfsparms))) {
      return (ret);
   }

   return(0);
}


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

      FILE:    MINUTIA.C
      AUTHOR:  Michael D. Garris
      DATE:    05/11/1999
      UPDATED: 10/04/1999 Version 2 by MDG
      UPDATED: 09/13/2004

      Contains routines responsible for detecting initial minutia
      points as part of the NIST Latent Fingerprint System (LFS).

***********************************************************************
               ROUTINES:
                        alloc_minutiae()
                        realloc_minutiae()
                        detect_minutiae_V2()
                        update_minutiae()
                        update_minutiae_V2()
                        sort_minutiae_y_x()
                        sort_minutiae_x_y()
                        rm_dup_minutiae()
                        dump_minutiae()
                        dump_minutiae_pts()
                        dump_reliable_minutiae_pts()
                        create_minutia()
                        free_minutiae()
                        free_minutia()
                        remove_minutia()
                        join_minutia()
                        minutia_type()
                        is_minutia_appearing()
                        choose_scan_direction()
                        scan4minutiae()
                        scan4minutiae_horizontally()
                        scan4minutiae_horizontally_V2()
                        scan4minutiae_vertically()
                        scan4minutiae_vertically_V2()
                        rescan4minutiae_horizontally()
                        rescan4minutiae_vertically()
                        rescan_partial_horizontally()
                        rescan_partial_vertically()
                        get_nbr_block_index()
                        adjust_horizontal_rescan()
                        adjust_vertical_rescan()
                        process_horizontal_scan_minutia()
                        process_horizontal_scan_minutia_V2()
                        process_vertical_scan_minutia()
                        process_vertical_scan_minutia_V2()
                        adjust_high_curvature_minutia()
                        adjust_high_curvature_minutia_V2()
                        get_low_curvature_direction()
                        lfs2nist_minutia_XYT()

***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <lfs.h>



/*************************************************************************
**************************************************************************
#cat: alloc_minutiae - Allocates and initializes a minutia list based on the
#cat:            specified maximum number of minutiae to be detected.

   Input:
      max_minutiae - number of minutia to be allocated in list
   Output:
      ominutiae    - points to the allocated minutiae list
   Return Code:
      Zero      - successful completion
      Negative  - system error
**************************************************************************/
int alloc_minutiae(MINUTIAE **ominutiae, const int max_minutiae)
{
   MINUTIAE *minutiae;

   minutiae = (MINUTIAE *)malloc(sizeof(MINUTIAE));
   if(minutiae == (MINUTIAE *)NULL){
      fprintf(stderr, "ERROR : alloc_minutiae : malloc : minutiae\n");
      exit(-430);
   }
   minutiae->list = (MINUTIA **)malloc(max_minutiae * sizeof(MINUTIA *));
   if(minutiae->list == (MINUTIA **)NULL){
      fprintf(stderr, "ERROR : alloc_minutiae : malloc : minutiae->list\n");
      exit(-431);
   }

   minutiae->alloc = max_minutiae;
   minutiae->num = 0;

   *ominutiae = minutiae;
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: realloc_minutiae - Reallocates a previously allocated minutia list
#cat:            extending its allocated length based on the specified
#cat:            increment.

   Input:
      minutiae    - previously allocated list of minutiae points
      max_minutiae - number of minutia to be allocated in list
   Output:
      minutiae    - extended list of minutiae points
   Return Code:
      Zero      - successful completion
      Negative  - system error
**************************************************************************/
int realloc_minutiae(MINUTIAE *minutiae, const int incr_minutiae)
{
   minutiae->alloc += incr_minutiae;
   minutiae->list = (MINUTIA **)realloc(minutiae->list,
                                     minutiae->alloc * sizeof(MINUTIA *));
   if(minutiae->list == (MINUTIA **)NULL){
      fprintf(stderr, "ERROR : realloc_minutiae : realloc : minutiae->list\n");
      exit(-432);
   }

   return(0);
}

/*************************************************************************
**************************************************************************
#cat: detect_minutiae_V2 - Takes a binary image and its associated
#cat:            Direction and Low Flow Maps and scans each image block
#cat:            with valid direction for minutia points.  Minutia points
#cat:            detected in LOW FLOW blocks are set with lower reliability.

   Input:
      bdata     - binary image data (0==while & 1==black)
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
      direction_map  - map of image blocks containing directional ridge flow
      low_flow_map   - map of image blocks flagged as LOW RIDGE FLOW
      high_curve_map - map of image blocks flagged as HIGH CURVATURE
      mw        - width (in blocks) of the maps
      mh        - height (in blocks) of the maps
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      minutiae   - points to a list of detected minutia structures
   Return Code:
      Zero      - successful completion
      Negative  - system error
**************************************************************************/
int detect_minutiae_V2(MINUTIAE *minutiae,
            unsigned char *bdata, const int iw, const int ih,
            int *direction_map, int *low_flow_map, int *high_curve_map,
            const int mw, const int mh,
            const LFSPARMS *lfsparms)
{
   int ret;
   int *pdirection_map, *plow_flow_map, *phigh_curve_map;

   /* Pixelize the maps by assigning block values to individual pixels. */
   if((ret = pixelize_map(&pdirection_map, iw, ih, direction_map, mw, mh,
                         lfsparms->blocksize))){
      return(ret);
   }

   if((ret = pixelize_map(&plow_flow_map, iw, ih, low_flow_map, mw, mh,
                         lfsparms->blocksize))){
      free(pdirection_map);
      return(ret);
   }

   if((ret = pixelize_map(&phigh_curve_map, iw, ih, high_curve_map, mw, mh,
                         lfsparms->blocksize))){
      free(pdirection_map);
      free(plow_flow_map);
      return(ret);
   }

   if((ret = scan4minutiae_horizontally_V2(minutiae, bdata, iw, ih,
                 pdirection_map, plow_flow_map, phigh_curve_map, lfsparms))){
      free(pdirection_map);
      free(plow_flow_map);
      free(phigh_curve_map);
      return(ret);
   }

   if((ret = scan4minutiae_vertically_V2(minutiae, bdata, iw, ih,
                 pdirection_map, plow_flow_map, phigh_curve_map, lfsparms))){
      free(pdirection_map);
      free(plow_flow_map);
      free(phigh_curve_map);
      return(ret);
   }

   /* Deallocate working memories. */
   free(pdirection_map);
   free(plow_flow_map);
   free(phigh_curve_map);

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: update_minutiae - Takes a detected minutia point and (if it is not
#cat:                determined to already be in the minutiae list) adds it to
#cat:                the list.

   Input:
      minutia   - minutia structure for detected point
      bdata     - binary image data (0==while & 1==black)
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      minutiae   - points to a list of detected minutia structures
   Return Code:
      Zero      - minutia added to successfully added to minutiae list
      IGNORE    - minutia is to be ignored (already in the minutiae list)
      Negative  - system error
**************************************************************************/
int update_minutiae(MINUTIAE *minutiae, MINUTIA *minutia,
                   unsigned char *bdata, const int iw, const int ih,
                   const LFSPARMS *lfsparms)
{
   int i, ret, dy, dx, delta_dir;
   int qtr_ndirs, full_ndirs;

   /* Check to see if minutiae list is full ... if so, then extend */
   /* the length of the allocated list of minutia points.          */
   if(minutiae->num >= minutiae->alloc){
      if((ret = realloc_minutiae(minutiae, MAX_MINUTIAE)))
         return(ret);
   }

   /* Otherwise, there is still room for more minutia. */

   /* Compute quarter of possible directions in a semi-circle */
   /* (ie. 45 degrees).                                       */
   qtr_ndirs = lfsparms->num_directions>>2;

   /* Compute number of directions in full circle. */
   full_ndirs = lfsparms->num_directions<<1;

   /* Is the minutiae list empty? */
   if(minutiae->num > 0){
      /* Foreach minutia stored in the list... */
      for(i = 0; i < minutiae->num; i++){
         /* If x distance between new minutia and current list minutia */
         /* are sufficiently close...                                 */
         dx = abs(minutiae->list[i]->x - minutia->x);
         if(dx < lfsparms->max_minutia_delta){
            /* If y distance between new minutia and current list minutia */
            /* are sufficiently close...                                 */
            dy = abs(minutiae->list[i]->y - minutia->y);
            if(dy < lfsparms->max_minutia_delta){
               /* If new minutia and current list minutia are same type... */
               if(minutiae->list[i]->type == minutia->type){
                  /* Test to see if minutiae have similar directions. */
                  /* Take minimum of computed inner and outer        */
                  /* direction differences.                          */
                  delta_dir = abs(minutiae->list[i]->direction -
                                  minutia->direction);
                  delta_dir = min(delta_dir, full_ndirs-delta_dir);
                  /* If directional difference is <= 45 degrees... */
                  if(delta_dir <= qtr_ndirs){
                     /* If new minutia and current list minutia share */
                     /* the same point... */
                     if((dx==0) && (dy==0)){
                        /* Then the minutiae match, so don't add the new one */
                        /* to the list.                                     */
                        return(IGNORE);
                     }
                     /* Othewise, check if they share the same contour. */
                     /* Start by searching "max_minutia_delta" steps    */
                     /* clockwise.                                      */
                     /* If new minutia point found on contour...        */
                     if(search_contour(minutia->x, minutia->y,
                               lfsparms->max_minutia_delta,
                               minutiae->list[i]->x, minutiae->list[i]->y,
                               minutiae->list[i]->ex, minutiae->list[i]->ey,
                               SCAN_CLOCKWISE, bdata, iw, ih)){
                        /* Consider the new minutia to be the same as the */
                        /* current list minutia, so don't add the new one */
                        /* to the list.                                   */
                        return(IGNORE);
                     }
                     /* Now search "max_minutia_delta" steps counter-  */
                     /* clockwise along contour.                       */
                     /* If new minutia point found on contour...       */
                     if(search_contour(minutia->x, minutia->y,
                               lfsparms->max_minutia_delta,
                               minutiae->list[i]->x, minutiae->list[i]->y,
                               minutiae->list[i]->ex, minutiae->list[i]->ey,
                               SCAN_COUNTER_CLOCKWISE, bdata, iw, ih)){
                        /* Consider the new minutia to be the same as the */
                        /* current list minutia, so don't add the new one */
                        /* to the list.                                   */
                        return(IGNORE);
                     }

                     /* Otherwise, new minutia and current list minutia do */
                     /* not share the same contour, so although they are   */
                     /* similar in type and location, treat them as 2      */
                     /* different minutia.                                 */

                  } /* Otherwise, directions are too different. */
               } /* Otherwise, minutiae are different type. */
            } /* Otherwise, minutiae too far apart in Y. */
         } /* Otherwise, minutiae too far apart in X. */
      } /* End FOR minutia in list. */
   } /* Otherwise, minutiae list is empty. */

   /* Otherwise, assume new minutia is not in the list, so add it. */
   minutiae->list[minutiae->num] = minutia;
   (minutiae->num)++;

   /* New minutia was successfully added to the list. */
   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: update_minutiae_V2 - Takes a detected minutia point and (if it is not
#cat:                determined to already be in the minutiae list or the
#cat:                new point is determined to be "more compatible") adds
#cat:                it to the list.

   Input:
      minutia   - minutia structure for detected point
      scan_dir  - orientation of scan when minutia was detected
      dmapval   - directional ridge flow of block minutia is in
      bdata     - binary image data (0==while & 1==black)
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      minutiae   - points to a list of detected minutia structures
   Return Code:
      Zero      - minutia added to successfully added to minutiae list
      IGNORE    - minutia is to be ignored (already in the minutiae list)
      Negative  - system error
**************************************************************************/
int update_minutiae_V2(MINUTIAE *minutiae, MINUTIA *minutia,
                   const int scan_dir, const int dmapval,
                   unsigned char *bdata, const int iw, const int ih,
                   const LFSPARMS *lfsparms)
{
   int i, ret, dy, dx, delta_dir;
   int qtr_ndirs, full_ndirs;
   int map_scan_dir;

   /* Check to see if minutiae list is full ... if so, then extend */
   /* the length of the allocated list of minutia points.          */
   if(minutiae->num >= minutiae->alloc){
      if((ret = realloc_minutiae(minutiae, MAX_MINUTIAE)))
         return(ret);
   }

   /* Otherwise, there is still room for more minutia. */

   /* Compute quarter of possible directions in a semi-circle */
   /* (ie. 45 degrees).                                       */
   qtr_ndirs = lfsparms->num_directions>>2;

   /* Compute number of directions in full circle. */
   full_ndirs = lfsparms->num_directions<<1;

   /* Is the minutiae list empty? */
   if(minutiae->num > 0){
      /* Foreach minutia stored in the list (in reverse order) ... */
      for(i = minutiae->num-1; i >= 0; i--){
         /* If x distance between new minutia and current list minutia */
         /* are sufficiently close...                                 */
         dx = abs(minutiae->list[i]->x - minutia->x);
         if(dx < lfsparms->max_minutia_delta){
            /* If y distance between new minutia and current list minutia */
            /* are sufficiently close...                                 */
            dy = abs(minutiae->list[i]->y - minutia->y);
            if(dy < lfsparms->max_minutia_delta){
               /* If new minutia and current list minutia are same type... */
               if(minutiae->list[i]->type == minutia->type){
                  /* Test to see if minutiae have similar directions. */
                  /* Take minimum of computed inner and outer        */
                  /* direction differences.                          */
                  delta_dir = abs(minutiae->list[i]->direction -
                                  minutia->direction);
                  delta_dir = min(delta_dir, full_ndirs-delta_dir);
                  /* If directional difference is <= 45 degrees... */
                  if(delta_dir <= qtr_ndirs){
                     /* If new minutia and current list minutia share */
                     /* the same point... */
                     if((dx==0) && (dy==0)){
                        /* Then the minutiae match, so don't add the new one */
                        /* to the list.                                     */
                        return(IGNORE);
                     }
                     /* Othewise, check if they share the same contour. */
                     /* Start by searching "max_minutia_delta" steps    */
                     /* clockwise.                                      */
                     /* If new minutia point found on contour...        */
                     if(search_contour(minutia->x, minutia->y,
                               lfsparms->max_minutia_delta,
                               minutiae->list[i]->x, minutiae->list[i]->y,
                               minutiae->list[i]->ex, minutiae->list[i]->ey,
                               SCAN_CLOCKWISE, bdata, iw, ih) ||
                        search_contour(minutia->x, minutia->y,
                               lfsparms->max_minutia_delta,
                               minutiae->list[i]->x, minutiae->list[i]->y,
                               minutiae->list[i]->ex, minutiae->list[i]->ey,
                               SCAN_COUNTER_CLOCKWISE, bdata, iw, ih)){
                        /* If new minutia has VALID block direction ... */
                        if(dmapval >= 0){
                           /* Derive feature scan direction compatible */
                           /* with VALID direction.                    */
                           map_scan_dir = choose_scan_direction(dmapval,
                                                    lfsparms->num_directions);
                           /* If map scan direction compatible with scan   */
                           /* direction in which new minutia was found ... */
                           if(map_scan_dir == scan_dir){
                              /* Then choose the new minutia over the one */
                              /* currently in the list.                   */
                              if((ret = remove_minutia(i, minutiae))){
                                 return(ret);
                              }
                              /* Continue on ... */
                           }
                           else
                              /* Othersize, scan directions not compatible...*/
                              /* so choose to keep the current minutia in    */
                              /* the list and ignore the new one.            */
                              return(IGNORE);
                        }
                        else{
                           /* Otherwise, no reason to believe new minutia    */
                           /* is any better than the current one in the list,*/
                           /* so consider the new minutia to be the same as  */
                           /* the current list minutia, and don't add the new*/
                           /*  one to the list.                              */
                           return(IGNORE);
                        }
                     }

                     /* Otherwise, new minutia and current list minutia do */
                     /* not share the same contour, so although they are   */
                     /* similar in type and location, treat them as 2      */
                     /* different minutia.                                 */

                  } /* Otherwise, directions are too different. */
               } /* Otherwise, minutiae are different type. */
            } /* Otherwise, minutiae too far apart in Y. */
         } /* Otherwise, minutiae too far apart in X. */
      } /* End FOR minutia in list. */
   } /* Otherwise, minutiae list is empty. */

   /* Otherwise, assume new minutia is not in the list, or those that */
   /* were close neighbors were selectively removed, so add it.       */
   minutiae->list[minutiae->num] = minutia;
   (minutiae->num)++;

   /* New minutia was successfully added to the list. */
   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: sort_minutiae_y_x - Takes a list of minutia points and sorts them
#cat:                 top-to-bottom and then left-to-right.

   Input:
      minutiae  - list of minutiae
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
   Output:
      minutiae  - list of sorted minutiae
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
int sort_minutiae_y_x(MINUTIAE *minutiae, const int iw, const int ih)
{
   int *ranks, *order;
   int i, ret;
   MINUTIA **newlist;

   /* Allocate a list of integers to hold 1-D image pixel offsets */
   /* for each of the 2-D minutia coordinate points.               */
   ranks = (int *)malloc(minutiae->num * sizeof(int));
   if(ranks == (int *)NULL){
      fprintf(stderr, "ERROR : sort_minutiae_y_x : malloc : ranks\n");
      return(-310);
   }

   /* Compute 1-D image pixel offsets form 2-D minutia coordinate points. */
   for(i = 0; i < minutiae->num; i++)
      ranks[i] = (minutiae->list[i]->y * iw) + minutiae->list[i]->x;

   /* Get sorted order of minutiae. */
   if((ret = sort_indices_int_inc(&order, ranks, minutiae->num))){
      free(ranks);
      return(ret);
   }

   /* Allocate new MINUTIA list to hold sorted minutiae. */
   newlist = (MINUTIA **)malloc(minutiae->num * sizeof(MINUTIA *));
   if(newlist == (MINUTIA **)NULL){
      free(ranks);
      free(order);
      fprintf(stderr, "ERROR : sort_minutiae_y_x : malloc : newlist\n");
      return(-311);
   }

   /* Put minutia into sorted order in new list. */
   for(i = 0; i < minutiae->num; i++)
      newlist[i] = minutiae->list[order[i]];

   /* Deallocate non-sorted list of minutia pointers. */
   free(minutiae->list);
   /* Assign new sorted list of minutia to minutiae list. */
   minutiae->list = newlist;

   /* Free the working memories supporting the sort. */
   free(order);
   free(ranks);

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: sort_minutiae_x_y - Takes a list of minutia points and sorts them
#cat:                 left-to-right and then top-to-bottom.

   Input:
      minutiae  - list of minutiae
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
   Output:
      minutiae  - list of sorted minutiae
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
int sort_minutiae_x_y(MINUTIAE *minutiae, const int iw, const int ih)
{
   int *ranks, *order;
   int i, ret;
   MINUTIA **newlist;

   /* Allocate a list of integers to hold 1-D image pixel offsets */
   /* for each of the 2-D minutia coordinate points.               */
   ranks = (int *)malloc(minutiae->num * sizeof(int));
   if(ranks == (int *)NULL){
      fprintf(stderr, "ERROR : sort_minutiae_x_y : malloc : ranks\n");
      return(-440);
   }

   /* Compute 1-D image pixel offsets form 2-D minutia coordinate points. */
   for(i = 0; i < minutiae->num; i++)
      ranks[i] = (minutiae->list[i]->x * iw) + minutiae->list[i]->y;

   /* Get sorted order of minutiae. */
   if((ret = sort_indices_int_inc(&order, ranks, minutiae->num))){
      free(ranks);
      return(ret);
   }

   /* Allocate new MINUTIA list to hold sorted minutiae. */
   newlist = (MINUTIA **)malloc(minutiae->num * sizeof(MINUTIA *));
   if(newlist == (MINUTIA **)NULL){
      free(ranks);
      free(order);
      fprintf(stderr, "ERROR : sort_minutiae_x_y : malloc : newlist\n");
      return(-441);
   }

   /* Put minutia into sorted order in new list. */
   for(i = 0; i < minutiae->num; i++)
      newlist[i] = minutiae->list[order[i]];

   /* Deallocate non-sorted list of minutia pointers. */
   free(minutiae->list);
   /* Assign new sorted list of minutia to minutiae list. */
   minutiae->list = newlist;

   /* Free the working memories supporting the sort. */
   free(order);
   free(ranks);

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: rm_dup_minutiae - Takes a list of minutiae sorted in some adjacent order
#cat:               and detects and removes redundant minutia that have the
#cat:               same exact pixel coordinate locations (even if other
#cat:               attributes may differ).

   Input:
      mintuiae - list of sorted minutiae
   Output:
      mintuiae - list of sorted minutiae with duplicates removed
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
int rm_dup_minutiae(MINUTIAE *minutiae)
{
   int i, ret;
   MINUTIA *minutia1, *minutia2;

   /* Work backward from the end of the list of minutiae.  This way */
   /* we can selectively remove minutia from the list and not cause */
   /* problems with keeping track of current indices.               */
   for(i = minutiae->num-1; i > 0; i--){
      minutia1 = minutiae->list[i];
      minutia2 = minutiae->list[i-1];
      /* If minutia pair has identical coordinates ... */
      if((minutia1->x == minutia2->x) &&
         (minutia1->y == minutia2->y)){
         /* Remove the 2nd minutia from the minutiae list. */
         if((ret = remove_minutia(i-1, minutiae)))
            return(ret);
         /* The first minutia slides into the position of the 2nd. */
      }
   }

   /* Return successfully. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: dump_minutiae - Given a minutiae list, writes a formatted text report of
#cat:            the list's contents to the specified open file pointer.

   Input:
      minutiae - list of minutia structures
   Output:
      fpout - open file pointer
**************************************************************************/
void dump_minutiae(FILE *fpout, const MINUTIAE *minutiae)
{
   int i, j;

   fprintf(fpout, "\n%d Minutiae Detected\n\n", minutiae->num);

   for(i = 0; i < minutiae->num; i++){
      /* Precision of reliablity added one decimal position */
      /* on 09-13-04 */
      fprintf(fpout, "%4d : %4d, %4d : %2d : %6.3f :", i,
             minutiae->list[i]->x, minutiae->list[i]->y,
             minutiae->list[i]->direction, minutiae->list[i]->reliability);
      if(minutiae->list[i]->type == RIDGE_ENDING)
         fprintf(fpout, "RIG : ");
      else
         fprintf(fpout, "BIF : ");
   
      if(minutiae->list[i]->appearing)
         fprintf(fpout, "APP : ");
      else
         fprintf(fpout, "DIS : ");

      fprintf(fpout, "%2d ", minutiae->list[i]->feature_id);

      for(j = 0; j < minutiae->list[i]->num_nbrs; j++){
         fprintf(fpout, ": %4d,%4d; %2d ",
                 minutiae->list[minutiae->list[i]->nbrs[j]]->x,
                 minutiae->list[minutiae->list[i]->nbrs[j]]->y,
                 minutiae->list[i]->ridge_counts[j]);
      }

      fprintf(fpout, "\n");
   }
}

/*************************************************************************
**************************************************************************
#cat: dump_minutiae_pts - Given a minutiae list, writes the coordinate point
#cat:            for each minutia in the list to the specified open
#cat:            file pointer.

   Input:
      minutiae - list of minutia structures
   Output:
      fpout - open file pointer
**************************************************************************/
void dump_minutiae_pts(FILE *fpout, const MINUTIAE *minutiae)
{
   int i;

   /* First line in the output file contians the number of minutia */
   /* points to be written to the file.                            */
   fprintf(fpout, "%d\n", minutiae->num);

   /* Foreach minutia in list... */
   for(i = 0; i < minutiae->num; i++){
      /* Write the minutia's coordinate point to the file pointer. */
      fprintf(fpout, "%4d %4d\n", minutiae->list[i]->x, minutiae->list[i]->y);
   }
}


/*************************************************************************
**************************************************************************
#cat: dump_reliable_minutiae_pts - Given a minutiae list, writes the
#cat:            coordinate point for each minutia in the list that has
#cat:            the specified reliability to the specified open
#cat:            file pointer.

   Input:
      minutiae    - list of minutia structures
      reliability - desired reliability level for minutiae to be reported
   Output:
      fpout - open file pointer
**************************************************************************/
void dump_reliable_minutiae_pts(FILE *fpout, const MINUTIAE *minutiae,
                                const double reliability)
{
   int i, count;

   /* First count the number of qualifying minutiae so that the */
   /* MFS header may be written.                                */
   count = 0;
   /* Foreach minutia in list... */
   for(i = 0; i < minutiae->num; i++){
      if(minutiae->list[i]->reliability == reliability)
         count++;
   }

   /* First line in the output file contians the number of minutia */
   /* points to be written to the file.                            */
   fprintf(fpout, "%d\n", count);

   /* Foreach minutia in list... */
   for(i = 0; i < minutiae->num; i++){
      if(minutiae->list[i]->reliability == reliability)
         /* Write the minutia's coordinate point to the file pointer. */
         fprintf(fpout, "%4d %4d\n",
                 minutiae->list[i]->x, minutiae->list[i]->y);
   }
}

/*************************************************************************
**************************************************************************
#cat: create_minutia - Takes attributes associated with a detected minutia
#cat:            point and allocates and initializes a minutia structure.

   Input:
      x_loc   - x-pixel coord of minutia (interior to feature)
      y_loc   - y-pixel coord of minutia (interior to feature)
      x_edge  - x-pixel coord of corresponding edge pixel (exterior to feature)
      y_edge  - y-pixel coord of corresponding edge pixel (exterior to feature)
      idir    - integer direction of the minutia
      reliability - floating point measure of minutia's reliability
      type    - type of the minutia (ridge-ending or bifurcation)
      appearing  - designates the minutia as appearing or disappearing
      feature_id - index of minutia's matching feature_patterns[]
   Output:
      ominutia - ponter to an allocated and initialized minutia structure
   Return Code:
      Zero       - minutia structure successfully allocated and initialized
      Negative   - system error
*************************************************************************/
int create_minutia(MINUTIA **ominutia, const int x_loc, const int y_loc,
                   const int x_edge, const int y_edge, const int idir,
                   const double reliability,
                   const int type, const int appearing, const int feature_id)
{
   MINUTIA *minutia;

   /* Allocate a minutia structure. */
   minutia = (MINUTIA *)malloc(sizeof(MINUTIA));
   /* If allocation error... */
   if(minutia == (MINUTIA *)NULL){
      fprintf(stderr, "ERROR : create_minutia : malloc : minutia\n");
      return(-230);
   }

   /* Assign minutia structure attributes. */
   minutia->x = x_loc;
   minutia->y = y_loc;
   minutia->ex = x_edge;
   minutia->ey = y_edge;
   minutia->direction = idir;
   minutia->reliability = reliability;
   minutia->type = type;
   minutia->appearing = appearing;
   minutia->feature_id = feature_id;
   minutia->nbrs = (int *)NULL;
   minutia->ridge_counts = (int *)NULL;
   minutia->num_nbrs = 0;

   /* Set minutia object to output pointer. */
   *ominutia = minutia;
   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: free_minutiae - Takes a minutiae list and deallocates all memory
#cat:                 associated with it.

   Input:
      minutiae - pointer to allocated list of minutia structures
*************************************************************************/
void free_minutiae(MINUTIAE *minutiae)
{
   int i;

   /* Deallocate minutia structures in the list. */
   for(i = 0; i < minutiae->num; i++)
      free_minutia(minutiae->list[i]);
   /* Deallocate list of minutia pointers. */
   free(minutiae->list);

   /* Deallocate the list structure. */
   free(minutiae);
}

/*************************************************************************
**************************************************************************
#cat: free_minutia - Takes a minutia pointer and deallocates all memory
#cat:            associated with it.

   Input:
      minutia - pointer to allocated minutia structure
*************************************************************************/
void free_minutia(MINUTIA *minutia)
{
   /* Deallocate sublists. */
   if(minutia->nbrs != (int *)NULL)
      free(minutia->nbrs);
   if(minutia->ridge_counts != (int *)NULL)
      free(minutia->ridge_counts);

   /* Deallocate the minutia structure. */
   free(minutia);
}

/*************************************************************************
**************************************************************************
#cat: remove_minutia - Removes the specified minutia point from the input
#cat:                  list of minutiae.

   Input:
      index      - position of minutia to be removed from list
      minutiae   - input list of minutiae
   Output:
      minutiae   - list with minutia removed
   Return Code:
      Zero      - successful completion
      Negative  - system error
**************************************************************************/
int remove_minutia(const int index, MINUTIAE *minutiae)
{
   int fr, to;

   /* Make sure the requested index is within range. */
   if((index < 0) && (index >= minutiae->num)){
      fprintf(stderr, "ERROR : remove_minutia : index out of range\n");
      return(-380);
   }

   /* Deallocate the minutia structure to be removed. */
   free_minutia(minutiae->list[index]);

   /* Slide the remaining list of minutiae up over top of the */
   /* position of the minutia being removed.                 */
   for(to = index, fr = index+1; fr < minutiae->num; to++, fr++)
      minutiae->list[to] = minutiae->list[fr];

   /* Decrement the number of minutiae remaining in the list. */
   minutiae->num--;

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: join_minutia - Takes 2 minutia points and connectes their features in
#cat:                the input binary image.  A line is drawn in the image
#cat:                between the 2 minutia with a specified line-width radius
#cat:                and a conditional border of pixels opposite in color
#cat:                from the interior line.

   Input:
      minutia1      - first minutia point to be joined
      minutia2      - second minutia point to be joined
      bdata         - binary image data (0==while & 1==black)
      iw            - width (in pixels) of image
      ih            - height (in pixels) of image
      with_boundary - signifies the inclusion of border pixels
      line_radius   - line-width radius of join line
   Output:
      bdata      - edited image with minutia features joined
   Return Code:
      Zero      - successful completion
      Negative  - system error
**************************************************************************/
int join_minutia(const MINUTIA *minutia1, const MINUTIA *minutia2,
                 unsigned char *bdata, const int iw, const int ih,
                 const int with_boundary, const int line_radius)
{
   int dx_gte_dy, delta_x, delta_y;
   int *x_list, *y_list, num;
   int minutia_pix, boundary_pix;
   int i, j, ret;
   int x1, y1, x2, y2;

   /* Compute X and Y deltas between minutia points. */
   delta_x = abs(minutia1->x - minutia2->x);
   delta_y = abs(minutia1->y - minutia2->y);

   /* Set flag based on |DX| >= |DY|. */
   /* If flag is true then add additional pixel width to the join line  */
   /* by adding pixels neighboring top and bottom.                      */
   /* If flag is false then add additional pixel width to the join line */
   /* by adding pixels neighboring left and right.                      */
   if(delta_x >= delta_y)
      dx_gte_dy = 1;
   else
      dx_gte_dy = 0;

   /* Compute points along line segment between the two minutia points. */
   if((ret = line_points(&x_list, &y_list, &num,
                      minutia1->x, minutia1->y, minutia2->x, minutia2->y)))
      /* If error with line routine, return error code. */
      return(ret);

   /* Determine pixel color of minutia and boundary. */
   if(minutia1->type == RIDGE_ENDING){
      /* To connect 2 ridge-endings, draw black. */
      minutia_pix = 1;
      boundary_pix = 0;
   }
   else{
      /* To connect 2 bifurcations, draw white. */
      minutia_pix = 0;
      boundary_pix = 1;
   }

   /* Foreach point on line connecting the minutiae points ... */
   for(i = 1; i < num-1; i++){
      /* Draw minutia pixel at current point on line. */
      *(bdata+(y_list[i]*iw)+x_list[i]) = minutia_pix;

      /* Initialize starting corrdinates for adding width to the */
      /* join line to the current point on the line.             */
      x1 = x_list[i];
      y1 = y_list[i];
      x2 = x1;
      y2 = y1;
      /* Foreach pixel of added radial width ... */
      for(j = 0; j < line_radius; j++){

         /* If |DX|>=|DY|, we want to add width to line by writing */
         /*                to pixels neighboring above and below.  */
         /*                x1 -= (0=(1-1)); y1 -= 1 ==> ABOVE      */
         /*                x2 += (0=(1-1)); y2 += 1 ==> BELOW      */
         /* If |DX|<|DY|, we want to add width to line by writing  */
         /*                to pixels neighboring left and right.   */
         /*                x1 -= (1=(1-0)); y1 -= 0 ==> LEFT       */
         /*                x2 += (1=(1-0)); y2 += 0 ==> RIGHT      */

         /* Advance 1st point along width dimension. */
         x1 -= (1 - dx_gte_dy);
         y1 -= dx_gte_dy;
         /* If pixel 1st point is within image boundaries ... */
         if((x1 >= 0) && (x1 < iw) &&
            (y1 >= 0) && (y1 < ih))
            /* Write the pixel ABOVE or LEFT. */
            *(bdata+(y1*iw)+x1) = minutia_pix;

         /* Advance 2nd point along width dimension. */
         x2 += (1 - dx_gte_dy);
         y2 += dx_gte_dy;
         /* If pixel 2nd point is within image boundaries ... */
         if((x2 >= 0) && (x2 < iw) &&
            /* Write the pixel BELOW or RIGHT. */
            (y2 >= 0) && (y2 < ih))
            *(bdata+(y2*iw)+x2) = minutia_pix;
      }

      /* If boundary flag is set ... draw the boundary pixels.*/
      if(with_boundary){
         /* Advance 1st point along width dimension. */
         x1 -= (1 - dx_gte_dy);
         y1 -= dx_gte_dy;
         /* If pixel 1st point is within image boundaries ... */
         if((x1 >= 0) && (x1 < iw) &&
            (y1 >= 0) && (y1 < ih))
            /* Write the pixel ABOVE or LEFT of opposite color. */
            *(bdata+(y1*iw)+x1) = boundary_pix;

         /* Advance 2nd point along width dimension. */
         x2 += (1 - dx_gte_dy);
         y2 += dx_gte_dy;
         /* If pixel 2nd point is within image boundaries ... */
         if((x2 >= 0) && (x2 < iw) &&
            (y2 >= 0) && (y2 < ih))
            /* Write the pixel BELOW or RIGHT of opposite color. */
            *(bdata+(y2*iw)+x2) = boundary_pix;
      }
   }

   /* Deallocate points along connecting line. */
   free(x_list);
   free(y_list);

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: minutia_type - Given the pixel color of the detected feature, returns
#cat:            whether the minutia is a ridge-ending (black pixel) or
#cat:            bifurcation (white pixel).

   Input:
      feature_pix - pixel color of the feature's interior
   Return Code:
      RIDGE_ENDING - minutia is a ridge-ending
      BIFURCATION  - minutia is a bifurcation (valley-ending)
**************************************************************************/
int minutia_type(const int feature_pix)
{
   int type;

   /* If feature pixel is white ... */
   if(feature_pix == 0)
      /* Then the feature is a valley-ending, so BIFURCATION. */
      type = BIFURCATION;
   /* Otherwise, the feature pixel is black ... */
   else
      /* So the feature is a RIDGE-ENDING. */
      type = RIDGE_ENDING;

   /* Return the type. */
   return(type);
}

/*************************************************************************
**************************************************************************
#cat: is_minutia_appearing - Given the pixel location of a minutia feature
#cat:            and its corresponding adjacent edge pixel, returns whether
#cat:            the minutia is appearing or disappearing.  Remeber, that
#cat:            "feature" refers to either a ridge or valley-ending.

   Input:
      x_loc      - x-pixel coord of feature (interior to feature)
      y_loc      - y-pixel coord of feature (interior to feature)
      x_edge     - x-pixel coord of corresponding edge pixel
                   (exterior to feature)
      y_edge     - y-pixel coord of corresponding edge pixel
                   (exterior to feature)
   Return Code:
      APPEARING    - minutia is appearing (TRUE==1)
      DISAPPEARING - minutia is disappearing (FALSE==0)
      Negative     - system error
**************************************************************************/
int is_minutia_appearing(const int x_loc, const int y_loc,
                         const int x_edge, const int y_edge)
{
   /* Edge pixels will always be N,S,E,W of feature pixel. */

   /* 1. When scanning for feature's HORIZONTALLY... */
   /* If the edge is above the feature, then appearing. */
   if(x_edge < x_loc)
      return(APPEARING);
   /* If the edge is below the feature, then disappearing. */
   if(x_edge > x_loc)
      return(DISAPPEARING);

   /* 1. When scanning for feature's VERTICALLY... */
   /* If the edge is left of feature, then appearing. */
   if(y_edge < y_loc)
      return(APPEARING);
   /* If the edge is right of feature, then disappearing. */
   if(y_edge > y_loc)
      return(DISAPPEARING);

   /* Should never get here, but just in case. */
   fprintf(stderr,
           "ERROR : is_minutia_appearing : bad configuration of pixels\n");
   return(-240);
}

/*************************************************************************
**************************************************************************
#cat: choose_scan_direction - Determines the orientation (horizontal or
#cat:            vertical) in which a block is to be scanned for minutiae.
#cat:            The orientation is based on the blocks corresponding IMAP
#cat:            direction.

   Input:
      imapval   - Block's IMAP direction
      ndirs     - number of possible IMAP directions (within semicircle)
   Return Code:
      SCAN_HORIZONTAL - horizontal orientation
      SCAN_VERTICAL   - vertical orientation
**************************************************************************/
int choose_scan_direction(const int imapval, const int ndirs)
{
   int qtr_ndirs;

   /* Compute quarter of directions in semi-circle. */
   qtr_ndirs = ndirs>>2;

   /* If ridge flow in block is relatively vertical, then we want */
   /* to scan for minutia features in the opposite direction      */
   /* (ie. HORIZONTALLY).                                         */
   if((imapval <= qtr_ndirs) || (imapval > (qtr_ndirs*3)))
      return(SCAN_HORIZONTAL);
   /* Otherwise, ridge flow is realtively horizontal, and we want */
   /* to scan for minutia features in the opposite direction      */
   /* (ie. VERTICALLY).                                           */
   else
      return(SCAN_VERTICAL);

}

/*************************************************************************
**************************************************************************
#cat: scan4minutiae - Scans a block of binary image data detecting potential
#cat:                minutiae points.

   Input:
      bdata     - binary image data (0==while & 1==black)
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
      imap      - matrix of ridge flow directions
      nmap      - IMAP augmented with blocks of HIGH-CURVATURE and
                  blocks which have no neighboring valid directions.
      blk_x     - x-block coord to be scanned
      blk_y     - y-block coord to be scanned
      mw        - width (in blocks) of IMAP and NMAP matrices.
      mh        - height (in blocks) of IMAP and NMAP matrices.
      scan_x    - x-pixel coord of origin of region to be scanned
      scan_y    - y-pixel coord of origin of region to be scanned
      scan_w    - width (in pixels) of region to be scanned
      scan_h    - height (in pixels) of region to be scanned
      scan_dir  - the scan orientation (horizontal or vertical)
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      minutiae   - points to a list of detected minutia structures
   Return Code:
      Zero      - successful completion
      Negative  - system error
**************************************************************************/
int scan4minutiae(MINUTIAE *minutiae,
                unsigned char *bdata, const int iw, const int ih,
                const int *imap, const int *nmap,
                const int blk_x, const int blk_y, const int mw, const int mh,
                const int scan_x, const int scan_y,
                const int scan_w, const int scan_h, const int scan_dir,
                const LFSPARMS *lfsparms)
{
   int blk_i, ret;

   /* Compute block index from block coordinates. */
   blk_i = (blk_y*mw) + blk_x;

   /* Conduct primary scan for minutiae horizontally. */
   if(scan_dir == SCAN_HORIZONTAL){

      if((ret = scan4minutiae_horizontally(minutiae, bdata, iw, ih,
                         imap[blk_i], nmap[blk_i],
                         scan_x, scan_y, scan_w, scan_h, lfsparms))){
         /* Return code may be:                      */
         /* 1. ret<0 (implying system error)         */
         return(ret);
      }

      /* Rescan block vertically. */
      if((ret = rescan4minutiae_vertically(minutiae, bdata, iw, ih,
                        imap, nmap, blk_x, blk_y, mw, mh,
                        scan_x, scan_y, scan_w, scan_h, lfsparms))){
         /* Return code may be:                      */
         /* 1. ret<0 (implying system error)         */
         return(ret);
      }
   }

   /* Otherwise, conduct primary scan for minutiae vertically. */
   else{
      if((ret = scan4minutiae_vertically(minutiae, bdata, iw, ih,
                            imap[blk_i], nmap[blk_i],
                            scan_x, scan_y, scan_w, scan_h, lfsparms))){
         /* Return resulting code. */
         return(ret);
      }

      /* Rescan block horizontally. */
      if((ret = rescan4minutiae_horizontally(minutiae, bdata, iw, ih,
                            imap, nmap, blk_x, blk_y, mw, mh,
                            scan_x, scan_y, scan_w, scan_h, lfsparms))){
         /* Return resulting code. */
         return(ret);
      }
   }

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: scan4minutiae_horizontally - Scans a specified region of binary image
#cat:                data horizontally, detecting potential minutiae points.
#cat:                Minutia detected via the horizontal scan process are
#cat:                by nature vertically oriented (orthogonal to the scan).
#cat:                The region actually scanned is slightly larger than that
#cat:                specified.  This overlap attempts to minimize the number
#cat:                of minutiae missed at the region boundaries.
#cat:                HOWEVER, some minutiae will still be missed!

   Input:
      bdata     - binary image data (0==while & 1==black)
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
      imapval   - IMAP value associated with this image region
      nmapval   - NMAP value associated with this image region
      scan_x    - x-pixel coord of origin of region to be scanned
      scan_y    - y-pixel coord of origin of region to be scanned
      scan_w    - width (in pixels) of region to be scanned
      scan_h    - height (in pixels) of region to be scanned
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      minutiae   - points to a list of detected minutia structures
   Return Code:
      Zero      - successful completion
      Negative  - system error
**************************************************************************/
int scan4minutiae_horizontally(MINUTIAE *minutiae,
                unsigned char *bdata, const int iw, const int ih,
                const int imapval, const int nmapval,
                const int scan_x, const int scan_y,
                const int scan_w, const int scan_h,
                const LFSPARMS *lfsparms)
{
   int sx, sy, ex, ey, cx, cy, x2;
   unsigned char *p1ptr, *p2ptr;
   int possible[NFEATURES], nposs;
   int ret;

   /* NOTE!!! Minutia that "straddle" region boundaries may be missed! */

   /* If possible, overlap left and right of current scan region      */
   /* by 2 pixel columns to help catch some minutia that straddle the */
   /* the scan region boundaries.                                     */
   sx = max(0, scan_x-2);
   ex = min(iw, scan_x+scan_w+2);

   /* If possible, overlap the scan region below by 1 pixel row. */
   sy = scan_y;
   ey = min(ih, scan_y+scan_h+1);

   /* For now, we will not adjust for IMAP edge, as the binary image  */
   /* was properly padded at its edges so as not to cause anomallies. */

   /* Start at first row in region. */
   cy = sy;
   /* While second scan row not outside the bottom of the scan region... */
   while(cy+1 < ey){
      /* Start at beginning of new scan row in region. */
      cx = sx;
      /* While not at end of region's current scan row. */
      while(cx < ex){
         /* Get pixel pair from current x position in current and next */
         /* scan rows. */
         p1ptr = bdata+(cy*iw)+cx;
         p2ptr = bdata+((cy+1)*iw)+cx;
         /* If scan pixel pair matches first pixel pair of */
         /* 1 or more features... */
         if(match_1st_pair(*p1ptr, *p2ptr, possible, &nposs)){
            /* Bump forward to next scan pixel pair. */
            cx++;
            p1ptr++;
            p2ptr++;
            /* If not at end of region's current scan row... */
            if(cx < ex){
               /* If scan pixel pair matches second pixel pair of */
               /* 1 or more features... */
               if(match_2nd_pair(*p1ptr, *p2ptr, possible, &nposs)){
                  /* Store current x location. */
                  x2 = cx;
                  /* Skip repeated pixel pairs. */
                  skip_repeated_horizontal_pair(&cx, ex, &p1ptr, &p2ptr,
                                                    iw, ih);
                  /* If not at end of region's current scan row... */
                  if(cx < ex){
                     /* If scan pixel pair matches third pixel pair of */
                     /* a single feature... */
                     if(match_3rd_pair(*p1ptr, *p2ptr, possible, &nposs)){
                        /* Process detected minutia point. */
                        if((ret = process_horizontal_scan_minutia(minutiae,
                                         cx, cy, x2, possible[0],
                                         bdata, iw, ih,
                                         imapval, nmapval, lfsparms))){
                           /* Return code may be:                       */
                           /* 1.  ret< 0 (implying system error)        */
                           /* 2. ret==IGNORE (ignore current feature)   */
                           if(ret < 0)
                              return(ret);
                           /* Otherwise, IGNORE and continue. */
                        }
                     }

                     /* Set up to resume scan. */
                     /* Test to see if 3rd pair can slide into 2nd pair. */
                     /* The values of the 2nd pair MUST be different.    */
                     /* If 3rd pair values are different ... */
                     if(*p1ptr != *p2ptr){
                        /* Set next first pair to last of repeated */
                        /* 2nd pairs, ie. back up one pair.        */
                        cx--;
                     }

                     /* Otherwise, 3rd pair can't be a 2nd pair, so  */
                     /* keep pointing to 3rd pair so that it is used */
                     /* in the next first pair test.                 */

                  } /* Else, at end of current scan row. */
               }

               /* Otherwise, 2nd pair failed, so keep pointing to it */
               /* so that it is used in the next first pair test.    */

            } /* Else, at end of current scan row. */
         }
         /* Otherwise, 1st pair failed... */
         else{
            /* Bump forward to next pixel pair. */
            cx++;
         }
      } /* While not at end of current scan row. */
      /* Bump forward to next scan row. */
      cy++;
   } /* While not out of scan rows. */

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: scan4minutiae_horizontally_V2 - Scans an entire binary image
#cat:                horizontally, detecting potential minutiae points.
#cat:                Minutia detected via the horizontal scan process are
#cat:                by nature vertically oriented (orthogonal to the scan).

   Input:
      bdata     - binary image data (0==while & 1==black)
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
      pdirection_map  - pixelized Direction Map
      plow_flow_map   - pixelized Low Ridge Flow Map
      phigh_curve_map - pixelized High Curvature Map
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      minutiae   - points to a list of detected minutia structures
   Return Code:
      Zero      - successful completion
      Negative  - system error
**************************************************************************/
int scan4minutiae_horizontally_V2(MINUTIAE *minutiae,
                unsigned char *bdata, const int iw, const int ih,
                int *pdirection_map, int *plow_flow_map, int *phigh_curve_map,
                const LFSPARMS *lfsparms)
{
   int sx, sy, ex, ey, cx, cy, x2;
   unsigned char *p1ptr, *p2ptr;
   int possible[NFEATURES], nposs;
   int ret;

   /* Set scan region to entire image. */
   sx = 0;
   ex = iw;
   sy = 0;
   ey = ih;

   /* Start at first row in region. */
   cy = sy;
   /* While second scan row not outside the bottom of the scan region... */
   while(cy+1 < ey){
      /* Start at beginning of new scan row in region. */
      cx = sx;
      /* While not at end of region's current scan row. */
      while(cx < ex){
         /* Get pixel pair from current x position in current and next */
         /* scan rows. */
         p1ptr = bdata+(cy*iw)+cx;
         p2ptr = bdata+((cy+1)*iw)+cx;
         /* If scan pixel pair matches first pixel pair of */
         /* 1 or more features... */
         if(match_1st_pair(*p1ptr, *p2ptr, possible, &nposs)){
            /* Bump forward to next scan pixel pair. */
            cx++;
            p1ptr++;
            p2ptr++;
            /* If not at end of region's current scan row... */
            if(cx < ex){
               /* If scan pixel pair matches second pixel pair of */
               /* 1 or more features... */
               if(match_2nd_pair(*p1ptr, *p2ptr, possible, &nposs)){
                  /* Store current x location. */
                  x2 = cx;
                  /* Skip repeated pixel pairs. */
                  skip_repeated_horizontal_pair(&cx, ex, &p1ptr, &p2ptr,
                                                    iw, ih);
                  /* If not at end of region's current scan row... */
                  if(cx < ex){
                     /* If scan pixel pair matches third pixel pair of */
                     /* a single feature... */
                     if(match_3rd_pair(*p1ptr, *p2ptr, possible, &nposs)){
                        /* Process detected minutia point. */
                        if((ret = process_horizontal_scan_minutia_V2(minutiae,
                                         cx, cy, x2, possible[0],
                                         bdata, iw, ih, pdirection_map,
                                         plow_flow_map, phigh_curve_map,
                                         lfsparms))){
                           /* Return code may be:                       */
                           /* 1.  ret< 0 (implying system error)        */
                           /* 2. ret==IGNORE (ignore current feature)   */
                           if(ret < 0)
                              return(ret);
                           /* Otherwise, IGNORE and continue. */
                        }
                     }

                     /* Set up to resume scan. */
                     /* Test to see if 3rd pair can slide into 2nd pair. */
                     /* The values of the 2nd pair MUST be different.    */
                     /* If 3rd pair values are different ... */
                     if(*p1ptr != *p2ptr){
                        /* Set next first pair to last of repeated */
                        /* 2nd pairs, ie. back up one pair.        */
                        cx--;
                     }

                     /* Otherwise, 3rd pair can't be a 2nd pair, so  */
                     /* keep pointing to 3rd pair so that it is used */
                     /* in the next first pair test.                 */

                  } /* Else, at end of current scan row. */
               }

               /* Otherwise, 2nd pair failed, so keep pointing to it */
               /* so that it is used in the next first pair test.    */

            } /* Else, at end of current scan row. */
         }
         /* Otherwise, 1st pair failed... */
         else{
            /* Bump forward to next pixel pair. */
            cx++;
         }
      } /* While not at end of current scan row. */
      /* Bump forward to next scan row. */
      cy++;
   } /* While not out of scan rows. */

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: scan4minutiae_vertically - Scans a specified region of binary image data 
#cat:                vertically, detecting potential minutiae points.
#cat:                Minutia detected via the vetical scan process are
#cat:                by nature horizontally oriented (orthogonal to  the scan).
#cat:                The region actually scanned is slightly larger than that
#cat:                specified.  This overlap attempts to minimize the number
#cat:                of minutiae missed at the region boundaries.
#cat:                HOWEVER, some minutiae will still be missed!

   Input:
      bdata     - binary image data (0==while & 1==black)
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
      imapval   - IMAP value associated with this image region
      nmapval   - NMAP value associated with this image region
      scan_x    - x-pixel coord of origin of region to be scanned
      scan_y    - y-pixel coord of origin of region to be scanned
      scan_w    - width (in pixels) of region to be scanned
      scan_h    - height (in pixels) of region to be scanned
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      minutiae   - points to a list of detected minutia structures
   Return Code:
      Zero      - successful completion
      Negative  - system error
**************************************************************************/
int scan4minutiae_vertically(MINUTIAE *minutiae,
                unsigned char *bdata, const int iw, const int ih,
                const int imapval, const int nmapval,
                const int scan_x, const int scan_y,
                const int scan_w, const int scan_h,
                const LFSPARMS *lfsparms)
{
   int sx, sy, ex, ey, cx, cy, y2;
   unsigned char *p1ptr, *p2ptr;
   int possible[NFEATURES], nposs;
   int ret;

   /* NOTE!!! Minutia that "straddle" region boundaries may be missed! */

   /* If possible, overlap scan region to the right by 1 pixel column. */
   sx = scan_x;
   ex = min(iw, scan_x+scan_w+1);

   /* If possible, overlap top and bottom of current scan region   */
   /* by 2 pixel rows to help catch some minutia that straddle the */
   /* the scan region boundaries.                                  */
   sy = max(0, scan_y-2);
   ey = min(ih, scan_y+scan_h+2);

   /* For now, we will not adjust for IMAP edge, as the binary image */
   /* was properly padded at its edges so as not to cause anomalies. */

   /* Start at first column in region. */
   cx = sx;
   /* While second scan column not outside the right of the region ... */
   while(cx+1 < ex){
      /* Start at beginning of new scan column in region. */
      cy = sy;
      /* While not at end of region's current scan column. */
      while(cy < ey){
         /* Get pixel pair from current y position in current and next */
         /* scan columns. */
         p1ptr = bdata+(cy*iw)+cx;
         p2ptr = p1ptr+1;
         /* If scan pixel pair matches first pixel pair of */
         /* 1 or more features... */
         if(match_1st_pair(*p1ptr, *p2ptr, possible, &nposs)){
            /* Bump forward to next scan pixel pair. */
            cy++;
            p1ptr+=iw;
            p2ptr+=iw;
            /* If not at end of region's current scan column... */
            if(cy < ey){
               /* If scan pixel pair matches second pixel pair of */
               /* 1 or more features... */
               if(match_2nd_pair(*p1ptr, *p2ptr, possible, &nposs)){
                  /* Store current y location. */
                  y2 = cy;
                  /* Skip repeated pixel pairs. */
                  skip_repeated_vertical_pair(&cy, ey, &p1ptr, &p2ptr,
                                                  iw, ih);
                  /* If not at end of region's current scan column... */
                  if(cy < ey){
                     /* If scan pixel pair matches third pixel pair of */
                     /* a single feature... */
                     if(match_3rd_pair(*p1ptr, *p2ptr, possible, &nposs)){
                        /* Process detected minutia point. */
                        if((ret = process_vertical_scan_minutia(minutiae,
                                         cx, cy, y2, possible[0],
                                         bdata, iw, ih,
                                         imapval, nmapval, lfsparms))){
                           /* Return code may be:                       */
                           /* 1.  ret< 0 (implying system error)        */
                           /* 2. ret==IGNORE (ignore current feature)   */
                           if(ret < 0)
                              return(ret);
                           /* Otherwise, IGNORE and continue. */
                        }
                     }

                     /* Set up to resume scan. */
                     /* Test to see if 3rd pair can slide into 2nd pair. */
                     /* The values of the 2nd pair MUST be different.    */
                     /* If 3rd pair values are different ... */
                     if(*p1ptr != *p2ptr){
                        /* Set next first pair to last of repeated */
                        /* 2nd pairs, ie. back up one pair.        */
                        cy--;
                     }

                     /* Otherwise, 3rd pair can't be a 2nd pair, so  */
                     /* keep pointing to 3rd pair so that it is used */
                     /* in the next first pair test.                 */

                  } /* Else, at end of current scan row. */
               }

               /* Otherwise, 2nd pair failed, so keep pointing to it */
               /* so that it is used in the next first pair test.    */

            } /* Else, at end of current scan column. */
         }
         /* Otherwise, 1st pair failed... */
         else{
            /* Bump forward to next pixel pair. */
            cy++;
         }
      } /* While not at end of current scan column. */
      /* Bump forward to next scan column. */
      cx++;
   } /* While not out of scan columns. */

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: scan4minutiae_vertically_V2 - Scans an entire binary image
#cat:                vertically, detecting potential minutiae points.
#cat:                Minutia detected via the vetical scan process are
#cat:                by nature horizontally oriented (orthogonal to  the scan).

   Input:
      bdata     - binary image data (0==while & 1==black)
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
      pdirection_map  - pixelized Direction Map
      plow_flow_map   - pixelized Low Ridge Flow Map
      phigh_curve_map - pixelized High Curvature Map
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      minutiae   - points to a list of detected minutia structures
   Return Code:
      Zero      - successful completion
      Negative  - system error
**************************************************************************/
int scan4minutiae_vertically_V2(MINUTIAE *minutiae,
                unsigned char *bdata, const int iw, const int ih,
                int *pdirection_map, int *plow_flow_map, int *phigh_curve_map,
                const LFSPARMS *lfsparms)
{
   int sx, sy, ex, ey, cx, cy, y2;
   unsigned char *p1ptr, *p2ptr;
   int possible[NFEATURES], nposs;
   int ret;

   /* Set scan region to entire image. */
   sx = 0;
   ex = iw;
   sy = 0;
   ey = ih;

   /* Start at first column in region. */
   cx = sx;
   /* While second scan column not outside the right of the region ... */
   while(cx+1 < ex){
      /* Start at beginning of new scan column in region. */
      cy = sy;
      /* While not at end of region's current scan column. */
      while(cy < ey){
         /* Get pixel pair from current y position in current and next */
         /* scan columns. */
         p1ptr = bdata+(cy*iw)+cx;
         p2ptr = p1ptr+1;
         /* If scan pixel pair matches first pixel pair of */
         /* 1 or more features... */
         if(match_1st_pair(*p1ptr, *p2ptr, possible, &nposs)){
            /* Bump forward to next scan pixel pair. */
            cy++;
            p1ptr+=iw;
            p2ptr+=iw;
            /* If not at end of region's current scan column... */
            if(cy < ey){
               /* If scan pixel pair matches second pixel pair of */
               /* 1 or more features... */
               if(match_2nd_pair(*p1ptr, *p2ptr, possible, &nposs)){
                  /* Store current y location. */
                  y2 = cy;
                  /* Skip repeated pixel pairs. */
                  skip_repeated_vertical_pair(&cy, ey, &p1ptr, &p2ptr,
                                                  iw, ih);
                  /* If not at end of region's current scan column... */
                  if(cy < ey){
                     /* If scan pixel pair matches third pixel pair of */
                     /* a single feature... */
                     if(match_3rd_pair(*p1ptr, *p2ptr, possible, &nposs)){
                        /* Process detected minutia point. */
                        if((ret = process_vertical_scan_minutia_V2(minutiae,
                                         cx, cy, y2, possible[0],
                                         bdata, iw, ih, pdirection_map,
                                         plow_flow_map, phigh_curve_map,
                                         lfsparms))){
                           /* Return code may be:                       */
                           /* 1.  ret< 0 (implying system error)        */
                           /* 2. ret==IGNORE (ignore current feature)   */
                           if(ret < 0)
                              return(ret);
                           /* Otherwise, IGNORE and continue. */
                        }
                     }

                     /* Set up to resume scan. */
                     /* Test to see if 3rd pair can slide into 2nd pair. */
                     /* The values of the 2nd pair MUST be different.    */
                     /* If 3rd pair values are different ... */
                     if(*p1ptr != *p2ptr){
                        /* Set next first pair to last of repeated */
                        /* 2nd pairs, ie. back up one pair.        */
                        cy--;
                     }

                     /* Otherwise, 3rd pair can't be a 2nd pair, so  */
                     /* keep pointing to 3rd pair so that it is used */
                     /* in the next first pair test.                 */

                  } /* Else, at end of current scan row. */
               }

               /* Otherwise, 2nd pair failed, so keep pointing to it */
               /* so that it is used in the next first pair test.    */

            } /* Else, at end of current scan column. */
         }
         /* Otherwise, 1st pair failed... */
         else{
            /* Bump forward to next pixel pair. */
            cy++;
         }
      } /* While not at end of current scan column. */
      /* Bump forward to next scan column. */
      cx++;
   } /* While not out of scan columns. */

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: rescan4minutiae_horizontally - Rescans portions of a block of binary
#cat:                image data horizontally for potential minutiae.  The areas
#cat:                rescanned within the block are based on the current
#cat:                block's neighboring blocks' IMAP and NMAP values.

   Input:
      bdata     - binary image data (0==while & 1==black)
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
      imap      - matrix of ridge flow directions
      nmap      - IMAP augmented with blocks of HIGH-CURVATURE and
                  blocks which have no neighboring valid directions.
      blk_x     - x-block coord to be rescanned
      blk_y     - y-block coord to be rescanned
      mw        - width (in blocks) of IMAP and NMAP matrices.
      mh        - height (in blocks) of IMAP and NMAP matrices.
      scan_x    - x-pixel coord of origin of region to be rescanned
      scan_y    - y-pixel coord of origin of region to be rescanned
      scan_w    - width (in pixels) of region to be rescanned
      scan_h    - height (in pixels) of region to be rescanned
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      minutiae   - points to a list of detected minutia structures
   Return Code:
      Zero      - successful completion
      Negative  - system error
**************************************************************************/
int rescan4minutiae_horizontally(MINUTIAE *minutiae,
                      unsigned char *bdata, const int iw, const int ih,
                      const int *imap, const int *nmap,
                      const int blk_x, const int blk_y,
                      const int mw, const int mh,
                      const int scan_x, const int scan_y,
                      const int scan_w, const int scan_h,
                      const LFSPARMS *lfsparms)
{
   int blk_i, ret;

   /* Compute block index from block coordinates. */
   blk_i = (blk_y*mw)+blk_x;

   /* If high-curve block... */
   if(nmap[blk_i] == HIGH_CURVATURE){
      /* Rescan entire block in orthogonal direction. */
      if((ret = scan4minutiae_horizontally(minutiae, bdata, iw, ih,
                            imap[blk_i], nmap[blk_i],
                            scan_x, scan_y, scan_w, scan_h, lfsparms)))
         /* Return code may be:                      */
         /* 1. ret<0 (implying system error)          */
         return(ret);
   }
   /* Otherwise, block is low-curvature. */
   else{
      /* 1. Rescan horizontally to the North. */
      if((ret = rescan_partial_horizontally(NORTH, minutiae, bdata, iw, ih,
                              imap, nmap, blk_x, blk_y, mw, mh,
                              scan_x, scan_y, scan_w, scan_h, lfsparms)))
         /* Return code may be:                      */
         /* 1. ret<0 (implying system error)          */
         return(ret);

      /* 2. Rescan horizontally to the East. */
      if((ret = rescan_partial_horizontally(EAST, minutiae, bdata, iw, ih,
                              imap, nmap, blk_x, blk_y, mw, mh,
                              scan_x, scan_y, scan_w, scan_h, lfsparms)))
         return(ret);

      /* 3. Rescan horizontally to the South. */
      if((ret = rescan_partial_horizontally(SOUTH, minutiae, bdata, iw, ih,
                              imap, nmap, blk_x, blk_y, mw, mh,
                              scan_x, scan_y, scan_w, scan_h, lfsparms)))
         return(ret);

      /* 4. Rescan horizontally to the West. */
      if((ret = rescan_partial_horizontally(WEST, minutiae, bdata, iw, ih,
                              imap, nmap, blk_x, blk_y, mw, mh,
                              scan_x, scan_y, scan_w, scan_h, lfsparms)))
         return(ret);
   } /* End low-curvature rescan. */

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: rescan4minutiae_vertically - Rescans portions of a block of binary
#cat:                image data vertically for potential minutiae.  The areas
#cat:                rescanned within the block are based on the current
#cat:                block's neighboring blocks' IMAP and NMAP values.

   Input:
      bdata     - binary image data (0==while & 1==black)
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
      imap      - matrix of ridge flow directions
      nmap      - IMAP augmented with blocks of HIGH-CURVATURE and
                  blocks which have no neighboring valid directions.
      blk_x     - x-block coord to be rescanned
      blk_y     - y-block coord to be rescanned
      mw        - width (in blocks) of IMAP and NMAP matrices.
      mh        - height (in blocks) of IMAP and NMAP matrices.
      scan_x    - x-pixel coord of origin of region to be rescanned
      scan_y    - y-pixel coord of origin of region to be rescanned
      scan_w    - width (in pixels) of region to be rescanned
      scan_h    - height (in pixels) of region to be rescanned
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      minutiae   - points to a list of detected minutia structures
   Return Code:
      Zero      - successful completion
      Negative  - system error
**************************************************************************/
int rescan4minutiae_vertically(MINUTIAE *minutiae,
                      unsigned char *bdata, const int iw, const int ih,
                      const int *imap, const int *nmap,
                      const int blk_x, const int blk_y,
                      const int mw, const int mh,
                      const int scan_x, const int scan_y,
                      const int scan_w, const int scan_h,
                      const LFSPARMS *lfsparms)
{
   int blk_i, ret;

   /* Compute block index from block coordinates. */
   blk_i = (blk_y*mw)+blk_x;

   /* If high-curve block... */
   if(nmap[blk_i] == HIGH_CURVATURE){
      /* Rescan entire block in orthogonal direction. */
      if((ret = scan4minutiae_vertically(minutiae, bdata, iw, ih,
                            imap[blk_i], nmap[blk_i],
                            scan_x, scan_y, scan_w, scan_h, lfsparms)))
         /* Return code may be:                      */
         /* 1. ret<0 (implying system error)          */
         return(ret);
   }
   /* Otherwise, block is low-curvature. */
   else{
      /* 1. Rescan vertically to the North. */
      if((ret = rescan_partial_vertically(NORTH, minutiae, bdata, iw, ih,
                              imap, nmap, blk_x, blk_y, mw, mh,
                              scan_x, scan_y, scan_w, scan_h, lfsparms)))
         /* Return code may be:                      */
         /* 1. ret<0 (implying system error)          */
         return(ret);

      /* 2. Rescan vertically to the East. */
      if((ret = rescan_partial_vertically(EAST, minutiae, bdata, iw, ih,
                              imap, nmap, blk_x, blk_y, mw, mh,
                              scan_x, scan_y, scan_w, scan_h, lfsparms)))
         return(ret);

      /* 3. Rescan vertically to the South. */
      if((ret = rescan_partial_vertically(SOUTH, minutiae, bdata, iw, ih,
                              imap, nmap, blk_x, blk_y, mw, mh,
                              scan_x, scan_y, scan_w, scan_h, lfsparms)))
         return(ret);

      /* 4. Rescan vertically to the West. */
      if((ret = rescan_partial_vertically(WEST, minutiae, bdata, iw, ih,
                              imap, nmap, blk_x, blk_y, mw, mh,
                              scan_x, scan_y, scan_w, scan_h, lfsparms)))
         return(ret);
   } /* End low-curvature rescan. */

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: rescan_partial_horizontally - Rescans a portion of a block of binary
#cat:                image data horizontally based on the IMAP and NMAP values
#cat:                of a specified neighboring block.

   Input:
      nbr_dir   - specifies which block neighbor {NORTH, SOUTH, EAST, WEST}
      bdata     - binary image data (0==while & 1==black)
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
      imap      - matrix of ridge flow directions
      nmap      - IMAP augmented with blocks of HIGH-CURVATURE and
                  blocks which have no neighboring valid directions.
      blk_x     - x-block coord to be rescanned
      blk_y     - y-block coord to be rescanned
      mw        - width (in blocks) of IMAP and NMAP matrices.
      mh        - height (in blocks) of IMAP and NMAP matrices.
      scan_x    - x-pixel coord of origin of image region
      scan_y    - y-pixel coord of origin of image region
      scan_w    - width (in pixels) of image region
      scan_h    - height (in pixels) of image region
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      minutiae   - points to a list of detected minutia structures
   Return Code:
      Zero      - successful completion
      Negative  - system error
**************************************************************************/
int rescan_partial_horizontally(const int nbr_dir, MINUTIAE *minutiae,
                      unsigned char *bdata, const int iw, const int ih,
                      const int *imap, const int *nmap,
                      const int blk_x, const int blk_y,
                      const int mw, const int mh,
                      const int scan_x, const int scan_y,
                      const int scan_w, const int scan_h,
                      const LFSPARMS *lfsparms)
{
   int nblk_i, blk_i;
   int rescan_dir;
   int rescan_x, rescan_y, rescan_w, rescan_h;
   int ret;

   /* Neighbor will either be NORTH, SOUTH, EAST, OR WEST. */
   ret = get_nbr_block_index(&nblk_i, nbr_dir, blk_x, blk_y, mw, mh);
   /* Will return:                               */
   /*    1. Neighbor index found == FOUND        */
   /*    2. Neighbor not found == NOT_FOUND      */
   /*    3. System error < 0                     */

   /* If system error ... */
   if(ret < 0)
      /* Return the error code. */
      return(ret);

   /* If neighbor not found ... */
   if(ret == NOT_FOUND)
      /* Nothing to do, so return normally. */
      return(0);

   /* Otherwise, neighboring block found ... */

   /* If neighbor block is VALID... */
   if(imap[nblk_i] != INVALID_DIR){

      /* Compute block index from current (not neighbor) block coordinates. */
      blk_i = (blk_y*mw)+blk_x;

      /* Select feature scan direction based on neighbor IMAP. */
      rescan_dir = choose_scan_direction(imap[nblk_i],
                                         lfsparms->num_directions);
      /* If new scan direction is HORIZONTAL... */
      if(rescan_dir == SCAN_HORIZONTAL){
         /* Adjust scan_x, scan_y, scan_w, scan_h for rescan. */
         if((ret = adjust_horizontal_rescan(nbr_dir, &rescan_x, &rescan_y,
                         &rescan_w, &rescan_h,
                         scan_x, scan_y, scan_w, scan_h, lfsparms->blocksize)))
            /* Return system error code. */
            return(ret);
         /* Rescan specified region in block vertically. */
         /* Pass IMAP direction for the block, NOT its neighbor. */
         if((ret = scan4minutiae_horizontally(minutiae, bdata, iw, ih,
                         imap[blk_i], nmap[blk_i],
                         rescan_x, rescan_y, rescan_w, rescan_h, lfsparms)))
            /* Return code may be:                      */
            /* 1. ret<0 (implying system error)          */
            return(ret);
      } /* Otherwise, block has already been scanned vertically. */
   } /* Otherwise, neighbor has INVALID IMAP, so ignore rescan. */

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: rescan_partial_vertically - Rescans a portion of a block of binary
#cat:                image data vertically based on the IMAP and NMAP values
#cat:                of a specified neighboring block.

   Input:
      nbr_dir   - specifies which block neighbor {NORTH, SOUTH, EAST, WEST}
      bdata     - binary image data (0==while & 1==black)
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
      imap      - matrix of ridge flow directions
      nmap      - IMAP augmented with blocks of HIGH-CURVATURE and
                  blocks which have no neighboring valid directions.
      blk_x     - x-block coord to be rescanned
      blk_y     - y-block coord to be rescanned
      mw        - width (in blocks) of IMAP and NMAP matrices.
      mh        - height (in blocks) of IMAP and NMAP matrices.
      scan_x    - x-pixel coord of origin of image region
      scan_y    - y-pixel coord of origin of image region
      scan_w    - width (in pixels) of image region
      scan_h    - height (in pixels) of image region
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      minutiae   - points to a list of detected minutia structures
   Return Code:
      Zero      - successful completion
      Negative  - system error
**************************************************************************/
int rescan_partial_vertically(const int nbr_dir, MINUTIAE *minutiae,
                      unsigned char *bdata, const int iw, const int ih,
                      const int *imap, const int *nmap,
                      const int blk_x, const int blk_y,
                      const int mw, const int mh,
                      const int scan_x, const int scan_y,
                      const int scan_w, const int scan_h,
                      const LFSPARMS *lfsparms)
{
   int nblk_i, blk_i;
   int rescan_dir;
   int rescan_x, rescan_y, rescan_w, rescan_h;
   int ret;

   /* Neighbor will either be NORTH, SOUTH, EAST, OR WEST. */
   ret = get_nbr_block_index(&nblk_i, nbr_dir, blk_x, blk_y, mw, mh);
   /* Will return:                               */
   /*    1. Neighbor index found == FOUND        */
   /*    2. Neighbor not found == NOT_FOUND      */
   /*    3. System error < 0                     */

   /* If system error ... */
   if(ret < 0)
      /* Return the error code. */
      return(ret);

   /* If neighbor not found ... */
   if(ret == NOT_FOUND)
      /* Nothing to do, so return normally. */
      return(0);

   /* Otherwise, neighboring block found ... */

   /* If neighbor block is VALID... */
   if(imap[nblk_i] != INVALID_DIR){

      /* Compute block index from current (not neighbor) block coordinates. */
      blk_i = (blk_y*mw)+blk_x;

      /* Select feature scan direction based on neighbor IMAP. */
      rescan_dir = choose_scan_direction(imap[nblk_i],
                                         lfsparms->num_directions);
      /* If new scan direction is VERTICAL... */
      if(rescan_dir == SCAN_VERTICAL){
         /* Adjust scan_x, scan_y, scan_w, scan_h for rescan. */
         if((ret = adjust_vertical_rescan(nbr_dir, &rescan_x, &rescan_y,
                         &rescan_w, &rescan_h,
                         scan_x, scan_y, scan_w, scan_h, lfsparms->blocksize)))
            /* Return system error code. */
            return(ret);
         /* Rescan specified region in block vertically. */
         /* Pass IMAP direction for the block, NOT its neighbor. */
         if((ret = scan4minutiae_vertically(minutiae, bdata, iw, ih,
                         imap[blk_i], nmap[blk_i],
                         rescan_x, rescan_y, rescan_w, rescan_h, lfsparms)))
            /* Return code may be:                      */
            /* 1. ret<0 (implying system error)          */
            return(ret);
      } /* Otherwise, block has already been scanned horizontally. */
   } /* Otherwise, neighbor has INVALID IMAP, so ignore rescan. */

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: get_nbr_block_index - Determines the block index (if one exists)
#cat:                for a specified neighbor of a block in the image.

   Input:
      nbr_dir   - specifies which block neighbor {NORTH, SOUTH, EAST, WEST}
      blk_x     - x-block coord to find neighbor of
      blk_y     - y-block coord to find neighbor of
      mw        - width (in blocks) of IMAP and NMAP matrices.
      mh        - height (in blocks) of IMAP and NMAP matrices.
   Output:
      oblk_i    - points to neighbor's block index
   Return Code:
      NOT_FOUND - neighbor index does not exist
      FOUND     - neighbor index exists and returned
      Negative  - system error
**************************************************************************/
int get_nbr_block_index(int *oblk_i, const int nbr_dir,
            const int blk_x, const int blk_y, const int mw, const int mh)
{
   int nx, ny, ni;

   switch(nbr_dir){
      case NORTH:
         /* If neighbor doesn't exist above... */
         if((ny = blk_y-1) < 0)
            /* Done, so return normally. */
            return(NOT_FOUND);
         /* Get neighbor's block index. */
         ni = (ny*mw)+blk_x;
         break;
      case EAST:
         /* If neighbor doesn't exist to the right... */
         if((nx = blk_x+1) >= mw)
            /* Done, so return normally. */
            return(NOT_FOUND);
         /* Get neighbor's block index. */
         ni = (blk_y*mw)+nx;
         break;
      case SOUTH:
         /* If neighbor doesn't exist below... */
         if((ny = blk_y+1) >= mh)
            /* Return normally. */
            return(NOT_FOUND);
         /* Get neighbor's block index. */
         ni = (ny*mw)+blk_x;
         break;
      case WEST:
         /* If neighbor doesn't exist to the left... */
         if((nx = blk_x-1) < 0)
            /* Return normally. */
            return(NOT_FOUND);
         /* Get neighbor's block index. */
         ni = (blk_y*mw)+nx;
         break;
      default:
         fprintf(stderr,
         "ERROR : get_nbr_block_index : illegal neighbor direction\n");
          return(-200);
   }

   /* Assign output pointer. */
   *oblk_i = ni;

   /* Return neighbor FOUND. */
   return(FOUND);
}

/*************************************************************************
**************************************************************************
#cat: adjust_horizontal_rescan - Determines the portion of an image block to
#cat:                be rescanned horizontally based on a specified neighbor.

   Input:
      nbr_dir   - specifies which block neighbor {NORTH, SOUTH, EAST, WEST}
      scan_x    - x-pixel coord of origin of image region
      scan_y    - y-pixel coord of origin of image region
      scan_w    - width (in pixels) of image region
      scan_h    - height (in pixels) of image region
      blocksize - dimension of image blocks (in pixels)
   Output:
      rescan_x    - x-pixel coord of origin of region to be rescanned
      rescan_y    - y-pixel coord of origin of region to be rescanned
      rescan_w    - width (in pixels) of region to be rescanned
      rescan_h    - height (in pixels) of region to be rescanned
   Return Code:
      Zero      - successful completion
      Negative  - system error
**************************************************************************/
int adjust_horizontal_rescan(const int nbr_dir, int *rescan_x, int *rescan_y,
            int *rescan_w, int *rescan_h, const int scan_x, const int scan_y,
            const int scan_w, const int scan_h, const int blocksize)
{
   int half_blocksize, qtr_blocksize;

   /* Compute half of blocksize. */
   half_blocksize = blocksize>>1;
   /* Compute quarter of blocksize. */
   qtr_blocksize = blocksize>>2;

   /* Neighbor will either be NORTH, SOUTH, EAST, OR WEST. */
   switch(nbr_dir){
      case NORTH:
         /*
               *************************
               *     RESCAN NORTH      *
               *        AREA           *
               *************************
               |                       |
               |                       |
               |                       |
               |                       |
               |                       |
               |                       |
               -------------------------
         */
         /* Rescan origin stays the same. */
         *rescan_x = scan_x;
         *rescan_y = scan_y;
         /* Rescan width stays the same. */
         *rescan_w = scan_w;
         /* Rescan height is reduced to "qtr_blocksize" */
         /* if scan_h is larger.                         */
         *rescan_h = min(qtr_blocksize, scan_h);
         break;
      case EAST:
         /*
               ------------*************
               |           *           *
               |           *           *
               |           *    E R    *
               |           *    A E    *
               |           *    S S    *
               |           *    T C    *
               |           *      A    *
               |           *      N    *
               |           *           *
               |           *           *
               ------------*************
         */
         /* Rescan x-orign is set to half_blocksize from right edge of */
         /* block if scan width is larger.                            */
         *rescan_x = max(scan_x+scan_w-half_blocksize, scan_x);
         /* Rescan y-origin stays the same. */
         *rescan_y = scan_y;
         /* Rescan width is reduced to "half_blocksize" */
         /* if scan width is larger.                   */
         *rescan_w = min(half_blocksize, scan_w);
         /* Rescan height stays the same. */
         *rescan_h = scan_h;
         break;
      case SOUTH:
         /*
               -------------------------
               |                       |
               |                       |
               |                       |
               |                       |
               |                       |
               |                       |
               *************************
               *     RESCAN SOUTH      *
               *        AREA           *
               *************************
         */
         /* Rescan x-origin stays the same. */
         *rescan_x = scan_x;
         /* Rescan y-orign is set to qtr_blocksize from bottom edge of */
         /* block if scan height is larger.                             */
         *rescan_y = max(scan_y+scan_h-qtr_blocksize, scan_y);
         /* Rescan width stays the same. */
         *rescan_w = scan_w;
         /* Rescan height is reduced to "qtr_blocksize" */
         /* if scan height is larger.                    */
         *rescan_h = min(qtr_blocksize, scan_h);
         break;
      case WEST:
         /*
               *************------------
               *           *           |
               *           *           |
               *    W R    *           |
               *    E E    *           |
               *    S S    *           |
               *    T C    *           |
               *      A    *           |
               *      N    *           |
               *           *           |
               *           *           |
               *************------------
         */
         /* Rescan origin stays the same. */
         *rescan_x = scan_x;
         *rescan_y = scan_y;
         /* Rescan width is reduced to "half_blocksize" */
         /* if scan width is larger.                   */
         *rescan_w = min(half_blocksize, scan_w);
         /* Rescan height stays the same. */
         *rescan_h = scan_h;
         break;
      default:
         fprintf(stderr,
         "ERROR : adjust_horizontal_rescan : illegal neighbor direction\n");
          return(-210);
   }

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: adjust_vertical_rescan - Determines the portion of an image block to
#cat:                be rescanned vertically based on a specified neighbor.

   Input:
      nbr_dir   - specifies which block neighbor {NORTH, SOUTH, EAST, WEST}
      scan_x    - x-pixel coord of origin of image region
      scan_y    - y-pixel coord of origin of image region
      scan_w    - width (in pixels) of image region
      scan_h    - height (in pixels) of image region
      blocksize - dimension of image blocks (in pixels)
   Output:
      rescan_x    - x-pixel coord of origin of region to be rescanned
      rescan_y    - y-pixel coord of origin of region to be rescanned
      rescan_w    - width (in pixels) of region to be rescanned
      rescan_h    - height (in pixels) of region to be rescanned
   Return Code:
      Zero      - successful completion
      Negative  - system error
**************************************************************************/
int adjust_vertical_rescan(const int nbr_dir, int *rescan_x, int *rescan_y,
            int *rescan_w, int *rescan_h, const int scan_x, const int scan_y,
            const int scan_w, const int scan_h, const int blocksize)
{
   int half_blocksize, qtr_blocksize;

   /* Compute half of blocksize. */
   half_blocksize = blocksize>>1;
   /* Compute quarter of blocksize. */
   qtr_blocksize = blocksize>>2;

   /* Neighbor will either be NORTH, SOUTH, EAST, OR WEST. */
   switch(nbr_dir){
      case NORTH:
         /*
               *************************
               *                       *
               *     RESCAN NORTH      *
               *        AREA           *
               *                       *
               *************************
               |                       |
               |                       |
               |                       |
               |                       |
               |                       |
               -------------------------
         */
         /* Rescan origin stays the same. */
         *rescan_x = scan_x;
         *rescan_y = scan_y;
         /* Rescan width stays the same. */
         *rescan_w = scan_w;
         /* Rescan height is reduced to "half_blocksize" */
         /* if scan_h is larger.                         */
         *rescan_h = min(half_blocksize, scan_h);
         break;
      case EAST:
         /*
               ------------------*******
               |                 *     *
               |                 *     *
               |                 * E R *
               |                 * A E *
               |                 * S S *
               |                 * T C *
               |                 *   A *
               |                 *   N *
               |                 *     *
               |                 *     *
               ------------------*******
         */
         /* Rescan x-orign is set to qtr_blocksize from right edge of */
         /* block if scan width is larger.                            */
         *rescan_x = max(scan_x+scan_w-qtr_blocksize, scan_x);
         /* Rescan y-origin stays the same. */
         *rescan_y = scan_y;
         /* Rescan width is reduced to "qtr_blocksize" */
         /* if scan width is larger.                   */
         *rescan_w = min(qtr_blocksize, scan_w);
         /* Rescan height stays the same. */
         *rescan_h = scan_h;
         break;
      case SOUTH:
         /*
               -------------------------
               |                       |
               |                       |
               |                       |
               |                       |
               |                       |
               *************************
               *                       *
               *     RESCAN SOUTH      *
               *        AREA           *
               *                       *
               *************************
         */
         /* Rescan x-origin stays the same. */
         *rescan_x = scan_x;
         /* Rescan y-orign is set to half_blocksize from bottom edge of */
         /* block if scan height is larger.                             */
         *rescan_y = max(scan_y+scan_h-half_blocksize, scan_y);
         /* Rescan width stays the same. */
         *rescan_w = scan_w;
         /* Rescan height is reduced to "half_blocksize" */
         /* if scan height is larger.                    */
         *rescan_h = min(half_blocksize, scan_h);
         break;
      case WEST:
         /*
               *******------------------
               *     *                 |
               *     *                 |
               * W R *                 |
               * E E *                 |
               * S S *                 |
               * T C *                 |
               *   A *                 |
               *   N *                 |
               *     *                 |
               *     *                 |
               *******------------------
         */
         /* Rescan origin stays the same. */
         *rescan_x = scan_x;
         *rescan_y = scan_y;
         /* Rescan width is reduced to "qtr_blocksize" */
         /* if scan width is larger.                   */
         *rescan_w = min(qtr_blocksize, scan_w);
         /* Rescan height stays the same. */
         *rescan_h = scan_h;
         break;
      default:
         fprintf(stderr,
         "ERROR : adjust_vertical_rescan : illegal neighbor direction\n");
          return(-220);
   }

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: process_horizontal_scan_minutia - Takes a minutia point that was
#cat:                detected via the horizontal scan process and
#cat:                adjusts its location (if necessary), determines its
#cat:                direction, and (if it is not already in the minutiae
#cat:                list) adds it to the list.  These minutia are by nature
#cat:                vertical in orientation (orthogonal to the scan).

   Input:
      cx        - x-pixel coord where 3rd pattern pair of mintuia was detected
      cy        - y-pixel coord where 3rd pattern pair of mintuia was detected
      y2        - y-pixel coord where 2nd pattern pair of mintuia was detected
      feature_id - type of minutia (ex. index into feature_patterns[] list)
      bdata     - binary image data (0==while & 1==black)
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
      imapval   - IMAP value associated with this image region
      nmapval   - NMAP value associated with this image region
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      minutiae   - points to a list of detected minutia structures
   Return Code:
      Zero      - successful completion
      IGNORE    - minutia is to be ignored
      Negative  - system error
**************************************************************************/
int process_horizontal_scan_minutia(MINUTIAE *minutiae,
                    const int cx, const int cy,
                    const int x2, const int feature_id,
                    unsigned char *bdata, const int iw, const int ih,
                    const int imapval, const int nmapval,
                    const LFSPARMS *lfsparms)
{
   MINUTIA *minutia;
   int x_loc, y_loc;
   int x_edge, y_edge;
   int idir, ret;

   /* Set x location of minutia point to be half way between */
   /* first position of second feature pair and position of  */
   /* third feature pair.                                    */
   x_loc = (cx + x2)>>1;

   /* Set same x location to neighboring edge pixel. */
   x_edge = x_loc;

   /* Feature location should always point to either ending  */
   /* of ridge or (for bifurcations) ending of valley.       */
   /* So, if detected feature is APPEARING...                */ 
   if(g_feature_patterns[feature_id].appearing){
      /* Set y location to second scan row. */
      y_loc = cy+1;
      /* Set y location of neighboring edge pixel to the first scan row. */
      y_edge = cy;
   }
   /* Otherwise, feature is DISAPPEARING... */
   else{
      /* Set y location to first scan row. */
      y_loc = cy;
      /* Set y location of neighboring edge pixel to the second scan row. */
      y_edge = cy+1;
   }

   /* If current minutia is in a high-curvature block... */
   if(nmapval == HIGH_CURVATURE){
      /* Adjust location and direction locally. */
      if((ret = adjust_high_curvature_minutia(&idir, &x_loc, &y_loc,
                           &x_edge, &y_edge, x_loc, y_loc, x_edge, y_edge,
                           bdata, iw, ih, minutiae, lfsparms))){
         /* Could be a system error or IGNORE minutia. */
         return(ret);
      }
      /* Otherwise, we have our high-curvature minutia attributes. */
   }
   /* Otherwise, minutia is in fairly low-curvature block... */
   else{
      /* Get minutia direction based on current IMAP value. */
      idir = get_low_curvature_direction(SCAN_HORIZONTAL,
                     g_feature_patterns[feature_id].appearing,
                     imapval, lfsparms->num_directions);
   }

   /* Create a minutia object based on derived attributes. */
   if((ret = create_minutia(&minutia, x_loc, y_loc, x_edge, y_edge, idir,
                     DEFAULT_RELIABILITY,
                     g_feature_patterns[feature_id].type,
                     g_feature_patterns[feature_id].appearing, feature_id)))
      /* Return system error. */
      return(ret);

   /* Update the minutiae list with potential new minutia. */
   ret = update_minutiae(minutiae, minutia, bdata, iw, ih, lfsparms);

   /* If minuitia IGNORED and not added to the minutia list ... */
   if(ret == IGNORE)
      /* Deallocate the minutia. */
      free_minutia(minutia);

   /* Otherwise, return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: process_horizontal_scan_minutia_V2 - Takes a minutia point that was
#cat:                detected via the horizontal scan process and
#cat:                adjusts its location (if necessary), determines its
#cat:                direction, and (if it is not already in the minutiae
#cat:                list) adds it to the list.  These minutia are by nature
#cat:                vertical in orientation (orthogonal to the scan).

   Input:
      cx        - x-pixel coord where 3rd pattern pair of mintuia was detected
      cy        - y-pixel coord where 3rd pattern pair of mintuia was detected
      y2        - y-pixel coord where 2nd pattern pair of mintuia was detected
      feature_id - type of minutia (ex. index into feature_patterns[] list)
      bdata     - binary image data (0==while & 1==black)
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
      pdirection_map  - pixelized Direction Map
      plow_flow_map   - pixelized Low Ridge Flow Map
      phigh_curve_map - pixelized High Curvature Map
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      minutiae   - points to a list of detected minutia structures
   Return Code:
      Zero      - successful completion
      IGNORE    - minutia is to be ignored
      Negative  - system error
**************************************************************************/
int process_horizontal_scan_minutia_V2(MINUTIAE *minutiae,
                 const int cx, const int cy,
                 const int x2, const int feature_id,
                 unsigned char *bdata, const int iw, const int ih,
                 int *pdirection_map, int *plow_flow_map, int *phigh_curve_map,
                 const LFSPARMS *lfsparms)
{
   MINUTIA *minutia;
   int x_loc, y_loc;
   int x_edge, y_edge;
   int idir, ret;
   int dmapval, fmapval, cmapval;
   double reliability;

   /* Set x location of minutia point to be half way between */
   /* first position of second feature pair and position of  */
   /* third feature pair.                                    */
   x_loc = (cx + x2)>>1;

   /* Set same x location to neighboring edge pixel. */
   x_edge = x_loc;

   /* Feature location should always point to either ending  */
   /* of ridge or (for bifurcations) ending of valley.       */
   /* So, if detected feature is APPEARING...                */ 
   if(g_feature_patterns[feature_id].appearing){
      /* Set y location to second scan row. */
      y_loc = cy+1;
      /* Set y location of neighboring edge pixel to the first scan row. */
      y_edge = cy;
   }
   /* Otherwise, feature is DISAPPEARING... */
   else{
      /* Set y location to first scan row. */
      y_loc = cy;
      /* Set y location of neighboring edge pixel to the second scan row. */
      y_edge = cy+1;
   }

   dmapval = *(pdirection_map+(y_loc*iw)+x_loc);
   fmapval = *(plow_flow_map+(y_loc*iw)+x_loc);
   cmapval = *(phigh_curve_map+(y_loc*iw)+x_loc);

   /* If the minutia point is in a block with INVALID direction ... */
   if(dmapval == INVALID_DIR)
      /* Then, IGNORE the point. */
      return(IGNORE);

   /* If current minutia is in a HIGH CURVATURE block ... */
   if(cmapval){
      /* Adjust location and direction locally. */
      if((ret = adjust_high_curvature_minutia_V2(&idir, &x_loc, &y_loc,
                           &x_edge, &y_edge, x_loc, y_loc, x_edge, y_edge,
                           bdata, iw, ih, plow_flow_map, minutiae, lfsparms))){
         /* Could be a system error or IGNORE minutia. */
         return(ret);
      }
      /* Otherwise, we have our high-curvature minutia attributes. */
   }
   /* Otherwise, minutia is in fairly low-curvature block... */
   else{
      /* Get minutia direction based on current block's direction. */
      idir = get_low_curvature_direction(SCAN_HORIZONTAL,
                     g_feature_patterns[feature_id].appearing, dmapval,
                     lfsparms->num_directions);
   }

   /* If current minutia is in a LOW RIDGE FLOW block ... */
   if(fmapval)
      reliability = MEDIUM_RELIABILITY;
   else
      /* Otherwise, minutia is in a block with reliable direction and */
      /* binarization.                                                */
      reliability = HIGH_RELIABILITY;

   /* Create a minutia object based on derived attributes. */
   if((ret = create_minutia(&minutia, x_loc, y_loc, x_edge, y_edge, idir,
                     reliability,
                     g_feature_patterns[feature_id].type,
                     g_feature_patterns[feature_id].appearing, feature_id)))
      /* Return system error. */
      return(ret);

   /* Update the minutiae list with potential new minutia. */
   ret = update_minutiae_V2(minutiae, minutia, SCAN_HORIZONTAL,
                            dmapval, bdata, iw, ih, lfsparms);

   /* If minuitia IGNORED and not added to the minutia list ... */
   if(ret == IGNORE)
      /* Deallocate the minutia. */
      free_minutia(minutia);

   /* Otherwise, return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: process_vertical_scan_minutia - Takes a minutia point that was
#cat:                detected in via the vertical scan process and
#cat:                adjusts its location (if necessary), determines its
#cat:                direction, and (if it is not already in the minutiae
#cat:                list) adds it to the list.  These minutia are by nature
#cat:                horizontal in orientation (orthogonal to the scan).

   Input:
      cx        - x-pixel coord where 3rd pattern pair of mintuia was detected
      cy        - y-pixel coord where 3rd pattern pair of mintuia was detected
      x2        - x-pixel coord where 2nd pattern pair of mintuia was detected
      feature_id - type of minutia (ex. index into feature_patterns[] list)
      bdata     - binary image data (0==while & 1==black)
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
      imapval   - IMAP value associated with this image region
      nmapval   - NMAP value associated with this image region
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      minutiae   - points to a list of detected minutia structures
   Return Code:
      Zero      - successful completion
      IGNORE    - minutia is to be ignored
      Negative  - system error
**************************************************************************/
int process_vertical_scan_minutia(MINUTIAE *minutiae,
                    const int cx, const int cy,
                    const int y2, const int feature_id,
                    unsigned char *bdata, const int iw, const int ih,
                    const int imapval, const int nmapval,
                    const LFSPARMS *lfsparms)
{
   MINUTIA *minutia;
   int x_loc, y_loc;
   int x_edge, y_edge;
   int idir, ret;

   /* Feature location should always point to either ending  */
   /* of ridge or (for bifurcations) ending of valley.       */
   /* So, if detected feature is APPEARING...                */ 
   if(g_feature_patterns[feature_id].appearing){
      /* Set x location to second scan column. */
      x_loc = cx+1;
      /* Set x location of neighboring edge pixel to the first scan column. */
      x_edge = cx;
   }
   /* Otherwise, feature is DISAPPEARING... */
   else{
      /* Set x location to first scan column. */
      x_loc = cx;
      /* Set x location of neighboring edge pixel to the second scan column. */
      x_edge = cx+1;
   }

   /* Set y location of minutia point to be half way between */
   /* first position of second feature pair and position of  */
   /* third feature pair.                                    */
   y_loc = (cy + y2)>>1;
   /* Set same y location to neighboring edge pixel. */
   y_edge = y_loc;

   /* If current minutia is in a high-curvature block... */
   if(nmapval == HIGH_CURVATURE){
      /* Adjust location and direction locally. */
      if((ret = adjust_high_curvature_minutia(&idir, &x_loc, &y_loc,
                           &x_edge, &y_edge, x_loc, y_loc, x_edge, y_edge,
                           bdata, iw, ih, minutiae, lfsparms))){
         /* Could be a system error or IGNORE minutia. */
         return(ret);
      }
      /* Otherwise, we have our high-curvature minutia attributes. */  
   }
   /* Otherwise, minutia is in fairly low-curvature block... */
   else{
      /* Get minutia direction based on current IMAP value. */
      idir = get_low_curvature_direction(SCAN_VERTICAL,
                     g_feature_patterns[feature_id].appearing,
                     imapval, lfsparms->num_directions);
   }

   /* Create a minutia object based on derived attributes. */
   if((ret = create_minutia(&minutia, x_loc, y_loc, x_edge, y_edge, idir,
                     DEFAULT_RELIABILITY,
                     g_feature_patterns[feature_id].type,
                     g_feature_patterns[feature_id].appearing, feature_id)))
      /* Return system error. */
      return(ret);

   /* Update the minutiae list with potential new minutia. */
   ret = update_minutiae(minutiae, minutia, bdata, iw, ih, lfsparms);

   /* If minuitia IGNORED and not added to the minutia list ... */
   if(ret == IGNORE)
      /* Deallocate the minutia. */
      free_minutia(minutia);

   /* Otherwise, return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: process_vertical_scan_minutia_V2 - Takes a minutia point that was
#cat:                detected in via the vertical scan process and
#cat:                adjusts its location (if necessary), determines its
#cat:                direction, and (if it is not already in the minutiae
#cat:                list) adds it to the list.  These minutia are by nature
#cat:                horizontal in orientation (orthogonal to the scan).

   Input:
      cx        - x-pixel coord where 3rd pattern pair of mintuia was detected
      cy        - y-pixel coord where 3rd pattern pair of mintuia was detected
      x2        - x-pixel coord where 2nd pattern pair of mintuia was detected
      feature_id - type of minutia (ex. index into feature_patterns[] list)
      bdata     - binary image data (0==while & 1==black)
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
      pdirection_map  - pixelized Direction Map
      plow_flow_map   - pixelized Low Ridge Flow Map
      phigh_curve_map - pixelized High Curvature Map
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      minutiae   - points to a list of detected minutia structures
   Return Code:
      Zero      - successful completion
      IGNORE    - minutia is to be ignored
      Negative  - system error
**************************************************************************/
int process_vertical_scan_minutia_V2(MINUTIAE *minutiae,
                 const int cx, const int cy,
                 const int y2, const int feature_id,
                 unsigned char *bdata, const int iw, const int ih,
                 int *pdirection_map, int *plow_flow_map, int *phigh_curve_map,
                 const LFSPARMS *lfsparms)
{
   MINUTIA *minutia;
   int x_loc, y_loc;
   int x_edge, y_edge;
   int idir, ret;
   int dmapval, fmapval, cmapval;
   double reliability;

   /* Feature location should always point to either ending  */
   /* of ridge or (for bifurcations) ending of valley.       */
   /* So, if detected feature is APPEARING...                */ 
   if(g_feature_patterns[feature_id].appearing){
      /* Set x location to second scan column. */
      x_loc = cx+1;
      /* Set x location of neighboring edge pixel to the first scan column. */
      x_edge = cx;
   }
   /* Otherwise, feature is DISAPPEARING... */
   else{
      /* Set x location to first scan column. */
      x_loc = cx;
      /* Set x location of neighboring edge pixel to the second scan column. */
      x_edge = cx+1;
   }

   /* Set y location of minutia point to be half way between */
   /* first position of second feature pair and position of  */
   /* third feature pair.                                    */
   y_loc = (cy + y2)>>1;
   /* Set same y location to neighboring edge pixel. */
   y_edge = y_loc;

   dmapval = *(pdirection_map+(y_loc*iw)+x_loc);
   fmapval = *(plow_flow_map+(y_loc*iw)+x_loc);
   cmapval = *(phigh_curve_map+(y_loc*iw)+x_loc);

   /* If the minutia point is in a block with INVALID direction ... */
   if(dmapval == INVALID_DIR)
      /* Then, IGNORE the point. */
      return(IGNORE);

   /* If current minutia is in a HIGH CURVATURE block... */
   if(cmapval){
      /* Adjust location and direction locally. */
      if((ret = adjust_high_curvature_minutia_V2(&idir, &x_loc, &y_loc,
                           &x_edge, &y_edge, x_loc, y_loc, x_edge, y_edge,
                           bdata, iw, ih, plow_flow_map, minutiae, lfsparms))){
         /* Could be a system error or IGNORE minutia. */
         return(ret);
      }
      /* Otherwise, we have our high-curvature minutia attributes. */  
   }
   /* Otherwise, minutia is in fairly low-curvature block... */
   else{
      /* Get minutia direction based on current block's direction. */
      idir = get_low_curvature_direction(SCAN_VERTICAL,
                     g_feature_patterns[feature_id].appearing, dmapval,
                     lfsparms->num_directions);
   }

   /* If current minutia is in a LOW RIDGE FLOW block ... */
   if(fmapval)
      reliability = MEDIUM_RELIABILITY;
   else
      /* Otherwise, minutia is in a block with reliable direction and */
      /* binarization.                                                */
      reliability = HIGH_RELIABILITY;

   /* Create a minutia object based on derived attributes. */
   if((ret = create_minutia(&minutia, x_loc, y_loc, x_edge, y_edge, idir,
                     reliability,
                     g_feature_patterns[feature_id].type,
                     g_feature_patterns[feature_id].appearing, feature_id)))
      /* Return system error. */
      return(ret);

   /* Update the minutiae list with potential new minutia. */
   ret = update_minutiae_V2(minutiae, minutia, SCAN_VERTICAL,
                            dmapval, bdata, iw, ih, lfsparms);

   /* If minuitia IGNORED and not added to the minutia list ... */
   if(ret == IGNORE)
      /* Deallocate the minutia. */
      free_minutia(minutia);

   /* Otherwise, return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: adjust_high_curvature_minutia - Takes an initial minutia point detected
#cat:            in a high-curvature area and adjusts its location and
#cat:            direction.  First, it walks and extracts the contour
#cat:            of the detected feature looking for and processing any loop
#cat:            discovered along the way.  Once the contour is extracted,
#cat:            the point of highest-curvature is determined and used to
#cat:            adjust the location of the minutia point.  The angle of
#cat:            the line perpendicular to the tangent on the high-curvature
#cat:            contour at the minutia point is used as the mintutia's
#cat:            direction.

   Input:
      x_loc     - starting x-pixel coord of feature (interior to feature)
      y_loc     - starting y-pixel coord of feature (interior to feature)
      x_edge    - x-pixel coord of corresponding edge pixel
                  (exterior to feature)
      y_edge    - y-pixel coord of corresponding edge pixel
                  (exterior to feature)
      bdata     - binary image data (0==while & 1==black)
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      oidir     - direction of adjusted minutia point
      ox_loc    - adjusted x-pixel coord of feature
      oy_loc    - adjusted y-pixel coord of feature
      ox_edge   - adjusted x-pixel coord of corresponding edge pixel
      oy_edge   - adjusted y-pixel coord of corresponding edge pixel
      minutiae   - points to a list of detected minutia structures
   Return Code:
      Zero      - minutia point processed successfully
      IGNORE    - minutia point is to be ignored
      Negative  - system error
**************************************************************************/
int adjust_high_curvature_minutia(int *oidir, int *ox_loc, int *oy_loc,
              int *ox_edge, int *oy_edge,
              const int x_loc, const int y_loc,
              const int x_edge, const int y_edge,
              unsigned char *bdata, const int iw, const int ih,
              MINUTIAE *minutiae, const LFSPARMS *lfsparms)
{
   int ret;
   int *contour_x, *contour_y, *contour_ex, *contour_ey, ncontour;
   int min_i;
   double min_theta;
   int feature_pix;
   int mid_x, mid_y, mid_pix;
   int idir;
   int half_contour, angle_edge;

   /* Set variable from parameter structure. */
   half_contour = lfsparms->high_curve_half_contour;

   /* Set edge length for computing contour's angle of curvature */
   /* to one quarter of desired pixel length of entire contour.  */
   /* Ex. If half_contour==14, then contour length==29=(2X14)+1  */
   /* and angle_edge==7=(14/2).                                  */
   angle_edge = half_contour>>1;

   /* Get the pixel value of current feature. */
   feature_pix = *(bdata + (y_loc * iw) + x_loc);

   /* Extract feature's contour. */
   if((ret = get_high_curvature_contour(&contour_x, &contour_y,
                         &contour_ex, &contour_ey, &ncontour, half_contour,
                         x_loc, y_loc, x_edge, y_edge, bdata, iw, ih))){
      /* Returns with:                                                    */
      /*    1. Successful or empty contour == 0                           */
      /*       If contour is empty, then contour lists are not allocated. */
      /*    2. Contour forms loop == LOOP_FOUND                           */
      /*    3. Sysetm error < 0                                           */

      /* If the contour forms a loop... */
      if(ret == LOOP_FOUND){

         /* If the order of the contour is clockwise, then the loops's     */
         /* contour pixels are outside the corresponding edge pixels.  We  */
         /* definitely do NOT want to fill based on the feature pixel in   */
         /* this case, because it is OUTSIDE the loop.  For now we will    */
         /* ignore the loop and the minutia that triggered its tracing.    */
         /* It is likely that other minutia on the loop will be            */
         /* detected that create a contour on the "inside" of the loop.    */
         /* There is another issue here that could be addressed ...        */
         /* It seems that many/multiple minutia are often detected within  */
         /* the same loop, which currently requires retracing the loop,    */
         /* locating minutia on opposite ends of the major axis of the     */
         /* loop, and then determining that the minutia have already been  */
         /* entered into the minutiae list upon processing the very first   */
         /* minutia detected in the loop.  There is a lot of redundant     */
         /* work being done here!                                          */
         /* Is_loop_clockwise takes a default value to be returned if the  */
         /* routine is unable to determine the direction of the contour.   */
         /* In this case, we want to IGNORE the loop if we can't tell its  */
         /* direction so that we do not inappropriately fill the loop, so  */
         /* we are passing the default value TRUE.                         */
         if((ret = is_loop_clockwise(contour_x, contour_y, ncontour, TRUE))){
            /* Deallocate contour lists. */
            free_contour(contour_x, contour_y, contour_ex, contour_ey);
            /* If we had a system error... */
            if(ret < 0)
               /* Return the error code. */
               return(ret);
            /* Otherwise, loop is clockwise, so return IGNORE. */
            return(IGNORE);
         }

         /* Otherwise, process the clockwise-ordered contour of the loop */
         /* as it may contain minutia.  If no minutia found, then it is  */
         /* filled in.                                                   */
         ret = process_loop(minutiae, contour_x, contour_y,
                            contour_ex, contour_ey, ncontour,
                            bdata, iw, ih, lfsparms);
         /* Returns with:                              */
         /*    1. Successful processing of loop == 0   */
         /*    2. System error < 0                     */

         /* Deallocate contour lists. */
         free_contour(contour_x, contour_y, contour_ex, contour_ey);

         /* If loop processed successfully ... */
         if(ret == 0)
            /* Then either a minutia pair was extracted or the loop was */
            /* filled.  Either way we want to IGNORE the minutia that   */
            /* started the whole loop processing in the beginning.      */
            return(IGNORE);

         /* Otherwise, there was a system error. */
         /* Return the resulting code.           */
         return(ret);
      }

      /* Otherwise not a loop, so get_high_curvature_contour incurred */
      /* a system error.  Return the error code.                      */
      return(ret);
   }

   /* If contour is empty ... then contour lists were not allocated, so */
   /* simply return IGNORE.  The contour comes back empty when there    */
   /* were not a sufficient number of points found on the contour.      */
   if(ncontour == 0)
      return(IGNORE);

   /* Otherwise, there are contour points to process. */

   /* Given the contour, determine the point of highest curvature */
   /* (ie. forming the minimum angle between contour walls).      */
   if((ret = min_contour_theta(&min_i, &min_theta, angle_edge,
                              contour_x, contour_y, ncontour))){
      /* Deallocate contour lists. */
      free_contour(contour_x, contour_y, contour_ex, contour_ey);
      /* Returns IGNORE or system error.  Either way */
      /* free the contour and return the code.       */
      return(ret);
   }

   /* If the minimum theta found along the contour is too large... */
   if(min_theta >= lfsparms->max_high_curve_theta){
      /* Deallocate contour lists. */
      free_contour(contour_x, contour_y, contour_ex, contour_ey);
      /* Reject the high-curvature minutia, and return IGNORE. */
      return(IGNORE);
   }

   /* Test to see if interior of curvature is OK.  Compute midpoint   */
   /* between left and right points symmetrically distant (angle_edge */
   /* pixels) from the contour's point of minimum theta.              */
   mid_x = (contour_x[min_i-angle_edge] + contour_x[min_i+angle_edge])>>1;
   mid_y = (contour_y[min_i-angle_edge] + contour_y[min_i+angle_edge])>>1;
   mid_pix = *(bdata + (mid_y * iw) + mid_x);
   /* If the interior pixel value is not the same as the feature's... */
   if(mid_pix != feature_pix){
      /* Deallocate contour lists. */
      free_contour(contour_x, contour_y, contour_ex, contour_ey);
      /* Reject the high-curvature minutia and return IGNORE. */
      return(IGNORE);
   }

   /* Compute new direction based on line connecting adjusted feature */
   /* location and the midpoint in the feature's interior.            */
   idir = line2direction(contour_x[min_i], contour_y[min_i],
                         mid_x, mid_y, lfsparms->num_directions);

   /* Set minutia location to minimum theta position on the contour. */
   *oidir = idir;
   *ox_loc = contour_x[min_i];
   *oy_loc = contour_y[min_i];
   *ox_edge = contour_ex[min_i];
   *oy_edge = contour_ey[min_i];

   /* Deallocate contour buffers. */
   free_contour(contour_x, contour_y, contour_ex, contour_ey);

   /*Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: adjust_high_curvature_minutia_V2 - Takes an initial minutia point
#cat:            in a high-curvature area and adjusts its location and
#cat:            direction.  First, it walks and extracts the contour
#cat:            of the detected feature looking for and processing any loop
#cat:            discovered along the way.  Once the contour is extracted,
#cat:            the point of highest-curvature is determined and used to
#cat:            adjust the location of the minutia point.  The angle of
#cat:            the line perpendicular to the tangent on the high-curvature
#cat:            contour at the minutia point is used as the mintutia's
#cat:            direction.

   Input:
      x_loc     - starting x-pixel coord of feature (interior to feature)
      y_loc     - starting y-pixel coord of feature (interior to feature)
      x_edge    - x-pixel coord of corresponding edge pixel
                  (exterior to feature)
      y_edge    - y-pixel coord of corresponding edge pixel
                  (exterior to feature)
      bdata     - binary image data (0==while & 1==black)
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
      plow_flow_map - pixelized Low Ridge Flow Map
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      oidir     - direction of adjusted minutia point
      ox_loc    - adjusted x-pixel coord of feature
      oy_loc    - adjusted y-pixel coord of feature
      ox_edge   - adjusted x-pixel coord of corresponding edge pixel
      oy_edge   - adjusted y-pixel coord of corresponding edge pixel
      minutiae   - points to a list of detected minutia structures
   Return Code:
      Zero      - minutia point processed successfully
      IGNORE    - minutia point is to be ignored
      Negative  - system error
**************************************************************************/
int adjust_high_curvature_minutia_V2(int *oidir, int *ox_loc, int *oy_loc,
              int *ox_edge, int *oy_edge,
              const int x_loc, const int y_loc,
              const int x_edge, const int y_edge,
              unsigned char *bdata, const int iw, const int ih,
              int *plow_flow_map, MINUTIAE *minutiae, const LFSPARMS *lfsparms)
{
   int ret;
   int *contour_x, *contour_y, *contour_ex, *contour_ey, ncontour;
   int min_i;
   double min_theta;
   int feature_pix;
   int mid_x, mid_y, mid_pix;
   int idir;
   int half_contour, angle_edge;

   /* Set variable from parameter structure. */
   half_contour = lfsparms->high_curve_half_contour;

   /* Set edge length for computing contour's angle of curvature */
   /* to one quarter of desired pixel length of entire contour.  */
   /* Ex. If half_contour==14, then contour length==29=(2X14)+1  */
   /* and angle_edge==7=(14/2).                                  */
   angle_edge = half_contour>>1;

   /* Get the pixel value of current feature. */
   feature_pix = *(bdata + (y_loc * iw) + x_loc);

   /* Extract feature's contour. */
   if((ret = get_high_curvature_contour(&contour_x, &contour_y,
                         &contour_ex, &contour_ey, &ncontour, half_contour,
                         x_loc, y_loc, x_edge, y_edge, bdata, iw, ih))){
      /* Returns with:                                                    */
      /*    1. Successful or empty contour == 0                           */
      /*       If contour is empty, then contour lists are not allocated. */
      /*    2. Contour forms loop == LOOP_FOUND                           */
      /*    3. Sysetm error < 0                                           */

      /* If the contour forms a loop... */
      if(ret == LOOP_FOUND){

         /* If the order of the contour is clockwise, then the loops's     */
         /* contour pixels are outside the corresponding edge pixels.  We  */
         /* definitely do NOT want to fill based on the feature pixel in   */
         /* this case, because it is OUTSIDE the loop.  For now we will    */
         /* ignore the loop and the minutia that triggered its tracing.    */
         /* It is likely that other minutia on the loop will be            */
         /* detected that create a contour on the "inside" of the loop.    */
         /* There is another issue here that could be addressed ...        */
         /* It seems that many/multiple minutia are often detected within  */
         /* the same loop, which currently requires retracing the loop,    */
         /* locating minutia on opposite ends of the major axis of the     */
         /* loop, and then determining that the minutia have already been  */
         /* entered into the minutiae list upon processing the very first   */
         /* minutia detected in the loop.  There is a lot of redundant     */
         /* work being done here!                                          */
         /* Is_loop_clockwise takes a default value to be returned if the  */
         /* routine is unable to determine the direction of the contour.   */
         /* In this case, we want to IGNORE the loop if we can't tell its  */
         /* direction so that we do not inappropriately fill the loop, so  */
         /* we are passing the default value TRUE.                         */
         if((ret = is_loop_clockwise(contour_x, contour_y, ncontour, TRUE))){
            /* Deallocate contour lists. */
            free_contour(contour_x, contour_y, contour_ex, contour_ey);
            /* If we had a system error... */
            if(ret < 0)
               /* Return the error code. */
               return(ret);
            /* Otherwise, loop is clockwise, so return IGNORE. */
            return(IGNORE);
         }

         /* Otherwise, process the clockwise-ordered contour of the loop */
         /* as it may contain minutia.  If no minutia found, then it is  */
         /* filled in.                                                   */
         ret = process_loop_V2(minutiae, contour_x, contour_y,
                            contour_ex, contour_ey, ncontour,
                            bdata, iw, ih, plow_flow_map, lfsparms);
         /* Returns with:                              */
         /*    1. Successful processing of loop == 0   */
         /*    2. System error < 0                     */

         /* Deallocate contour lists. */
         free_contour(contour_x, contour_y, contour_ex, contour_ey);

         /* If loop processed successfully ... */
         if(ret == 0)
            /* Then either a minutia pair was extracted or the loop was */
            /* filled.  Either way we want to IGNORE the minutia that   */
            /* started the whole loop processing in the beginning.      */
            return(IGNORE);

         /* Otherwise, there was a system error. */
         /* Return the resulting code.           */
         return(ret);
      }

      /* Otherwise not a loop, so get_high_curvature_contour incurred */
      /* a system error.  Return the error code.                      */
      return(ret);
   }

   /* If contour is empty ... then contour lists were not allocated, so */
   /* simply return IGNORE.  The contour comes back empty when there    */
   /* were not a sufficient number of points found on the contour.      */
   if(ncontour == 0)
      return(IGNORE);

   /* Otherwise, there are contour points to process. */

   /* Given the contour, determine the point of highest curvature */
   /* (ie. forming the minimum angle between contour walls).      */
   if((ret = min_contour_theta(&min_i, &min_theta, angle_edge,
                              contour_x, contour_y, ncontour))){
      /* Deallocate contour lists. */
      free_contour(contour_x, contour_y, contour_ex, contour_ey);
      /* Returns IGNORE or system error.  Either way */
      /* free the contour and return the code.       */
      return(ret);
   }

   /* If the minimum theta found along the contour is too large... */
   if(min_theta >= lfsparms->max_high_curve_theta){
      /* Deallocate contour lists. */
      free_contour(contour_x, contour_y, contour_ex, contour_ey);
      /* Reject the high-curvature minutia, and return IGNORE. */
      return(IGNORE);
   }

   /* Test to see if interior of curvature is OK.  Compute midpoint   */
   /* between left and right points symmetrically distant (angle_edge */
   /* pixels) from the contour's point of minimum theta.              */
   mid_x = (contour_x[min_i-angle_edge] + contour_x[min_i+angle_edge])>>1;
   mid_y = (contour_y[min_i-angle_edge] + contour_y[min_i+angle_edge])>>1;
   mid_pix = *(bdata + (mid_y * iw) + mid_x);
   /* If the interior pixel value is not the same as the feature's... */
   if(mid_pix != feature_pix){
      /* Deallocate contour lists. */
      free_contour(contour_x, contour_y, contour_ex, contour_ey);
      /* Reject the high-curvature minutia and return IGNORE. */
      return(IGNORE);
   }

   /* Compute new direction based on line connecting adjusted feature */
   /* location and the midpoint in the feature's interior.            */
   idir = line2direction(contour_x[min_i], contour_y[min_i],
                         mid_x, mid_y, lfsparms->num_directions);

   /* Set minutia location to minimum theta position on the contour. */
   *oidir = idir;
   *ox_loc = contour_x[min_i];
   *oy_loc = contour_y[min_i];
   *ox_edge = contour_ex[min_i];
   *oy_edge = contour_ey[min_i];

   /* Deallocate contour buffers. */
   free_contour(contour_x, contour_y, contour_ex, contour_ey);

   /*Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: get_low_curvature_direction - Converts a bi-direcitonal IMAP direction
#cat:            (based on a semi-circle) to a uni-directional value covering
#cat:            a full circle based on the scan orientation used to detect
#cat:            a minutia feature (horizontal or vertical) and whether the
#cat:            detected minutia is appearing or disappearing.

   Input:
      scan_dir   - designates the feature scan orientation
      appearing  - designates the minutia as appearing or disappearing
      imapval    - IMAP block direction
      ndirs      - number of IMAP directions (in semicircle)
   Return Code:
      New direction - bi-directonal integer direction on full circle
*************************************************************************/
int get_low_curvature_direction(const int scan_dir, const int appearing,
                                const int imapval, const int ndirs)
{
   int idir;

   /* Start direction out with IMAP value. */
   idir = imapval;

   /* NOTE!                                                           */
   /* The logic in this routine should hold whether for ridge endings */
   /* or for bifurcations.  The examples in the comments show ridge   */
   /* ending conditions only.                                         */

   /* CASE I : Ridge flow in Quadrant I; directions [0..8]             */
   if(imapval <= (ndirs>>1)){
      /*    I.A: HORIZONTAL scan                                       */
      if(scan_dir == SCAN_HORIZONTAL){
         /*       I.A.1: Appearing Minutia                             */
         if(appearing){
            /*        Ex.    0 0 0                                     */
            /*               0 1 0                                     */
            /*               ? ?                                       */
            /*              Ridge flow is up and to the right, whereas */
            /*              actual ridge is running down and to the    */
            /*              left.                                      */
            /*              Thus: HORIZONTAL : appearing   : should be */
            /*                    OPPOSITE the ridge flow direction.   */
            idir += ndirs;
         }
         /* Otherwise:                                                 */
            /*       I.A.2: Disappearing Minutia                       */
            /*           Ex.   ? ?                                     */
            /*               0 1 0                                     */
            /*               0 0 0                                     */
            /*              Ridge flow is up and to the right, which   */
            /*              should be SAME direction from which ridge  */
            /*              is projecting.                             */
            /*              Thus: HORIZONTAL : disappearing : should   */
            /*                    be the same as ridge flow direction. */
      } /* End if HORIZONTAL scan */
      /* Otherwise:                                                    */
      /*    I.B: VERTICAL scan                                         */
      else{
         /*       I.B.1: Disappearing Minutia                          */
         if(!appearing){
            /*        Ex.    0 0                                       */
            /*             ? 1 0                                       */
            /*             ? 0 0                                       */
            /*              Ridge flow is up and to the right, whereas */
            /*              actual ridge is projecting down and to the */
            /*              left.                                      */
            /*              Thus: VERTICAL : disappearing : should be  */
            /*                    OPPOSITE the ridge flow direction.   */
            idir += ndirs;
         }
         /* Otherwise:                                                 */
            /*       I.B.2: Appearing Minutia                          */
            /*           Ex. 0 0 ?                                     */
            /*               0 1 ?                                     */
            /*               0 0                                       */
            /*              Ridge flow is up and to the right, which   */
            /*              should be SAME direction the ridge is      */
            /*              running.                                   */
            /*              Thus: VERTICAL : appearing   : should be   */
            /*                    be the same as ridge flow direction. */
      } /* End else VERTICAL scan */
   } /* End if Quadrant I */

   /* Otherwise:                                                       */
   /* CASE II : Ridge flow in Quadrant II; directions [9..15]          */
   else{
      /*   II.A: HORIZONTAL scan                                       */
      if(scan_dir == SCAN_HORIZONTAL){
         /*      II.A.1: Disappearing Minutia                          */
         if(!appearing){
            /*           Ex. ? ?                                       */
            /*               0 1 0                                     */
            /*               0 0 0                                     */
            /*              Ridge flow is down and to the right,       */
            /*              whereas actual ridge is running up and to  */
            /*              the left.                                  */
            /*              Thus: HORIZONTAL : disappearing : should   */
            /*                    be OPPOSITE the ridge flow direction.*/
            idir += ndirs;
         }
         /* Otherwise:                                                 */
         /*      II.A.2: Appearing Minutia                             */
            /*           Ex. 0 0 0                                     */
            /*               0 1 0                                     */
            /*                 ? ?                                     */
            /*              Ridge flow is down and to the right, which */
            /*              should be same direction from which ridge  */
            /*              is projecting.                             */
            /*              Thus: HORIZONTAL : appearing : should be   */
            /*                    the SAME as ridge flow direction.    */
      } /* End if HORIZONTAL scan */
      /* Otherwise:                                                    */
      /*   II.B: VERTICAL scan                                         */
      else{
         /*      II.B.1: Disappearing Minutia                          */
         if(!appearing){
            /*           Ex. ? 0 0                                     */
            /*               ? 1 0                                     */
            /*                 0 0                                     */
            /*              Ridge flow is down and to the right,       */
            /*              whereas actual ridge is running up and to  */
            /*              the left.                                  */
            /*              Thus: VERTICAL : disappearing : should be  */
            /*                    OPPOSITE the ridge flow direction.   */
            idir += ndirs;
         }
         /* Otherwise:                                                 */
            /*      II.B.2: Appearing Minutia                          */
            /*           Ex. 0 0                                       */
            /*               0 1 ?                                     */
            /*               0 0 ?                                     */
            /*              Ridge flow is down and to the right, which */
            /*              should be same direction the ridge is      */
            /*              projecting.                                */
            /*              Thus: VERTICAL : appearing   : should be   */
            /*                    be the SAME as ridge flow direction. */
      } /* End else VERTICAL scan */
   } /* End else Quadrant II */

   /* Return resulting direction on range [0..31]. */
   return(idir);
}

/*************************************************************************
**************************************************************************
#cat: lfs2nist_minutia_XYT - Converts XYT minutiae attributes in LFS native
#cat:        representation to NIST internal representation

   Input:
      minutia  - LFS minutia structure containing attributes to be converted
   Output:
      ox       - NIST internal based x-pixel coordinate
      oy       - NIST internal based y-pixel coordinate
      ot       - NIST internal based minutia direction/orientation
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
void lfs2nist_minutia_XYT(int *ox, int *oy, int *ot,
                          const MINUTIA *minutia, const int iw, const int ih)
{
   int x, y, t;
   float degrees_per_unit;

   /*       XYT's according to NIST internal rep:           */
    /*      1. pixel coordinates with origin bottom-left    */
   /*       2. orientation in degrees on range [0..360]     */
   /*          with 0 pointing east and increasing counter  */
   /*          clockwise (same as M1)                       */
   /*       3. direction pointing out and away from the     */
   /*             ridge ending or bifurcation valley        */
   /*             (opposite direction from M1)              */

   x = minutia->x;
   y = ih - minutia->y;

   degrees_per_unit = 180 / (float)NUM_DIRECTIONS;

   t = (270 - sround(minutia->direction * degrees_per_unit)) % 360;
   if(t < 0){
      t += 360;
   }

   *ox = x;
   *oy = y;
   *ot = t;
}


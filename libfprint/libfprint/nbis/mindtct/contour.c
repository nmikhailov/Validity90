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

      FILE:    CONTOUR.C
      AUTHOR:  Michael D. Garris
      DATE:    05/11/1999
      UPDATED: 10/04/1999 Version 2 by MDG
      UPDATED: 03/16/2005 by MDG

      Contains routines responsible for extracting and analyzing
      minutia feature contour lists as part of the NIST Latent
      Fingerprint System (LFS).

***********************************************************************
               ROUTINES:
                        allocate_contour()
                        free_contour()
                        get_high_curvature_contour()
                        get_centered_contour()
                        trace_contour()
                        search_contour()
                        next_contour_pixel()
                        start_scan_nbr()
                        next_scan_nbr()
                        min_contour_theta()
                        contour_limits()
***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <lfs.h>

/*************************************************************************
**************************************************************************
#cat: allocate_contour - Allocates the lists needed to represent the
#cat:            contour of a minutia feature (a ridge or valley-ending).
#cat:            This includes two lists of coordinate pairs.  The first is
#cat:            the 8-connected chain of points interior to the feature
#cat:            and are called the feature's "contour points".
#cat:            The second is a list or corresponding points each
#cat:            adjacent to its respective feature contour point in the first
#cat:            list and on the exterior of the feature.  These second points
#cat:            are called the feature's "edge points".  Don't be confused,
#cat:            both lists of points are on the "edge".  The first set is
#cat:            guaranteed 8-connected and the color of the feature.  The
#cat:            second set is NOT guaranteed to be 8-connected and its points
#cat:            are opposite the color of the feature.  Remeber that "feature"
#cat:            means either ridge-ending (black pixels) or valley-ending
#cat:            (white pixels).

   Input:
      ncontour    - number of items in each coordinate list to be allocated
   Output:
      ocontour_x  - allocated x-coord list for feature's contour points
      ocontour_y  - allocated y-coord list for feature's contour points
      ocontour_ex - allocated x-coord list for feature's edge points
      ocontour_ey - allocated y-coord list for feature's edge points
   Return Code:
      Zero      - lists were successfully allocated
      Negative  - system (allocation) error
**************************************************************************/
int allocate_contour(int **ocontour_x, int **ocontour_y,
                     int **ocontour_ex, int **ocontour_ey, const int ncontour)
{
   int *contour_x, *contour_y, *contour_ex, *contour_ey;

   /* Allocate contour's x-coord list. */
   contour_x = (int *)malloc(ncontour*sizeof(int));
   /* If allocation error... */
   if(contour_x == (int *)NULL){
      fprintf(stderr, "ERROR : allocate_contour : malloc : contour_x\n");
      return(-180);
   }

   /* Allocate contour's y-coord list. */
   contour_y = (int *)malloc(ncontour*sizeof(int));
   /* If allocation error... */
   if(contour_y == (int *)NULL){
      /* Deallocate memory allocated to this point in this routine. */
      free(contour_x);
      fprintf(stderr, "ERROR : allocate_contour : malloc : contour_y\n");
      return(-181);
   }

   /* Allocate contour's edge x-coord list. */
   contour_ex = (int *)malloc(ncontour*sizeof(int));
   /* If allocation error... */
   if(contour_ex == (int *)NULL){
      /* Deallocate memory allocated to this point in this routine. */
      free(contour_x);
      free(contour_y);
      fprintf(stderr, "ERROR : allocate_contour : malloc : contour_ex\n");
      return(-182);
   }

   /* Allocate contour's edge y-coord list. */
   contour_ey = (int *)malloc(ncontour*sizeof(int));
   /* If allocation error... */
   if(contour_ey == (int *)NULL){
      /* Deallocate memory allocated to this point in this routine. */
      free(contour_x);
      free(contour_y);
      free(contour_ex);
      fprintf(stderr, "ERROR : allocate_contour : malloc : contour_ey\n");
      return(-183);
   }

   /* Otherwise, allocations successful, so assign output pointers. */
   *ocontour_x = contour_x;
   *ocontour_y = contour_y;
   *ocontour_ex = contour_ex;
   *ocontour_ey = contour_ey;

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: free_contour - Deallocates the lists used to represent the
#cat:            contour of a minutia feature (a ridge or valley-ending).
#cat:            This includes two lists of coordinate pairs.  The first is
#cat:            the 8-connected chain of points interior to the feature
#cat:            and are called the feature's "contour points".
#cat:            The second is a list or corresponding points each
#cat:            adjacent to its respective feature contour point in the first
#cat:            list and on the exterior of the feature.  These second points
#cat:            are called the feature's "edge points".

   Input:
      contour_x  - x-coord list for feature's contour points
      contour_y  - y-coord list for feature's contour points
      contour_ex - x-coord list for feature's edge points
      contour_ey - y-coord list for feature's edge points
**************************************************************************/
void free_contour(int *contour_x, int *contour_y,
                  int *contour_ex, int *contour_ey)
{
   free(contour_x);
   free(contour_y);
   free(contour_ex);
   free(contour_ey);
}

/*************************************************************************
**************************************************************************
#cat: get_high_curvature_contour - Takes the pixel coordinate of a detected
#cat:            minutia feature point and its corresponding/adjacent edge
#cat:            pixel and attempts to extract a contour of specified length
#cat:            of the feature's edge.  The contour is extracted by walking
#cat:            the feature's edge a specified number of steps clockwise and
#cat:            then counter-clockwise. If a loop is detected while
#cat:            extracting the contour, the contour of the loop is returned
#cat:            with a return code of (LOOP_FOUND).  If the process fails
#cat:            to extract a contour of total specified length, then
#cat:            the returned contour length is set to Zero, NO allocated
#cat:            memory is returned in this case, and the return code is set
#cat:            to Zero.  An alternative implementation would be to return
#cat:            the incomplete contour with a return code of (INCOMPLETE).
#cat:            For now, NO allocated contour is returned in this case.

   Input:
      half_contour - half the length of the extracted contour
                     (full-length non-loop contour = (half_contourX2)+1)
      x_loc  - starting x-pixel coord of feature (interior to feature)
      y_loc  - starting y-pixel coord of feature (interior to feature)
      x_edge - x-pixel coord of corresponding edge pixel
               (exterior to feature)
      y_edge - y-pixel coord of corresponding edge pixel
                  (exterior to feature)
      bdata  - binary image data (0==while & 1==black)
      iw     - width (in pixels) of image
      ih     - height (in pixels) of image
   Output:
      ocontour_x  - x-pixel coords of contour (interior to feature)
      ocontour_y  - y-pixel coords of contour (interior to feature)
      ocontour_ex - x-pixel coords of corresponding edge (exterior to feature)
      ocontour_ey - y-pixel coords of corresponding edge (exterior to feature)
      oncontour   - number of contour points returned
   Return Code:
      Zero       - resulting contour was successfully extracted or is empty
      LOOP_FOUND - resulting contour forms a complete loop
      Negative   - system error
**************************************************************************/
int get_high_curvature_contour(int **ocontour_x, int **ocontour_y,
                 int **ocontour_ex, int **ocontour_ey, int *oncontour,
                 const int half_contour,
                 const int x_loc, const int y_loc,
                 const int x_edge, const int y_edge,
                 unsigned char *bdata, const int iw, const int ih)
{
   int max_contour;
   int *half1_x, *half1_y, *half1_ex, *half1_ey, nhalf1;
   int *half2_x, *half2_y, *half2_ex, *half2_ey, nhalf2;
   int *contour_x, *contour_y, *contour_ex, *contour_ey, ncontour;
   int i, j, ret;

   /* Compute maximum length of complete contour */
   /* (2 half contours + feature point).         */
   max_contour = (half_contour<<1) + 1;

   /* Initialize output contour length to 0. */
   *oncontour = 0;

   /* Get 1st half contour with clockwise neighbor trace. */
   if((ret = trace_contour(&half1_x, &half1_y, &half1_ex, &half1_ey, &nhalf1,
                  half_contour, x_loc, y_loc, x_loc, y_loc, x_edge, y_edge,
                  SCAN_CLOCKWISE, bdata, iw, ih))){

      /* If trace was not possible ... */
      if(ret == IGNORE)
         /* Return, with nothing allocated and contour length equal to 0. */
         return(0);

      /* If 1st half contour forms a loop ... */
      if(ret == LOOP_FOUND){
         /* Need to reverse the 1st half contour so that the points are    */
         /* in consistent order.                                           */
         /* We need to add the original feature point to the list, so      */
         /* set new contour length to one plus length of 1st half contour. */
         ncontour = nhalf1+1;
         /* Allocate new contour list. */
         if((ret = allocate_contour(&contour_x, &contour_y,
                                   &contour_ex, &contour_ey, ncontour))){
            /* If allcation error, then deallocate memory allocated to */
            /* this point in this routine.                             */
            free_contour(half1_x, half1_y, half1_ex, half1_ey);
            /* Return error code. */
            return(ret);
         }

         /* Otherwise, we have the new contour allocated, so store the */
         /* original feature point.                                    */
         contour_x[0] = x_loc;
         contour_y[0] = y_loc;
         contour_ex[0] = x_edge;
         contour_ey[0] = y_edge;

         /* Now store the first half contour in reverse order. */
         for(i = 1, j = nhalf1-1; i < ncontour; i++, j--){
             contour_x[i] = half1_x[j];
             contour_y[i] = half1_y[j];
             contour_ex[i] = half1_ex[j];
             contour_ey[i] = half1_ey[j];
         }

         /* Deallocate the first half contour. */
         free_contour(half1_x, half1_y, half1_ex, half1_ey);

         /* Assign the output pointers. */
         *ocontour_x = contour_x;
         *ocontour_y = contour_y;
         *ocontour_ex = contour_ex;
         *ocontour_ey = contour_ey;
         *oncontour = ncontour;

         /* Return LOOP_FOUND for further processing. */
         return(LOOP_FOUND);
      }

      /* Otherwise, return the system error code from the first */
      /* call to trace_contour.                                 */
      return(ret);
   }

   /* If 1st half contour not complete ... */
   if(nhalf1 < half_contour){
      /* Deallocate the partial contour. */
      free_contour(half1_x, half1_y, half1_ex, half1_ey);
      /* Return, with nothing allocated and contour length equal to 0. */
      return(0);
   }

   /* Otherwise, we have a complete 1st half contour...           */
   /* Get 2nd half contour with counter-clockwise neighbor trace. */
   /* Use the last point from the first contour trace as the      */
   /* point to test for a loop when tracing the second contour.   */
   if((ret = trace_contour(&half2_x, &half2_y, &half2_ex, &half2_ey, &nhalf2,
                  half_contour, half1_x[nhalf1-1], half1_y[nhalf1-1],
                  x_loc, y_loc, x_edge, y_edge,
                 SCAN_COUNTER_CLOCKWISE, bdata, iw, ih))){

      /* If 2nd trace was not possible ... */
      if(ret == IGNORE){
         /* Deallocate the 1st half contour. */
         free_contour(half1_x, half1_y, half1_ex, half1_ey);
         /* Return, with nothing allocated and contour length equal to 0. */
         return(0);
      }

      /* If non-zero return code is NOT LOOP_FOUND, then system error ... */
      if(ret != LOOP_FOUND){
         /* Deallocate the 1st half contour. */
         free_contour(half1_x, half1_y, half1_ex, half1_ey);
         /* Return system error. */
         return(ret);
      }
   }

   /* If 2nd half NOT a loop AND not complete ... */
   if((ret != LOOP_FOUND) && (nhalf2 < half_contour)){
      /* Deallocate both half contours. */
      free_contour(half1_x, half1_y, half1_ex, half1_ey);
      free_contour(half2_x, half2_y, half2_ex, half2_ey);
      /* Return, with nothing allocated and contour length equal to 0. */
      return(0);
   }

   /* Otherwise we have a full 1st half contour and a 2nd half contour */
   /* that is either a loop or complete.  In either case we need to    */
   /* concatenate the two half contours into one longer contour.       */

   /* Allocate output contour list.  Go ahead and allocate the    */
   /* "max_contour" amount even though the resulting contour will */
   /* likely be shorter if it forms a loop.                       */
   if((ret = allocate_contour(&contour_x, &contour_y,
                             &contour_ex, &contour_ey, max_contour))){
      /* If allcation error, then deallocate memory allocated to */
      /* this point in this routine.                             */
      free_contour(half1_x, half1_y, half1_ex, half1_ey);
      free_contour(half2_x, half2_y, half2_ex, half2_ey);
      /* Return error code. */
      return(ret);
   }

   /* Set the current contour point counter to 0 */
   ncontour = 0;

   /* Copy 1st half contour into output contour buffers.      */
   /* This contour was collected clockwise, so it's points    */
   /* are entered in reverse order of the trace.  The result  */
   /* is the first point in the output contour if farthest    */
   /* from the starting feature point.                        */
   for(i = 0, j = nhalf1-1; i < nhalf1; i++, j--){
      contour_x[i] = half1_x[j];
      contour_y[i] = half1_y[j];
      contour_ex[i] = half1_ex[j];
      contour_ey[i] = half1_ey[j];
      ncontour++;
   }

   /* Deallocate 1st half contour. */
   free_contour(half1_x, half1_y, half1_ex, half1_ey);

   /* Next, store starting feature point into output contour buffers. */
   contour_x[nhalf1] = x_loc;
   contour_y[nhalf1] = y_loc;
   contour_ex[nhalf1] = x_edge;
   contour_ey[nhalf1] = y_edge;
   ncontour++;

   /* Now, append 2nd half contour to permanent contour buffers.  */
   for(i = 0, j = nhalf1+1; i < nhalf2; i++, j++){
      contour_x[j] = half2_x[i];
      contour_y[j] = half2_y[i];
      contour_ex[j] = half2_ex[i];
      contour_ey[j] = half2_ey[i];
      ncontour++;
   }

   /* Deallocate 2nd half contour. */
   free_contour(half2_x, half2_y, half2_ex, half2_ey);

   /* Assign outputs contour to output ponters. */
   *ocontour_x = contour_x;
   *ocontour_y = contour_y;
   *ocontour_ex = contour_ex;
   *ocontour_ey = contour_ey;
   *oncontour = ncontour;

   /* Return the resulting return code form the 2nd call to trace_contour */
   /* (the value will either be 0 or LOOP_FOUND).                         */
   return(ret);
}

/*************************************************************************
**************************************************************************
#cat: get_centered_contour - Takes the pixel coordinate of a detected
#cat:            minutia feature point and its corresponding/adjacent edge
#cat:            pixel and attempts to extract a contour of specified length
#cat:            of the feature's edge.  The contour is extracted by walking
#cat:            the feature's edge a specified number of steps clockwise and
#cat:            then counter-clockwise. If a loop is detected while
#cat:            extracting the contour, no contour is returned with a return
#cat:            code of (LOOP_FOUND).  If the process fails to extract a
#cat:            a complete contour, a code of INCOMPLETE is returned.

   Input:
      half_contour - half the length of the extracted contour
                     (full-length non-loop contour = (half_contourX2)+1)
      x_loc  - starting x-pixel coord of feature (interior to feature)
      y_loc  - starting y-pixel coord of feature (interior to feature)
      x_edge - x-pixel coord of corresponding edge pixel
               (exterior to feature)
      y_edge - y-pixel coord of corresponding edge pixel
                  (exterior to feature)
      bdata  - binary image data (0==while & 1==black)
      iw     - width (in pixels) of image
      ih     - height (in pixels) of image
   Output:
      ocontour_x  - x-pixel coords of contour (interior to feature)
      ocontour_y  - y-pixel coords of contour (interior to feature)
      ocontour_ex - x-pixel coords of corresponding edge (exterior to feature)
      ocontour_ey - y-pixel coords of corresponding edge (exterior to feature)
      oncontour   - number of contour points returned
   Return Code:
      Zero       - resulting contour was successfully extracted or is empty
      LOOP_FOUND - resulting contour forms a complete loop
      IGNORE     - contour could not be traced due to problem starting
                   conditions
      INCOMPLETE - resulting contour was not long enough
      Negative   - system error
**************************************************************************/
int get_centered_contour(int **ocontour_x, int **ocontour_y,
                 int **ocontour_ex, int **ocontour_ey, int *oncontour,
                 const int half_contour,
                 const int x_loc, const int y_loc,
                 const int x_edge, const int y_edge,
                 unsigned char *bdata, const int iw, const int ih)
{
   int max_contour;
   int *half1_x, *half1_y, *half1_ex, *half1_ey, nhalf1;
   int *half2_x, *half2_y, *half2_ex, *half2_ey, nhalf2;
   int *contour_x, *contour_y, *contour_ex, *contour_ey, ncontour;
   int i, j, ret;

   /* Compute maximum length of complete contour */
   /* (2 half contours + feature point).         */
   max_contour = (half_contour<<1) + 1;

   /* Initialize output contour length to 0. */
   *oncontour = 0;

   /* Get 1st half contour with clockwise neighbor trace. */
   ret = trace_contour(&half1_x, &half1_y, &half1_ex, &half1_ey, &nhalf1,
                  half_contour, x_loc, y_loc, x_loc, y_loc, x_edge, y_edge,
                  SCAN_CLOCKWISE, bdata, iw, ih);

   /* If system error occurred ... */
   if(ret < 0){
      /* Return error code. */
      return(ret);
   }

   /* If trace was not possible ... */
   if(ret == IGNORE)
      /* Return IGNORE, with nothing allocated. */
      return(IGNORE);

   /* If 1st half contour forms a loop ... */
   if(ret == LOOP_FOUND){
      /* Deallocate loop's contour. */
      free_contour(half1_x, half1_y, half1_ex, half1_ey);
      /* Return LOOP_FOUND, with nothing allocated. */
      return(LOOP_FOUND);
   }

   /* If 1st half contour not complete ... */
   if(nhalf1 < half_contour){
      /* Deallocate the partial contour. */
      free_contour(half1_x, half1_y, half1_ex, half1_ey);
      /* Return, with nothing allocated and contour length equal to 0. */
      return(INCOMPLETE);
   }

   /* Otherwise, we have a complete 1st half contour...           */
   /* Get 2nd half contour with counter-clockwise neighbor trace. */
   /* Use the last point from the first contour trace as the      */
   /* point to test for a loop when tracing the second contour.   */
   ret = trace_contour(&half2_x, &half2_y, &half2_ex, &half2_ey, &nhalf2,
                  half_contour, half1_x[nhalf1-1], half1_y[nhalf1-1],
                  x_loc, y_loc, x_edge, y_edge,
                 SCAN_COUNTER_CLOCKWISE, bdata, iw, ih);

   /* If system error occurred on 2nd trace ... */
   if(ret < 0){
      /* Return error code. */
      return(ret);
   }

   /* If 2nd trace was not possible ... */
   if(ret == IGNORE){
      /* Deallocate the 1st half contour. */
      free_contour(half1_x, half1_y, half1_ex, half1_ey);
      /* Return, with nothing allocated and contour length equal to 0. */
      return(IGNORE);
   }

   /* If 2nd trace forms a loop ... */
   if(ret == LOOP_FOUND){
      /* Deallocate 1st and 2nd half contours. */
      free_contour(half1_x, half1_y, half1_ex, half1_ey);
      free_contour(half2_x, half2_y, half2_ex, half2_ey);
      /* Return LOOP_FOUND, with nothing allocated. */
      return(LOOP_FOUND);
   }

   /* If 2nd half contour not complete ... */
   if(nhalf2 < half_contour){
      /* Deallocate 1st and 2nd half contours. */
      free_contour(half1_x, half1_y, half1_ex, half1_ey);
      free_contour(half2_x, half2_y, half2_ex, half2_ey);
      /* Return, with nothing allocated and contour length equal to 0. */
      return(INCOMPLETE);
   }

   /* Otherwise we have a full 1st half contour and a 2nd half contour */
   /* that do not form a loop and are complete.  We now need to        */
   /* concatenate the two half contours into one longer contour.       */

   /* Allocate output contour list. */
   if((ret = allocate_contour(&contour_x, &contour_y,
                             &contour_ex, &contour_ey, max_contour))){
      /* If allcation error, then deallocate memory allocated to */
      /* this point in this routine.                             */
      free_contour(half1_x, half1_y, half1_ex, half1_ey);
      free_contour(half2_x, half2_y, half2_ex, half2_ey);
      /* Return error code. */
      return(ret);
   }

   /* Set the current contour point counter to 0 */
   ncontour = 0;

   /* Copy 1st half contour into output contour buffers.      */
   /* This contour was collected clockwise, so it's points    */
   /* are entered in reverse order of the trace.  The result  */
   /* is the first point in the output contour if farthest    */
   /* from the starting feature point.                        */
   for(i = 0, j = nhalf1-1; i < nhalf1; i++, j--){
      contour_x[i] = half1_x[j];
      contour_y[i] = half1_y[j];
      contour_ex[i] = half1_ex[j];
      contour_ey[i] = half1_ey[j];
      ncontour++;
   }

   /* Deallocate 1st half contour. */
   free_contour(half1_x, half1_y, half1_ex, half1_ey);

   /* Next, store starting feature point into output contour buffers. */
   contour_x[nhalf1] = x_loc;
   contour_y[nhalf1] = y_loc;
   contour_ex[nhalf1] = x_edge;
   contour_ey[nhalf1] = y_edge;
   ncontour++;

   /* Now, append 2nd half contour to permanent contour buffers.  */
   for(i = 0, j = nhalf1+1; i < nhalf2; i++, j++){
      contour_x[j] = half2_x[i];
      contour_y[j] = half2_y[i];
      contour_ex[j] = half2_ex[i];
      contour_ey[j] = half2_ey[i];
      ncontour++;
   }

   /* Deallocate 2nd half contour. */
   free_contour(half2_x, half2_y, half2_ex, half2_ey);

   /* Assign outputs contour to output ponters. */
   *ocontour_x = contour_x;
   *ocontour_y = contour_y;
   *ocontour_ex = contour_ex;
   *ocontour_ey = contour_ey;
   *oncontour = ncontour;

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: start_scan_nbr - Takes a two pixel coordinates that are either
#cat:            aligned north-to-south or east-to-west, and returns the
#cat:            position the second pixel is in realtionship to the first.
#cat:            The positions returned are based on 8-connectedness.
#cat:            NOTE, this routine does NOT account for diagonal positions.

   Input:
      x_prev - x-coord of first point
      y_prev - y-coord of first point
      x_next - x-coord of second point
      y_next - y-coord of second point
   Return Code:
      NORTH - second pixel above first
      SOUTH - second pixel below first
      EAST  - second pixel right of first
      WEST  - second pixel left of first
**************************************************************************/
static int start_scan_nbr(const int x_prev, const int y_prev,
                   const int x_next, const int y_next)
{
   if((x_prev==x_next) && (y_next > y_prev))
      return(SOUTH);
   else if ((x_prev==x_next) && (y_next < y_prev))
      return(NORTH);
   else if ((x_next > x_prev) && (y_prev==y_next))
      return(EAST);
   else if ((x_next < x_prev) && (y_prev==y_next))
      return(WEST);

   /* Added by MDG on 03-16-05 */
   /* Should never reach here.  Added to remove compiler warning. */
   return(INVALID_DIR); /* -1 */
}

/*************************************************************************
**************************************************************************
#cat: next_scan_nbr - Advances the given 8-connected neighbor index
#cat:            on location in the specifiec direction (clockwise or
#cat:            counter-clockwise). 

   Input:
      nbr_i      - current 8-connected neighbor index
      scan_clock - direction in which the neighbor index is to be advanced
   Return Code:
      Next neighbor - 8-connected index of next neighbor
**************************************************************************/
static int next_scan_nbr(const int nbr_i, const int scan_clock)
{
   int new_i;

   /* If scanning neighbors clockwise ... */
   if(scan_clock == SCAN_CLOCKWISE)
      /* Advance one neighbor clockwise. */
      new_i = (nbr_i+1)%8;
   /* Otherwise, scanning neighbors counter-clockwise ... */
   else
      /* Advance one neighbor counter-clockwise.         */
      /* There are 8 pixels in the neighborhood, so to   */
      /* decrement with wrapping from 0 around to 7, add */
      /* the nieghbor index by 7 and mod with 8.         */
      new_i = (nbr_i+7)%8;

   /* Return the new neighbor index. */
   return(new_i);
}

/*************************************************************************
**************************************************************************
#cat: next_contour_pixel - Takes a pixel coordinate of a point determined
#cat:            to be on the interior edge of a feature (ridge or valley-
#cat:            ending), and attempts to locate a neighboring pixel on the
#cat:            feature's contour.  Neighbors of the current feature pixel
#cat:            are searched in a specified direction (clockwise or counter-
#cat:            clockwise) and the first pair of adjacent/neigboring pixels
#cat:            found with the first pixel having the color of the feature
#cat:            and the second the opposite color are returned as the next
#cat:            point on the contour.  One exception happens when the new
#cat:            point is on an "exposed" corner.

   Input:
      cur_x_loc  - x-pixel coord of current point on feature's
                   interior contour
      cur_y_loc  - y-pixel coord of current point on feature's
                   interior contour
      cur_x_edge - x-pixel coord of corresponding edge pixel
                   (exterior to feature)
      cur_y_edge - y-pixel coord of corresponding edge pixel
                   (exterior to feature)
      scan_clock - direction in which neighboring pixels are to be scanned
                   for the next contour pixel
      bdata      - binary image data (0==while & 1==black)
      iw         - width (in pixels) of image
      ih         - height (in pixels) of image
   Output:
      next_x_loc  - x-pixel coord of next point on feature's interior contour
      next_y_loc  - y-pixel coord of next point on feature's interior contour
      next_x_edge - x-pixel coord of corresponding edge (exterior to feature)
      next_y_edge - y-pixel coord of corresponding edge (exterior to feature)
   Return Code:
      TRUE  - next contour point found and returned
      FALSE - next contour point NOT found
**************************************************************************/
/*************************************************************************/
static int next_contour_pixel(int *next_x_loc, int *next_y_loc,
                int *next_x_edge, int *next_y_edge,
                const int cur_x_loc, const int cur_y_loc,
                const int cur_x_edge, const int cur_y_edge,
                const int scan_clock,
                unsigned char *bdata, const int iw, const int ih)
{
   int feature_pix, edge_pix;
   int prev_nbr_pix, prev_nbr_x, prev_nbr_y;
   int cur_nbr_pix, cur_nbr_x, cur_nbr_y;
   int ni, nx, ny, npix;
   int nbr_i, i;

   /* Get the feature's pixel value. */
   feature_pix = *(bdata + (cur_y_loc * iw) + cur_x_loc);
   /* Get the feature's edge pixel value. */
   edge_pix = *(bdata + (cur_y_edge * iw) + cur_x_edge);

   /* Get the nieghbor position of the feature's edge pixel in relationship */
   /* to the feature's actual position.                                     */
   /* REMEBER: The feature's position is always interior and on a ridge     */
   /* ending (black pixel) or (for bifurcations) on a valley ending (white  */
   /* pixel).  The feature's edge pixel is an adjacent pixel to the feature */
   /* pixel that is exterior to the ridge or valley ending and opposite in  */
   /* pixel value.                                                          */
   nbr_i = start_scan_nbr(cur_x_loc, cur_y_loc, cur_x_edge, cur_y_edge);

   /* Set current neighbor scan pixel to the feature's edge pixel. */
   cur_nbr_x = cur_x_edge;
   cur_nbr_y = cur_y_edge;
   cur_nbr_pix = edge_pix;

   /* Foreach pixel neighboring the feature pixel ... */
   for(i = 0; i < 8; i++){

      /* Set current neighbor scan pixel to previous scan pixel. */
      prev_nbr_x = cur_nbr_x;
      prev_nbr_y = cur_nbr_y;
      prev_nbr_pix = cur_nbr_pix;

      /* Bump pixel neighbor index clockwise or counter-clockwise. */
      nbr_i = next_scan_nbr(nbr_i, scan_clock);

      /* Set current scan pixel to the new neighbor.                   */
      /* REMEMBER: the neighbors are being scanned around the original */
      /* feature point.                                                */
      cur_nbr_x = cur_x_loc + g_nbr8_dx[nbr_i];
      cur_nbr_y = cur_y_loc + g_nbr8_dy[nbr_i];

      /* If new neighbor is not within image boundaries... */
      if((cur_nbr_x < 0) || (cur_nbr_x >= iw) ||
         (cur_nbr_y < 0) || (cur_nbr_y >= ih))
         /* Return (FALSE==>Failure) if neighbor out of bounds. */
         return(FALSE);

      /* Get the new neighbor's pixel value. */
      cur_nbr_pix = *(bdata + (cur_nbr_y * iw) + cur_nbr_x);

      /* If the new neighbor's pixel value is the same as the feature's   */
      /* pixel value AND the previous neighbor's pixel value is the same  */
      /* as the features's edge, then we have "likely" found our next     */
      /* contour pixel.                                                   */
      if((cur_nbr_pix == feature_pix) && (prev_nbr_pix == edge_pix)){

         /* Check to see if current neighbor is on the corner of the */
         /* neighborhood, and if so, test to see if it is "exposed". */
         /* The neighborhood corners have odd neighbor indicies.     */
         if(nbr_i % 2){
            /* To do this, look ahead one more neighbor pixel. */
            ni = next_scan_nbr(nbr_i, scan_clock);
            nx = cur_x_loc + g_nbr8_dx[ni];
            ny = cur_y_loc + g_nbr8_dy[ni];
            /* If new neighbor is not within image boundaries... */
            if((nx < 0) || (nx >= iw) ||
               (ny < 0) || (ny >= ih))
               /* Return (FALSE==>Failure) if neighbor out of bounds. */
               return(FALSE);
            npix = *(bdata + (ny * iw) + nx);

            /* If the next neighbor's value is also the same as the */
            /* feature's pixel, then corner is NOT exposed...       */
            if(npix == feature_pix){
               /* Assign the current neighbor pair to the output pointers. */
               *next_x_loc = cur_nbr_x;
               *next_y_loc = cur_nbr_y;
               *next_x_edge = prev_nbr_x;
               *next_y_edge = prev_nbr_y;
               /* Return TRUE==>Success. */
               return(TRUE);
            }
            /* Otherwise, corner pixel is "exposed" so skip it. */
            else{
               /* Skip current corner neighbor by resetting it to the      */
               /* next neighbor, which upon the iteration will immediately */
               /* become the previous neighbor.                            */
               cur_nbr_x = nx;
               cur_nbr_y = ny;
               cur_nbr_pix = npix;
               /* Advance neighbor index. */
               nbr_i = ni;
               /* Advance neighbor count. */
               i++;
            }
         }
         /* Otherwise, current neighbor is not a corner ... */
         else{
            /* Assign the current neighbor pair to the output pointers. */
            *next_x_loc = cur_nbr_x;
            *next_y_loc = cur_nbr_y;
            *next_x_edge = prev_nbr_x;
            *next_y_edge = prev_nbr_y;
            /* Return TRUE==>Success. */
            return(TRUE);
         }
      }
   }

   /* If we get here, then we did not find the next contour pixel */
   /* within the 8 neighbors of the current feature pixel so      */
   /* return (FALSE==>Failure).                                   */
   /* NOTE: This must mean we found a single isolated pixel.      */
   /*       Perhaps this should be filled?                        */
   return(FALSE);
}

/*************************************************************************
**************************************************************************
#cat: trace_contour - Takes the pixel coordinate of a detected minutia
#cat:            feature point and its corresponding/adjacent edge pixel
#cat:            and extracts a contour (up to a specified maximum length)
#cat:            of the feature's edge in either a clockwise or counter-
#cat:            clockwise direction.  A second point is specified, such that
#cat:            if this point is encounted while extracting the contour,
#cat:            it is to be assumed that a loop has been found and a code
#cat:            of (LOOP_FOUND) is returned with the contour. By independently
#cat:            specifying this point, successive calls can be made to
#cat:            this routine from the same starting point, and loops across
#cat:            successive calls can be detected.

   Input:
      max_len - maximum length of contour to be extracted
      x_loop  - x-pixel coord of point, if encountered, triggers LOOP_FOUND
      y_loop  - y-pixel coord of point, if encountered, triggers LOOP_FOUND
      x_loc   - starting x-pixel coord of feature (interior to feature)
      y_loc   - starting y-pixel coord of feature (interior to feature)
      x_edge  - x-pixel coord of corresponding edge pixel (exterior to feature)
      y_edge  - y-pixel coord of corresponding edge pixel (exterior to feature)
      scan_clock - direction in which neighboring pixels are to be scanned
                for the next contour pixel
      bdata  - binary image data (0==while & 1==black)
      iw     - width (in pixels) of image
      ih     - height (in pixels) of image
   Output:
      ocontour_x  - x-pixel coords of contour (interior to feature)
      ocontour_y  - y-pixel coords of contour (interior to feature)
      ocontour_ex - x-pixel coords of corresponding edge (exterior to feature)
      ocontour_ey - y-pixel coords of corresponding edge (exterior to feature)
      oncontour   - number of contour points returned
   Return Code:
      Zero       - resulting contour was successfully allocated and extracted
      LOOP_FOUND - resulting contour forms a complete loop
      IGNORE     - trace is not possible due to state of inputs
      Negative   - system error
**************************************************************************/
int trace_contour(int **ocontour_x, int **ocontour_y,
                  int **ocontour_ex, int **ocontour_ey, int *oncontour,
                  const int max_len, const int x_loop, const int y_loop,
                  const int x_loc, const int y_loc,
                  const int x_edge, const int y_edge,
                  const int scan_clock,
                  unsigned char *bdata, const int iw, const int ih)
{
   int *contour_x, *contour_y, *contour_ex, *contour_ey, ncontour;
   int cur_x_loc, cur_y_loc;
   int cur_x_edge, cur_y_edge;
   int next_x_loc, next_y_loc;
   int next_x_edge, next_y_edge;
   int i, ret;

   /* Check to make sure that the feature and edge values are opposite. */
   if(*(bdata+(y_loc*iw)+x_loc) ==
      *(bdata+(y_edge*iw)+x_edge))
      /* If not opposite, then the trace will not work, so return IGNORE. */
      return(IGNORE);

   /* Allocate contour buffers. */
   if((ret = allocate_contour(&contour_x, &contour_y,
                     &contour_ex, &contour_ey, max_len))){
      /* If allocation error, return code. */
      return(ret);
   }

   /* Set pixel counter to 0. */
   ncontour = 0;

   /* Set up for finding first contour pixel. */
   cur_x_loc = x_loc;
   cur_y_loc = y_loc;
   cur_x_edge = x_edge;
   cur_y_edge = y_edge;

   /* Foreach pixel to be collected on the feature's contour... */
   for(i = 0; i < max_len; i++){
      /* Find the next contour pixel. */
      if(next_contour_pixel(&next_x_loc, &next_y_loc,
                            &next_x_edge, &next_y_edge,
                            cur_x_loc, cur_y_loc,
                            cur_x_edge, cur_y_edge,
                            scan_clock, bdata, iw, ih)){

         /* If we trace back around to the specified starting */
         /* feature location...                               */
         if((next_x_loc == x_loop) && (next_y_loc == y_loop)){
            /* Then we have found a loop, so return what we */
            /* have traced to this point.                   */
            *ocontour_x = contour_x;
            *ocontour_y = contour_y;
            *ocontour_ex = contour_ex;
            *ocontour_ey = contour_ey;
            *oncontour = ncontour;
            return(LOOP_FOUND);
         }

         /* Otherwise, we found another point on our feature's contour, */
         /* so store the new contour point.                             */
         contour_x[i] = next_x_loc;
         contour_y[i] = next_y_loc;
         contour_ex[i] = next_x_edge;
         contour_ey[i] = next_y_edge;
         /* Bump the number of points stored. */
         ncontour++;

         /* Set up for finding next contour pixel. */
         cur_x_loc = next_x_loc;
         cur_y_loc = next_y_loc;
         cur_x_edge = next_x_edge;
         cur_y_edge = next_y_edge;
      }
      /* Otherwise, no new contour point found ... */
      else{
         /* So, stop short and return the number of pixels found */
         /* on the contour to this point.                        */
         *ocontour_x = contour_x;
         *ocontour_y = contour_y;
         *ocontour_ex = contour_ex;
         *ocontour_ey = contour_ey;
         *oncontour = ncontour;

         /* Return normally. */
         return(0);
      }
   }

   /* If we get here, we successfully found the maximum points we    */
   /* were looking for on the feature contour, so assign the contour */
   /* buffers to the output pointers and return.                     */
   *ocontour_x = contour_x;
   *ocontour_y = contour_y;
   *ocontour_ex = contour_ex;
   *ocontour_ey = contour_ey;
   *oncontour = ncontour;

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: search_contour - Walk the contour of a minutia feature starting at a
#cat:            specified point on the feature and walking N steps in the
#cat:            specified direction (clockwise or counter-clockwise), looking
#cat:            for a second specified point.  In this code, "feature" is
#cat:            consistently referring to either the black interior edge of
#cat:            a ridge-ending or the white interior edge of a valley-ending
#cat:            (bifurcation).  The term "edge of the feature" refers to
#cat:            neighboring pixels on the "exterior" edge of the feature.
#cat:            So "edge" pixels are opposite in color from the interior
#cat:            feature pixels.

   Input:
      x_search   - x-pixel coord of point being searched for
      y_search   - y-pixel coord of point being searched for
      search_len - number of step to walk contour in search
      x_loc      - starting x-pixel coord of feature (interior to feature)
      y_loc      - starting y-pixel coord of feature (interior to feature)
      x_edge     - x-pixel coord of corresponding edge pixel
                   (exterior to feature)
      y_edge     - y-pixel coord of corresponding edge pixel
                   (exterior to feature)
      scan_clock - direction in which neighbor pixels are to be scanned
                   (clockwise or counter-clockwise)
      bdata      - binary image data (0==while & 1==black)
      iw         - width (in pixels) of image
      ih         - height (in pixels) of image
   Return Code:
      NOT_FOUND  - desired pixel not found along N steps of feature's contour
      FOUND      - desired pixel WAS found along N steps of feature's contour
**************************************************************************/
int search_contour(const int x_search, const int y_search,
                   const int search_len,
                   const int x_loc, const int y_loc,
                   const int x_edge, const int y_edge,
                   const int scan_clock,
                   unsigned char *bdata, const int iw, const int ih)
{
   int cur_x_loc, cur_y_loc;
   int cur_x_edge, cur_y_edge;
   int next_x_loc, next_y_loc;
   int next_x_edge, next_y_edge;
   int i;

   /* Set up for finding first contour pixel. */
   cur_x_loc = x_loc;
   cur_y_loc = y_loc;
   cur_x_edge = x_edge;
   cur_y_edge = y_edge;

   /* Foreach point to be collected on the feature's contour... */
   for(i = 0; i < search_len; i++){
      /* Find the next contour pixel. */
      if(next_contour_pixel(&next_x_loc, &next_y_loc,
                            &next_x_edge, &next_y_edge,
                            cur_x_loc, cur_y_loc,
                            cur_x_edge, cur_y_edge,
                            scan_clock, bdata, iw, ih)){

         /* If we find the point we are looking for on the contour... */
         if((next_x_loc == x_search) && (next_y_loc == y_search)){
            /* Then return FOUND. */
            return(FOUND);
         }

         /* Otherwise, set up for finding next contour pixel. */
         cur_x_loc = next_x_loc;
         cur_y_loc = next_y_loc;
         cur_x_edge = next_x_edge;
         cur_y_edge = next_y_edge;
      }
      /* Otherwise, no new contour point found ... */
      else{
         /* So, stop searching, and return NOT_FOUND. */
         return(NOT_FOUND);
      }
   }

   /* If we get here, we successfully searched the maximum points */
   /* without finding our desired point, so return NOT_FOUND.     */
   return(NOT_FOUND);
}

/*************************************************************************
**************************************************************************
#cat: min_contour_theta - Takes a contour list and analyzes it locating the
#cat:            point at which the contour has highest curvature
#cat:            (or minimum interior angle).  The angle of curvature is
#cat:            computed by searching a majority of points on the contour.
#cat:            At each of these points, a left and right segment (or edge)
#cat:            are extended out N number of pixels from the center point
#cat:            on the contour.  The angle is formed between the straight line
#cat:            connecting the center point to the end point on the left edge
#cat:            and the line connecting the center point to the end of the
#cat:            right edge.  The point of highest curvature is determined
#cat:            by locating the where the minimum of these angles occurs.

   Input:
      angle_edge - length of the left and right edges extending from a
                   common/centered pixel on the contour
      contour_x  - x-coord list for contour points
      contour_y  - y-coord list for contour points
      ncontour   - number of points in contour
   Output:
      omin_i     - index of contour point where minimum occurred
      omin_theta - minimum angle found along the contour
   Return Code:
      Zero       - minimum angle successfully located
      IGNORE     - ignore the contour
      Negative   - system error
**************************************************************************/
int min_contour_theta(int *omin_i, double *omin_theta,
                      const int angle_edge,  const int *contour_x,
                      const int *contour_y, const int ncontour)
{
   int pleft, pcenter, pright;
   double theta1, theta2, dtheta;
   int min_i;
   double min_theta;

   /* If the contour length is too short for processing... */
   if(ncontour < (angle_edge<<1)+1)
      /* Return IGNORE. */
      return(IGNORE);

   /* Intialize running minimum values. */
   min_theta = M_PI;
   /* Need to truncate precision so that answers are consistent   */
   /* on different computer architectures when comparing doubles. */
   min_theta = trunc_dbl_precision(min_theta, TRUNC_SCALE);
   min_i = -1;

   /* Set left angle point to first contour point. */
   pleft = 0;
   /* Set center angle point to "angle_edge" points into contour. */
   pcenter = angle_edge;
   /* Set right angle point to "angle_edge" points from pcenter. */
   pright = pcenter + angle_edge;

   /* Loop until the right angle point exceeds the contour list. */
   while(pright < ncontour){
      /* Compute angle to first edge line (connecting pcenter to pleft). */
      theta1 = angle2line(contour_x[pcenter],contour_y[pcenter],
                          contour_x[pleft],contour_y[pleft]);
      /* Compute angle to second edge line (connecting pcenter to pright). */
      theta2 = angle2line(contour_x[pcenter],contour_y[pcenter],
                          contour_x[pright],contour_y[pright]);

      /* Compute delta between angles accounting for an inner */
      /* and outer distance between the angles.               */
      dtheta = fabs(theta2 - theta1);
      dtheta = min(dtheta, (M_PI*2.0)-dtheta);
      /* Need to truncate precision so that answers are consistent   */
      /* on different computer architectures when comparing doubles. */
      dtheta = trunc_dbl_precision(dtheta, TRUNC_SCALE);

      /* Keep track of running minimum theta. */
      if(dtheta < min_theta){
         min_i = pcenter;
         min_theta = dtheta;
      }

      /* Bump to next points on contour. */
      pleft++;
      pcenter++;
      pright++;
   }

   /* If no minimum found (then contour is perfectly flat) so minimum */
   /* to center point on contour.                                     */
   if(min_i == -1){
      *omin_i = ncontour>>1;
      *omin_theta = min_theta;
   }
   else{
      /* Assign minimum theta information to output pointers. */
      *omin_i = min_i;
      *omin_theta = min_theta;
   }

   /* Return successfully. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: contour_limits - Determines the X and Y coordinate limits of the
#cat:                  given contour list.

   Input:
      contour_x  - x-coord list for contour points
      contour_y  - y-coord list for contour points
      ncontour   - number of points in contour
   Output:
      xmin   - left-most x-coord in contour
      ymin   - top-most y-coord in contour
      xmax   - right-most x-coord in contour
      ymax   - bottom-most y-coord in contour
**************************************************************************/
void contour_limits(int *xmin, int *ymin, int *xmax, int *ymax,
           const int *contour_x, const int *contour_y, const int ncontour)
{
   /* Find the minimum x-coord from the list of contour points. */
   *xmin = minv(contour_x, ncontour);
   /* Find the minimum y-coord from the list of contour points. */
   *ymin = minv(contour_y, ncontour);
   /* Find the maximum x-coord from the list of contour points. */
   *xmax = maxv(contour_x, ncontour);
   /* Find the maximum y-coord from the list of contour points. */
   *ymax = maxv(contour_y, ncontour);
}

/*************************************************************************
**************************************************************************
#cat: fix_edge_pixel_pair - Takes a pair of pixel points with the first
#cat:       pixel on a feature and the second adjacent and off the feature,
#cat:       determines if the pair neighbor diagonally.  If they do, their
#cat:       locations are adjusted so that the resulting pair retains the
#cat:       same pixel values, but are neighboring either to the N,S,E or W.
#cat:       This routine is needed in order to prepare the pixel pair for
#cat:       contour tracing.

   Input:
      feat_x  - pointer to x-pixel coord on feature
      feat_y  - pointer to y-pixel coord on feature
      edge_x  - pointer to x-pixel coord on edge of feature
      edge_y  - pointer to y-pixel coord on edge of feature
      bdata   - binary image data (0==while & 1==black)
      iw      - width (in pixels) of image
      ih      - height (in pixels) of image
   Output:
      feat_x  - pointer to resulting x-pixel coord on feature
      feat_y  - pointer to resulting y-pixel coord on feature
      edge_x  - pointer to resulting x-pixel coord on edge of feature
      edge_y  - pointer to resulting y-pixel coord on edge of feature
**************************************************************************/
void fix_edge_pixel_pair(int *feat_x, int *feat_y, int *edge_x, int *edge_y,
                         unsigned char *bdata, const int iw, const int ih)
{
   int dx, dy;
   int px, py, cx, cy;
   int feature_pix;

   /* Get the pixel value of the feature. */
   feature_pix = *(bdata + ((*feat_y) * iw) + (*feat_x));

   /* Store the input points to current and previous points. */
   cx = *feat_x;
   cy = *feat_y;
   px = *edge_x;
   py = *edge_y;

   /* Compute detlas between current and previous point. */
   dx = px - cx;
   dy = py - cy;

   /* If previous point (P) is diagonal neighbor of    */
   /* current point (C)... This is a problem because   */
   /* the contour tracing routine requires that the    */
   /* "edge" pixel be north, south, east, or west of   */
   /* of the feature point.  If the previous pixel is  */
   /* diagonal neighbor, then we need to adjust either */
   /* the positon of te previous or current pixel.     */
   if((abs(dx)==1) && (abs(dy)==1)){
      /* Then we have one of the 4 following conditions:  */
      /*                                                  */
      /*        *C                             C*         */
      /*    1.  P*     2.  P*     3. *P     4. *P         */
      /*                   *C        C*                   */
      /*                                                  */
      /*  dx =  -1         -1         1         1         */
      /*  dy =   1         -1        -1         1         */
      /*                                                  */
      /* Want to test values in positions of '*':         */
      /*  Let point P == (px, py)                         */
      /*           p1 == '*' positon where x changes      */
      /*           p2 == '*' positon where y changes      */
      /*                                                  */
      /*  p1 = px+1,py    px+1,py   px-1,py    px-1,py    */
      /*  p2 = px,py-1    px,py+1   px,py+1    px,py-1    */
      /*                                                  */
      /* These can all be rewritten:                      */
      /*  p1 = px-dx,py                                   */
      /*  p2 = px,py-dy                                   */

      /* Check if 'p1' is NOT the value we are searching for... */
      if(*(bdata+(py*iw)+(px-dx)) != feature_pix)
         /* Then set x-coord of edge pixel to p1. */
         px -= dx;
      /* Check if 'p2' is NOT the value we are searching for... */
      else if(*(bdata+((py-dy)*iw)+px) != feature_pix)
         /* Then set y-coord of edge pixel to p2. */
         py -= dy;
      /* Otherwise, the current pixel 'C' is exposed on a corner ... */
      else{
         /* Set pixel 'C' to 'p1', which also has the pixel */
         /* value we are searching for.                     */
         cy += dy;
      }

      /* Set the pointers to the resulting values. */
      *feat_x = cx;
      *feat_y = cy;
      *edge_x = px;
      *edge_y = py;
   }

   /* Otherwise, nothing has changed. */
}

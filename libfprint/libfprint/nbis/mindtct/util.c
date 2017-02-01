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

      FILE:    UTIL.C
      AUTHOR:  Michael D. Garris
      DATE:    03/16/1999

      Contains general support routines required by the NIST
      Latent Fingerprint System (LFS).

***********************************************************************
               ROUTINES:
                        maxv()
                        minv()
                        minmaxs()
                        distance()
                        squared_distance()
                        in_int_list()
                        remove_from_int_list()
                        find_incr_position_dbl()
                        angle2line()
                        line2direction()
                        closest_dir_dist()
***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <lfs.h>

/*************************************************************************
**************************************************************************
#cat: maxv - Determines the maximum value in the given list of integers.
#cat:        NOTE, the list is assumed to be NOT empty!

   Input:
      list - non-empty list of integers to be searched
      num  - number of integers in the list
   Return Code:
      Maximum - maximum value in the list
**************************************************************************/
int maxv(const int *list, const int num)
{
   int i;
   int maxval;

   /* NOTE: The list is assumed to be NOT empty. */
   /* Initialize running maximum to first item in list. */
   maxval = list[0];

   /* Foreach subsequent item in the list... */
   for(i = 1; i < num; i++){
      /* If current item is larger than running maximum... */
      if(list[i] > maxval)
         /* Set running maximum to the larger item. */
         maxval = list[i];
      /* Otherwise, skip to next item. */
   }

   /* Return the resulting maximum. */
   return(maxval);
}

/*************************************************************************
**************************************************************************
#cat: minv - Determines the minimum value in the given list of integers.
#cat:        NOTE, the list is assumed to be NOT empty!

   Input:
      list - non-empty list of integers to be searched
      num  - number of integers in the list
   Return Code:
      Minimum - minimum value in the list
**************************************************************************/
int minv(const int *list, const int num)
{
   int i;
   int minval;

   /* NOTE: The list is assumed to be NOT empty. */
   /* Initialize running minimum to first item in list. */
   minval = list[0];

   /* Foreach subsequent item in the list... */
   for(i = 1; i < num; i++){
      /* If current item is smaller than running minimum... */
      if(list[i] < minval)
         /* Set running minimum to the smaller item. */
         minval = list[i];
      /* Otherwise, skip to next item. */
   }

   /* Return the resulting minimum. */
   return(minval);
}

/*************************************************************************
**************************************************************************
#cat: minmaxs - Takes a list of integers and identifies points of relative
#cat:           minima and maxima.  The midpoint of flat plateaus and valleys
#cat:           are selected when they are detected.

   Input:
      items     - list of integers to be analyzed
      num       - number of items in the list
   Output:
      ominmax_val   - value of the item at each minima or maxima
      ominmax_type  - identifies a minima as '-1' and maxima as '1'
      ominmax_i     - index of item's position in list
      ominmax_alloc - number of allocated minima and/or maxima
      ominmax_num   - number of detected minima and/or maxima
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
int minmaxs(int **ominmax_val, int **ominmax_type, int **ominmax_i,
            int *ominmax_alloc, int *ominmax_num,
            const int *items, const int num)
{
   int i, diff, state, start, loc;
   int *minmax_val, *minmax_type, *minmax_i, minmax_alloc, minmax_num;

   
   /* Determine maximum length for allocation of buffers. */
   /* If there are fewer than 3 items ...                */
   if(num < 3){
      /* Then no min/max is possible, so set allocated length */
      /* to 0 and return.                                     */
      *ominmax_alloc = 0;
      *ominmax_num = 0;
      return(0);
   }
   /* Otherwise, set allocation length to number of items - 2    */
   /* (one for the first item in the list, and on for the last). */
   /* Every other intermediate point can potentially represent a */
   /* min or max.                                                */
   minmax_alloc = num - 2;
   /* Allocate the buffers. */
   minmax_val = (int *)malloc(minmax_alloc * sizeof(int));
   if(minmax_val == (int *)NULL){
      fprintf(stderr, "ERROR : minmaxs : malloc : minmax_val\n");
      return(-290);
   }
   minmax_type = (int *)malloc(minmax_alloc * sizeof(int));
   if(minmax_type == (int *)NULL){
      free(minmax_val);
      fprintf(stderr, "ERROR : minmaxs : malloc : minmax_type\n");
      return(-291);
   }
   minmax_i = (int *)malloc(minmax_alloc * sizeof(int));
   if(minmax_i == (int *)NULL){
      free(minmax_val);
      free(minmax_type);
      fprintf(stderr, "ERROR : minmaxs : malloc : minmax_i\n");
      return(-292);
   }

   /* Initialize number of min/max to 0. */
   minmax_num = 0;

   /* Start witht the first item in the list. */
   i = 0;

   /* Get starting state between first pair of items. */
   diff = items[1] - items[0];
   if(diff > 0)
      state = 1;
   else if (diff < 0)
      state = -1;
   else
      state = 0;

   /* Set start location to first item in list. */
   start = 0;

   /* Bump to next item in list. */
   i++;

   /* While not at the last item in list. */
   while(i < num-1){

      /* Compute difference between next pair of items. */
      diff = items[i+1] - items[i];
      /* If items are increasing ... */
      if(diff > 0){
         /* If previously increasing ... */
         if(state == 1){
            /* Reset start to current location. */
            start = i;
         }
         /* If previously decreasing ... */
         else if (state == -1){
            /* Then we have incurred a minima ... */
            /* Compute midpoint of minima. */
            loc = (start + i)/2;
            /* Store value at minima midpoint. */
            minmax_val[minmax_num] = items[loc];
            /* Store type code for minima. */
            minmax_type[minmax_num] = -1;
            /* Store location of minima midpoint. */
            minmax_i[minmax_num++] = loc;
            /* Change state to increasing. */
            state = 1;
            /* Reset start location. */
            start = i;
         }
         /* If previously level (this state only can occur at the */
         /* beginning of the list of items) ...                   */
         else {
            /* If more than one level state in a row ... */
            if(i-start > 1){
               /* Then consider a minima ... */
               /* Compute midpoint of minima. */
               loc = (start + i)/2;
               /* Store value at minima midpoint. */
               minmax_val[minmax_num] = items[loc];
               /* Store type code for minima. */
               minmax_type[minmax_num] = -1;
               /* Store location of minima midpoint. */
               minmax_i[minmax_num++] = loc;
               /* Change state to increasing. */
               state = 1;
               /* Reset start location. */
               start = i;
            }
            /* Otherwise, ignore single level state. */
            else{
               /* Change state to increasing. */
               state = 1;
               /* Reset start location. */
               start = i;
            }
         }
      }
      /* If items are decreasing ... */
      else if(diff < 0){
         /* If previously decreasing ... */
         if(state == -1){
            /* Reset start to current location. */
            start = i;
         }
         /* If previously increasing ... */
         else if (state == 1){
            /* Then we have incurred a maxima ... */
            /* Compute midpoint of maxima. */
            loc = (start + i)/2;
            /* Store value at maxima midpoint. */
            minmax_val[minmax_num] = items[loc];
            /* Store type code for maxima. */
            minmax_type[minmax_num] = 1;
            /* Store location of maxima midpoint. */
            minmax_i[minmax_num++] = loc;
            /* Change state to decreasing. */
            state = -1;
            /* Reset start location. */
            start = i;
         }
         /* If previously level (this state only can occur at the */
         /* beginning of the list of items) ...                   */
         else {
            /* If more than one level state in a row ... */
            if(i-start > 1){
               /* Then consider a maxima ... */
               /* Compute midpoint of maxima. */
               loc = (start + i)/2;
               /* Store value at maxima midpoint. */
               minmax_val[minmax_num] = items[loc];
               /* Store type code for maxima. */
               minmax_type[minmax_num] = 1;
               /* Store location of maxima midpoint. */
               minmax_i[minmax_num++] = loc;
               /* Change state to decreasing. */
               state = -1;
               /* Reset start location. */
               start = i;
            }
            /* Otherwise, ignore single level state. */
            else{
               /* Change state to decreasing. */
               state = -1;
               /* Reset start location. */
               start = i;
            }
         }
      }
      /* Otherwise, items are level, so continue to next item pair. */

      /* Advance to next item pair in list. */
      i++;
   }

   /* Set results to output pointers. */
   *ominmax_val   = minmax_val;
   *ominmax_type  = minmax_type;
   *ominmax_i     = minmax_i;
   *ominmax_alloc = minmax_alloc;
   *ominmax_num   = minmax_num;

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: distance - Takes two coordinate points and computes the
#cat:            Euclidean distance between the two points.

   Input:
      x1  - x-coord of first point
      y1  - y-coord of first point
      x2  - x-coord of second point
      y2  - y-coord of second point
   Return Code:
      Distance - computed Euclidean distance
**************************************************************************/
double distance(const int x1, const int y1, const int x2, const int y2)
{
   double dx, dy, dist;

   /* Compute delta x between points. */
   dx = (double)(x1 - x2);
   /* Compute delta y between points. */
   dy = (double)(y1 - y2);
   /* Compute the squared distance between points. */
   dist = (dx*dx) + (dy*dy);
   /* Take square root of squared distance. */
   dist = sqrt(dist);

   /* Return the squared distance. */
   return(dist);
}

/*************************************************************************
**************************************************************************
#cat: squared_distance - Takes two coordinate points and computes the
#cat:                    squared distance between the two points.

   Input:
      x1  - x-coord of first point
      y1  - y-coord of first point
      x2  - x-coord of second point
      y2  - y-coord of second point
   Return Code:
      Distance - computed squared distance
**************************************************************************/
double squared_distance(const int x1, const int y1, const int x2, const int y2)
{
   double dx, dy, dist;

   /* Compute delta x between points. */
   dx = (double)(x1 - x2);
   /* Compute delta y between points. */
   dy = (double)(y1 - y2);
   /* Compute the squared distance between points. */
   dist = (dx*dx) + (dy*dy);

   /* Return the squared distance. */
   return(dist);
}

/*************************************************************************
**************************************************************************
#cat: in_int_list - Determines if a specified value is store in a list of
#cat:               integers and returns its location if found.

   Input:
      item    - value to search for in list
      list    - list of integers to be searched
      len     - number of integers in search list
   Return Code:
      Zero or greater - first location found equal to search value
      Negative        - search value not found in the list of integers
**************************************************************************/
int  in_int_list(const int item, const int *list, const int len)
{
   int i;

   /* Foreach item in list ... */
   for(i = 0; i < len; i++){
      /* If search item found in list ... */
      if(list[i] == item)
         /* Return the location in list where found. */
         return(i);
   }

   /* If we get here, then search item not found in list, */
   /* so return -1 ==> NOT FOUND.                         */
   return(-1);
}

/*************************************************************************
**************************************************************************
#cat: remove_from_int_list - Takes a position index into an integer list and
#cat:                removes the value from the list, collapsing the resulting
#cat:                list.

   Input:
      index      - position of value to be removed from list
      list       - input list of integers
      num        - number of integers in the list
   Output:
      list       - list with specified integer removed
      num        - decremented number of integers in list
   Return Code:
      Zero      - successful completion
      Negative  - system error
**************************************************************************/
int remove_from_int_list(const int index, int *list, const int num)
{
   int fr, to;

   /* Make sure the requested index is within range. */
   if((index < 0) && (index >= num)){
      fprintf(stderr, "ERROR : remove_from_int_list : index out of range\n");
      return(-370);
   }

   /* Slide the remaining list of integers up over top of the */
   /* position of the integer being removed.                  */
   for(to = index, fr = index+1; fr < num; to++, fr++)
      list[to] = list[fr];

   /* NOTE: Decrementing the number of integers remaining in the list is */
   /*       the responsibility of the caller!                            */

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: ind_incr_position_dbl - Takes a double value and a list of doubles and
#cat:               determines where in the list the double may be inserted,
#cat:               preserving the increasing sorted order of the list.

   Input:
      val  - value to be inserted into the list
      list - list of double in increasing sorted order
      num  - number of values in the list
   Return Code:
      Zero or Positive - insertion position in the list
**************************************************************************/
int find_incr_position_dbl(const double val, double *list, const int num)
{
   int i;

   /* Foreach item in double list ... */
   for(i = 0; i < num; i++){
      /* If the value is smaller than the current item in list ... */
      if(val < list[i])
         /* Then we found were to insert the value in the list maintaining */
         /* an increasing sorted order.                                    */
         return(i);

      /* Otherwise, the value is still larger than current item, so */
      /* continue to next item in the list.                         */
   }

   /* Otherwise, we never found a slot within the list to insert the */
   /* the value, so place at the end of the sorted list.             */
   return(i);
}

/*************************************************************************
**************************************************************************
#cat: angle2line - Takes two coordinate points and computes the angle
#cat:            to the line formed by the two points.

   Input:
      fx         - x-coord of first point
      fy         - y-coord of first point
      tx         - x-coord of second point
      ty         - y-coord of second point
   Return Code:
      Angle - angle to the specified line
**************************************************************************/
double angle2line(const int fx, const int fy, const int tx, const int ty)
{
   double dx, dy, theta;

   /* Compute slope of line connecting the 2 specified points. */
   dy = (double)(fy - ty);
   dx = (double)(tx - fx);
   /* If delta's are sufficiently small ... */
   if((fabs(dx) < MIN_SLOPE_DELTA) && (fabs(dy) < MIN_SLOPE_DELTA))
      theta = 0.0;
   /* Otherwise, compute angle to the line. */
   else
      theta = atan2(dy, dx);

   /* Return the compute angle in radians. */
   return(theta);
}

/*************************************************************************
**************************************************************************
#cat: line2direction - Takes two coordinate points and computes the
#cat:            directon (on a full circle) in which the first points
#cat:            to the second.

   Input:
      fx         - x-coord of first point (pointing from)
      fy         - y-coord of first point (pointing from)
      tx         - x-coord of second point (pointing to)
      ty         - y-coord of second point (pointing to)
      ndirs      - number of IMAP directions (in semicircle)
   Return Code:
      Direction  - determined direction on a "full" circle
**************************************************************************/
int line2direction(const int fx, const int fy,
                   const int tx, const int ty, const int ndirs)
{
   double theta, pi_factor;
   int idir, full_ndirs;
   static double pi2 = M_PI*2.0;

   /* Compute angle to line connecting the 2 points.             */
   /* Coordinates are swapped and order of points reversed to    */
   /* account for 0 direction is vertical and positive direction */
   /* is clockwise.                                              */
   theta = angle2line(ty, tx, fy, fx);

   /* Make sure the angle is positive. */
   theta += pi2;
   theta = fmod(theta, pi2);
   /* Convert from radians to integer direction on range [0..(ndirsX2)]. */
   /* Multiply radians by units/radian ((ndirsX2)/(2PI)), and you get    */
   /* angle in integer units.                                            */
   /* Compute number of directions on full circle. */
   full_ndirs = ndirs<<1;
   /* Compute the radians to integer direction conversion factor. */
   pi_factor = (double)full_ndirs/pi2;
   /* Convert radian angle to integer direction on full circle. */
   theta *= pi_factor;
   /* Need to truncate precision so that answers are consistent */
   /* on different computer architectures when rounding doubles. */
   theta = trunc_dbl_precision(theta, TRUNC_SCALE);
   idir = sround(theta);
   /* Make sure on range [0..(ndirsX2)]. */
   idir %= full_ndirs;

   /* Return the integer direction. */   
   return(idir);
}

/*************************************************************************
**************************************************************************
#cat: closest_dir_dist - Takes to integer IMAP directions and determines the
#cat:                    closest distance between them accounting for
#cat:                    wrap-around either at the beginning or ending of
#cat:                    the range of directions.

   Input:
      dir1  - integer value of the first direction
      dir2  - integer value of the second direction
      ndirs - the number of possible directions
   Return Code:
      Non-negative - distance between the 2 directions
**************************************************************************/
int closest_dir_dist(const int dir1, const int dir2, const int ndirs)
{
   int d1, d2, dist;

   /* Initialize distance to -1 = INVALID. */
   dist = INVALID_DIR;

   /* Measure shortest distance between to directions. */
   /* If both neighbors are VALID ... */
   if((dir1 >= 0)&&(dir2 >= 0)){
      /* Compute inner and outer distances to account for distances */
      /* that wrap around the end of the range of directions, which */
      /* may in fact be closer.                                     */
      d1 = abs(dir2 - dir1);
      d2 = ndirs - d1;
      dist = min(d1, d2);
   }
   /* Otherwise one or both directions are INVALID, so ignore */
   /* and return INVALID. */

   /* Return determined closest distance. */
   return(dist);
}


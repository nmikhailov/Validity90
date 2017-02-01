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

      FILE:    RIDGES.C
      AUTHOR:  Michael D. Garris
      DATE:    08/09/1999
      UPDATED: 03/16/2005 by MDG

      Contains routines responsible for locating nearest minutia
      neighbors and counting intervening ridges as part of the
      NIST Latent Fingerprint System (LFS).

***********************************************************************
               ROUTINES:
                        count_minutiae_ridges()
                        count_minutia_ridges()
                        find_neighbors()
                        update_nbr_dists()
                        insert_neighbor()
                        sort_neighbors()
                        ridge_count()
                        find_transition()
                        validate_ridge_crossing()
***********************************************************************/

#include <stdio.h>
#include <lfs.h>
#include <log.h>

/*************************************************************************
**************************************************************************
#cat: insert_neighbor - Takes a minutia index and its squared distance to a
#cat:               primary minutia point, and inserts them in the specified
#cat:               position of their respective lists, shifting previously
#cat:               stored values down and off the lists as necessary.

   Input:
      pos       - postions where values are to be inserted in lists
      nbr_index - index of minutia being inserted
      nbr_dist2 - squared distance of minutia to its primary point
      nbr_list  - current list of nearest neighbor minutia indices
      nbr_sqr_dists - corresponding squared euclidean distance of each
                  neighbor to the primary minutia point
      nnbrs     - number of neighbors currently in the list
      max_nbrs  - maximum number of closest neighbors to be returned
   Output:
      nbr_list - updated list of nearest neighbor indices
      nbr_sqr_dists - updated list of nearest neighbor distances
      nnbrs    - number of neighbors in the update lists
   Return Code:
      Zero      - successful completion
      Negative  - system error
**************************************************************************/
static int insert_neighbor(const int pos, const int nbr_index, const double nbr_dist2,
                    int *nbr_list, double *nbr_sqr_dists,
                    int *nnbrs, const int max_nbrs)
{
   int i;

   /* If the desired insertion position is beyond one passed the last     */
   /* neighbor in the lists OR greater than equal to the maximum ...      */
   /* NOTE: pos is zero-oriented while nnbrs and max_nbrs are 1-oriented. */
   if((pos > *nnbrs) ||
      (pos >= max_nbrs)){
      fprintf(stderr,
              "ERROR : insert_neighbor : insertion point exceeds lists\n");
      return(-480);
   }

   /* If the neighbor lists are NOT full ... */
   if(*nnbrs < max_nbrs){
      /* Then we have room to shift everything down to make room for new */
      /* neighbor and increase the number of neighbors stored by 1.      */
      i = *nnbrs-1;
      (*nnbrs)++;
   }
   /* Otherwise, the neighbors lists are full ... */
   else if(*nnbrs == max_nbrs)
      /* So, we must bump the last neighbor in the lists off to make */
      /* room for the new neighbor (ignore last neighbor in lists).  */
      i = *nnbrs-2;
   /* Otherwise, there is a list overflow error condition */
   /* (shouldn't ever happen, but just in case) ...       */
   else{
      fprintf(stderr,
              "ERROR : insert_neighbor : overflow in neighbor lists\n");
      return(-481);
   }

   /* While we havn't reached the desired insertion point ... */
   while(i >= pos){
      /* Shift the current neighbor down the list 1 positon. */
      nbr_list[i+1] = nbr_list[i];
      nbr_sqr_dists[i+1] = nbr_sqr_dists[i];
      i--;
   }

   /* We are now ready to put our new neighbor in the position where */
   /* we shifted everything down from to make room.                  */
   nbr_list[pos] = nbr_index;
   nbr_sqr_dists[pos] = nbr_dist2;

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: update_nbr_dists - Takes the current list of neighbors along with a
#cat:               primary minutia and a potential new neighbor, and
#cat:               determines if the new neighbor is sufficiently close
#cat:               to be added to the list of nearest neighbors.  If added,
#cat:               it is placed in the list in its proper order based on
#cat:               squared distance to the primary point.

   Input:
      nbr_list - current list of nearest neighbor minutia indices
      nbr_sqr_dists - corresponding squared euclidean distance of each
                 neighbor to the primary minutia point
      nnbrs    - number of neighbors currently in the list
      max_nbrs - maximum number of closest neighbors to be returned
      first    - index of the primary minutia point
      second   - index of the secondary (new neighbor) point
      minutiae - list of minutiae
   Output:
      nbr_list - updated list of nearest neighbor indices
      nbr_sqr_dists - updated list of nearest neighbor distances
      nnbrs    - number of neighbors in the update lists
   Return Code:
      Zero      - successful completion
      Negative  - system error
**************************************************************************/
static int update_nbr_dists(int *nbr_list, double *nbr_sqr_dists,
                      int *nnbrs, const int max_nbrs,
                      const int first, const int second, MINUTIAE *minutiae)
{
   double dist2;
   MINUTIA *minutia1, *minutia2;
   int pos, last_nbr;

   /* Compute position of maximum last neighbor stored. */
   last_nbr = max_nbrs - 1;

   /* Assigne temporary minutia pointers. */
   minutia1 = minutiae->list[first];
   minutia2 = minutiae->list[second];

   /* Compute squared euclidean distance between minutia pair. */
   dist2 = squared_distance(minutia1->x, minutia1->y,
                            minutia2->x, minutia2->y);

   /* If maximum number of neighbors not yet stored in lists OR */
   /* if the squared distance to current secondary is less      */
   /* than the largest stored neighbor distance ...             */
   if((*nnbrs < max_nbrs) ||
      (dist2 < nbr_sqr_dists[last_nbr])){

      /* Find insertion point in neighbor lists. */
      pos = find_incr_position_dbl(dist2, nbr_sqr_dists, *nnbrs);
      /* If the position returned is >= maximum list length (this should */
      /* never happen, but just in case) ...                             */
      if(pos >= max_nbrs){
         fprintf(stderr, 
         "ERROR : update_nbr_dists : illegal position for new neighbor\n");
         return(-470);
      }
      /* Insert the new neighbor into the neighbor lists at the */
      /* specified location.                                    */
      if(insert_neighbor(pos, second, dist2,
                         nbr_list, nbr_sqr_dists, nnbrs, max_nbrs))
         return(-471);

      /* Otherwise, neighbor inserted successfully, so return normally. */
      return(0);
   }
   /* Otherwise, the new neighbor is not sufficiently close to be       */
   /* added or inserted into the neighbor lists, so ignore the neighbor */
   /* and return normally.                                              */
   else
      return(0);

}

/*************************************************************************
**************************************************************************
#cat: find_neighbors - Takes a primary minutia and a list of all minutiae
#cat:               and locates a specified maximum number of closest neighbors
#cat:               to the primary point.  Neighbors are searched, starting
#cat:               in the same pixel column, below, the primary point and then
#cat:               along consecutive and complete pixel columns in the image
#cat:               to the right of the primary point.

   Input:
      max_nbrs - maximum number of closest neighbors to be returned
      first    - index of the primary minutia point
      minutiae - list of minutiae
   Output:
      onbr_list - points to list of detected closest neighbors
      onnbrs    - points to number of neighbors returned
   Return Code:
      Zero      - successful completion
      Negative  - system error
**************************************************************************/
static int find_neighbors(int **onbr_list, int *onnbrs, const int max_nbrs,
                   const int first, MINUTIAE *minutiae)
{
   int ret, second, last_nbr;
   MINUTIA *minutia1, *minutia2;
   int *nbr_list, nnbrs;
   double *nbr_sqr_dists, xdist, xdist2;

   /* Allocate list of neighbor minutiae indices. */
   nbr_list = (int *)malloc(max_nbrs * sizeof(int));
   if(nbr_list == (int *)NULL){
      fprintf(stderr, "ERROR : find_neighbors : malloc : nbr_list\n");
      return(-460);
   }

   /* Allocate list of squared euclidean distances between neighbors */
   /* and current primary minutia point.                             */
   nbr_sqr_dists = (double *)malloc(max_nbrs * sizeof(double));
   if(nbr_sqr_dists == (double *)NULL){
      free(nbr_list);
      fprintf(stderr,
              "ERROR : find_neighbors : malloc : nbr_sqr_dists\n");
      return(-461);
   }

   /* Initialize number of stored neighbors to 0. */
   nnbrs = 0;
   /* Assign secondary to one passed current primary minutia. */
   second = first + 1;
   /* Compute location of maximum last stored neighbor. */
   last_nbr = max_nbrs - 1;

   /* While minutia (in sorted order) still remian for processing ... */
   /* NOTE: The minutia in the input list have been sorted on X and   */
   /* then on Y.  So, the neighbors are selected according to those   */
   /* that lie below the primary minutia in the same pixel column and */
   /* then subsequently those that lie in complete pixel columns to   */
   /* the right of the primary minutia.                               */
   while(second < minutiae->num){
      /* Assign temporary minutia pointers. */
      minutia1 = minutiae->list[first];
      minutia2 = minutiae->list[second];

      /* Compute squared distance between minutiae along x-axis. */
      xdist = minutia2->x - minutia1->x;
      xdist2 = xdist * xdist;

      /* If the neighbor lists are not full OR the x-distance to current */
      /* secondary is smaller than maximum neighbor distance stored ...  */
      if((nnbrs < max_nbrs) ||
         (xdist2 < nbr_sqr_dists[last_nbr])){
         /* Append or insert the new neighbor into the neighbor lists. */
         if((ret = update_nbr_dists(nbr_list, nbr_sqr_dists, &nnbrs, max_nbrs,
                          first, second, minutiae))){
            free(nbr_sqr_dists);
            return(ret);
         }
      }
      /* Otherwise, if the neighbor lists is full AND the x-distance   */
      /* to current secondary is larger than maximum neighbor distance */
      /* stored ...                                                    */
      else
         /* So, stop searching for more neighbors. */
         break;

       /* Bump to next secondary minutia. */
       second++;
   }

   /* Deallocate working memory. */
   free(nbr_sqr_dists);

   /* If no neighbors found ... */
   if(nnbrs == 0){
      /* Deallocate the neighbor list. */
      free(nbr_list);
      *onnbrs = 0;
   }
   /* Otherwise, assign neighbors to output pointer. */
   else{
      *onbr_list = nbr_list;
      *onnbrs = nnbrs;
   }

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: sort_neighbors - Takes a list of primary minutia and its neighboring
#cat:               minutia indices and sorts the neighbors based on their
#cat:               position relative to the primary minutia point.  Neighbors
#cat:               are sorted starting vertical to the primary point and
#cat:               proceeding clockwise.

   Input:
      nbr_list - list of neighboring minutia indices
      nnbrs    - number of neighbors in the list
      first    - the index of the primary minutia point
      minutiae - list of minutiae
   Output:
      nbr_list - neighboring minutia indices in sorted order
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
static int sort_neighbors(int *nbr_list, const int nnbrs, const int first,
                   MINUTIAE *minutiae)
{
   double *join_thetas, theta;
   int i;
   static double pi2 = M_PI*2.0;

   /* List of angles of lines joining the current primary to each */
   /* of the secondary neighbors.                                 */
   join_thetas = (double *)malloc(nnbrs * sizeof(double));
   if(join_thetas == (double *)NULL){
      fprintf(stderr, "ERROR : sort_neighbors : malloc : join_thetas\n");
      return(-490);
   }

   for(i = 0; i < nnbrs; i++){
      /* Compute angle to line connecting the 2 points.             */
      /* Coordinates are swapped and order of points reversed to    */
      /* account for 0 direction is vertical and positive direction */
      /* is clockwise.                                              */
      theta = angle2line(minutiae->list[nbr_list[i]]->y,
                         minutiae->list[nbr_list[i]]->x,
                         minutiae->list[first]->y,
                         minutiae->list[first]->x);

      /* Make sure the angle is positive. */
      theta += pi2;
      theta = fmod(theta, pi2);
      join_thetas[i] = theta;
   }

   /* Sort the neighbor indicies into rank order. */
   bubble_sort_double_inc_2(join_thetas, nbr_list, nnbrs);

   /* Deallocate the list of angles. */
   free(join_thetas);

   /* Return normally. */
   return(0);      
}

/*************************************************************************
**************************************************************************
#cat: find_transition - Takes a pixel trajectory and a starting index, and
#cat:               searches forward along the trajectory until the specified
#cat:               adjacent pixel pair is found, returning the index where
#cat:               the pair was found (the index of the second pixel).

   Input:
      iptr  - pointer to starting pixel index into trajectory
      pix1  - first pixel value in transition pair
      pix2  - second pixel value in transition pair
      xlist - x-pixel coords of line trajectory
      ylist - y-pixel coords of line trajectory
      num   - number of coords in line trajectory
      bdata - binary image data (0==while & 1==black)
      iw    - width (in pixels) of image
      ih    - height (in pixels) of image
   Output:
      iptr  - points to location where 2nd pixel in pair is found
   Return Code:
      TRUE  - pixel pair transition found
      FALSE - pixel pair transition not found
**************************************************************************/
static int find_transition(int *iptr, const int pix1, const int pix2,
                    const int *xlist, const int *ylist, const int num,
                    unsigned char *bdata, const int iw, const int ih)
{
   int i, j;

   /* Set previous index to starting position. */
   i = *iptr;
   /* Bump previous index by 1 to get next index. */
   j = i+1;

   /* While not one point from the end of the trajectory .. */
   while(i < num-1){
      /* If we have found the desired transition ... */
      if((*(bdata+(ylist[i]*iw)+xlist[i]) == pix1) &&
         (*(bdata+(ylist[j]*iw)+xlist[j]) == pix2)){
         /* Adjust the position pointer to the location of the */
         /* second pixel in the transition.                    */
         *iptr = j;

         /* Return TRUE. */
         return(TRUE);
      }
      /* Otherwise, the desired transition was not found in current */
      /* pixel pair, so bump to the next pair along the trajector.  */
      i++;
      j++;
   }

   /* If we get here, then we exhausted the trajector without finding */
   /* the desired transition, so set the position pointer to the end  */
   /* of the trajector, and return FALSE.                             */
   *iptr = num;
   return(FALSE);
}

/*************************************************************************
**************************************************************************
#cat: validate_ridge_crossing - Takes a pair of points, a ridge start
#cat:               transition and a ridge end transition, and walks the
#cat:               ridge contour from thre ridge end points a specified
#cat:               number of steps, looking for the ridge start point.
#cat:               If found, then transitions determined not to be a valid
#cat:               ridge crossing.

   Input:
      ridge_start - index into line trajectory of ridge start transition
      ridge_end   - index into line trajectory of ridge end transition
      xlist       - x-pixel coords of line trajectory
      ylist       - y-pixel coords of line trajectory
      num         - number of coords in line trajectory
      bdata       - binary image data (0==while & 1==black)
      iw          - width (in pixels) of image
      ih          - height (in pixels) of image
      max_ridge_steps  - number of steps taken in search in both
                         scan directions
   Return Code:
      TRUE        - ridge crossing VALID
      FALSE       - ridge corssing INVALID
      Negative    - system error
**************************************************************************/
static int validate_ridge_crossing(const int ridge_start, const int ridge_end,
                            const int *xlist, const int *ylist, const int num,
                            unsigned char *bdata, const int iw, const int ih,
                            const int max_ridge_steps)
{
   int ret;
   int feat_x, feat_y, edge_x, edge_y;
   int *contour_x, *contour_y, *contour_ex, *contour_ey, ncontour;

   /* Assign edge pixel pair for contour trace. */
   feat_x = xlist[ridge_end];
   feat_y = ylist[ridge_end];
   edge_x = xlist[ridge_end-1];
   edge_y = ylist[ridge_end-1];

   /* Adjust pixel pair if they neighbor each other diagonally. */
   fix_edge_pixel_pair(&feat_x, &feat_y, &edge_x, &edge_y,
                       bdata, iw, ih);

   /* Trace ridge contour, starting at the ridge end transition, and */
   /* taking a specified number of step scanning for edge neighbors  */
   /* clockwise.  As we trace the ridge, we want to detect if we     */
   /* encounter the ridge start transition.  NOTE: The ridge end     */
   /* position is on the white (of a black to white transition) and  */
   /* the ridge start is on the black (of a black to white trans),   */
   /* so the edge trace needs to look for the what pixel (not the    */
   /* black one) of the ridge start transition.                      */	
   ret = trace_contour(&contour_x, &contour_y,
                       &contour_ex, &contour_ey, &ncontour,
                       max_ridge_steps,
                       xlist[ridge_start-1], ylist[ridge_start-1],
                       feat_x, feat_y, edge_x, edge_y,
                       SCAN_CLOCKWISE, bdata, iw, ih);
   /* If a system error occurred ... */
   if(ret < 0)
      /* Return error code. */
      return(ret);

   /* Otherwise, if the trace was not IGNORED, then a contour was */
   /* was generated and returned.  We aren't interested in the    */
   /* actual contour, so deallocate it.                           */
   if(ret != IGNORE)
      free_contour(contour_x, contour_y, contour_ex, contour_ey);

   /* If the trace was IGNORED, then we had some sort of initialization */
   /* problem, so treat this the same as if was actually located the    */
   /* ridge start point (in which case LOOP_FOUND is returned).         */
   /* So, If not IGNORED and ridge start not encounted in trace ...     */
   if((ret != IGNORE) &&
      (ret != LOOP_FOUND)){

      /* Now conduct contour trace scanning for edge neighbors counter- */
      /* clockwise.                                                     */
      ret = trace_contour(&contour_x, &contour_y,
                          &contour_ex, &contour_ey, &ncontour,
                          max_ridge_steps,
                          xlist[ridge_start-1], ylist[ridge_start-1],
                          feat_x, feat_y, edge_x, edge_y,
                          SCAN_COUNTER_CLOCKWISE, bdata, iw, ih);
      /* If a system error occurred ... */
      if(ret < 0)
         /* Return error code. */
         return(ret);

      /* Otherwise, if the trace was not IGNORED, then a contour was */
      /* was generated and returned.  We aren't interested in the    */
      /* actual contour, so deallocate it.                           */
      if(ret != IGNORE)
         free_contour(contour_x, contour_y, contour_ex, contour_ey);

      /* If trace not IGNORED and ridge start not encounted in 2nd trace ... */
      if((ret != IGNORE) &&
         (ret != LOOP_FOUND)){
         /* If we get here, assume we have a ridge crossing. */
         return(TRUE);
      }
      /* Otherwise, second trace returned IGNORE or ridge start found. */
   }
   /* Otherwise, first trace returned IGNORE or ridge start found. */
   
   /* If we get here, then we failed to validate a ridge crossing. */
   return(FALSE);
}

/*************************************************************************
**************************************************************************
#cat: ridge_count - Takes a pair of minutiae, and counts the number of
#cat:               ridges crossed along the linear trajectory connecting
#cat:               the 2 points in the image.

   Input:
      first     - index of primary minutia
      second    - index of secondary (neighbor) minutia
      minutiae  - list of minutiae
      bdata     - binary image data (0==while & 1==black)
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
      lfsparms  - parameters and thresholds for controlling LFS
   Return Code:
      Zero or Positive - number of ridges counted
      Negative         - system error
**************************************************************************/
static int ridge_count(const int first, const int second, MINUTIAE *minutiae,
                unsigned char *bdata, const int iw, const int ih,
                const LFSPARMS *lfsparms)
{
   MINUTIA *minutia1, *minutia2;
   int i, ret, found;
   int *xlist, *ylist, num;
   int ridge_cnt, ridge_start, ridge_end;
   int prevpix, curpix;

   minutia1 = minutiae->list[first];
   minutia2 = minutiae->list[second];

   /* If the 2 mintuia have identical pixel coords ... */
   if((minutia1->x == minutia2->x) &&
      (minutia1->y == minutia2->y))
      /* Then zero ridges between points. */
     return(0);

   /* Compute linear trajectory of contiguous pixels between first */
   /* and second minutia points.                                   */
   if((ret = line_points(&xlist, &ylist, &num,
                        minutia1->x, minutia1->y, minutia2->x, minutia2->y))){
      return(ret);
   }

   /* It there are no points on the line trajectory, then no ridges */
   /* to count (this should not happen, but just in case) ...       */
   if(num == 0){
      free(xlist);
      free(ylist);
      return(0);
   }

   /* Find first pixel opposite type along linear trajectory from */
   /* first minutia.                                              */
   prevpix = *(bdata+(ylist[0]*iw)+xlist[0]);
   i = 1;
   found = FALSE;
   while(i < num){
      curpix = *(bdata+(ylist[i]*iw)+xlist[i]);
      if(curpix != prevpix){
         found = TRUE;
         break;
      }
      i++;
   }

   /* If opposite pixel not found ... then no ridges to count */
   if(!found){
      free(xlist);
      free(ylist);
      return(0);
   }

   /* Ready to count ridges, so initialize counter to 0. */
   ridge_cnt = 0;

   print2log("RIDGE COUNT: %d,%d to %d,%d ", minutia1->x, minutia1->y,
                                               minutia2->x, minutia2->y);

   /* While not at the end of the trajectory ... */
   while(i < num){
      /* If 0-to-1 transition not found ... */
      if(!find_transition(&i, 0, 1, xlist, ylist, num, bdata, iw, ih)){
         /* Then we are done looking for ridges. */
         free(xlist);
         free(ylist);

         print2log("\n");

         /* Return number of ridges counted to this point. */
         return(ridge_cnt);
      }
      /* Otherwise, we found a new ridge start transition, so store */
      /* its location (the location of the 1 in 0-to-1 transition). */
      ridge_start = i;

      print2log(": RS %d,%d ", xlist[i], ylist[i]);

      /* If 1-to-0 transition not found ... */
      if(!find_transition(&i, 1, 0, xlist, ylist, num, bdata, iw, ih)){
         /* Then we are done looking for ridges. */
         free(xlist);
         free(ylist);

         print2log("\n");

         /* Return number of ridges counted to this point. */
         return(ridge_cnt);
      }
      /* Otherwise, we found a new ridge end transition, so store   */
      /* its location (the location of the 0 in 1-to-0 transition). */
      ridge_end = i;

      print2log("; RE %d,%d ", xlist[i], ylist[i]);

      /* Conduct the validation, tracing the contour of the ridge  */
      /* from the ridge ending point a specified number of steps   */
      /* scanning for neighbors clockwise and counter-clockwise.   */
      /* If the ridge starting point is encounted during the trace */
      /* then we can assume we do not have a valid ridge crossing  */
      /* and instead we are walking on and off the edge of the     */
      /* side of a ridge.                                          */
      ret = validate_ridge_crossing(ridge_start, ridge_end,
                                    xlist, ylist, num, bdata, iw, ih,
                                    lfsparms->max_ridge_steps);

      /* If system error ... */
      if(ret < 0){
         free(xlist);
         free(ylist);
         /* Return the error code. */
         return(ret);
      }

      print2log("; V%d ", ret);

      /* If validation result is TRUE ... */
      if(ret){
         /* Then assume we have found a valid ridge crossing and bump */
         /* the ridge counter.                                        */
         ridge_cnt++;
      }

      /* Otherwise, ignore the current ridge start and end transitions */
      /* and go back and search for new ridge start.                   */
   }

   /* Deallocate working memories. */
   free(xlist);
   free(ylist);

   print2log("\n");

   /* Return the number of ridges counted. */
   return(ridge_cnt);
}

/*************************************************************************
**************************************************************************
#cat: count_minutia_ridges - Takes a minutia, and determines its closest
#cat:                neighbors and counts the number of interveining ridges
#cat:                between the minutia point and each of its neighbors.

   Input:
      minutia   - input minutia
      bdata     - binary image data (0==while & 1==black)
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      minutiae  - minutia augmented with neighbors and ridge counts
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
static int count_minutia_ridges(const int first, MINUTIAE *minutiae,
                      unsigned char *bdata, const int iw, const int ih,
                      const LFSPARMS *lfsparms)
{
   int i, ret, *nbr_list = NULL, *nbr_nridges, nnbrs;

   /* Find up to the maximum number of qualifying neighbors. */
   if((ret = find_neighbors(&nbr_list, &nnbrs, lfsparms->max_nbrs,
                           first, minutiae))){
      free(nbr_list);
      return(ret);
   }

   print2log("NBRS FOUND: %d,%d = %d\n", minutiae->list[first]->x,
              minutiae->list[first]->y, nnbrs);

   /* If no neighors found ... */
   if(nnbrs == 0){
      /* Then no list returned and no ridges to count. */
      return(0);
   }

   /* Sort neighbors on delta dirs. */
   if((ret = sort_neighbors(nbr_list, nnbrs, first, minutiae))){
      free(nbr_list);
      return(ret);
   }

   /* Count ridges between first and neighbors. */
   /* List of ridge counts, one for each neighbor stored. */
   nbr_nridges = (int *)malloc(nnbrs * sizeof(int));
   if(nbr_nridges == (int *)NULL){
      free(nbr_list);
      fprintf(stderr, "ERROR : count_minutia_ridges : malloc : nbr_nridges\n");
      return(-450);
   }

   /* Foreach neighbor found and sorted in list ... */
   for(i = 0; i < nnbrs; i++){
      /* Count the ridges between the primary minutia and the neighbor. */
      ret = ridge_count(first, nbr_list[i], minutiae, bdata, iw, ih, lfsparms);
      /* If system error ... */
      if(ret < 0){
         /* Deallocate working memories. */
         free(nbr_list);
         free(nbr_nridges);
         /* Return error code. */
         return(ret);
      }

      /* Otherwise, ridge count successful, so store ridge count to list. */
      nbr_nridges[i] = ret;
   }

   /* Assign neighbor indices and ridge counts to primary minutia. */
   minutiae->list[first]->nbrs = nbr_list;
   minutiae->list[first]->ridge_counts = nbr_nridges;
   minutiae->list[first]->num_nbrs = nnbrs;

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: count_minutiae_ridges - Takes a list of minutiae, and for each one,
#cat:                determines its closest neighbors and counts the number
#cat:                of interveining ridges between the minutia point and
#cat:                each of its neighbors.

   Input:
      minutiae  - list of minutiae
      bdata     - binary image data (0==while & 1==black)
      iw        - width (in pixels) of image
      ih        - height (in pixels) of image
      lfsparms  - parameters and thresholds for controlling LFS
   Output:
      minutiae  - list of minutiae augmented with neighbors and ridge counts
   Return Code:
      Zero     - successful completion
      Negative - system error
**************************************************************************/
int count_minutiae_ridges(MINUTIAE *minutiae,
                      unsigned char *bdata, const int iw, const int ih,
                      const LFSPARMS *lfsparms)
{
   int ret;
   int i;

   print2log("\nFINDING NBRS AND COUNTING RIDGES:\n");

   /* Sort minutia points on x then y (column-oriented). */
   if((ret = sort_minutiae_x_y(minutiae, iw, ih))){
      return(ret);
   }

   /* Remove any duplicate minutia points from the list. */
   if((ret = rm_dup_minutiae(minutiae))){
      return(ret);
   }

   /* Foreach remaining sorted minutia in list ... */
   for(i = 0; i < minutiae->num-1; i++){
      /* Located neighbors and count number of ridges in between. */
      /* NOTE: neighbor and ridge count results are stored in     */
      /*       minutiae->list[i].                                 */
      if((ret = count_minutia_ridges(i, minutiae, bdata, iw, ih, lfsparms))){
         return(ret);
      }
   }

   /* Return normally. */
   return(0);
}


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

      FILE:    SHAPE.C
      AUTHOR:  Michael D. Garris
      DATE:    05/11/1999
      UPDATED: 03/16/2005 by MDG

      Contains routines responsible for creating and manipulating
      shape stuctures as part of the NIST Latent Fingerprint System (LFS).

***********************************************************************
               ROUTINES:
                        alloc_shape()
                        free_shape()
                        shape_from_contour()
                        sort_row_on_x()
***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <lfs.h>

/*************************************************************************
**************************************************************************
#cat: alloc_shape - Allocates and initializes a shape structure given the
#cat:              the X and Y limits of the shape.

   Input:
      xmin   - left-most x-coord in shape
      ymin   - top-most y-coord in shape
      xmax   - right-most x-coord in shape
      ymax   - bottom-most y-coord in shape
   Output:
      oshape - pointer to the allocated & initialized shape structure
   Return Code:
      Zero     - Shape successfully allocated and initialized
      Negative - System error
**************************************************************************/
static int alloc_shape(SHAPE **oshape, const int xmin, const int ymin,
                  const int xmax, const int ymax)
{
   SHAPE *shape;
   int alloc_rows, alloc_pts;
   int i, j, y;

   /* Compute allocation parameters. */
   /* First, compute the number of scanlines spanned by the shape. */
   alloc_rows = ymax - ymin + 1;
   /* Second, compute the "maximum" number of contour points possible    */
   /* on a row.  Here we are allocating the maximum number of contiguous */
   /* pixels on each row which will be sufficiently larger than the      */
   /* number of actual contour points.                                   */
   alloc_pts = xmax - xmin + 1;

   /* Allocate the shape structure. */
   shape = (SHAPE *)malloc(sizeof(SHAPE));
   /* If there is an allocation error... */
   if(shape == (SHAPE *)NULL){
      fprintf(stderr, "ERROR : alloc_shape : malloc : shape\n");
      return(-250);
   }

   /* Allocate the list of row pointers.  We now this number will fit */
   /* the shape exactly.                                              */
   shape->rows = (ROW **)malloc(alloc_rows * sizeof(ROW *));
   /* If there is an allocation error... */
   if(shape->rows == (ROW **)NULL){
      /* Deallocate memory alloated by this routine to this point. */
      free(shape);
      fprintf(stderr, "ERROR : alloc_shape : malloc : shape->rows\n");
      return(-251);
   }

   /* Initialize the shape structure's attributes. */
   shape->ymin = ymin;
   shape->ymax = ymax;
   /* The number of allocated rows will be exactly the number of */
   /* assigned rows for the shape.                               */
   shape->alloc = alloc_rows;
   shape->nrows = alloc_rows;

   /* Foreach row in the shape... */
   for(i = 0, y = ymin; i < alloc_rows; i++, y++){
      /* Allocate a row structure and store it in its respective position */
      /* in the shape structure's list of row pointers.                   */
      shape->rows[i] = (ROW *)malloc(sizeof(ROW));
      /* If there is an allocation error... */
      if(shape->rows[i] == (ROW *)NULL){
         /* Deallocate memory alloated by this routine to this point. */
         for(j = 0; j < i; j++){
            free(shape->rows[j]->xs);
            free(shape->rows[j]);
         }
         free(shape->rows);
         free(shape);
         fprintf(stderr, "ERROR : alloc_shape : malloc : shape->rows[i]\n");
         return(-252);
      }

      /* Allocate the current rows list of x-coords. */
      shape->rows[i]->xs = (int *)malloc(alloc_pts * sizeof(int));
      /* If there is an allocation error... */
      if(shape->rows[i]->xs == (int *)NULL){
         /* Deallocate memory alloated by this routine to this point. */
         for(j = 0; j < i; j++){
            free(shape->rows[j]->xs);
            free(shape->rows[j]);
         }
         free(shape->rows[i]);
         free(shape->rows);
         free(shape);
         fprintf(stderr,
                 "ERROR : alloc_shape : malloc : shape->rows[i]->xs\n");
         return(-253);
      }

      /* Initialize the current row structure's attributes. */
      shape->rows[i]->y = y;
      shape->rows[i]->alloc = alloc_pts;
      /* There are initially ZERO points assigned to the row. */
      shape->rows[i]->npts = 0;
   }

   /* Assign structure to output pointer. */
   *oshape = shape;

   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: free_shape - Deallocates a shape structure and all its allocated
#cat:              attributes.

   Input:
      shape     - pointer to the shape structure to be deallocated
**************************************************************************/
void free_shape(SHAPE *shape)
{
   int i;

   /* Foreach allocated row in the shape ... */
   for(i = 0; i < shape->alloc; i++){
      /* Deallocate the current row's list of x-coords. */
      free(shape->rows[i]->xs);
      /* Deallocate the current row structure. */
      free(shape->rows[i]);
   }

   /* Deallocate the list of row pointers. */
   free(shape->rows);
   /* Deallocate the shape structure. */
   free(shape);
}

/*************************************************************************
**************************************************************************
#cat: sort_row_on_x - Takes a row structure and sorts its points left-to-
#cat:            right on X.

   Input:
      row       - row structure to be sorted
   Output:
      row       - row structure with points in sorted order
**************************************************************************/
static void sort_row_on_x(ROW *row)
{
   /* Conduct a simple increasing bubble sort on the x-coords */
   /* in the given row.  A bubble sort is satisfactory as the */
   /* number of points will be relatively small.              */
   bubble_sort_int_inc(row->xs, row->npts);
}

/*************************************************************************
**************************************************************************
#cat: shape_from_contour - Converts a contour list that has been determined
#cat:            to form a complete loop into a shape representation where
#cat:            the contour points on each contiguous scanline of the shape
#cat:            are stored in left-to-right order.

   Input:
      contour_x  - x-coord list for loop's contour points
      contour_y  - y-coord list for loop's contour points
      ncontour   - number of points in contour
   Output:
      oshape     - points to the resulting shape structure
   Return Code:
      Zero      - shape successfully derived
      Negative  - system error
**************************************************************************/
int shape_from_contour(SHAPE **oshape, const int *contour_x,
                        const int *contour_y, const int ncontour)
{
   SHAPE *shape;
   ROW *row;
   int ret, i, xmin, ymin, xmax, ymax;

   /* Find xmin, ymin, xmax, ymax on contour. */
   contour_limits(&xmin, &ymin, &xmax, &ymax,
                  contour_x, contour_y, ncontour);

   /* Allocate and initialize a shape structure. */
   if((ret = alloc_shape(&shape, xmin, ymin, xmax, ymax)))
      /* If system error, then return error code. */
      return(ret);

   /* Foreach point on contour ... */
   for(i = 0; i < ncontour; i++){
      /* Add point to corresponding row. */
      /* First set a pointer to the current row.  We need to subtract */
      /* ymin because the rows are indexed relative to the top-most   */
      /* scanline in the shape.                                       */
      row = shape->rows[contour_y[i]-ymin];

      /* It is possible with complex shapes to reencounter points        */
      /* already visited on a contour, especially at "pinching" points   */
      /* along the contour.  So we need to test to see if a point has    */
      /* already been stored in the row.  If not in row list already ... */
      if(in_int_list(contour_x[i], row->xs, row->npts) < 0){
         /* If row is full ... */
         if(row->npts >= row->alloc){
            /* This should never happen becuase we have allocated */
            /* based on shape bounding limits.                    */
            fprintf(stderr,
                    "ERROR : shape_from_contour : row overflow\n");
            return(-260);
         }
         /* Assign the x-coord of the current contour point to the row */
         /* and bump the row's point counter.  All the contour points  */
         /* on the same row share the same y-coord.                    */
         row->xs[row->npts++] = contour_x[i];
      }
      /* Otherwise, point is already stored in row, so ignore. */
   }

   /* Foreach row in the shape. */
   for(i = 0; i < shape->nrows; i++)
      /* Sort row points increasing on their x-coord. */
      sort_row_on_x(shape->rows[i]);

   /* Assign shape structure to output pointer. */
   *oshape = shape;

   /* Return normally. */
   return(0);
}


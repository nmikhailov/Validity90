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

      FILE:    SORT.C
      AUTHOR:  Michael D. Garris
      DATE:    03/16/1999
      UPDATED: 03/16/2005 by MDG

      Contains sorting routines required by the NIST Latent Fingerprint
      System (LFS).

***********************************************************************
               ROUTINES:
                        sort_indices_int_inc()
                        sort_indices_double_inc()
                        bubble_sort_int_inc_2()
                        bubble_sort_double_inc_2()
                        bubble_sort_double_dec_2()
                        bubble_sort_int_inc()
***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <lfs.h>

/*************************************************************************
**************************************************************************
#cat: sort_indices_int_inc - Takes a list of integers and returns a list of
#cat:                 indices referencing the integer list in increasing order.
#cat:                 The original list of integers is also returned in sorted
#cat:                 order.

   Input:
      ranks  - list of integers to be sorted
      num    - number of integers in the list
   Output:
      optr   - list of indices referencing the integer list in sorted order
      ranks  - list of integers in increasing order
   Return Code:
      Zero      - successful completion
      Negative  - system error
**************************************************************************/
int sort_indices_int_inc(int **optr, int *ranks, const int num)
{
   int *order;
   int i;

   /* Allocate list of sequential indices. */
   order = (int *)malloc(num * sizeof(int));
   if(order == (int *)NULL){
      fprintf(stderr, "ERROR : sort_indices_int_inc : malloc : order\n");
      return(-390);
   }
   /* Initialize list of sequential indices. */
   for(i = 0; i < num; i++)
      order[i] = i;

   /* Sort the indecies into rank order. */
   bubble_sort_int_inc_2(ranks, order, num);

   /* Set output pointer to the resulting order of sorted indices. */
   *optr = order;
   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: sort_indices_double_inc - Takes a list of doubles and returns a list of
#cat:                 indices referencing the double list in increasing order.
#cat:                 The original list of doubles is also returned in sorted
#cat:                 order.

   Input:
      ranks  - list of doubles to be sorted
      num    - number of doubles in the list
   Output:
      optr   - list of indices referencing the double list in sorted order
      ranks  - list of doubles in increasing order
   Return Code:
      Zero      - successful completion
      Negative  - system error
**************************************************************************/
int sort_indices_double_inc(int **optr, double *ranks, const int num)
{
   int *order;
   int i;

   /* Allocate list of sequential indices. */
   order = (int *)malloc(num * sizeof(int));
   if(order == (int *)NULL){
      fprintf(stderr, "ERROR : sort_indices_double_inc : malloc : order\n");
      return(-400);
   }
   /* Initialize list of sequential indices. */
   for(i = 0; i < num; i++)
      order[i] = i;

   /* Sort the indicies into rank order. */
   bubble_sort_double_inc_2(ranks, order, num);

   /* Set output pointer to the resulting order of sorted indices. */
   *optr = order;
   /* Return normally. */
   return(0);
}

/*************************************************************************
**************************************************************************
#cat: bubble_sort_int_inc_2 - Takes a list of integer ranks and a corresponding
#cat:                         list of integer attributes, and sorts the ranks
#cat:                         into increasing order moving the attributes
#cat:                         correspondingly.

   Input:
      ranks     - list of integers to be sort on
      items     - list of corresponding integer attributes
      len       - number of items in list
   Output:
      ranks     - list of integers sorted in increasing order
      items     - list of attributes in corresponding sorted order
**************************************************************************/
void bubble_sort_int_inc_2(int *ranks, int *items, const int len)
{
   int done = 0;
   int i, p, n, trank, titem;

   /* Set counter to the length of the list being sorted. */
   n = len;

   /* While swaps in order continue to occur from the */
   /* previous iteration...                           */
   while(!done){
      /* Reset the done flag to TRUE. */
      done = TRUE;
      /* Foreach rank in list up to current end index...               */
      /* ("p" points to current rank and "i" points to the next rank.) */
      for (i=1, p = 0; i<n; i++, p++){
         /* If previous rank is < current rank ... */
         if(ranks[p] > ranks[i]){
            /* Swap ranks. */
            trank = ranks[i];
            ranks[i] = ranks[p];
            ranks[p] = trank;
            /* Swap items. */
            titem = items[i];
            items[i] = items[p];
            items[p] = titem;
            /* Changes were made, so set done flag to FALSE. */
            done = FALSE;
         }
         /* Otherwise, rank pair is in order, so continue. */
      }
      /* Decrement the ending index. */
      n--;
   }     
}

/*************************************************************************
**************************************************************************
#cat: bubble_sort_double_inc_2 - Takes a list of double ranks and a
#cat:              corresponding list of integer attributes, and sorts the
#cat:              ranks into increasing order moving the attributes
#cat:              correspondingly.

   Input:
      ranks     - list of double to be sort on
      items     - list of corresponding integer attributes
      len       - number of items in list
   Output:
      ranks     - list of doubles sorted in increasing order
      items     - list of attributes in corresponding sorted order
**************************************************************************/
void bubble_sort_double_inc_2(double *ranks, int *items, const int len)
{
   int done = 0;
   int i, p, n, titem;
   double trank;

   /* Set counter to the length of the list being sorted. */
   n = len;

   /* While swaps in order continue to occur from the */
   /* previous iteration...                           */
   while(!done){
      /* Reset the done flag to TRUE. */
      done = TRUE;
      /* Foreach rank in list up to current end index...               */
      /* ("p" points to current rank and "i" points to the next rank.) */
      for (i=1, p = 0; i<n; i++, p++){
         /* If previous rank is < current rank ... */
         if(ranks[p] > ranks[i]){
            /* Swap ranks. */
            trank = ranks[i];
            ranks[i] = ranks[p];
            ranks[p] = trank;
            /* Swap items. */
            titem = items[i];
            items[i] = items[p];
            items[p] = titem;
            /* Changes were made, so set done flag to FALSE. */
            done = FALSE;
         }
         /* Otherwise, rank pair is in order, so continue. */
      }
      /* Decrement the ending index. */
      n--;
   }     
}

/***************************************************************************
**************************************************************************
#cat: bubble_sort_double_dec_2 - Conducts a simple bubble sort returning a list
#cat:        of ranks in decreasing order and their associated items in sorted
#cat:        order as well.

   Input:
      ranks - list of values to be sorted
      items - list of items, each corresponding to a particular rank value
      len   - length of the lists to be sorted
   Output:
      ranks - list of values sorted in descending order
      items - list of items in the corresponding sorted order of the ranks.
              If these items are indices, upon return, they may be used as
              indirect addresses reflecting the sorted order of the ranks.
****************************************************************************/
void bubble_sort_double_dec_2(double *ranks, int *items,  const int len)
{
   int done = 0;
   int i, p, n, titem;
   double trank;

   n = len;
   while(!done){
      done = 1;
      for (i=1, p = 0;i<n;i++, p++){
         /* If previous rank is < current rank ... */
         if(ranks[p] < ranks[i]){
            /* Swap ranks */
            trank = ranks[i];
            ranks[i] = ranks[p];
            ranks[p] = trank;
            /* Swap corresponding items */
            titem = items[i];
            items[i] = items[p];
            items[p] = titem;
            done = 0;
         }
      }
      n--;
   }     
}

/*************************************************************************
**************************************************************************
#cat: bubble_sort_int_inc - Takes a list of integers and sorts them into
#cat:            increasing order using a simple bubble sort.

   Input:
      ranks     - list of integers to be sort on
      len       - number of items in list
   Output:
      ranks     - list of integers sorted in increasing order
**************************************************************************/
void bubble_sort_int_inc(int *ranks, const int len)
{
   int done = 0;
   int i, p, n;
   int trank;

   /* Set counter to the length of the list being sorted. */
   n = len;

   /* While swaps in order continue to occur from the */
   /* previous iteration...                           */
   while(!done){
      /* Reset the done flag to TRUE. */
      done = TRUE;
      /* Foreach rank in list up to current end index...               */
      /* ("p" points to current rank and "i" points to the next rank.) */
      for (i=1, p = 0; i<n; i++, p++){
         /* If previous rank is < current rank ... */
         if(ranks[p] > ranks[i]){
            /* Swap ranks. */
            trank = ranks[i];
            ranks[i] = ranks[p];
            ranks[p] = trank;
            /* Changes were made, so set done flag to FALSE. */
            done = FALSE;
         }
         /* Otherwise, rank pair is in order, so continue. */
      }
      /* Decrement the ending index. */
      n--;
   }     
}


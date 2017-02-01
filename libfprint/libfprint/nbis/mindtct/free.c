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

      FILE:    FREE.C
      AUTHOR:  Michael D. Garris
      DATE:    03/16/1999

      Contains routines responsible for deallocating
      memories required by the NIST Latent Fingerprint System (LFS).

***********************************************************************
               ROUTINES:
                        free_dir2rad()
                        free_dftwaves()
                        free_rotgrids()
                        free_dir_powers()
***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <lfs.h>

/*************************************************************************
**************************************************************************
#cat: free_dir2rad - Deallocates memory associated with a DIR2RAD structure

   Input:
      dir2rad - pointer to memory to be freed
*************************************************************************/
void free_dir2rad(DIR2RAD *dir2rad)
{
   free(dir2rad->cos);
   free(dir2rad->sin);
   free(dir2rad);
}

/*************************************************************************
**************************************************************************
#cat: free_dftwaves - Deallocates the memory associated with a DFTWAVES
#cat:                 structure

   Input:
      dftwaves - pointer to memory to be freed
**************************************************************************/
void free_dftwaves(DFTWAVES *dftwaves)
{
   int i;

   for(i = 0; i < dftwaves->nwaves; i++){
       free(dftwaves->waves[i]->cos);
       free(dftwaves->waves[i]->sin);
       free(dftwaves->waves[i]);
   }
   free(dftwaves->waves);
   free(dftwaves);
}

/*************************************************************************
**************************************************************************
#cat: free_rotgrids - Deallocates the memory associated with a ROTGRIDS
#cat:                 structure

   Input:
      rotgrids - pointer to memory to be freed
**************************************************************************/
void free_rotgrids(ROTGRIDS *rotgrids)
{
   int i;

   for(i = 0; i < rotgrids->ngrids; i++)
      free(rotgrids->grids[i]);
   free(rotgrids->grids);
   free(rotgrids);
}

/*************************************************************************
**************************************************************************
#cat: free_dir_powers - Deallocate memory associated with DFT power vectors

   Input:
      powers - vectors of DFT power values (N Waves X M Directions)
      nwaves - number of DFT wave forms used
**************************************************************************/
void free_dir_powers(double **powers, const int nwaves)
{
   int w;

   for(w = 0; w < nwaves; w++)
      free(powers[w]);

   free(powers);
}


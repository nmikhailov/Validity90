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

      FILE:    LOG.C
      AUTHOR:  Michael D. Garris
      DATE:    08/02/1999

      Contains routines responsible for dynamically updating a log file
      during the execution of the NIST Latent Fingerprint System (LFS).

***********************************************************************
               ROUTINES:
                        open_logfile()
                        print2log()
                        close_logfile()
***********************************************************************/

#include <log.h>

/* If logging is on, declare global file pointer and supporting */
/* global variable for logging intermediate results.            */
FILE *logfp;
int avrdir;
float dir_strength;
int nvalid;

/***************************************************************************/
/***************************************************************************/
int open_logfile()
{
#ifdef LOG_REPORT
   if((logfp = fopen(LOG_FILE, "wb")) == NULL){
      fprintf(stderr, "ERROR : open_logfile : fopen : %s\n", LOG_FILE);
      return(-1);
   }
#endif

   return(0);
}

/***************************************************************************/
/***************************************************************************/
void print2log(char *fmt, ...)
{
#ifdef LOG_REPORT
   va_list ap;

   va_start(ap, fmt);
   vfprintf(logfp, fmt, ap);
   va_end(ap);
#endif
}

/***************************************************************************/
/***************************************************************************/
int close_logfile()
{
#ifdef LOG_REPORT
   if(fclose(logfp)){
      fprintf(stderr, "ERROR : close_logfile : fclose : %s\n", LOG_FILE);
      return(-1);
   }
#endif

   return(0);
}


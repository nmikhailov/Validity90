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

#ifndef _LOG_H
#define _LOG_H

/* Definitions and references to support log report files. */
/* UPDATED: 03/16/2005 by MDG */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#ifdef LOG_REPORT
/* Uncomment the following line to enable logging. */
#define LOG_FILE     "log.txt"
#endif

extern FILE *g_logfp;
extern int g_avrdir;
extern float g_dir_strength;
extern int g_nvalid;

extern int open_logfile(void);
extern int close_logfile(void);
extern void print2log(char *, ...);

#endif

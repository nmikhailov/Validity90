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

#ifndef _DEFS_H
#define _DEFS_H

/*********************************************************************/
/*          General Purpose Defines                                  */
/*********************************************************************/
#ifndef True
#define True		1
#define False		0
#endif
#ifndef TRUE
#define TRUE		True
#define FALSE		False
#endif
#define Yes		True
#define No		False
#define Empty		NULL
#ifndef None
#define None		-1
#endif
#ifndef FOUND
#define FOUND            1
#endif
#define NOT_FOUND_NEG   -1
#define EOL		EOF
#ifndef DEG2RAD
#define DEG2RAD	(double)(57.29578)
#endif
#define max(a, b)   ((a) > (b) ? (a) : (b))
#define min(a, b)   ((a) < (b) ? (a) : (b))
#define sround(x) ((int) (((x)<0) ? (x)-0.5 : (x)+0.5))
#define sround_uint(x) ((unsigned int) (((x)<0) ? (x)-0.5 : (x)+0.5))
#define xor(a, b)  (!(a && b) && (a || b))
#define align_to_16(_v_)   ((((_v_)+15)>>4)<<4)
#define align_to_32(_v_) ((((_v_)+31)>>5)<<5)
#ifndef CHUNKS
#define CHUNKS          100
#endif

#endif /* !_DEFS_H */

/* ----------------------------------------------------------------------    
* Copyright (C) 2010 ARM Limited. All rights reserved.    
*    
* $Date:        15. February 2012  
* $Revision: 	V1.1.0  
*    
* Project: 	    CMSIS DSP Library    
* Title:	    arm_mat_scale_q15.c    
*    
* Description:	Multiplies a Q15 matrix by a scalar.    
*    
* Target Processor: Cortex-M4/Cortex-M3/Cortex-M0
*  
* Version 1.1.0 2012/02/15 
*    Updated with more optimizations, bug fixes and minor API changes.  
*   
* Version 1.0.10 2011/7/15  
*    Big Endian support added and Merged M0 and M3/M4 Source code.   
*    
* Version 1.0.3 2010/11/29   
*    Re-organized the CMSIS folders and updated documentation.    
*     
* Version 1.0.2 2010/11/11    
*    Documentation updated.     
*    
* Version 1.0.1 2010/10/05     
*    Production release and review comments incorporated.    
*    
* Version 1.0.0 2010/09/20     
*    Production release and review comments incorporated.    
*    
* Version 0.0.5  2010/04/26     
*    incorporated review comments and updated with latest CMSIS layer    
*    
* Version 0.0.3  2010/03/10     
*    Initial version    
* -------------------------------------------------------------------- */

#include "arm_math.h"

/**    
 * @ingroup groupMatrix    
 */

/**    
 * @addtogroup MatrixScale    
 * @{    
 */

/**    
 * @brief Q15 matrix scaling.    
 * @param[in]       *pSrc points to input matrix    
 * @param[in]       scaleFract fractional portion of the scale factor    
 * @param[in]       shift number of bits to shift the result by    
 * @param[out]      *pDst points to output matrix structure    
 * @return     		The function returns either    
 * <code>ARM_MATH_SIZE_MISMATCH</code> or <code>ARM_MATH_SUCCESS</code> based on the outcome of size checking.    
 *    
 * @details    
 * <b>Scaling and Overflow Behavior:</b>    
 * \par    
 * The input data <code>*pSrc</code> and <code>scaleFract</code> are in 1.15 format.    
 * These are multiplied to yield a 2.30 intermediate result and this is shifted with saturation to 1.15 format.    
 */

arm_status arm_mat_scale_q15(
  const arm_matrix_instance_q15 * pSrc,
  q15_t scaleFract,
  int32_t shift,
  arm_matrix_instance_q15 * pDst)
{
  q15_t *pIn = pSrc->pData;                      /* input data matrix pointer */
  q15_t *pOut = pDst->pData;                     /* output data matrix pointer */
  uint32_t numSamples;                           /* total number of elements in the matrix */
  int32_t totShift = 15 - shift;                 /* total shift to apply after scaling */
  uint32_t blkCnt;                               /* loop counters */
  arm_status status;                             /* status of matrix scaling     */

#ifndef ARM_MATH_CM0

  q15_t in1, in2, in3, in4;
  q31_t out1, out2, out3, out4;
  q31_t inA1, inA2;

#endif //     #ifndef ARM_MATH_CM0

#ifdef ARM_MATH_MATRIX_CHECK
  /* Check for matrix mismatch */
  if((pSrc->numRows != pDst->numRows) || (pSrc->numCols != pDst->numCols))
  {
    /* Set status as ARM_MATH_SIZE_MISMATCH */
    status = ARM_MATH_SIZE_MISMATCH;
  }
  else
#endif //    #ifdef ARM_MATH_MATRIX_CHECK
  {
    /* Total number of samples in the input matrix */
    numSamples = (uint32_t) pSrc->numRows * pSrc->numCols;

#ifndef ARM_MATH_CM0

    /* Run the below code for Cortex-M4 and Cortex-M3 */
    /* Loop Unrolling */
    blkCnt = numSamples >> 2;

    /* First part of the processing with loop unrolling.  Compute 4 outputs at a time.    
     ** a second loop below computes the remaining 1 to 3 samples. */
    while(blkCnt > 0u)
    {
      /* C(m,n) = A(m,n) * k */
      /* Scale, saturate and then store the results in the destination buffer. */
      /* Reading 2 inputs from memory */
      inA1 = _SIMD32_OFFSET(pIn);
      inA2 = _SIMD32_OFFSET(pIn + 2);

      /* C = A * scale */
      /* Scale the inputs and then store the 2 results in the destination buffer        
       * in single cycle by packing the outputs */
      out1 = (q31_t) ((q15_t) (inA1 >> 16) * scaleFract);
      out2 = (q31_t) ((q15_t) inA1 * scaleFract);
      out3 = (q31_t) ((q15_t) (inA2 >> 16) * scaleFract);
      out4 = (q31_t) ((q15_t) inA2 * scaleFract);

      out1 = out1 >> totShift;
      inA1 = _SIMD32_OFFSET(pIn + 4);
      out2 = out2 >> totShift;
      inA2 = _SIMD32_OFFSET(pIn + 6);
      out3 = out3 >> totShift;
      out4 = out4 >> totShift;

      in1 = (q15_t) (__SSAT(out1, 16));
      in2 = (q15_t) (__SSAT(out2, 16));
      in3 = (q15_t) (__SSAT(out3, 16));
      in4 = (q15_t) (__SSAT(out4, 16));

      _SIMD32_OFFSET(pOut) = __PKHBT(in2, in1, 16);
      _SIMD32_OFFSET(pOut + 2) = __PKHBT(in4, in3, 16);

      /* update pointers to process next sampels */
      pIn += 4u;
      pOut += 4u;


      /* Decrement the numSamples loop counter */
      blkCnt--;
    }

    /* If the numSamples is not a multiple of 4, compute any remaining output samples here.        
     ** No loop unrolling is used. */
    blkCnt = numSamples % 0x4u;

#else

    /* Run the below code for Cortex-M0 */

    /* Initialize blkCnt with number of samples */
    blkCnt = numSamples;

#endif /* #ifndef ARM_MATH_CM0 */

    while(blkCnt > 0u)
    {
      /* C(m,n) = A(m,n) * k */
      /* Scale, saturate and then store the results in the destination buffer. */
      *pOut++ =
        (q15_t) (__SSAT(((q31_t) (*pIn++) * scaleFract) >> totShift, 16));

      /* Decrement the numSamples loop counter */
      blkCnt--;
    }
    /* Set status as ARM_MATH_SUCCESS */
    status = ARM_MATH_SUCCESS;
  }

  /* Return to application */
  return (status);
}

/**        
 * @} end of MatrixScale group        
 */

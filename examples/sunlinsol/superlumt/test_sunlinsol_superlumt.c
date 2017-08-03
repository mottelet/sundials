/*
 * ----------------------------------------------------------------- 
 * Programmer(s): Daniel Reynolds @ SMU
 * -----------------------------------------------------------------
 * LLNS/SMU Copyright Start
 * Copyright (c) 2017, Southern Methodist University and 
 * Lawrence Livermore National Security
 *
 * This work was performed under the auspices of the U.S. Department 
 * of Energy by Southern Methodist University and Lawrence Livermore 
 * National Laboratory under Contract DE-AC52-07NA27344.
 * Produced at Southern Methodist University and the Lawrence 
 * Livermore National Laboratory.
 *
 * All rights reserved.
 * For details, see the LICENSE file.
 * LLNS/SMU Copyright End
 * -----------------------------------------------------------------
 * This is the testing routine to check the SUNLinSol SuperLUMT
 * module implementation. 
 * -----------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <sundials/sundials_types.h>
#include <sunlinsol/sunlinsol_superlumt.h>
#include <sunmatrix/sunmatrix_dense.h>
#include <sunmatrix/sunmatrix_sparse.h>
#include <nvector/nvector_serial.h>
#include <sundials/sundials_math.h>
#include "test_sunlinsol.h"


/* ----------------------------------------------------------------------
 * SUNSuperLUMT Linear Solver Testing Routine
 * --------------------------------------------------------------------*/
int main(int argc, char *argv[]) 
{
  int             fails = 0;          /* counter for test failures  */
  sunindextype    N;                  /* matrix columns, rows       */
  SUNLinearSolver LS;                 /* linear solver object       */
  SUNMatrix       A, B;               /* test matrices              */
  N_Vector        x, y, b;            /* test vectors               */
  realtype        *matdata, *xdata;
  int             mattype, num_threads, print_timing;
  sunindextype    i, j, k;

  /* check input and set matrix dimensions */
  if (argc < 5){
    printf("ERROR: FOUR (4) Inputs required: matrix size, matrix type (0/1), num_threads, print timing \n");
    return(-1);
  }

  N = atol(argv[1]); 
  if (N <= 0) {
    printf("ERROR: matrix size must be a positive integer \n");
    return(-1); 
  }

  mattype = atoi(argv[2]);
  if ((mattype != 0) && (mattype != 1)) {
    printf("ERROR: matrix type must be 0 or 1 \n");
    return(-1); 
  }
  mattype = (mattype == 0) ? CSC_MAT : CSR_MAT;
  
  num_threads = atoi(argv[3]);
  if (num_threads <= 0) {
    printf("ERROR: number_threads must be a positive integer \n");
    return(-1); 
  }
  
  print_timing = atoi(argv[4]);
  SetTiming(print_timing);

  printf("\nSuperLUMT linear solver test: size %ld, type %i, num_threads %i\n\n",
         (long int) N, mattype, num_threads);

  /* Create matrices and vectors */
  B = SUNDenseMatrix(N, N);
  x = N_VNew_Serial(N);
  y = N_VNew_Serial(N);
  b = N_VNew_Serial(N);

  /* Fill matrix with uniform random data in [0,1/N] */
  for (k=0; k<5*N; k++) {
    i = random() % N;
    j = random() % N;
    matdata = SUNDenseMatrix_Column(B,j);
    matdata[i] = random() / (pow(2.0,31.0) - 1.0) / N;
  }

  /* Add identity to matrix */
  fails = SUNMatScaleAddI(ONE, B);

  /* Fill x vector with uniform random data in [0,1] */
  xdata = N_VGetArrayPointer(x);
  for (i=0; i<N; i++)
    xdata[i] = random() / (pow(2.0,31.0) - 1.0);

  /* Create sparse matrix from dense, and destroy B */
  A = SUNSparseFromDenseMatrix(B, ZERO, mattype);
  SUNMatDestroy(B);

  /* copy x into y to print in case of solver failure */
  N_VScale(ONE, x, y);

  /* create right-hand side vector for linear solve */
  fails = SUNMatMatvec(A, x, b);
  
  /* Create SuperLUMT linear solver */
  LS = SUNSuperLUMT(x, A, num_threads);
  
  /* Run Tests */
  fails += Test_SUNLinSolInitialize(LS, 0);
  fails += Test_SUNLinSolSetup(LS, A, 0);
  fails += Test_SUNLinSolSolve(LS, A, x, b, 100*UNIT_ROUNDOFF, 0);
 
  fails += Test_SUNLinSolGetType(LS, SUNLINEARSOLVER_DIRECT, 0);
  fails += Test_SUNLinSolLastFlag(LS, 0);
  fails += Test_SUNLinSolSpace(LS, 0);
  fails += Test_SUNLinSolNumIters(LS, 0);
  fails += Test_SUNLinSolResNorm(LS, 0);
  fails += Test_SUNLinSolSetATimes(LS, NULL, NULL, NULL, FALSE, 0);
  fails += Test_SUNLinSolSetPreconditioner(LS, NULL, NULL, NULL, FALSE, 0);
  fails += Test_SUNLinSolSetScalingVectors(LS, x, y, FALSE, 0);

  /* Print result */
  if (fails) {
    printf("FAIL: SUNLinSol module failed %i tests \n \n", fails);
    printf("\nA =\n");
    SUNSparseMatrix_Print(A,stdout);
    printf("\nx (original) =\n");
    N_VPrint_Serial(y);
    printf("\nb =\n");
    N_VPrint_Serial(b);
    printf("\nx (computed) =\n");
    N_VPrint_Serial(x);
  } else {
    printf("SUCCESS: SUNLinSol module passed all tests \n \n");
  }

  /* Free solver, matrix and vectors */
  SUNLinSolFree(LS);
  SUNMatDestroy(A);
  N_VDestroy(x);
  N_VDestroy(y);
  N_VDestroy(b);

  return(0);
}

/* ----------------------------------------------------------------------
 * Implementation-specific 'check' routines
 * --------------------------------------------------------------------*/
int check_vector(N_Vector X, N_Vector Y, realtype tol)
{
  int failure = 0;
  sunindextype i, local_length, maxloc;
  realtype *Xdata, *Ydata, maxerr;
  
  Xdata = N_VGetArrayPointer(X);
  Ydata = N_VGetArrayPointer(Y);
  local_length = N_VGetLength_Serial(X);
  
  /* check vector data */
  for(i=0; i < local_length; i++)
    failure += FNEQ(Xdata[i], Ydata[i], tol);

  if (failure > ZERO) {
    maxerr = ZERO;
    maxloc = -1;
    for(i=0; i < local_length; i++) {
      if (SUNRabs(Xdata[i]-Ydata[i]) >  maxerr) {
        maxerr = SUNRabs(Xdata[i]-Ydata[i]);
        maxloc = i;
      }
    }
    printf("check err failure: maxerr = %g at loc %li (tol = %g)\n",
	   maxerr, (long int) maxloc, tol);
    return(1);
  }
  else
    return(0);
}

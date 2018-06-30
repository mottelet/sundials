/* -----------------------------------------------------------------------------
 * Programmer(s): David J. Gardner @ LLNL
 * -----------------------------------------------------------------------------
 * LLNS Copyright Start
 * Copyright (c) 2014, Lawrence Livermore National Security
 * This work was performed under the auspices of the U.S. Department
 * of Energy by Lawrence Livermore National Laboratory in part under
 * Contract W-7405-Eng-48 and in part under Contract DE-AC52-07NA27344.
 * Produced at the Lawrence Livermore National Laboratory.
 * All rights reserved.
 * For details, see the LICENSE file.
 * LLNS Copyright End
 * -----------------------------------------------------------------------------
 * This the implementation file for the IDA nonlinear solver interface.
 * ---------------------------------------------------------------------------*/

#include "ida_impl.h"
#include "sundials/sundials_math.h"

/* nonlinear solver constants */
#define MAXIT   4              /* max number of nonlinear iterations */
#define RATEMAX RCONST(0.9)    /* max convergence rate               */
#define PT0001  RCONST(0.0001) /* real 0.0001                        */
#define ONE     RCONST(1.0)    /* real 1.0                           */
#define TWENTY  RCONST(20.0)   /* real 20.0                          */

/* private functions passed to nonlinear solver */
static int IDANls_Res(N_Vector yy, N_Vector res, void* ida_mem);
static int IDANls_LSetup(N_Vector yy, N_Vector res, void* ida_mem);
static int IDANls_LSolve(N_Vector yy, N_Vector delta, void* ida_mem);
static int IDANls_ConvTest(int m, realtype delnrm, realtype tol, void* ida_mem);

/* -----------------------------------------------------------------------------
 * Exported functions
 * ---------------------------------------------------------------------------*/

int IDASetNonlinearSolver(void *ida_mem, SUNNonlinearSolver NLS)
{
  IDAMem IDA_mem;
  int retval;

  /* Return immediately if IDA memory is NULL */
  if (ida_mem == NULL) {
    IDAProcessError(NULL, IDA_MEM_NULL, "IDA", "IDASetNonlinearSolver", MSG_NO_MEM);
    return(IDA_MEM_NULL);
  }
  IDA_mem = (IDAMem) ida_mem;

  /* Return immediately if NLS memory is NULL */
  if (NLS == NULL) {
    IDAProcessError(NULL, IDA_ILL_INPUT, "IDA", "IDASetNonlinearSolver",
                    "NLS must be non-NULL");
    return (IDA_ILL_INPUT);
  }

  /* check for required nonlinear solver functions */
  if ( NLS->ops->gettype    == NULL ||
       NLS->ops->initialize == NULL ||
       NLS->ops->setup      == NULL ||
       NLS->ops->solve      == NULL ||
       NLS->ops->free       == NULL ||
       NLS->ops->setsysfn   == NULL ) {
    IDAProcessError(IDA_mem, IDA_ILL_INPUT, "IDA", "IDASetNonlinearSolver",
                    "NLS does not support required operations");
    return(IDA_ILL_INPUT);
  }

  /* check for allowed nonlinear solver types */
  if (SUNNonlinSolGetType(NLS) != SUNNONLINEARSOLVER_ROOTFIND) {
    IDAProcessError(IDA_mem, IDA_ILL_INPUT, "IDA", "IDASetNonlinearSolver",
                    "NLS type must be SUNNONLINEARSOLVER_ROOTFIND");
    return(IDA_ILL_INPUT);
  }

  /* free any existing nonlinear solver */
  if (IDA_mem->NLS)
    retval = SUNNonlinSolFree(IDA_mem->NLS);

  /* set SUNNonlinearSolver pointer */
  IDA_mem->NLS = NLS;

  /* set the nonlinear residual function */
  retval = SUNNonlinSolSetSysFn(IDA_mem->NLS, IDANls_Res);
  if (retval != IDA_SUCCESS) {
    IDAProcessError(IDA_mem, IDA_ILL_INPUT, "IDA", "IDASetNonlinearSolver",
                    "Setting nonlinear system function failed");
    return(IDA_ILL_INPUT);
  }

  /* set convergence test function */
  retval = SUNNonlinSolSetConvTestFn(IDA_mem->NLS, IDANls_ConvTest);
  if (retval != IDA_SUCCESS) {
    IDAProcessError(IDA_mem, IDA_ILL_INPUT, "IDA", "IDASetNonlinearSolver",
                    "Setting convergence test function failed");
    return(IDA_ILL_INPUT);
  }

  /* set max allowed nonlinear iterations */
  retval = SUNNonlinSolSetMaxIters(IDA_mem->NLS, MAXIT);
  if (retval != IDA_SUCCESS) {
    IDAProcessError(IDA_mem, IDA_ILL_INPUT, "IDA", "IDASetNonlinearSolver",
                    "Setting maximum number of nonlinear iterations failed");
    return(IDA_ILL_INPUT);
  }

  return(IDA_SUCCESS);
}


/* -----------------------------------------------------------------------------
 * Private functions
 * ---------------------------------------------------------------------------*/

int IDANlsInit(IDAMem IDA_mem)
{
  int retval;

  /* set the linear solver setup wrapper function */
  if (IDA_mem->ida_lsetup)
    retval = SUNNonlinSolSetLSetupFn(IDA_mem->NLS, IDANls_LSetup);
  else
    retval = SUNNonlinSolSetLSetupFn(IDA_mem->NLS, NULL);

  if (retval != IDA_SUCCESS) {
    IDAProcessError(IDA_mem, IDA_ILL_INPUT, "IDA", "IDAInitialSetup",
                    "Setting the linear solver setup function failed");
    return(IDA_NLS_INIT_FAIL);
  }

  /* set the linear solver solve wrapper function */
  if (IDA_mem->ida_lsolve)
    retval = SUNNonlinSolSetLSolveFn(IDA_mem->NLS, IDANls_LSolve);
  else
    retval = SUNNonlinSolSetLSolveFn(IDA_mem->NLS, NULL);

  if (retval != IDA_SUCCESS) {
    IDAProcessError(IDA_mem, IDA_ILL_INPUT, "IDA", "IDAInitialSetup",
                    "Setting linear solver solve function failed");
    return(IDA_NLS_INIT_FAIL);
  }

  /* initialize nonlinear solver */
  retval = SUNNonlinSolInitialize(IDA_mem->NLS);

  if (retval != IDA_SUCCESS) {
    IDAProcessError(IDA_mem, IDA_ILL_INPUT, "IDA", "IDAInitialSetup",
                    MSG_NLS_INIT_FAIL);
    return(IDA_NLS_INIT_FAIL);
  }

  return(IDA_SUCCESS);
}


static int IDANls_LSetup(N_Vector yy, N_Vector res, void* ida_mem)
{
  IDAMem   IDA_mem;
  N_Vector tempv3;
  int      retval;

  if (ida_mem == NULL) {
    IDAProcessError(NULL, IDA_MEM_NULL, "IDA", "IDANlsRes", MSG_NO_MEM);
    return(IDA_MEM_NULL);
  }
  IDA_mem = (IDAMem) ida_mem;

  tempv3 = IDA_mem->ida_ee;

  IDA_mem->ida_nsetups++;
  retval = IDA_mem->ida_lsetup(IDA_mem, yy, IDA_mem->ida_yp, res,
                               IDA_mem->ida_tempv1, IDA_mem->ida_tempv2, tempv3);

  IDA_mem->ida_cjold = IDA_mem->ida_cj;
  IDA_mem->ida_cjratio = ONE;
  IDA_mem->ida_ss = TWENTY;

  if (retval < 0) return(IDA_LSETUP_FAIL);
  if (retval > 0) return(IDA_LSETUP_RECVR);

  return(IDA_SUCCESS);
}


static int IDANls_LSolve(N_Vector yy, N_Vector delta, void* ida_mem)
{
  IDAMem IDA_mem;
  int    retval;

  if (ida_mem == NULL) {
    IDAProcessError(NULL, IDA_MEM_NULL, "IDA", "IDANlsRes", MSG_NO_MEM);
    return(IDA_MEM_NULL);
  }
  IDA_mem = (IDAMem) ida_mem;

  retval = IDA_mem->ida_lsolve(IDA_mem, delta, IDA_mem->ida_ewt, yy, IDA_mem->ida_yp,
                               IDA_mem->ida_savres);

  if (retval < 0) return(IDA_LSOLVE_FAIL);
  if (retval > 0) return(IDA_LSOLVE_RECVR);

  return(IDA_SUCCESS);
}


static int IDANls_Res(N_Vector yy, N_Vector res, void* ida_mem)
{
  IDAMem IDA_mem;
  int retval;

  if (ida_mem == NULL) {
    IDAProcessError(NULL, IDA_MEM_NULL, "IDA", "IDANlsRes", MSG_NO_MEM);
    return(IDA_MEM_NULL);
  }
  IDA_mem = (IDAMem) ida_mem;

  /* compute current yp */
  N_VLinearSum(IDA_mem->ida_cj, yy, ONE, IDA_mem->ida_ypbeta, IDA_mem->ida_yp);

  /* evaluate residual */
  retval = IDA_mem->ida_res(IDA_mem->ida_tn, yy, IDA_mem->ida_yp, res, IDA_mem->ida_user_data);

  /* increment the number of residual evaluations */
  IDA_mem->ida_nre++;

  /* save a copy of the residual vector in savres */
  N_VScale(ONE, res, IDA_mem->ida_savres);

  if (retval < 0) return(IDA_RES_FAIL);
  if (retval > 0) return(IDA_RES_RECVR);

  return(IDA_SUCCESS);
}


static int IDANls_ConvTest(int m, realtype delnrm, realtype tol, void* ida_mem)
{
  IDAMem IDA_mem;
  realtype rate;
  static realtype oldnrm;

  if (ida_mem == NULL) {
    IDAProcessError(NULL, IDA_MEM_NULL, "IDA", "IDANlsRes", MSG_NO_MEM);
    return(IDA_MEM_NULL);
  }
  IDA_mem = (IDAMem) ida_mem;

  /* Test for convergence, first directly, then with rate estimate. */
  if (m == 0){
    oldnrm = delnrm;
    if (delnrm <= PT0001 * IDA_mem->ida_toldel) return(SUN_NLS_SUCCESS);
  } else {
    rate = SUNRpowerR( delnrm/oldnrm, ONE/m );
    if (rate > RATEMAX) return(SUN_NLS_CONV_RECVR);
    IDA_mem->ida_ss = rate/(ONE - rate);
  }

  if (IDA_mem->ida_ss*delnrm <= tol) return(SUN_NLS_SUCCESS);

  /* Not yet converged */
  return(SUN_NLS_CONTINUE);
}

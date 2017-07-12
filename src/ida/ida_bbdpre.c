/*
 * -----------------------------------------------------------------
 * $Revision$
 * $Date$
 * ----------------------------------------------------------------- 
 * Programmer(s): Alan C. Hindmarsh and Radu Serban @ LLNL
 * -----------------------------------------------------------------
 * LLNS Copyright Start
 * Copyright (c) 2014, Lawrence Livermore National Security
 * This work was performed under the auspices of the U.S. Department 
 * of Energy by Lawrence Livermore National Laboratory in part under 
 * Contract W-7405-Eng-48 and in part under Contract DE-AC52-07NA27344.
 * Produced at the Lawrence Livermore National Laboratory.
 * All rights reserved.
 * For details, see the LICENSE file.
 * LLNS Copyright End
 * -----------------------------------------------------------------
 * This file contains implementations of routines for a
 * band-block-diagonal preconditioner, i.e. a block-diagonal
 * matrix with banded blocks, for use with IDA and an IDASPILS
 * linear solver.
 *
 * NOTE: With only one processor in use, a banded matrix results
 * rather than a block-diagonal matrix with banded blocks.
 * Diagonal blocking occurs at the processor level.
 * -----------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>

#include "ida_impl.h"
#include "ida_spils_impl.h"
#include "ida_bbdpre_impl.h"

#include <ida/ida_spgmr.h>
#include <ida/ida_spbcgs.h>
#include <ida/ida_sptfqmr.h>

#include <sundials/sundials_math.h>

#define ZERO RCONST(0.0)
#define ONE  RCONST(1.0)
#define TWO  RCONST(2.0)

/* Prototypes of IDABBDPrecSetup and IDABBDPrecSolve */

static int IDABBDPrecSetup(realtype tt,
                           N_Vector yy, N_Vector yp, N_Vector rr,
                           realtype c_j, void *prec_data,
                           N_Vector tmp1, N_Vector tmp2, N_Vector tmp3);
 
static int IDABBDPrecSolve(realtype tt,
                           N_Vector yy, N_Vector yp, N_Vector rr,
                           N_Vector rvec, N_Vector zvec,
                           realtype c_j, realtype delta, void *prec_data,
                           N_Vector tmp);

/* Prototype for IDABBDPrecFree */

static int IDABBDPrecFree(IDAMem ida_mem);

/* Prototype for difference quotient Jacobian calculation routine */

static int IBBDDQJac(IBBDPrecData pdata, realtype tt, realtype cj,
                     N_Vector yy, N_Vector yp, N_Vector gref, 
                     N_Vector ytemp, N_Vector yptemp, N_Vector gtemp);

/* 
 * ================================================================
 * User-Callable Functions: initialization, reinit and free
 * ================================================================
 */

/* Readability Replacements */

#define uround   (IDA_mem->ida_uround)
#define vec_tmpl (IDA_mem->ida_tempv1)

/*
 * -----------------------------------------------------------------
 * User-Callable Functions : malloc, reinit and free
 * -----------------------------------------------------------------
 */

int IDABBDPrecInit(void *ida_mem, sunindextype Nlocal, 
                   sunindextype mudq, sunindextype mldq, 
                   sunindextype mukeep, sunindextype mlkeep, 
                   realtype dq_rel_yy, 
                   IDABBDLocalFn Gres, IDABBDCommFn Gcomm)
{
  IDAMem IDA_mem;
  IDASpilsMem idaspils_mem;
  IBBDPrecData pdata;
  N_Vector tempv4;
  sunindextype muk, mlk, storage_mu;
  int flag;

  if (ida_mem == NULL) {
    IDAProcessError(NULL, IDASPILS_MEM_NULL, "IDABBDPRE", "IDABBDPrecInit", MSGBBD_MEM_NULL);
    return(IDASPILS_MEM_NULL);
  }
  IDA_mem = (IDAMem) ida_mem;

  /* Test if one of the SPILS linear solvers has been attached */
  if (IDA_mem->ida_lmem == NULL) {
    IDAProcessError(IDA_mem, IDASPILS_LMEM_NULL, "IDABBDPRE", "IDABBDPrecInit", MSGBBD_LMEM_NULL);
    return(IDASPILS_LMEM_NULL);
  }
  idaspils_mem = (IDASpilsMem) IDA_mem->ida_lmem;

  /* Test if the NVECTOR package is compatible with BLOCK BAND preconditioner */
  if(vec_tmpl->ops->nvgetarraypointer == NULL) {
    IDAProcessError(IDA_mem, IDASPILS_ILL_INPUT, "IDABBDPRE", "IDABBDPrecInit", MSGBBD_BAD_NVECTOR);
    return(IDASPILS_ILL_INPUT);
  }

  /* Allocate data memory. */
  pdata = NULL;
  pdata = (IBBDPrecData) malloc(sizeof *pdata);
  if (pdata == NULL) {
    IDAProcessError(IDA_mem, IDASPILS_MEM_FAIL, "IDABBDPRE", "IDABBDPrecInit", MSGBBD_MEM_FAIL);
    return(IDASPILS_MEM_FAIL);
  }

  /* Set pointers to glocal and gcomm; load half-bandwidths. */
  pdata->ida_mem = IDA_mem;
  pdata->glocal = Gres;
  pdata->gcomm = Gcomm;
  pdata->mudq = SUNMIN(Nlocal-1, SUNMAX(0, mudq));
  pdata->mldq = SUNMIN(Nlocal-1, SUNMAX(0, mldq));
  muk = SUNMIN(Nlocal-1, SUNMAX(0, mukeep));
  mlk = SUNMIN(Nlocal-1, SUNMAX(0, mlkeep));
  pdata->mukeep = muk;
  pdata->mlkeep = mlk;

  /* Set extended upper half-bandwidth for PP (required for pivoting). */
  storage_mu = SUNMIN(Nlocal-1, muk+mlk);

  /* Allocate memory for preconditioner matrix. */
  pdata->PP = NULL;
  pdata->PP = NewBandMat(Nlocal, muk, mlk, storage_mu);
  if (pdata->PP == NULL) { 
    free(pdata); pdata = NULL;
    IDAProcessError(IDA_mem, IDASPILS_MEM_FAIL, "IDABBDPRE", "IDABBDPrecInit", MSGBBD_MEM_FAIL);
    return(IDASPILS_MEM_FAIL); 
  }

  /* Allocate memory for lpivots. */
  pdata->lpivots = NULL;
  pdata->lpivots = NewIndexArray(Nlocal);
  if (pdata->lpivots == NULL) {
    DestroyMat(pdata->PP);
    free(pdata); pdata = NULL;
    IDAProcessError(IDA_mem, IDASPILS_MEM_FAIL, "IDABBDPRE", "IDABBDPrecInit", MSGBBD_MEM_FAIL);
    return(IDASPILS_MEM_FAIL);
  }

  /* Allocate tempv4 for use by IBBDDQJac */
  tempv4 = NULL;
  tempv4 = N_VClone(vec_tmpl); 
  if (tempv4 == NULL){
    DestroyMat(pdata->PP);
    DestroyArray(pdata->lpivots);
    free(pdata); pdata = NULL;
    IDAProcessError(IDA_mem, IDASPILS_MEM_FAIL, "IDABBDPRE", "IDABBDPrecInit", MSGBBD_MEM_FAIL);
    return(IDASPILS_MEM_FAIL);
  }
  pdata->tempv4 = tempv4;
  
  /* Set rel_yy based on input value dq_rel_yy (0 implies default). */
  pdata->rel_yy = (dq_rel_yy > ZERO) ? dq_rel_yy : SUNRsqrt(uround); 

  /* Store Nlocal to be used in IDABBDPrecSetup */
  pdata->n_local = Nlocal;
  
  /* Set work space sizes and initialize nge. */
  pdata->rpwsize = Nlocal*(mlk + storage_mu + 1);
  pdata->ipwsize = Nlocal;
  pdata->nge = 0;

  /* make sure s_pdata is free from any previous allocations */
  if (idaspils_mem->s_pfree != NULL) {
    idaspils_mem->s_pfree(IDA_mem);
  }

  /* Point to the new pdata field in the SPILS memory */
  idaspils_mem->s_pdata = pdata;

  /* Attach the pfree function */
  idaspils_mem->s_pfree = IDABBDPrecFree;

  /* Attach preconditioner solve and setup functions */
  flag = IDASpilsSetPreconditioner(ida_mem, IDABBDPrecSetup, IDABBDPrecSolve);

  return(flag);
}

int IDABBDPrecReInit(void *ida_mem,
		     sunindextype mudq, sunindextype mldq, 
		     realtype dq_rel_yy)
{
  IDAMem IDA_mem;
  IDASpilsMem idaspils_mem;
  IBBDPrecData pdata;
  sunindextype Nlocal;


  if (ida_mem == NULL) {
    IDAProcessError(NULL, IDASPILS_MEM_NULL, "IDABBDPRE", "IDABBDPrecReInit", MSGBBD_MEM_NULL);
    return(IDASPILS_MEM_NULL);
  }
  IDA_mem = (IDAMem) ida_mem;

  /* Test if one of the SPILS linear solvers has been attached */
  if (IDA_mem->ida_lmem == NULL) {
    IDAProcessError(IDA_mem, IDASPILS_LMEM_NULL, "IDABBDPRE", "IDABBDPrecReInit", MSGBBD_LMEM_NULL);
    return(IDASPILS_LMEM_NULL);
  }
  idaspils_mem = (IDASpilsMem) IDA_mem->ida_lmem;

  /* Test if the preconditioner data is non-NULL */
  if (idaspils_mem->s_pdata == NULL) {
    IDAProcessError(IDA_mem, IDASPILS_PMEM_NULL, "IDABBDPRE", "IDABBDPrecReInit", MSGBBD_PMEM_NULL);
    return(IDASPILS_PMEM_NULL);
  } 
  pdata = (IBBDPrecData) idaspils_mem->s_pdata;

  /* Load half-bandwidths. */
  Nlocal = pdata->n_local;
  pdata->mudq = SUNMIN(Nlocal-1, SUNMAX(0, mudq));
  pdata->mldq = SUNMIN(Nlocal-1, SUNMAX(0, mldq));

  /* Set rel_yy based on input value dq_rel_yy (0 implies default). */
  pdata->rel_yy = (dq_rel_yy > ZERO) ? dq_rel_yy : SUNRsqrt(uround); 

  /* Re-initialize nge */
  pdata->nge = 0;

  return(IDASPILS_SUCCESS);
}

int IDABBDPrecGetWorkSpace(void *ida_mem, sunindextype *lenrwBBDP, sunindextype *leniwBBDP)
{
  IDAMem IDA_mem;
  IDASpilsMem idaspils_mem;
  IBBDPrecData pdata;

  if (ida_mem == NULL) {
    IDAProcessError(NULL, IDASPILS_MEM_NULL, "IDABBDPRE", "IDABBDPrecGetWorkSpace", MSGBBD_MEM_NULL);
    return(IDASPILS_MEM_NULL);
  }
  IDA_mem = (IDAMem) ida_mem;

  if (IDA_mem->ida_lmem == NULL) {
    IDAProcessError(IDA_mem, IDASPILS_LMEM_NULL, "IDABBDPRE", "IDABBDPrecGetWorkSpace", MSGBBD_LMEM_NULL);
    return(IDASPILS_LMEM_NULL);
  }
  idaspils_mem = (IDASpilsMem) IDA_mem->ida_lmem;

  if (idaspils_mem->s_pdata == NULL) {
    IDAProcessError(IDA_mem, IDASPILS_PMEM_NULL, "IDABBDPRE", "IDABBDPrecGetWorkSpace", MSGBBD_PMEM_NULL);
    return(IDASPILS_PMEM_NULL);
  } 
  pdata = (IBBDPrecData) idaspils_mem->s_pdata;

  *lenrwBBDP = pdata->rpwsize;
  *leniwBBDP = pdata->ipwsize;

  return(IDASPILS_SUCCESS);
}

int IDABBDPrecGetNumGfnEvals(void *ida_mem, long int *ngevalsBBDP)
{
  IDAMem IDA_mem;
  IDASpilsMem idaspils_mem;
  IBBDPrecData pdata;

  if (ida_mem == NULL) {
    IDAProcessError(NULL, IDASPILS_MEM_NULL, "IDABBDPRE", "IDABBDPrecGetNumGfnEvals", MSGBBD_MEM_NULL);
    return(IDASPILS_MEM_NULL);
  }
  IDA_mem = (IDAMem) ida_mem;

  if (IDA_mem->ida_lmem == NULL) {
    IDAProcessError(IDA_mem, IDASPILS_LMEM_NULL, "IDABBDPRE", "IDABBDPrecGetNumGfnEvals", MSGBBD_LMEM_NULL);
    return(IDASPILS_LMEM_NULL);
  }
  idaspils_mem = (IDASpilsMem) IDA_mem->ida_lmem;

  if (idaspils_mem->s_pdata == NULL) {
    IDAProcessError(IDA_mem, IDASPILS_PMEM_NULL, "IDABBDPRE", "IDABBDPrecGetNumGfnEvals", MSGBBD_PMEM_NULL);
    return(IDASPILS_PMEM_NULL);
  } 
  pdata = (IBBDPrecData) idaspils_mem->s_pdata;

  *ngevalsBBDP = pdata->nge;

  return(IDASPILS_SUCCESS);
}


/* Readability Replacements */

#define Nlocal  (pdata->n_local)
#define mudq    (pdata->mudq)
#define mldq    (pdata->mldq)
#define mukeep  (pdata->mukeep)
#define mlkeep  (pdata->mlkeep)
#define glocal  (pdata->glocal)
#define gcomm   (pdata->gcomm)
#define lpivots (pdata->lpivots)
#define PP      (pdata->PP)
#define tempv4  (pdata->tempv4)
#define nge     (pdata->nge)
#define rel_yy  (pdata->rel_yy)

/*
 * -----------------------------------------------------------------
 * Function : IDABBDPrecSetup                                     
 * -----------------------------------------------------------------
 * IDABBDPrecSetup generates a band-block-diagonal preconditioner
 * matrix, where the local block (on this processor) is a band
 * matrix. Each local block is computed by a difference quotient
 * scheme via calls to the user-supplied routines glocal, gcomm.
 * After generating the block in the band matrix PP, this routine
 * does an LU factorization in place in PP.
 *
 * The IDABBDPrecSetup parameters used here are as follows:
 *
 * tt is the current value of the independent variable t.
 *
 * yy is the current value of the dependent variable vector,
 *    namely the predicted value of y(t).
 *
 * yp is the current value of the derivative vector y',
 *    namely the predicted value of y'(t).
 *
 * c_j is the scalar in the system Jacobian, proportional to 1/hh.
 *
 * bbd_data is the pointer to BBD memory set by IDABBDInit
 *
 * tmp1, tmp2, tmp3 are pointers to vectors of type
 *                  N_Vector, used for temporary storage or
 *                  work space.
 *
 * The arguments Neq, rr, res, uround, and nrePtr are not used.
 *
 * Return value:
 * The value returned by this IDABBDPrecSetup function is a int
 * flag indicating whether it was successful. This value is
 *    0    if successful,
 *  > 0    for a recoverable error (step will be retried), or
 *  < 0    for a nonrecoverable error (step fails).
 * -----------------------------------------------------------------
 */

static int IDABBDPrecSetup(realtype tt,
                           N_Vector yy, N_Vector yp, N_Vector rr,
                           realtype c_j, void *bbd_data,
                           N_Vector tempv1, N_Vector tempv2, N_Vector tempv3)
{
  int retval;
  long int ier;
  IBBDPrecData pdata;
  IDAMem IDA_mem;

  pdata =(IBBDPrecData) bbd_data;

  IDA_mem = (IDAMem) pdata->ida_mem;

  /* Call IBBDDQJac for a new Jacobian calculation and store in PP. */
  SetToZero(PP);
  retval = IBBDDQJac(pdata, tt, c_j, yy, yp,
                     tempv1, tempv2, tempv3, tempv4);
  if (retval < 0) {
    IDAProcessError(IDA_mem, -1, "IDABBDPRE", "IDABBDPrecSetup", MSGBBD_FUNC_FAILED);
    return(-1);
  }
  if (retval > 0) {
    return(+1);
  } 
 
  /* Do LU factorization of preconditioner block in place (in PP). */
  ier = BandGBTRF(PP, lpivots);

  /* Return 0 if the LU was complete, or +1 otherwise. */
  if (ier > 0) return(+1);
  return(0);
}

/*
 * -----------------------------------------------------------------
 * Function: IDABBDPrecSolve
 * -----------------------------------------------------------------
 * The function IDABBDPrecSolve computes a solution to the linear
 * system P z = r, where P is the left preconditioner defined by
 * the routine IDABBDPrecSetup.
 *
 * The IDABBDPrecSolve parameters used here are as follows:
 *
 * rvec is the input right-hand side vector r.
 *
 * zvec is the computed solution vector z.
 *
 * bbd_data is the pointer to BBD data set by IDABBDInit.
 *
 * The arguments tt, yy, yp, rr, c_j, delta, and tmp are NOT used.
 *
 * IDABBDPrecSolve always returns 0, indicating success.
 * -----------------------------------------------------------------
 */

static int IDABBDPrecSolve(realtype tt,
                           N_Vector yy, N_Vector yp, N_Vector rr,
                           N_Vector rvec, N_Vector zvec,
                           realtype c_j, realtype delta, void *bbd_data,
                           N_Vector tmp)
{
  IBBDPrecData pdata;
  realtype *zd;

  pdata = (IBBDPrecData) bbd_data;

  /* Copy rvec to zvec, do the backsolve, and return. */
  N_VScale(ONE, rvec, zvec);

  zd = N_VGetArrayPointer(zvec);

  BandGBTRS(PP, lpivots, zd);

  return(0);
}



static int IDABBDPrecFree(IDAMem IDA_mem)
{
  IDASpilsMem idaspils_mem;
  IBBDPrecData pdata;
  
  if (IDA_mem->ida_lmem == NULL) return(0);
  idaspils_mem = (IDASpilsMem) IDA_mem->ida_lmem;
  
  if (idaspils_mem->s_pdata == NULL) return(0);
  pdata = (IBBDPrecData) idaspils_mem->s_pdata;

  DestroyMat(PP);
  DestroyArray(lpivots);
  N_VDestroy(tempv4);

  free(pdata);
  pdata = NULL;

  return(0);
}


#define ewt         (IDA_mem->ida_ewt)
#define user_data   (IDA_mem->ida_user_data)
#define hh          (IDA_mem->ida_hh)
#define constraints (IDA_mem->ida_constraints)

/*
 * -----------------------------------------------------------------
 * IBBDDQJac
 * -----------------------------------------------------------------
 * This routine generates a banded difference quotient approximation
 * to the local block of the Jacobian of G(t,y,y'). It assumes that
 * a band matrix of type BandMat is stored column-wise, and that
 * elements within each column are contiguous.
 *
 * All matrix elements are generated as difference quotients, by way
 * of calls to the user routine glocal. By virtue of the band
 * structure, the number of these calls is bandwidth + 1, where
 * bandwidth = mldq + mudq + 1. But the band matrix kept has
 * bandwidth = mlkeep + mukeep + 1. This routine also assumes that
 * the local elements of a vector are stored contiguously.
 *
 * Return values are: 0 (success), > 0 (recoverable error),
 * or < 0 (nonrecoverable error).
 * -----------------------------------------------------------------
 */

static int IBBDDQJac(IBBDPrecData pdata, realtype tt, realtype cj,
                     N_Vector yy, N_Vector yp, N_Vector gref, 
                     N_Vector ytemp, N_Vector yptemp, N_Vector gtemp)
{
  IDAMem IDA_mem;
  realtype inc, inc_inv;
  int  retval;
  sunindextype group, i, j, width, ngroups, i1, i2;
  realtype *ydata, *ypdata, *ytempdata, *yptempdata, *grefdata, *gtempdata;
  realtype *cnsdata = NULL, *ewtdata;
  realtype *col_j, conj, yj, ypj, ewtj;

  IDA_mem = (IDAMem) pdata->ida_mem;

  /* Initialize ytemp and yptemp. */

  N_VScale(ONE, yy, ytemp);
  N_VScale(ONE, yp, yptemp);

  /* Obtain pointers as required to the data array of vectors. */

  ydata     = N_VGetArrayPointer(yy);
  ypdata    = N_VGetArrayPointer(yp);
  gtempdata = N_VGetArrayPointer(gtemp);
  ewtdata   = N_VGetArrayPointer(ewt);
  if (constraints != NULL) 
    cnsdata = N_VGetArrayPointer(constraints);
  ytempdata = N_VGetArrayPointer(ytemp);
  yptempdata= N_VGetArrayPointer(yptemp);
  grefdata = N_VGetArrayPointer(gref);

  /* Call gcomm and glocal to get base value of G(t,y,y'). */

  if (gcomm != NULL) {
    retval = gcomm(Nlocal, tt, yy, yp, user_data);
    if (retval != 0) return(retval);
  }

  retval = glocal(Nlocal, tt, yy, yp, gref, user_data); 
  nge++;
  if (retval != 0) return(retval);


  /* Set bandwidth and number of column groups for band differencing. */

  width = mldq + mudq + 1;
  ngroups = SUNMIN(width, Nlocal);

  /* Loop over groups. */
  for(group = 1; group <= ngroups; group++) {
    
    /* Loop over the components in this group. */
    for(j = group-1; j < Nlocal; j += width) {
      yj = ydata[j];
      ypj = ypdata[j];
      ewtj = ewtdata[j];
      
      /* Set increment inc to yj based on rel_yy*abs(yj), with
         adjustments using ypj and ewtj if this is small, and a further
         adjustment to give it the same sign as hh*ypj. */
      inc = rel_yy*SUNMAX(SUNRabs(yj), SUNMAX( SUNRabs(hh*ypj), ONE/ewtj));
      if (hh*ypj < ZERO) inc = -inc;
      inc = (yj + inc) - yj;
      
      /* Adjust sign(inc) again if yj has an inequality constraint. */
      if (constraints != NULL) {
        conj = cnsdata[j];
        if (SUNRabs(conj) == ONE)      {if ((yj+inc)*conj <  ZERO) inc = -inc;}
        else if (SUNRabs(conj) == TWO) {if ((yj+inc)*conj <= ZERO) inc = -inc;}
      }

      /* Increment yj and ypj. */
      ytempdata[j] += inc;
      yptempdata[j] += cj*inc;
      
    }

    /* Evaluate G with incremented y and yp arguments. */

    retval = glocal(Nlocal, tt, ytemp, yptemp, gtemp, user_data); 
    nge++;
    if (retval != 0) return(retval);

    /* Loop over components of the group again; restore ytemp and yptemp. */
    for(j = group-1; j < Nlocal; j += width) {
      yj  = ytempdata[j]  = ydata[j];
      ypj = yptempdata[j] = ypdata[j];
      ewtj = ewtdata[j];

      /* Set increment inc as before .*/
      inc = rel_yy*SUNMAX(SUNRabs(yj), SUNMAX( SUNRabs(hh*ypj), ONE/ewtj));
      if (hh*ypj < ZERO) inc = -inc;
      inc = (yj + inc) - yj;
      if (constraints != NULL) {
        conj = cnsdata[j];
        if (SUNRabs(conj) == ONE)      {if ((yj+inc)*conj <  ZERO) inc = -inc;}
        else if (SUNRabs(conj) == TWO) {if ((yj+inc)*conj <= ZERO) inc = -inc;}
      }

      /* Form difference quotients and load into PP. */
      inc_inv = ONE/inc;
      col_j = BAND_COL(PP,j);
      i1 = SUNMAX(0, j-mukeep);
      i2 = SUNMIN(j+mlkeep, Nlocal-1);
      for(i = i1; i <= i2; i++) BAND_COL_ELEM(col_j,i,j) =
                                  inc_inv * (gtempdata[i] - grefdata[i]);
    }
  }
  
  return(0);
}


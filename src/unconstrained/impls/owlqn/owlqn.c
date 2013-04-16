#include "taolinesearch.h"
#include "src/matrix/lmvmmat.h"
#include "owlqn.h"

#define OWLQN_BFGS                0
#define OWLQN_SCALED_GRADIENT     1
#define OWLQN_GRADIENT            2


#undef __FUNCT__  
#define __FUNCT__ "ProjDirect_OWLQN"
static PetscErrorCode ProjDirect_OWLQN(Vec d, Vec g)
{
  PetscErrorCode ierr;
  PetscReal *gptr,*dptr;
  PetscInt low,high,low1,high1,i;

  PetscFunctionBegin;

  ierr=VecGetOwnershipRange(d,&low,&high);CHKERRQ(ierr);
  ierr=VecGetOwnershipRange(g,&low1,&high1);CHKERRQ(ierr);
  
  ierr = VecGetArray(g,&gptr);CHKERRQ(ierr);
  ierr = VecGetArray(d,&dptr);CHKERRQ(ierr);
  for (i = 0; i < high-low; i++) {
    if (dptr[i] * gptr[i] <= 0.0 )
      {
	dptr[i] = 0.0;
      }
  }
  ierr = VecRestoreArray(d,&dptr);CHKERRQ(ierr);  
  ierr = VecRestoreArray(g,&gptr);CHKERRQ(ierr); 
  

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "ComputePseudoGrad_OWLQN"
static PetscErrorCode ComputePseudoGrad_OWLQN(Vec x, Vec gv, PetscReal lambda)
{
  
  PetscErrorCode ierr;
  PetscReal *xptr,*gptr;
  PetscInt low,high,low1,high1,i;


  PetscFunctionBegin;
 
  ierr=VecGetOwnershipRange(x,&low,&high);CHKERRQ(ierr);
  ierr=VecGetOwnershipRange(gv,&low1,&high1);CHKERRQ(ierr);
  
  ierr = VecGetArray(x,&xptr);CHKERRQ(ierr);
  ierr = VecGetArray(gv,&gptr);CHKERRQ(ierr);
  for (i = 0; i < high-low; i++) {
    if (xptr[i] < 0.0)
      gptr[i] = gptr[i] - lambda;
    else if (xptr[i] > 0.0)
      gptr[i] = gptr[i] + lambda;
    else if (gptr[i] + lambda < 0.0)
      gptr[i] = gptr[i] + lambda;
    else if (gptr[i] - lambda > 0.0)
      gptr[i] = gptr[i] - lambda;
    else
      gptr[i] = 0.0;
  }
  ierr = VecRestoreArray(gv,&gptr);CHKERRQ(ierr); 
  ierr = VecRestoreArray(x,&xptr);CHKERRQ(ierr);
     
  PetscFunctionReturn(0);
 }




#undef __FUNCT__  
#define __FUNCT__ "TaoSolve_OWLQN"
static PetscErrorCode TaoSolve_OWLQN(TaoSolver tao)
{

  TAO_OWLQN *lmP = (TAO_OWLQN *)tao->data;
  
  PetscReal f, fold, gdx, gnorm;
  PetscReal step = 1.0;

  PetscReal delta;

  PetscErrorCode ierr;
  PetscInt stepType;
  PetscInt iter = 0;
  TaoSolverTerminationReason reason = TAO_CONTINUE_ITERATING;
  TaoLineSearchTerminationReason ls_status = TAOLINESEARCH_CONTINUE_ITERATING;

  PetscFunctionBegin;

  if (tao->XL || tao->XU || tao->ops->computebounds) {
    ierr = PetscPrintf(((PetscObject)tao)->comm,"WARNING: Variable bounds have been set but will be ignored by owlqn algorithm\n"); CHKERRQ(ierr);
  }

  /* Check convergence criteria */
  ierr = TaoComputeObjectiveAndGradient(tao, tao->solution, &f, tao->gradient); CHKERRQ(ierr);

  ierr = VecCopy(tao->gradient, lmP->GV); CHKERRQ(ierr);

  ierr = ComputePseudoGrad_OWLQN(tao->solution,lmP->GV,lmP->lambda);CHKERRQ(ierr);

  ierr = VecNorm(lmP->GV,NORM_2,&gnorm); CHKERRQ(ierr);

  if (PetscIsInfOrNanReal(f) || PetscIsInfOrNanReal(gnorm)) {
    SETERRQ(PETSC_COMM_SELF,1, "User provided compute function generated Inf or NaN");
  }  

  ierr = TaoMonitor(tao, iter, f, gnorm, 0.0, step, &reason); CHKERRQ(ierr);
  if (reason != TAO_CONTINUE_ITERATING) {
    PetscFunctionReturn(0);
  }

  /* Set initial scaling for the function */
  if (f != 0.0) {
    delta = 2.0 * PetscAbsScalar(f) / (gnorm*gnorm);
  }
  else {
    delta = 2.0 / (gnorm*gnorm);
  }
  ierr = MatLMVMSetDelta(lmP->M,delta); CHKERRQ(ierr);

  /* Set counter for gradient/reset steps */
  lmP->bfgs = 0;
  lmP->sgrad = 0;
  lmP->grad = 0;

  /* Have not converged; continue with Newton method */
  while (reason == TAO_CONTINUE_ITERATING) {

    /* Compute direction */
    ierr = MatLMVMUpdate(lmP->M,tao->solution,tao->gradient); CHKERRQ(ierr);
    ierr = MatLMVMSolve(lmP->M, lmP->GV, lmP->D); CHKERRQ(ierr);

    ierr = ProjDirect_OWLQN(lmP->D,lmP->GV);CHKERRQ(ierr); 

    ++lmP->bfgs;

    /* Check for success (descent direction) */
    ierr = VecDot(lmP->D, lmP->GV , &gdx); CHKERRQ(ierr);
    if ((gdx <= 0.0) || PetscIsInfOrNanReal(gdx)) {

      /* Step is not descent or direction produced not a number
	 We can assert bfgsUpdates > 1 in this case because
	 the first solve produces the scaled gradient direction,
	 which is guaranteed to be descent
  
	 Use steepest descent direction (scaled) */
      ++lmP->grad;

      if (f != 0.0) {
        delta = 2.0 * PetscAbsScalar(f) / (gnorm*gnorm);
      }
      else {
        delta = 2.0 / (gnorm*gnorm);
      }
      ierr = MatLMVMSetDelta(lmP->M, delta); CHKERRQ(ierr);
      ierr = MatLMVMReset(lmP->M); CHKERRQ(ierr);
      ierr = MatLMVMUpdate(lmP->M, tao->solution, tao->gradient); CHKERRQ(ierr);
      ierr = MatLMVMSolve(lmP->M,lmP->GV, lmP->D); CHKERRQ(ierr);

      ierr = ProjDirect_OWLQN(lmP->D,lmP->GV);CHKERRQ(ierr); 
      /* On a reset, the direction cannot be not a number; it is a 
	 scaled gradient step.  No need to check for this condition.
	 info = D->Norm2(&dnorm); CHKERRQ(info);
	 if (PetscIsInfOrNanReal(dnorm)) {
	 SETERRQ(PETSC_COMM_SELF,1, "Direction generated Not-a-Number");
       } */

      lmP->bfgs = 1;
      ++lmP->sgrad;
      stepType = OWLQN_SCALED_GRADIENT;
    }
    else {
      if (1 == lmP->bfgs) {
        /* The first BFGS direction is always the scaled gradient */
        ++lmP->sgrad;
        stepType = OWLQN_SCALED_GRADIENT;
      }
      else {
        ++lmP->bfgs;
        stepType = OWLQN_BFGS;
      }
    }
    
    ierr = VecScale(lmP->D, -1.0); CHKERRQ(ierr);
    
    /* Perform the linesearch */
    fold = f;
    ierr = VecCopy(tao->solution, lmP->Xold); CHKERRQ(ierr);
    ierr = VecCopy(tao->gradient, lmP->Gold); CHKERRQ(ierr);

    ierr = TaoLineSearchApply(tao->linesearch, tao->solution, &f, lmP->GV, lmP->D, &step,&ls_status); CHKERRQ(ierr);
    ierr = TaoAddLineSearchCounts(tao); CHKERRQ(ierr);
    
    while (((int)ls_status < 0) && (stepType != OWLQN_GRADIENT)) {

      /* Reset factors and use scaled gradient step */
      f = fold;
      ierr = VecCopy(lmP->Xold, tao->solution); CHKERRQ(ierr);
      ierr = VecCopy(lmP->Gold, tao->gradient); CHKERRQ(ierr);
      ierr = VecCopy(tao->gradient, lmP->GV); CHKERRQ(ierr);  

      ierr = ComputePseudoGrad_OWLQN(tao->solution,lmP->GV,lmP->lambda);CHKERRQ(ierr);
      
      switch(stepType) {
      case OWLQN_BFGS:
        /* Failed to obtain acceptable iterate with BFGS step
	   Attempt to use the scaled gradient direction */

        if (f != 0.0) {
          delta = 2.0 * PetscAbsScalar(f) / (gnorm*gnorm);
        }
        else {
          delta = 2.0 / (gnorm*gnorm);
        }
	ierr = MatLMVMSetDelta(lmP->M, delta); CHKERRQ(ierr);
	ierr = MatLMVMReset(lmP->M); CHKERRQ(ierr);
	ierr = MatLMVMUpdate(lmP->M, tao->solution, tao->gradient); CHKERRQ(ierr);
	ierr = MatLMVMSolve(lmP->M, lmP->GV, lmP->D); CHKERRQ(ierr);

        ierr = ProjDirect_OWLQN(lmP->D,lmP->GV);CHKERRQ(ierr); 
        /* On a reset, the direction cannot be not a number; it is a 
	   scaled gradient step.  No need to check for this condition.
	   info = D->Norm2(&dnorm); CHKERRQ(info);
	   if (PetscIsInfOrNanReal(dnorm)) {
	   SETERRQ(PETSC_COMM_SELF,1, "Direction generated Not-a-Number");
	   }*/
  
	lmP->bfgs = 1;
	++lmP->sgrad;
	stepType = OWLQN_SCALED_GRADIENT;
	break;

      case OWLQN_SCALED_GRADIENT:
        /* The scaled gradient step did not produce a new iterate;
	   attempt to use the gradient direction.
	   Need to make sure we are not using a different diagonal scaling */
	ierr = MatLMVMSetDelta(lmP->M, 1.0); CHKERRQ(ierr);
	ierr = MatLMVMReset(lmP->M); CHKERRQ(ierr);
	ierr = MatLMVMUpdate(lmP->M, tao->solution, tao->gradient); CHKERRQ(ierr);
	ierr = MatLMVMSolve(lmP->M, lmP->GV, lmP->D); CHKERRQ(ierr);

	ierr = ProjDirect_OWLQN(lmP->D,lmP->GV);CHKERRQ(ierr); 

        lmP->bfgs = 1;
        ++lmP->grad;
        stepType = OWLQN_GRADIENT;
        break;
      }
      ierr = VecScale(lmP->D, -1.0); CHKERRQ(ierr);

        
      /* Perform the linesearch */
      ierr = TaoLineSearchApply(tao->linesearch, tao->solution, &f, lmP->GV, lmP->D, &step, &ls_status); CHKERRQ(ierr);
      ierr = TaoAddLineSearchCounts(tao); CHKERRQ(ierr);
    }    
    
    if ((int)ls_status < 0) {
      /* Failed to find an improving point*/
      f = fold;
      ierr = VecCopy(lmP->Xold, tao->solution); CHKERRQ(ierr);
      ierr = VecCopy(lmP->Gold, tao->gradient); CHKERRQ(ierr);
      ierr = VecCopy(tao->gradient, lmP->GV); CHKERRQ(ierr);
      step = 0.0;
    }
    else {
      /* a little hack here, because that gv is used to store g */
      ierr = VecCopy(lmP->GV, tao->gradient); CHKERRQ(ierr); 
    }
    
    ierr = ComputePseudoGrad_OWLQN(tao->solution,lmP->GV,lmP->lambda);CHKERRQ(ierr);
    
    /* Check for termination */
   
    ierr = VecNorm(lmP->GV,NORM_2,&gnorm); CHKERRQ(ierr);

    iter++;
    ierr = TaoMonitor(tao,iter,f,gnorm,0.0,step,&reason); CHKERRQ(ierr);

    if ((int)ls_status < 0)
      break;
  }
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "TaoSetUp_OWLQN"
static PetscErrorCode TaoSetUp_OWLQN(TaoSolver tao)
{
  TAO_OWLQN *lmP = (TAO_OWLQN *)tao->data;
  PetscInt n,N;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  /* Existence of tao->solution checked in TaoSolverSetUp() */
  if (!tao->gradient) {ierr = VecDuplicate(tao->solution,&tao->gradient); CHKERRQ(ierr);  }
  if (!tao->stepdirection) {ierr = VecDuplicate(tao->solution,&tao->stepdirection); CHKERRQ(ierr);  }
  if (!lmP->D) {ierr = VecDuplicate(tao->solution,&lmP->D); CHKERRQ(ierr);  }
  if (!lmP->GV) {ierr = VecDuplicate(tao->solution,&lmP->GV); CHKERRQ(ierr);  }
  if (!lmP->Xold) {ierr = VecDuplicate(tao->solution,&lmP->Xold); CHKERRQ(ierr);  }
  if (!lmP->Gold) {ierr = VecDuplicate(tao->solution,&lmP->Gold); CHKERRQ(ierr);  }
  
  /* Create matrix for the limited memory approximation */
  ierr = VecGetLocalSize(tao->solution,&n); CHKERRQ(ierr);
  ierr = VecGetSize(tao->solution,&N); CHKERRQ(ierr);
  ierr = MatCreateLMVM(((PetscObject)tao)->comm,n,N,&lmP->M); CHKERRQ(ierr);
  ierr = MatLMVMAllocateVectors(lmP->M,tao->solution); CHKERRQ(ierr);
  

  PetscFunctionReturn(0);
}

/* ---------------------------------------------------------- */
#undef __FUNCT__  
#define __FUNCT__ "TaoDestroy_OWLQN"
static PetscErrorCode TaoDestroy_OWLQN(TaoSolver tao)
{

  TAO_OWLQN *lmP = (TAO_OWLQN *)tao->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (tao->setupcalled) {
    ierr = VecDestroy(&lmP->Xold); CHKERRQ(ierr);
    ierr = VecDestroy(&lmP->Gold); CHKERRQ(ierr);
    ierr = VecDestroy(&lmP->D); CHKERRQ(ierr);
    ierr = MatDestroy(&lmP->M); CHKERRQ(ierr);
    ierr = VecDestroy(&lmP->GV);CHKERRQ(ierr);
  }
  ierr = PetscFree(tao->data); CHKERRQ(ierr);
  tao->data = PETSC_NULL;

  PetscFunctionReturn(0); 
}

/*------------------------------------------------------------*/
#undef __FUNCT__  
#define __FUNCT__ "TaoSetFromOptions_OWLQN"
static PetscErrorCode TaoSetFromOptions_OWLQN(TaoSolver tao)
{

  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscOptionsHead("Limited-memory variable-metric method for unconstrained optimization"); CHKERRQ(ierr);
  ierr = TaoLineSearchSetFromOptions(tao->linesearch); CHKERRQ(ierr);
  ierr = PetscOptionsTail(); CHKERRQ(ierr);
  PetscFunctionReturn(0);

  PetscFunctionReturn(0);
}

/*------------------------------------------------------------*/
#undef __FUNCT__  
#define __FUNCT__ "TaoView_OWLQN"
static PetscErrorCode TaoView_OWLQN(TaoSolver tao, PetscViewer viewer)
{

    TAO_OWLQN *lm = (TAO_OWLQN *)tao->data;
    PetscBool isascii;
    PetscErrorCode ierr;


    PetscFunctionBegin;
    ierr = PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERASCII, &isascii); CHKERRQ(ierr);
    if (isascii) {

        ierr = PetscViewerASCIIPushTab(viewer); CHKERRQ(ierr);
	ierr = PetscViewerASCIIPrintf(viewer, "BFGS steps: %d\n", lm->bfgs); CHKERRQ(ierr);
	ierr = PetscViewerASCIIPrintf(viewer, "Scaled gradient steps: %d\n", lm->sgrad); CHKERRQ(ierr);
	ierr = PetscViewerASCIIPrintf(viewer, "Gradient steps: %d\n", lm->grad); CHKERRQ(ierr);
        ierr = PetscViewerASCIIPopTab(viewer); CHKERRQ(ierr);
    } else {
      SETERRQ1(((PetscObject)tao)->comm,PETSC_ERR_SUP,"Viewer type %s not supported for TAO OWLQN",((PetscObject)viewer)->type_name);
    }
    PetscFunctionReturn(0);
}

/* ---------------------------------------------------------- */

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "TaoCreate_OWLQN"
PetscErrorCode TaoCreate_OWLQN(TaoSolver tao)
{
    
  TAO_OWLQN *lmP;
  const char *owarmijo_type = TAOLINESEARCH_OWARMIJO;
  PetscErrorCode ierr;
  char lambda_str[20];
  PetscBool flg;

  PetscFunctionBegin;
  tao->ops->setup = TaoSetUp_OWLQN;
  tao->ops->solve = TaoSolve_OWLQN;
  tao->ops->view = TaoView_OWLQN;
  tao->ops->setfromoptions = TaoSetFromOptions_OWLQN;
  tao->ops->destroy = TaoDestroy_OWLQN;

  ierr = PetscNewLog(tao,TAO_OWLQN, &lmP); CHKERRQ(ierr);
  lmP->D = 0;
  lmP->M = 0;
  lmP->GV = 0;
  lmP->Xold = 0;
  lmP->Gold = 0;
  lmP->lambda = 1.0;

  tao->data = (void*)lmP;
  tao->max_it = 2000;
  tao->max_funcs = 4000;
  tao->fatol = 1e-4;
  tao->frtol = 1e-4;

  ierr = TaoLineSearchCreate(((PetscObject)tao)->comm,&tao->linesearch); CHKERRQ(ierr);
  ierr = TaoLineSearchSetType(tao->linesearch,owarmijo_type); CHKERRQ(ierr);
  ierr = TaoLineSearchUseTaoSolverRoutines(tao->linesearch,tao); CHKERRQ(ierr);
  
  ierr = PetscOptionsGetString(PETSC_NULL,"-tao_owlqn_lambda",lambda_str,20,&flg);
  lmP->lambda = atof(lambda_str); 

  PetscFunctionReturn(0);
}
EXTERN_C_END


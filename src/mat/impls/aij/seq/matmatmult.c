/*
  Defines matrix-matrix product routines for pairs of SeqAIJ matrices
          C = A * B
*/

#include "src/mat/impls/aij/seq/aij.h" /*I "petscmat.h" I*/
#include "src/mat/utils/freespace.h"
#include "petscbt.h"

#undef __FUNCT__
#define __FUNCT__ "MatMatMult"
/*@
   MatMatMult - Performs Matrix-Matrix Multiplication C=A*B.

   Collective on Mat

   Input Parameters:
+  A - the left matrix
.  B - the right matrix
.  scall - either MAT_INITIAL_MATRIX or MAT_REUSE_MATRIX
-  fill - expected fill as ratio of nnz(C)/(nnz(A) + nnz(B))

   Output Parameters:
.  C - the product matrix

   Notes:
   C will be created and must be destroyed by the user with MatDestroy().

   This routine is currently only implemented for pairs of AIJ matrices and classes
   which inherit from AIJ.  C will be of type MATAIJ.

   Level: intermediate

.seealso: MatMatMultSymbolic(),MatMatMultNumeric()
@*/
PetscErrorCode MatMatMult(Mat A,Mat B,MatReuse scall,PetscReal fill,Mat *C) 
{
  PetscErrorCode ierr;
  PetscErrorCode (*fA)(Mat,Mat,MatReuse,PetscReal,Mat*);
  PetscErrorCode (*fB)(Mat,Mat,MatReuse,PetscReal,Mat*);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(A,MAT_COOKIE,1);
  PetscValidType(A,1);
  MatPreallocated(A);
  if (!A->assembled) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Not for unassembled matrix");
  if (A->factor) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Not for factored matrix"); 
  PetscValidHeaderSpecific(B,MAT_COOKIE,2);
  PetscValidType(B,2);
  MatPreallocated(B);
  if (!B->assembled) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Not for unassembled matrix");
  if (B->factor) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Not for factored matrix"); 
  PetscValidPointer(C,3);
  if (B->M!=A->N) SETERRQ2(PETSC_ERR_ARG_SIZ,"Matrix dimensions are incompatible, %D != %D",B->M,A->N);

  if (fill <=0.0) SETERRQ1(PETSC_ERR_ARG_SIZ,"fill=%g must be > 0.0",fill);

  /* For now, we do not dispatch based on the type of A and B */
  /* When implementations like _SeqAIJ_MAIJ exist, attack the multiple dispatch problem. */  
  fA = A->ops->matmult;
  if (!fA) SETERRQ1(PETSC_ERR_SUP,"MatMatMult not supported for A of type %s",A->type_name);
  fB = B->ops->matmult;
  if (!fB) SETERRQ1(PETSC_ERR_SUP,"MatMatMult not supported for B of type %s",B->type_name);
  if (fB!=fA) SETERRQ2(PETSC_ERR_ARG_INCOMP,"MatMatMult requires A, %s, to be compatible with B, %s",A->type_name,B->type_name);

  ierr = PetscLogEventBegin(MAT_MatMult,A,B,0,0);CHKERRQ(ierr); 
  ierr = (*A->ops->matmult)(A,B,scall,fill,C);CHKERRQ(ierr);
  ierr = PetscLogEventEnd(MAT_MatMult,A,B,0,0);CHKERRQ(ierr); 
  
  PetscFunctionReturn(0);
} 

#undef __FUNCT__
#define __FUNCT__ "MatMatMult_SeqAIJ_SeqAIJ"
PetscErrorCode MatMatMult_SeqAIJ_SeqAIJ(Mat A,Mat B,MatReuse scall,PetscReal fill,Mat *C) 
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (scall == MAT_INITIAL_MATRIX){
    ierr = MatMatMultSymbolic_SeqAIJ_SeqAIJ(A,B,fill,C);CHKERRQ(ierr);
  }
  ierr = MatMatMultNumeric_SeqAIJ_SeqAIJ(A,B,*C);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "MatMatMultSymbolic"
/*@
   MatMatMultSymbolic - Performs construction, preallocation, and computes the ij structure
   of the matrix-matrix product C=A*B.  Call this routine before calling MatMatMultNumeric().

   Collective on Mat

   Input Parameters:
+  A - the left matrix
.  B - the right matrix
-  fill - expected fill as ratio of nnz(C)/(nnz(A) + nnz(B))

   Output Parameters:
.  C - the matrix containing the ij structure of product matrix

   Notes:
   C will be created as a MATSEQAIJ matrix and must be destroyed by the user with MatDestroy().

   This routine is currently only implemented for SeqAIJ matrices and classes which inherit from SeqAIJ.

   Level: intermediate

.seealso: MatMatMult(),MatMatMultNumeric()
@*/
PetscErrorCode MatMatMultSymbolic(Mat A,Mat B,PetscReal fill,Mat *C) 
{
  PetscErrorCode ierr;
  PetscErrorCode (*Asymbolic)(Mat,Mat,PetscReal,Mat *);
  PetscErrorCode (*Bsymbolic)(Mat,Mat,PetscReal,Mat *);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(A,MAT_COOKIE,1);
  PetscValidType(A,1);
  MatPreallocated(A);
  if (!A->assembled) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Not for unassembled matrix");
  if (A->factor) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Not for factored matrix"); 

  PetscValidHeaderSpecific(B,MAT_COOKIE,2);
  PetscValidType(B,2);
  MatPreallocated(B);
  if (!B->assembled) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Not for unassembled matrix");
  if (B->factor) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Not for factored matrix"); 
  PetscValidPointer(C,3);

  if (B->M!=A->N) SETERRQ2(PETSC_ERR_ARG_SIZ,"Matrix dimensions are incompatible, %D != %D",B->M,A->N);
  if (fill <=0.0) SETERRQ1(PETSC_ERR_ARG_SIZ,"fill=%g must be > 0.0",fill);

  /* For now, we do not dispatch based on the type of A and P */
  /* When implementations like _SeqAIJ_MAIJ exist, attack the multiple dispatch problem. */  
  Asymbolic = A->ops->matmultsymbolic;
  if (!Asymbolic) SETERRQ1(PETSC_ERR_SUP,"C=A*B not implemented for A of type %s",A->type_name);
  Bsymbolic = B->ops->matmultsymbolic;
  if (!Bsymbolic) SETERRQ1(PETSC_ERR_SUP,"C=A*B not implemented for B of type %s",B->type_name);
  if (Bsymbolic!=Asymbolic) SETERRQ2(PETSC_ERR_ARG_INCOMP,"MatMatMultSymbolic requires A, %s, to be compatible with B, %s",A->type_name,B->type_name);

  ierr = PetscLogEventBegin(MAT_MatMultSymbolic,A,B,0,0);CHKERRQ(ierr); 
  ierr = (*Asymbolic)(A,B,fill,C);CHKERRQ(ierr);
  ierr = PetscLogEventEnd(MAT_MatMultSymbolic,A,B,0,0);CHKERRQ(ierr); 

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMatMultSymbolic_SeqAIJ_SeqAIJ"
PetscErrorCode MatMatMultSymbolic_SeqAIJ_SeqAIJ(Mat A,Mat B,PetscReal fill,Mat *C)
{
  PetscErrorCode ierr;
  FreeSpaceList  free_space=PETSC_NULL,current_space=PETSC_NULL;
  Mat_SeqAIJ     *a=(Mat_SeqAIJ*)A->data,*b=(Mat_SeqAIJ*)B->data,*c;
  PetscInt       *ai=a->i,*aj=a->j,*bi=b->i,*bj=b->j,*bjj,*ci,*cj;
  PetscInt       am=A->M,bn=B->N,bm=B->M;
  PetscInt       i,j,anzi,brow,bnzj,cnzi,nlnk,*lnk,nspacedouble=0;
  MatScalar      *ca;
  PetscBT        lnkbt;

  PetscFunctionBegin;
  /* Set up */
  /* Allocate ci array, arrays for fill computation and */
  /* free space for accumulating nonzero column info */
  ierr = PetscMalloc(((am+1)+1)*sizeof(PetscInt),&ci);CHKERRQ(ierr);
  ci[0] = 0;
  
  /* create and initialize a linked list */
  nlnk = bn+1;
  ierr = PetscLLCreate(bn,bn,nlnk,lnk,lnkbt);CHKERRQ(ierr);

  /* Initial FreeSpace size is fill*(nnz(A)+nnz(B)) */
  ierr = GetMoreSpace((PetscInt)(fill*(ai[am]+bi[bm])),&free_space);CHKERRQ(ierr);
  current_space = free_space;

  /* Determine symbolic info for each row of the product: */
  for (i=0;i<am;i++) {
    anzi = ai[i+1] - ai[i];
    cnzi = 0;
    j    = anzi;
    aj   = a->j + ai[i];
    while (j){/* assume cols are almost in increasing order, starting from its end saves computation */
      j--;
      brow = *(aj + j);
      bnzj = bi[brow+1] - bi[brow];
      bjj  = bj + bi[brow];
      /* add non-zero cols of B into the sorted linked list lnk */
      ierr = PetscLLAdd(bnzj,bjj,bn,nlnk,lnk,lnkbt);CHKERRQ(ierr);
      cnzi += nlnk;
    }

    /* If free space is not available, make more free space */
    /* Double the amount of total space in the list */
    if (current_space->local_remaining<cnzi) {
      ierr = GetMoreSpace(current_space->total_array_size,&current_space);CHKERRQ(ierr);
      nspacedouble++;
    }

    /* Copy data into free space, then initialize lnk */
    ierr = PetscLLClean(bn,bn,cnzi,lnk,current_space->array,lnkbt);CHKERRQ(ierr);
    current_space->array           += cnzi;
    current_space->local_used      += cnzi;
    current_space->local_remaining -= cnzi;

    ci[i+1] = ci[i] + cnzi;
  }

  /* Column indices are in the list of free space */
  /* Allocate space for cj, initialize cj, and */
  /* destroy list of free space and other temporary array(s) */
  ierr = PetscMalloc((ci[am]+1)*sizeof(PetscInt),&cj);CHKERRQ(ierr);
  ierr = MakeSpaceContiguous(&free_space,cj);CHKERRQ(ierr);
  ierr = PetscLLDestroy(lnk,lnkbt);CHKERRQ(ierr);
    
  /* Allocate space for ca */
  ierr = PetscMalloc((ci[am]+1)*sizeof(MatScalar),&ca);CHKERRQ(ierr);
  ierr = PetscMemzero(ca,(ci[am]+1)*sizeof(MatScalar));CHKERRQ(ierr);
  
  /* put together the new symbolic matrix */
  ierr = MatCreateSeqAIJWithArrays(A->comm,am,bn,ci,cj,ca,C);CHKERRQ(ierr);

  /* MatCreateSeqAIJWithArrays flags matrix so PETSc doesn't free the user's arrays. */
  /* These are PETSc arrays, so change flags so arrays can be deleted by PETSc */
  c = (Mat_SeqAIJ *)((*C)->data);
  c->freedata = PETSC_TRUE;
  c->nonew    = 0;

  if (nspacedouble){
    PetscLogInfo((PetscObject)(*C),"MatMatMultSymbolic_SeqAIJ_SeqAIJ: nspacedouble:%D, nnz(A):%D, nnz(B):%D, fill:%g, nnz(C):%D\n",nspacedouble,ai[am],bi[bm],fill,ci[am]);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "MatMatMultNumeric"
/*@
   MatMatMultNumeric - Performs the numeric matrix-matrix product.
   Call this routine after first calling MatMatMultSymbolic().

   Collective on Mat

   Input Parameters:
+  A - the left matrix
-  B - the right matrix

   Output Parameters:
.  C - the product matrix, whose ij structure was defined from MatMatMultSymbolic().

   Notes:
   C must have been created with MatMatMultSymbolic.

   This routine is currently only implemented for SeqAIJ type matrices.

   Level: intermediate

.seealso: MatMatMult(),MatMatMultSymbolic()
@*/
PetscErrorCode MatMatMultNumeric(Mat A,Mat B,Mat C)
{
  PetscErrorCode ierr;
  PetscErrorCode (*Anumeric)(Mat,Mat,Mat);
  PetscErrorCode (*Bnumeric)(Mat,Mat,Mat);

  PetscFunctionBegin;

  PetscValidHeaderSpecific(A,MAT_COOKIE,1);
  PetscValidType(A,1);
  MatPreallocated(A);
  if (!A->assembled) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Not for unassembled matrix");
  if (A->factor) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Not for factored matrix"); 

  PetscValidHeaderSpecific(B,MAT_COOKIE,2);
  PetscValidType(B,2);
  MatPreallocated(B);
  if (!B->assembled) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Not for unassembled matrix");
  if (B->factor) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Not for factored matrix"); 

  PetscValidHeaderSpecific(C,MAT_COOKIE,3);
  PetscValidType(C,3);
  MatPreallocated(C);
  if (!C->assembled) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Not for unassembled matrix");
  if (C->factor) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Not for factored matrix"); 

  if (B->N!=C->N) SETERRQ2(PETSC_ERR_ARG_SIZ,"Matrix dimensions are incompatible, %D != %D",B->N,C->N);
  if (B->M!=A->N) SETERRQ2(PETSC_ERR_ARG_SIZ,"Matrix dimensions are incompatible, %D != %D",B->M,A->N);
  if (A->M!=C->M) SETERRQ2(PETSC_ERR_ARG_SIZ,"Matrix dimensions are incompatible, %D != %D",A->M,C->M);

  /* For now, we do not dispatch based on the type of A and B */
  /* When implementations like _SeqAIJ_MAIJ exist, attack the multiple dispatch problem. */  
  Anumeric = A->ops->matmultnumeric;
  if (!Anumeric) SETERRQ1(PETSC_ERR_SUP,"MatMatMultNumeric not supported for A of type %s",A->type_name);
  Bnumeric = B->ops->matmultnumeric;
  if (!Bnumeric) SETERRQ1(PETSC_ERR_SUP,"MatMatMultNumeric not supported for B of type %s",B->type_name);
  if (Bnumeric!=Anumeric) SETERRQ2(PETSC_ERR_ARG_INCOMP,"MatMatMultNumeric requires A, %s, to be compatible with B, %s",A->type_name,B->type_name);

  ierr = PetscLogEventBegin(MAT_MatMultNumeric,A,B,0,0);CHKERRQ(ierr); 
  ierr = (*Anumeric)(A,B,C);CHKERRQ(ierr);
  ierr = PetscLogEventEnd(MAT_MatMultNumeric,A,B,0,0);CHKERRQ(ierr); 

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMatMultNumeric_SeqAIJ_SeqAIJ"
PetscErrorCode MatMatMultNumeric_SeqAIJ_SeqAIJ(Mat A,Mat B,Mat C)
{
  PetscErrorCode ierr;
  PetscInt       flops=0;
  Mat_SeqAIJ     *a = (Mat_SeqAIJ *)A->data;
  Mat_SeqAIJ     *b = (Mat_SeqAIJ *)B->data;
  Mat_SeqAIJ     *c = (Mat_SeqAIJ *)C->data;
  PetscInt       *ai=a->i,*aj=a->j,*bi=b->i,*bj=b->j,*bjj,*ci=c->i,*cj=c->j;
  PetscInt       am=A->M,cm=C->M;
  PetscInt       i,j,k,anzi,bnzi,cnzi,brow,nextb;
  MatScalar      *aa=a->a,*ba=b->a,*baj,*ca=c->a; 

  PetscFunctionBegin;  
  /* clean old values in C */
  ierr = PetscMemzero(ca,ci[cm]*sizeof(MatScalar));CHKERRQ(ierr);
  /* Traverse A row-wise. */
  /* Build the ith row in C by summing over nonzero columns in A, */
  /* the rows of B corresponding to nonzeros of A. */
  for (i=0;i<am;i++) {
    anzi = ai[i+1] - ai[i];
    for (j=0;j<anzi;j++) {
      brow = *aj++;
      bnzi = bi[brow+1] - bi[brow];
      bjj  = bj + bi[brow];
      baj  = ba + bi[brow];
      nextb = 0;
      for (k=0; nextb<bnzi; k++) {
        if (cj[k] == bjj[nextb]){ /* ccol == bcol */
          ca[k] += (*aa)*baj[nextb++];
        }
      }
      flops += 2*bnzi;
      aa++;
    }
    cnzi = ci[i+1] - ci[i];
    ca += cnzi;
    cj += cnzi;
  }
  ierr = MatAssemblyBegin(C,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(C,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);     

  ierr = PetscLogFlops(flops);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "MatMatMultTranspose"
/*@
   MatMatMultTranspose - Performs Matrix-Matrix Multiplication C=A^T*B.

   Collective on Mat

   Input Parameters:
+  A - the left matrix
.  B - the right matrix
.  scall - either MAT_INITIAL_MATRIX or MAT_REUSE_MATRIX
-  fill - expected fill as ratio of nnz(C)/(nnz(A) + nnz(B))

   Output Parameters:
.  C - the product matrix

   Notes:
   C will be created and must be destroyed by the user with MatDestroy().

   This routine is currently only implemented for pairs of SeqAIJ matrices and classes
   which inherit from SeqAIJ.  C will be of type MATSEQAIJ.

   Level: intermediate

.seealso: MatMatMultTransposeSymbolic(),MatMatMultTransposeNumeric()
@*/
PetscErrorCode MatMatMultTranspose(Mat A,Mat B,MatReuse scall,PetscReal fill,Mat *C) 
{
  PetscErrorCode ierr;
  PetscErrorCode (*fA)(Mat,Mat,MatReuse,PetscReal,Mat*);
  PetscErrorCode (*fB)(Mat,Mat,MatReuse,PetscReal,Mat*);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(A,MAT_COOKIE,1);
  PetscValidType(A,1);
  MatPreallocated(A);
  if (!A->assembled) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Not for unassembled matrix");
  if (A->factor) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Not for factored matrix"); 
  PetscValidHeaderSpecific(B,MAT_COOKIE,2);
  PetscValidType(B,2);
  MatPreallocated(B);
  if (!B->assembled) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Not for unassembled matrix");
  if (B->factor) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Not for factored matrix"); 
  PetscValidPointer(C,3);
  if (B->M!=A->M) SETERRQ2(PETSC_ERR_ARG_SIZ,"Matrix dimensions are incompatible, %D != %D",B->M,A->M);

  if (fill <=0.0) SETERRQ1(PETSC_ERR_ARG_SIZ,"fill=%g must be > 0.0",fill);

  fA = A->ops->matmulttranspose;
  if (!fA) SETERRQ1(PETSC_ERR_SUP,"MatMatMultTranspose not supported for A of type %s",A->type_name);
  fB = B->ops->matmulttranspose;
  if (!fB) SETERRQ1(PETSC_ERR_SUP,"MatMatMultTranspose not supported for B of type %s",B->type_name);
  if (fB!=fA) SETERRQ2(PETSC_ERR_ARG_INCOMP,"MatMatMultTranspose requires A, %s, to be compatible with B, %s",A->type_name,B->type_name);

  ierr = PetscLogEventBegin(MAT_MatMultTranspose,A,B,0,0);CHKERRQ(ierr); 
  ierr = (*A->ops->matmulttranspose)(A,B,scall,fill,C);CHKERRQ(ierr);
  ierr = PetscLogEventEnd(MAT_MatMultTranspose,A,B,0,0);CHKERRQ(ierr); 
  
  PetscFunctionReturn(0);
} 

#undef __FUNCT__
#define __FUNCT__ "MatMatMultTranspose_SeqAIJ_SeqAIJ"
PetscErrorCode MatMatMultTranspose_SeqAIJ_SeqAIJ(Mat A,Mat B,MatReuse scall,PetscReal fill,Mat *C) {
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (scall == MAT_INITIAL_MATRIX){
    ierr = MatMatMultTransposeSymbolic_SeqAIJ_SeqAIJ(A,B,fill,C);CHKERRQ(ierr);
  }
  ierr = MatMatMultTransposeNumeric_SeqAIJ_SeqAIJ(A,B,*C);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMatMultTransposeSymbolic_SeqAIJ_SeqAIJ"
PetscErrorCode MatMatMultTransposeSymbolic_SeqAIJ_SeqAIJ(Mat A,Mat B,PetscReal fill,Mat *C)
{
  PetscErrorCode ierr;
  Mat            At;
  PetscInt       *ati,*atj;

  PetscFunctionBegin;
  /* create symbolic At */
  ierr = MatGetSymbolicTranspose_SeqAIJ(A,&ati,&atj);CHKERRQ(ierr);
  ierr = MatCreateSeqAIJWithArrays(PETSC_COMM_SELF,A->n,A->m,ati,atj,PETSC_NULL,&At);CHKERRQ(ierr);

  /* get symbolic C=At*B */
  ierr = MatMatMultSymbolic_SeqAIJ_SeqAIJ(At,B,fill,C);CHKERRQ(ierr);

  /* clean up */
  ierr = MatDestroy(At);CHKERRQ(ierr);
  ierr = MatRestoreSymbolicTranspose_SeqAIJ(A,&ati,&atj);CHKERRQ(ierr);
 
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMatMultTransposeNumeric_SeqAIJ_SeqAIJ"
PetscErrorCode MatMatMultTransposeNumeric_SeqAIJ_SeqAIJ(Mat A,Mat B,Mat C)
{
  PetscErrorCode ierr; 
  Mat_SeqAIJ     *a=(Mat_SeqAIJ*)A->data,*b=(Mat_SeqAIJ*)B->data,*c=(Mat_SeqAIJ*)C->data;
  PetscInt       am=A->m,anzi,*ai=a->i,*aj=a->j,*bi=b->i,*bj,bnzi,nextb;
  PetscInt       cm=C->m,*ci=c->i,*cj=c->j,crow,*cjj,i,j,k,flops=0;
  MatScalar      *aa=a->a,*ba,*ca=c->a,*caj;
 
  PetscFunctionBegin;
  /* clear old values in C */
  ierr = PetscMemzero(ca,ci[cm]*sizeof(MatScalar));CHKERRQ(ierr);

  /* compute A^T*B using outer product (A^T)[:,i]*B[i,:] */
  for (i=0;i<am;i++) {
    bj   = b->j + bi[i];
    ba   = b->a + bi[i];
    bnzi = bi[i+1] - bi[i];
    anzi = ai[i+1] - ai[i];
    for (j=0; j<anzi; j++) { 
      nextb = 0;
      crow  = *aj++;
      cjj   = cj + ci[crow];
      caj   = ca + ci[crow];
      /* perform sparse axpy operation.  Note cjj includes bj. */
      for (k=0; nextb<bnzi; k++) {
        if (cjj[k] == *(bj+nextb)) { /* ccol == bcol */
          caj[k] += (*aa)*(*(ba+nextb));
          nextb++;
        }
      }
      flops += 2*bnzi;
      aa++;
    }
  }

  /* Assemble the final matrix and clean up */
  ierr = MatAssemblyBegin(C,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(C,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = PetscLogFlops(flops);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

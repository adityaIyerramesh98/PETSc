
static char help[] = "Tests MatMatSolve() for computing inv(A) with MUMPS.\n\
Example: mpiexec -n 1 ./ex214 -f <matrix binary file> -nrhs 4 \n\n";

#include <petscmat.h>

int main(int argc,char **args)
{
  Mat            A,RHS,C,F,X;//,S;
  Vec            u,x,b;
  PetscErrorCode ierr;
  PetscMPIInt    size;
  PetscInt       m,n,nrhs;
  PetscReal      norm,tol=PETSC_SQRT_MACHINE_EPSILON;
  PetscRandom    rand;
  PetscBool      data_provided;
  PetscViewer    fd;              /* viewer */
  char           solver[256];
  char           file[PETSC_MAX_PATH_LEN]; /* input file name */

  ierr = PetscInitialize(&argc,&args,(char*)0,help);if (ierr) return ierr;
  ierr = MPI_Comm_size(PETSC_COMM_WORLD, &size);CHKERRQ(ierr);
  if (size > 1) SETERRQ(PETSC_COMM_WORLD,1,"This is a uniprocessor test");

  /* Determine file from which we read the matrix A */
  ierr = PetscOptionsGetString(NULL,NULL,"-f",file,PETSC_MAX_PATH_LEN,&data_provided);CHKERRQ(ierr);
  if (!data_provided) { /* get matrices from PETSc distribution */
    ierr = PetscStrcpy(file,"${PETSC_DIR}/share/petsc/datafiles/matrices/");CHKERRQ(ierr);

#if defined (PETSC_USE_COMPLEX)
      ierr = PetscStrcat(file,"nh-complex-");CHKERRQ(ierr);
#else
      ierr = PetscStrcat(file,"ns-real-");CHKERRQ(ierr);
#endif

#if defined(PETSC_USE_64BIT_INDICES)
    ierr = PetscStrcat(file,"int64-");CHKERRQ(ierr);
#else
    ierr = PetscStrcat(file,"int32-");CHKERRQ(ierr);
#endif
#if defined (PETSC_USE_REAL_SINGLE)
    ierr = PetscStrcat(file,"float32");CHKERRQ(ierr);
#else
    ierr = PetscStrcat(file,"float64");CHKERRQ(ierr);
#endif
  }

  /* Load matrix A */
  ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,file,FILE_MODE_READ,&fd);CHKERRQ(ierr);
  ierr = MatCreate(PETSC_COMM_WORLD,&A);CHKERRQ(ierr);
  ierr = MatLoad(A,fd);CHKERRQ(ierr);
  ierr = PetscViewerDestroy(&fd);CHKERRQ(ierr);
  ierr = MatGetSize(A,&m,&n);CHKERRQ(ierr);
  if (m != n) SETERRQ2(PETSC_COMM_SELF,PETSC_ERR_ARG_SIZ, "This example is not intended for rectangular matrices (%d, %d)", m, n);

  /* Create dense matrix C and X; C holds true solution with identical colums */
  nrhs = 2;
  ierr = PetscOptionsGetInt(NULL,NULL,"-nrhs",&nrhs,NULL);CHKERRQ(ierr);
  ierr = MatCreate(PETSC_COMM_WORLD,&C);CHKERRQ(ierr);
  ierr = MatSetSizes(C,m,PETSC_DECIDE,PETSC_DECIDE,nrhs);CHKERRQ(ierr);
  ierr = MatSetType(C,MATDENSE);CHKERRQ(ierr);
  ierr = MatSetFromOptions(C);CHKERRQ(ierr);
  ierr = MatSetUp(C);CHKERRQ(ierr);

  ierr = PetscRandomCreate(PETSC_COMM_WORLD,&rand);CHKERRQ(ierr);
  ierr = PetscRandomSetFromOptions(rand);CHKERRQ(ierr);
  ierr = MatSetRandom(C,rand);CHKERRQ(ierr);
  ierr = MatDuplicate(C,MAT_DO_NOT_COPY_VALUES,&X);CHKERRQ(ierr);

  /* Create vectors */
  ierr = VecCreate(PETSC_COMM_WORLD,&x);CHKERRQ(ierr);
  ierr = VecSetSizes(x,n,PETSC_DECIDE);CHKERRQ(ierr);
  ierr = VecSetFromOptions(x);CHKERRQ(ierr);
  ierr = VecDuplicate(x,&b);CHKERRQ(ierr);
  ierr = VecDuplicate(x,&u);CHKERRQ(ierr); /* save the true solution */

  ierr = PetscStrcpy(solver,MATSOLVERMUMPS);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_SELF,"Solving with %s: nrhs %D, size mat %D\n",solver,nrhs,m);CHKERRQ(ierr);

  /* Test LU Factorization */
  ierr = MatGetFactor(A,solver,MAT_FACTOR_LU,&F);CHKERRQ(ierr);
  ierr = MatLUFactorSymbolic(F,A,NULL,NULL,NULL);CHKERRQ(ierr);

  ierr = VecSetRandom(x,rand);CHKERRQ(ierr);
  ierr = MatLUFactorNumeric(F,A,NULL);CHKERRQ(ierr);

  ierr = VecSetRandom(x,rand);CHKERRQ(ierr);
  ierr = VecCopy(x,u);CHKERRQ(ierr);

  /* (1) Test MatMatSolve(): dense RHS = A*C, C: true solutions */
  ierr = MatMatMult(A,C,MAT_INITIAL_MATRIX,2.0,&RHS);CHKERRQ(ierr);
  ierr = MatMatSolve(F,RHS,X);CHKERRQ(ierr);

  /* Check the error */
  ierr = MatAXPY(X,-1.0,C,SAME_NONZERO_PATTERN);CHKERRQ(ierr);
  ierr = MatNorm(X,NORM_FROBENIUS,&norm);CHKERRQ(ierr);
  if (norm > tol) {
    ierr = PetscPrintf(PETSC_COMM_SELF,"(1) MatMatSolve: Norm of error %g\n",norm);CHKERRQ(ierr);
  }

  /* (2) Test MatMatSolve() for inv(A) with dense RHS:
   RHS = [e[0],...,e[nrhs-1], dense X holds first nrhs columns of inv(A) */
  ierr = MatZeroEntries(RHS);CHKERRQ(ierr);
  PetscInt i;
  for (i=0; i<nrhs; i++) {
    PetscScalar one = 1.0;
    ierr = MatSetValues(RHS,1,&i,1,&i,&one,INSERT_VALUES);CHKERRQ(ierr);
  }
  ierr = MatAssemblyBegin(RHS,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(RHS,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);

  ierr = MatMatSolve(F,RHS,X);CHKERRQ(ierr);
  /* Check the residual */
  Mat AX;
  ierr = MatMatMult(A,X,MAT_INITIAL_MATRIX,2.0,&AX);CHKERRQ(ierr);
  ierr = MatAXPY(AX,-1.0,RHS,SAME_NONZERO_PATTERN);CHKERRQ(ierr);
  ierr = MatNorm(AX,NORM_INFINITY,&norm);CHKERRQ(ierr);
  if (norm > tol) {
    ierr = PetscPrintf(PETSC_COMM_SELF,"(2) MatMatSolve: Norm of residual %g\n",norm);CHKERRQ(ierr);
  }

  /* (3) Test MatMatSolve() for inv(A) with sparse RHS:
   spRHS = [e[0],...,e[nrhs-1], dense X holds first nrhs columns of inv(A) */
  Mat spRHS,spRHST,RHST;

  ierr = MatTranspose(RHS,MAT_INITIAL_MATRIX,&RHST);CHKERRQ(ierr);
  ierr = MatConvert(RHST,MATSEQAIJ,MAT_INITIAL_MATRIX,&spRHST);CHKERRQ(ierr);

  /* MUMPS requres spRHS in compressed column format, which PETSc does not support.
     PETSc can create a new matrix object that shares same data structure with A and behaves like A^T */
  ierr = MatCreateTranspose(spRHST,&spRHS);CHKERRQ(ierr);
  ierr = MatMatSolve(F,spRHS,X);CHKERRQ(ierr);
  ierr = MatView(X,0);CHKERRQ(ierr);

  /* Check the residual */
  ierr = MatMatMult(A,X,MAT_REUSE_MATRIX,2.0,&AX);CHKERRQ(ierr);
  ierr = MatAXPY(AX,-1.0,RHS,SAME_NONZERO_PATTERN);CHKERRQ(ierr);
  ierr = MatNorm(AX,NORM_INFINITY,&norm);CHKERRQ(ierr);
  if (norm > tol) {
    ierr = PetscPrintf(PETSC_COMM_SELF,"(3) MatMatSolve: Norm of residual %g\n",norm);CHKERRQ(ierr);
  }
  ierr = MatDestroy(&AX);CHKERRQ(ierr);

  /* (4) Test MatMatSolve() for inv(A) with selected entries:
   input: spRHS gives selected indices; output: spRHS holds selected entries of inv(A) */

  /* first, set mumps flag for computing user-specified set of entries in inv(A) */
  ierr = MatMumpsSetIcntl(F,30,1);CHKERRQ(ierr);

  ierr = MatMatSolve(F,spRHS,X);CHKERRQ(ierr); /* X is not being used here, but PETSc MatMatSolve() does not allow NULL as an input */
  printf("Transpose of sparse solution matrix:\n");
  ierr = MatView(spRHST,0);CHKERRQ(ierr); /* spRHST shares same data structure as spRHS */

  Mat spRHSTT;
  ierr = MatTranspose(spRHST,MAT_INITIAL_MATRIX,&spRHSTT);CHKERRQ(ierr);
  printf("\nSparse solution matrix:\n");
  ierr = MatView(spRHSTT,0);CHKERRQ(ierr);
  ierr = MatDestroy(&spRHSTT);CHKERRQ(ierr);

  ierr = MatDestroy(&spRHST);CHKERRQ(ierr);
  ierr = MatDestroy(&spRHS);CHKERRQ(ierr);
  ierr = MatDestroy(&RHST);CHKERRQ(ierr);

  /* Free data structures */
  ierr = MatDestroy(&A);CHKERRQ(ierr);
  ierr = MatDestroy(&C);CHKERRQ(ierr);
  ierr = MatDestroy(&F);CHKERRQ(ierr);
  ierr = MatDestroy(&X);CHKERRQ(ierr);
  ierr = MatDestroy(&RHS);CHKERRQ(ierr);
  ierr = PetscRandomDestroy(&rand);CHKERRQ(ierr);
  ierr = VecDestroy(&x);CHKERRQ(ierr);
  ierr = VecDestroy(&b);CHKERRQ(ierr);
  ierr = VecDestroy(&u);CHKERRQ(ierr);
  ierr = PetscFinalize();
  return ierr;
}

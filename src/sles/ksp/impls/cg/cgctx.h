/*  
    Private Context Structure for Conjugate Gradient 
*/

#if !defined(__CG)
#define __CG

#include "petsc.h"
#include "src/ksp/kspimpl.h"

typedef struct {
  Scalar    emin, emax;           /* eigenvalues */
  Scalar    *e, *d;
  double    *ee, *dd;             /* work space for Lanczos algorithm */
  KSPCGType type;                 /* type of system (symmetric or Hermitian) */
} KSP_CG;

#endif

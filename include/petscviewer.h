/* $Id: viewer.h,v 1.23 1996/03/08 05:49:06 bsmith Exp curfman $ */

#if !defined(__VIEWER_PACKAGE)
#define __VIEWER_PACKAGE

#include "petsc.h"

typedef struct _Viewer*            Viewer;
#define VIEWER_COOKIE              PETSC_COOKIE+1
typedef enum { MATLAB_VIEWER,ASCII_FILE_VIEWER, ASCII_FILES_VIEWER, 
               BINARY_FILE_VIEWER, STRING_VIEWER, DRAW_VIEWER} ViewerType;

#define FILE_FORMAT_DEFAULT       0
#define FILE_FORMAT_MATLAB        1
#define FILE_FORMAT_IMPL          2
#define FILE_FORMAT_INFO          3
#define FILE_FORMAT_INFO_DETAILED 4

extern int ViewerFileOpenASCII(MPI_Comm,char*,Viewer *);
typedef enum { BINARY_RDONLY, BINARY_WRONLY, BINARY_CREATE} ViewerBinaryType;
extern int ViewerFileOpenBinary(MPI_Comm,char*,ViewerBinaryType,Viewer *);
extern int ViewerMatlabOpen(MPI_Comm,char*,int,Viewer *);
extern int ViewerStringOpen(MPI_Comm,char *,int, Viewer *);
extern int ViewerDrawOpenX(MPI_Comm,char *,char *,int,int,int,int,Viewer*);

extern int ViewerGetType(Viewer,ViewerType*);
extern int ViewerDestroy(Viewer);

extern int ViewerFileGetPointer(Viewer,FILE**);
extern int ViewerFileGetDescriptor(Viewer,int*);
extern int ViewerFileSetFormat(Viewer,int,char *);
extern int ViewerFlush(Viewer);
extern int ViewerStringsprintf(Viewer,char *,...);

extern Viewer STDOUT_VIEWER_SELF;  
extern Viewer STDERR_VIEWER_SELF;
extern Viewer STDOUT_VIEWER_WORLD;

#endif

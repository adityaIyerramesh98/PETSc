#include <list>
#include <Distribution.hh>
#include "petscmesh.h"
#include "petscviewer.h"
#include "src/dm/mesh/meshpcice.h"
#include "src/dm/mesh/meshpylith.h"
#include <stdlib.h>
#include <string.h>
#include <string>
#include <triangle.h>
#include <tetgen.h>
  
  
namespace ALE {
namespace Coarsener {
  
  PetscErrorCode IdentifyBoundary(Obj<ALE::Mesh>&, int);  //identify the boundary faces/edges/nodes.
  PetscErrorCode CreateSpacingFunction(Obj<ALE::Mesh>&, int);  //same 'ol, same 'ol.  (puts a nearest neighbor value on each vertex) (edges?)
  PetscErrorCode CreateCoarsenedHierarchy(Obj<ALE::Mesh>&, int, int, float); //returns the meshes!
  PetscErrorCode TriangleToMesh(Obj<ALE::Mesh>, triangulateio *, ALE::Mesh::section_type::patch_type);
  PetscErrorCode LevelCoarsen(Obj<ALE::Mesh>&, int,  ALE::Mesh::section_type::patch_type, bool, float);
  int BoundaryNodeDimension_2D(Obj<ALE::Mesh>&, ALE::Mesh::point_type);
  int BoundaryNodeDimension_3D(Obj<ALE::Mesh>&, ALE::Mesh::point_type);
  
  
  ////////////////////////////////////////////////////////////////
  
  PetscErrorCode CreateSpacingFunction(Obj<ALE::Mesh> & mesh, int dim) {
    Obj<ALE::Mesh::topology_type> topology = mesh->getTopologyNew();
    ALE::Mesh::section_type::patch_type patch = 0;
    const Obj<ALE::Mesh::topology_type::label_sequence>& vertices = topology->depthStratum(patch, 0);
  
    PetscFunctionBegin;
  
    //initialize the spacing function section
  
    Obj<ALE::Mesh::section_type> spacing = mesh->getSection("spacing");
    spacing->setFiberDimensionByDepth(patch, 0, 1);
    spacing->allocate();
    
    Obj<ALE::Mesh::section_type> coords = mesh->getSection("coordinates");
    
    ALE::Mesh::topology_type::label_sequence::iterator v_iter = vertices->begin();
    ALE::Mesh::topology_type::label_sequence::iterator v_iter_end = vertices->end();
  
    double vCoords[dim], nCoords[dim];
  
    while (v_iter != v_iter_end) {
	//printf("vertex: %d\n", *v_iter);
      const double * rBuf = coords->restrict(patch, *v_iter);
      PetscMemcpy(vCoords, rBuf, dim*sizeof(double));
	  
      double minDist = -1; //using the max is silly.
    ALE::Obj<ALE::Mesh::sieve_type::traits::supportSequence> support = topology->getPatch(patch)->support(*v_iter);
    ALE::Mesh::topology_type::label_sequence::iterator s_iter     = support->begin();
    ALE::Mesh::topology_type::label_sequence::iterator s_iter_end = support->end();
    while(s_iter != s_iter_end) {
      ALE::Obj<ALE::Mesh::sieve_type::traits::coneSequence> neighbors = topology->getPatch(patch)->cone(*s_iter);
      ALE::Mesh::sieve_type::traits::coneSequence::iterator n_iter = neighbors->begin();
      ALE::Mesh::sieve_type::traits::coneSequence::iterator n_iter_end = neighbors->end();
      while(n_iter != n_iter_end) {
	if (*v_iter != *n_iter) {
	  rBuf = coords->restrict(patch, *n_iter);
	  PetscMemcpy(nCoords, rBuf, dim*sizeof(double));
	  double d_tmp, dist    = 0.0;

	  for (int d = 0; d < dim; d++) {
	    d_tmp = nCoords[d] - vCoords[d];
	    dist += d_tmp * d_tmp;
	  }

	  if (dist < minDist || minDist == -1) minDist = dist;
	}
	n_iter++;
      }
      s_iter++;
     }
    minDist = sqrt(minDist);
    spacing->update(patch, *v_iter, &minDist);
    v_iter++;
  }
  PetscFunctionReturn(0);
}

PetscErrorCode IdentifyBoundary(Obj<ALE::Mesh>& mesh, int dim) {

  ALE::Mesh::section_type::patch_type patch = 0;
  Obj<ALE::Mesh::topology_type> topology = mesh->getTopologyNew();
  const Obj<ALE::Mesh::topology_type::patch_label_type>& boundary = topology->createLabel(patch, "boundary");


  if (dim == 2) {
    const Obj<ALE::Mesh::topology_type::label_sequence>& edges = topology->heightStratum(patch, 1);
    const Obj<ALE::Mesh::topology_type::label_sequence>& vertices = topology->depthStratum(patch, 0);


    ALE::Mesh::topology_type::label_sequence::iterator e_iter = edges->begin();
    ALE::Mesh::topology_type::label_sequence::iterator e_iter_end = edges->end();

    ALE::Mesh::topology_type::label_sequence::iterator v_iter = vertices->begin();
    ALE::Mesh::topology_type::label_sequence::iterator v_iter_end = vertices->end();

//initialize all the vertices

    while (v_iter != v_iter_end) {
      topology->setValue(boundary, *v_iter, 0);
      v_iter++;
    }

//trace through the edges, initializing them to be non-boundary, then setting them as boundary.

    // int nBoundaryVertices = 0;
    while (e_iter != e_iter_end) {
      topology->setValue(boundary, *e_iter, 0);
//find out if the edge is not supported on both sides, if so, this is a boundary node
       //printf("Edge %d supported by %d faces", *e_iter, topology->getPatch(patch)->support(*e_iter)->size());
      if (topology->getPatch(patch)->support(*e_iter)->size() < 2) {
	topology->setValue(boundary, *e_iter, 1);
	ALE::Obj<ALE::Mesh::sieve_type::traits::coneSequence> endpoints = topology->getPatch(patch)->cone(*e_iter); //the adjacent elements
	ALE::Mesh::sieve_type::traits::coneSequence::iterator p_iter = endpoints->begin();
	ALE::Mesh::sieve_type::traits::coneSequence::iterator p_iter_end = endpoints->end();
	while (p_iter != p_iter_end) {
	  topology->setValue(boundary, *p_iter, -1);
           //boundVerts++;
	  p_iter++;
	}
      }
      e_iter++;
    }

//determine if the edge is straight or not.

    v_iter = vertices->begin();
    v_iter_end = vertices->end();
    while(v_iter != v_iter_end) {
      if (topology->getValue(boundary, *v_iter) == -1) {
      topology->setValue(boundary, *v_iter, BoundaryNodeDimension_2D(mesh, *v_iter));
      //printf("set boundary dimension for %d as %d\n", *v_iter, topology->getValue(boundary, *v_iter));
      }
      v_iter++;
    }
  } else if (dim == 3) {  //loop over the faces to determine the 
      
  } else {
      
  }
PetscFunctionReturn(0);
}

PetscErrorCode CreateCoarsenedHierarchy(Obj<ALE::Mesh>& mesh, int dim, int nMeshes, float beta) {
   //in this function we will assume that the original mesh is given to us in patch 0, and that its boundary has been identified with IdentifyBoundary.  We will put nMesh - 1 coarsenings in patches 1 through nMeshes.
   
  for (int curLevel = nMeshes; curLevel > 0; curLevel--) {
      printf("Creating coarsening level: %d\n", curLevel);
      bool isTopLevel = (curLevel == nMeshes);
      double crsBeta = pow(1.41, curLevel);
      printf("Creating coarsening level: %d with beta = %f\n", curLevel, crsBeta);
      LevelCoarsen(mesh, dim, curLevel, !isTopLevel,crsBeta);
  }
   mesh->getTopologyNew()->stratify();
  PetscFunctionReturn(0);
}

PetscErrorCode LevelCoarsen(Obj<ALE::Mesh>& mesh, int dim,  ALE::Mesh::section_type::patch_type newPatch, bool includePrevious, float beta) {
  PetscFunctionBegin;
    
  ALE::Mesh::section_type::patch_type originalPatch = 0;
  std::list<ALE::Mesh::point_type> incPoints;
  Obj<ALE::Mesh::topology_type> topology = mesh->getTopologyNew();
  double v_coord[dim], c_coord[dim];
  Obj<ALE::Mesh::section_type> coords = mesh->getSection("coordinates");
  Obj<ALE::Mesh::section_type> spacing = mesh->getSection("spacing");

  const Obj<ALE::Mesh::topology_type::patch_label_type>& boundary = topology->getLabel(originalPatch, "boundary");
  if(includePrevious) {

    ALE::Mesh::section_type::patch_type coarserPatch = newPatch+1;
    const Obj<ALE::Mesh::topology_type::label_sequence>& previousVertices = topology->depthStratum(coarserPatch, 0);

      //Add the vertices from the next coarser patch to the list of included vertices.
      ALE::Mesh::topology_type::label_sequence::iterator v_iter = previousVertices->begin();
      ALE::Mesh::topology_type::label_sequence::iterator v_iter_end = previousVertices->end();
    while(v_iter != v_iter_end) {
      incPoints.push_front(*v_iter);
      v_iter++;
    }
  } else {
      //get the essential boundary nodes and add them to the set.
    const Obj<ALE::Mesh::topology_type::label_sequence>& essVertices = topology->getLabelStratum(originalPatch, "boundary", dim);
    printf("- adding %d boundary nodes.\n", essVertices->size());
    ALE::Mesh::topology_type::label_sequence::iterator v_iter = essVertices->begin();
    ALE::Mesh::topology_type::label_sequence::iterator v_iter_end = essVertices->end();
    while (v_iter != v_iter_end) {
      incPoints.push_front(*v_iter);
      v_iter++;
    }
  }
//for now just loop over the points
    const Obj<ALE::Mesh::topology_type::label_sequence>& verts = topology->depthStratum(originalPatch, 0);
    ALE::Mesh::topology_type::label_sequence::iterator v_iter = verts->begin();
    ALE::Mesh::topology_type::label_sequence::iterator v_iter_end = verts->end();
    while (v_iter != v_iter_end) {
      PetscMemcpy(v_coord, coords->restrict(originalPatch, *v_iter), dim*sizeof(double));
      double v_space = *spacing->restrict(originalPatch, *v_iter);
      std::list<ALE::Mesh::point_type>::iterator c_iter = incPoints.begin(), c_iter_end = incPoints.end();
      bool isOk = true;
	  while (c_iter != c_iter_end) {
	    PetscMemcpy(c_coord, coords->restrict(originalPatch, *c_iter), dim*sizeof(double));
	    double dist = 0;
	    double c_space = *spacing->restrict(originalPatch, *c_iter);
	    for (int d = 0; d < dim; d++) {
	      dist += (v_coord[d] - c_coord[d])*(v_coord[d] - c_coord[d]);
	    }
	    double mdist = c_space + v_space;
	    if (dist < beta*beta*mdist*mdist/4) {
	      isOk = false;
              break;
	    }
            c_iter++;
	  }
	  if (isOk) {
	    incPoints.push_front(*v_iter);
            //printf("  - Adding point %d to the new mesh\n", *v_iter);
	  }
	  v_iter++;
    }

  printf("- creating input to triangle: %d points\n", incPoints.size());
  //At this point we will set up the triangle(tetgen) calls (with preservation of vertex order.  This is why I do not use the functions build in).
  triangulateio * input = new triangulateio;
  triangulateio * output = new triangulateio;
  
  input->numberofpoints = incPoints.size();
  input->numberofpointattributes = 0;
  input->pointlist = new double[dim*input->numberofpoints];

  //copy the points over
  std::list<ALE::Mesh::point_type>::iterator c_iter = incPoints.begin(), c_iter_end = incPoints.end();

  int index = 0;
  while (c_iter != c_iter_end) {
     PetscMemcpy(input->pointlist + dim*index, coords->restrict(originalPatch, *c_iter), dim*sizeof(double));
     c_iter++;
     index++;
  }

  //ierr = PetscPrintf(srcMesh->comm(), "copy is ok\n");
  input->numberofpointattributes = 0;
  input->pointattributelist = NULL;

//set up the pointmarkerlist to hold the names of the points

  input->pointmarkerlist = new int[input->numberofpoints];
  c_iter = incPoints.begin();
  c_iter_end = incPoints.end();
  index = 0;
  while(c_iter != c_iter_end) {
    input->pointmarkerlist[index] = *c_iter;
    c_iter++;
    index++;
  }


  input->numberoftriangles = 0;
  input->numberofcorners = 0;
  input->numberoftriangleattributes = 0;
  input->trianglelist = NULL;
  input->triangleattributelist = NULL;
  input->trianglearealist = NULL;

  input->segmentlist = NULL;
  input->segmentmarkerlist = NULL;
  input->numberofsegments = 0;

  input->holelist = NULL;
  input->numberofholes = 0;
   
  input->regionlist = NULL;
  input->numberofregions = 0;

  output->pointlist = NULL;
  output->pointattributelist = NULL;
  output->pointmarkerlist = NULL;
  output->trianglelist = NULL;
  output->triangleattributelist = NULL;
  output->trianglearealist = NULL;
  output->neighborlist = NULL;
  output->segmentlist = NULL;
  output->segmentmarkerlist = NULL;
  output->holelist = NULL;
  output->regionlist = NULL;
  output->edgelist = NULL;
  output->edgemarkerlist = NULL;
  output->normlist = NULL;

  string triangleOptions = "-zeQ"; //(z)ero indexing, output (e)dges, Quiet
  triangulate((char *)triangleOptions.c_str(), input, output, NULL);
  TriangleToMesh(mesh, output, newPatch);
  delete input->pointlist;
  delete output->pointlist;
  delete output->trianglelist;
  delete output->edgelist;
  delete input;
  delete output;
  PetscFunctionReturn(0);
}

int BoundaryNodeDimension_2D(Obj<ALE::Mesh>& mesh, ALE::Mesh::point_type vertex) {

  ALE::Mesh::section_type::patch_type patch = 0; 
  Obj<ALE::Mesh::topology_type> topology = mesh->getTopologyNew();
  const Obj<ALE::Mesh::topology_type::patch_label_type>& markers = topology->getLabel(patch, "boundary");
  Obj<ALE::Mesh::section_type> coords = mesh->getSection("coordinates");
  const double *vCoords = coords->restrict(patch, vertex);
  double v_x = vCoords[0], v_y = vCoords[1];
  bool foundNeighbor = false;
  int isEssential = 1;
  
  double f_n_x, f_n_y;
  
  ALE::Obj<ALE::Mesh::sieve_type::traits::supportSequence> support = topology->getPatch(patch)->support(vertex);
  ALE::Mesh::topology_type::label_sequence::iterator s_iter = support->begin();
  ALE::Mesh::topology_type::label_sequence::iterator s_iter_end = support->end();
  while(s_iter != s_iter_end) {
      if (topology->getValue(markers, *s_iter) == 1) {
      ALE::Obj<ALE::Mesh::sieve_type::traits::coneSequence> neighbors = topology->getPatch(patch)->cone(*s_iter);
      ALE::Mesh::sieve_type::traits::coneSequence::iterator n_iter = neighbors->begin();
      ALE::Mesh::sieve_type::traits::coneSequence::iterator n_iter_end = neighbors->end();
      while(n_iter != n_iter_end) {
	if (vertex != *n_iter) {
	  if (!foundNeighbor) {
	    const double *nCoords = coords->restrict(patch, *n_iter);
	    f_n_x = nCoords[0]; f_n_y = nCoords[1];
	    foundNeighbor = true;
	  } else {
	    const double *nCoords = coords->restrict(patch, *n_iter);
	    double n_x = nCoords[0], n_y = nCoords[1];
	    double parArea = fabs((f_n_x - v_x) * (n_y - v_y) - (f_n_y - v_y) * (n_x - v_x));
	    if (parArea > .0000001) isEssential = 2;
	   // printf("Parallelogram area: %f\n", parArea);
	  }
	}
	n_iter++;
      }
      }
    s_iter++;
  }
  return isEssential;
}

//determines if two triangles are coplanar

bool areCoPlanar(Obj<ALE::Mesh>& mesh, ALE::Mesh::point_type tri1, ALE::Mesh::point_type tri2) {
  Obj<ALE::Mesh::topology_type> topology = mesh->getTopologyNew();
    
  return false; // stub
}

PetscErrorCode TriangleToMesh(Obj<ALE::Mesh> mesh, triangulateio * src, ALE::Mesh::section_type::patch_type patch) {

  PetscErrorCode ierr;
  PetscFunctionBegin;

//preprocess the triangle list so that the triangles are built over the same sieve point list we started from.

  for (int i = 0; i != src->numberoftriangles; i++) {
     src->trianglelist[i] = src->pointmarkerlist[src->trianglelist[i]];
     src->trianglelist[i+1] = src->pointmarkerlist[src->trianglelist[i+1]];
  }

  Obj<ALE::Mesh::sieve_type> sieve = new ALE::Mesh::sieve_type(mesh->comm(), 0);
  Obj<ALE::Mesh::topology_type> topology = mesh->getTopologyNew();

//make the sieve and the topology actually count for something
  ALE::New::SieveBuilder<ALE::Mesh::sieve_type>::buildTopology(sieve, 2, src->numberoftriangles, src->trianglelist, src->numberofpoints, false, 3);
  sieve->stratify();
  topology->setPatch(patch, sieve);
  topology->stratify();
  
  PetscFunctionReturn(0);
}

}  //end Coarsener
}  //end ALE

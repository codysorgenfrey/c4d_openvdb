//
//  C4DOpenVDBFunctions.h
//  C4DOpenVDB
//
//  Created by Cody Sorgenfrey on 7/20/18.
//  Copyright © 2018 MAXON Computer GmbH. All rights reserved.
//
//  Wrapper class for openvdb stuff for the main C4DOpenVDB object

#ifndef C4DOpenVDBFunctions_h
#define C4DOpenVDBFunctions_h

#include "C4DOpenVDBObject.h"
#include "C4DOpenVDBVisualizer.h"

class VDBObjectHelper;

VDBObjectHelper* InitVDBObjectHelper();
void DeleteVDBObjectHelper(VDBObjectHelper *helper);
Bool SDFToFog(VDBObjectHelper *helper);
Bool FillSDFInterior(VDBObjectHelper *helper);
Bool UnsignSDF(VDBObjectHelper *helper);
Bool UpdateSurface(C4DOpenVDBObject *obj, BaseObject *op, Vector32 userColor);

// C4DOpenVDBPrimitive Functions
Bool MakeShape(VDBObjectHelper *helper, Int32 shape, float width, Vector center, float voxelSize, float bandWidth);

// C4DOpenVDBVisualizer Functions
Bool UpdateSurfaceSlice(C4DOpenVDBObject *vdb, C4DOpenVDBVisualizer *vis, Int32 axis, Float offset, Gradient *grad);

// C4DOpenVDBMesher Functions
Bool GetVDBPolygonized(BaseObject *inObject, Float iso, Float adapt, BaseObject *outObject);

// C4DOpenVDBCombine Functions
Bool CombineVDBs(C4DOpenVDBObject *obj,
                 maxon::BaseArray<BaseObject*> *inputs,
                 Int32 operation,
                 Int32 resample,
                 Int32 interpolation,
                 Bool deactivate,
                 Float deactivateVal,
                 Bool prune,
                 Float pruneVal,
                 Bool signedFloodFill);

#endif /* C4DOpenVDBFunctions_h */

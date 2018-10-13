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

Bool SDFToFog(VDBObjectHelper *helper);
Bool FillSDFInterior(VDBObjectHelper *helper);
Bool UnsignSDF(VDBObjectHelper *helper);
Bool UpdateSurface(C4DOpenVDBObject *obj, BaseObject *op, Vector32 userColor);
void ClearSurface(C4DOpenVDBObject *obj, Bool free);

// C4DOpenVDBPrimitive Functions
Bool MakeShape(VDBObjectHelper *helper, Int32 shape, Float width, Vector center, Float voxelSize, Int32 bandWidth);

// C4DOpenVDBVisualizer Functions
Bool UpdateVisualizerSlice(C4DOpenVDBObject *vdb, C4DOpenVDBVisualizer *vis, Int32 axis, Float offset, Gradient *grad);

// C4DOpenVDBMesher Functions
Bool GetVDBPolygonized(BaseObject *inObject, Float iso, Float adapt, BaseObject *outObject);

// C4DOpenVDBCombine Functions
Bool CombineVDBs(C4DOpenVDBObject *obj,
                 maxon::BaseArray<BaseObject*> *inputs,
                 Float aMult,
                 Float bMult,
                 Int32 operation,
                 Int32 resample,
                 Int32 interpolation,
                 Bool deactivate,
                 Float deactivateVal,
                 Bool prune,
                 Float pruneVal,
                 Bool signedFloodFill);

// C4DOpenVDBFromPolygons Functions
Bool VDBFromPolygons(VDBObjectHelper *helper,
                     BaseObject *hClone,
                     Float voxelSize,
                     Int32 inBandWidth,
                     Int32 exBandWidth,
                     Bool UDF,
                     Bool fill);

// C4DOpenVDBSmooth Functions
Bool SmoothVDB(VDBObjectHelper *helper, C4DOpenVDBObject *obj, Int32 operation, Int32 filter, Int32 iter, Int32 renorm);

#endif /* C4DOpenVDBFunctions_h */

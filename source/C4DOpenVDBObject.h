//
//  C4DOpenVDBObject.h
//  C4DOpenVDB
//
//  Created by Cody Sorgenfrey on 7/20/18.
//  Copyright © 2018 MAXON Computer GmbH. All rights reserved.
//
//  Base class for the C4DOpenVDB Object.

#ifndef C4DOpenVDBObject_h
#define C4DOpenVDBObject_h

#include "C4DOpenVDBGenerator.h"

class VDBObjectHelper; // forward declare our helper since maxon stuff can't know about vdb stuff.

struct VoxelAttribs { Vector32 pos; Vector32 norm; Vector32 col; Float zdepth; };

class C4DOpenVDBObject : public C4DOpenVDBGenerator
{
    INSTANCEOF(C4DOpenVDBObject, C4DOpenVDBGenerator);
    
public:
    VDBObjectHelper                               *helper;
    GlVertexSubBufferData                         *subBuffer;
    maxon::BaseArray<GlVertexBufferVectorInfo>    *vectorInfoArray;
    maxon::BaseArray<GlVertexBufferAttributeInfo> *attribInfoArray;
    GlVertexBufferVectorInfo                      *vectorInfo[3];
    GlVertexBufferAttributeInfo                   *attribInfo[3];
    VoxelAttribs                                  *surfaceAttribs;
    Int32                                         voxelCnt, surfaceCnt, gridClass;
    Float                                         voxelSize;
    Bool                                          isSlaveOfMaster, UDF;
    String                                        gridName, gridType;
    GlString                                      voxelSizeUni, vpSizeUni, projMatUni, displayTypeUni, displayTypeFragUni, backFaceUni, userColorUni; //uniforms for opengl
    
    static NodeData* Alloc(void) { return NewObjClear(C4DOpenVDBObject); }
    virtual Bool Init(GeListNode* node);
    virtual void Free(GeListNode* node);
    virtual DRAWRESULT Draw(BaseObject *op, DRAWPASS drawpass, BaseDraw *bd, BaseDrawHelp *bh);
    virtual Bool GetDDescription(GeListNode* node, Description* description, DESCFLAGS_DESC& flags);
    void UpdateVoxelCountUI(BaseObject *op);
};

// helper functions

BaseObject* GetDummyPolygon(void);

#endif /* C4DOpenVDBObject_h */

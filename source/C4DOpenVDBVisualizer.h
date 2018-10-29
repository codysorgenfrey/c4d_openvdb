//
//  C4DOpenVDBVisualizer.h
//  C4DOpenVDB
//
//  Created by Cody Sorgenfrey on 9/4/18.
//  Copyright © 2018 MAXON Computer GmbH. All rights reserved.
//

#ifndef C4DOpenVDBVisualizer_h
#define C4DOpenVDBVisualizer_h

#include "C4DOpenVDBObject.h" // Include base C4DOpenVDBObject which we need to subclass

struct visualizerAttribs { Vector32 pos; Vector32 col; };

class C4DOpenVDBVisualizer : public C4DOpenVDBGenerator
{
    INSTANCEOF(C4DOpenVDBVisualizer, C4DOpenVDBGenerator);
    
public:
    visualizerAttribs                             *sliceAttribs;
    GlVertexSubBufferData                         *subBuffer;
    maxon::BaseArray<GlVertexBufferVectorInfo>    *vectorInfoArray;
    maxon::BaseArray<GlVertexBufferAttributeInfo> *attribInfoArray;
    GlVertexBufferVectorInfo                      *vectorInfo[3];
    GlVertexBufferAttributeInfo                   *attribInfo[3];
    Int32                                         surfaceCnt;
    Float                                         voxelSize;
    GlString                                      voxelSizeUni, vpSizeUni, projMatUni, userColorUni; //uniforms for opengl
    Vector                                        myMp, myRad;
    
    static NodeData* Alloc(void) { return NewObjClear(C4DOpenVDBVisualizer); }
    virtual Bool Init(GeListNode* node);
    virtual void Free(GeListNode* node);
    virtual Bool Message(GeListNode* node, Int32 type, void* t_data);
    virtual BaseObject* GetVirtualObjects(BaseObject* op, HierarchyHelp* hh);
    virtual void GetDimension(BaseObject *op, Vector *mp, Vector *rad);
    virtual DRAWRESULT 	Draw(BaseObject *op, DRAWPASS drawpass, BaseDraw *bd, BaseDrawHelp *bh);
};

#endif /* C4DOpenVDBVisualizer_h */

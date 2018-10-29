//
//  C4DOpenVDBToPolygons.hpp
//  C4DOpenVDB
//
//  Created by Cody Sorgenfrey on 9/23/18.
//  Copyright © 2018 MAXON Computer GmbH. All rights reserved.
//

#ifndef C4DOpenVDBToPolygons_h
#define C4DOpenVDBToPolygons_h

#include "C4DOpenVDBObject.h"

class C4DOpenVDBToPolygons : public C4DOpenVDBGenerator
{
    INSTANCEOF(C4DOpenVDBToPolygons, C4DOpenVDBGenerator);
public:
    Vector myMp, myRad;
    static NodeData* Alloc(void) { return NewObjClear(C4DOpenVDBToPolygons); }
    virtual Bool Init(GeListNode* node);
    virtual void Free(GeListNode* node);
    virtual BaseObject* GetVirtualObjects(BaseObject* op, HierarchyHelp* hh);
    virtual void GetDimension(BaseObject *op, Vector *mp, Vector *rad);
};

#endif /* C4DOpenVDBToPolygons_h */

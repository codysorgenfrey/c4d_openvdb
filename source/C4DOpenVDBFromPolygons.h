//
//  C4DOpenVDBFromPolygons.h
//  C4DOpenVDB
//
//  Created by Cody Sorgenfrey on 10/6/18.
//  Copyright © 2018 MAXON Computer GmbH. All rights reserved.
//

#ifndef C4DOpenVDBFromPolygons_h
#define C4DOpenVDBFromPolygons_h

#include "C4DOpenVDBObject.h"

class C4DOpenVDBFromPolygons : public C4DOpenVDBObject
{
    INSTANCEOF(C4DOpenVDBFromPolygons, C4DOpenVDBObject);
public:
    static NodeData* Alloc(void) { return NewObjClear(C4DOpenVDBFromPolygons); }
    virtual Bool Init(GeListNode* node);
    virtual void Free(GeListNode* node);
    virtual Bool GetDEnabling(GeListNode *node, const DescID &id, const GeData &t_data, DESCFLAGS_ENABLE flags, const BaseContainer *itemdesc);
    virtual BaseObject* GetVirtualObjects(BaseObject* op, HierarchyHelp* hh);
};


#endif /* C4DOpenVDBFromPolygons_h */

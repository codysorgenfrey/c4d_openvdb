//
//  C4DOpenVDBFromParticles.h
//  C4DOpenVDB
//
//  Created by Cody Sorgenfrey on 10/15/18.
//  Copyright © 2018 MAXON Computer GmbH. All rights reserved.
//

#ifndef C4DOpenVDBFromParticles_h
#define C4DOpenVDBFromParticles_h

#include "C4DOpenVDBObject.h"

class C4DOpenVDBFromParticles : public C4DOpenVDBObject
{
    INSTANCEOF(C4DOpenVDBFromParticles, C4DOpenVDBObject);
public:
    UInt32          inputDirtyTracker;
    TP_MasterSystem *sys;
    static NodeData* Alloc(void) { return NewObjClear(C4DOpenVDBFromParticles); }
    virtual Bool Init(GeListNode* node);
    virtual void Free(GeListNode* node);
    virtual Bool Message(GeListNode* node, Int32 type, void* t_data);
    virtual Bool GetDEnabling(GeListNode *node, const DescID &id, const GeData &t_data, DESCFLAGS_ENABLE flags, const BaseContainer *itemdesc);
    virtual BaseObject* GetVirtualObjects(BaseObject* op, HierarchyHelp* hh);
};

#endif /* C4DOpenVDBFromParticles_h */

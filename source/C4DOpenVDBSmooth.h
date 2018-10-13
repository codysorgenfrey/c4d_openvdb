//
//  C4DOpenVDBSmooth.h
//  C4DOpenVDB
//
//  Created by Cody Sorgenfrey on 10/10/18.
//  Copyright © 2018 MAXON Computer GmbH. All rights reserved.
//

#ifndef C4DOpenVDBSmooth_h
#define C4DOpenVDBSmooth_h

#include "C4DOpenVDBObject.h"

class C4DOpenVDBSmooth : public C4DOpenVDBObject
{
    INSTANCEOF(C4DOpenVDBSmooth, C4DOpenVDBObject);
public:
    static NodeData* Alloc(void) { return NewObjClear(C4DOpenVDBSmooth); }
    virtual Bool Init(GeListNode* node);
    virtual void Free(GeListNode* node);
    virtual Bool GetDEnabling(GeListNode *node, const DescID &id, const GeData &t_data, DESCFLAGS_ENABLE flags, const BaseContainer *itemdesc);
    virtual BaseObject* GetVirtualObjects(BaseObject* op, HierarchyHelp* hh);
};


#endif /* C4DOpenVDBSmooth_h */

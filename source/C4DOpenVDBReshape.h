//
//  C4DOpenVDBReshape.h
//  C4DOpenVDB
//
//  Created by Cody Sorgenfrey on 10/10/18.
//  Copyright © 2018 MAXON Computer GmbH. All rights reserved.
//

#ifndef C4DOpenVDBReshape_h
#define C4DOpenVDBReshape_h

#include "C4DOpenVDBObject.h"

class C4DOpenVDBReshape : public C4DOpenVDBObject
{
    INSTANCEOF(C4DOpenVDBReshape, C4DOpenVDBObject);
public:
    static NodeData* Alloc(void) { return NewObjClear(C4DOpenVDBReshape); }
    virtual Bool Init(GeListNode* node);
    virtual void Free(GeListNode* node);
    virtual BaseObject* GetVirtualObjects(BaseObject* op, HierarchyHelp* hh);
};


#endif /* C4DOpenVDBReshape_h */

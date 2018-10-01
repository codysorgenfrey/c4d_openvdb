//
//  C4DOpenVDBMesher.hpp
//  C4DOpenVDB
//
//  Created by Cody Sorgenfrey on 9/23/18.
//  Copyright © 2018 MAXON Computer GmbH. All rights reserved.
//

#ifndef C4DOpenVDBMesher_h
#define C4DOpenVDBMesher_h

#include "C4DOpenVDBObject.h"

class C4DOpenVDBMesher : public C4DOpenVDBGenerator
{
    INSTANCEOF(C4DOpenVDBMesher, C4DOpenVDBGenerator);
public:
    static NodeData* Alloc(void) { return NewObjClear(C4DOpenVDBMesher); }
    virtual Bool Init(GeListNode* node);
    virtual void Free(GeListNode* node);
    virtual BaseObject* GetVirtualObjects(BaseObject* op, HierarchyHelp* hh);
};

#endif /* C4DOpenVDBMesher_h */

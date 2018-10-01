//
//  C4DOpenVDBCombine.hpp
//  C4DOpenVDB
//
//  Created by Cody Sorgenfrey on 9/23/18.
//  Copyright © 2018 MAXON Computer GmbH. All rights reserved.
//

#ifndef C4DOpenVDBCombine_h
#define C4DOpenVDBCombine_h

#include "C4DOpenVDBObject.h"

class C4DOpenVDBCombine : public C4DOpenVDBObject
{
    INSTANCEOF(C4DOpenVDBCombine, C4DOpenVDBObject);
public:
    static NodeData* Alloc(void) { return NewObjClear(C4DOpenVDBCombine); }
    virtual Bool Init(GeListNode* node);
    virtual void Free(GeListNode* node);
    virtual BaseObject* GetVirtualObjects(BaseObject* op, HierarchyHelp* hh);
    virtual Bool GetDEnabling(GeListNode *node, const DescID &id, const GeData &t_data, DESCFLAGS_ENABLE flags, const BaseContainer *itemdesc);
};

#endif /* C4DOpenVDBCombine_h */

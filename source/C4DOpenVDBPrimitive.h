//
//  C4DOpenVDBPrimitive.h
//  C4DOpenVDB
//
//  Created by Cody Sorgenfrey on 8/13/18.
//  Copyright © 2018 MAXON Computer GmbH. All rights reserved.
//

#ifndef C4DOpenVDBPrimitive_h
#define C4DOpenVDBPrimitive_h

#include "C4DOpenVDBObject.h" // Include base C4DOpenVDB Object which we need to subclass

class C4DOpenVDBPrimitive : public C4DOpenVDBObject
{
    INSTANCEOF(C4DOpenVDBPrimitive, C4DOpenVDBObject);
    
public:
    static NodeData* Alloc(void) { return NewObjClear(C4DOpenVDBPrimitive); }
    virtual Bool Init(GeListNode* node);
    virtual void Free(GeListNode* node);
    virtual Bool GetDDescription(GeListNode* node, Description* description, DESCFLAGS_DESC& flags);
    virtual Bool GetDEnabling(GeListNode *node, const DescID &id, const GeData &t_data, DESCFLAGS_ENABLE flags, const BaseContainer *itemdesc);
    virtual BaseObject* GetVirtualObjects(BaseObject* op, HierarchyHelp* hh);
};

#endif /* C4DOpenVDBPrimitive_h */

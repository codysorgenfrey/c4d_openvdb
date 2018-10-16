//
//  C4DOpenVDBGenerator.cpp
//  C4DOpenVDB
//
//  Created by Cody Sorgenfrey on 9/14/18.
//  Copyright © 2018 MAXON Computer GmbH. All rights reserved.
//

#include "C4DOpenVDBGenerator.h"
#include "C4DOpenVDBObject.h"

//---------------------------------------------------------------------------------
//
// MARK: Public helper functions
//
//---------------------------------------------------------------------------------

Bool ValidVDBType(Int32 type)
{
    switch (type) {
        case Oc4dopenvdbprimitive:
        case Oc4dopenvdbcombine:
        case Oc4dopenvdbfrompolygons:
        case Oc4dopenvdbsmooth:
        case Oc4dopenvdbreshape:
            return true;
            
        default:
            return false;
    }
}

//---------------------------------------------------------------------------------
//
// MARK: C4DOpenVDBGenerator Class
//
//---------------------------------------------------------------------------------

Bool C4DOpenVDBGenerator::Init(GeListNode *node)
{
    slaveLinks = NewObj(maxon::BaseArray<BaseLink*>);
    return true;
}

void C4DOpenVDBGenerator::Free(GeListNode *node)
{
    FreeSlaves(node->GetDocument());
    DeleteObj(slaveLinks);
}

DRAWRESULT C4DOpenVDBGenerator::Draw(BaseObject *op, DRAWPASS drawpass, BaseDraw *bd, BaseDrawHelp *bh)
{
    Bool drawOK = true;
        
    //check if generator is enabled
    if (!op->GetDeformMode())
    {
        FreeSlaves(bh->GetDocument());
        EventAdd(EVENT_FORCEREDRAW);
        drawOK = false;
    }
    
    return (drawOK) ? SUPER::Draw(op, drawpass, bd, bh) : DRAWRESULT_SKIP;
}

Bool C4DOpenVDBGenerator::RecurseSetSlave(BaseObject *obj, Bool topLevelOnly)
{
    while (obj)
    {
        BaseLink *slaveLink = BaseLink::Alloc();
        slaveLink->SetLink(obj);
        if (!slaveLinks->Append(slaveLink))
            return false;
        
        C4DOpenVDBObject *vdb = obj->GetNodeData<C4DOpenVDBObject>();
        if (!vdb)
            return false;
        
        vdb->isSlaveOfMaster = true;
        
        if (!topLevelOnly)
        {
             BaseObject *down = obj->GetDown();
             if (down)
                 RecurseSetSlave(down, topLevelOnly);
        }
        obj = obj->GetNext();
    }
    return true;
}

Bool C4DOpenVDBGenerator::RegisterSlave(BaseObject *slave)
{
    FreeSlaves(slave->GetDocument());
    
    BaseLink *slaveLink = BaseLink::Alloc();
    slaveLink->SetLink(slave);
    if (!slaveLinks->Append(slaveLink))
        return false;
    
    C4DOpenVDBObject *vdb = slave->GetNodeData<C4DOpenVDBObject>();
    if (!vdb)
        return false;
    
    vdb->isSlaveOfMaster = true;
    
    return true;
}

Bool C4DOpenVDBGenerator::RegisterSlaves(BaseObject *first, Bool topLevelOnly)
{
    FreeSlaves(first->GetDocument());
    
    if (!RecurseSetSlave(first, topLevelOnly))
        return false;
    
    return true;
}

void C4DOpenVDBGenerator::FreeSlaves(BaseDocument *doc)
{
    for (Int32 x = 0; x < slaveLinks->GetCount(); x++)
    {
        BaseLink *slaveLink = slaveLinks->operator[](x);
        if (!slaveLink)
            continue;
        
        BaseList2D *bl = slaveLink->GetLink(doc);
        if (!bl)
            continue;
        
        BaseObject *obj = static_cast<BaseObject*>(bl);
        if (!obj)
            continue;
        
        C4DOpenVDBObject *vdb = obj->GetNodeData<C4DOpenVDBObject>();
        if (!vdb)
            continue;
        
        vdb->isSlaveOfMaster = false;
    }
    
    slaveLinks->Reset();
}

Bool C4DOpenVDBGenerator::RecurseCollectVDBs(BaseObject *obj, maxon::BaseArray<BaseObject*> *inputs, Bool topLevelOnly)
{
    BaseObject *down;
    
    while (obj)
    {
        if (!obj->GetDeformMode())
            goto skip;
        
        if (!ValidVDBType(obj->GetType()))
            goto skip;
        
        if (!inputs->Append(obj))
            return false;
        
        if (!topLevelOnly)
        {
            down = obj->GetDown();
            if (down)
                RecurseCollectVDBs(down, inputs, topLevelOnly);
        }
        
    skip:
        obj = obj->GetNext();
    }
    
    return true;
}

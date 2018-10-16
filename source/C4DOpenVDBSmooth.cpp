//
//  C4DOpenVDBSmooth.cpp
//  C4DOpenVDB
//
//  Created by Cody Sorgenfrey on 10/10/18.
//  Copyright © 2018 MAXON Computer GmbH. All rights reserved.
//

#include "C4DOpenVDBSmooth.h"
#include "C4DOpenVDBFunctions.h" // to get our vdb functions
#include "C4DOpenVDBHelp.h" // to launch help


//---------------------------------------------------------------------------------
//
// MARK: C4DOpenVDBSmooth Class
//
//---------------------------------------------------------------------------------

Bool C4DOpenVDBSmooth::Init(GeListNode* node)
{
    if (!SUPER::Init(node)) return false;
    
    node->SetParameter(DescLevel(C4DOPENVDB_SMOOTH_OP), GeData(C4DOPENVDB_SMOOTH_OP_MEAN), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_SMOOTH_FILTER), GeData(1), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_SMOOTH_ITER), GeData(1), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_SMOOTH_RENORM), GeData(C4DOPENVDB_SMOOTH_RENORM_FIRST), DESCFLAGS_SET_0);
    
    return true;
}

void C4DOpenVDBSmooth::Free(GeListNode *node)
{
    SUPER::Free(node);
}

Bool C4DOpenVDBSmooth::GetDEnabling(GeListNode *node, const DescID &id, const GeData &t_data, DESCFLAGS_ENABLE flags, const BaseContainer *itemdesc)
{
    if (id[0].id == C4DOPENVDB_SMOOTH_FILTER)
    {
        GeData myData;
        node->GetParameter(DescLevel(C4DOPENVDB_SMOOTH_OP), myData, DESCFLAGS_GET_0);
        Int32 vdbType = myData.GetInt32();
        if (vdbType == C4DOPENVDB_SMOOTH_OP_MEANFLOW || vdbType == C4DOPENVDB_SMOOTH_OP_LAP)
        {
            return false;
        }
    }
    return SUPER::GetDEnabling(node, id, t_data, flags, itemdesc);
}

BaseObject* C4DOpenVDBSmooth::GetVirtualObjects(BaseObject* op, HierarchyHelp* hh)
{
    if (!op->GetDeformMode())
        return nullptr;
    
    GeData           myData;
    Vector32         userColor;
    Bool             useColor, dirty;
    BaseObject       *outObject, *inObject, *hClone;
    Int32            filter, iter, operation, renorm;
    C4DOpenVDBObject *vdb;
    
    outObject = GetDummyPolygon(); // dummy output object
    userColor = Vector32(C4DOPENVDB_DEFAULT_COLOR);
    dirty = false;
    hClone = nullptr;
    
    inObject = op->GetDown();
    
    if (!inObject)
        goto error;
    
    hClone = op->GetAndCheckHierarchyClone(hh, inObject, HIERARCHYCLONEFLAGS_ASPOLY, &dirty, nullptr, false);
    
    if (!dirty) {
        blDelete(outObject);
        return hClone;
    }
    
    if (!ValidVDBType(inObject->GetType()))
        goto error;
    
    if (!RegisterSlave(inObject))
        goto error;
    
    vdb = inObject->GetNodeData<C4DOpenVDBObject>();
    if (!vdb)
        goto error;
    
    op->GetParameter(DescLevel(ID_BASEOBJECT_USECOLOR), myData, DESCFLAGS_GET_0);
    useColor = myData.GetBool();
    
    op->GetParameter(DescLevel(C4DOPENVDB_SMOOTH_OP), myData, DESCFLAGS_GET_0);
    operation = myData.GetInt32();
    
    op->GetParameter(DescLevel(C4DOPENVDB_SMOOTH_ITER), myData, DESCFLAGS_GET_0);
    iter = myData.GetInt32();
    
    op->GetParameter(DescLevel(C4DOPENVDB_SMOOTH_FILTER), myData, DESCFLAGS_GET_0);
    filter = myData.GetInt32();
    
    op->GetParameter(DescLevel(C4DOPENVDB_SMOOTH_RENORM), myData, DESCFLAGS_GET_0);
    renorm = myData.GetInt32();
    
    if (useColor)
    {
        op->GetParameter(DescLevel(ID_BASEOBJECT_COLOR), myData, DESCFLAGS_GET_0);
        userColor = (Vector32)myData.GetVector();
    }
    
    StatusSetSpin();
    
    if (!FilterVDB(helper, vdb, operation, filter, iter, renorm))
        goto error;
    
    if (!UpdateSurface(this, op, userColor))
        goto error;
    
    StatusClear();
    
    return outObject;
    
error:
    blDelete(outObject);
    blDelete(hClone);
    FreeSlaves(hh->GetDocument());
    ClearSurface(this, false);
    StatusClear();
    return nullptr;
}

static Bool C4DOpenVDBSmoothObjectHelpDelegate(const String& opType, const String& baseType, const String& group, const String& property)
{
    // Make sure that your object type name is unique and only return true if this is really your object type name and you are able to present a decent help.
    if (opType.ToUpper() == "OC4DOPENVDBSMOOTH")
    {
        if (property != opType) HelpDialog::location = (opType.ToUpper() + ".html#" + property).GetCStringCopy();
        else if (group != opType) HelpDialog::location = (opType.ToUpper() + ".html#" + group).GetCStringCopy();
        else HelpDialog::location = (opType.ToUpper() + ".html").GetCStringCopy();
        CallCommand(Oc4dopenvdbhelp);
        return true;
    }
    // If this is not your object/plugin type return false.
    return false;
}


Bool RegisterC4DOpenVDBSmooth(void)
{
    if (!RegisterPluginHelpDelegate(Oc4dopenvdbsmooth, C4DOpenVDBSmoothObjectHelpDelegate)) return false;
    
    return RegisterObjectPlugin(Oc4dopenvdbsmooth,
                                GeLoadString(IDS_C4DOpenVDBSmooth),
                                OBJECT_GENERATOR | OBJECT_INPUT,
                                C4DOpenVDBSmooth::Alloc,
                                "Oc4dopenvdbsmooth",
                                AutoBitmap("C4DOpenVDB.tiff"),
                                0);
}

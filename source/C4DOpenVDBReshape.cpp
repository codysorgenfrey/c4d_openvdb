//
//  C4DOpenVDBReshape.cpp
//  C4DOpenVDB
//
//  Created by Cody Sorgenfrey on 10/10/18.
//  Copyright © 2018 MAXON Computer GmbH. All rights reserved.
//

#include "C4DOpenVDBReshape.h"
#include "C4DOpenVDBFunctions.h" // to get our vdb functions
#include "C4DOpenVDBHelp.h" // to launch help


//---------------------------------------------------------------------------------
//
// MARK: C4DOpenVDBReshape Class
//
//---------------------------------------------------------------------------------

Bool C4DOpenVDBReshape::Init(GeListNode* node)
{
    if (!SUPER::Init(node)) return false;
    
    node->SetParameter(DescLevel(C4DOPENVDB_RESHAPE_OP), GeData(C4DOPENVDB_RESHAPE_OP_DILATE), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_RESHAPE_OFFSET), GeData(1), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_RESHAPE_RENORM), GeData(C4DOPENVDB_RESHAPE_RENORM_FIRST), DESCFLAGS_SET_0);
    
    return true;
}

void C4DOpenVDBReshape::Free(GeListNode *node)
{
    SUPER::Free(node);
}

BaseObject* C4DOpenVDBReshape::GetVirtualObjects(BaseObject* op, HierarchyHelp* hh)
{
    if (!op->GetDeformMode())
        return nullptr;
    
    GeData           myData;
    Vector32         userColor;
    Bool             useColor, dirty;
    BaseObject       *outObject, *inObject, *hClone;
    Int32            offset, operation, renorm;
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
    
    op->GetParameter(DescLevel(C4DOPENVDB_RESHAPE_OP), myData, DESCFLAGS_GET_0);
    operation = myData.GetInt32();
    
    op->GetParameter(DescLevel(C4DOPENVDB_RESHAPE_OFFSET), myData, DESCFLAGS_GET_0);
    offset = myData.GetInt32();
    
    op->GetParameter(DescLevel(C4DOPENVDB_RESHAPE_RENORM), myData, DESCFLAGS_GET_0);
    renorm = myData.GetInt32();
    
    if (useColor)
    {
        op->GetParameter(DescLevel(ID_BASEOBJECT_COLOR), myData, DESCFLAGS_GET_0);
        userColor = (Vector32)myData.GetVector();
    }
    
    StatusSetSpin();
    
    if (!FilterVDB(helper, vdb, operation, offset, /*iter=*/1, renorm))
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

static Bool C4DOpenVDBReshapeObjectHelpDelegate(const String& opType, const String& baseType, const String& group, const String& property)
{
    // Make sure that your object type name is unique and only return true if this is really your object type name and you are able to present a decent help.
    if (opType.ToUpper() == "OC4DOPENVDBRESHAPE")
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


Bool RegisterC4DOpenVDBReshape(void)
{
    if (!RegisterPluginHelpDelegate(Oc4dopenvdbreshape, C4DOpenVDBReshapeObjectHelpDelegate)) return false;
    
    return RegisterObjectPlugin(Oc4dopenvdbreshape,
                                GeLoadString(IDS_C4DOpenVDBReshape),
                                OBJECT_GENERATOR | OBJECT_INPUT,
                                C4DOpenVDBReshape::Alloc,
                                "Oc4dopenvdbreshape",
                                AutoBitmap("C4DOpenVDB.tiff"),
                                0);
}

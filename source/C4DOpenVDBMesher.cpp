//
//  C4DOpenVDBMesher.cpp
//  C4DOpenVDB
//
//  Created by Cody Sorgenfrey on 9/23/18.
//  Copyright © 2018 MAXON Computer GmbH. All rights reserved.
//

#include "C4DOpenVDBMesher.h"
#include "C4DOpenVDBFunctions.h" // to get our vdb functions
#include "C4DOpenVDBHelp.h" // to launch help

//---------------------------------------------------------------------------------
//
// MARK: C4DOpenVDBMesher Class
//
//---------------------------------------------------------------------------------

Bool C4DOpenVDBMesher::Init(GeListNode* node)
{
    if (!SUPER::Init(node)) return false;
    return true;
}

void C4DOpenVDBMesher::Free(GeListNode *node)
{
    SUPER::Free(node);
}

BaseObject* C4DOpenVDBMesher::GetVirtualObjects(BaseObject* op, HierarchyHelp* hh)
{
    if (!op->GetDeformMode())
        return nullptr;
    
    GeData           mydata;
    BaseObject       *inObject, *outObject, *hClone;
    Bool             dirty;
    Float            iso, adapt;
    
    outObject = BaseObject::Alloc(Onull);
    dirty = false;
    hClone = nullptr;
    
    inObject = op->GetDown();
    
    if (!inObject)
        goto error;
    
    if (!inObject->GetDeformMode())
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
    
    op->GetParameter(DescLevel(C4DOPENVDB_MESHER_ISO), mydata, DESCFLAGS_GET_0);
    iso = mydata.GetFloat();
    
    op->GetParameter(DescLevel(C4DOPENVDB_MESHER_ADAPT), mydata, DESCFLAGS_GET_0);
    adapt = mydata.GetFloat();
    
    StatusSetSpin();
    
    if (!GetVDBPolygonized(inObject, iso, adapt, outObject)) goto error;
    
    StatusClear();
    
    blDelete(hClone);
    
    return outObject;
    
error:
    blDelete(outObject);
    blDelete(hClone);
    FreeSlaves(hh->GetDocument());
    StatusClear();
    return nullptr;
}

static Bool C4DOpenVDBMesherObjectHelpDelegate(const String& opType, const String& baseType, const String& group, const String& property)
{
    // Make sure that your object type name is unique and only return true if this is really your object type name and you are able to present a decent help.
    if (opType.ToUpper() == "OC4DOPENVDBMESHER")
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


Bool RegisterC4DOpenVDBMesher(void)
{
    if (!RegisterPluginHelpDelegate(Oc4dopenvdbmesher, C4DOpenVDBMesherObjectHelpDelegate)) return false;
    
    return RegisterObjectPlugin(Oc4dopenvdbmesher,
                                GeLoadString(IDS_C4DOpenVDBMesher),
                                OBJECT_GENERATOR | OBJECT_INPUT,
                                C4DOpenVDBMesher::Alloc,
                                "Oc4dopenvdbmesher",
                                AutoBitmap("C4DOpenVDB.tiff"),
                                0);
}

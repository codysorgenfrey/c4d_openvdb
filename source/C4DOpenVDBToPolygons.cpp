//
//  C4DOpenVDBToPolygons.cpp
//  C4DOpenVDB
//
//  Created by Cody Sorgenfrey on 9/23/18.
//  Copyright © 2018 MAXON Computer GmbH. All rights reserved.
//

#include "C4DOpenVDBToPolygons.h"
#include "C4DOpenVDBFunctions.h" // to get our vdb functions
#include "C4DOpenVDBHelp.h" // to launch help

//---------------------------------------------------------------------------------
//
// MARK: C4DOpenVDBToPolygons Class
//
//---------------------------------------------------------------------------------

Bool C4DOpenVDBToPolygons::Init(GeListNode* node)
{
    if (!SUPER::Init(node)) return false;
    
    myMp = Vector(0);
    myRad = Vector(0);
    
    return true;
}

void C4DOpenVDBToPolygons::Free(GeListNode *node)
{
    SUPER::Free(node);
}

BaseObject* C4DOpenVDBToPolygons::GetVirtualObjects(BaseObject* op, HierarchyHelp* hh)
{
    if (!op->GetDeformMode())
        return nullptr;
    
    GeData           mydata;
    BaseObject       *inObject, *outObject, *hClone;
    Bool             dirty;
    Float            iso, adapt;
    C4DOpenVDBObject *vdb;
    
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
    
    vdb = inObject->GetNodeData<C4DOpenVDBObject>();
    if (!vdb)
        goto error;
    
    if (!RegisterSlave(inObject))
        goto error;
    
    op->GetParameter(DescLevel(C4DOPENVDB_TOPOLYGONS_ISO), mydata, DESCFLAGS_GET_0);
    iso = mydata.GetFloat();
    
    op->GetParameter(DescLevel(C4DOPENVDB_TOPOLYGONS_ADAPT), mydata, DESCFLAGS_GET_0);
    adapt = mydata.GetFloat();
    
    StatusSetSpin();
    
    if (!GetVDBPolygonized(inObject, iso, adapt, outObject))
        goto error;
    
    GetVDBBBox(vdb->helper, &myMp, &myRad);
    myMp = inObject->GetMl() * myMp;
    
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

void C4DOpenVDBToPolygons::GetDimension(BaseObject *op, Vector *mp, Vector *rad)
{
    *mp = myMp;
    *rad = myRad;
}

static Bool C4DOpenVDBToPolygonsObjectHelpDelegate(const String& opType, const String& baseType, const String& group, const String& property)
{
    // Make sure that your object type name is unique and only return true if this is really your object type name and you are able to present a decent help.
    if (opType.ToUpper() == "OC4DOPENVDBTOPOLYGONS")
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


Bool RegisterC4DOpenVDBToPolygons(void)
{
    if (!RegisterPluginHelpDelegate(Oc4dopenvdbtopolygons, C4DOpenVDBToPolygonsObjectHelpDelegate)) return false;
    
    return RegisterObjectPlugin(Oc4dopenvdbtopolygons,
                                GeLoadString(IDS_C4DOpenVDBToPolygons),
                                OBJECT_GENERATOR | OBJECT_INPUT,
                                C4DOpenVDBToPolygons::Alloc,
                                "Oc4dopenvdbtopolygons",
                                AutoBitmap("C4DOpenVDB.tiff"),
                                0);
}

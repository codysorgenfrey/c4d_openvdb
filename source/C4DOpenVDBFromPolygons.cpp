//
//  C4DOpenVDBFromPolygons.cpp
//  C4DOpenVDB
//
//  Created by Cody Sorgenfrey on 10/6/18.
//  Copyright © 2018 MAXON Computer GmbH. All rights reserved.
//

#include "C4DOpenVDBFromPolygons.h"
#include "C4DOpenVDBFunctions.h" // to get our vdb functions
#include "C4DOpenVDBHelp.h" // to launch help


//---------------------------------------------------------------------------------
//
// MARK: C4DOpenVDBFromPolygons Class
//
//---------------------------------------------------------------------------------

Bool C4DOpenVDBFromPolygons::Init(GeListNode* node)
{
    if (!SUPER::Init(node)) return false;
    
    node->SetParameter(DescLevel(C4DOPENVDB_FROMPOLYGONS_VOXEL_SIZE), GeData(C4DOPENVDB_DEFAULT_VOXEL_SIZE), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_FROMPOLYGONS_IN_BAND_RADIUS), GeData(C4DOPENVDB_DEFAULT_BAND_WIDTH), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_FROMPOLYGONS_EX_BAND_RADIUS), GeData(C4DOPENVDB_DEFAULT_BAND_WIDTH), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_FROMPOLYGONS_VDB_TYPE), GeData(C4DOPENVDB_FROMPOLYGONS_VDB_TYPE_SDF), DESCFLAGS_SET_0);
    
    return true;
}

void C4DOpenVDBFromPolygons::Free(GeListNode *node)
{
    SUPER::Free(node);
}

Bool C4DOpenVDBFromPolygons::GetDEnabling(GeListNode *node, const DescID &id, const GeData &t_data, DESCFLAGS_ENABLE flags, const BaseContainer *itemdesc)
{
    if (id[0].id == C4DOPENVDB_FROMPOLYGONS_UNSIGNED_DISTANCE_FIELD)
    {
        GeData myData;
        node->GetParameter(DescLevel(C4DOPENVDB_FROMPOLYGONS_VDB_TYPE), myData, DESCFLAGS_GET_0);
        Int32 vdbType = myData.GetInt32();
        if (vdbType == C4DOPENVDB_FROMPOLYGONS_VDB_TYPE_FOG)
        {
            return false;
        }
    }
    if (id[0].id == C4DOPENVDB_FROMPOLYGONS_VDB_TYPE)
    {
        GeData myData;
        node->GetParameter(DescLevel(C4DOPENVDB_FROMPOLYGONS_UNSIGNED_DISTANCE_FIELD), myData, DESCFLAGS_GET_0);
        Bool UDF = myData.GetBool();
        if (UDF)
        {
            return false;
        }
    }
    return SUPER::GetDEnabling(node, id, t_data, flags, itemdesc);
}

BaseObject* C4DOpenVDBFromPolygons::GetVirtualObjects(BaseObject* op, HierarchyHelp* hh)
{
    if (!op->GetDeformMode())
        return nullptr;
    
    GeData     myData;
    Int32      volumeType;
    Float      inBandWidth, exBandWidth, voxelSize;
    Vector     center;
    Vector32   userColor;
    Bool       fillInterior, unsignedDF, useColor, cache, dirty;
    BaseObject *outObject, *inObject, *hClone;
    
    // dummy output object
    outObject = GetDummyPolygon();
    
    cache = op->CheckCache(hh);
    dirty = op->IsDirty(DIRTYFLAGS_DATA);
    
    inBandWidth = C4DOPENVDB_DEFAULT_BAND_WIDTH;
    exBandWidth = C4DOPENVDB_DEFAULT_BAND_WIDTH;
    voxelSize = C4DOPENVDB_DEFAULT_VOXEL_SIZE;
    userColor = Vector32(C4DOPENVDB_DEFAULT_COLOR);
    
    inObject = op->GetDown();
    
    if (!inObject)
        goto error;
    
    hClone = op->GetAndCheckHierarchyClone(hh, inObject, HIERARCHYCLONEFLAGS_ASPOLY, &dirty, nullptr, false);
    
    if (!dirty) {
        blDelete(outObject);
        return hClone;
    }
    
    if (!helper)
        goto error;
    
    op->GetParameter(DescLevel(C4DOPENVDB_FROMPOLYGONS_VOXEL_SIZE), myData, DESCFLAGS_GET_0);
    voxelSize = myData.GetFloat();
    
    op->GetParameter(DescLevel(C4DOPENVDB_FROMPOLYGONS_IN_BAND_RADIUS), myData, DESCFLAGS_GET_0);
    inBandWidth = myData.GetFloat();
    
    op->GetParameter(DescLevel(C4DOPENVDB_FROMPOLYGONS_EX_BAND_RADIUS), myData, DESCFLAGS_GET_0);
    exBandWidth = myData.GetFloat();
    
    op->GetParameter(DescLevel(C4DOPENVDB_FROMPOLYGONS_VDB_TYPE), myData, DESCFLAGS_GET_0);
    volumeType = myData.GetInt32();
    
    op->GetParameter(DescLevel(C4DOPENVDB_FROMPOLYGONS_FILL_INTERIOR), myData, DESCFLAGS_GET_0);
    fillInterior = myData.GetBool();
    
    op->GetParameter(DescLevel(C4DOPENVDB_FROMPOLYGONS_UNSIGNED_DISTANCE_FIELD), myData, DESCFLAGS_GET_0);
    unsignedDF = myData.GetBool();
    
    op->GetParameter(DescLevel(ID_BASEOBJECT_USECOLOR), myData, DESCFLAGS_GET_0);
    useColor = myData.GetBool();
    
    if (useColor)
    {
        op->GetParameter(DescLevel(ID_BASEOBJECT_COLOR), myData, DESCFLAGS_GET_0);
        userColor = (Vector32)myData.GetVector();
    }
    
    if (fillInterior)
        inBandWidth = LIMIT<Float>::Max();
    
    StatusSetSpin();
    
    if (!VDBFromPolygons(helper, hClone, voxelSize, inBandWidth, exBandWidth, unsignedDF))
        goto error;
    
    if (volumeType == C4DOPENVDB_FROMPOLYGONS_VDB_TYPE_FOG)
        if (!SDFToFog(helper))
            goto error;
    
    if (!UpdateSurface(this, op, userColor))
        goto error;
    
    StatusClear();
    
    return outObject;
    
error:
    blDelete(outObject);
    ClearSurface(this, false);
    return nullptr;
}

static Bool C4DOpenVDBFromPolygonsObjectHelpDelegate(const String& opType, const String& baseType, const String& group, const String& property)
{
    // Make sure that your object type name is unique and only return true if this is really your object type name and you are able to present a decent help.
    if (opType.ToUpper() == "OC4DOPENVDBFROMPOLYGONS")
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


Bool RegisterC4DOpenVDBFromPolygons(void)
{
    if (!RegisterPluginHelpDelegate(Oc4dopenvdbfrompolygons, C4DOpenVDBFromPolygonsObjectHelpDelegate)) return false;
    
    return RegisterObjectPlugin(Oc4dopenvdbfrompolygons,
                                GeLoadString(IDS_C4DOpenVDBFromPolygons),
                                OBJECT_GENERATOR | OBJECT_INPUT,
                                C4DOpenVDBFromPolygons::Alloc,
                                "Oc4dopenvdbfrompolygons",
                                AutoBitmap("C4DOpenVDB.tiff"),
                                0);
}

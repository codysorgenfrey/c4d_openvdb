//
//  C4DOpenVDBPrimitive.cpp
//  C4DOpenVDB
//
//  Created by Cody Sorgenfrey on 8/13/18.
//  Copyright © 2018 MAXON Computer GmbH. All rights reserved.
//

#include "C4DOpenVDBPrimitive.h"
#include "C4DOpenVDBFunctions.h" // to get our vdb functions
#include "C4DOpenVDBHelp.h" // to launch help

//---------------------------------------------------------------------------------
//
// MARK: C4DOpenVDBPrimitive Class
//
//---------------------------------------------------------------------------------

Bool C4DOpenVDBPrimitive::Init(GeListNode* node)
{
    if (!SUPER::Init(node)) return false;
    
    node->SetParameter(DescLevel(C4DOPENVDB_PRIM_VOXEL_SIZE), GeData(C4DOPENVDB_DEFAULT_VOXEL_SIZE), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_PRIM_BAND_RADIUS), GeData(C4DOPENVDB_DEFAULT_BAND_WIDTH), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_PRIM_VDB_TYPE), GeData(C4DOPENVDB_PRIM_VDB_TYPE_SDF), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_PRIM_SETTINGS_TYPE), GeData(C4DOPENVDB_PRIM_SETTINGS_TYPE_SPHERE), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_SPHERE_SETTINGS_RADIUS), GeData(100.0), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_CUBE_SETTINGS_SIZE), GeData(200.0), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_DODEC_SETTINGS_SIZE), GeData(100.0), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_ICOS_SETTINGS_SIZE), GeData(100.0), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_OCT_SETTINGS_SIZE), GeData(100.0), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_TET_SETTINGS_SIZE), GeData(100.0), DESCFLAGS_SET_0);
    
    return true;
}

void C4DOpenVDBPrimitive::Free(GeListNode *node)
{
    SUPER::Free(node);
}

Bool C4DOpenVDBPrimitive::GetDDescription(GeListNode* node, Description* description, DESCFLAGS_DESC& flags)
{
    if (!node || !description)
        return false;
    
    // load the description of this type
    if (!description->LoadDescription(node->GetType()))
        return false;
    
    const DescID* singleid = description->GetSingleDescID();
    // parameter ID
    DescID cid = DescLevel(C4DOPENVDB_PRIM_SETTINGS_TYPE, DTYPE_LONG, 0);
    // check if this parameter ID is requested
    if (!singleid || cid.IsPartOf(*singleid, nullptr))
    {
        GeData data;
        node->GetParameter(DescID(C4DOPENVDB_PRIM_SETTINGS_TYPE), data, DESCFLAGS_GET_0);
        const Int32 selection = data.GetInt32();
        
        switch (selection) {
            case C4DOPENVDB_PRIM_SETTINGS_TYPE_SPHERE:
                cid = DescLevel(C4DOPENVDB_SPHERE_SETTINGS_GROUP, DTYPE_GROUP, 0);
                break;
            case C4DOPENVDB_PRIM_SETTINGS_TYPE_CUBE:
                cid = DescLevel(C4DOPENVDB_CUBE_SETTINGS_GROUP, DTYPE_GROUP, 0);
                break;
            case C4DOPENVDB_PRIM_SETTINGS_TYPE_DODEC:
                cid = DescLevel(C4DOPENVDB_DODEC_SETTINGS_GROUP, DTYPE_GROUP, 0);
                break;
            case C4DOPENVDB_PRIM_SETTINGS_TYPE_ICOS:
                cid = DescLevel(C4DOPENVDB_ICOS_SETTINGS_GROUP, DTYPE_GROUP, 0);
                break;
            case C4DOPENVDB_PRIM_SETTINGS_TYPE_OCT:
                cid = DescLevel(C4DOPENVDB_OCT_SETTINGS_GROUP, DTYPE_GROUP, 0);
                break;
            case C4DOPENVDB_PRIM_SETTINGS_TYPE_TET:
                cid = DescLevel(C4DOPENVDB_TET_SETTINGS_GROUP, DTYPE_GROUP, 0);
                break;
            default:
                break;
        }
        
        BaseContainer* settings = description->GetParameterI(cid, nullptr);
        if (settings) settings->SetInt32(DESC_HIDE, 0);
    }
    
    flags |= DESCFLAGS_DESC_LOADED;
    return SUPER::GetDDescription(node, description, flags);
}

Bool C4DOpenVDBPrimitive::GetDEnabling(GeListNode *node, const DescID &id, const GeData &t_data, DESCFLAGS_ENABLE flags, const BaseContainer *itemdesc)
{
    if (id[0].id == C4DOPENVDB_PRIM_UNSIGNED_DISTANCE_FIELD)
    {
        GeData myData;
        node->GetParameter(DescLevel(C4DOPENVDB_PRIM_VDB_TYPE), myData, DESCFLAGS_GET_0);
        Int32 vdbType = myData.GetInt32();
        if (vdbType == C4DOPENVDB_PRIM_VDB_TYPE_FOG)
        {
            return false;
        }
    }
    if (id[0].id == C4DOPENVDB_PRIM_VDB_TYPE)
    {
        GeData myData;
        node->GetParameter(DescLevel(C4DOPENVDB_PRIM_UNSIGNED_DISTANCE_FIELD), myData, DESCFLAGS_GET_0);
        Bool UDF = myData.GetBool();
        if (UDF)
        {
            return false;
        }
    }
    return SUPER::GetDEnabling(node, id, t_data, flags, itemdesc);
}

BaseObject* C4DOpenVDBPrimitive::GetVirtualObjects(BaseObject* op, HierarchyHelp* hh)
{
    if (!op->GetDeformMode())
        return nullptr;
    
    GeData     myData;
    Int32      primType, volumeType;
    Float      width = 100.0, bandWidth = 1.0, voxelSize = 4.0;
    Vector     center = Vector(0);
    Vector32   userColor;
    Bool       fillInterior, unsignedDF, useColor, cache, dirty;
    BaseObject *outObject;
    
    // dummy output object
    outObject = GetDummyPolygon();
    
    cache = op->CheckCache(hh);
    dirty = op->IsDirty(DIRTYFLAGS_DATA);
    
    userColor = Vector32(C4DOPENVDB_DEFAULT_COLOR);
        
    // if !dirty object is already cached and doesn't need to be rebuilt
    if (!dirty && !cache)
    {
        blDelete(outObject);
        return op->GetCache();
    }
        
    if (!helper)
        goto error;
    
    op->GetParameter(DescLevel(C4DOPENVDB_PRIM_SETTINGS_TYPE), myData, DESCFLAGS_GET_0);
    primType = myData.GetInt32();
    
    op->GetParameter(DescLevel(C4DOPENVDB_PRIM_VOXEL_SIZE), myData, DESCFLAGS_GET_0);
    voxelSize = myData.GetFloat();
    
    op->GetParameter(DescLevel(C4DOPENVDB_PRIM_BAND_RADIUS), myData, DESCFLAGS_GET_0);
    bandWidth = myData.GetFloat();
    
    op->GetParameter(DescLevel(C4DOPENVDB_PRIM_VDB_TYPE), myData, DESCFLAGS_GET_0);
    volumeType = myData.GetInt32();
    
    op->GetParameter(DescLevel(C4DOPENVDB_PRIM_FILL_INTERIOR), myData, DESCFLAGS_GET_0);
    fillInterior = myData.GetBool();
    
    op->GetParameter(DescLevel(C4DOPENVDB_PRIM_UNSIGNED_DISTANCE_FIELD), myData, DESCFLAGS_GET_0);
    unsignedDF = myData.GetBool();
    
    op->GetParameter(DescLevel(ID_BASEOBJECT_USECOLOR), myData, DESCFLAGS_GET_0);
    useColor = myData.GetBool();
    
    if (useColor)
    {
        op->GetParameter(DescLevel(ID_BASEOBJECT_COLOR), myData, DESCFLAGS_GET_0);
        userColor = (Vector32)myData.GetVector();
    }
    
    switch (primType) {
        case C4DOPENVDB_PRIM_SETTINGS_TYPE_SPHERE:
            op->GetParameter(DescLevel(C4DOPENVDB_SPHERE_SETTINGS_RADIUS), myData, DESCFLAGS_GET_0);
            break;
            
        case C4DOPENVDB_PRIM_SETTINGS_TYPE_CUBE:
            op->GetParameter(DescLevel(C4DOPENVDB_CUBE_SETTINGS_SIZE), myData, DESCFLAGS_GET_0);
            break;
        
        case C4DOPENVDB_PRIM_SETTINGS_TYPE_DODEC:
            op->GetParameter(DescLevel(C4DOPENVDB_DODEC_SETTINGS_SIZE), myData, DESCFLAGS_GET_0);
            break;
        
        case C4DOPENVDB_PRIM_SETTINGS_TYPE_ICOS:
            op->GetParameter(DescLevel(C4DOPENVDB_ICOS_SETTINGS_SIZE), myData, DESCFLAGS_GET_0);
            break;
        
        case C4DOPENVDB_PRIM_SETTINGS_TYPE_OCT:
            op->GetParameter(DescLevel(C4DOPENVDB_OCT_SETTINGS_SIZE), myData, DESCFLAGS_GET_0);
            break;
        
        case C4DOPENVDB_PRIM_SETTINGS_TYPE_TET:
            op->GetParameter(DescLevel(C4DOPENVDB_TET_SETTINGS_SIZE), myData, DESCFLAGS_GET_0);
            break;
        
        default:
            goto error;
    }
    
    width = myData.GetFloat();
    
    StatusSetSpin();
    
    if (!MakeShape(helper, primType, width, center, voxelSize, bandWidth))
        goto error;
    
    if (fillInterior) if (!FillSDFInterior(helper))
        goto error;
    
    if (unsignedDF) if (!UnsignSDF(helper))
        goto error;
    
    if (volumeType == C4DOPENVDB_PRIM_VDB_TYPE_FOG)
        if (!SDFToFog(helper))
            goto error;
    
    if (!UpdateSurface(this, op, userColor))
        goto error;
    
    StatusClear();
    
    return outObject;
    
error:
    blDelete(outObject);
    return nullptr;
}

void C4DOpenVDBPrimitive::GetDimension(BaseObject *op, Vector *mp, Vector *rad)
{
    *mp = op->GetMg().off;
    
    GeData myData;
    Int32  primType;
    Float  width;
    op->GetParameter(DescLevel(C4DOPENVDB_PRIM_SETTINGS_TYPE), myData, DESCFLAGS_GET_0);
    primType = myData.GetInt32();
    
    switch (primType) {
        case C4DOPENVDB_PRIM_SETTINGS_TYPE_SPHERE:
            op->GetParameter(DescLevel(C4DOPENVDB_SPHERE_SETTINGS_RADIUS), myData, DESCFLAGS_GET_0);
            width = myData.GetFloat();
            break;
            
        case C4DOPENVDB_PRIM_SETTINGS_TYPE_CUBE:
            op->GetParameter(DescLevel(C4DOPENVDB_CUBE_SETTINGS_SIZE), myData, DESCFLAGS_GET_0);
            width = myData.GetFloat() * 0.5;
            break;
            
        case C4DOPENVDB_PRIM_SETTINGS_TYPE_DODEC:
            op->GetParameter(DescLevel(C4DOPENVDB_DODEC_SETTINGS_SIZE), myData, DESCFLAGS_GET_0);
            width = myData.GetFloat();
            break;
            
        case C4DOPENVDB_PRIM_SETTINGS_TYPE_ICOS:
            op->GetParameter(DescLevel(C4DOPENVDB_ICOS_SETTINGS_SIZE), myData, DESCFLAGS_GET_0);
            width = myData.GetFloat();
            break;
            
        case C4DOPENVDB_PRIM_SETTINGS_TYPE_OCT:
            op->GetParameter(DescLevel(C4DOPENVDB_OCT_SETTINGS_SIZE), myData, DESCFLAGS_GET_0);
            width = myData.GetFloat();
            break;
            
        case C4DOPENVDB_PRIM_SETTINGS_TYPE_TET:
            op->GetParameter(DescLevel(C4DOPENVDB_TET_SETTINGS_SIZE), myData, DESCFLAGS_GET_0);
            width = myData.GetFloat();
            break;
            
        default:
            width = 100.0;
            break;
    }
    
    *rad = Vector(width);
}

static Bool C4DOpenVDBPrimitiveObjectHelpDelegate(const String& opType, const String& baseType, const String& group, const String& property)
{
    // Make sure that your object type name is unique and only return true if this is really your object type name and you are able to present a decent help.
    if (opType.ToUpper() == "OC4DOPENVDBPRIMITIVE")
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

Bool RegisterC4DOpenVDBPrimitive(void)
{
    if (!RegisterPluginHelpDelegate(Oc4dopenvdbprimitive, C4DOpenVDBPrimitiveObjectHelpDelegate)) return false;

    return RegisterObjectPlugin(Oc4dopenvdbprimitive,
                                GeLoadString(IDS_C4DOpenVDBPrimitive),
                                OBJECT_GENERATOR,
                                C4DOpenVDBPrimitive::Alloc,
                                "Oc4dopenvdbprimitive",
                                AutoBitmap("C4DOpenVDB.tiff"),
                                0);
}

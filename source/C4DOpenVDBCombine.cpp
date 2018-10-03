//
//  C4DOpenVDBCombine.cpp
//  C4DOpenVDB
//
//  Created by Cody Sorgenfrey on 9/23/18.
//  Copyright © 2018 MAXON Computer GmbH. All rights reserved.
//

#include "C4DOpenVDBCombine.h"
#include "C4DOpenVDBFunctions.h" // to get our vdb functions
#include "C4DOpenVDBHelp.h" // to launch help

//---------------------------------------------------------------------------------
//
// MARK: C4DOpenVDBCombine Class
//
//---------------------------------------------------------------------------------

Bool C4DOpenVDBCombine::Init(GeListNode* node)
{
    if (!SUPER::Init(node)) return false;
    
    node->SetParameter(DescLevel(C4DOPENVDB_COMBINE_MULT_A), GeData(1.0), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_COMBINE_MULT_B), GeData(1.0), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_COMBINE_OP), GeData(C4DOPENVDB_COMBINE_OP_SDF_DIFFERENCE), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_COMBINE_RESAMPLE), GeData(C4DOPENVDB_COMBINE_RESAMPLE_B_A), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_COMBINE_INTERPOLATION), GeData(C4DOPENVDB_COMBINE_INTERPOLATION_LINEAR), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_COMBINE_DEACTIVATE), GeData(false), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_COMBINE_DEACTIVATE_VALUE), GeData(0.0), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_COMBINE_PRUNE), GeData(true), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_COMBINE_PRUNE_VALUE), GeData(0.0), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_COMBINE_SIGN_FLOOD_FILL), GeData(false), DESCFLAGS_SET_0);
    
    return true;
}

void C4DOpenVDBCombine::Free(GeListNode *node)
{
    SUPER::Free(node);
}

Bool C4DOpenVDBCombine::GetDEnabling(GeListNode *node, const DescID &id, const GeData &t_data, DESCFLAGS_ENABLE flags, const BaseContainer *itemdesc)
{
    if (id == C4DOPENVDB_COMBINE_PRUNE_VALUE)
    {
        GeData mydata;
        node->GetParameter(DescLevel(C4DOPENVDB_COMBINE_PRUNE), mydata, DESCFLAGS_GET_0);
        if (!mydata.GetBool())
            return false;
    }
    if (id == C4DOPENVDB_COMBINE_DEACTIVATE_VALUE)
    {
        GeData mydata;
        node->GetParameter(DescLevel(C4DOPENVDB_COMBINE_DEACTIVATE), mydata, DESCFLAGS_GET_0);
        if (!mydata.GetBool())
            return false;
    }
    if (id == C4DOPENVDB_COMBINE_INTERPOLATION)
    {
        GeData mydata;
        node->GetParameter(DescLevel(C4DOPENVDB_COMBINE_RESAMPLE), mydata, DESCFLAGS_GET_0);
        if (mydata.GetInt32() == C4DOPENVDB_COMBINE_RESAMPLE_NONE)
            return false;
    }
    return SUPER::GetDEnabling(node, id, t_data, flags, itemdesc);
}

BaseObject* C4DOpenVDBCombine::GetVirtualObjects(BaseObject* op, HierarchyHelp* hh)
{
    if (!op->GetDeformMode())
        return nullptr;
    
    BaseDocument                          *doc;
    GeData                                mydata;
    BaseObject                            *inObject, *outObject, *hClone;
    Bool                                  dirty, useColor, signedFloodFill, deactivate, prune;
    maxon::BaseArray<BaseObject*>         inputs;
    Int32                                 operation, resample, interpolation;
    Float                                 deactivateVal, pruneVal, aMult, bMult;
    Vector32                              userColor;
    
    dirty = false;
    outObject = GetDummyPolygon();
    hClone = nullptr;
    doc = hh->GetDocument();
    userColor = Vector32(C4DOPENVDB_DEFAULT_COLOR);
    
    inObject = op->GetDown();
    
    if (!inObject)
        goto error;
        
    hClone = op->GetAndCheckHierarchyClone(hh, inObject, HIERARCHYCLONEFLAGS_ASPOLY, &dirty, nullptr, true);
    
    if (!dirty) {
        blDelete(outObject);
        return hClone;
    }
    
    if (!RegisterSlaves(inObject, true))
        goto error;
    
    if (!RecurseCollectVDBs(inObject, &inputs, true))
        goto error;
    
    op->GetParameter(DescLevel(C4DOPENVDB_COMBINE_MULT_A), mydata, DESCFLAGS_GET_0);
    aMult = mydata.GetFloat();
    
    op->GetParameter(DescLevel(C4DOPENVDB_COMBINE_MULT_B), mydata, DESCFLAGS_GET_0);
    bMult = mydata.GetFloat();
    
    op->GetParameter(DescLevel(C4DOPENVDB_COMBINE_OP), mydata, DESCFLAGS_GET_0);
    operation = mydata.GetInt32();
    
    op->GetParameter(DescLevel(C4DOPENVDB_COMBINE_RESAMPLE), mydata, DESCFLAGS_GET_0);
    resample = mydata.GetInt32();
    
    op->GetParameter(DescLevel(C4DOPENVDB_COMBINE_INTERPOLATION), mydata, DESCFLAGS_GET_0);
    interpolation = mydata.GetInt32();
    
    op->GetParameter(DescLevel(C4DOPENVDB_COMBINE_DEACTIVATE), mydata, DESCFLAGS_GET_0);
    deactivate = mydata.GetBool();
    
    op->GetParameter(DescLevel(C4DOPENVDB_COMBINE_DEACTIVATE_VALUE), mydata, DESCFLAGS_GET_0);
    deactivateVal = mydata.GetFloat();

    op->GetParameter(DescLevel(C4DOPENVDB_COMBINE_PRUNE), mydata, DESCFLAGS_GET_0);
    prune = mydata.GetBool();

    op->GetParameter(DescLevel(C4DOPENVDB_COMBINE_PRUNE_VALUE), mydata, DESCFLAGS_GET_0);
    pruneVal = mydata.GetFloat();
    
    op->GetParameter(DescLevel(C4DOPENVDB_COMBINE_SIGN_FLOOD_FILL), mydata, DESCFLAGS_GET_0);
    signedFloodFill = mydata.GetBool();
    
    if (!CombineVDBs(this,
                     &inputs,
                     aMult,
                     bMult,
                     operation,
                     resample,
                     interpolation,
                     deactivate,
                     deactivateVal,
                     prune,
                     pruneVal,
                     signedFloodFill)
        )
        goto error;
    
    op->GetParameter(DescLevel(ID_BASEOBJECT_USECOLOR), mydata, DESCFLAGS_GET_0);
    useColor = mydata.GetBool();
    
    if (useColor)
    {
        op->GetParameter(DescLevel(ID_BASEOBJECT_COLOR), mydata, DESCFLAGS_GET_0);
        userColor = (Vector32)mydata.GetVector();
    }
    
    if (!UpdateSurface(this, op, userColor))
        goto error;
    
    blDelete(hClone);
    
    return outObject;
    
error:
    FreeSlaves(doc);
    blDelete(outObject);
    blDelete(hClone);
    return nullptr;
}

static Bool C4DOpenVDBCombineObjectHelpDelegate(const String& opType, const String& baseType, const String& group, const String& property)
{
    // Make sure that your object type name is unique and only return true if this is really your object type name and you are able to present a decent help.
    if (opType.ToUpper() == "OC4DOPENVDBCOMBINE")
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


Bool RegisterC4DOpenVDBCombine(void)
{
    if (!RegisterPluginHelpDelegate(Oc4dopenvdbcombine, C4DOpenVDBCombineObjectHelpDelegate)) return false;
    
    return RegisterObjectPlugin(Oc4dopenvdbcombine,
                                GeLoadString(IDS_C4DOpenVDBCombine),
                                OBJECT_GENERATOR | OBJECT_INPUT,
                                C4DOpenVDBCombine::Alloc,
                                "Oc4dopenvdbcombine",
                                AutoBitmap("C4DOpenVDB.tiff"),
                                0);
}

//
//  C4DOpenVDBFromParticles.cpp
//  C4DOpenVDB
//
//  Created by Cody Sorgenfrey on 10/15/18.
//  Copyright © 2018 MAXON Computer GmbH. All rights reserved.
//

#include "C4DOpenVDBFromParticles.h"
#include "C4DOpenVDBFunctions.h" // to get our vdb functions
#include "C4DOpenVDBHelp.h" // to launch help
#include "c4d_baseeffectordata.h" // for mograph

//---------------------------------------------------------------------------------
//
// MARK: Private Helper Functions
//
//---------------------------------------------------------------------------------

HIERARCHYCLONEFLAGS GetHierarchyCloneFlag(BaseObject *inObject);
TP_MasterSystem* GetTpMasterSystem(BaseDocument* doc);
Bool GetParticlesFromObject(BaseList2D *inList,
                         maxon::BaseArray<Vector> *pointList,
                         maxon::BaseArray<Float> *radList,
                         maxon::BaseArray<Vector> *velList,
                         Float radius);

HIERARCHYCLONEFLAGS GetHierarchyCloneFlag(BaseObject *inObject)
{
    HIERARCHYCLONEFLAGS type = HIERARCHYCLONEFLAGS_ASPOLY;
    switch (inObject->GetType()) {
        case Ospline:
        case Osplineprimitive:
        case Osplineprofile:
        case Osplineflower:
        case Osplineformula:
        case Osplinetext:
        case Osplinenside:
        case Ospline4side:
        case Osplinecircle:
        case Osplinearc:
        case Osplinecissoid:
        case Osplinecycloid:
        case Osplinehelix:
        case Osplinerectangle:
        case Osplinestar:
        case Osplinecogwheel:
        case Osplinecontour:
            type = HIERARCHYCLONEFLAGS_ASSPLINE;
            break;
            
        case Omograph_matrix:
            type = HIERARCHYCLONEFLAGS_ASIS;
            break;
    }
    
    return type;
}

TP_MasterSystem* GetTpMasterSystem(BaseDocument* doc)
{
    if (!doc)
        return nullptr;
    BaseSceneHook* hook = doc->FindSceneHook(ID_THINKINGPARTICLES);
    if (!hook || hook->GetType() != ID_THINKINGPARTICLES)
        return nullptr;
    return static_cast<TP_MasterSystem*>(hook);
}

Bool GetParticlesFromObject(BaseList2D *inList,
                         maxon::BaseArray<Vector> *pointList,
                         maxon::BaseArray<Float> *radList,
                         maxon::BaseArray<Vector> *velList,
                         Float radius)
{
    switch (inList->GetType())
    {
        case Omograph_matrix:
        {
            BaseObject *inObj = static_cast<BaseObject*>(inList);
            if (!inObj)
                return false;
            
            Matrix xform = inObj->GetMl();
            
            MoData *md;
            GetMoDataMessage mdm;
            mdm.modata = nullptr;
            mdm.index = 0; // Get first set of modata
            
            BaseTag *mct = inObj->GetTag(ID_MOBAKETAG); // check for cache
            BaseTag *mdt = inObj->GetTag(ID_MOTAGDATA); // get mograph tag
            if (!mct && !mdt) // we need at least one
                return false;
            
            if (mct) // check cache first
                mct->Message(MSG_GET_MODATA, &mdm);
            
            if (!mdm.modata) // if cache didn't work, or if there is no cache
                mdt->Message(MSG_GET_MODATA, &mdm);
            
            if (mdm.user_owned)
                md = mdm.Release();
            else
                md = mdm.modata;
            
            if (!md)
                return false;
            
            MDArray<Matrix> marr = md->GetMatrixArray(MODATA_MATRIX);
            MDArray<Int32> flags = md->GetLongArray(MODATA_FLAGS); // we have to check if the clones are enabled or not
            for (Int32 x = 0; x < md->GetCount(); x++)
            {
                if (flags[x] & MOGENFLAG_CLONE_ON && !(flags[x] & MOGENFLAG_DISABLE))
                {
                    pointList->Append(xform * marr[x].off);
                    radList->Append(radius);
                }
            }
            
            if (mdm.user_owned)
                MoData::Free(md);
            
            break;
        }
        case ID_TP_PGROUP:
        {
            TP_MasterSystem *sys = GetTpMasterSystem(inList->GetDocument());
            TP_PGroup *group = static_cast<TP_PGroup*>(inList);
            if (!group || !sys)
                return false;
            
            for (TP_ParticleNode *node = group->GetFirstNode(); node; node = node->GetNext())
            {
                pointList->Append( sys->Position( node->GetParticleID() ) );
                radList->Append( sys->Size( node->GetParticleID() ) * radius );
                velList->Append( sys->Velocity( node->GetParticleID() ) );
            }
            
            break;
        }
        default:
        {
            PointObject *pointObj = static_cast<PointObject*>(inList);
            if (!pointObj)
                return false;
            
            pointObj->Touch(); // mark that we're replacing this object
            
            Matrix xform = pointObj->GetMl();
            
            Vector *points = pointObj->GetPointW();
            for (Int32 x = 0; x < pointObj->GetPointCount(); x++)
            {
                pointList->Append(xform * points[x]);
                radList->Append(radius);
            }
            
            break;
        }
    }
    
    for (GeListNode *node = inList->GetDown(); node; node = node->GetNext())
    {
        BaseList2D *thisList = static_cast<BaseList2D*>(node);
        if (!thisList)
            return false;
        
        if (!GetParticlesFromObject(thisList, pointList, radList, velList, radius))
            return false;
    }
    
    return true;
}

//---------------------------------------------------------------------------------
//
// MARK: C4DOpenVDBFromParticles Class
//
//---------------------------------------------------------------------------------

Bool C4DOpenVDBFromParticles::Init(GeListNode* node)
{
    if (!SUPER::Init(node)) return false;
    
    node->SetParameter(DescLevel(C4DOPENVDB_FROMPARTICLES_VOXEL_SIZE), GeData(C4DOPENVDB_DEFAULT_VOXEL_SIZE), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_FROMPARTICLES_BAND_WIDTH), GeData(C4DOPENVDB_DEFAULT_BAND_WIDTH), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_FROMPARTICLES_POINT_RADIUS), GeData(25.0), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_FROMPARTICLES_PRUNE), GeData(true), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_FROMPARTICLES_SHAPE), GeData(C4DOPENVDB_FROMPARTICLES_SHAPE_SPHERE), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_FROMPARTICLES_VEL_MULT), GeData(1.0), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_FROMPARTICLES_VEL_SPACE), GeData(1.0), DESCFLAGS_SET_0);
    node->SetParameter(DescLevel(C4DOPENVDB_FROMPARTICLES_MIN_RADIUS), GeData(1.5), DESCFLAGS_SET_0);
    
    inputDirtyTracker = 0;
    
    return true;
}

void C4DOpenVDBFromParticles::Free(GeListNode *node)
{
    inputDirtyTracker = 0;
    
    SUPER::Free(node);
}

Bool C4DOpenVDBFromParticles::Message(GeListNode* node, Int32 type, void* t_data)
{
    switch (type) {
        case MSG_DESCRIPTION_CHECKDRAGANDDROP:
        {
            DescriptionCheckDragAndDrop *descdnd = static_cast<DescriptionCheckDragAndDrop*>(t_data);
            if (descdnd->element->GetType() == ID_TP_PGROUP)
                descdnd->result = true;
            else
                descdnd->result = false;
            break;
        }
            
        case MSG_MOGRAPH_REEVALUATE:
        {
            // This is where we are notified of changes on the mograph matrix object
            node->SetDirty(DIRTYFLAGS_DATA);
            node->Message(MSG_UPDATE);
            break;
        }
            
        default:
            break;
    }
    return SUPER::Message(node, type, t_data);
}

Bool C4DOpenVDBFromParticles::GetDEnabling(GeListNode *node, const DescID &id, const GeData &t_data, DESCFLAGS_ENABLE flags, const BaseContainer *itemdesc)
{
    if (id[0].id == C4DOPENVDB_FROMPARTICLES_VEL_MULT || id[0].id == C4DOPENVDB_FROMPARTICLES_VEL_SPACE)
    {
        GeData myData;
        node->GetParameter(DescLevel(C4DOPENVDB_FROMPARTICLES_SHAPE), myData, DESCFLAGS_GET_0);
        Int32 shape = myData.GetInt32();
        if (shape == C4DOPENVDB_FROMPARTICLES_SHAPE_SPHERE)
        {
            return false;
        }
    }
    return SUPER::GetDEnabling(node, id, t_data, flags, itemdesc);
}

BaseObject* C4DOpenVDBFromParticles::GetVirtualObjects(BaseObject* op, HierarchyHelp* hh)
{
    if (!op->GetDeformMode())
        return nullptr;
    
    GeData                   myData;
    Int32                    bandWidth, shape;
    UInt32                   inputDirty;
    Float                    voxelSize, pointRadius, velMult, velSpace, minRadius;
    Vector                   center;
    Vector32                 userColor;
    Bool                     prune, useColor, dirty;
    BaseList2D               *inList;
    BaseObject               *outObject, *inObject;
    maxon::BaseArray<Vector> pointList;
    maxon::BaseArray<Float>  radList;
    maxon::BaseArray<Vector> velList;
    HIERARCHYCLONEFLAGS      hFlags;
    
    outObject = GetDummyPolygon(); // dummy output object
    dirty = false;
    userColor = Vector32(C4DOPENVDB_DEFAULT_COLOR);
    hFlags = HIERARCHYCLONEFLAGS_0;
    
    op->GetParameter(DescLevel(C4DOPENVDB_FROMPARTICLES_TPGROUP), myData, DESCFLAGS_GET_0);
    inList = myData.GetLink(hh->GetDocument());
    
    if (!inList) // if there's no TP_PGroup in the link field get the child object
    {
        inObject = op->GetDown();
        if (!inObject)
            goto error;
        
        hFlags = GetHierarchyCloneFlag(inObject);
        if (hFlags != HIERARCHYCLONEFLAGS_ASIS)
        {
            inList = (BaseList2D *)op->GetAndCheckHierarchyClone(hh, inObject, hFlags, &dirty, nullptr, false);
        }
        else // we don't want to Touch() the input object
        {
            dirty = op->IsDirty(DIRTYFLAGS_DATA) | inObject->IsDirty(DIRTYFLAGS_ALL);
            inList = (BaseList2D *)inObject;
        }
    }
    else
    {
        if (!sys) // check to see if we've accessed the system before
        {
            sys = GetTpMasterSystem(hh->GetDocument());
            if (!sys)
                goto error;
        }
        inputDirty = sys->GetDirty();
        if (inputDirty != inputDirtyTracker)
        {
            dirty = true;
            inputDirtyTracker = inputDirty;
        }
        dirty |= op->IsDirty(DIRTYFLAGS_DATA);
    }
    
    if (!inList)
        goto error;
    
    if (!dirty)
        return op->GetCache();
    
    GePrint("dirty");
    
    op->GetParameter(DescLevel(C4DOPENVDB_FROMPARTICLES_VOXEL_SIZE), myData, DESCFLAGS_GET_0);
    voxelSize = myData.GetFloat();
    
    op->GetParameter(DescLevel(C4DOPENVDB_FROMPARTICLES_POINT_RADIUS), myData, DESCFLAGS_GET_0);
    pointRadius = myData.GetFloat();
    
    op->GetParameter(DescLevel(C4DOPENVDB_FROMPARTICLES_PRUNE), myData, DESCFLAGS_GET_0);
    prune = myData.GetBool();
    
    op->GetParameter(DescLevel(C4DOPENVDB_FROMPARTICLES_BAND_WIDTH), myData, DESCFLAGS_GET_0);
    bandWidth = myData.GetInt32();
    
    op->GetParameter(DescLevel(C4DOPENVDB_FROMPARTICLES_SHAPE), myData, DESCFLAGS_GET_0);
    shape = myData.GetInt32();
    
    op->GetParameter(DescLevel(C4DOPENVDB_FROMPARTICLES_VEL_MULT), myData, DESCFLAGS_GET_0);
    velMult = myData.GetFloat();
    
    op->GetParameter(DescLevel(C4DOPENVDB_FROMPARTICLES_VEL_SPACE), myData, DESCFLAGS_GET_0);
    velSpace = myData.GetFloat();
    
    op->GetParameter(DescLevel(C4DOPENVDB_FROMPARTICLES_MIN_RADIUS), myData, DESCFLAGS_GET_0);
    minRadius = myData.GetFloat();
    
    op->GetParameter(DescLevel(ID_BASEOBJECT_USECOLOR), myData, DESCFLAGS_GET_0);
    useColor = myData.GetBool();
    
    if (useColor)
    {
        op->GetParameter(DescLevel(ID_BASEOBJECT_COLOR), myData, DESCFLAGS_GET_0);
        userColor = (Vector32)myData.GetVector();
    }
    
    StatusSetSpin();
    
    if (!GetParticlesFromObject(inList, &pointList, &radList, &velList, pointRadius))
        goto error;
    
    if (!VDBFromParticles(helper, &pointList, &radList, &velList, voxelSize, bandWidth, prune, shape, velMult, velSpace, minRadius))
        goto error;
    
    if (!UpdateSurface(this, op, userColor))
        goto error;
    
    // empty out my arrays
    pointList.Reset();
    radList.Reset();
    velList.Reset();
    
    StatusClear();
    
    return outObject;
    
error:
    blDelete(outObject);
    pointList.Reset();
    radList.Reset();
    velList.Reset();
    ClearSurface(this, false);
    StatusClear();
    return nullptr;
}

static Bool C4DOpenVDBFromParticlesObjectHelpDelegate(const String& opType, const String& baseType, const String& group, const String& property)
{
    // Make sure that your object type name is unique and only return true if this is really your object type name and you are able to present a decent help.
    if (opType.ToUpper() == "OC4DOPENVDBFROMPARTICLES")
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


Bool RegisterC4DOpenVDBFromParticles(void)
{
    if (!RegisterPluginHelpDelegate(Oc4dopenvdbfromparticles, C4DOpenVDBFromParticlesObjectHelpDelegate)) return false;
    
    return RegisterObjectPlugin(Oc4dopenvdbfromparticles,
                                GeLoadString(IDS_C4DOpenVDBFromParticles),
                                OBJECT_GENERATOR | OBJECT_INPUT | OBJECT_PARTICLEMODIFIER,
                                C4DOpenVDBFromParticles::Alloc,
                                "Oc4dopenvdbfromparticles",
                                AutoBitmap("C4DOpenVDB.tiff"),
                                0);
}

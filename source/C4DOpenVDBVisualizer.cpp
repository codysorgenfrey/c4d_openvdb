//
//  C4DOpenVDBVisualizer.cpp
//  C4DOpenVDB
//
//  Created by Cody Sorgenfrey on 9/4/18.
//  Copyright © 2018 MAXON Computer GmbH. All rights reserved.
//

#include "C4DOpenVDBVisualizer.h"
#include "C4DOpenVDBFunctions.h" // to get our vdb functions
#include "C4DOpenVDBHelp.h" // to launch help

//---------------------------------------------------------------------------------
//
// MARK: Private helper functions
//
//---------------------------------------------------------------------------------

static Int32 GlErrorHandler(GlProgramType type, const char* pszError);
Bool DrawSlice(BaseObject *op, C4DOpenVDBVisualizer *vdb, BaseDraw *bd, BaseDrawHelp *bh);

Int32 GlErrorHandler(GlProgramType type, const char* pszError)
{
    switch (type) {
        case VertexProgram:
            GePrint(String("Vertex Program Error: ") + String(pszError));
            break;
        case GeometryProgram:
            GePrint(String("Geometry Program Error: ") + String(pszError));
            break;
        case FragmentProgram:
            GePrint(String("Fragment Program Error: ") + String(pszError));
            break;
        case CompiledProgram:
            GePrint(String("Compiled Program Error: ") + String(pszError));
            break;
        default:
            GePrint(String("GL Error: ") + String(pszError));
            break;
    }
    return 0;
}

Bool DrawSlice(BaseObject *op, C4DOpenVDBVisualizer *vdb, BaseDraw *bd, BaseDrawHelp *bh)
{
    GeData                                 myData;
    Bool                                   bEOGL, x, cl, ct, cr, cb;
    Int32                                  vectorCnt, drawportType;
    UInt32                                 myFlags;
    GlVertexBufferDrawSubbuffer            di(C4D_VERTEX_BUFFER_POINTS, vdb->surfaceCnt, 0);
    const GlVertexBufferVectorInfo* const* ppVectorInfo;
    GlProgramFactory*                      pFactory = nullptr;
    Matrix                                 camMat = bd->GetMg();
    Matrix4d32                             projMat = RMtoSM4(bd->GetViewMatrix(DRAW_GET_VIEWMATRIX_PROJECTION));
    BaseContainer                          displaySettings = bh->GetDisplay();
    BaseLink                               *slaveLink;
    BaseList2D                             *slave;
    BaseObject                             *slaveObj;
    
    bEOGL = false;
    
    drawportType = bd->GetDrawParam(BASEDRAW_DRAWPORTTYPE).GetInt32();
    
    op->GetParameter(DescLevel(CUSTOMDATATYPE_GRADIENT), myData, DESCFLAGS_GET_0);
    
    myFlags = DRAW_SUBBUFFER_FLAGS;
    
    bd->GetSafeFrame(&cl, &ct, &cr, &cb);
    
    bd->SetMatrix_Matrix(nullptr, Matrix());
    
    if (vdb->slaveLinks->GetCount() > 0)
    {
        slaveLink = vdb->slaveLinks->operator[](0);
        if (!slaveLink)
            return false;
        
        slave = slaveLink->GetLink(bh->GetDocument());
        if (!slave)
            return false;
        
        slaveObj = static_cast<BaseObject*>(slave);
        if (!slaveObj)
            return false;
        
        bd->SetMatrix_Matrix(op, slaveObj->GetMg());
    }
    
    if (drawportType == DRAWPORT_TYPE_OGL_HQ)
    {
        GlVertexBuffer* pBuffer = GlVertexBuffer::GetVertexBuffer(bd, op, 0, nullptr, 0, 0);
        if (!pBuffer) goto _no_eogl;
        if (pBuffer->IsDirty())
        {
            // let's allocate a buffer that holds our data
            pBuffer->FreeBuffer(bd, vdb->subBuffer);
            vdb->subBuffer = pBuffer->AllocSubBuffer(bd, VBArrayBuffer, vdb->surfaceCnt * (sizeof(Vector32) * 3), nullptr);
            if (!vdb->subBuffer) goto _no_eogl;
            // map the buffer so that we can store our data
            // note that this memory is located on the graphics card
            void* pData = pBuffer->MapBuffer(bd, vdb->subBuffer, VBWriteOnly);
            if (!pData)
            {
                pBuffer->UnmapBuffer(bd, vdb->subBuffer);
                goto _no_eogl;
            }
            
            Vector32* pvData = (Vector32*)pData;
            for (x = 0; x < vdb->surfaceCnt; x++)
            {
                *pvData++ = vdb->sliceAttribs[x].pos;
                *pvData++ = Vector32(0);
                *pvData++ = (Vector32)bd->ConvertColor((Vector)vdb->sliceAttribs[x].col);
            }
            pBuffer->UnmapBuffer(bd, vdb->subBuffer);
            pBuffer->SetDirty(false);
        }
        
        // we need a program to show the data
        pFactory = GlProgramFactory::GetFactory(bd, op, 0, nullptr, nullptr, 0, 0, 0, /*ulFlags=*/0, vdb->attribInfo, 3, vdb->vectorInfo, 3, nullptr);
        if (!pFactory) goto _no_eogl;
        pFactory->LockFactory();
        pFactory->BindToView(bd);
        if (!pFactory->IsProgram(CompiledProgram))
        {
            // just route the vertex information to the fragment program NOTE: order of stuff is very very important
            UInt64 ulParameters = GL_PROGRAM_PARAM_NORMALS | GL_PROGRAM_PARAM_COLOR | GL_PROGRAM_PARAM_EYEPOSITION | GL_PROGRAM_PARAM_WORLDCOORD;
            pFactory->AddParameters(ulParameters);
            pFactory->Init(0);
            pFactory->AddErrorHandler(GlErrorHandler);
            vdb->voxelSizeUni = pFactory->AddUniformParameter(VertexProgram, UniformFloat1, "voxelSize");
            vdb->vpSizeUni = pFactory->AddUniformParameter(VertexProgram, UniformInt2, "vpSize");
            vdb->projMatUni = pFactory->AddUniformParameter(VertexProgram, UniformFloatMatrix4, "projMat");
            vdb->userColorUni = pFactory->AddUniformParameter(FragmentProgram, UniformFloat3, "userColor");
            pFactory->HeaderFinished();
            pFactory->AddLine(VertexProgram, "oposition = (transmatrix * vec4(iposition.xyz, 1));");
            pFactory->AddLine(VertexProgram, "gl_PointSize = ("+vdb->vpSizeUni+".y * "+vdb->projMatUni+"[0][0] * "+vdb->voxelSizeUni+" / oposition.w) * 1.2;");
            pFactory->AddLine(VertexProgram, "ocolor = vec4(icolor, 1.0);");
            pFactory->AddLine(VertexProgram, "onormal = vec4(inormal.xyz, 1.0);");
            pFactory->AddLine(VertexProgram, "worldcoord.xyz = objectmatrix_r * iposition.xyz + objectmatrix_p.xyz;");
            pFactory->AddLine(FragmentProgram, "vec3 V = normalize(eyeposition - worldcoord.xyz);");
            pFactory->AddLine(FragmentProgram, "ocolor = icolor;");
            pFactory->CompilePrograms();
        }
        pFactory->BindPrograms();
        
        pFactory->SetParameterMatrixTransform(pFactory->GetParameterHandle(VertexProgram, "transmatrix"));
        pFactory->SetParameterVector(pFactory->GetParameterHandle(FragmentProgram, "eyeposition"), (Vector32)bd->GetMg().off);
        pFactory->SetParameterMatrix(pFactory->GetParameterHandle(FragmentProgram, "objectmatrix_p"), pFactory->GetParameterHandle(FragmentProgram, "objectmatrix_r"), -1, (Matrix32)bh->GetMg());
        
        //set uniforms
        pFactory->InitSetParameters();
        pFactory->SetParameterReal(pFactory->GetParameterHandle(VertexProgram, vdb->voxelSizeUni.GetCString()), (Float32)vdb->voxelSize);
        pFactory->SetParameterInt2(pFactory->GetParameterHandle(VertexProgram, vdb->vpSizeUni.GetCString()), (cr-cl), (cb-ct));
        pFactory->SetParameterMatrix(pFactory->GetParameterHandle(VertexProgram, vdb->projMatUni.GetCString()), projMat);
        pFactory->SetParameterVector(pFactory->GetParameterHandle(FragmentProgram, vdb->userColorUni.GetCString()), Vector32(1));
        
        // obtain the vector information from the factory so that we have the proper attribute locations
        pFactory->GetVectorInfo(vectorCnt, ppVectorInfo);
        myFlags |= DRAW_SUBBUFFER_FLAGS_SHADER_POINT_SIZE;
        pBuffer->DrawSubBuffer(bd, vdb->subBuffer, nullptr, 1, &di, vectorCnt, ppVectorInfo, myFlags);
        pFactory->UnbindPrograms();
        
        pFactory->BindToView(nullptr);
        pFactory->UnlockFactory();
        
        bEOGL = true;
    _no_eogl:;
    }
    if (!bEOGL)
    {
        for (x=0; x<vdb->surfaceCnt; x++)
        {
            Vector *polyPoints = NewMem(Vector, 4);
            Vector *polyCol = NewMem(Vector, 4);
            Vector *polyNor = NewMem(Vector, 4);
            
            polyPoints[0] = (Vector)vdb->sliceAttribs[x].pos + (camMat.v1 * -0.5 * vdb->voxelSize) + (camMat.v2 * -0.5 * vdb->voxelSize);
            polyCol[0] = bd->ConvertColor((Vector)vdb->sliceAttribs[x].col);
            
            polyPoints[1] = (Vector)vdb->sliceAttribs[x].pos + (camMat.v1 * 0.5 * vdb->voxelSize) + (camMat.v2 * -0.5 * vdb->voxelSize);
            polyCol[1] = bd->ConvertColor((Vector)vdb->sliceAttribs[x].col);
            
            polyPoints[2] = (Vector)vdb->sliceAttribs[x].pos + (camMat.v1 * 0.5 * vdb->voxelSize) + (camMat.v2 * 0.5 * vdb->voxelSize);
            polyCol[2] = bd->ConvertColor((Vector)vdb->sliceAttribs[x].col);
            
            polyPoints[3] = (Vector)vdb->sliceAttribs[x].pos + (camMat.v1 * -0.5 * vdb->voxelSize) + (camMat.v2 * 0.5 * vdb->voxelSize);
            polyCol[3] = bd->ConvertColor((Vector)vdb->sliceAttribs[x].col);
            
            polyNor[0] = polyNor[1] = polyNor[2] = polyNor[3] = Vector(0);
            
            bd->DrawPoly(polyPoints, polyCol, polyNor, 4, 0);
            
            DeleteMem(polyPoints);
            DeleteMem(polyCol);
            DeleteMem(polyNor);
        }
    }
    
    return true;
}

//---------------------------------------------------------------------------------
//
// MARK: C4DOpenVDBVisualizer Class
//
//---------------------------------------------------------------------------------

Bool C4DOpenVDBVisualizer::Init(GeListNode* node)
{
    if (!SUPER::Init(node)) return false;
    
    subBuffer = nullptr;
    sliceAttribs = nullptr;
    surfaceCnt = 0;
    voxelSize = 0.0;
    myMp = Vector(0);
    myRad = Vector(0);
    
    GeData mydata(CUSTOMDATATYPE_GRADIENT, DEFAULTVALUE);
    Gradient *grad = (Gradient*)mydata.GetCustomDataType(CUSTOMDATATYPE_GRADIENT);
    GradientKnot knot1, knot2, knot3;
    knot1.col = Vector(1, 0.95, 0.9);
    knot1.pos = 0;
    knot2.col = Vector(0);
    knot2.pos = 0.5;
    knot3.col = Vector(0.9, 0.95, 1);
    knot3.pos = 1;
    grad->InsertKnot(knot1);
    grad->InsertKnot(knot2);
    grad->InsertKnot(knot3);
    node->SetParameter(DescLevel(C4DOPENVDB_VIS_COLOR), mydata, DESCFLAGS_SET_0);
    
    vectorInfoArray = NewObj(maxon::BaseArray<GlVertexBufferVectorInfo>);
    attribInfoArray = NewObj(maxon::BaseArray<GlVertexBufferAttributeInfo>);
    
    maxon::BaseArray<GlVertexBufferVectorInfo>    &vertex = *vectorInfoArray;
    maxon::BaseArray<GlVertexBufferAttributeInfo> &attrib = *attribInfoArray;
    
    if (!vectorInfoArray || !attribInfoArray || !vectorInfoArray->Resize(3) || !attribInfoArray->Resize(3))
    {
        GePrint("Error allocating GLSL arrays.");
        return false;
    }
    
    for (Int32 a = 0; a < 3; a++)
    {
        vectorInfo[a] = &(vertex[a]);
        attribInfo[a] = &(attrib[a]);
    }
    
    Int lStride = 3 * sizeof(Vector32);
    vertex[0].lDataType = C4D_GL_DATATYPE_FLOAT;
    vertex[0].lCount	= 3;
    vertex[0].strType = "vec3";
    vertex[0].strName = "attrib0";
    vertex[0].lOffset = 0;
    vertex[0].lStride = lStride;
    vertex[0].nAttribLocation = C4D_VERTEX_BUFFER_VERTEX;
    
    vertex[1].lDataType = C4D_GL_DATATYPE_FLOAT;
    vertex[1].lCount	= 3;
    vertex[1].strType = "vec3";
    vertex[1].strName = "attrib1";
    vertex[1].lOffset = sizeof(Vector32);
    vertex[1].lStride = lStride;
    vertex[1].nAttribLocation = C4D_VERTEX_BUFFER_NORMAL;
    
    vertex[2].lDataType = C4D_GL_DATATYPE_FLOAT;
    vertex[2].lCount	= 3;
    vertex[2].strType = "vec3";
    vertex[2].strName = "attrib2";
    vertex[2].lOffset = 2 * sizeof(Vector32);
    vertex[2].lStride = lStride;
    vertex[2].nAttribLocation = C4D_VERTEX_BUFFER_COLOR;
    
    attrib[0].lVector[0] = attrib[0].lVector[1] = attrib[0].lVector[2] = 0;
    attrib[0].lIndex[0]	 = 0; attrib[0].lIndex[1] = 1; attrib[0].lIndex[2] = 2;
    attrib[0].strDeclaration = "vec3 iposition = attrib0.xyz";
    attrib[0].strName = "iposition";
    
    attrib[1].lVector[0] = attrib[1].lVector[1] = attrib[1].lVector[2] = 0;
    attrib[1].lIndex[0]	 = 0; attrib[1].lIndex[1] = 1; attrib[1].lIndex[2] = 2;
    attrib[1].strDeclaration = "vec3 inormal = attrib1.xyz";
    attrib[1].strName = "inormal";
    
    attrib[2].lVector[0] = attrib[2].lVector[1] = attrib[2].lVector[2] = 0;
    attrib[2].lIndex[0]	 = 0; attrib[2].lIndex[1] = 1; attrib[2].lIndex[2] = 2;
    attrib[2].strDeclaration = "vec3 icolor = attrib2.xyz";
    attrib[2].strName = "icolor";
    
    return true;
}

void C4DOpenVDBVisualizer::Free(GeListNode *node)
{
    GlVertexBuffer::RemoveObject((C4DAtom*)node, 0);
    GlProgramFactory::RemoveReference((C4DAtom*)node);
    DeleteMem(sliceAttribs);
    DeleteObj(vectorInfoArray);
    DeleteObj(attribInfoArray);
    
    SUPER::Free(node);
}

Bool C4DOpenVDBVisualizer::Message(GeListNode* node, Int32 type, void* t_data)
{
    switch (type) {
        case MSG_DESCRIPTION_CHECKDRAGANDDROP:
        {
            DescriptionCheckDragAndDrop *descdnd = static_cast<DescriptionCheckDragAndDrop*>(t_data);
            if (ValidVDBType(descdnd->element->GetType()))
                descdnd->result = true;
            else
                descdnd->result = false;
            break;
        }
            
        default:
            break;
    }
    return SUPER::Message(node, type, t_data);
}

BaseObject* C4DOpenVDBVisualizer::GetVirtualObjects(BaseObject* op, HierarchyHelp* hh)
{
    if (!op->GetDeformMode())
        return nullptr;
    
    GeData           mydata;
    BaseList2D       *inList;
    BaseObject       *inObject, *outObject, *hClone;
    Bool             dirty;
    C4DOpenVDBObject *vdb;
    Int32            axis;
    Float            offset;
    Gradient         *grad;
    InitRenderStruct irs;
    
    outObject = GetDummyPolygon(); // dummy output object
    dirty = false;
    hClone = nullptr;
    
    op->GetParameter(DescLevel(C4DOPENVDB_VIS_OBJECT), mydata, DESCFLAGS_GET_0);
    inList = mydata.GetLink(hh->GetDocument());
    
    if (inList)
        inObject = static_cast<BaseObject*>(inList);
    else
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
    
    vdb = inObject->GetNodeData<C4DOpenVDBObject>();
    if (!vdb)
        goto error;
    
    op->GetParameter(DescLevel(PRIM_AXIS), mydata, DESCFLAGS_GET_0);
    axis = mydata.GetInt32();
    
    op->GetParameter(DescLevel(C4DOPENVDB_VIS_OFFSET), mydata, DESCFLAGS_GET_0);
    offset = mydata.GetFloat();
    
    op->GetParameter(DescLevel(C4DOPENVDB_VIS_COLOR), mydata, DESCFLAGS_GET_0);
    grad = (Gradient*)mydata.GetCustomDataType(CUSTOMDATATYPE_GRADIENT);
    
    StatusSetSpin();
    
    if (!grad->InitRender(irs))
        goto error;
    
    if (!UpdateVisualizerSlice(vdb, this, axis, offset, grad))
        goto error;
    
    GetVDBBBox(vdb->helper, &myMp, &myRad);
    myMp = inObject->GetMl() * (myMp);
    
    grad->FreeRender();
    
    StatusClear();
    
    blDelete(hClone);
    
    return outObject;
    
error:
    blDelete(outObject);
    blDelete(hClone);
    FreeSlaves(hh->GetDocument());
    surfaceCnt = 0;
    voxelSize = 0.0;
    DeleteMem(sliceAttribs);
    return nullptr;
}

void C4DOpenVDBVisualizer::GetDimension(BaseObject *op, Vector *mp, Vector *rad)
{
    *mp = myMp;
    *rad = myRad;
}

DRAWRESULT C4DOpenVDBVisualizer::Draw(BaseObject *op, DRAWPASS drawpass, BaseDraw *bd, BaseDrawHelp *bh)
{
    BaseDocument    *doc = bh->GetDocument();
    const LayerData *ld  = op->GetLayerData(doc);
    
    if (drawpass != DRAWPASS_OBJECT)
        return DRAWRESULT_SKIP;
    
    if (bd->TestBreak())
        return DRAWRESULT_SKIP;
    
    // check if layer has view or generators disabled
    if (ld && ((!ld->view) || (!ld->generators)))
        return DRAWRESULT_SKIP;
    
    // check if this is the active basedraw
    //BaseDraw*  const activeBD = doc->GetActiveBaseDraw();
    //const Bool       isActiveBD = bd && (bd == activeBD);
    //if (!isActiveBD) return DRAWRESULT_SKIP;
    
    // check display filter for "Generator" objects
    if (!(bd->GetDisplayFilter() & DISPLAYFILTER_GENERATOR))
        return DRAWRESULT_SKIP;
    
    //check if generator is enabled
    if (!op->GetDeformMode())
        return SUPER::Draw(op, drawpass, bd, bh);
    
    return (DrawSlice(op, this, bd, bh)) ? SUPER::Draw(op, drawpass, bd, bh) : DRAWRESULT_SKIP;
}

static Bool C4DOpenVDBVisualizerObjectHelpDelegate(const String& opType, const String& baseType, const String& group, const String& property)
{
    // Make sure that your object type name is unique and only return true if this is really your object type name and you are able to present a decent help.
    if (opType.ToUpper() == "OC4DOPENVDBVISUALIZER")
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


Bool RegisterC4DOpenVDBVisualizer(void)
{
    if (!RegisterPluginHelpDelegate(Oc4dopenvdbvisualizer, C4DOpenVDBVisualizerObjectHelpDelegate)) return false;
    
    return RegisterObjectPlugin(Oc4dopenvdbvisualizer,
                                GeLoadString(IDS_C4DOpenVDBVisualizer),
                                OBJECT_GENERATOR | OBJECT_INPUT,
                                C4DOpenVDBVisualizer::Alloc,
                                "Oc4dopenvdbvisualizer",
                                AutoBitmap("C4DOpenVDB.tiff"),
                                0);
}

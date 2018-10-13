#include "C4DOpenVDBObject.h"
#include "C4DOpenVDBFunctions.h" // To get our VDBObjectHelper object
#include <algorithm> // for std::sort

//---------------------------------------------------------------------------------
//
// MARK: Private helper functions
//
//---------------------------------------------------------------------------------

Int32 GlErrorHandler(GlProgramType type, const char* pszError);
Bool sortDepth(const VoxelAttribs a, const VoxelAttribs b);
String FormatWithCommas(Int num);
Bool DrawVDB(BaseObject *op, C4DOpenVDBObject *vdb, BaseDraw *bd, BaseDrawHelp *bh);

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

Bool sortDepth(const VoxelAttribs a, const VoxelAttribs b)
{
    return a.zdepth > b.zdepth; //draw from back to front
}

String FormatWithCommas(Int num)
{
    String fout;
    String in = String::IntToString(num);
    while (in.GetLength() != 0) {
        if (in.GetLength() > 3){
            fout = "," + in.Right(3) + fout;
            in.Delete(in.GetLength()-3, 3);
        } else {
            fout = in + fout;
            in = "";
        }
    }
    return fout;
}

Bool DrawVDB(BaseObject *op, C4DOpenVDBObject *vdb, BaseDraw *bd, BaseDrawHelp *bh)
{
    GeData                                 myData;
    Bool                                   bEOGL, x, cl, ct, cr, cb, useColor, transperant, rebuildBuffer;
    Int32                                  displayType, vectorCnt, lightCnt, drawportType;
    UInt32                                 myFlags;
    GlVertexBufferDrawSubbuffer            di(C4D_VERTEX_BUFFER_POINTS, vdb->surfaceCnt, 0);
    const GlVertexBufferVectorInfo* const* ppVectorInfo;
    const GlLight**                        ppLights = nullptr;
    GlProgramFactory*                      pFactory = nullptr;
    Matrix                                 camMat = bd->GetMg();
    Matrix4d32                             projMat = RMtoSM4(bd->GetViewMatrix(DRAW_GET_VIEWMATRIX_PROJECTION));
    Matrix4d32                             MVP = RMtoSM4(bd->GetViewMatrix(DRAW_GET_VIEWMATRIX_MODELVIEW_PROJECTION));
    BaseContainer                          displaySettings = bh->GetDisplay();
    Vector                                 userColor;
    
    op->GetParameter(DescLevel(C4DOPENVDB_DISPLAY_SHAPE), myData, DESCFLAGS_GET_0);
    displayType = myData.GetInt32();
    
    op->GetParameter(DescLevel(ID_BASEOBJECT_USECOLOR), myData, DESCFLAGS_GET_0);
    useColor = myData.GetBool();
    
    if (useColor){
        op->GetParameter(DescLevel(ID_BASEOBJECT_COLOR), myData, DESCFLAGS_GET_0);
        userColor = bd->ConvertColor(myData.GetVector());
    } else {
        userColor = bd->ConvertColor(Vector(C4DOPENVDB_DEFAULT_COLOR));
    }
    
    transperant = (vdb->UDF || vdb->gridClass == C4DOPENVDB_GRID_CLASS_FOG) && displayType == C4DOPENVDB_DISPLAY_SHAPE_SQUARE;
    
    bEOGL = false;
    
    lightCnt = 0;
    
    drawportType = bd->GetDrawParam(BASEDRAW_DRAWPORTTYPE).GetInt32();
    
    myFlags = DRAW_SUBBUFFER_FLAGS;
    
    bd->GetSafeFrame(&cl, &ct, &cr, &cb);
    
    bd->SetLightList(BDRAW_SETLIGHTLIST_SCENELIGHTS); //Has to come before set matrix for some reason.
    
    bd->SetMatrix_Matrix(op, bh->GetMg());
    
    if (drawportType == DRAWPORT_TYPE_OGL_HQ && displayType != C4DOPENVDB_DISPLAY_SHAPE_BOX && displayType != C4DOPENVDB_DISPLAY_SHAPE_BOX_LINES)
    {
        GlVertexBuffer* pBuffer = GlVertexBuffer::GetVertexBuffer(bd, op, 0, nullptr, 0, 0);
        if (!pBuffer)
            goto _no_eogl;
        
        rebuildBuffer = pBuffer->IsDirty();
        
        if (transperant && !vdb->prevFacingVector.IsZero())
        {
            // if our camera has rotated more that 90 degrees we need to resort our depth
            if (Dot((camMat.off - op->GetAbsPos()).GetNormalized(), vdb->prevFacingVector.GetNormalized()) <= 0.5)
                rebuildBuffer = true;
        }
        
        if (rebuildBuffer)
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
            
            if (transperant)
            {
                vdb->prevFacingVector = camMat.off - op->GetAbsPos();
                for (x = 0; x < vdb->surfaceCnt; x++)
                {
                    //z component of the position in camera space
                    vdb->surfaceAttribs[x].zdepth = (MVP * vdb->surfaceAttribs[x].pos).z;
                }
                std::sort(vdb->surfaceAttribs, vdb->surfaceAttribs + vdb->surfaceCnt, sortDepth);
            }
            
            Vector32* pvData = (Vector32*)pData;
            for (x = 0; x < vdb->surfaceCnt; x++)
            {
                *pvData++ = vdb->surfaceAttribs[x].pos;
                *pvData++ = vdb->surfaceAttribs[x].norm;
                *pvData++ = (Vector32)bd->ConvertColor((Vector)vdb->surfaceAttribs[x].col);
            }
            pBuffer->UnmapBuffer(bd, vdb->subBuffer);
            pBuffer->SetDirty(false);
        }
        
        lightCnt = bd->GetGlLightCount();
        if (lightCnt > 0)
        {
            ppLights = NewMemClear(const GlLight *, lightCnt);
            for (x = 0; x < lightCnt; x++)
                ppLights[x] = bd->GetGlLight(x);
        }
        else
        {
            ppLights = 0;
        }
        
        // we need a program to show the data
        pFactory = GlProgramFactory::GetFactory(bd, op, 0, nullptr, nullptr, 0, ppLights, lightCnt, /*ulFlags=*/0, vdb->attribInfo, 3, vdb->vectorInfo, 3, nullptr);
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
            vdb->displayTypeUni = pFactory->AddUniformParameter(VertexProgram, UniformInt1, "displayType");
            vdb->displayTypeFragUni = pFactory->AddUniformParameter(FragmentProgram, UniformInt1, "displayType");
            vdb->backFaceUni = pFactory->AddUniformParameter(FragmentProgram, UniformInt1, "backFaceCulling");
            vdb->userColorUni = pFactory->AddUniformParameter(FragmentProgram, UniformFloat3, "userColor");
            pFactory->HeaderFinished();
            pFactory->AddLine(VertexProgram, "oposition = (transmatrix * vec4(iposition.xyz, 1));");
            pFactory->AddLine(VertexProgram, "if ("+vdb->displayTypeUni+" == "+GlString(C4DOPENVDB_DISPLAY_SHAPE_SQUARE)+"){");
            pFactory->AddLine(VertexProgram, "  gl_PointSize = ("+vdb->vpSizeUni+".y * "+vdb->projMatUni+"[0][0] * "+vdb->voxelSizeUni+" / oposition.w) * 1.2;");
            pFactory->AddLine(VertexProgram, "}");
            pFactory->AddLine(VertexProgram, "ocolor = vec4(icolor, 1.0);");
            pFactory->AddLine(VertexProgram, "onormal = vec4((objectmatrix_n * inormal.xyz), 1.0);");
            pFactory->AddLine(VertexProgram, "worldcoord = objectmatrix_r * iposition.xyz + objectmatrix_p.xyz;");
            pFactory->AddLine(FragmentProgram, "vec3 V = normalize(eyeposition - worldcoord.xyz);");
            if (lightCnt)
            {
                pFactory->AddLine(FragmentProgram, "if ("+vdb->displayTypeFragUni+" == "+GlString(C4DOPENVDB_DISPLAY_SHAPE_SQUARE)+" && normal != vec4(0,0,0,1)){");
                pFactory->AddLine(FragmentProgram, "    ocolor = vec4(0.0);");
                pFactory->StartLightLoop();
                pFactory->AddLine(FragmentProgram, "    ocolor.rgb += icolor.rgb * lightcolorD.rgb * max(0, dot(normal.xyz, L.xyz));");
                pFactory->EndLightLoop();
                pFactory->AddLine(FragmentProgram, "} else {");
                pFactory->AddLine(FragmentProgram, "    ocolor.rgb = icolor.rgb;");
                pFactory->AddLine(FragmentProgram, "}");
            }
            else
            {
                // use the fragment color multiplied by the facing ratio if no light source is used
                pFactory->AddLine(FragmentProgram, "if ("+vdb->displayTypeFragUni+" == "+GlString(C4DOPENVDB_DISPLAY_SHAPE_SQUARE)+" && normal != vec4(0,0,0,1)){");
                pFactory->AddLine(FragmentProgram, "    ocolor.rgb = icolor.rgb * abs(dot(normal.xyz, V));");
                pFactory->AddLine(FragmentProgram, "} else {");
                pFactory->AddLine(FragmentProgram, "    ocolor.rgb = "+vdb->userColorUni+";");
                pFactory->AddLine(FragmentProgram, "}");
            }
            pFactory->AddLine(FragmentProgram, "if (normal != vec4(0,0,0,1)){"); // if not transperency
            pFactory->AddLine(FragmentProgram, "    ocolor.a = icolor.a;");
            pFactory->AddLine(FragmentProgram, "} else {");
            pFactory->AddLine(FragmentProgram, "    ocolor.a = icolor.r * 0.025;"); // take transparency from r channel
            pFactory->AddLine(FragmentProgram, "}");
            pFactory->AddLine(FragmentProgram, "if ("+vdb->backFaceUni+" > 0) if (dot(normal.xyz, V) < 0.0) discard;");
            pFactory->CompilePrograms();
            if (lightCnt > 0)
                pFactory->InitLightParameters();
        }
        pFactory->BindPrograms();
        
        pFactory->SetParameterMatrixTransform(pFactory->GetParameterHandle(VertexProgram, "transmatrix"));
        pFactory->SetParameterVector(pFactory->GetParameterHandle(FragmentProgram, "eyeposition"), (Vector32)bd->GetMg().off);
        pFactory->SetParameterMatrix(pFactory->GetParameterHandle(VertexProgram, "objectmatrix_p"), pFactory->GetParameterHandle(VertexProgram, "objectmatrix_r"), pFactory->GetParameterHandle(VertexProgram, "objectmatrix_n"), (Matrix32)bh->GetMg());
        
        //set uniforms
        pFactory->InitSetParameters();
        pFactory->SetParameterReal(pFactory->GetParameterHandle(VertexProgram, vdb->voxelSizeUni.GetCString()), (Float32)vdb->voxelSize);
        pFactory->SetParameterInt2(pFactory->GetParameterHandle(VertexProgram, vdb->vpSizeUni.GetCString()), (cr-cl), (cb-ct));
        pFactory->SetParameterMatrix(pFactory->GetParameterHandle(VertexProgram, vdb->projMatUni.GetCString()), projMat);
        pFactory->SetParameterInt(pFactory->GetParameterHandle(VertexProgram, vdb->displayTypeUni.GetCString()), displayType);
        pFactory->SetParameterInt(pFactory->GetParameterHandle(FragmentProgram, vdb->displayTypeFragUni.GetCString()), displayType);
        pFactory->SetParameterInt(pFactory->GetParameterHandle(FragmentProgram, vdb->backFaceUni.GetCString()), displaySettings.GetBool(DISPLAYTAG_BACKFACECULLING));
        pFactory->SetParameterVector(pFactory->GetParameterHandle(FragmentProgram, vdb->userColorUni.GetCString()), (Vector32)userColor);
        
        if (lightCnt > 0)
            pFactory->SetLightParameters(ppLights, lightCnt, Matrix4d32());
        
        if (transperant)
            bd->SetTransparency(-255);
        
        // obtain the vector information from the factory so that we have the proper attribute locations
        pFactory->GetVectorInfo(vectorCnt, ppVectorInfo);
        if (displayType == C4DOPENVDB_DISPLAY_SHAPE_SQUARE)
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
        if (transperant)
            displayType = C4DOPENVDB_DISPLAY_SHAPE_DOT;
        
        switch (displayType)
        {
            case C4DOPENVDB_DISPLAY_SHAPE_SQUARE:
            {
                for (x=0; x < vdb->surfaceCnt; x++)
                {
                    Vector *polyPoints = NewMem(Vector, 4);
                    Vector *polyCol = NewMem(Vector, 4);
                    Vector *polyNor = NewMem(Vector, 4);
                    Vector32 V = ((Vector32)bd->GetMg().off - vdb->surfaceAttribs[x].pos).GetNormalized();
                    Float32 colBrightness = Abs(Dot(vdb->surfaceAttribs[x].norm, V));

                    polyPoints[0] = (Vector)vdb->surfaceAttribs[x].pos + (camMat.v1 * -0.5 * vdb->voxelSize) + (camMat.v2 * -0.5 * vdb->voxelSize);
                    polyCol[0] = (Vector)vdb->surfaceAttribs[x].col * colBrightness;
                    
                    polyPoints[1] = (Vector)vdb->surfaceAttribs[x].pos + (camMat.v1 * 0.5 * vdb->voxelSize) + (camMat.v2 * -0.5 * vdb->voxelSize);
                    polyCol[1] = (Vector)vdb->surfaceAttribs[x].col * colBrightness;
                    
                    polyPoints[2] = (Vector)vdb->surfaceAttribs[x].pos + (camMat.v1 * 0.5 * vdb->voxelSize) + (camMat.v2 * 0.5 * vdb->voxelSize);
                    polyCol[2] = (Vector)vdb->surfaceAttribs[x].col * colBrightness;
                    
                    polyPoints[3] = (Vector)vdb->surfaceAttribs[x].pos + (camMat.v1 * -0.5 * vdb->voxelSize) + (camMat.v2 * 0.5 * vdb->voxelSize);
                    polyCol[3] = (Vector)vdb->surfaceAttribs[x].col * colBrightness;
                    
                    polyNor[0] = polyNor[1] = polyNor[2] = polyNor[3] = CalcFaceNormal(polyPoints, CPolygon(0,1,2,3));
                    
                    bd->DrawPoly(polyPoints, polyCol, polyNor, 4, 0);
                    
                    DeleteMem(polyPoints);
                    DeleteMem(polyCol);
                    DeleteMem(polyNor);
                }
                break;
            }
            case C4DOPENVDB_DISPLAY_SHAPE_BOX_LINES:
            {
                for (x=0; x< vdb->surfaceCnt; x++)
                {
                    bd->DrawBox(MatrixMove((Vector)vdb->surfaceAttribs[x].pos), vdb->voxelSize/2, (Vector)vdb->surfaceAttribs[x].col, true);
                }
                break;
            }
            case C4DOPENVDB_DISPLAY_SHAPE_BOX:
            {
                bd->SetLightList(BDRAW_SETLIGHTLIST_NOLIGHTS);
                for (x=0; x < vdb->surfaceCnt; x++)
                {
                    bd->DrawBox(MatrixMove((Vector)vdb->surfaceAttribs[x].pos), vdb->voxelSize/2, (Vector)vdb->surfaceAttribs[x].col, false);
                }
                break;
            }
            default:
            {
                Float32 *c = NewMem(Float32, vdb->surfaceCnt*3);
                Vector32 *voxelP = NewMem(Vector32, vdb->surfaceCnt);
                for (x=0; x < vdb->surfaceCnt; x++)
                {
                    Vector32 cLin = (Vector32)vdb->surfaceAttribs[x].col;
                    c[x*3] = cLin.x;
                    c[(x*3)+1] = cLin.y;
                    c[(x*3)+2] = cLin.z;
                    
                    voxelP[x] = vdb->surfaceAttribs[x].pos;
                }
                bd->DrawPointArray(vdb->surfaceCnt, voxelP, c, 3);
                DeleteMem(c);
                break;
            }
        }
    }
    
    return true;
}

//---------------------------------------------------------------------------------
//
// MARK: C4DOpenVDBObject Class
//
//---------------------------------------------------------------------------------

Bool C4DOpenVDBObject::Init(GeListNode* node)
{
    if (!SUPER::Init(node))
        return false;
    
    ClearSurface(this, false); // set up our new VDB surface
    
    node->SetParameter(DescLevel(C4DOPENVDB_DISPLAY_SHAPE), GeData(C4DOPENVDB_DISPLAY_SHAPE_SQUARE), DESCFLAGS_SET_0);
    
    BaseObject *obj = static_cast<BaseObject*>(node);
    if (!obj)
        return false;
    
    UpdateVoxelCountUI(obj);
    
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

void C4DOpenVDBObject::Free(GeListNode *node)
{
    GlVertexBuffer::RemoveObject((C4DAtom*)node, 0);
    GlProgramFactory::RemoveReference((C4DAtom*)node);
    DeleteMem(surfaceAttribs);
    DeleteObj(vectorInfoArray);
    DeleteObj(attribInfoArray);
    
    if (helper)
        ClearSurface(this, true);
    
    SUPER::Free(node);
}

DRAWRESULT C4DOpenVDBObject::Draw(BaseObject *op, DRAWPASS drawpass, BaseDraw *bd, BaseDrawHelp *bh)
{
    if (drawpass != DRAWPASS_OBJECT)
        return DRAWRESULT_SKIP;
    
    if (bd->TestBreak())
        return DRAWRESULT_SKIP;
    
    BaseDocument*    doc = bh->GetDocument();
    const LayerData* ld  = op->GetLayerData(doc);
    
    //check if generator is enabled
    if (!op->GetDeformMode())
        return SUPER::Draw(op, drawpass, bd, bh); // We'll skip in the parent class
    
    //check if object is being controlled
    if (isSlaveOfMaster)
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
        
    return (DrawVDB(op, this, bd, bh)) ? SUPER::Draw(op, drawpass, bd, bh) : DRAWRESULT_SKIP;
}

Bool C4DOpenVDBObject::GetDDescription(GeListNode* node, Description* description, DESCFLAGS_DESC& flags)
{
    if (!node || !description)
        return false;
    
    if (!(flags & DESCFLAGS_DESC_LOADED))
    {
        // load the description of this type
        if (!description->LoadDescription(node->GetType()))
            return false;
        
        flags |= DESCFLAGS_DESC_LOADED;
    }
    
    const DescID* singleid = description->GetSingleDescID();
    
    // parameter ID
    DescID cid = DescLevel(C4DOPENVDB_DISPLAY_GROUP, DTYPE_GROUP, 0);
    // check if this parameter ID is requested
    if (!singleid || cid.IsPartOf(*singleid, nullptr))
    {
        BaseContainer bc = GetCustomDataTypeDefault(DTYPE_GROUP);
        bc.SetString(DESC_NAME, GeLoadString(C4DOPENVDB_DISPLAY_GROUP));
        bc.SetInt32(DESC_DEFAULT, 1);
        bc.SetInt32(DESC_SCALEH, 1);
        
        description->SetParameter(cid, bc, DescLevel(ID_OBJECTPROPERTIES));
    }
    
    // parameter ID
    cid = DescLevel(C4DOPENVDB_DISPLAY_SHAPE, DTYPE_LONG, 0);
    // check if this parameter ID is requested
    if (!singleid || cid.IsPartOf(*singleid, nullptr))
    {
        BaseContainer bc = GetCustomDataTypeDefault(DTYPE_LONG);
        bc.SetInt32(DESC_CUSTOMGUI, CUSTOMGUI_CYCLE);
        bc.SetString(DESC_NAME, GeLoadString(C4DOPENVDB_DISPLAY_SHAPE));
        bc.SetInt32(DESC_SCALEH, 1);
        
        // cycle elements
        BaseContainer items;
        items.SetString(C4DOPENVDB_DISPLAY_SHAPE_SQUARE, GeLoadString(C4DOPENVDB_DISPLAY_SHAPE_SQUARE));
        items.SetString(C4DOPENVDB_DISPLAY_SHAPE_DOT, GeLoadString(C4DOPENVDB_DISPLAY_SHAPE_DOT));
        items.SetString(C4DOPENVDB_DISPLAY_SHAPE_BOX, GeLoadString(C4DOPENVDB_DISPLAY_SHAPE_BOX));
        items.SetString(C4DOPENVDB_DISPLAY_SHAPE_BOX_LINES, GeLoadString(C4DOPENVDB_DISPLAY_SHAPE_BOX_LINES));
        
        bc.SetContainer(DESC_CYCLE,items);
        
        description->SetParameter(cid, bc, DescLevel(C4DOPENVDB_DISPLAY_GROUP));
    }
    
    // parameter ID
    cid = DescLevel(C4DOPENVDB_SEPARATOR, DTYPE_SEPARATOR, 0);
    // check if this parameter ID is requested
    if (!singleid || cid.IsPartOf(*singleid, nullptr))
    {
        BaseContainer bc = GetCustomDataTypeDefault(DTYPE_SEPARATOR);
        bc.SetInt32(DESC_CUSTOMGUI, CUSTOMGUI_SEPARATOR);
        bc.SetBool(DESC_SEPARATORLINE, true);
        
        description->SetParameter(cid, bc, DescLevel(ID_OBJECTPROPERTIES));
    }
    
    // parameter ID
    cid = DescLevel(C4DOPENVDB_GRID_NAME, DTYPE_STATICTEXT, 0);
    // check if this parameter ID is requested
    if (!singleid || cid.IsPartOf(*singleid, nullptr))
    {
        BaseContainer bc = GetCustomDataTypeDefault(DTYPE_STATICTEXT);
        bc.SetString(DESC_NAME, GeLoadString(C4DOPENVDB_GRID_NAME));
        bc.SetInt32(DESC_CUSTOMGUI, CUSTOMGUI_STATICTEXT);
        
        description->SetParameter(cid, bc, DescLevel(ID_OBJECTPROPERTIES));
    }
    
    // parameter ID
    cid = DescLevel(C4DOPENVDB_GRID_TYPE, DTYPE_STATICTEXT, 0);
    // check if this parameter ID is requested
    if (!singleid || cid.IsPartOf(*singleid, nullptr))
    {
        BaseContainer bc = GetCustomDataTypeDefault(DTYPE_STATICTEXT);
        bc.SetString(DESC_NAME, GeLoadString(C4DOPENVDB_GRID_TYPE));
        bc.SetInt32(DESC_CUSTOMGUI, CUSTOMGUI_STATICTEXT);
        
        description->SetParameter(cid, bc, DescLevel(ID_OBJECTPROPERTIES));
    }
    
    // parameter ID
    cid = DescLevel(C4DOPENVDB_SURFACE_COUNT, DTYPE_STATICTEXT, 0);
    // check if this parameter ID is requested
    if (!singleid || cid.IsPartOf(*singleid, nullptr))
    {
        BaseContainer bc = GetCustomDataTypeDefault(DTYPE_STATICTEXT);
        bc.SetString(DESC_NAME, GeLoadString(C4DOPENVDB_SURFACE_COUNT));
        bc.SetInt32(DESC_CUSTOMGUI, CUSTOMGUI_STATICTEXT);
        
        description->SetParameter(cid, bc, DescLevel(ID_OBJECTPROPERTIES));
    }
    
    // parameter ID
    cid = DescLevel(C4DOPENVDB_VOXEL_COUNT, DTYPE_STATICTEXT, 0);
    // check if this parameter ID is requested
    if (!singleid || cid.IsPartOf(*singleid, nullptr))
    {
        BaseContainer bc = GetCustomDataTypeDefault(DTYPE_STATICTEXT);
        bc.SetString(DESC_NAME, GeLoadString(C4DOPENVDB_VOXEL_COUNT));
        bc.SetInt32(DESC_CUSTOMGUI, CUSTOMGUI_STATICTEXT);
        
        description->SetParameter(cid, bc, DescLevel(ID_OBJECTPROPERTIES));
    }
    
    return SUPER::GetDDescription(node, description, flags);
}

void C4DOpenVDBObject::UpdateVoxelCountUI(BaseObject *op)
{
    String fSurfaceCnt = FormatWithCommas(surfaceCnt);
    String fVoxelCnt = FormatWithCommas(voxelCnt);
    op->SetParameter(DescLevel(C4DOPENVDB_SURFACE_COUNT), GeData(fSurfaceCnt), DESCFLAGS_SET_0);
    op->SetParameter(DescLevel(C4DOPENVDB_VOXEL_COUNT), GeData(fVoxelCnt), DESCFLAGS_SET_0);
    op->SetParameter(DescLevel(C4DOPENVDB_GRID_NAME), GeData(gridName), DESCFLAGS_SET_0);
    op->SetParameter(DescLevel(C4DOPENVDB_GRID_TYPE), GeData(gridType), DESCFLAGS_SET_0);
}

//---------------------------------------------------------------------------------
//
// MARK: Public helper functions
//
//---------------------------------------------------------------------------------

BaseObject* GetDummyPolygon(void)
{
    BaseObject *outObject = BaseObject::Alloc(Onull);
    
    if (!outObject)
    {
        blDelete(outObject);
        return nullptr;
    }
    
    BaseObject *dummy = BaseObject::Alloc(Osinglepoly);
    
    if (!dummy)
    {
        blDelete(dummy);
        return nullptr;
    }
    
    dummy->SetEditorMode(MODE_OFF);
    dummy->SetRenderMode(MODE_OFF);
    dummy->SetAbsScale(Vector(0));
    dummy->SetName("Dummy Polygon");
    dummy->InsertUnder(outObject);
    
    return outObject;
}

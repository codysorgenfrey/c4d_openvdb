//
//  openVDBFunctions.cpp
//  C4DOpenVDB
//
//  Created by Cody Sorgenfrey on 7/20/18.
//  Copyright © 2018 MAXON Computer GmbH. All rights reserved.
//

#include "C4DOpenVDBFunctions.h"
#include <openvdb/openvdb.h>
#include <openvdb/tools/VolumeToMesh.h> //To get surface intersecting voxels
#include <openvdb/tools/LevelSetSphere.h> //make sphere
#include <openvdb/tools/LevelSetPlatonic.h> //make other shapes
#include <openvdb/tools/LevelSetUtil.h> //for sdf to fog
#include <openvdb/tools/GridOperators.h> // for gradient of the grid
#include <openvdb/tools/Morphology.h> // for activate
#include <openvdb/tools/GridTransformer.h> //for resampleToMatch
#include <openvdb/util/PagedArray.h> // paged array for collecting surface voxels
#include <openvdb/tools/Statistics.h> // for debugging
#include <openvdb/tools/Composite.h> // for combining grids
#include <openvdb/tools/LevelSetRebuild.h> // for doLevelSetRebuild

using namespace openvdb;

//
//---------------------------------------------------------------------------------
//
// MARK: Private helper functions
//
//---------------------------------------------------------------------------------

Bool TransformGrid(Matrix mat, FloatGrid::Ptr grid);

Bool TransformGrid(Matrix mat, FloatGrid::Ptr grid)
{
    if (!grid)
        return false;
    
    math::Transform::Ptr xform = grid->transformPtr();
    Vec4d v1(mat.v1.x, mat.v1.y, mat.v1.z, 0);
    Vec4d v2(mat.v2.x, mat.v2.y, mat.v2.z, 0);
    Vec4d v3(mat.v3.x, mat.v3.y, mat.v3.z, 0);
    Vec4d v4(mat.off.x, mat.off.y, mat.off.z, 1);
    Mat4R xformMat(v1, v2, v3, v4);
    
    xform->postMult(xformMat);
    
    return true;
}

//---------------------------------------------------------------------------------
//
// MARK: Public helper functions
//
//---------------------------------------------------------------------------------

class VDBObjectHelper
{
public:
    //members
    FloatGrid::Ptr grid;
    
    //methods
    inline VDBObjectHelper(void){ grid = nullptr; };
};

VDBObjectHelper* InitVDBObjectHelper()
{
    initialize();
    
    VDBObjectHelper *helper = NewObj(VDBObjectHelper);
    
    return helper;
}

void DeleteVDBObjectHelper(VDBObjectHelper *helper)
{
    DeleteObj(helper);
}

Bool SDFToFog(VDBObjectHelper *helper)
{
    tools::sdfToFogVolume(*helper->grid);
    helper->grid->setGridClass(GRID_FOG_VOLUME);
    helper->grid->setName("density");
    return true;
}

Bool FillSDFInterior(VDBObjectHelper *helper)
{
    if (!helper->grid) return false;
    
    float inBand = std::numeric_limits<float>::max();
    float exBand = helper->grid->metaValue<float>("Band Width");
    float iso = tools::lsutilGridZero<FloatGrid>();
    FloatGrid::Ptr outGrid = tools::levelSetRebuild(*helper->grid, iso, exBand, inBand, &helper->grid->transform());
    
    outGrid->setName(helper->grid->getName());
    MetaMap::Ptr map = helper->grid->deepCopyMeta();
    outGrid->insertMeta(*map);
    outGrid->setGridClass(helper->grid->getGridClass());
    
    helper->grid = outGrid;

    return true;
}

Bool UnsignSDF(VDBObjectHelper *helper)
{
    if (!helper->grid) return false;
    
    struct unsignSDF
    {
        static inline void op(const FloatGrid::ValueOnIter& iter){
            float val = iter.getValue();
            if (val < 0) iter.setValue(math::Abs(val));
        }
    };
    tools::foreach(helper->grid->beginValueOn(), unsignSDF::op);
    helper->grid->insertMeta("Unsigned", BoolMetadata(true));
    return true;
}

Bool UpdateSurface(C4DOpenVDBObject *obj, BaseObject *op, Vector32 userColor)
{
    if (!obj->helper->grid) return false;
    
    struct attribs { Vec3f pos; Vec3f norm; Vec3f col; };
    util::PagedArray<attribs> attribsArray;
    util::PagedArray<attribs>::ValueBuffer attribsBufferDummy(attribsArray);//dummy used for initialization
    tbb::enumerable_thread_specific<util::PagedArray<attribs>::ValueBuffer> attribsPool(attribsBufferDummy);//thread local storage pool of ValueBuffers
    math::Transform xform = obj->helper->grid->transform();
    
    if (obj->helper->grid->getGridClass() == GRID_LEVEL_SET && !obj->helper->grid->metaValue<bool>("Unsigned"))
    {
        FloatGrid::ValueConverter<bool>::Type::Ptr isoMask = tools::extractIsosurfaceMask(*obj->helper->grid, tools::lsutilGridZero<FloatGrid>());
        tools::ScalarToVectorConverter<FloatGrid>::Type::Ptr gradGrid = tools::gradient(*obj->helper->grid, *isoMask);

        tree::IteratorRange<VectorGrid::ValueOnCIter> gradRange(gradGrid->tree().cbeginValueOn());
        tbb::parallel_for(
                          gradRange,
                          [&xform, &attribsPool, &userColor](tree::IteratorRange<VectorGrid::ValueOnCIter>& range) {
                              util::PagedArray<attribs>::ValueBuffer &attribsBuffer = attribsPool.local();
                              for ( ; range; ++range){
                                  VectorGrid::ValueOnCIter iter = range.iterator();
                                  attribs myAttribs;
                                  myAttribs.pos = xform.indexToWorld( iter.getCoord() );
                                  myAttribs.norm = iter.getValue();
                                  myAttribs.col = Vec3f(userColor.x, userColor.y, userColor.z);
                                  attribsBuffer.push_back( myAttribs );
                              }
                          }
                          );
    }
    else
    {
        tree::IteratorRange<FloatGrid::ValueOnCIter> floatRange(obj->helper->grid->tree().cbeginValueOn());
        float min,max;
        obj->helper->grid->evalMinMax(min, max);
        tbb::parallel_for(
                          floatRange,
                          [&xform, &attribsPool, &min, &max](tree::IteratorRange<FloatGrid::ValueOnCIter>& range) {
                              util::PagedArray<attribs>::ValueBuffer &attribsBuffer = attribsPool.local();
                              for ( ; range; ++range){
                                  FloatGrid::ValueOnCIter iter = range.iterator();
                                  if (*iter >= min && *iter < max) { // exclude max since the interior is filled with max
                                      attribs myAttribs;
                                      myAttribs.pos = xform.indexToWorld( iter.getCoord() );
                                      myAttribs.norm = Vec3f(0);
                                      float col = math::Abs(*iter) / max;
                                      if (!math::isFinite(col)) col = 0.0f;
                                      myAttribs.col = Vec3f(col);
                                      attribsBuffer.push_back( myAttribs );
                                  }
                              }
                          }
                          );
    }
    for (auto i=attribsPool.begin(); i!=attribsPool.end(); ++i) i->flush();
    
    obj->surfaceCnt = Int32(attribsArray.size());
    
    DeleteMem(obj->surfaceAttribs);
    
    obj->surfaceAttribs = NewMem(voxelAttribs, obj->surfaceCnt);
    
    for (int x=0;x<obj->surfaceCnt;x++)
    {
        obj->surfaceAttribs[x].pos = Vector32(attribsArray[x].pos.x(), attribsArray[x].pos.y(), attribsArray[x].pos.z());
        obj->surfaceAttribs[x].col = Vector32(attribsArray[x].col.x(), attribsArray[x].col.y(), attribsArray[x].col.z());
        obj->surfaceAttribs[x].norm = Vector32(attribsArray[x].norm.x(), attribsArray[x].norm.y(), attribsArray[x].norm.z());
    }
    
    GridClass gridClass = obj->helper->grid->getGridClass();
    Name gridType = obj->helper->grid->valueType();
    
    switch (gridClass) {
        case GRID_LEVEL_SET:
            obj->gridType = String(gridType.c_str()) + ", " + GeLoadString(C4DOPENVDB_GRID_CLASS_SDF);
            obj->gridClass = C4DOPENVDB_GRID_CLASS_SDF;
            break;
            
        case GRID_FOG_VOLUME:
            obj->gridType = String(gridType.c_str()) + ", " + GeLoadString(C4DOPENVDB_GRID_CLASS_FOG);
            obj->gridClass = C4DOPENVDB_GRID_CLASS_FOG;
            break;
            
        default:
            obj->gridType = String(gridType.c_str()) + ", " + GeLoadString(C4DOPENVDB_GRID_CLASS_UNKNOWN);
            obj->gridClass = C4DOPENVDB_GRID_CLASS_UNKNOWN;
            break;
    }
    
    if (obj->helper->grid->metaValue<bool>("Unsigned"))
        obj->UDF = true;
    else
        obj->UDF = false;
    
    obj->gridName = obj->helper->grid->getName().c_str();
    obj->voxelCnt = obj->helper->grid->activeVoxelCount();
    obj->voxelSize = obj->helper->grid->voxelSize()[0]; // Asuming uniform voxel size
    
    obj->UpdateVoxelCountUI(op);
    
    return true;
}

Bool MakeShape(VDBObjectHelper *helper, Int32 shape, float width, Vector center, float voxelSize, float bandWidth)
{
    //std::clock_t begin = std::clock();
    switch (shape) {
        case C4DOPENVDB_PRIM_SETTINGS_TYPE_SPHERE:
            helper->grid = tools::createLevelSetSphere<FloatGrid>(width, Vec3f(center.x, center.y, center.z), voxelSize, bandWidth);
            break;
            
        case C4DOPENVDB_PRIM_SETTINGS_TYPE_CUBE:
            helper->grid = tools::createLevelSetCube<FloatGrid>(width, Vec3f(center.x, center.y, center.z), voxelSize, bandWidth);
            break;
            
        case C4DOPENVDB_PRIM_SETTINGS_TYPE_DODEC:
            helper->grid = tools::createLevelSetDodecahedron<FloatGrid>(width, Vec3f(center.x, center.y, center.z), voxelSize, bandWidth);
            break;
            
        case C4DOPENVDB_PRIM_SETTINGS_TYPE_ICOS:
            helper->grid = tools::createLevelSetIcosahedron<FloatGrid>(width, Vec3f(center.x, center.y, center.z), voxelSize, bandWidth);
            break;
            
        case C4DOPENVDB_PRIM_SETTINGS_TYPE_OCT:
            helper->grid = tools::createLevelSetOctahedron<FloatGrid>(width, Vec3f(center.x, center.y, center.z), voxelSize, bandWidth);
            break;
            
        case C4DOPENVDB_PRIM_SETTINGS_TYPE_TET:
            helper->grid = tools::createLevelSetTetrahedron<FloatGrid>(width, Vec3f(center.x, center.y, center.z), voxelSize, bandWidth);
            break;
            
        default:
            break;
    }
    helper->grid->setGridClass(GRID_LEVEL_SET);
    helper->grid->setName("surface");
    helper->grid->insertMeta("Unsigned", BoolMetadata(false));
    helper->grid->insertMeta("Band Width", FloatMetadata(bandWidth));
    
    //std::clock_t end = std::clock();
    //std::cout << "Build SDF (sec): " << double(end - begin) / CLOCKS_PER_SEC << std::endl;
    
    return true;
}

Bool UpdateSurfaceSlice(C4DOpenVDBObject *vdb, C4DOpenVDBVisualizer *vis, Int32 axis, Float offset, Gradient *grad)
{
    if (!vdb->helper->grid) return false;
    
    vis->voxelSize = vdb->helper->grid->voxelSize().x();
    
    struct attribs { Vec3f pos; float val; };
    util::PagedArray<attribs> attribsArray;
    util::PagedArray<attribs>::ValueBuffer attribsBufferDummy(attribsArray);//dummy used for initialization
    tbb::enumerable_thread_specific<util::PagedArray<attribs>::ValueBuffer> attribsPool(attribsBufferDummy);//thread local storage pool of ValueBuffers
    math::Transform xform = vdb->helper->grid->transform();
    Float offsetMin = offset - vis->voxelSize;
    Float offsetMax = offset + vis->voxelSize;
    
    tree::IteratorRange<FloatGrid::ValueOnCIter> floatRange(vdb->helper->grid->tree().cbeginValueOn());
    float min,max;
    vdb->helper->grid->evalMinMax(min, max);
    switch (axis) {
        case PRIM_AXIS_XN:
        case PRIM_AXIS_XP:
            tbb::parallel_for(
                              floatRange,
                              [&xform, &attribsPool, &min, &max, &offsetMin, &offsetMax](tree::IteratorRange<FloatGrid::ValueOnCIter>& range) {
                                  util::PagedArray<attribs>::ValueBuffer &attribsBuffer = attribsPool.local();
                                  for ( ; range; ++range){
                                      FloatGrid::ValueOnCIter iter = range.iterator();
                                      Vec3f pos = xform.indexToWorld( iter.getCoord() );
                                      if (pos.x() > offsetMin && pos.x() < offsetMax){
                                          attribs myAttribs;
                                          myAttribs.pos = pos;
                                          if (*iter > 0)
                                          {
                                              myAttribs.val = *iter / max;
                                          } else
                                          {
                                              myAttribs.val = *iter / math::Abs(min);
                                          }
                                          if (!math::isFinite(myAttribs.val)) myAttribs.val = 0.0f;
                                          attribsBuffer.push_back( myAttribs );
                                      }
                                  }
                              }
                              );
            break;
        
        case PRIM_AXIS_YN:
        case PRIM_AXIS_YP:
            tbb::parallel_for(
                              floatRange,
                              [&xform, &attribsPool, &min, &max, &offsetMin, &offsetMax](tree::IteratorRange<FloatGrid::ValueOnCIter>& range) {
                                  util::PagedArray<attribs>::ValueBuffer &attribsBuffer = attribsPool.local();
                                  for ( ; range; ++range){
                                      FloatGrid::ValueOnCIter iter = range.iterator();
                                      Vec3f pos = xform.indexToWorld( iter.getCoord() );
                                      if (pos.y() > offsetMin && pos.y() < offsetMax){
                                          attribs myAttribs;
                                          myAttribs.pos = pos;
                                          if (*iter > 0)
                                          {
                                              myAttribs.val = *iter / max;
                                          } else
                                          {
                                              myAttribs.val = *iter / math::Abs(min);
                                          }
                                          if (!math::isFinite(myAttribs.val)) myAttribs.val = 0.0f;
                                          attribsBuffer.push_back( myAttribs );
                                      }
                                  }
                              }
                              );
            break;
            
        case PRIM_AXIS_ZN:
        case PRIM_AXIS_ZP:
            tbb::parallel_for(
                              floatRange,
                              [&xform, &attribsPool, &min, &max, &offsetMin, &offsetMax](tree::IteratorRange<FloatGrid::ValueOnCIter>& range) {
                                  util::PagedArray<attribs>::ValueBuffer &attribsBuffer = attribsPool.local();
                                  for ( ; range; ++range){
                                      FloatGrid::ValueOnCIter iter = range.iterator();
                                      Vec3f pos = xform.indexToWorld( iter.getCoord() );
                                      if (pos.z() > offsetMin && pos.z() < offsetMax){
                                          attribs myAttribs;
                                          myAttribs.pos = pos;
                                          if (*iter > 0)
                                          {
                                              myAttribs.val = *iter / max;
                                          } else
                                          {
                                              myAttribs.val = *iter / math::Abs(min);
                                          }
                                          if (!math::isFinite(myAttribs.val)) myAttribs.val = 0.0f;
                                          attribsBuffer.push_back( myAttribs );
                                      }
                                  }
                              }
                              );
            break;
            
        default:
            break;
    }
    for (auto i=attribsPool.begin(); i!=attribsPool.end(); ++i) i->flush();
    
    vis->surfaceCnt = (Int32)attribsArray.size();
    
    DeleteMem(vis->sliceAttribs);
    
    vis->sliceAttribs = NewMem(visualizerAttribs, vis->surfaceCnt);
    
    for (int x=0;x<vis->surfaceCnt;x++)
    {
        vis->sliceAttribs[x].pos = Vector32(attribsArray[x].pos.x(), attribsArray[x].pos.y(), attribsArray[x].pos.z());
        vis->sliceAttribs[x].col = Vector32( grad->CalcGradientPixel( ((attribsArray[x].val + 1) * 0.5) ) ) ;
    }
    
    return true;
}

Bool GetVDBPolygonized(BaseObject *inObject, Float iso, Float adapt, BaseObject *outObject)
{
    std::vector<Vec3s> points;
    std::vector<Vec3I> tris;
    std::vector<Vec4I> quads;
    PolygonObject      *polyObj;
    Vector             *pointsArr;
    CPolygon           *polyArr;
    C4DOpenVDBObject   *vdb;
    FloatGrid::Ptr     gridCopy;
    
    vdb = inObject->GetNodeData<C4DOpenVDBObject>();
    if (!vdb || !vdb->helper->grid)
        return false;
    
    gridCopy = vdb->helper->grid->deepCopy();
    
    TransformGrid(inObject->GetMl(), gridCopy);
    
    tools::volumeToMesh(*gridCopy, points, tris, quads, iso, adapt);
    
    // setup new poly obj
    polyObj = PolygonObject::Alloc(points.size(), (tris.size() + quads.size()));
    
    // set points
    pointsArr = polyObj->GetPointW();
    for (Int32 x = 0; x < points.size(); x++)
    {
        pointsArr[x] = Vector(points[x].x(), points[x].y(), points[x].z());
    }
    
    // set quads
    polyArr = polyObj->GetPolygonW();
    for (Int32 x = 0; x < quads.size(); x++)
    {
        polyArr[x] = CPolygon(quads[x].x(), quads[x].y(), quads[x].z(), quads[x].w());
    }
    
    // set tris
    for (Int32 x = 0; x < tris.size(); x++)
    {
        polyArr[x + quads.size()] = CPolygon(tris[x].x(), tris[x].y(), tris[x].z());
    }
    
    polyObj->InsertUnder(outObject);
    
    return true;
}

struct GridOps {
    static inline void diff(const float& a, const float& b, float& result) {
        result = a - b;
    }
    static inline void invert(const float& a, const float& b, float& result) {
        result = 1 - a;
    }
    static inline void blend1(const float& a, const float& b, float& result) {
        result = (1 - a) * b;
    }
    static inline void blend2(const float& a, const float& b, float& result) {
        result = a + (1 - a) * b;
    }
};

Bool CombineVDBs(C4DOpenVDBObject *obj,
                 maxon::BaseArray<BaseObject*> *inputs,
                 Int32 operation,
                 Int32 resample,
                 Int32 interpolation,
                 Bool deactivate,
                 Float deactivateVal,
                 Bool prune,
                 Float pruneVal,
                 Bool signedFloodFill)
{
    FloatGrid::Ptr          gridA, gridB;
    C4DOpenVDBObject        *vdb;
    float                   gridZero = 0;
    
    for (Int32 x = 0; x < inputs->GetCount(); x++)
    {
        // Set up our loop, only set gridA on the first pass so that we collect the results through the whole heirarchy.
        if (!gridA)
        {
            vdb = inputs->operator[](x)->GetNodeData<C4DOpenVDBObject>();
            if (!vdb || !vdb->helper->grid)
                return false;
            
            gridA = vdb->helper->grid->deepCopy();
            
            TransformGrid(inputs->operator[](x)->GetMl(), gridA);
            
            if (x+1 < inputs->GetCount())
                x++;
            else
                goto end; // don't return false cause that will be an error and constantly rebuild cache
        }
        
        vdb = inputs->operator[](x)->GetNodeData<C4DOpenVDBObject>();
        if (!vdb || !vdb->helper->grid)
            return false;
        
        gridB = vdb->helper->grid->deepCopy();

        TransformGrid(inputs->operator[](x)->GetMl(), gridB);
        
        if (resample != C4DOPENVDB_COMBINE_RESAMPLE_NONE && gridA->constTransform() != gridB->constTransform())
        {
            FloatGrid::Ptr resampleGrid, toMatchGrid;
            switch (resample)
            {
                case C4DOPENVDB_COMBINE_RESAMPLE_B_A:
                    resampleGrid = gridB->deepCopy();
                    toMatchGrid = gridA->deepCopy();
                    break;
                    
                case C4DOPENVDB_COMBINE_RESAMPLE_A_B:
                    resampleGrid = gridA->deepCopy();
                    toMatchGrid = gridB->deepCopy();
                    break;
                    
                case C4DOPENVDB_COMBINE_RESAMPLE_HIGH_LOW:
                    if (gridA->voxelSize()[0] < gridB->voxelSize()[0])
                    {
                        resampleGrid = gridA->deepCopy();
                        toMatchGrid = gridB->deepCopy();
                    }
                    else {
                        resampleGrid = gridB->deepCopy();
                        toMatchGrid = gridA->deepCopy();
                    }
                    break;
                    
                case C4DOPENVDB_COMBINE_RESAMPLE_LOW_HIGH:
                    if (gridA->voxelSize()[0] < gridB->voxelSize()[0])
                    {
                        resampleGrid = gridB->deepCopy();
                        toMatchGrid = gridA->deepCopy();
                    }
                    else {
                        resampleGrid = gridA->deepCopy();
                        toMatchGrid = gridB->deepCopy();
                    }
                    break;
            }
            
            switch (interpolation) {
                case C4DOPENVDB_COMBINE_INTERPOLATION_LINEAR:
                    tools::resampleToMatch<tools::Sampler<1,false>>(*resampleGrid, *toMatchGrid);
                    break;
                    
                case C4DOPENVDB_COMBINE_INTERPOLATION_NEAREST:
                default:
                    tools::resampleToMatch<tools::Sampler<0,false>>(*resampleGrid, *toMatchGrid);
                    break;
                    
                case C4DOPENVDB_COMBINE_INTERPOLATION_QUADRATIC:
                    tools::resampleToMatch<tools::Sampler<2,false>>(*resampleGrid, *toMatchGrid);
                    break;
            }
            
            switch (resample)
            {
                case C4DOPENVDB_COMBINE_RESAMPLE_B_A:
                    gridB = toMatchGrid;
                    break;
                    
                case C4DOPENVDB_COMBINE_RESAMPLE_A_B:
                    gridA = toMatchGrid;
                    break;
                    
                case C4DOPENVDB_COMBINE_RESAMPLE_HIGH_LOW:
                    if (gridA->voxelSize()[0] < gridB->voxelSize()[0])
                        gridA = toMatchGrid;
                    else
                        gridB = toMatchGrid;
                    break;
                    
                case C4DOPENVDB_COMBINE_RESAMPLE_LOW_HIGH:
                    if (gridA->voxelSize()[0] < gridB->voxelSize()[0])
                        gridB = toMatchGrid;
                    else
                        gridA = toMatchGrid;
                    break;
                    break;
            }
        }
        
        try
        {
            switch (operation)
            {
                case C4DOPENVDB_COMBINE_OP_SDF_UNION:
                    tools::csgUnion(*gridA, *gridB);
                    break;
                    
                case C4DOPENVDB_COMBINE_OP_SDF_DIFFERENCE:
                    tools::csgDifference(*gridA, *gridB);
                    break;
                    
                case C4DOPENVDB_COMBINE_OP_SDF_INTERSECTION:
                    tools::csgIntersection(*gridA, *gridB);
                    break;
                
                case C4DOPENVDB_COMBINE_OP_COPY_B:
                    gridA = gridB;
                    break;
                    
                case C4DOPENVDB_COMBINE_OP_INVERT_A:
                    gridA->tree().combine(gridA->tree(), GridOps::invert, false);
                    break;
                    
                case C4DOPENVDB_COMBINE_OP_ADD:
                    tools::compSum(*gridA, *gridB);
                    break;
                    
                case C4DOPENVDB_COMBINE_OP_SUBTRACT:
                    gridA->tree().combine(gridB->tree(), GridOps::diff, false);
                    break;
                    
                case C4DOPENVDB_COMBINE_OP_MULT:
                    tools::compMul(*gridA, *gridB);
                    break;
                    
                case C4DOPENVDB_COMBINE_OP_DIVIDE:
                    tools::compDiv(*gridA, *gridB);
                    break;
                    
                case C4DOPENVDB_COMBINE_OP_MAX:
                    tools::compMax(*gridA, *gridB);
                    break;
                    
                case C4DOPENVDB_COMBINE_OP_MIN:
                    tools::compMin(*gridA, *gridB);
                    break;
                    
                case C4DOPENVDB_COMBINE_OP_NEG_A_X_B:
                    gridA->tree().combine(gridB->tree(), GridOps::blend1, false);
                    break;
                    
                case C4DOPENVDB_COMBINE_OP_A_PLUS_NEG_A_X_B:
                    gridA->tree().combine(gridB->tree(), GridOps::blend2, false);
                    break;
                    
                case C4DOPENVDB_COMBINE_OP_ACT_UNION:
                    gridA->topologyUnion(*gridB);
                    break;
                    
                case C4DOPENVDB_COMBINE_OP_ACT_DIFFERENCE:
                    gridA->topologyDifference(*gridB);
                    break;
                    
                case C4DOPENVDB_COMBINE_OP_ACT_INTERESCTION:
                    gridA->topologyIntersection(*gridB);
                    break;
                    
                case C4DOPENVDB_COMBINE_OP_ACT_REPLACE_A_B:
                    tools::compReplace(*gridA, *gridB);
                    break;
                
                case C4DOPENVDB_COMBINE_OP_COPY_A:
                default:
                    break;
            }
        }
        catch (Exception &e)
        {
            WarningOutput(e.what());
        }
        
        if (deactivate)
            tools::deactivate(*gridA, gridA->background(), (gridZero + deactivateVal));
        
        if (signedFloodFill && gridA->getGridClass() == GRID_LEVEL_SET)
            tools::signedFloodFill(gridA->tree());
        
        if (prune)
            tools::prune(gridA->tree(), (gridZero + pruneVal));
    }
end:
    obj->helper->grid = gridA;
     
    return true;
}





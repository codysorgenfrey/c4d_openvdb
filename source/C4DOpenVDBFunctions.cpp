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
#include <openvdb/util/CpuTimer.h> // for profiling
#include <openvdb/tools/MeshToVolume.h> // for mesh to volume and MeshDataAdapter struct
#include <openvdb/tools/LevelSetFilter.h> // for FilterVDB
#include <openvdb/tools/Filter.h> // for FilterVDB
#include <openvdb/tools/ParticlesToLevelSet.h> // for VDBFromPoints

using namespace openvdb;

//
//---------------------------------------------------------------------------------
//
// MARK: Private helper functions
//
//---------------------------------------------------------------------------------

Bool UpdateGridTransform(Matrix mat, FloatGrid &grid);
math::Coord GetVoxelPosFromLeaf(FloatGrid::ValueOnCIter iter, int x, int y, int z);

Bool UpdateGridTransform(Matrix mat, FloatGrid &grid)
{
    StatusSetText("Updating transform: " + String(grid.getName().c_str()));
    
    math::Transform::Ptr xform = grid.transformPtr();
    Vec4d v1(mat.v1.x, mat.v1.y, mat.v1.z, 0);
    Vec4d v2(mat.v2.x, mat.v2.y, mat.v2.z, 0);
    Vec4d v3(mat.v3.x, mat.v3.y, mat.v3.z, 0);
    Vec4d v4(mat.off.x, mat.off.y, mat.off.z, 1);
    Mat4R xformMat(v1, v2, v3, v4);
    
    xform->postMult(xformMat);
    
    return true;
}

math::Coord GetVoxelPosFromLeaf(FloatGrid::ValueOnCIter iter, int x, int y, int z)
{
    const math::CoordBBox bbox = iter.getBoundingBox();
    const math::Coord min = bbox.min();
    // add our offset to the min to get the voxel position
    const int voxelX = min.x() + x;
    const int voxelY = min.y() + y;
    const int voxelZ = min.z() + z;
    return math::Coord(voxelX, voxelY, voxelZ);
}

//---------------------------------------------------------------------------------
//
// MARK: Public helper functions
//
//---------------------------------------------------------------------------------

Bool InitVDBLib(void)
{
    initialize();
    return true;
}

class VDBObjectHelper
{
public:
    //members
    FloatGrid::Ptr grid;
    
    //methods
    inline VDBObjectHelper(void){ grid = nullptr; };
};

Bool SDFToFog(VDBObjectHelper *helper)
{
    StatusSetText("Converting to fog volume");
    
    tools::sdfToFogVolume(*helper->grid);
    helper->grid->setGridClass(GRID_FOG_VOLUME);
    helper->grid->setName("density");
    return true;
}

Bool FillSDFInterior(VDBObjectHelper *helper)
{
    if (!helper->grid) return false;
    
    StatusSetText("Filling interior");
    
    float inBand = std::numeric_limits<float>::max();
    float exBand = helper->grid->metaValue<float>("Ext Band Width");
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
    
    StatusSetText("Converting to unsigned");
    
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

struct SurfaceVoxelAttribs
{
    Vec3f pos;
    Vec3f norm;
    Vec3f col;
};

void GetSurfaceAttribsForVoxel(
                               FloatGrid::ValueOnCIter iter,
                               Vec3f pos,
                               float min,
                               float max,
                               util::PagedArray<SurfaceVoxelAttribs>::ValueBuffer &attribsBuffer
                               );

void GetSurfaceAttribsForVoxel(
                               FloatGrid::ValueOnCIter iter,
                               Vec3f pos,
                               float min,
                               float max,
                               util::PagedArray<SurfaceVoxelAttribs>::ValueBuffer &attribsBuffer
                               )
{
    SurfaceVoxelAttribs myAttribs;
    myAttribs.pos = pos;
    myAttribs.norm = Vec3f(0);
    float col = math::Abs(*iter) / max;
    if (!math::isFinite(col))
        col = 0.0f;
    myAttribs.col = Vec3f(col);
    attribsBuffer.push_back( myAttribs );
}

Bool UpdateSurface(C4DOpenVDBObject *obj, BaseObject *op, Vector32 userColor)
{
    if (!obj->helper->grid) return false;
    
    StatusSetText("Updating surface");
    
    util::PagedArray<SurfaceVoxelAttribs> attribsArray;
    util::PagedArray<SurfaceVoxelAttribs>::ValueBuffer attribsBufferDummy(attribsArray);//dummy used for initialization
    tbb::enumerable_thread_specific<util::PagedArray<SurfaceVoxelAttribs>::ValueBuffer> attribsPool(attribsBufferDummy);//thread local storage pool of ValueBuffers
    math::Transform xform = obj->helper->grid->transform();
    
    if (obj->helper->grid->getGridClass() == GRID_LEVEL_SET && !obj->helper->grid->metaValue<bool>("Unsigned"))
    {
        FloatGrid::ValueConverter<bool>::Type::Ptr isoMask = tools::extractIsosurfaceMask(*obj->helper->grid, tools::lsutilGridZero<FloatGrid>());
        tools::ScalarToVectorConverter<FloatGrid>::Type::Ptr gradGrid = tools::gradient(*obj->helper->grid, *isoMask);

        tree::IteratorRange<VectorGrid::ValueOnCIter> gradRange(gradGrid->tree().cbeginValueOn());
        tbb::parallel_for(
                          gradRange,
                          [&xform, &attribsPool, &userColor](tree::IteratorRange<VectorGrid::ValueOnCIter>& range) {
                              util::PagedArray<SurfaceVoxelAttribs>::ValueBuffer &attribsBuffer = attribsPool.local();
                              for ( ; range; ++range){
                                  VectorGrid::ValueOnCIter iter = range.iterator();
                                  SurfaceVoxelAttribs myAttribs;
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
        float min,max, bgVal;
        obj->helper->grid->evalMinMax(min, max);
        bgVal = obj->helper->grid->background();
        tbb::parallel_for(
                          floatRange,
                          [&xform, &attribsPool, &min, &max, &bgVal](tree::IteratorRange<FloatGrid::ValueOnCIter>& range) {
                              util::PagedArray<SurfaceVoxelAttribs>::ValueBuffer &attribsBuffer = attribsPool.local();
                              for ( ; range; ++range){
                                  FloatGrid::ValueOnCIter iter = range.iterator();
                                  if (*iter != bgVal && *iter != math::negative(bgVal)) {
                                      if (iter.isVoxelValue())
                                      {
                                          Vec3f pos = xform.indexToWorld( iter.getCoord() );
                                          GetSurfaceAttribsForVoxel(iter, pos, min, max, attribsBuffer);
                                      } else
                                      {
                                          for (int x=0; x<8; x++)
                                          {
                                              for (int y=0; y<8; y++)
                                              {
                                                  for (int z=0; z<8; z++)
                                                  {
                                                      Vec3f pos = xform.indexToWorld( GetVoxelPosFromLeaf(iter, x, y, z) );
                                                      GetSurfaceAttribsForVoxel(iter, pos, min, max, attribsBuffer);
                                                  }
                                              }
                                          }
                                      }
                                      
                                  }
                              }
                          }
                          );
    }
    for (auto i=attribsPool.begin(); i!=attribsPool.end(); ++i) i->flush();
        
    obj->surfaceCnt = Int32(attribsArray.size());
    
    DeleteMem(obj->surfaceAttribs);
    
    obj->surfaceAttribs = NewMem(VoxelAttribs, obj->surfaceCnt);
    
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
    
    GetVDBBBox(obj->helper, &obj->myMp, &obj->myRad);
    
    return true;
}

void ClearSurface(C4DOpenVDBObject *obj, Bool free)
{
    if (obj->helper)
        DeleteObj(obj->helper);
    if (!free)
        obj->helper = NewObj(VDBObjectHelper);
    obj->subBuffer = nullptr;
    obj->surfaceAttribs = nullptr;
    obj->surfaceCnt = 0;
    obj->voxelCnt = 0;
    obj->voxelSize = 0.0f;
    obj->gridClass = C4DOPENVDB_GRID_CLASS_SDF;
    obj->gridName = "None";
    obj->gridType = "None";
    obj->isSlaveOfMaster = false;
    obj->UDF = false;
    obj->prevFacingVector = Vector(0);
}

void GetVDBBBox(VDBObjectHelper *helper, Vector *mp, Vector *rad)
{
    if (!helper->grid)
        return;
    
    math::Transform xform = helper->grid->transform();
    math::CoordBBox bbox = helper->grid->evalActiveVoxelBoundingBox();
    
    Vec3d center = xform.indexToWorld(bbox.getCenter());
    Vec3d min = xform.indexToWorld(bbox.min());
    Vec3d max = xform.indexToWorld(bbox.max());
    
    Float x = (max.x() - min.x()) / 2;
    Float y = (max.y() - min.y()) / 2;
    Float z = (max.z() - min.z()) / 2;
    
    *rad = Vector(x,y,z);
    *mp = Vector(center.x(), center.y(), center.z());
}

Bool MakeShape(VDBObjectHelper *helper, Int32 shape, Float width, Vector center, Float voxelSize, Int32 bandWidth)
{
    StatusSetText("Generating VDB");
    
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
    helper->grid->insertMeta("Int Band Width", FloatMetadata(bandWidth * 0.5));
    helper->grid->insertMeta("Ext Band Width", FloatMetadata(bandWidth * 0.5));
    
    return true;
}

struct VisualizerAttribs { Vec3f pos; float val; };

void GetVisualizerAttribsForVoxelInSlice(
                                         FloatGrid::ValueOnCIter iter,
                                         Vec3f voxelPos,
                                         float minVoxelVal,
                                         float maxVoxelVal,
                                         Int32 axis,
                                         float axisOffsetMin,
                                         float axisOffsetMax,
                                         util::PagedArray<VisualizerAttribs>::ValueBuffer &attribsBuffer
                                         );

void GetVisualizerAttribsForVoxelInSlice(
                                         FloatGrid::ValueOnCIter iter,
                                         Vec3f voxelPos,
                                         float minVoxelVal,
                                         float maxVoxelVal,
                                         Int32 axis,
                                         float axisOffsetMin,
                                         float axisOffsetMax,
                                         util::PagedArray<VisualizerAttribs>::ValueBuffer &attribsBuffer
                                         )
{
    bool isInRange = false;
    
    switch (axis) {
        case PRIM_AXIS_XN:
        case PRIM_AXIS_XP:
            isInRange = (voxelPos.x() > axisOffsetMin && voxelPos.x() < axisOffsetMax);
            break;
            
        case PRIM_AXIS_YN:
        case PRIM_AXIS_YP:
            isInRange = (voxelPos.y() > axisOffsetMin && voxelPos.y() < axisOffsetMax);
            break;
            
        case PRIM_AXIS_ZN:
        case PRIM_AXIS_ZP:
            isInRange = (voxelPos.z() > axisOffsetMin && voxelPos.z() < axisOffsetMax);
            break;
            
        default:
            break;
    }
    
    if (isInRange)
    {
        VisualizerAttribs myAttribs;
        myAttribs.pos = voxelPos;
        if (*iter > 0)
        {
            myAttribs.val = *iter / maxVoxelVal;
        } else
        {
            myAttribs.val = *iter / math::Abs(minVoxelVal);
        }
        if (!math::isFinite(myAttribs.val))
            myAttribs.val = 0.0f;
        attribsBuffer.push_back( myAttribs );
    }
}

Bool UpdateVisualizerSlice(C4DOpenVDBObject *vdb, C4DOpenVDBVisualizer *vis, Int32 axis, Float offset, Gradient *grad)
{
    if (!vdb->helper->grid) return false;
    
    StatusSetText("Slicing VDB");
    
    vis->voxelSize = vdb->helper->grid->voxelSize().x();
    
    util::PagedArray<VisualizerAttribs> attribsArray;
    util::PagedArray<VisualizerAttribs>::ValueBuffer attribsBufferDummy(attribsArray);//dummy used for initialization
    tbb::enumerable_thread_specific<util::PagedArray<VisualizerAttribs>::ValueBuffer> attribsPool(attribsBufferDummy);//thread local storage pool of ValueBuffers
    math::Transform xform = vdb->helper->grid->transform();
    float offsetMin = offset - vis->voxelSize;
    float offsetMax = offset + vis->voxelSize;
    
    tree::IteratorRange<FloatGrid::ValueOnCIter> floatRange(vdb->helper->grid->tree().cbeginValueOn());
    float min,max;
    vdb->helper->grid->evalMinMax(min, max);
    tbb::parallel_for(
                      floatRange,
                      [&axis, &xform, &attribsPool, &min, &max, &offsetMin, &offsetMax](tree::IteratorRange<FloatGrid::ValueOnCIter>& range) {
                          util::PagedArray<VisualizerAttribs>::ValueBuffer &attribsBuffer = attribsPool.local();
                          for ( ; range; ++range)
                          {
                              FloatGrid::ValueOnCIter iter = range.iterator();
                              if (iter.isVoxelValue())
                              {
                                  Vec3f pos = xform.indexToWorld( iter.getCoord() );
                                  GetVisualizerAttribsForVoxelInSlice(iter, pos, min, max, axis, offsetMin, offsetMax, attribsBuffer);
                              } else
                              {
                                  for (int x=0; x<8; x++)
                                  {
                                      for (int y=0; y<8; y++)
                                      {
                                          for (int z=0; z<8; z++)
                                          {
                                              Vec3f pos = xform.indexToWorld( GetVoxelPosFromLeaf(iter, x, y, z) );
                                              GetVisualizerAttribsForVoxelInSlice(iter, pos, min, max, axis, offsetMin, offsetMax, attribsBuffer);
                                          }
                                      }
                                  }
                              }
                              
                          }
                      }
                      );
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
    
    StatusSetText("Generating mesh");
    
    gridCopy = vdb->helper->grid->deepCopy();
    
    tools::volumeToMesh(*gridCopy, points, tris, quads, iso, adapt);
    
    // setup new poly obj
    polyObj = PolygonObject::Alloc(points.size(), (tris.size() + quads.size()));
    polyObj->SetMl(inObject->GetMl());
    
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
        polyArr[x] = CPolygon(quads[x].w(), quads[x].z(), quads[x].y(), quads[x].x());
    }
    
    // set tris
    for (Int32 x = 0; x < tris.size(); x++)
    {
        polyArr[x + quads.size()] = CPolygon(tris[x].z(), tris[x].y(), tris[x].x());
    }
    
    polyObj->SetPhong(true, true, DegToRad(40.0f));
    polyObj->Message(MSG_UPDATE); // update poly obj since point data changed.
    polyObj->InsertUnder(outObject);
    
    return true;
}

struct CombineGridOps {
    static inline void diff(const float& a, const float& b, float& result) {
        result = a - b;
    }
    static inline void invert(const openvdb::FloatGrid::ValueAllIter& iter) {
        iter.setValue(*iter * -1);
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
                 Float aMult,
                 Float bMult,
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
    
    struct MultGrid
    {
        float m;
        MultGrid(float inM): m(inM) {}
        inline void operator()(const FloatGrid::ValueAllIter& iter) const {
            iter.setValue(*iter * m);
        }
    };
    
    StatusSetText("Generating VDB");
    
    for (Int32 x = 0; x < inputs->GetCount(); x++)
    {
        // Set up our loop, only set gridA on the first pass so that we collect the results through the whole heirarchy.
        if (!gridA)
        {
            vdb = inputs->operator[](x)->GetNodeData<C4DOpenVDBObject>();
            if (!vdb || !vdb->helper->grid)
                return false;
            
            gridA = vdb->helper->grid->deepCopy();
            
            UpdateGridTransform(inputs->operator[](x)->GetMl(), *gridA);
            
            if (x+1 < inputs->GetCount())
                x++;
            else
                goto end; // don't return false cause that will be an error and constantly rebuild cache
        }
        
        vdb = inputs->operator[](x)->GetNodeData<C4DOpenVDBObject>();
        if (!vdb || !vdb->helper->grid)
            return false;
        
        gridB = vdb->helper->grid->deepCopy();

        UpdateGridTransform(inputs->operator[](x)->GetMl(), *gridB);
        
        if (aMult != 1)
            tools::foreach(gridA->beginValueAll(), MultGrid(aMult));
        
        if (bMult != 1)
            tools::foreach(gridB->beginValueAll(), MultGrid(bMult));
        
        if (resample != C4DOPENVDB_COMBINE_RESAMPLE_NONE && gridA->constTransform() != gridB->constTransform())
        {
            FloatGrid::Ptr resampleGrid, toMatchGrid;
            switch (resample)
            {
                case C4DOPENVDB_COMBINE_RESAMPLE_B_A:
                    resampleGrid = gridB;
                    toMatchGrid = gridA->deepCopy();
                    break;
                    
                case C4DOPENVDB_COMBINE_RESAMPLE_A_B:
                    resampleGrid = gridA;
                    toMatchGrid = gridB->deepCopy();
                    break;
                    
                case C4DOPENVDB_COMBINE_RESAMPLE_HIGH_LOW:
                    if (gridA->voxelSize()[0] < gridB->voxelSize()[0])
                    {
                        resampleGrid = gridA;
                        toMatchGrid = gridB->deepCopy();
                    }
                    else {
                        resampleGrid = gridB;
                        toMatchGrid = gridA->deepCopy();
                    }
                    break;
                    
                case C4DOPENVDB_COMBINE_RESAMPLE_LOW_HIGH:
                    if (gridA->voxelSize()[0] < gridB->voxelSize()[0])
                    {
                        resampleGrid = gridB;
                        toMatchGrid = gridA->deepCopy();
                    }
                    else {
                        resampleGrid = gridA;
                        toMatchGrid = gridB->deepCopy();
                    }
                    break;
            }
            
            toMatchGrid->newTree(); // erase copied tree, keeping the rest of the grid's settings.
            
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
                    
                case C4DOPENVDB_COMBINE_OP_COPY_A:
                    x = inputs->GetCount(); // don't continue down the heirarchy.
                    break;
                
                case C4DOPENVDB_COMBINE_OP_COPY_B:
                    gridA = gridB;
                    x = inputs->GetCount(); // don't continue down the heirarchy.
                    break;
                    
                case C4DOPENVDB_COMBINE_OP_INVERT_A:
                    tools::foreach(gridA->beginValueAll(), CombineGridOps::invert);
                    x = inputs->GetCount(); // don't continue down the heirarchy.
                    break;
                    
                case C4DOPENVDB_COMBINE_OP_ADD:
                    tools::compSum(*gridA, *gridB);
                    break;
                    
                case C4DOPENVDB_COMBINE_OP_SUBTRACT:
                    gridA->tree().combine(gridB->tree(), CombineGridOps::diff, false);
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
                    gridA->tree().combine(gridB->tree(), CombineGridOps::blend1, false);
                    break;
                    
                case C4DOPENVDB_COMBINE_OP_A_PLUS_NEG_A_X_B:
                    gridA->tree().combine(gridB->tree(), CombineGridOps::blend2, false);
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
                
                default:
                    break;
            }
        }
        catch (Exception &e)
        {
            WarningOutput(e.what());
        }
    }
    
    if (deactivate)
        tools::deactivate(*gridA, gridA->background(), (gridZero + deactivateVal));
    
    if (signedFloodFill && gridA->getGridClass() == GRID_LEVEL_SET)
        tools::signedFloodFill(gridA->tree());
    
    if (prune)
        tools::prune(gridA->tree(), (gridZero + pruneVal));
end:
    obj->helper->grid = gridA;
    return true;
}

struct MeshDataAdapter {
private:
    PolygonObject *mesh;
    const Vector *pointsArr;
    const CPolygon *polyArr;
    math::Transform xform;
    Matrix ml;
public:
    MeshDataAdapter(PolygonObject *in, math::Transform inX){
        mesh = in;
        pointsArr = in->GetPointR();
        polyArr = in->GetPolygonR();
        xform = inX;
        ml = in->GetMl();
    };
    size_t polygonCount() const;        // Total number of polygons
    size_t pointCount() const;          // Total number of points
    size_t vertexCount(size_t n) const; // Vertex count for polygon n
    void getIndexSpacePoint(size_t n, size_t v, openvdb::Vec3d& pos) const; // Return position pos in local grid index space for polygon n and vertex v
};

size_t MeshDataAdapter::polygonCount() const
{
    return size_t(mesh->GetPolygonCount());
}

size_t MeshDataAdapter::pointCount() const
{
    return size_t(mesh->GetPointCount());
}

size_t MeshDataAdapter::vertexCount(size_t n) const
{
    if (polyArr[(int)n].IsTriangle())
        return 3;
    else
        return 4;
}

void MeshDataAdapter::getIndexSpacePoint(size_t n, size_t v, Vec3d &pos) const
{
    Vector c4dPos = pointsArr[ polyArr[(int)n].operator[]((int)v) ];
    c4dPos = ml * c4dPos; // put the point into our generator's space
    pos.operator=( xform.worldToIndex( Vec3d(c4dPos.x, c4dPos.y, c4dPos.z) ) );
}

Bool VDBFromPolygons(VDBObjectHelper *helper,
                     BaseObject *hClone,
                     Float voxelSize,
                     Int32 inBandWidth,
                     Int32 exBandWidth,
                     Bool UDF,
                     Bool fill)
{
    math::Transform::Ptr xform;
    int flags = 0;
    float interior = float(inBandWidth);
    float exterior = float(exBandWidth);
    PolygonObject *poly = static_cast<PolygonObject*>(hClone);
    
    if (!poly)
        return false;
    
    StatusSetText("Generating VDB");
    
    xform = math::Transform::createLinearTransform(voxelSize);
    
    MeshDataAdapter md(poly, *xform);
    
    if (UDF)
        flags |= tools::UNSIGNED_DISTANCE_FIELD;
    if (fill)
        exterior = std::numeric_limits<float>::max();
    
    helper->grid = tools::meshToVolume<FloatGrid>(md, *xform, exterior, interior, flags);
    
    helper->grid->setGridClass(GRID_LEVEL_SET);
    helper->grid->setName("surface");
    helper->grid->insertMeta("Unsigned", BoolMetadata(UDF));
    helper->grid->insertMeta("Int Band Width", FloatMetadata(inBandWidth));
    helper->grid->insertMeta("Ext Band Width", FloatMetadata(exBandWidth));
    
    return true;
}

Bool FilterVDB(VDBObjectHelper *helper, C4DOpenVDBObject *obj, Int32 operation, Int32 filter, Int32 iter, Int32 renorm)
{
    if (!obj->helper->grid)
        return false;
    
    StatusSetText("Filtering VDB");
    
    helper->grid = obj->helper->grid->deepCopy();
    
    if (obj->helper->grid->getGridClass() == GRID_LEVEL_SET)
    {
        tools::LevelSetFilter<FloatGrid> lsFilter(*helper->grid);
        lsFilter.setTemporalScheme(math::TemporalIntegrationScheme::TVD_RK1);
        switch (renorm)
        {
            default:
            case C4DOPENVDB_SMOOTH_RENORM_FIRST:
                lsFilter.setSpatialScheme(math::BiasedGradientScheme::FIRST_BIAS);
                break;
                
            case C4DOPENVDB_SMOOTH_RENORM_SEC:
                lsFilter.setSpatialScheme(math::BiasedGradientScheme::SECOND_BIAS);
                break;
                
            case C4DOPENVDB_SMOOTH_RENORM_FIFTH:
                lsFilter.setSpatialScheme(math::BiasedGradientScheme::HJWENO5_BIAS);
                break;
        }
        for (Int32 x = 0; x < iter; x++)
        {
            switch (operation) {
                case C4DOPENVDB_SMOOTH_OP_MEAN: lsFilter.mean(filter); break;
                case C4DOPENVDB_SMOOTH_OP_GAUS: lsFilter.gaussian(filter); break;
                case C4DOPENVDB_SMOOTH_OP_MED: lsFilter.median(filter); break;
                case C4DOPENVDB_SMOOTH_OP_MEANFLOW: lsFilter.meanCurvature(); break;
                case C4DOPENVDB_SMOOTH_OP_LAP: lsFilter.laplacian(); break;
                case C4DOPENVDB_RESHAPE_OP_DILATE: lsFilter.offset(float(-1) * filter); break;
                case C4DOPENVDB_RESHAPE_OP_ERODE: lsFilter.offset(filter); break;
                case C4DOPENVDB_RESHAPE_OP_OPEN: lsFilter.offset(float(-1) * filter); lsFilter.offset(filter); break;
                case C4DOPENVDB_RESHAPE_OP_CLOSE: lsFilter.offset(filter); lsFilter.offset(float(-1) * filter); break;
                default: break;
            }
        }
    }
    else
    {
        tools::Filter<FloatGrid> vdbFilter(*helper->grid);
        switch (operation) {
            case C4DOPENVDB_SMOOTH_OP_MEAN: vdbFilter.mean(filter, iter); break;
            case C4DOPENVDB_SMOOTH_OP_GAUS: vdbFilter.gaussian(filter, iter); break;
            case C4DOPENVDB_SMOOTH_OP_MED: vdbFilter.median(filter, iter); break;
            default: break;
        }
    }
    
    return true;
}

// This wrapper class is required by tools::ParticlesToLeveSet
class ParticleList
{
private:
    maxon::BaseArray<Vector> *pointList;
    maxon::BaseArray<Float>  *radList;
    maxon::BaseArray<Vector> *velList;
    Float velScale;
public:
    // Required by @c openvdb::tools::PointPartitioner
    typedef openvdb::Vec3R  PosType;
    
    ParticleList(maxon::BaseArray<Vector> *inPointList,
                 maxon::BaseArray<Float> *inRadList,
                 maxon::BaseArray<Vector> *inVelList,
                 Float inVelScale)
    {
        pointList = inPointList;
        radList = inRadList;
        velList = inVelList;
        velScale = inVelScale;
    }
    
    // required by tools::ParticlesToLevelSet
    size_t size() const { return size_t(pointList->GetCount()); }
    
    // Position of the particle in world space
    // Required by ParticlesToLevelSet::rasterizeSpheres(*this, radius)
    void getPos(size_t n, Vec3R &xyz) const
    {
        Vector wsPos = pointList->operator[](Int32(n));
        xyz = Vec3R(wsPos.x, wsPos.y, wsPos.z);
    }
    
    // Position and radius of the particle in world space
    // Required by ParticlesToLevelSet::rasterizeSpheres(*this)
    void getPosRad(size_t n, Vec3R &xyz, Real &rad) const
    {
        Vector wsPos = pointList->operator[](Int32(n));
        xyz = Vec3R(wsPos.x, wsPos.y, wsPos.z);
        rad = Real(radList->operator[](Int32(n)));
    }
    
    // Position, radius, and velocity of the particle in world space
    // Required by ParticlesToLevelSet::rasterizeTrails(*this)
    void getPosRadVel(size_t n, Vec3R &xyz, Real &rad, Vec3R &vel) const
    {
        Vector wsPos = pointList->operator[](Int32(n));
        Vector v = velList->operator[](Int32(n));
        v = v.GetNormalized() * velScale;
        xyz = Vec3R(wsPos.x, wsPos.y, wsPos.z);
        rad = Real(radList->operator[](Int32(n)));
        vel = Vec3R(v.x, v.y, v.z);
    }
};

Bool VDBFromParticles(VDBObjectHelper *helper,
                   maxon::BaseArray<Vector> *pointList,
                   maxon::BaseArray<Float> *radList,
                   maxon::BaseArray<Vector> *velList,
                   Float voxelSize,
                   Int32 bandWidth,
                   Bool prune,
                   Int32 shape,
                   Float velMult,
                   Float velSpace,
                   Float minRadius)
{
    if (helper->grid)
        helper->grid.reset();
    
    StatusSetText("Generating VDB");
    
    float bgValue = float(voxelSize * bandWidth); // formula for computing BG Values
    helper->grid = FloatGrid::create(bgValue);
    math::Transform::Ptr xform = math::Transform::createLinearTransform(voxelSize);
    helper->grid->setTransform(xform);
    helper->grid->setGridClass(GRID_LEVEL_SET);
    tools::ParticlesToLevelSet<FloatGrid> ptls(*helper->grid);
    
    ptls.setRmin(minRadius);
    
    ParticleList particleList(pointList, radList, velList, velMult);
    
    if (shape == C4DOPENVDB_FROMPARTICLES_SHAPE_VEL)
    {
        if (velList->GetCount() == 0)
            return false;
        ptls.rasterizeTrails(particleList, velSpace);
    }
    else
    {
        ptls.rasterizeSpheres(particleList);
    }
    
    ptls.finalize(prune);
    
    helper->grid->setName("surface");
    helper->grid->insertMeta("Unsigned", BoolMetadata(false));
    helper->grid->insertMeta("Int Band Width", FloatMetadata(bandWidth));
    helper->grid->insertMeta("Ext Band Width", FloatMetadata(bandWidth));
    
    return true;
}





#include "mario.h"
#include "plugin.h"
#include "CHud.h"
#include "CCamera.h"
#include "CPlayerPed.h"
#include "CWorld.h"
#include "CGame.h"
#include "CEntryExitManager.h"

#define _USE_MATH_DEFINES
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>

extern "C" {
    #include <decomp/include/PR/ultratypes.h>
    #include <decomp/include/audio_defines.h>
    #include <decomp/include/surface_terrains.h>
    #include <decomp/include/sm64shared.h>
}

#include "d3d9_funcs.h"
#include "main.h"

#define lerp(a, b, amnt) a + (b - a) * amnt
#define MAX_OBJS 512

struct LoadedSurface
{
    LoadedSurface() : ID(0), transform(), ent(nullptr), cachedPos(), cachedRotations{0,0,0} {}

    uint32_t ID;
    SM64ObjectTransform transform;
    CEntity* ent;
    CVector cachedPos;
    float cachedRotations[3];
};

LoadedSurface loadedBuildings[MAX_OBJS];
LoadedSurface loadedObjects[MAX_OBJS];

SM64MarioState marioState;
SM64MarioInputs marioInput;
SM64MarioGeometryBuffers marioGeometry;

CVector marioLastPos, marioCurrPos, marioInterpPos, marioBlocksPos;
RwIm3DVertex marioInterpGeo[SM64_GEO_MAX_TRIANGLES * 3];
RwIm3DVertex marioCurrGeoPos[SM64_GEO_MAX_TRIANGLES * 3];
RwIm3DVertex marioLastGeoPos[SM64_GEO_MAX_TRIANGLES * 3];
RwImVertexIndex marioTextureIndices[SM64_GEO_MAX_TRIANGLES * 3];
RwUInt32 marioOriginalColor[SM64_GEO_MAX_TRIANGLES * 3];
uint32_t elapsedTicks = 0;
int marioTexturedCount = 0;
int marioId = -1;
float ticks = 0;
bool surfaceDebugger = false;


void marioToggleDebug()
{
    surfaceDebugger = !surfaceDebugger;
}

bool marioSpawned()
{
    return marioId != -1;
}

void deleteBuildings()
{
    for (int i=0; i<MAX_OBJS; i++)
    {
        if (loadedBuildings[i].ent)
        {
            sm64_surface_object_delete(loadedBuildings[i].ID);
            loadedBuildings[i].ent = nullptr;
            loadedBuildings[i].ID = 0;
        }
    }
}

void loadBuildings(const CVector& pos)
{
    deleteBuildings();

    char buf[256];
    marioBlocksPos = pos;

    // look for static GTA surfaces (buildings) nearby
    short foundObjs = 0;
    CEntity* outEntities[MAX_OBJS] = {0};
    CWorld::FindObjectsIntersectingCube(pos-CVector(64,64,64), pos+CVector(64,64,64), &foundObjs, MAX_OBJS, outEntities, true, false, false, false, true);
    //FindObjectsIntersectingCube(CVector const& cornerA, CVector const& cornerB, short* outCount, short maxCount, CEntity** outEntities, bool buildings, bool vehicles, bool peds, bool objects, bool dummies);
    printf("%d foundObjs\n", foundObjs);
    fflush(stdout);

    for (short i=0; i<foundObjs; i++)
    {
        CCollisionData* colData = outEntities[i]->GetColModel()->m_pColData;
        if (!colData) continue;

        CVector ePos = outEntities[i]->GetPosition();
        float heading = outEntities[i]->GetHeading();
        float orX = outEntities[i]->GetMatrix()->up.z;
        float orY = outEntities[i]->GetMatrix()->right.z;

        SM64SurfaceObject obj;
        memset(&obj, 0, sizeof(SM64SurfaceObject));
        obj.transform.position[0] = ePos.x/MARIO_SCALE;
        obj.transform.position[1] = ePos.z/MARIO_SCALE;
        obj.transform.position[2] = -ePos.y/MARIO_SCALE;
        obj.transform.eulerRotation[0] = -orX * 180.f / M_PI;
        obj.transform.eulerRotation[1] = -heading * 180.f / M_PI;
        obj.transform.eulerRotation[2] = -orY * 180.f / M_PI;

        for (uint16_t j=0; j<colData->m_nNumTriangles; j++)
        {
            CVector vertA, vertB, vertC;
            colData->GetTrianglePoint(vertA, colData->m_pTriangles[j].m_nVertA);
            colData->GetTrianglePoint(vertB, colData->m_pTriangles[j].m_nVertB);
            colData->GetTrianglePoint(vertC, colData->m_pTriangles[j].m_nVertC);
            vertA /= MARIO_SCALE; vertB /= MARIO_SCALE; vertC /= MARIO_SCALE;

            obj.surfaceCount++;
            obj.surfaces = (SM64Surface*)realloc(obj.surfaces, sizeof(SM64Surface) * obj.surfaceCount);

            obj.surfaces[obj.surfaceCount-1].vertices[0][0] = vertC.x;  obj.surfaces[obj.surfaceCount-1].vertices[0][1] = vertC.z;  obj.surfaces[obj.surfaceCount-1].vertices[0][2] = -vertC.y;
            obj.surfaces[obj.surfaceCount-1].vertices[1][0] = vertB.x;  obj.surfaces[obj.surfaceCount-1].vertices[1][1] = vertB.z;	obj.surfaces[obj.surfaceCount-1].vertices[1][2] = -vertB.y;
            obj.surfaces[obj.surfaceCount-1].vertices[2][0] = vertA.x;  obj.surfaces[obj.surfaceCount-1].vertices[2][1] = vertA.z;	obj.surfaces[obj.surfaceCount-1].vertices[2][2] = -vertA.y;
        }

        for (uint16_t j=0; j<colData->m_nNumBoxes; j++)
        {
            obj.surfaceCount += 12;
            obj.surfaces = (SM64Surface*)realloc(obj.surfaces, sizeof(SM64Surface) * obj.surfaceCount);

            CVector Min = colData->m_pBoxes[j].m_vecMin;
            CVector Max = colData->m_pBoxes[j].m_vecMax;
            Min /= MARIO_SCALE; Max /= MARIO_SCALE;

            // block ground face
            obj.surfaces[obj.surfaceCount-12].vertices[0][0] = Max.x;	obj.surfaces[obj.surfaceCount-12].vertices[0][1] = Max.z;	obj.surfaces[obj.surfaceCount-12].vertices[0][2] = -Min.y;
            obj.surfaces[obj.surfaceCount-12].vertices[1][0] = Min.x;	obj.surfaces[obj.surfaceCount-12].vertices[1][1] = Max.z;	obj.surfaces[obj.surfaceCount-12].vertices[1][2] = -Max.y;
            obj.surfaces[obj.surfaceCount-12].vertices[2][0] = Min.x;	obj.surfaces[obj.surfaceCount-12].vertices[2][1] = Max.z;	obj.surfaces[obj.surfaceCount-12].vertices[2][2] = -Min.y;

            obj.surfaces[obj.surfaceCount-11].vertices[0][0] = Min.x; 	obj.surfaces[obj.surfaceCount-11].vertices[0][1] = Max.z;	obj.surfaces[obj.surfaceCount-11].vertices[0][2] = -Max.y;
            obj.surfaces[obj.surfaceCount-11].vertices[1][0] = Max.x;	obj.surfaces[obj.surfaceCount-11].vertices[1][1] = Max.z;	obj.surfaces[obj.surfaceCount-11].vertices[1][2] = -Min.y;
            obj.surfaces[obj.surfaceCount-11].vertices[2][0] = Max.x;	obj.surfaces[obj.surfaceCount-11].vertices[2][1] = Max.z;	obj.surfaces[obj.surfaceCount-11].vertices[2][2] = -Max.y;

            // left (Y+)
            obj.surfaces[obj.surfaceCount-10].vertices[0][0] = Min.x;	obj.surfaces[obj.surfaceCount-10].vertices[0][1] = Max.z;	obj.surfaces[obj.surfaceCount-10].vertices[0][2] = -Max.y;
            obj.surfaces[obj.surfaceCount-10].vertices[1][0] = Max.x;	obj.surfaces[obj.surfaceCount-10].vertices[1][1] = Min.z;	obj.surfaces[obj.surfaceCount-10].vertices[1][2] = -Max.y;
            obj.surfaces[obj.surfaceCount-10].vertices[2][0] = Min.x;	obj.surfaces[obj.surfaceCount-10].vertices[2][1] = Min.z;	obj.surfaces[obj.surfaceCount-10].vertices[2][2] = -Max.y;

            obj.surfaces[obj.surfaceCount-9].vertices[0][0] = Max.x; 	obj.surfaces[obj.surfaceCount-9].vertices[0][1] = Min.z;	obj.surfaces[obj.surfaceCount-9].vertices[0][2] = -Max.y;
            obj.surfaces[obj.surfaceCount-9].vertices[1][0] = Min.x;	obj.surfaces[obj.surfaceCount-9].vertices[1][1] = Max.z;	obj.surfaces[obj.surfaceCount-9].vertices[1][2] = -Max.y;
            obj.surfaces[obj.surfaceCount-9].vertices[2][0] = Max.x;	obj.surfaces[obj.surfaceCount-9].vertices[2][1] = Max.z;	obj.surfaces[obj.surfaceCount-9].vertices[2][2] = -Max.y;

            // right (Y-)
            obj.surfaces[obj.surfaceCount-8].vertices[0][0] = Max.x;	obj.surfaces[obj.surfaceCount-8].vertices[0][1] = Max.z;	obj.surfaces[obj.surfaceCount-8].vertices[0][2] = -Min.y;
            obj.surfaces[obj.surfaceCount-8].vertices[1][0] = Min.x;	obj.surfaces[obj.surfaceCount-8].vertices[1][1] = Min.z;	obj.surfaces[obj.surfaceCount-8].vertices[1][2] = -Min.y;
            obj.surfaces[obj.surfaceCount-8].vertices[2][0] = Max.x;	obj.surfaces[obj.surfaceCount-8].vertices[2][1] = Min.z;	obj.surfaces[obj.surfaceCount-8].vertices[2][2] = -Min.y;

            obj.surfaces[obj.surfaceCount-7].vertices[0][0] = Min.x; 	obj.surfaces[obj.surfaceCount-7].vertices[0][1] = Min.z;	obj.surfaces[obj.surfaceCount-7].vertices[0][2] = -Min.y;
            obj.surfaces[obj.surfaceCount-7].vertices[1][0] = Max.x;	obj.surfaces[obj.surfaceCount-7].vertices[1][1] = Max.z;	obj.surfaces[obj.surfaceCount-7].vertices[1][2] = -Min.y;
            obj.surfaces[obj.surfaceCount-7].vertices[2][0] = Min.x;	obj.surfaces[obj.surfaceCount-7].vertices[2][1] = Max.z;	obj.surfaces[obj.surfaceCount-7].vertices[2][2] = -Min.y;

            // back (X+)
            obj.surfaces[obj.surfaceCount-6].vertices[0][0] = Max.x;	obj.surfaces[obj.surfaceCount-6].vertices[0][1] = Max.z;	obj.surfaces[obj.surfaceCount-6].vertices[0][2] = -Min.y;
            obj.surfaces[obj.surfaceCount-6].vertices[1][0] = Max.x;	obj.surfaces[obj.surfaceCount-6].vertices[1][1] = Min.z;	obj.surfaces[obj.surfaceCount-6].vertices[1][2] = -Min.y;
            obj.surfaces[obj.surfaceCount-6].vertices[2][0] = Max.x;	obj.surfaces[obj.surfaceCount-6].vertices[2][1] = Min.z;	obj.surfaces[obj.surfaceCount-6].vertices[2][2] = -Max.y;

            obj.surfaces[obj.surfaceCount-5].vertices[0][0] = Max.x; 	obj.surfaces[obj.surfaceCount-5].vertices[0][1] = Min.z;	obj.surfaces[obj.surfaceCount-5].vertices[0][2] = -Max.y;
            obj.surfaces[obj.surfaceCount-5].vertices[1][0] = Max.x;	obj.surfaces[obj.surfaceCount-5].vertices[1][1] = Max.z;	obj.surfaces[obj.surfaceCount-5].vertices[1][2] = -Max.y;
            obj.surfaces[obj.surfaceCount-5].vertices[2][0] = Max.x;	obj.surfaces[obj.surfaceCount-5].vertices[2][1] = Max.z;	obj.surfaces[obj.surfaceCount-5].vertices[2][2] = -Min.y;

            // front (X-)
            obj.surfaces[obj.surfaceCount-4].vertices[0][0] = Min.x;	obj.surfaces[obj.surfaceCount-4].vertices[0][1] = Max.z;	obj.surfaces[obj.surfaceCount-4].vertices[0][2] = -Max.y;
            obj.surfaces[obj.surfaceCount-4].vertices[1][0] = Min.x;	obj.surfaces[obj.surfaceCount-4].vertices[1][1] = Min.z;	obj.surfaces[obj.surfaceCount-4].vertices[1][2] = -Max.y;
            obj.surfaces[obj.surfaceCount-4].vertices[2][0] = Min.x;	obj.surfaces[obj.surfaceCount-4].vertices[2][1] = Min.z;	obj.surfaces[obj.surfaceCount-4].vertices[2][2] = -Min.y;

            obj.surfaces[obj.surfaceCount-3].vertices[0][0] = Min.x; 	obj.surfaces[obj.surfaceCount-3].vertices[0][1] = Min.z;	obj.surfaces[obj.surfaceCount-3].vertices[0][2] = -Min.y;
            obj.surfaces[obj.surfaceCount-3].vertices[1][0] = Min.x;	obj.surfaces[obj.surfaceCount-3].vertices[1][1] = Max.z;	obj.surfaces[obj.surfaceCount-3].vertices[1][2] = -Min.y;
            obj.surfaces[obj.surfaceCount-3].vertices[2][0] = Min.x;	obj.surfaces[obj.surfaceCount-3].vertices[2][1] = Max.z;	obj.surfaces[obj.surfaceCount-3].vertices[2][2] = -Max.y;

            // block bottom face
            obj.surfaces[obj.surfaceCount-2].vertices[0][0] = Min.x;	obj.surfaces[obj.surfaceCount-2].vertices[0][1] = Min.z;	obj.surfaces[obj.surfaceCount-2].vertices[0][2] = -Min.y;
            obj.surfaces[obj.surfaceCount-2].vertices[1][0] = Max.x;	obj.surfaces[obj.surfaceCount-2].vertices[1][1] = Min.z;	obj.surfaces[obj.surfaceCount-2].vertices[1][2] = -Max.y;
            obj.surfaces[obj.surfaceCount-2].vertices[2][0] = Max.x;	obj.surfaces[obj.surfaceCount-2].vertices[2][1] = Min.z;	obj.surfaces[obj.surfaceCount-2].vertices[2][2] = -Min.y;

            obj.surfaces[obj.surfaceCount-1].vertices[0][0] = Max.x; 	obj.surfaces[obj.surfaceCount-1].vertices[0][1] = Min.z;	obj.surfaces[obj.surfaceCount-1].vertices[0][2] = -Max.y;
            obj.surfaces[obj.surfaceCount-1].vertices[1][0] = Min.x;	obj.surfaces[obj.surfaceCount-1].vertices[1][1] = Min.z;	obj.surfaces[obj.surfaceCount-1].vertices[1][2] = -Min.y;
            obj.surfaces[obj.surfaceCount-1].vertices[2][0] = Min.x;	obj.surfaces[obj.surfaceCount-1].vertices[2][1] = Min.z;	obj.surfaces[obj.surfaceCount-1].vertices[2][2] = -Max.y;
        }

        for (uint32_t j=0; j<obj.surfaceCount; j++)
        {
            obj.surfaces[j].type = SURFACE_DEFAULT;
            obj.surfaces[j].force = 0;
            obj.surfaces[j].terrain = TERRAIN_STONE;
        }

        for (int j=0; j<MAX_OBJS; j++)
        {
            if (loadedBuildings[j].ent) continue;
            loadedBuildings[j].ent = outEntities[i];
            loadedBuildings[j].cachedPos = ePos;
            loadedBuildings[j].cachedRotations[0] = orX;
            loadedBuildings[j].cachedRotations[1] = heading;
            loadedBuildings[j].cachedRotations[2] = orY;
            loadedBuildings[j].transform = obj.transform;
            loadedBuildings[j].ID = sm64_surface_object_create(&obj);
            break;
        }

        if (obj.surfaces) free(obj.surfaces);
    }
}

void deleteNonBuildings()
{
    for (int i=0; i<MAX_OBJS; i++)
    {
        if (loadedObjects[i].ent)
        {
            sm64_surface_object_delete(loadedObjects[i].ID);
            loadedObjects[i].ent = nullptr;
            loadedObjects[i].ID = 0;
        }
    }
}

void loadNonBuildings(const CVector& pos)
{
    deleteNonBuildings();

    // look for non-static GTA surfaces (vehicles, objects, etc) nearby
    short foundObjs = 0;
    CEntity* outEntities[MAX_OBJS] = {0};
    CWorld::FindObjectsIntersectingCube(pos-CVector(16,16,16), pos+CVector(16,16,16), &foundObjs, MAX_OBJS, outEntities, false, true, false, true, false);
    //FindObjectsIntersectingCube(CVector const& cornerA, CVector const& cornerB, short* outCount, short maxCount, CEntity** outEntities, bool buildings, bool vehicles, bool peds, bool objects, bool dummies);
    printf("%d foundObjs (nonBuildings)\n", foundObjs);
    fflush(stdout);

    for (short i=0; i<foundObjs; i++)
    {
        // do not add these models to collisions
        eModelID ignoreList[] = {
            MODEL_BLACKBAG1,
            MODEL_BLACKBAG2,
            MODEL_FIRE_HYDRANT,
            MODEL_PICKUPSAVE
        };

        bool ignore = false;
        for (int j=0; j<sizeof(ignoreList) / sizeof(eModelID); j++)
        {
            if (outEntities[i]->m_nModelIndex == ignoreList[j])
            {
                ignore = true;
                break;
            }
        }
        if (ignore) continue;

        if (outEntities[i]->m_bRemoveFromWorld || !outEntities[i]->m_bIsVisible)
            continue;

        CCollisionData* colData = outEntities[i]->GetColModel()->m_pColData;
        if (!colData) continue;

        CVector ePos = outEntities[i]->GetPosition();
        float heading = outEntities[i]->GetHeading();
        float orX = outEntities[i]->GetMatrix()->up.z;
        float orY = outEntities[i]->GetMatrix()->right.z;

        SM64SurfaceObject obj;
        memset(&obj, 0, sizeof(SM64SurfaceObject));
        obj.transform.position[0] = ePos.x/MARIO_SCALE;
        obj.transform.position[1] = ePos.z/MARIO_SCALE;
        obj.transform.position[2] = -ePos.y/MARIO_SCALE;
        obj.transform.eulerRotation[0] = -orX * 180.f / M_PI;
        obj.transform.eulerRotation[1] = -heading * 180.f / M_PI;
        obj.transform.eulerRotation[2] = -orY * 180.f / M_PI;

        for (uint16_t j=0; j<colData->m_nNumTriangles; j++)
        {
            CVector vertA, vertB, vertC;
            colData->GetTrianglePoint(vertA, colData->m_pTriangles[j].m_nVertA);
            colData->GetTrianglePoint(vertB, colData->m_pTriangles[j].m_nVertB);
            colData->GetTrianglePoint(vertC, colData->m_pTriangles[j].m_nVertC);
            vertA /= MARIO_SCALE; vertB /= MARIO_SCALE; vertC /= MARIO_SCALE;

            obj.surfaceCount++;
            obj.surfaces = (SM64Surface*)realloc(obj.surfaces, sizeof(SM64Surface) * obj.surfaceCount);

            obj.surfaces[obj.surfaceCount-1].vertices[0][0] = vertC.x;  obj.surfaces[obj.surfaceCount-1].vertices[0][1] = vertC.z;  obj.surfaces[obj.surfaceCount-1].vertices[0][2] = -vertC.y;
            obj.surfaces[obj.surfaceCount-1].vertices[1][0] = vertB.x;  obj.surfaces[obj.surfaceCount-1].vertices[1][1] = vertB.z;	obj.surfaces[obj.surfaceCount-1].vertices[1][2] = -vertB.y;
            obj.surfaces[obj.surfaceCount-1].vertices[2][0] = vertA.x;  obj.surfaces[obj.surfaceCount-1].vertices[2][1] = vertA.z;	obj.surfaces[obj.surfaceCount-1].vertices[2][2] = -vertA.y;
        }

        for (uint16_t j=0; j<colData->m_nNumBoxes; j++)
        {
            obj.surfaceCount += 12;
            obj.surfaces = (SM64Surface*)realloc(obj.surfaces, sizeof(SM64Surface) * obj.surfaceCount);

            CVector Min = colData->m_pBoxes[j].m_vecMin;
            CVector Max = colData->m_pBoxes[j].m_vecMax;
            Min /= MARIO_SCALE; Max /= MARIO_SCALE;

            // block ground face
            obj.surfaces[obj.surfaceCount-12].vertices[0][0] = Max.x;	obj.surfaces[obj.surfaceCount-12].vertices[0][1] = Max.z;	obj.surfaces[obj.surfaceCount-12].vertices[0][2] = -Min.y;
            obj.surfaces[obj.surfaceCount-12].vertices[1][0] = Min.x;	obj.surfaces[obj.surfaceCount-12].vertices[1][1] = Max.z;	obj.surfaces[obj.surfaceCount-12].vertices[1][2] = -Max.y;
            obj.surfaces[obj.surfaceCount-12].vertices[2][0] = Min.x;	obj.surfaces[obj.surfaceCount-12].vertices[2][1] = Max.z;	obj.surfaces[obj.surfaceCount-12].vertices[2][2] = -Min.y;

            obj.surfaces[obj.surfaceCount-11].vertices[0][0] = Min.x; 	obj.surfaces[obj.surfaceCount-11].vertices[0][1] = Max.z;	obj.surfaces[obj.surfaceCount-11].vertices[0][2] = -Max.y;
            obj.surfaces[obj.surfaceCount-11].vertices[1][0] = Max.x;	obj.surfaces[obj.surfaceCount-11].vertices[1][1] = Max.z;	obj.surfaces[obj.surfaceCount-11].vertices[1][2] = -Min.y;
            obj.surfaces[obj.surfaceCount-11].vertices[2][0] = Max.x;	obj.surfaces[obj.surfaceCount-11].vertices[2][1] = Max.z;	obj.surfaces[obj.surfaceCount-11].vertices[2][2] = -Max.y;

            // left (Y+)
            obj.surfaces[obj.surfaceCount-10].vertices[0][0] = Min.x;	obj.surfaces[obj.surfaceCount-10].vertices[0][1] = Max.z;	obj.surfaces[obj.surfaceCount-10].vertices[0][2] = -Max.y;
            obj.surfaces[obj.surfaceCount-10].vertices[1][0] = Max.x;	obj.surfaces[obj.surfaceCount-10].vertices[1][1] = Min.z;	obj.surfaces[obj.surfaceCount-10].vertices[1][2] = -Max.y;
            obj.surfaces[obj.surfaceCount-10].vertices[2][0] = Min.x;	obj.surfaces[obj.surfaceCount-10].vertices[2][1] = Min.z;	obj.surfaces[obj.surfaceCount-10].vertices[2][2] = -Max.y;

            obj.surfaces[obj.surfaceCount-9].vertices[0][0] = Max.x; 	obj.surfaces[obj.surfaceCount-9].vertices[0][1] = Min.z;	obj.surfaces[obj.surfaceCount-9].vertices[0][2] = -Max.y;
            obj.surfaces[obj.surfaceCount-9].vertices[1][0] = Min.x;	obj.surfaces[obj.surfaceCount-9].vertices[1][1] = Max.z;	obj.surfaces[obj.surfaceCount-9].vertices[1][2] = -Max.y;
            obj.surfaces[obj.surfaceCount-9].vertices[2][0] = Max.x;	obj.surfaces[obj.surfaceCount-9].vertices[2][1] = Max.z;	obj.surfaces[obj.surfaceCount-9].vertices[2][2] = -Max.y;

            // right (Y-)
            obj.surfaces[obj.surfaceCount-8].vertices[0][0] = Max.x;	obj.surfaces[obj.surfaceCount-8].vertices[0][1] = Max.z;	obj.surfaces[obj.surfaceCount-8].vertices[0][2] = -Min.y;
            obj.surfaces[obj.surfaceCount-8].vertices[1][0] = Min.x;	obj.surfaces[obj.surfaceCount-8].vertices[1][1] = Min.z;	obj.surfaces[obj.surfaceCount-8].vertices[1][2] = -Min.y;
            obj.surfaces[obj.surfaceCount-8].vertices[2][0] = Max.x;	obj.surfaces[obj.surfaceCount-8].vertices[2][1] = Min.z;	obj.surfaces[obj.surfaceCount-8].vertices[2][2] = -Min.y;

            obj.surfaces[obj.surfaceCount-7].vertices[0][0] = Min.x; 	obj.surfaces[obj.surfaceCount-7].vertices[0][1] = Min.z;	obj.surfaces[obj.surfaceCount-7].vertices[0][2] = -Min.y;
            obj.surfaces[obj.surfaceCount-7].vertices[1][0] = Max.x;	obj.surfaces[obj.surfaceCount-7].vertices[1][1] = Max.z;	obj.surfaces[obj.surfaceCount-7].vertices[1][2] = -Min.y;
            obj.surfaces[obj.surfaceCount-7].vertices[2][0] = Min.x;	obj.surfaces[obj.surfaceCount-7].vertices[2][1] = Max.z;	obj.surfaces[obj.surfaceCount-7].vertices[2][2] = -Min.y;

            // back (X+)
            obj.surfaces[obj.surfaceCount-6].vertices[0][0] = Max.x;	obj.surfaces[obj.surfaceCount-6].vertices[0][1] = Max.z;	obj.surfaces[obj.surfaceCount-6].vertices[0][2] = -Min.y;
            obj.surfaces[obj.surfaceCount-6].vertices[1][0] = Max.x;	obj.surfaces[obj.surfaceCount-6].vertices[1][1] = Min.z;	obj.surfaces[obj.surfaceCount-6].vertices[1][2] = -Min.y;
            obj.surfaces[obj.surfaceCount-6].vertices[2][0] = Max.x;	obj.surfaces[obj.surfaceCount-6].vertices[2][1] = Min.z;	obj.surfaces[obj.surfaceCount-6].vertices[2][2] = -Max.y;

            obj.surfaces[obj.surfaceCount-5].vertices[0][0] = Max.x; 	obj.surfaces[obj.surfaceCount-5].vertices[0][1] = Min.z;	obj.surfaces[obj.surfaceCount-5].vertices[0][2] = -Max.y;
            obj.surfaces[obj.surfaceCount-5].vertices[1][0] = Max.x;	obj.surfaces[obj.surfaceCount-5].vertices[1][1] = Max.z;	obj.surfaces[obj.surfaceCount-5].vertices[1][2] = -Max.y;
            obj.surfaces[obj.surfaceCount-5].vertices[2][0] = Max.x;	obj.surfaces[obj.surfaceCount-5].vertices[2][1] = Max.z;	obj.surfaces[obj.surfaceCount-5].vertices[2][2] = -Min.y;

            // front (X-)
            obj.surfaces[obj.surfaceCount-4].vertices[0][0] = Min.x;	obj.surfaces[obj.surfaceCount-4].vertices[0][1] = Max.z;	obj.surfaces[obj.surfaceCount-4].vertices[0][2] = -Max.y;
            obj.surfaces[obj.surfaceCount-4].vertices[1][0] = Min.x;	obj.surfaces[obj.surfaceCount-4].vertices[1][1] = Min.z;	obj.surfaces[obj.surfaceCount-4].vertices[1][2] = -Max.y;
            obj.surfaces[obj.surfaceCount-4].vertices[2][0] = Min.x;	obj.surfaces[obj.surfaceCount-4].vertices[2][1] = Min.z;	obj.surfaces[obj.surfaceCount-4].vertices[2][2] = -Min.y;

            obj.surfaces[obj.surfaceCount-3].vertices[0][0] = Min.x; 	obj.surfaces[obj.surfaceCount-3].vertices[0][1] = Min.z;	obj.surfaces[obj.surfaceCount-3].vertices[0][2] = -Min.y;
            obj.surfaces[obj.surfaceCount-3].vertices[1][0] = Min.x;	obj.surfaces[obj.surfaceCount-3].vertices[1][1] = Max.z;	obj.surfaces[obj.surfaceCount-3].vertices[1][2] = -Min.y;
            obj.surfaces[obj.surfaceCount-3].vertices[2][0] = Min.x;	obj.surfaces[obj.surfaceCount-3].vertices[2][1] = Max.z;	obj.surfaces[obj.surfaceCount-3].vertices[2][2] = -Max.y;

            // block bottom face
            obj.surfaces[obj.surfaceCount-2].vertices[0][0] = Min.x;	obj.surfaces[obj.surfaceCount-2].vertices[0][1] = Min.z;	obj.surfaces[obj.surfaceCount-2].vertices[0][2] = -Min.y;
            obj.surfaces[obj.surfaceCount-2].vertices[1][0] = Max.x;	obj.surfaces[obj.surfaceCount-2].vertices[1][1] = Min.z;	obj.surfaces[obj.surfaceCount-2].vertices[1][2] = -Max.y;
            obj.surfaces[obj.surfaceCount-2].vertices[2][0] = Max.x;	obj.surfaces[obj.surfaceCount-2].vertices[2][1] = Min.z;	obj.surfaces[obj.surfaceCount-2].vertices[2][2] = -Min.y;

            obj.surfaces[obj.surfaceCount-1].vertices[0][0] = Max.x; 	obj.surfaces[obj.surfaceCount-1].vertices[0][1] = Min.z;	obj.surfaces[obj.surfaceCount-1].vertices[0][2] = -Max.y;
            obj.surfaces[obj.surfaceCount-1].vertices[1][0] = Min.x;	obj.surfaces[obj.surfaceCount-1].vertices[1][1] = Min.z;	obj.surfaces[obj.surfaceCount-1].vertices[1][2] = -Min.y;
            obj.surfaces[obj.surfaceCount-1].vertices[2][0] = Min.x;	obj.surfaces[obj.surfaceCount-1].vertices[2][1] = Min.z;	obj.surfaces[obj.surfaceCount-1].vertices[2][2] = -Max.y;
        }

        for (uint32_t j=0; j<obj.surfaceCount; j++)
        {
            obj.surfaces[j].type = SURFACE_DEFAULT;
            obj.surfaces[j].force = 0;
            obj.surfaces[j].terrain = TERRAIN_STONE;
        }

        for (int j=0; j<MAX_OBJS; j++)
        {
            if (loadedObjects[j].ent) continue;
            loadedObjects[j].ent = outEntities[i];
            loadedObjects[j].cachedPos = ePos;
            loadedObjects[j].cachedRotations[0] = orX;
            loadedObjects[j].cachedRotations[1] = heading;
            loadedObjects[j].cachedRotations[2] = orY;
            loadedObjects[j].transform = obj.transform;
            loadedObjects[j].ID = sm64_surface_object_create(&obj);
            break;
        }

        if (obj.surfaces) free(obj.surfaces);
    }
}

void loadCollisions(const CVector& pos)
{
    loadBuildings(pos);
    loadNonBuildings(pos);
}

void marioSetPos(const CVector& pos)
{
    if (!marioSpawned()) return;
    loadCollisions(pos);

    CVector sm64pos(pos.x / MARIO_SCALE, pos.z / MARIO_SCALE, -pos.y / MARIO_SCALE);
    sm64_set_mario_position(marioId, sm64pos.x, sm64pos.y, sm64pos.z);
}

void onWallAttack(uint32_t surfaceObjectID)
{
    if (surfaceObjectID == UINT_MAX) return;

    for (int i=0; i<MAX_OBJS; i++)
    {
        if (!loadedObjects[i].ent || loadedObjects[i].ID != surfaceObjectID)
            continue;
        CPlayerPed* player = FindPlayerPed();
        CVector direction(0,0,0);

        if (loadedObjects[i].ent->m_nType == ENTITY_TYPE_OBJECT)
        {
            CObject* obj = (CObject*)loadedObjects[i].ent;
            obj->m_fHealth -= 500;
            if (obj->m_fHealth <= 0)
            {
                obj->ObjectDamage(1000.f, &marioInterpPos, &direction, player, WEAPON_UNARMED); // directly using this without "health -= 500" instantly destroys object
                sm64_play_sound_global(SOUND_GENERAL_BREAK_BOX);
                //char buf[256];
                //sprintf(buf, "%d: %d", i, obj->m_nModelIndex);
                //CHud::SetHelpMessage(buf, false,false,false);
            }
        }
        break;
    }
}

void marioSpawn()
{
    if (marioSpawned() || !FindPlayerPed()) return;
    char buf[256];

    // SM64 <--> GTA SA coordinates translation:
    // Y and Z coordinates must be swapped. SM64 up coord is Y+ and GTA SA is Z+
    // Mario model must also be unmirrored by making SM64 Z coordinate / GTA-SA Y coordinate negative
    // GTA SA -> SM64: divide scale
    // SM64 -> GTA SA: multiply scale
    CVector pos = FindPlayerPed()->GetPosition();
    pos.z -= 1;

    loadCollisions(pos);

    CVector sm64pos(pos.x / MARIO_SCALE, pos.z / MARIO_SCALE, -pos.y / MARIO_SCALE);
    marioId = sm64_mario_create(sm64pos.x, sm64pos.y, sm64pos.z);
    if (!marioSpawned())
    {
        sprintf(buf, "Failed to spawn Mario at %.2f %.2f %.2f", pos.x, pos.y, pos.z);
        CHud::SetHelpMessage(buf, false, false, true);
        return;
    }

    marioLastPos = pos;
    marioCurrPos = pos;
    marioInterpPos = pos;

    ticks = 0;
    elapsedTicks = 0;
    marioGeometry.position = new float[9 * SM64_GEO_MAX_TRIANGLES];
    marioGeometry.normal   = new float[9 * SM64_GEO_MAX_TRIANGLES];
    marioGeometry.color    = new float[9 * SM64_GEO_MAX_TRIANGLES];
    marioGeometry.uv       = new float[6 * SM64_GEO_MAX_TRIANGLES];
    marioGeometry.numTrianglesUsed = 0;
    memset(&marioInput, 0, sizeof(marioInput));
    memset(&marioTextureIndices, 0, sizeof(marioTextureIndices));
    memset(&marioOriginalColor, 0, sizeof(marioOriginalColor));
    marioTexturedCount = 0;
    FindPlayerPed()->DeactivatePlayerPed(0);
}

void marioDestroy()
{
    if (!marioSpawned()) return;

    sm64_mario_delete(marioId);
    marioId = -1;

    delete[] marioGeometry.position;
    delete[] marioGeometry.normal;
    delete[] marioGeometry.color;
    delete[] marioGeometry.uv;
    memset(&marioGeometry, 0, sizeof(marioGeometry));
    if (FindPlayerPed()) FindPlayerPed()->ReactivatePlayerPed(0);
}

void marioTick(float dt)
{
    if (!marioSpawned() || !FindPlayerPed()) return;
    CPlayerPed* ped = FindPlayerPed();
    CPad* pad = ped->GetPadFromPlayer();

    // handle entering/exiting buildings
    static CEntryExit* entryexit = nullptr;
    if (CEntryExitManager::mp_Active && !entryexit)
    {
        // entering/exiting a building
        entryexit = CEntryExitManager::mp_Active;
    }
    else if (!CEntryExitManager::mp_Active && entryexit)
    {
        // entered/exited the building, teleport Mario to the destination
        ped->DeactivatePlayerPed(0);
        marioSetPos(entryexit->m_pLink->m_vecExitPos - CVector(0,0,1));
        entryexit = nullptr;
    }

    ticks += dt;
    while (ticks >= 1.f/30)
    {
        ticks -= 1.f/30;
        elapsedTicks++;

        marioTexturedCount = 0;
        memcpy(&marioLastPos, &marioCurrPos, sizeof(marioCurrPos));
        memcpy(marioLastGeoPos, marioCurrGeoPos, sizeof(marioCurrGeoPos));

        // handle input
        float length = sqrtf(pad->GetPedWalkLeftRight() * pad->GetPedWalkLeftRight() + pad->GetPedWalkUpDown() * pad->GetPedWalkUpDown()) / 128.f;
        if (length > 1) length = 1;

        float angle;
        if ((marioState.action & ACT_GROUP_MASK) == ACT_GROUP_SUBMERGED)
            angle = atan2(-pad->GetPedWalkUpDown(), -pad->GetPedWalkLeftRight());
        else
            angle = atan2(pad->GetPedWalkUpDown(), pad->GetPedWalkLeftRight());

        marioInput.stickX = -cosf(angle) * length;
        marioInput.stickY = -sinf(angle) * length;
        marioInput.buttonA = pad->GetJump();
        marioInput.buttonB = pad->GetMeleeAttack();
        marioInput.buttonZ = pad->GetDuck();
        marioInput.camLookX = TheCamera.GetPosition().x/MARIO_SCALE - marioState.position[0];
        marioInput.camLookZ = -TheCamera.GetPosition().y/MARIO_SCALE - marioState.position[2];

        // water level
        sm64_set_mario_water_level(marioId, (CGame::CanSeeWaterFromCurrArea() ? 0 : INT16_MIN));

        // handles loaded objects, vehicles and peds
        for (int i=0; i<MAX_OBJS; i++)
        {
            LoadedSurface& obj = loadedObjects[i];

            if (!obj.ent) continue;
            if (obj.ent->m_bRemoveFromWorld || !obj.ent->m_bIsVisible)
            {
                sm64_surface_object_delete(obj.ID);
                obj.ent = nullptr;
                obj.ID = 0;
                continue;
            }

            CVector ePos = obj.ent->GetPosition();
            float heading = obj.ent->GetHeading();
            float orX = obj.ent->GetMatrix()->up.z;
            float orY = obj.ent->GetMatrix()->right.z;

            if (obj.cachedPos.x != ePos.x || obj.cachedPos.y != ePos.y || obj.cachedPos.z != ePos.z)
            {
                obj.cachedPos = ePos;
                obj.transform.position[0] = ePos.x/MARIO_SCALE;
                obj.transform.position[1] = ePos.z/MARIO_SCALE;
                obj.transform.position[2] = -ePos.y/MARIO_SCALE;
                sm64_surface_object_move(obj.ID, &obj.transform);
            }
            if (obj.cachedRotations[0] != orX || obj.cachedRotations[1] != heading || obj.cachedRotations[2] != orY)
            {
                obj.cachedRotations[0] = orX;
                obj.cachedRotations[1] = heading;
                obj.cachedRotations[2] = orY;
                obj.transform.eulerRotation[0] = -orX * 180.f / M_PI;
                obj.transform.eulerRotation[1] = -heading * 180.f / M_PI;
                obj.transform.eulerRotation[2] = -orY * 180.f / M_PI;
                sm64_surface_object_move(obj.ID, &obj.transform);
            }
        }

        sm64_mario_tick(marioId, &marioInput, &marioState, &marioGeometry);

        marioCurrPos = CVector(marioState.position[0] * MARIO_SCALE, -marioState.position[2] * MARIO_SCALE, marioState.position[1] * MARIO_SCALE);

        for (int i=0; i<marioGeometry.numTrianglesUsed*3; i++)
        {
            bool hasTexture = (marioGeometry.uv[i*2+0] != 1 && marioGeometry.uv[i*2+1] != 1);

            RwUInt32 col = RWRGBALONG((int)(marioGeometry.color[i*3+0]*255), (int)(marioGeometry.color[i*3+1]*255), (int)(marioGeometry.color[i*3+2]*255), 255);
            if (hasTexture)
            {
                marioOriginalColor[marioTexturedCount] = col;
                marioTextureIndices[marioTexturedCount++] = i;
            }
            marioCurrGeoPos[i].color = col;

            marioCurrGeoPos[i].u = marioGeometry.uv[i*2+0];
            marioCurrGeoPos[i].v = marioGeometry.uv[i*2+1];

            marioCurrGeoPos[i].objNormal.x = marioGeometry.normal[i*3+0];
            marioCurrGeoPos[i].objNormal.y = -marioGeometry.normal[i*3+2];
            marioCurrGeoPos[i].objNormal.z = marioGeometry.normal[i*3+1];

            marioCurrGeoPos[i].objVertex.x = marioGeometry.position[i*3+0] * MARIO_SCALE;
            marioCurrGeoPos[i].objVertex.y = -marioGeometry.position[i*3+2] * MARIO_SCALE;
            marioCurrGeoPos[i].objVertex.z = marioGeometry.position[i*3+1] * MARIO_SCALE;
        }

        memcpy(&marioInterpPos, &marioCurrPos, sizeof(marioCurrPos));
        memcpy(marioInterpGeo, marioCurrGeoPos, sizeof(marioCurrGeoPos));

        if (fabsf(marioBlocksPos.x - marioCurrPos.x) > 64 || fabsf(marioBlocksPos.y - marioCurrPos.y) > 64 || fabsf(marioBlocksPos.z - marioCurrPos.z) > 64)
            loadCollisions(marioCurrPos);
        if (elapsedTicks % 30 == 0)
            loadNonBuildings(marioCurrPos);
    }

    marioInterpPos.x = lerp(marioLastPos.x, marioCurrPos.x, ticks / (1./30));
    marioInterpPos.y = lerp(marioLastPos.y, marioCurrPos.y, ticks / (1./30));
    marioInterpPos.z = lerp(marioLastPos.z, marioCurrPos.z, ticks / (1./30));
    ped->SetPosn(marioInterpPos + CVector(0, 0, 0.5f));
    for (int i=0; i<marioGeometry.numTrianglesUsed*3; i++)
    {
        marioInterpGeo[i].objVertex.x = lerp(marioLastGeoPos[i].objVertex.x, marioCurrGeoPos[i].objVertex.x, ticks / (1./30));
        marioInterpGeo[i].objVertex.y = lerp(marioLastGeoPos[i].objVertex.y, marioCurrGeoPos[i].objVertex.y, ticks / (1./30));
        marioInterpGeo[i].objVertex.z = lerp(marioLastGeoPos[i].objVertex.z, marioCurrGeoPos[i].objVertex.z, ticks / (1./30));
    }
}

void marioRender()
{
    if (!marioSpawned()) return;

    RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)1);
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)1);

    if (RwIm3DTransform(marioInterpGeo, SM64_GEO_MAX_TRIANGLES*3, 0, rwIM3D_VERTEXXYZ | rwIM3D_VERTEXRGBA | rwIM3D_VERTEXUV))
    {
        for (int i=0; i<marioTexturedCount; i++) marioInterpGeo[marioTextureIndices[i]].color = marioOriginalColor[i];
        RwD3D9SetTexture(0, 0);
        RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, marioIndices, marioGeometry.numTrianglesUsed*3);

        for (int i=0; i<marioTexturedCount; i++) marioInterpGeo[marioTextureIndices[i]].color = RWRGBALONG(255, 255, 255, 255);
        RwD3D9SetTexture(marioTextureRW, 0);
        RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, (void*)rwFILTERLINEAR);
        RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDSRCALPHA);
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDINVSRCALPHA);
        RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, marioTextureIndices, marioTexturedCount);
    }

#ifdef _DEBUG
    if (surfaceDebugger)
    {
        uint32_t objectCount = 0, vertexCount = 0;
        SM64LoadedSurfaceObject* objects = sm64_get_all_surface_objects(&objectCount);

        for (uint32_t i=0; i<objectCount; i++)
            vertexCount += objects[i].surfaceCount*3;

        RwIm3DVertex surfaceVertices[vertexCount];
        memset(surfaceVertices, 0, sizeof(RwIm3DVertex) * vertexCount);
        uint16_t surfaceIndices[vertexCount];

        uint32_t startInd = 0;

        for (uint32_t i=0; i<objectCount; i++)
        {
            for (uint32_t j=0; j<objects[i].surfaceCount; j++)
            {
                uint8_t r = ((0.5 + 0.25 * 1) * (.5+.5*objects[i].engineSurfaces[j].normal.x)) * 255;
                uint8_t g = ((0.5 + 0.25 * 1) * (.5+.5*objects[i].engineSurfaces[j].normal.y)) * 255;
                uint8_t b = ((0.5 + 0.25 * 1) * (.5+.5*objects[i].engineSurfaces[j].normal.z)) * 255;

                for (int k=0; k<3; k++)
                {
                    surfaceIndices[startInd + j*3+k] = startInd + j*3+k;

                    surfaceVertices[startInd + j*3+k].objNormal.x = objects[i].engineSurfaces[j].normal.x;
                    surfaceVertices[startInd + j*3+k].objNormal.y = objects[i].engineSurfaces[j].normal.y;
                    surfaceVertices[startInd + j*3+k].objNormal.z = objects[i].engineSurfaces[j].normal.z;

                    surfaceVertices[startInd + j*3+k].color = RWRGBALONG(r, g, b, 128);
                }

                surfaceVertices[startInd + j*3+0].objVertex.x = objects[i].engineSurfaces[j].vertex1[0] * MARIO_SCALE;
                surfaceVertices[startInd + j*3+0].objVertex.y = -objects[i].engineSurfaces[j].vertex1[2] * MARIO_SCALE;
                surfaceVertices[startInd + j*3+0].objVertex.z = objects[i].engineSurfaces[j].vertex1[1] * MARIO_SCALE;

                surfaceVertices[startInd + j*3+1].objVertex.x = objects[i].engineSurfaces[j].vertex2[0] * MARIO_SCALE;
                surfaceVertices[startInd + j*3+1].objVertex.y = -objects[i].engineSurfaces[j].vertex2[2] * MARIO_SCALE;
                surfaceVertices[startInd + j*3+1].objVertex.z = objects[i].engineSurfaces[j].vertex2[1] * MARIO_SCALE;

                surfaceVertices[startInd + j*3+2].objVertex.x = objects[i].engineSurfaces[j].vertex3[0] * MARIO_SCALE;
                surfaceVertices[startInd + j*3+2].objVertex.y = -objects[i].engineSurfaces[j].vertex3[2] * MARIO_SCALE;
                surfaceVertices[startInd + j*3+2].objVertex.z = objects[i].engineSurfaces[j].vertex3[1] * MARIO_SCALE;
            }

            startInd += objects[i].surfaceCount*3;
        }

        if (RwIm3DTransform(surfaceVertices, vertexCount, 0, rwIM3D_VERTEXXYZ | rwIM3D_VERTEXRGBA))
        {
            RwD3D9SetTexture(0, 0);
            RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, surfaceIndices, vertexCount);
        }
    }
#endif // _DEBUG

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)0);
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)0);
    RwIm3DEnd();
}

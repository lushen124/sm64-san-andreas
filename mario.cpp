#include "mario.h"
#include "plugin.h"
#include "CHud.h"
#include "CCamera.h"
#include "CPlayerPed.h"
#include "CWorld.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

extern "C" {
    #include <decomp/include/surface_terrains.h>
}

#include "d3d9_funcs.h"
#include "main.h"

#define lerp(a, b, amnt) a + (b - a) * amnt

SM64MarioState marioState;
SM64MarioInputs marioInput;
SM64MarioGeometryBuffers marioGeometry;
CVector marioLastPos, marioCurrPos, marioInterpPos;
RwIm3DVertex marioInterpGeo[SM64_GEO_MAX_TRIANGLES * 3];
RwIm3DVertex marioCurrGeoPos[SM64_GEO_MAX_TRIANGLES * 3];
RwIm3DVertex marioLastGeoPos[SM64_GEO_MAX_TRIANGLES * 3];
RwImVertexIndex marioTextureIndices[SM64_GEO_MAX_TRIANGLES * 3];
RwUInt32 marioOriginalColor[SM64_GEO_MAX_TRIANGLES * 3];
int marioTexturedCount = 0;
int marioId = -1;
float ticks = 0;

bool marioSpawned()
{
    return marioId != -1;
}

void loadCollisions(const CVector& pos)
{
    char buf[256];
    CVector sm64pos(pos.x / MARIO_SCALE, pos.z / MARIO_SCALE, -pos.y / MARIO_SCALE);

    uint32_t surfaceCount = 0;
    SM64Surface* surfaces = 0;
    //SM64Surface* surfaces = (SM64Surface*)malloc(sizeof(SM64Surface) * surfaceCount);

    // spawn a dummy ground at Mario's position
    /*int width = 16384;
    surfaces[surfaceCount-2].vertices[0][0] = sm64pos.x + width;	surfaces[surfaceCount-2].vertices[0][1] = sm64pos.y;	surfaces[surfaceCount-2].vertices[0][2] = sm64pos.z + width;
    surfaces[surfaceCount-2].vertices[1][0] = sm64pos.x - width;	surfaces[surfaceCount-2].vertices[1][1] = sm64pos.y;	surfaces[surfaceCount-2].vertices[1][2] = sm64pos.z - width;
    surfaces[surfaceCount-2].vertices[2][0] = sm64pos.x - width;	surfaces[surfaceCount-2].vertices[2][1] = sm64pos.y;	surfaces[surfaceCount-2].vertices[2][2] = sm64pos.z + width;

    surfaces[surfaceCount-1].vertices[0][0] = sm64pos.x - width;	surfaces[surfaceCount-1].vertices[0][1] = sm64pos.y;	surfaces[surfaceCount-1].vertices[0][2] = sm64pos.z - width;
    surfaces[surfaceCount-1].vertices[1][0] = sm64pos.x + width;	surfaces[surfaceCount-1].vertices[1][1] = sm64pos.y;	surfaces[surfaceCount-1].vertices[1][2] = sm64pos.z + width;
    surfaces[surfaceCount-1].vertices[2][0] = sm64pos.x + width;	surfaces[surfaceCount-1].vertices[2][1] = sm64pos.y;	surfaces[surfaceCount-1].vertices[2][2] = sm64pos.z - width;*/

    // look for static GTA surfaces (buildings) nearby
    short foundObjs = 0;
    short maxObjs = 255;
    CEntity* outEntities[maxObjs] = {0};
    CWorld::FindObjectsIntersectingCube(pos-CVector(64,64,64), pos+CVector(64,64,64), &foundObjs, 255, outEntities, true, false, false, false, false);
    //FindObjectsIntersectingCube(CVector const& cornerA, CVector const& cornerB, short* outCount, short maxCount, CEntity** outEntities, bool buildings, bool vehicles, bool peds, bool objects, bool dummies);
    sprintf(buf, "%d foundObjs", foundObjs);
    CHud::SetHelpMessage(buf, false, false, false);

    for (short i=0; i<foundObjs; i++)
    {
        CCollisionData* colData = outEntities[i]->GetColModel()->m_pColData;
        if (!colData) continue;

        //CVector ePos = outEntities[i]->GetPosition();
        CVector ePos = outEntities[i]->m_placement.m_vPosn;
        for (uint16_t j=0; j<colData->m_nNumTriangles; j++)
        {
            CompressedVector vertA = colData->m_pVertices[colData->m_pTriangles[j].m_nVertA];
            CompressedVector vertB = colData->m_pVertices[colData->m_pTriangles[j].m_nVertB];
            CompressedVector vertC = colData->m_pVertices[colData->m_pTriangles[j].m_nVertC];

            surfaceCount++;
            surfaces = (SM64Surface*)realloc(surfaces, sizeof(SM64Surface) * surfaceCount);

            surfaces[surfaceCount-1].vertices[0][0] = ePos.x/MARIO_SCALE + vertC.x; surfaces[surfaceCount-1].vertices[0][1] = ePos.z/MARIO_SCALE + vertC.z; surfaces[surfaceCount-1].vertices[0][2] = ePos.y/-MARIO_SCALE - vertC.y;
            surfaces[surfaceCount-1].vertices[1][0] = ePos.x/MARIO_SCALE + vertB.x;	surfaces[surfaceCount-1].vertices[1][1] = ePos.z/MARIO_SCALE + vertB.z;	surfaces[surfaceCount-1].vertices[1][2] = ePos.y/-MARIO_SCALE - vertB.y;
            surfaces[surfaceCount-1].vertices[2][0] = ePos.x/MARIO_SCALE + vertA.x;	surfaces[surfaceCount-1].vertices[2][1] = ePos.z/MARIO_SCALE + vertA.z;	surfaces[surfaceCount-1].vertices[2][2] = ePos.y/-MARIO_SCALE - vertA.y;
        }
    }

    for (uint32_t i=0; i<surfaceCount; i++)
    {
      surfaces[i].type = SURFACE_DEFAULT;
      surfaces[i].force = 0;
      surfaces[i].terrain = TERRAIN_STONE;
    }

    sm64_static_surfaces_load(surfaces, surfaceCount);
    free(surfaces);
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

    ticks += dt;
    while (ticks >= 1.f/30)
    {
        ticks -= 1.f/30;

        marioTexturedCount = 0;
        memcpy(&marioLastPos, &marioCurrPos, sizeof(marioCurrPos));
        memcpy(marioLastGeoPos, marioCurrGeoPos, sizeof(marioCurrGeoPos));

        float angle = atan2(pad->GetPedWalkUpDown(), pad->GetPedWalkLeftRight());
        float length = sqrtf(pad->GetPedWalkLeftRight() * pad->GetPedWalkLeftRight() + pad->GetPedWalkUpDown() * pad->GetPedWalkUpDown()) / 128.f;
        if (length > 1) length = 1;

        marioInput.stickX = -cosf(angle) * length;
        marioInput.stickY = -sinf(angle) * length;
        marioInput.buttonA = pad->GetJump();
        marioInput.buttonB = pad->GetMeleeAttack();
        marioInput.buttonZ = pad->GetDuck();
        marioInput.camLookX = TheCamera.GetPosition().x/MARIO_SCALE - marioState.position[0];
        marioInput.camLookZ = -TheCamera.GetPosition().y/MARIO_SCALE - marioState.position[2];

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
    uint32_t surfaceCount = 0;
    SM64SurfaceCollisionData* surfaces = sm64_get_static_surface_data(&surfaceCount);
    RwIm3DVertex surfaceVertices[surfaceCount*3];
    memset(surfaceVertices, 0, sizeof(RwIm3DVertex) * surfaceCount*3);
    uint16_t surfaceIndices[surfaceCount*3];

    for (uint32_t i=0; i<surfaceCount; i++)
    {
        uint8_t r = ((0.5 + 0.25 * 1) * (.5+.5*surfaces[i].normal.x)) * 255;
        uint8_t g = ((0.5 + 0.25 * 1) * (.5+.5*surfaces[i].normal.y)) * 255;
        uint8_t b = ((0.5 + 0.25 * 1) * (.5+.5*surfaces[i].normal.z)) * 255;

        for (int j=0; j<3; j++)
        {
            surfaceIndices[i*3+j] = i*3+j;

            surfaceVertices[i*3+j].objNormal.x = surfaces[i].normal.x;
            surfaceVertices[i*3+j].objNormal.y = surfaces[i].normal.y;
            surfaceVertices[i*3+j].objNormal.z = surfaces[i].normal.z;

            surfaceVertices[i*3+j].color = RWRGBALONG(r, g, b, 128);
        }

        surfaceVertices[i*3+0].objVertex.x = surfaces[i].vertex1[0] * MARIO_SCALE;
        surfaceVertices[i*3+0].objVertex.y = -surfaces[i].vertex1[2] * MARIO_SCALE;
        surfaceVertices[i*3+0].objVertex.z = surfaces[i].vertex1[1] * MARIO_SCALE;

        surfaceVertices[i*3+1].objVertex.x = surfaces[i].vertex2[0] * MARIO_SCALE;
        surfaceVertices[i*3+1].objVertex.y = -surfaces[i].vertex2[2] * MARIO_SCALE;
        surfaceVertices[i*3+1].objVertex.z = surfaces[i].vertex2[1] * MARIO_SCALE;

        surfaceVertices[i*3+2].objVertex.x = surfaces[i].vertex3[0] * MARIO_SCALE;
        surfaceVertices[i*3+2].objVertex.y = -surfaces[i].vertex3[2] * MARIO_SCALE;
        surfaceVertices[i*3+2].objVertex.z = surfaces[i].vertex3[1] * MARIO_SCALE;
    }

    if (RwIm3DTransform(surfaceVertices, surfaceCount*3, 0, rwIM3D_VERTEXXYZ | rwIM3D_VERTEXRGBA))
    {
        RwD3D9SetTexture(0, 0);
        RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, surfaceIndices, surfaceCount*3);
    }
#endif // _DEBUG

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)0);
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)0);
    RwIm3DEnd();
}

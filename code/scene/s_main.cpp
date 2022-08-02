/*
Copyright (c) 2021-2022 Bjarke Damsgaard Eriksen. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

    1. Redistributions of source code must retain the above
       copyright notice, this list of conditions and the
       following disclaimer.

    2. Redistributions in binary form must reproduce the above
       copyright notice, this list of conditions and the following
       disclaimer in the documentation and/or other materials
       provided with the distribution.

    3. Neither the name of the copyright holder nor the names of
       its contributors may be used to endorse or promote products
       derived from this software without specific prior written
       permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "s_private.h"

#define M_SPEED 0.0025f
#define SPEED 0.55f

static void InitializeCamera(CameraState* camera)
{
    camera->eye = { 0.0f, 1.0f, 5.0f };
    camera->at = { 1.0f, 0.0f, 5.0f };
    camera->up = { 0.0f, 1.0f, 0.0f };
    camera->orient = { 0.0f, 0.0f, 0.0f };
    camera->forward = { 0.0f, 0.0f, 1.0f };
    camera->right = { 1.0f, 0.0f, 0.0f };

    camera->eye = { 800.0f, 500.0f, 0.0f };
    //camera->eye = { 800.0f, 1000.0f, 0.0f }; // good for investigating the weird color patches
    camera->orient = { 0.0f, M_PI / 2.0f, 0.0f };
}

void UpdateCamera(Scene* mesh, RenderCommandQueue* cmdQueue, CameraState* cam)
{
    m4x4 cameraO = YRotation(cam->orient.y) * XRotation(-cam->orient.x);

    cam->at = Transform(cameraO, { 0.0f, 0.0f, 1.0f }, 0.0f);
    cam->at = norm(cam->at);

    cam->right = Transform(cameraO, { 1.0f, 0.0f, 0.0f });
    cam->forward = Transform(cameraO, { 0.0f, 0.0f, 1.0f });
    cam->up = cross(cam->forward, cam->right);

    cam->at = cam->eye + cam->at;

    vec3_t aabbLength = mesh->aabb.max - mesh->aabb.min;
    f32 zFar = sqrt(aabbLength.x * aabbLength.x + aabbLength.y * aabbLength.y + aabbLength.z * aabbLength.z);
    f32 zNear = zFar / 1000.0f;
    m4x4 viewMatrix = LookAt(cam->eye, cam->at, cam->up);
    cmdQueue->projectionMatrix = PerspectiveProjection(0.7853982f,
        (f32)r_videoConfig.width / (f32)r_videoConfig.height,
        zFar, zNear);

    cmdQueue->ab.u = cmdQueue->projectionMatrix.E[2][2];
    cmdQueue->ab.v = cmdQueue->projectionMatrix.E[2][3];

    cmdQueue->viewVector = norm(cam->at - cam->eye);
    cmdQueue->cameraPosition = cam->eye;
    cmdQueue->invViewMatrix = Invert(&viewMatrix);
    cmdQueue->modelViewMatrix = Identity() * viewMatrix;
    cmdQueue->invProjectionMatrix = Invert(&cmdQueue->projectionMatrix);
    cmdQueue->invViewProjectionMatrix = Invert(&(cmdQueue->projectionMatrix * cmdQueue->modelViewMatrix));
}

static MemoryPools sceneMemory;

static ScenePersistentState* sceneState;
static SceneTransientState* tranState;
void S_Init(MemoryPools* memory)
{
    SubArena(&sceneMemory.persistent, &memory->persistent, Megabytes(20), "persistent scene arena");
    SubArena(&sceneMemory.transient, &memory->transient, Megabytes(30), "transient scene arena");

    sceneState = PushStruct(&sceneMemory.persistent, ScenePersistentState);
    tranState = PushStruct(&sceneMemory.persistent, SceneTransientState);

    InitializeCamera(&sceneState->camera);
    tranState->editorAssets = AllocateEditorAssets(&sceneMemory.transient, tranState, Megabytes(25));
}


void S_Update(ClientInput* input, RenderCommandQueue* cmdQueue)
{
    CameraState* camera = &sceneState->camera;

    // Handle input
    if (!ImGui::GetIO().WantCaptureMouse && !ImGui::GetIO().WantCaptureKeyboard)
    {
        if (input->mouse.Mouse2.isDown)
        {
            camera->orient.x += M_SPEED * input->mouse.dy;
            camera->orient.y += M_SPEED * input->mouse.dx;
        }

        if (input->mouse.wheel_dt != 0)
        {
            float scale = -input->mouse.wheel_dt * input->dt;
            if (input->keyboard.isDown[0x10])
            {
                scale *= 0.1f;
            }
            camera->eye = camera->eye + camera->forward * scale;
        }

        float moveSpeed = SPEED;
        if (input->mouse.Mouse3.isDown)
        {
            moveSpeed *= 0.1f;
        }

        if (input->keyboard.isDown[KeyCode::Control])
            camera->eye.y -= moveSpeed * input->dt;
        if (input->keyboard.isDown[KeyCode::Space])
            camera->eye.y += moveSpeed * input->dt;
        if (input->keyboard.isDown['W'])
            camera->eye = camera->eye - camera->forward * moveSpeed * input->dt;
        if (input->keyboard.isDown['S'])
            camera->eye = camera->eye + camera->forward * moveSpeed * input->dt;
        if (input->keyboard.isDown['A'])
            camera->eye = camera->eye + camera->right * moveSpeed * input->dt;
        if (input->keyboard.isDown['D'])
            camera->eye = camera->eye - camera->right * moveSpeed * input->dt;

        if (input->keyboard.isDown['H'] && !input->keyboard.wasDown['H'])
        {
            r_backendFlags.drawAABB = !r_backendFlags.drawAABB;
        }
    }

    // push items to the command queue
    // Update information for the gui
    cmdQueue->assets = tranState->editorAssets;

    // Render the object first! (lights are forward rendered atm)
    // @NOTE:
    // Something is wrong when using a SubArena and pushing an object to the renderer
    Scene* loadedMesh = GetLoadedMesh(cmdQueue->assets, 0);
    Scene* loadedMeshLowRes = GetLoadedMesh(cmdQueue->assets, 1);
    R_PushMesh(cmdQueue, loadedMesh, loadedMeshLowRes);

    R_RenderImGui(cmdQueue, cmdQueue->assets, input->dt);

    // Lights
    R_PushBeginDebugRegion(cmdQueue, L"Debug");
    for (u32 i = 0; i < cmdQueue->assets->lights.Length(); ++i)
    {
        assert(i <= ARRAY_LEN(cmdQueue->lights));
        cmdQueue->lights[i] = cmdQueue->assets->lights[i];

        Light* e = &cmdQueue->lights[i];
        if (r_backendFlags.drawLights)
        {
            vec3_t min = e->position - 10.0f;
            vec3_t max = e->position + 10.0f;
            R_PushBox(cmdQueue, e->color, min, max);
        }
    }

    cmdQueue->lightCount = cmdQueue->assets->lights.Length();

    if (r_backendFlags.drawAABB)
    {
        R_PushBox(cmdQueue, { 1.0f, 0.0f, 0.0f, 0.5f }, cmdQueue->aabb.min, cmdQueue->aabb.max);
    }

    R_PushEndDebugRegion(cmdQueue);

    UpdateCamera(GetLoadedMesh(cmdQueue->assets, 0), cmdQueue, camera);
}

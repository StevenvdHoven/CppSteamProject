#ifdef _DEBUG
#include <algorithm>
#include "Viewport.h"
#include <SDL/SDL.h>
#include <iostream>
#include "Time.h"

Viewport::Viewport(SDL_Renderer* renderer, int width, int height) : 
    EditorWindow(),
    Renderer(renderer)
{
    Resize(width, height);
}

Viewport::~Viewport()
{
    if (ViewportTexture)
        SDL_DestroyTexture(ViewportTexture);
}

void Viewport::Resize(int width, int height)
{
    if (width <= 0 || height <= 0)
        return;

    if (ViewportTexture)
        SDL_DestroyTexture(ViewportTexture);

    ViewportTexture = SDL_CreateTexture(
        Renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        width,
        height
    );

    Width = width;
    Height = height;
}

void Viewport::HandleInput()
{
    ImGuiIO& io = ImGui::GetIO();

    Math::Vector2 dir{};
    if (ImGui::IsKeyDown(ImGuiKey_D)) {
        dir.x -= 1.0f;
    }
    if (ImGui::IsKeyDown(ImGuiKey_A)) {
        dir.x += 1.0f;
    }
    if (ImGui::IsKeyDown(ImGuiKey_W)) {
        dir.y += 1.0f;
    }
    if (ImGui::IsKeyDown(ImGuiKey_S)) {
        dir.y -= 1.0f;
    }

    constexpr float flightSpeed = 200.f;
    Offset += dir * flightSpeed * Time::GetDeltaTime();

    Scale += io.MouseWheel;
    Scale = std::clamp(Scale, 0.1f, 20.0f);
}

void Viewport::BeginFrame()
{
    if (!ViewportTexture)
        return;

    SDL_SetRenderTarget(Renderer, ViewportTexture);
    SDL_SetRenderDrawColor(Renderer, 0, 255, 0, 255);
    SDL_RenderClear(Renderer);
}

void Viewport::EndFrame()
{
    SDL_SetRenderTarget(Renderer, nullptr);
}

void Viewport::ImGui_OnWindow()
{
    HandleInput();
    ImGui::Begin("Viewport");
    ImGuizmo::BeginFrame();

    ImGui::BeginGroup();
    if (ImGui::Button("Start Game")) {

    }
    ImGui::SameLine();
    if (ImGui::Button("End Game")) {

    }
    ImGui::EndGroup();

    ImGui::BeginGroup();
    ImVec2 avail = ImGui::GetContentRegionAvail();

    if (avail.x > 1 && avail.y > 1)
    {
        int newW = (int)avail.x;
        int newH = (int)avail.y;

        if (newW != Width || newH != Height)
        {
            Resize(newW, newH);
        }

        if (ViewportTexture)
        {
            ImGui::Image(
                (ImTextureID)ViewportTexture,
                avail
            );
        }
    }
    ImGui::EndGroup();
    ImGui::End();
}

void Viewport::EditTransform(float* cameraView, float* cameraProjection, float* matrix)
{
    static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::ROTATE);
    static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);
    if (ImGui::IsKeyPressed(ImGuiKey_T))
        mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
    if (ImGui::IsKeyPressed(ImGuiKey_E))
        mCurrentGizmoOperation = ImGuizmo::ROTATE;
    if (ImGui::IsKeyPressed(ImGuiKey_R))
        mCurrentGizmoOperation = ImGuizmo::SCALE;
    if (ImGui::RadioButton("Translate", mCurrentGizmoOperation == ImGuizmo::TRANSLATE))
        mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", mCurrentGizmoOperation == ImGuizmo::ROTATE))
        mCurrentGizmoOperation = ImGuizmo::ROTATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", mCurrentGizmoOperation == ImGuizmo::SCALE))
        mCurrentGizmoOperation = ImGuizmo::SCALE;
    float matrixTranslation[3], matrixRotation[3], matrixScale[3];
    ImGuizmo::DecomposeMatrixToComponents(matrix, matrixTranslation, matrixRotation, matrixScale);
    ImGui::InputFloat3("Tr", matrixTranslation);
    ImGui::InputFloat3("Rt", matrixRotation);
    ImGui::InputFloat3("Sc", matrixScale);
    ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, matrix);

    if (mCurrentGizmoOperation != ImGuizmo::SCALE)
    {
        if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL))
            mCurrentGizmoMode = ImGuizmo::LOCAL;
        ImGui::SameLine();
        if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD))
            mCurrentGizmoMode = ImGuizmo::WORLD;
    }
    
    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    ImGuizmo::Manipulate(cameraView, cameraProjection, mCurrentGizmoOperation,
        mCurrentGizmoMode, matrix, NULL, NULL);
}
#endif
#include "Editor/ViewportWindow.h"
#include "Editor/EditorContext.h"
#include "Editor/SceneEditing.h"
#include "Core/Graphics/Renderer.h"
#include "Core/Graphics/SpriteRenderer.h"
#include "Core/World/GameObject.h"
#include "Core/World/Transform.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Flex
{
	namespace
	{
		constexpr float CrossSize = 6.0f;        // screen points, for GameObjects without a sprite
		constexpr float MinZoom = 1.0f / 32.0f;
		constexpr float MaxZoom = 32.0f;

		ImVec2 ToImVec2(const Vector2& value) { return { value.x, value.y }; }
	}

	ViewportWindow::ViewportWindow(EditorContext& context)
		: EditorWindow("Viewport")
		, Context{ context }
	{
	}

	ViewportWindow::~ViewportWindow()
	{
		if (Target && Renderer::IsInitialised()) {
			SDL_DestroyTexture(Target);
		}
	}

	void ViewportWindow::EnsureRenderTarget(int width, int height)
	{
		if (Target && width == TargetWidth && height == TargetHeight) {
			return;
		}
		if (Target) {
			SDL_DestroyTexture(Target);
		}
		Target = SDL_CreateTexture(Renderer::GetSDLRenderer(), SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, width, height);
		TargetWidth = width;
		TargetHeight = height;
		if (Target) {
			SDL_SetTextureScaleMode(Target, SDL_SCALEMODE_NEAREST);
		} else {
			SDL_Log("Viewport: could not create render target: %s", SDL_GetError());
		}
	}

	void ViewportWindow::Draw()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		const bool bVisible = BeginWindow();
		ImGui::PopStyleVar();
		if (!bVisible) {
			EndWindow();
			return;
		}

		const ImVec2 available = ImGui::GetContentRegionAvail();
		const Vector2 size{ std::max(available.x, 1.0f), std::max(available.y, 1.0f) };
		const float pixelScale = ImGui::GetIO().DisplayFramebufferScale.x;
		EnsureRenderTarget(std::max(1, static_cast<int>(size.x * pixelScale)), std::max(1, static_cast<int>(size.y * pixelScale)));

		const ImVec2 imageMin = ImGui::GetCursorScreenPos();
		RenderScene(size, pixelScale);

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		if (Target) {
			drawList->AddImage(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(Target)), imageMin, ImVec2(imageMin.x + size.x, imageMin.y + size.y));
		}

		// One invisible button covers the image: it takes the mouse and is the drop target.
		ImGui::InvisibleButton("##ViewportInput", ToImVec2(size), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
		HandleInput(imageMin, size);

		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetPayloadType)) {
				const Uid assetId = *static_cast<const Uid*>(payload->Data);
				const ImVec2 mouse = ImGui::GetMousePos();
				const Vector2 world = Camera.ScreenToWorld({ mouse.x - imageMin.x, mouse.y - imageMin.y }, size);
				SceneEditing::CreateFromAsset(Context, assetId, Entity{}, world);
			}
			ImGui::EndDragDropTarget();
		}

		DrawOverlay(drawList, imageMin, size);

		// Zoom and help, top-left.
		drawList->AddText(ImVec2(imageMin.x + 8.0f, imageMin.y + 6.0f), IM_COL32(200, 200, 200, 160), "RMB/MMB: pan   Wheel: zoom   F: frame selection");
		char zoomText[32];
		std::snprintf(zoomText, sizeof(zoomText), "%.0f%%", Camera.Zoom * 100.0f);
		drawList->AddText(ImVec2(imageMin.x + 8.0f, imageMin.y + 6.0f + ImGui::GetTextLineHeightWithSpacing()), IM_COL32(200, 200, 200, 160), zoomText);

		EndWindow();
	}

	void ViewportWindow::RenderScene(const Vector2& size, float pixelScale)
	{
		if (!Target) {
			return;
		}
		SDL_Renderer* sdlRenderer = Renderer::GetSDLRenderer();
		SDL_SetRenderTarget(sdlRenderer, Target);
		SDL_SetRenderScale(sdlRenderer, 1.0f, 1.0f);
		SDL_SetRenderViewport(sdlRenderer, nullptr);
		SDL_SetRenderDrawColor(sdlRenderer, 38, 40, 46, 255);
		SDL_RenderClear(sdlRenderer);

		DrawGrid(size, pixelScale);

		CollectSprites(Context.EditorScene.GetWorld(), Items);
		const Matrix2x3 view = Matrix2x3::FromScale({ pixelScale, pixelScale }) * Camera.GetViewMatrix(size);
		Batch.ResetStatistics();
		DrawSprites(Items, Batch, view);
		Batch.Flush();

		SDL_SetRenderTarget(sdlRenderer, nullptr);
	}

	void ViewportWindow::DrawGrid(const Vector2& size, float pixelScale)
	{
		// Grid spacing in world units, a power of two times 32 that stays 24-192 points apart on screen.
		float spacing = 32.0f;
		while (spacing * Camera.Zoom < 24.0f) {
			spacing *= 2.0f;
		}
		while (spacing * Camera.Zoom > 192.0f) {
			spacing *= 0.5f;
		}

		SDL_Renderer* sdlRenderer = Renderer::GetSDLRenderer();
		SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);
		const Vector2 topLeft = Camera.ScreenToWorld({ 0.0f, 0.0f }, size);
		const Vector2 bottomRight = Camera.ScreenToWorld(size, size);

		auto drawLines = [&](bool bVertical) {
			const float first = std::floor((bVertical ? topLeft.x : topLeft.y) / spacing) * spacing;
			const float last = bVertical ? bottomRight.x : bottomRight.y;
			for (float value = first; value <= last; value += spacing) {
				const bool bAxis = std::abs(value) < spacing * 0.01f;
				if (bAxis) {
					SDL_SetRenderDrawColor(sdlRenderer, bVertical ? 90 : 200, bVertical ? 200 : 90, 90, 160);
				} else {
					SDL_SetRenderDrawColor(sdlRenderer, 255, 255, 255, 18);
				}
				const Vector2 screen = Camera.WorldToScreen({ value, value }, size) * pixelScale;
				if (bVertical) {
					SDL_RenderLine(sdlRenderer, screen.x, 0.0f, screen.x, size.y * pixelScale);
				} else {
					SDL_RenderLine(sdlRenderer, 0.0f, screen.y, size.x * pixelScale, screen.y);
				}
			}
		};
		drawLines(true);
		drawLines(false);
	}

	void ViewportWindow::HandleInput(const ImVec2& imageMin, const Vector2& size)
	{
		ImGuiIO& io = ImGui::GetIO();
		const Vector2 mouse{ io.MousePos.x - imageMin.x, io.MousePos.y - imageMin.y };
		const bool bHovered = ImGui::IsItemHovered();
		const bool bActive = ImGui::IsItemActive();

		// Zoom around the mouse.
		if (bHovered && io.MouseWheel != 0.0f) {
			const Vector2 before = Camera.ScreenToWorld(mouse, size);
			Camera.Zoom = std::clamp(Camera.Zoom * std::pow(1.15f, io.MouseWheel), MinZoom, MaxZoom);
			Camera.Position += before - Camera.ScreenToWorld(mouse, size);
		}

		// Pan.
		if (bActive && (ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::IsMouseDown(ImGuiMouseButton_Middle))) {
			Camera.Position -= Vector2{ io.MouseDelta.x, io.MouseDelta.y } / Camera.Zoom;
		}

		// Select, then drag to move.
		World& world = Context.EditorScene.GetWorld();
		if (bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			const Entity picked = Pick(mouse, size);
			if (picked.IsValid()) {
				Context.CurrentSelection.SelectEntity(picked);
				MovingEntity = picked;
				MoveOffset = ComputeWorldMatrix(world, picked).GetTranslation() - Camera.ScreenToWorld(mouse, size);
				MoveStartMouse = mouse;
				bMoving = false;
			} else {
				Context.CurrentSelection.Clear();
				MovingEntity = {};
			}
		}
		if (MovingEntity.IsValid()) {
			if (!world.IsAlive(MovingEntity) || !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
				if (bMoving && world.IsAlive(MovingEntity)) {
					const GameObjectInfo* info = world.GetComponent<GameObjectInfo>(MovingEntity);
					SceneEditing::RecordUndo(Context, "Move " + (info ? info->Name : std::string()));
				}
				MovingEntity = {};
				bMoving = false;
			} else {
				if (!bMoving && Vector2::Distance(mouse, MoveStartMouse) > 3.0f) {
					bMoving = true;
				}
				if (bMoving) {
					SceneEditing::SetWorldPosition(Context, MovingEntity, Camera.ScreenToWorld(mouse, size) + MoveOffset);
				}
			}
		}

		// Keys while the viewport has focus.
		if (ImGui::IsWindowFocused() && !io.WantTextInput) {
			const Entity selected = Context.CurrentSelection.GetEntity();
			if (selected.IsValid() && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
				SceneEditing::Delete(Context, selected);
			}
			if (ImGui::IsKeyPressed(ImGuiKey_F)) {
				FrameSelection();
			}
		}
	}

	Entity ViewportWindow::Pick(const Vector2& screenPosition, const Vector2& size)
	{
		World& world = Context.EditorScene.GetWorld();

		// Crosses of GameObjects without a sprite are drawn on top, so they win.
		Entity picked;
		world.Each<const WorldTransform, const GameObjectInfo>([&](Entity entity, const WorldTransform& worldTransform, const GameObjectInfo&) {
			if (!picked.IsValid() && !world.HasComponent<SpriteRenderer>(entity)) {
				const Vector2 screen = Camera.WorldToScreen(worldTransform.GetPosition(), size);
				if (Vector2::Distance(screen, screenPosition) <= CrossSize + 2.0f) {
					picked = entity;
				}
			}
		});
		if (picked.IsValid()) {
			return picked;
		}

		// Sprites from the top down.
		const Vector2 world2D = Camera.ScreenToWorld(screenPosition, size);
		for (auto it = Items.rbegin(); it != Items.rend(); ++it) {
			if (!world.HasComponent<GameObjectInfo>(it->Owner)) {
				continue;
			}
			const Vector2 local = it->World.Inverse().TransformPoint(world2D);
			if (it->Info.LocalRect.Contains(local)) {
				return it->Owner;
			}
		}
		return {};
	}

	void ViewportWindow::DrawOverlay(ImDrawList* drawList, const ImVec2& imageMin, const Vector2& size)
	{
		World& world = Context.EditorScene.GetWorld();
		const Entity selected = Context.CurrentSelection.GetEntity();
		const ImVec2 clipMax{ imageMin.x + size.x, imageMin.y + size.y };
		drawList->PushClipRect(imageMin, clipMax, true);

		auto toScreen = [&](const Vector2& worldPoint) {
			const Vector2 screen = Camera.WorldToScreen(worldPoint, size);
			return ImVec2(imageMin.x + screen.x, imageMin.y + screen.y);
		};

		// Crosses for GameObjects that have nothing to draw.
		world.Each<const WorldTransform, const GameObjectInfo>([&](Entity entity, const WorldTransform& worldTransform, const GameObjectInfo&) {
			if (world.HasComponent<SpriteRenderer>(entity)) {
				return;
			}
			const ImVec2 center = toScreen(worldTransform.GetPosition());
			const ImU32 color = entity == selected ? IM_COL32(255, 170, 40, 255) : IM_COL32(220, 220, 220, 200);
			drawList->AddLine(ImVec2(center.x - CrossSize, center.y), ImVec2(center.x + CrossSize, center.y), color, 2.0f);
			drawList->AddLine(ImVec2(center.x, center.y - CrossSize), ImVec2(center.x, center.y + CrossSize), color, 2.0f);
		});

		// Outline of the selected sprite.
		for (const SpriteDrawItem& item : Items) {
			if (item.Owner != selected) {
				continue;
			}
			const Rect& rect = item.Info.LocalRect;
			const ImVec2 corners[4] = {
				toScreen(item.World.TransformPoint({ rect.x, rect.y })),
				toScreen(item.World.TransformPoint({ rect.x + rect.width, rect.y })),
				toScreen(item.World.TransformPoint({ rect.x + rect.width, rect.y + rect.height })),
				toScreen(item.World.TransformPoint({ rect.x, rect.y + rect.height })),
			};
			drawList->AddPolyline(corners, 4, IM_COL32(255, 170, 40, 255), ImDrawFlags_Closed, 2.0f);
		}

		drawList->PopClipRect();
	}

	void ViewportWindow::FrameSelection()
	{
		const Entity selected = Context.CurrentSelection.GetEntity();
		if (Context.EditorScene.GetWorld().IsAlive(selected)) {
			Camera.Position = ComputeWorldMatrix(Context.EditorScene.GetWorld(), selected).GetTranslation();
		} else {
			Camera.Position = {};
			Camera.Zoom = 1.0f;
		}
	}
}

#include "Editor/ContentBrowser.h"
#include "Editor/EditorContext.h"
#include "Editor/TextureImporter.h"
#include "Core/Assets/AssetManager.h"
#include "Core/FileSystem.h"
#include "Core/Graphics/Renderer.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>

namespace Flex
{
	namespace
	{
		std::string ToLower(std::string text)
		{
			std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return text;
		}

		std::string GetFolderOf(const AssetInfo& asset)
		{
			return PathToGenericUtf8(asset.Path.parent_path());
		}
	}

	ContentBrowser::ContentBrowser(EditorContext& context)
		: EditorWindow("Content")
		, Context{ context }
	{
	}

	void ContentBrowser::RebuildFolders()
	{
		FoldersRevision = Context.AssetRevision;

		// Every folder on disk (also empty ones), sorted so parents come before children.
		std::map<std::string, std::string> paths; // path -> name
		const std::filesystem::path& root = Context.Assets.GetContentDirectory();
		std::error_code error;
		for (auto it = std::filesystem::recursive_directory_iterator(root, error); !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error)) {
			if (it->is_directory(error)) {
				const std::filesystem::path relative = std::filesystem::relative(it->path(), root, error);
				paths[PathToGenericUtf8(relative)] = PathToUtf8(it->path().filename());
			}
		}

		Folders.clear();
		Folders.push_back({ "", "Content", {} });
		std::map<std::string, size_t> indexByPath{ { "", 0 } };
		for (const auto& [path, name] : paths) {
			const size_t slash = path.find_last_of('/');
			const std::string parentPath = slash == std::string::npos ? std::string() : path.substr(0, slash);
			auto parent = indexByPath.find(parentPath);
			if (parent == indexByPath.end()) {
				continue;
			}
			const size_t index = Folders.size();
			Folders.push_back({ path, name, {} });
			Folders[parent->second].Children.push_back(index);
			indexByPath[path] = index;
		}

		if (!indexByPath.contains(CurrentFolder)) {
			CurrentFolder.clear();
		}
	}

	void ContentBrowser::Draw()
	{
		if (FoldersRevision != Context.AssetRevision) {
			RebuildFolders();
		}

		if (!BeginWindow()) {
			EndWindow();
			return;
		}

		DrawToolbar();

		if (ImGui::BeginTable("ContentLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV, ImVec2(0.0f, 0.0f)))
		{
			ImGui::TableSetupColumn("Folders", ImGuiTableColumnFlags_WidthFixed, 180.0f);
			ImGui::TableSetupColumn("Assets", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableNextColumn();
			if (ImGui::BeginChild("FolderTree")) {
				DrawFolderTree(0);
			}
			ImGui::EndChild();
			ImGui::TableNextColumn();
			if (ImGui::BeginChild("Tiles")) {
				DrawTiles();
			}
			ImGui::EndChild();
			ImGui::EndTable();
		}

		EndWindow();
	}

	void ContentBrowser::DrawToolbar()
	{
		if (ImGui::Button("Refresh") && Context.RefreshAssets) {
			Context.RefreshAssets();
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(220.0f);
		ImGui::InputTextWithHint("##Search", "Search all assets", SearchText, sizeof(SearchText));
		ImGui::SameLine();
		ImGui::SetNextItemWidth(120.0f);
		ImGui::SliderFloat("##TileSize", &TileSize, 40.0f, 160.0f, "Size %.0f");
		ImGui::SameLine();
		ImGui::TextDisabled("Content/%s", CurrentFolder.c_str());

		// Load statistics (kept from the old debug panel): one entry per loaded asset, however many users.
		size_t loadedBytes = 0;
		const auto loadedAssets = AssetManager::GetLoadedAssets();
		for (const auto& loaded : loadedAssets) {
			loadedBytes += loaded->GetMemorySize();
		}
		ImGui::SameLine();
		ImGui::TextDisabled("| %d assets | loaded: %d (%.2f MB) | GPU textures: %d",
			static_cast<int>(Context.Assets.GetAssets().size()), static_cast<int>(loadedAssets.size()),
			loadedBytes / (1024.0 * 1024.0), Renderer::GetLiveTextureCount());
	}

	void ContentBrowser::DrawFolderTree(size_t folderIndex)
	{
		const Folder& folder = Folders[folderIndex];
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
		if (folder.Children.empty()) {
			flags |= ImGuiTreeNodeFlags_Leaf;
		}
		if (folderIndex == 0) {
			flags |= ImGuiTreeNodeFlags_DefaultOpen;
		}
		if (folder.Path == CurrentFolder) {
			flags |= ImGuiTreeNodeFlags_Selected;
		}

		const bool bOpen = ImGui::TreeNodeEx(folder.Path.empty() ? "##ContentRoot" : folder.Path.c_str(), flags, "%s", folder.Name.c_str());
		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
			CurrentFolder = folder.Path;
		}
		if (bOpen) {
			for (size_t child : folder.Children) {
				DrawFolderTree(child);
			}
			ImGui::TreePop();
		}
	}

	bool ContentBrowser::MatchesSearch(const AssetInfo& asset) const
	{
		return ToLower(asset.Name).find(ToLower(SearchText)) != std::string::npos;
	}

	void ContentBrowser::DrawTiles()
	{
		const float scale = ImGui::GetStyle().FontScaleDpi;
		const float tileSize = TileSize * scale;
		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		const int columns = std::max(1, static_cast<int>((ImGui::GetContentRegionAvail().x + spacing) / (tileSize + spacing)));
		const bool bSearching = SearchText[0] != '\0';

		int column = 0;
		auto placeTile = [&](const AssetInfo& asset, bool bIsSubAsset) {
			if (column > 0) {
				ImGui::SameLine();
			}
			DrawTile(asset, tileSize, bIsSubAsset);
			column = (column + 1) % columns;
		};

		// Assets are sorted by path with sub-assets right after their parent.
		for (const AssetInfo& asset : Context.Assets.GetAssets()) {
			if (bSearching) {
				if (MatchesSearch(asset)) {
					placeTile(asset, asset.IsSubAsset());
				}
				continue;
			}
			if (asset.IsSubAsset()) {
				const AssetInfo* parent = Context.Assets.Find(asset.ParentId);
				if (parent && GetFolderOf(*parent) == CurrentFolder && ExpandedTextures.contains(asset.ParentId)) {
					placeTile(asset, true);
				}
			} else if (GetFolderOf(asset) == CurrentFolder) {
				placeTile(asset, false);
			}
		}

		// Clicking empty space clears an asset selection.
		if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered()
			&& Context.CurrentSelection.GetKind() == Selection::Kind::Asset) {
			Context.CurrentSelection.Clear();
		}
	}

	void ContentBrowser::DrawTile(const AssetInfo& asset, float tileSize, bool bIsSubAsset)
	{
		ImGui::PushID(static_cast<int>(asset.Id.GetValue() ^ (asset.Id.GetValue() >> 32)));
		const float textHeight = ImGui::GetTextLineHeight();
		const ImVec2 tileMin = ImGui::GetCursorScreenPos();
		const ImVec2 tileMax{ tileMin.x + tileSize, tileMin.y + tileSize + textHeight + 4.0f };

		// Textures with sprites get an arrow in the top-right corner to show/hide their sprites.
		const bool bHasSprites = !bIsSubAsset && asset.Type == TextureImporter::TypeName && !Context.Assets.GetSubAssets(asset.Id).empty();
		const float arrowSize = ImGui::GetFrameHeight();
		const ImVec2 arrowMin{ tileMax.x - arrowSize, tileMin.y };
		const bool bArrowHovered = bHasSprites && ImGui::IsMouseHoveringRect(arrowMin, ImVec2(tileMax.x, tileMin.y + arrowSize));

		const bool bSelected = Context.CurrentSelection.IsAssetSelected(asset.Id);
		if (ImGui::Selectable("##Tile", bSelected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(tileSize, tileSize + textHeight + 4.0f))) {
			if (bArrowHovered) {
				if (!ExpandedTextures.erase(asset.Id)) {
					ExpandedTextures.insert(asset.Id);
				}
				ImGui::PopID();
				return;
			}
			Context.CurrentSelection.SelectAsset(asset.Id);
			if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && Context.OpenAsset) {
				Context.OpenAsset(asset.Id);
			}
		}
		if (ImGui::BeginDragDropSource()) {
			ImGui::SetDragDropPayload(AssetPayloadType, &asset.Id, sizeof(Uid));
			Context.Thumbnails.Draw(ImGui::GetWindowDrawList(), asset.Id, ImGui::GetCursorScreenPos(),
				ImVec2(ImGui::GetCursorScreenPos().x + 48.0f, ImGui::GetCursorScreenPos().y + 48.0f));
			ImGui::Dummy(ImVec2(48.0f, 48.0f));
			ImGui::TextUnformatted(asset.Name.c_str());
			ImGui::EndDragDropSource();
		}
		if (bArrowHovered) {
			ImGui::SetTooltip(ExpandedTextures.contains(asset.Id) ? "Hide sprites" : "Show sprites");
		} else if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
			ImGui::SetTooltip("%s\n%s | %s", asset.Name.c_str(), asset.Type.c_str(), PathToGenericUtf8(asset.Path).c_str());
		}

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		if (bIsSubAsset) {
			drawList->AddRectFilled(tileMin, tileMax, IM_COL32(255, 255, 255, 12), 4.0f);
		}
		const float padding = 4.0f;
		const ImVec2 imageMin{ tileMin.x + padding, tileMin.y + padding };
		const ImVec2 imageMax{ tileMax.x - padding, tileMin.y + tileSize - padding };
		if (!Context.Thumbnails.Draw(drawList, asset.Id, imageMin, imageMax)) {
			// No preview: show the type instead.
			drawList->AddRectFilled(imageMin, imageMax, IM_COL32(60, 60, 70, 255), 4.0f);
			const ImVec2 typeSize = ImGui::CalcTextSize(asset.Type.c_str());
			drawList->AddText(ImVec2(imageMin.x + (imageMax.x - imageMin.x - typeSize.x) * 0.5f, imageMin.y + (imageMax.y - imageMin.y - typeSize.y) * 0.5f),
				IM_COL32(200, 200, 200, 255), asset.Type.c_str());
		}

		// Name under the image, clipped to the tile.
		drawList->PushClipRect(ImVec2(tileMin.x, tileMin.y), ImVec2(tileMax.x, tileMax.y), true);
		const ImVec2 nameSize = ImGui::CalcTextSize(asset.Name.c_str());
		drawList->AddText(ImVec2(tileMin.x + std::max(0.0f, (tileSize - nameSize.x) * 0.5f), tileMin.y + tileSize), ImGui::GetColorU32(ImGuiCol_Text), asset.Name.c_str());
		drawList->PopClipRect();

		if (bHasSprites) {
			drawList->AddRectFilled(arrowMin, ImVec2(tileMax.x, tileMin.y + arrowSize), bArrowHovered ? IM_COL32(90, 90, 110, 230) : IM_COL32(50, 50, 60, 200), 3.0f);
			ImGui::RenderArrow(drawList, ImVec2(arrowMin.x + arrowSize * 0.2f, arrowMin.y + arrowSize * 0.2f), ImGui::GetColorU32(ImGuiCol_Text),
				ExpandedTextures.contains(asset.Id) ? ImGuiDir_Left : ImGuiDir_Right, 0.8f);
		}
		ImGui::PopID();
	}
}

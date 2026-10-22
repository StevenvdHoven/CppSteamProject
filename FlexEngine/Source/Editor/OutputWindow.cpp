#include "Editor/OutputWindow.h"
#include "Editor/EditorContext.h"

#include <imgui.h>
#include <vector>

namespace Flex
{
	OutputWindow::OutputWindow(EditorContext& context)
		: EditorWindow("Output")
		, Context{ context }
	{
	}

	void OutputWindow::Draw()
	{
		if (!BeginWindow()) {
			EndWindow();
			return;
		}

		if (ImGui::Button("Clear")) {
			Context.Output.Clear();
		}
		ImGui::SameLine();
		ImGui::Checkbox("Auto-scroll", &bAutoScroll);
		ImGui::SameLine();
		ImGui::Checkbox("Errors and warnings only", &bErrorsOnly);
		if (!Context.ScriptJobStatus.empty()) {
			ImGui::SameLine();
			ImGui::TextDisabled("| %s", Context.ScriptJobStatus.c_str());
		}
		ImGui::Separator();

		if (ImGui::BeginChild("##Lines", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar)) {
			const uint64_t revision = Context.Output.GetRevision();
			Context.Output.Read([&](const std::vector<OutputLog::Line>& lines) {
				std::vector<int> visible;
				visible.reserve(lines.size());
				for (int index = 0; index < static_cast<int>(lines.size()); ++index) {
					if (!bErrorsOnly || lines[index].LineLevel == OutputLog::Level::Error || lines[index].LineLevel == OutputLog::Level::Warning) {
						visible.push_back(index);
					}
				}
				ImGuiListClipper clipper;
				clipper.Begin(static_cast<int>(visible.size()));
				while (clipper.Step()) {
					for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
						const OutputLog::Line& line = lines[visible[row]];
						switch (line.LineLevel) {
						case OutputLog::Level::Error:
							ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", line.Text.c_str());
							break;
						case OutputLog::Level::Warning:
							ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.35f, 1.0f), "%s", line.Text.c_str());
							break;
						case OutputLog::Level::Success:
							ImGui::TextColored(ImVec4(0.45f, 0.95f, 0.45f, 1.0f), "%s", line.Text.c_str());
							break;
						case OutputLog::Level::Info:
							ImGui::TextUnformatted(line.Text.c_str());
							break;
						}
					}
				}
			});
			if (bAutoScroll && revision != SeenRevision) {
				ImGui::SetScrollHereY(1.0f);
			}
			SeenRevision = revision;
		}
		ImGui::EndChild();
		EndWindow();
	}
}

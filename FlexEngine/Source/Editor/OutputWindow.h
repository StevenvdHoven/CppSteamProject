#pragma once
#include "Editor/EditorWindow.h"
#include <cstdint>

namespace Flex
{
	struct EditorContext;

	// The "Output" window: build output and engine log lines (EditorContext::Output).
	// Errors are red, warnings yellow, finished builds green.
	class OutputWindow final : public EditorWindow
	{
	public:
		explicit OutputWindow(EditorContext& context);

		void Draw() override;

	private:
		EditorContext& Context;
		bool bAutoScroll = true;
		bool bErrorsOnly = false;
		uint64_t SeenRevision = 0;
	};
}

#pragma once

namespace Flex
{
	// The Inspector's view of an InputReceiver: its map and player, then one row per action of the map
	// with the handlers bound to it (trigger + handler, picked from the FLEX_INPUT_HANDLER functions).
	void RegisterInputReceiverInspector();
}

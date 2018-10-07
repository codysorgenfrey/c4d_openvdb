#include <string.h>
#include "main.h"

//////////////////////////////////////////////////////////////////////////////////////////

Bool PluginStart(void)
{
    if (!InitVDBLib()) return false;
    if (!RegisterC4DOpenVDBPrimitive()) return false;
    if (!RegisterC4DOpenVDBHelp()) return false;
    if (!RegisterC4DOpenVDBVisualizer()) return false;
    if (!RegisterC4DOpenVDBMesher()) return false;
    if (!RegisterC4DOpenVDBCombine()) return false;
    
    return true;
}

void PluginEnd(void)
{
}

Bool PluginMessage(Int32 id, void* data)
{
	switch (id)
	{
		case C4DPL_INIT_SYS:
			if (!resource.Init())
				return false;		// don't start plugin without resource

			return true;

		case C4DMSG_PRIORITY:
			//react to this message to set a plugin priority (to determine in which order plugins are initialized or loaded
			return true;

		case C4DPL_BUILDMENU:
			//react to this message to dynamically enhance the menu
			break;

		case C4DPL_COMMANDLINEARGS:
			//react to this message to react to command line arguments on startup
			break;

		case C4DPL_EDITIMAGE:
			return false;
	}

	return false;
}

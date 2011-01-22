#include <dw.h>
#include "backend.h"

BackendPlugin plugin_list[PLUGIN_MAX];

int load_backend(char *name)
{
	int z, index = -1;
	HMOD hMod;

	/* Find a free plugin entry */
	for(z=0;z<PLUGIN_MAX;z++)
	{
		if(!plugin_list[z].hMod)
		{
			index = z;
			break;
		}
	}
	if(index == -1)
		return -1;

	if(dw_module_load(name, &hMod) == -1)
		return -1;

	if(dw_module_symbol(hMod, "backend_init", (void *)&(plugin_list[index].init)))
		goto Failed;
	if(dw_module_symbol(hMod, "backend_shutdown", (void *)&(plugin_list[index].shutdown)))
		goto Failed;
	if(dw_module_symbol(hMod, "backend_openaccount", (void *)&(plugin_list[index].openaccount)))
		goto Failed;
	if(dw_module_symbol(hMod, "backend_newaccount", (void *)&(plugin_list[index].newaccount)))
		goto Failed;
	if(dw_module_symbol(hMod, "backend_closeaccount", (void *)&(plugin_list[index].closeaccount)))
		goto Failed;
	if(dw_module_symbol(hMod, "backend_getaccounts", (void *)&(plugin_list[index].getaccounts)))
		goto Failed;
	if(dw_module_symbol(hMod, "backend_freeaccounts", (void *)&(plugin_list[index].freeaccounts)))
		goto Failed;
	if(dw_module_symbol(hMod, "backend_newsettings", (void *)&(plugin_list[index].newsettings)))
		goto Failed;
	if(dw_module_symbol(hMod, "backend_getsettings", (void *)&(plugin_list[index].getsettings)))
		goto Failed;
	if(dw_module_symbol(hMod, "backend_getfolders", (void *)&(plugin_list[index].getfolders)))
		goto Failed;
	if(dw_module_symbol(hMod, "backend_newfolder", (void *)&(plugin_list[index].newfolder)))
		goto Failed;
	if(dw_module_symbol(hMod, "backend_delfolder", (void *)&(plugin_list[index].delfolder)))
		goto Failed;
	if(dw_module_symbol(hMod, "backend_getitems", (void *)&(plugin_list[index].getitems)))
		goto Failed;
	if(dw_module_symbol(hMod, "backend_newitem", (void *)&(plugin_list[index].newitem)))
		goto Failed;
	if(dw_module_symbol(hMod, "backend_delitem", (void *)&(plugin_list[index].delitem)))
		goto Failed;
	if(dw_module_symbol(hMod, "backend_getmail", (void *)&(plugin_list[index].getmail)))
		goto Failed;
	if(dw_module_symbol(hMod, "backend_newmail", (void *)&(plugin_list[index].newmail)))
		goto Failed;
	if(dw_module_symbol(hMod, "backend_free", (void *)&(plugin_list[index].free)))
		goto Failed;

	if(!(plugin_list[index].Type = plugin_list[index].init(&(plugin_list[index].Info))))
		goto Failed;

	plugin_list[index].hMod = hMod;

	return 0;

Failed:
	dw_module_close(hMod);
	return -1;
}

// SPDX-License-Identifier: GPL-2.0-or-later
#include <obs-module.h>
#include "folder-dock.hpp"

OBS_DECLARE_MODULE()

namespace {
constexpr const char *DockId = "recording-folder-dock";
QPointer<FolderDock> dock;
}

MODULE_EXPORT const char *obs_module_name(void)
{
	return "Recording Folder Dock";
}

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Choose the recording destination from an OBS dock.";
}

bool obs_module_load(void)
{
	return true;
}

void obs_module_post_load(void)
{
	if (!obs_frontend_get_main_window())
		return;
	dock = new FolderDock;
	if (!obs_frontend_add_dock_by_id(DockId, "Recording Folder", dock.data())) {
		delete dock.data();
		blog(LOG_WARNING, "[recording-folder-dock] Could not register dock");
	}
}

void obs_module_unload(void)
{
	if (dock) {
		obs_frontend_remove_dock(DockId);
		// remove_dock may defer deletion; destroy our widget while module code is loaded.
		delete dock.data();
	}
}

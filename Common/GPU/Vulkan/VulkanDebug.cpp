// Copyright (c) 2016- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#include <string>
#include <sstream>
#include <map>

#include "Common/Log.h"
#include "Common/GPU/Vulkan/VulkanContext.h"
#include "Common/GPU/Vulkan/VulkanDebug.h"

// Used to stop outputting the same message over and over.
// TODO: Add some mechanism to reset this per game you launch.
static std::map<int, int> g_errorCount;

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugUtilsCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT                  messageType,
	const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
	void *pUserData) {
	const VulkanLogOptions *options = (const VulkanLogOptions *)pUserData;
	std::ostringstream message;

	const char *pMessage = pCallbackData->pMessage;

	int messageCode = pCallbackData->messageIdNumber;
	if (messageCode == 101294395) {
		// UNASSIGNED-CoreValidation-Shader-OutputNotConsumed - benign perf warning
		return false;
	}
	if (messageCode == 1303270965) {
		// Benign perf warning, image blit using GENERAL layout.
		// UNASSIGNED
		return false;
	}
	if (messageCode == 606910136 || messageCode == -392708513 || messageCode == -384083808) {
		// VUID-vkCmdDraw-None-02686
		// Kinda false positive, or at least very unnecessary, now that I solved the real issue.
		// See https://github.com/hrydgard/ppsspp/pull/16354
		return false;
	}

	if (g_errorCount[messageCode] > 10) {
		WARN_LOG(G3D, "Too many validation messages with message %d", messageCode);
	}
	g_errorCount[messageCode]++;

	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
		message << "ERROR(";
	} else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		message << "WARNING(";
	} else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
		message << "INFO(";
	} else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
		message << "VERBOSE(";
	}

	if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
		message << "perf";
	} else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
		message << "general";
	} else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
		message << "validation";
	}
	message << ":" << messageCode << ") " << pMessage << "\n";

	std::string msg = message.str();

#ifdef _WIN32
	OutputDebugStringA(msg.c_str());
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
		if (options->breakOnError && IsDebuggerPresent()) {
			DebugBreak();
		}
		if (options->msgBoxOnError) {
			MessageBoxA(NULL, pMessage, "Alert", MB_OK);
		}
	} else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		// Don't break on perf warnings for now, even with a debugger. We log them at least.
		if (options->breakOnWarning && IsDebuggerPresent() && 0 == (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)) {
			DebugBreak();
		}
	}
#endif

	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
		ERROR_LOG(G3D, "VKDEBUG: %s", msg.c_str());
	} else {
		WARN_LOG(G3D, "VKDEBUG: %s", msg.c_str());
	}

	// false indicates that layer should not bail-out of an
	// API call that had validation failures. This may mean that the
	// app dies inside the driver due to invalid parameter(s).
	// That's what would happen without validation layers, so we'll
	// keep that behavior here.
	return false;
}

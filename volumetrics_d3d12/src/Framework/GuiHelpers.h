#pragma once

#include "Core.h"
#include "imgui.h"
#include "imgui_internal.h"


namespace GuiHelpers
{

	// Disables all ImGui items in the scope, can be conditional
	class DisableScope
	{
	public:
		DisableScope(bool condition = true)
			: m_Cond(condition)
		{
			if (m_Cond)
			{
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
			}
		}
		~DisableScope()
		{
			if (m_Cond)
			{
				ImGui::PopItemFlag();
				ImGui::PopStyleVar();
			}
		}

		DISALLOW_COPY(DisableScope)
		DISALLOW_MOVE(DisableScope)

	private:
		bool m_Cond = false;

	};

	// Returns true when the flag was modified
	inline bool FlagOption(UINT* bitfield, const char* name, UINT value)
	{
		ASSERT(bitfield, "Invalid bitfield");
		bool flag = *bitfield & value;
		if (ImGui::Checkbox(name, &flag))
		{
			if (flag)
				*bitfield |= value;
			else
				*bitfield &= ~value;
			return true;
		}
		return false;
	};

}

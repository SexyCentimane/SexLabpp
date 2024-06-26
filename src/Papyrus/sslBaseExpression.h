#pragma once

namespace Papyrus::BaseExpression
{
	float GetModifier(VM* a_vm, StackID a_stackID, RE::StaticFunctionTag*, RE::Actor* ActorRef, uint32_t a_id);
	float GetPhonem(VM* a_vm, StackID a_stackID, RE::StaticFunctionTag*, RE::Actor* ActorRef, uint32_t a_id);
	float GetExpression(VM* a_vm, StackID a_stackID, RE::StaticFunctionTag*, RE::Actor* ActorRef, bool a_getid);

	std::vector<RE::BSFixedString> GetExpressionTags(VM* a_vm, StackID a_stackID, RE::StaticFunctionTag*, RE::BSFixedString a_id);
	void SetExpressionTags(VM* a_vm, StackID a_stackID, RE::StaticFunctionTag*, RE::BSFixedString a_id, std::vector<RE::BSFixedString> a_newtags);
	bool GetEnabled(VM* a_vm, StackID a_stackID, RE::StaticFunctionTag*, RE::BSFixedString a_id);
	void SetEnabled(VM* a_vm, StackID a_stackID, RE::StaticFunctionTag*, RE::BSFixedString a_id, bool a_enabled);
	void RenameExpression(RE::StaticFunctionTag*, RE::BSFixedString a_id, RE::BSFixedString a_newid);
	std::vector<int32_t> GetLevelCounts(VM* a_vm, StackID a_stackID, RE::StaticFunctionTag*, RE::BSFixedString a_id);
	std::vector<float> GetValues(VM* a_vm, StackID a_stackID, RE::StaticFunctionTag*, RE::BSFixedString a_id, bool a_female, int a_level);
	void SetValues(VM* a_vm, StackID a_stackID, RE::StaticFunctionTag*, RE::BSFixedString a_id, bool a_female, int a_level, std::vector<float> a_values);

	inline bool Register(VM* a_vm)
	{
		REGISTERFUNC(GetModifier, "sslBaseExpression", true);
		REGISTERFUNC(GetPhonem, "sslBaseExpression", true);
		REGISTERFUNC(GetExpression, "sslBaseExpression", true);
		REGISTERFUNC(GetExpressionTags, "sslBaseExpression", true);
		REGISTERFUNC(SetExpressionTags, "sslBaseExpression", true);
		REGISTERFUNC(GetEnabled, "sslBaseExpression", true);
		REGISTERFUNC(SetEnabled, "sslBaseExpression", true);
		REGISTERFUNC(RenameExpression, "sslBaseExpression", true);
		REGISTERFUNC(GetLevelCounts, "sslBaseExpression", true);
		REGISTERFUNC(GetValues, "sslBaseExpression", true);
		REGISTERFUNC(SetValues, "sslBaseExpression", true);

		return true;
	}

} // namespace Papyrus::BaseExpression

#include "sslBaseExpression.h"

#include "Registry/Expression.h"

namespace Papyrus::BaseExpression
{
	float GetModifier(VM* a_vm, StackID a_stackID, RE::StaticFunctionTag*, RE::Actor* a_actor, uint32_t a_id)
	{
		if (!a_actor) {
			a_vm->TraceStack("Actor is none", a_stackID);
			return 0.0f;
		}
		const auto data = a_actor->GetFaceGenAnimationData();
		if (!data)
			return 0.0f;
    const auto& keyframe = data->modifierKeyFrame;
		return a_id < keyframe.count ? keyframe.values[a_id] : 0.0f;
	}

	float GetPhonem(VM* a_vm, StackID a_stackID, RE::StaticFunctionTag*, RE::Actor* a_actor, uint32_t a_id)
	{
		if (!a_actor) {
			a_vm->TraceStack("Actor is none", a_stackID);
			return 0.0f;
		}
		const auto data = a_actor->GetFaceGenAnimationData();
		if (!data)
			return 0.0f;
		const auto& keyframe = data->phenomeKeyFrame;
		return a_id < keyframe.count ? keyframe.values[a_id] : 0.0f;
	}

	float GetExpression(VM* a_vm, StackID a_stackID, RE::StaticFunctionTag*, RE::Actor* a_actor, bool a_getid)
	{
		if (!a_actor) {
			a_vm->TraceStack("Actor is none", a_stackID);
			return 0.0f;
		}
		const auto data = a_actor->GetFaceGenAnimationData();
		if (!data)
			return 0.0f;
		const auto& keyframe = data->expressionKeyFrame;
		for (size_t i = 0; i < keyframe.count; i++) {
			if (keyframe.values[i] > 0.0f) {
				return a_getid ? i : keyframe.values[i];
			}
		}
		return 0.0f;
	}

	std::vector<RE::BSFixedString> GetExpressionTags(VM* a_vm, StackID a_stackID, RE::StaticFunctionTag*, RE::BSFixedString a_id)
  {
    auto profile = Registry::Expression::GetSingleton()->GetProfile(a_id);
    if (!profile) {
      a_vm->TraceStack("Invalid Expression Profile ID", a_stackID);
      return {};
    }
    return profile->tags.AsVector();
  }

	void SetExpressionTags(VM* a_vm, StackID a_stackID, RE::StaticFunctionTag*, RE::BSFixedString a_id, std::vector<RE::BSFixedString> a_newtags)
	{
		const auto profile = Registry::Expression::GetSingleton()->GetProfile(a_id);
		if (!profile) {
			a_vm->TraceStack("Invalid Expression Profile ID", a_stackID);
			return;
		}
		profile->tags = { a_newtags };
	}

	bool GetEnabled(VM* a_vm, StackID a_stackID, RE::StaticFunctionTag*, RE::BSFixedString a_id)
  {
		auto profile = Registry::Expression::GetSingleton()->GetProfile(a_id);
		if (!profile) {
			a_vm->TraceStack("Invalid Expression Profile ID", a_stackID);
			return false;
		}
		return profile->enabled;
	}

	void SetEnabled(VM* a_vm, StackID a_stackID, RE::StaticFunctionTag*, RE::BSFixedString a_id, bool a_enabled)
	{
		auto profile = Registry::Expression::GetSingleton()->GetProfile(a_id);
		if (!profile) {
			a_vm->TraceStack("Invalid Expression Profile ID", a_stackID);
			return;
		}
		profile->enabled = a_enabled;
	}

	void RenameExpression(RE::StaticFunctionTag*, RE::BSFixedString a_id, RE::BSFixedString a_newid)
	{
		if (a_id.empty()) {
			if (a_newid.empty()) {
				return;
			}
			Registry::Expression::GetSingleton()->CreateProfile(a_newid);
			return;
		}
		Registry::Expression::GetSingleton()->RenameProfile(a_id, a_newid);
	}

	std::vector<int32_t> GetLevelCounts(VM* a_vm, StackID a_stackID, RE::StaticFunctionTag*, RE::BSFixedString a_id)
	{
		auto profile = Registry::Expression::GetSingleton()->GetProfile(a_id);
		if (!profile) {
			a_vm->TraceStack("Invalid Expression Profile ID", a_stackID);
			return { 0, 0 };
		}
		return { static_cast<int32_t>(profile->data[RE::SEXES::kMale].size()), static_cast<int32_t>(profile->data[RE::SEXES::kFemale].size()) };
	}

	std::vector<float> GetValues(VM* a_vm, StackID a_stackID, RE::StaticFunctionTag*, RE::BSFixedString a_id, bool a_female, int a_level)
	{
		auto profile = Registry::Expression::GetSingleton()->GetProfile(a_id);
		if (!profile) {
			a_vm->TraceStack("Invalid Expression Profile ID", a_stackID);
			return std::vector<float>(Registry::Expression::Profile::Total);
		}
		auto& ret = profile->data[a_female];
		if (ret.size() <= a_level) {
			a_vm->TraceStack("Invalid level", a_stackID);
			return std::vector<float>(Registry::Expression::Profile::Total);
		}
		return { ret[a_level].begin(), ret[a_level].end() };
	}

	void SetValues(VM* a_vm, StackID a_stackID, RE::StaticFunctionTag*, RE::BSFixedString a_id, bool a_female, int a_level, std::vector<float> a_values)
	{
		if (a_values.size() != Registry::Expression::Profile::Total) {
			a_vm->TraceStack("Invalid Value Size", a_stackID);
			return;
		}
		auto profile = Registry::Expression::GetSingleton()->GetProfile(a_id);
		if (!profile) {
			a_vm->TraceStack("Invalid Expression Profile ID", a_stackID);
			return;
		}
		auto& data = profile->data[a_female];
		while (data.size() <= a_level) {
			data.emplace_back();
		}
		std::copy_n(a_values.begin(), data[a_level].size(), data[a_level].begin());
	}

} // namespace Papyrus::BaseExpression

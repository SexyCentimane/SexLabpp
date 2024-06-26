#include "sslThreadModel.h"

#include "Registry/Animation.h"
#include "Registry/Define/Furniture.h"
#include "Registry/Library.h"
#include "Registry/Physics.h"
#include "Registry/Stats.h"
#include "Registry/Util/CellCrawler.h"
#include "Registry/Util/Scale.h"

using Offset = Registry::CoordinateType;

namespace Papyrus::ThreadModel
{
	static inline std::pair<RE::TESObjectREFR*, RE::TESObjectREFR*> GetAliasRefs(RE::TESQuest* a_qst)
	{
		std::pair<RE::TESObjectREFR*, RE::TESObjectREFR*> ret{ nullptr, nullptr };
		a_qst->aliasAccessLock.LockForRead();
		for (auto&& alias : a_qst->aliases) {
			if (!alias)
				continue;
			if (alias->aliasName == "CenterAlias") {
				const auto aliasref = skyrim_cast<RE::BGSRefAlias*>(alias);
				if (aliasref) {
					ret.first = aliasref->GetReference();
				}
			} else {
				const auto aliasref = skyrim_cast<RE::BGSRefAlias*>(alias);
				if (!aliasref)
					continue;
				const auto ref = aliasref->GetReference();
				if (ref && (!ret.second || ref->IsPlayerRef())) {
					ret.second = ref;
				}
			}
		}
		a_qst->aliasAccessLock.UnlockForRead();
		return ret;
	}

	RE::TESObjectREFR* FindCenter(VM* a_vm, StackID a_stackID, RE::TESQuest* a_qst,
		const std::vector<RE::BSFixedString> a_scenes,
		RE::reference_array<RE::BSFixedString> a_out_scenes,
		RE::reference_array<float> a_out_coordinates,
		FurniStatus a_status)
	{
		if (a_scenes.empty()) {
			a_vm->TraceStack("Missing scenes", a_stackID);
			return nullptr;
		}
		if (a_out_scenes.empty()) {
			a_vm->TraceStack("Result scene array must be allocated", a_stackID);
			return nullptr;
		}
		if (a_out_coordinates.size() != 4) {
			a_vm->TraceStack("Result coordinates must have length 4", a_stackID);
			return nullptr;
		}
		const auto [center, actor] = GetAliasRefs(a_qst);
		if (!actor) {
			a_vm->TraceStack("Quest must have some actor positions filled", a_stackID);
			return nullptr;
		}
		const auto library = Registry::Library::GetSingleton();
		std::unordered_map<Registry::FurnitureType, std::vector<const Registry::Scene*>> scene_map{};
		stl::enumeration<Registry::FurnitureType> filled_types;
		for (auto&& sceneid : a_scenes) {
			const auto scene = library->GetSceneByID(sceneid);
			if (!scene) {
				a_vm->TraceStack("Invalid scene id in array", a_stackID);
				return nullptr;
			}
			if (scene->UsesFurniture()) {
				if (a_status == FurniStatus::Disallow) {
					continue;
				}
				filled_types.set(scene->furnitures.furnitures.get());
				const auto types = Registry::FlagToComponents(scene->furnitures.furnitures.get());
				for (auto&& flag : types) {
					auto& it = scene_map[flag];
					if (std::ranges::find(it, scene) == it.end()) {
						it.push_back(scene);
					}
				}
			} else {
				if (scene->furnitures.allowbed && a_status != FurniStatus::Disallow) {
					std::array bedtypes{
						Registry::FurnitureType::BedSingle,
						Registry::FurnitureType::BedDouble,
						Registry::FurnitureType::BedRoll
					};
					for (auto&& type : bedtypes) {
						auto& it = scene_map[type];
						if (std::ranges::find(it, scene) == it.end()) {
							filled_types.set(type);
							it.push_back(scene);
						}
					}
				}
				auto& it = scene_map[Registry::FurnitureType::None];
				if (std::ranges::find(it, scene) == it.end()) {
					it.push_back(scene);
				}
			}
		}
		assert(!scene_map.empty() && std::ranges::find_if(scene_map, [](auto& typevec) { return typevec.second.empty(); }) == scene_map.end());
		const auto ReturnData = [&](Registry::FurnitureType a_where, const Registry::Coordinate& a_coordinate) -> bool {
			if (!scene_map.contains(a_where)) {
				return false;
			}
			const auto& scenes = scene_map[a_where];
			const auto count = std::min<size_t>(a_out_scenes.size(), scenes.size());
			for (size_t i = 0; i < count; i++) {
				a_out_scenes[i] = scenes[i]->id;
			}
			a_coordinate.ToContainer(a_out_coordinates);
			return true;
		};

		if (center) {
			const auto details = library->GetFurnitureDetails(center);
			if (!details) {
				Registry::Coordinate coord{ center };
				return ReturnData(Registry::FurnitureType::None, coord) ? center : nullptr;
			}
			const auto coords = details->GetClosestCoordinateInBound(center, filled_types, actor);
			if (coords.empty()) {
				return nullptr;
			}
			const auto i = Random::draw<size_t>(0, coords.size() - 1);
			return ReturnData(coords[i].first, coords[i].second) ? center : nullptr;
		}
		if (a_status == FurniStatus::Disallow) {
			Registry::Coordinate coord{ actor };
			return ReturnData(Registry::FurnitureType::None, coord) ? actor : nullptr;
		} else if (a_status != FurniStatus::Prefer && !Random::draw<int>(0, Settings::iFurniturePrefWeight)) {
			Registry::Coordinate coord{ actor };
			if (ReturnData(Registry::FurnitureType::None, coord)) {
				return actor;
			}
		}

		std::vector<RE::TESObjectREFR*> used_furnitures{};
		const auto processlist = RE::ProcessLists::GetSingleton();
		for (auto&& ithandle : processlist->highActorHandles) {
			const auto it = ithandle.get();
			if (!it || it.get() == actor || it.get() == center)
				continue;
			const auto furni = it->GetOccupiedFurniture().get();
			if (!furni)
				continue;
			used_furnitures.push_back(furni.get());
		}

		std::vector<std::pair<RE::TESObjectREFR*, const Registry::FurnitureDetails*>> found_objects;
		CellCrawler::ForEachObjectInRange(actor, Settings::fScanRadius, [&](RE::TESObjectREFR* a_ref) {
			if (!a_ref || std::ranges::find(used_furnitures, a_ref) != used_furnitures.end()) {
				return RE::BSContainer::ForEachResult::kContinue;
			}
			const auto details = library->GetFurnitureDetails(a_ref);
			if (!details || !details->HasType(scene_map, [](auto& it) { return it.first; })) {
				return RE::BSContainer::ForEachResult::kContinue;
			}
			found_objects.push_back(std::make_pair(a_ref, details));
			return RE::BSContainer::ForEachResult::kContinue;
		});
		std::vector<std::tuple<Registry::FurnitureType, Registry::Coordinate, RE::TESObjectREFR*>> coords{};
		for (auto&& [ref, details] : found_objects) {
			const auto res = details->GetClosestCoordinateInBound(ref, filled_types, actor);
			for (auto&& pair : res) {
				coords.push_back(std::make_tuple(pair.first, pair.second, ref));
			}
		}
		if (!coords.empty()) {
			std::sort(coords.begin(), coords.end(), [&](auto& a, auto& b) {
				return std::get<1>(a).GetDistance(actor) < std::get<1>(b).GetDistance(actor);
			});
			const auto& res = coords[0];
			if (ReturnData(std::get<0>(res), std::get<1>(res))) {
				return std::get<2>(res);
			}
		}
		Registry::Coordinate coord{ actor };
		return ReturnData(Registry::FurnitureType::None, coord) ? actor : nullptr;
	}

	bool UpdateBaseCoordinates(VM* a_vm, StackID a_stackID, RE::TESQuest* a_qst, RE::BSFixedString a_sceneid, RE::reference_array<float> a_out)
	{
		const auto [center, actor] = GetAliasRefs(a_qst);
		if (!actor || !center) {
			a_vm->TraceStack("Invalid aliases", a_stackID);
			return false;
		}
		const auto library = Registry::Library::GetSingleton();
		const auto scene = library->GetSceneByID(a_sceneid);
		if (!scene) {
			a_vm->TraceStack("Invalid scene id", a_stackID);
			return false;
		}
		const auto details = library->GetFurnitureDetails(center);
		if (!details)	 // nothing to do
			return true;
		auto res = details->GetClosestCoordinateInBound(center, scene->furnitures.furnitures, actor);
		if (res.empty())
			return false;
		scene->furnitures.offset.Apply(res[0].second);
		res[0].second.ToContainer(a_out);
		return true;
	}

	void ApplySceneOffset(VM* a_vm, StackID a_stackID, RE::TESQuest*, RE::BSFixedString a_sceneid, RE::reference_array<float> a_out)
	{
		const auto library = Registry::Library::GetSingleton();
		const auto scene = library->GetSceneByID(a_sceneid);
		if (!scene) {
			a_vm->TraceStack("Invalid scene id", a_stackID);
			return;
		}
		Registry::Coordinate ret{ a_out };
		scene->furnitures.offset.Apply(ret);
		ret.ToContainer(a_out);
	}

	RE::BSFixedString PlaceAndPlay(VM* a_vm, StackID a_stackID, RE::TESQuest*,
		std::vector<RE::Actor*> a_positions,
		std::vector<float> a_coordinates,
		RE::BSFixedString a_scene,
		RE::BSFixedString a_stage)
	{
		if (a_coordinates.size() != Offset::Total) {
			a_vm->TraceStack("Invalid offset", a_stackID);
			return "";
		}

		const auto scene = Registry::Library::GetSingleton()->GetSceneByID(a_scene);
		if (!scene) {
			a_vm->TraceStack("Invalid scene id", a_stackID);
			return "";
		} else if (a_positions.size() != scene->positions.size()) {
			a_vm->TraceStack("Number positions do not match number of scene positions", a_stackID);
			return "";
		} else if (std::find(a_positions.begin(), a_positions.end(), nullptr) != a_positions.end()) {
			a_vm->TraceStack("Array contains a none reference", a_stackID);
			return "";
		}

		const auto stage = scene->GetStageByKey(a_stage);
		if (!stage) {
			a_vm->TraceStack("Invalid stage id", a_stackID);
			return "";
		}

		for (size_t i = 0; i < a_positions.size(); i++) {
			const auto& actor = a_positions[i];
			Registry::Coordinate coordinate{ a_coordinates };
			stage->positions[i].offset.Apply(coordinate);

			actor->data.angle.z = coordinate.rotation;
			actor->SetPosition(coordinate.AsNiPoint(), true);
			Registry::Scale::GetSingleton()->SetScale(actor, scene->positions[i].scale);

			const auto event = scene->GetNthAnimationEvent(stage, i);
			actor->NotifyAnimationGraph(event);
			// NOTE: This does not work because SOS is too slow to equip the schlong
			// const auto schlong = fmt::format("SOSBend{}", static_cast<int32_t>(stage->positions[i].schlong));
			// actor->NotifyAnimationGraph(schlong);
		}

		return stage->id;
	}

	void RePlace(VM* a_vm, StackID a_stackID, RE::TESQuest*,
		RE::Actor* a_position,
		std::vector<float> a_coordinates,
		RE::BSFixedString a_scene,
		RE::BSFixedString a_stage,
		int32_t n)
	{
		if (!a_position) {
			a_vm->TraceStack("Actor is none", a_stackID);
			return;
		}
		const auto scene = Registry::Library::GetSingleton()->GetSceneByID(a_scene);
		if (!scene) {
			a_vm->TraceStack("Invalid scene id", a_stackID);
			return;
		}
		const auto stage = scene->GetStageByKey(a_stage);
		if (!stage) {
			a_vm->TraceStack("Invalid stage id", a_stackID);
			return;
		}
		if (n < 0 || n >= stage->positions.size()) {
			a_vm->TraceStack("Invalid stage id", a_stackID);
			return;
		}
		Registry::Coordinate coordinate{ a_coordinates };
		stage->positions[n].offset.Apply(coordinate);
		a_position->data.angle.z = coordinate.rotation;
		a_position->SetPosition(coordinate.AsNiPoint(), true);
	}

	bool GetIsCompatiblecenter(VM* a_vm, StackID a_stackID, RE::TESQuest*, RE::BSFixedString a_sceneid, RE::TESObjectREFR* a_center)
	{
		if (!a_center) {
			a_vm->TraceStack("Cannot validate a none reference center", a_stackID);
			return false;
		}
		const auto scene = Registry::Library::GetSingleton()->GetSceneByID(a_sceneid);
		if (!scene) {
			a_vm->TraceStack("Invalid scene id", a_stackID);
			return false;
		}
		return scene->IsCompatibleFurniture(a_center);
	}

	std::vector<RE::BSFixedString> AddContextExImpl(RE::TESQuest*, std::vector<RE::BSFixedString> a_oldcontext, std::string a_newcontext)
	{
		const auto list = Registry::StringSplit(a_newcontext, ',');
		for (auto&& tag : list) {
			if (!tag.starts_with('!'))
				continue;
			const auto context = RE::BSFixedString(std::string(tag.substr(1)));
			if (std::find(a_oldcontext.begin(), a_oldcontext.end(), context) != a_oldcontext.end())
				continue;
			a_oldcontext.push_back(context);
		}
		return a_oldcontext;
	}

	void ShuffleScenes(RE::TESQuest*, RE::reference_array<RE::BSFixedString> a_scenes, RE::BSFixedString a_tofront)
	{
		if (a_scenes.empty()) {
			return;
		}
		auto start = a_scenes.begin();
		if (!a_tofront.empty() && Registry::Library::GetSingleton()->GetSceneByID(a_tofront)) {
			auto where = std::ranges::find(a_scenes, a_tofront);
			if (where == a_scenes.end()) {
				a_scenes[0] = a_tofront;
			} else {
				auto tmp = a_scenes[0];
				a_scenes[0] = a_tofront;
				*where = tmp;
			}
			start++;
		}
		std::random_device rd;
		std::mt19937 gen{ rd() };
		std::ranges::shuffle(start, a_scenes.end(), gen);
	}

	bool IsPhysicsRegistered(RE::TESQuest* a_qst)
	{
		return Registry::Physics::GetSingleton()->IsRegistered(a_qst->formID);
	}

	void RegisterPhysics(VM* a_vm, StackID a_stackID, RE::TESQuest* a_qst, std::vector<RE::Actor*> a_positions, RE::BSFixedString a_activescene)
	{
		const auto scene = Registry::Library::GetSingleton()->GetSceneByID(a_activescene);
		if (!scene || scene->CountPositions() != a_positions.size()) {
			a_vm->TraceStack("Invalid scene", a_stackID);
			return;
		}
		Registry::Physics::GetSingleton()->Register(a_qst->formID, a_positions, scene);
	}

	void UnregisterPhysics(RE::TESQuest* a_qst)
	{
		Registry::Physics::GetSingleton()->Unregister(a_qst->formID);
	}

	std::vector<int> GetPhysicTypes(VM* a_vm, StackID a_stackID, RE::TESQuest* a_qst, RE::Actor* a_position, RE::Actor* a_partner)
	{
		auto data = Registry::Physics::GetSingleton()->GetData(a_qst->formID);
		if (!data) {
			a_vm->TraceStack("Not registered", a_stackID);
			return {};
		}
		std::vector<int> ret{};
		for (auto&& p : data->_positions) {
			if (a_position && p._owner != a_position->formID)
				continue;
			for (auto&& type : p._types) {
				if (a_partner && type._partner != a_partner->formID)
					continue;
				ret.push_back(static_cast<int>(type._type));
			}
		}
		return ret;
	}

	bool HasPhysicType(VM* a_vm, StackID a_stackID, RE::TESQuest* a_qst, int a_type, RE::Actor* a_position, RE::Actor* a_partner)
	{
		auto data = Registry::Physics::GetSingleton()->GetData(a_qst->formID);
		if (!data) {
			a_vm->TraceStack("Not registered", a_stackID);
			return false;
		}
		for (auto&& p : data->_positions) {
			if (a_position && p._owner != a_position->formID)
				continue;
			for (auto&& type : p._types) {
				if (a_partner && type._partner != a_partner->formID)
					continue;
				if (a_type != -1 && a_type != static_cast<int>(type._type))
					continue;
				return true;
			}
		}
		return false;
	}

	RE::Actor* GetPhysicPartnerByType(VM* a_vm, StackID a_stackID, RE::TESQuest* a_qst, RE::Actor* a_position, int a_type)
	{
		if (!a_position) {
			a_vm->TraceStack("Actor is none", a_stackID);
			return nullptr;
		}
		auto data = Registry::Physics::GetSingleton()->GetData(a_qst->formID);
		if (!data) {
			a_vm->TraceStack("Not registered", a_stackID);
			return nullptr;
		}
		for (auto&& p : data->_positions) {
			if (p._owner != a_position->formID)
				continue;
			for (auto&& type : p._types) {
				if (a_type != -1 && a_type != static_cast<int>(type._type))
					continue;
				if (auto ret = RE::TESForm::LookupByID<RE::Actor>(type._partner))
					return ret;
			}
		}
		return nullptr;
	}

	std::vector<RE::Actor*> GetPhysicPartnersByType(VM* a_vm, StackID a_stackID, RE::TESQuest* a_qst, RE::Actor* a_position, int a_type)
	{
		if (!a_position) {
			a_vm->TraceStack("Actor is none", a_stackID);
			return {};
		}
		auto data = Registry::Physics::GetSingleton()->GetData(a_qst->formID);
		if (!data) {
			a_vm->TraceStack("Not registered", a_stackID);
			return {};
		}
		std::vector<RE::Actor*> ret{};
		for (auto&& p : data->_positions) {
			if (a_position && p._owner != a_position->formID)
				continue;
			for (auto&& type : p._types) {
				if (a_type != -1 && a_type != static_cast<int>(type._type))
					continue;
				if (auto it = RE::TESForm::LookupByID<RE::Actor>(type._partner))
					ret.push_back(it);
			}
		}
		return ret;
	}

	float GetPhysicVelocity(VM* a_vm, StackID a_stackID, RE::TESQuest* a_qst, RE::Actor* a_position, RE::Actor* a_partner, int a_type)
	{
		if (!a_position || !a_partner) {
			a_vm->TraceStack("Actor is none", a_stackID);
			return 0.0f;
		} else if (a_type == -1) {
			a_vm->TraceStack("Type cant be 'any' here", a_stackID);
			return 0.0f;
		}
		auto data = Registry::Physics::GetSingleton()->GetData(a_qst->formID);
		if (!data) {
			a_vm->TraceStack("Not registered", a_stackID);
			return 0.0f;
		}
		std::vector<RE::Actor*> ret{};
		for (auto&& p : data->_positions) {
			if (p._owner != a_position->formID)
				continue;
			for (auto&& type : p._types) {
				if (a_partner->formID != type._partner)
					continue;
				if (a_type != static_cast<int>(type._type))
					continue;
				return type._velocity;
			}
		}
		return 0.0f;
	}

	void AddExperience(VM* a_vm, StackID a_stackID, RE::TESQuest*, std::vector<RE::Actor*> a_positions,
		RE::BSFixedString a_scene, std::vector<RE::BSFixedString> a_playedstages)
	{
		const auto scene = Registry::Library::GetSingleton()->GetSceneByID(a_scene);
		if (!scene) {
			a_vm->TraceStack("Invalid scene id", a_stackID);
			return;
		} else if (scene->CountPositions() != a_positions.size()) {
			a_vm->TraceStack("Position cound does not match scene position count", a_stackID);
			return;
		}
		int vaginal = 0, anal = 0, oral = 0;
		for (auto&& it : a_playedstages) {
			auto stage = scene->GetStageByKey(it);
			if (!stage)
				continue;

			vaginal += stage->tags.HasTag(Registry::Tag::Vaginal);
			anal += stage->tags.HasTag(Registry::Tag::Anal);
			oral += stage->tags.HasTag(Registry::Tag::Oral);
		}
		const auto statdata = Registry::Statistics::StatisticsData::GetSingleton();
		for (auto&& p : a_positions) {
			if (!p)
				continue;

			auto& stats = statdata->GetStatistics(p);
			stats.AddStatistic(stats.XP_Vaginal, vaginal * 1.25f);
			stats.AddStatistic(stats.XP_Anal, anal * 1.25f);
			stats.AddStatistic(stats.XP_Oral, oral * 1.25f);
		}
	}


	void UpdateStatistics(VM* a_vm, StackID a_stackID, RE::TESQuest*, RE::Actor* a_actor, std::vector<RE::Actor*> a_positions,
		RE::BSFixedString a_scene, std::vector<RE::BSFixedString> a_playedstages, float a_time)
	{
		if (!a_actor) {
			a_vm->TraceStack("Actor is none", a_stackID);
			return;
		}
		const auto scene = Registry::Library::GetSingleton()->GetSceneByID(a_scene);
		if (!scene) {
			a_vm->TraceStack("Invalid scene id", a_stackID);
			return;
		} else if (scene->CountPositions() != a_positions.size()) {
			a_vm->TraceStack("Position cound does not match scene position count", a_stackID);
			return;
		}
		auto& stats = Registry::Statistics::StatisticsData::GetSingleton()->GetStatistics(a_actor);
		stats.SetStatistic(stats.LastUpdate_GameTime, RE::Calendar::GetSingleton()->GetCurrentGameTime());
		stats.AddStatistic(stats.SecondsInScene, a_time);
		stats.AddStatistic(stats.TimesTotal, 1);
		if (scene->CountPositions() == 1) {
			stats.AddStatistic(stats.TimesMasturbated, 1);
			if (scene->CountSubmissives() == 1) {
				stats.AddStatistic(stats.TimesSubmissive, 1);
			}
		} else {
			int sub = 0;
			for (size_t i = 0; i < a_positions.size(); i++) {
				if (!a_positions[i])
					continue;
				if (a_positions[i] == a_actor) {
					const auto& p = scene->GetNthPosition(i);
					sub = p->IsSubmissive();
					continue;
				}
				if (sub != 1 && scene->GetNthPosition(i)->IsSubmissive()) {
					sub = -1;
				}
				if (Registry::IsNPC(a_positions[i])) {
					switch (Registry::GetSex(a_positions[i])) {
					case Registry::Sex::Male:
						stats.AddStatistic(stats.PartnersMale, 1);
						break;
					case Registry::Sex::Female:
						stats.AddStatistic(stats.PartnersFemale, 1);
						break;
					case Registry::Sex::Futa:
						stats.AddStatistic(stats.PartnersFuta, 1);
						break;
					}
				} else {
					stats.AddStatistic(stats.PartnersCreature, 1);
				}
			}
			switch (sub) {
			case -1:
				stats.AddStatistic(stats.TimesDominant, 1);
				break;
			case 1:
				stats.AddStatistic(stats.TimesSubmissive, 1);
				break;
			}
		}
		int vaginal = 0, anal = 0, oral = 0;
		for (auto&& it : a_playedstages) {
			auto stage = scene->GetStageByKey(it);
			if (!stage)
				continue;

			vaginal += stage->tags.HasTag(Registry::Tag::Vaginal);
			anal += stage->tags.HasTag(Registry::Tag::Anal);
			oral += stage->tags.HasTag(Registry::Tag::Oral);
		}
		if (vaginal) {
			stats.AddStatistic(stats.TimesVaginal, 1);
			stats.AddStatistic(stats.XP_Vaginal, vaginal * 1.25f);
		}
		if (anal) {
			stats.AddStatistic(stats.TimesAnal, 1);
			stats.AddStatistic(stats.XP_Anal, anal * 1.25f);
		}
		if (oral) {
			stats.AddStatistic(stats.TimesOral, 1);
			stats.AddStatistic(stats.XP_Oral, oral * 1.25f);
		}
	}

}	 // namespace Papyrus::ThreadModel

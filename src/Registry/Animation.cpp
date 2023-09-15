#include "Animation.h"

#include "Registry/Define/RaceKey.h"
#include "Util/Combinatorics.h"

namespace Registry
{
	AnimPackage::AnimPackage(const fs::path a_file)
	{
		std::ifstream stream(a_file, std::ios::binary);
		stream.unsetf(std::ios::skipws);
		stream.exceptions(std::fstream::eofbit);
		stream.exceptions(std::fstream::badbit);
		stream.exceptions(std::fstream::failbit);

		uint8_t version;
		stream.read(reinterpret_cast<char*>(&version), 1);
		switch (version) {
		case 1:
			{
				Decode::Read(stream, name);
				Decode::Read(stream, author);

				hash.resize(Decode::HASH_SIZE);
				stream.read(hash.data(), Decode::HASH_SIZE);

				uint64_t scene_count;
				Decode::Read(stream, scene_count);
				scenes.reserve(scene_count);
				for (size_t i = 0; i < scene_count; i++) {
					scenes.push_back(
						std::make_unique<Scene>(stream, version));
				}
			}
			break;
		default:
			throw std::runtime_error(fmt::format("Invalid version: {}", version).c_str());
		}
	}

	Scene::Scene(std::ifstream& a_stream, std::string_view a_hash) :
		hash(a_hash)
	{
		id.resize(Decode::ID_SIZE);
		a_stream.read(id.data(), Decode::ID_SIZE);
		Decode::Read(a_stream, name);
		// --- Position Infos
		uint64_t info_count;
		Decode::Read(a_stream, info_count);
		positions.reserve(info_count);
		for (size_t i = 0; i < info_count; i++) {
			positions.emplace_back(a_stream);
		}

		enum legacySex
		{
			Male = 'M',
			Female = 'F',
			Futa = 'H',
			Creature = 'C',
		};
		std::vector<std::vector<legacySex>> sexes{};
		sexes.reserve(positions.size());
		for (auto&& position : positions) {
			std::vector<legacySex> vec{};
			if (position.race == RaceKey::Human) {
				if (position.sex.all(Sex::Male))
					vec.push_back(legacySex::Male);
				if (position.sex.all(Sex::Female))
					vec.push_back(legacySex::Female);
				if (position.sex.all(Sex::Futa))
					vec.push_back(legacySex::Futa);
			} else {
				vec.push_back(legacySex::Creature);
			}
			if (vec.empty())
				throw std::runtime_error(fmt::format("Some position has no associated sex in scene: {}", id).c_str());
			sexes.push_back(vec);
		}
		Combinatorics::ForEachCombination(sexes, [&](auto& it) {
			std::vector<char> gender_tag{};
			for (auto&& sex : it) {
				gender_tag.push_back(*sex);
			}
			RE::BSFixedString gTag1{ std::string{ gender_tag.begin(), gender_tag.end() } };
			RE::BSFixedString gTag2{ std::string{ gender_tag.rbegin(), gender_tag.rend() } };
			tags.AddTag(gTag1);
			if (gTag2 != gTag1) {
				tags.AddTag(gTag2);
			}
			return Combinatorics::CResult::Next;
		});
		// --- Stages
		std::string startstage(Decode::ID_SIZE, 'X');
		a_stream.read(startstage.data(), Decode::ID_SIZE);
		uint64_t stage_count;
		Decode::Read(a_stream, stage_count);
		stages.reserve(stage_count);
		for (size_t i = 0; i < stage_count; i++) {
			const auto& stage = stages.emplace_back(
				std::make_unique<Stage>(a_stream));

			tags.AddTag(stage->tags);
			if (stage->id == startstage) {
				start_animation = stage.get();
			}
		}
		if (!start_animation) {
			throw std::runtime_error(fmt::format("Start animation {} is not found in scene {}", startstage, id).c_str());
		}
		// --- Graph
		uint64_t graph_vertices;
		Decode::Read(a_stream, graph_vertices);
		if (graph_vertices != stage_count) {
			throw std::runtime_error(fmt::format("Invalid graph vertex count; expected {} but got {}", stage_count, graph_vertices).c_str());
		}
		std::string vertexid(Decode::ID_SIZE, 'X');
		for (size_t i = 0; i < graph_vertices; i++) {
			a_stream.read(vertexid.data(), Decode::ID_SIZE);
			const auto vertex = GetStageByKey(vertexid.data());
			if (!vertex) {
				throw std::runtime_error(fmt::format("Invalid vertex: {} in scene: {}", vertexid, id).c_str());
			}
			std::vector<const Stage*> edges{};
			uint64_t edge_count;
			Decode::Read(a_stream, edge_count);
			std::string edgeid(Decode::ID_SIZE, 'X');
			for (size_t n = 0; n < edge_count; n++) {
				a_stream.read(edgeid.data(), Decode::ID_SIZE);
				const auto edge = GetStageByKey(edgeid.data());
				if (!edge) {
					throw std::runtime_error(fmt::format("Invalid edge: {} for vertex: {} in scene: {}", edgeid, vertexid, id).c_str());
				}
				edges.push_back(edge);
			}
			graph.insert(std::make_pair(vertex, edges));
		}		
		// --- Misc
		a_stream.read(reinterpret_cast<char*>(&furnitures.furnitures), 1);
		a_stream.read(reinterpret_cast<char*>(&furnitures.allowbed), 1);
		Decode::Read(a_stream, furnitures.offset[Offset::X]);
		Decode::Read(a_stream, furnitures.offset[Offset::Y]);
		Decode::Read(a_stream, furnitures.offset[Offset::Z]);
		Decode::Read(a_stream, furnitures.offset[Offset::R]);
		a_stream.read(reinterpret_cast<char*>(&is_private), 1);
	}

	PositionInfo::PositionInfo(std::ifstream& a_stream)
	{
		a_stream.read(reinterpret_cast<char*>(&race), 1);
		a_stream.read(reinterpret_cast<char*>(&sex), 1);
		Decode::Read(a_stream, scale);
		a_stream.read(reinterpret_cast<char*>(&extra), 1);
	}

	Stage::Stage(std::ifstream& a_stream)
	{
		id.resize(Decode::ID_SIZE);
		a_stream.read(id.data(), Decode::ID_SIZE);

		uint64_t position_count;
		Decode::Read(a_stream, position_count);
		positions.reserve(position_count);
		for (size_t i = 0; i < position_count; i++) {
			positions.emplace_back(a_stream);
		}

		Decode::Read(a_stream, fixedlength);
		Decode::Read(a_stream, navtext);
		tags = TagData{ a_stream };
	}

	Position::Position(std::ifstream& a_stream) :
		event(Decode::Read<decltype(event)>(a_stream)),
		climax(Decode::Read<uint8_t>(a_stream)),
		offset(Transform(a_stream))
	{
		a_stream.read(reinterpret_cast<char*>(&strips), 1);
	}

	bool PositionInfo::CanFillPosition(RE::Actor* a_actor) const
	{
		auto fragment = stl::enumeration(MakeFragmentFromActor(a_actor, false));
		if (CanFillPosition(fragment.get()))
			return true;
		
		fragment.set(PositionFragment::Submissive);
		return CanFillPosition(fragment.get());
	}

	bool PositionInfo::CanFillPosition(PositionFragment a_fragment) const
	{
		const auto fragment = stl::enumeration(a_fragment);
		if (fragment.all(PositionFragment::Futa) && !this->sex.all(Sex::Futa))
			return false;
		if (fragment.all(PositionFragment::Male) && !this->sex.all(Sex::Male))
			return false;
		if (fragment.all(PositionFragment::Female) && !this->sex.all(Sex::Female))
			return false;

		if (fragment.all(PositionFragment::Unconscious) != this->extra.all(Extra::Unconscious))
			return false;
		else if (fragment.all(PositionFragment::Submissive) != this->extra.all(Extra::Submissive))
			return false;

		if (fragment.all(PositionFragment::Human)) {
			if (this->extra.all(Extra::Vamprie) && !fragment.all(PositionFragment::Vampire))
				return false;
			if (extra.any(Extra::Armbinder) != fragment.all(PositionFragment::Arminder))
				return false;
			if (extra.any(Extra::Yoke) != fragment.all(PositionFragment::Yoke))
				return false;
			if (extra.any(Extra::Legbinder) != fragment.all(PositionFragment::LegsBound))
				return false;
		} else {
			const auto thisrace = static_cast<uint64_t>(race);
			if ((fragment.underlying() & thisrace) != thisrace)
				return false;
		}
		return true;
	}

	bool PositionInfo::CanFillPosition(const PositionInfo& a_other) const
	{
		auto extra_copy = a_other.extra;
		extra_copy.reset(Extra::Optional);
		return race == a_other.race && sex.any(a_other.sex.get()) && extra.all(extra_copy.get());
	}

	std::vector<PositionFragment> PositionInfo::MakeFragments() const
	{
		std::vector<stl::enumeration<PositionFragment>> fragments{};
		const auto addVariance = [&](PositionFragment a_variancebit) {
			const auto count = fragments.size();
			for (size_t i = 0; i < count; i++) {
				auto copy = fragments[i];
				copy.set(a_variancebit);
				fragments.push_back(copy);
			}
		};
		const auto setFragmentBit = [&](PositionFragment a_bit) {
			for (auto&& fragment : fragments) {
				fragment.set(a_bit);
			}
		};
		const auto addRaceVariance = [&](RaceKey a_racekey) {
			const auto val = RaceKeyAsFragment(a_racekey);
			const auto count = fragments.size();
			for (size_t i = 0; i < count; i++) {
				auto copy = fragments[i];
				copy.reset(PositionFragment::CrtBit0,
					PositionFragment::CrtBit1,
					PositionFragment::CrtBit2,
					PositionFragment::CrtBit3,
					PositionFragment::CrtBit4,
					PositionFragment::CrtBit5);
				copy.set(val);
				fragments.push_back(copy);
			}
		};
		const auto setRaceBit = [&](RaceKey a_racekey) {
			const auto val = RaceKeyAsFragment(a_racekey);
			setFragmentBit(val);
		};

		if (this->sex.all(Sex::Male))
			fragments.emplace_back(PositionFragment::Male);
		if (this->sex.all(Sex::Female))
			fragments.emplace_back(PositionFragment::Female);
		if (this->sex.all(Sex::Futa))
			fragments.emplace_back(PositionFragment::Futa);

		if (this->extra.all(Extra::Unconscious))
			setFragmentBit(PositionFragment::Unconscious);
		else if (this->extra.all(Extra::Submissive))
			setFragmentBit(PositionFragment::Submissive);

		switch (this->race) {
		case RaceKey::Human:
			{
				setFragmentBit(PositionFragment::Human);
				if (this->extra.all(Extra::Vamprie)) {
					setFragmentBit(PositionFragment::Vampire);
				} else {
					addVariance(PositionFragment::Vampire);
				}
				if (this->extra.all(Extra::Armbinder)) {
					setFragmentBit(PositionFragment::Arminder);
				} else if (this->extra.all(Extra::Yoke)) {
					setFragmentBit(PositionFragment::Yoke);
				}
				if (this->extra.all(Extra::Legbinder)) {
					setFragmentBit(PositionFragment::LegsBound);
				}
			}
			break;
		case RaceKey::Boar:
			setRaceBit(RaceKey::BoarMounted);
			addRaceVariance(RaceKey::BoarSingle);
			break;
		case RaceKey::Canine:
			setRaceBit(RaceKey::Dog);
			addRaceVariance(RaceKey::Wolf);
		default:
			setRaceBit(this->race);
			break;
		}
		if (this->extra.all(Extra::Optional)) {
			fragments.push_back(PositionFragment::None);
		}

		std::vector<PositionFragment> ret{};
		for (auto&& it : fragments)
			ret.push_back(it.get());

		return ret;
	}

	PapyrusSex PositionInfo::GetSexPapyrus() const
	{
		if (race == Registry::RaceKey::Human) {
			return PapyrusSex(sex.underlying());
		} else {
			stl::enumeration<PapyrusSex> ret{ PapyrusSex::None };
			if (sex.all(Registry::Sex::Male))
				ret.set(PapyrusSex::CrtMale);
			if (sex.all(Registry::Sex::Female))
				ret.set(PapyrusSex::CrtFemale);

			return ret == PapyrusSex::None ? ret.get() : PapyrusSex::CrtMale;
		}
	}

	const Stage* Scene::GetStageByKey(const RE::BSFixedString& a_key) const
	{
		if (a_key.empty()) {
			return start_animation;
		}

		for (auto&& [key, dest] : graph) {
			if (a_key == key->id.c_str())
				return key;
		}
		return nullptr;
	}

	bool Scene::HasCreatures() const
	{
		for (auto&& info : positions) {
			if (!info.IsHuman())
				return true;
		}
		return false;
	}

	uint32_t Scene::CountSubmissives() const
	{
		uint32_t ret = 0;
		for (auto&& info : positions) {
			if (info.IsSubmissive()) {
				ret++;
			}
		}
		return ret;
	}

	uint32_t Scene::CountPositions() const
	{
		return static_cast<uint32_t>(positions.size());
	}

	uint32_t Scene::CountOptionalPositions() const
	{
		uint32_t ret = 0;
		for (auto&& info : positions) {
			if (info.IsOptional()) {
				ret++;
			}
		}
		return ret;

	}

	bool Scene::IsEnabled() const
	{
		return enabled;
	}

	bool Scene::IsPrivate() const
	{
		return is_private;
	}

	std::vector<std::vector<PositionFragment>> Scene::MakeFragments() const
	{
		std::vector<std::vector<PositionFragment>> ret;
		ret.reserve(this->positions.size());
		for (auto&& pinfo : this->positions) {
			ret.push_back(pinfo.MakeFragments());
		}
		return ret;
	}

	bool Scene::IsCompatibleTags(const TagData& a_tags) const
	{
		return this->tags.HasTags(a_tags, true);
	}
	bool Scene::IsCompatibleTags(const TagDetails& a_details) const
	{
		return a_details.MatchTags(tags);
	}

	bool Scene::UsesFurniture() const
	{
		return this->furnitures.furnitures != FurnitureType::None;
	}

	bool Scene::IsCompatibleFurniture(RE::TESObjectREFR* a_reference) const
	{
		const auto type = FurnitureHandler::GetFurnitureType(a_reference);
		return IsCompatibleFurniture(type);
	}

	bool Scene::IsCompatibleFurniture(FurnitureType a_furniture) const
	{
		switch (a_furniture) {
		case FurnitureType::None:
			return !UsesFurniture();
		case FurnitureType::BedDouble:
		case FurnitureType::BedSingle:
			if (this->furnitures.allowbed)
				return true;
			break;
		}
		return this->furnitures.furnitures.any(a_furniture);
	}

	bool Scene::Legacy_IsCompatibleSexCount(int32_t a_males, int32_t a_females) const
	{
		if (a_males < 0 && a_females < 0) {
			return true;
		}

		bool ret = false;
		tags.ForEachExtra([&](const std::string_view a_tag) {
			if (a_tag.find_first_not_of("MFC") != std::string_view::npos) {
				return false;
			}
			if (a_males == -1 || std::count(a_tag.begin(), a_tag.end(), 'M') == a_males) {
				if (a_females == -1 || std::count(a_tag.begin(), a_tag.end(), 'F') == a_females) {
					ret = true;
					return true;
				}
			}
			return false;
		});
		return ret;
	}

	bool Scene::Legacy_IsCompatibleSexCountCrt(int32_t a_males, int32_t a_females) const
	{
		enum
		{
			Male = 0,
			Female = 1,
			Either = 2,
		};

		bool ret = false;
		tags.ForEachExtra([&](const std::string_view a_tag) {
			if (a_tag.find_first_not_of("MFC") != std::string_view::npos) {
				return false;
			}
			const auto crt_total = std::count(a_tag.begin(), a_tag.end(), 'C');
			if (crt_total != a_males + a_females) {
				return true;
			}

			std::vector<int> count;
			for (auto&& position : positions) {
				if (position.race == Registry::RaceKey::Human)
					continue;
				if (position.sex.none(Registry::Sex::Female)) {
					count[Male]++;
				} else if (position.sex.none(Registry::Sex::Male)) {
					count[Female]++;
				} else {
					count[Either]++;
				}
			}
			if (count[Male] <= a_males && count[Male] + count[Either] >= a_males) {
				count[Either] -= a_males - count[Male];
				ret = count[Female] + count[Either] == a_females;
			}
			return true;
		});
		return ret;
	}

	std::optional<std::vector<RE::Actor*>> Scene::SortActors(const std::vector<RE::Actor*>& a_positions, bool a_withfallback) const
	{
		if (a_positions.size() < positions.size())
			return std::nullopt;

		std::vector<std::pair<RE::Actor*, Registry::PositionFragment>> argActor{};
		for (size_t i = 0; i < positions.size(); i++) {
			argActor.emplace_back(
				a_positions[i],
				MakeFragmentFromActor(a_positions[i], positions[i].IsSubmissive())
			);
		}
		return a_withfallback ? SortActors(argActor) : SortActorsFB(argActor);
	}

	std::optional<std::vector<RE::Actor*>> Scene::SortActors(const std::vector<std::pair<RE::Actor*, Registry::PositionFragment>>& a_positions) const
	{
		if (a_positions.size() > positions.size())
			return std::nullopt;
		// Mark every position that every actor can be placed in
		std::vector<std::vector<std::pair<size_t, RE::Actor*>>> compatibles{};
		compatibles.resize(a_positions.size());
		for (size_t i = 0; i < a_positions.size(); i++) {
			for (size_t n = 0; n < positions.size(); n++) {
				if (positions[n].CanFillPosition(a_positions[i].second)) {
					compatibles[i].emplace_back(n, a_positions[i].first);
				}
			}
		}
		// Then find a combination of compatibles that consists exclusively of unique elements
		std::vector<RE::Actor*> ret{};
		Combinatorics::ForEachCombination(compatibles, [&](auto it) {
			// Iteration always use the same nth actor + some idx of a compatible position
			std::vector<RE::Actor*> result{ it.size(), nullptr };
			for (auto&& current : it) {
				const auto [scene_idx, actor] = *current;
				if (result[scene_idx] != nullptr) {
					return Combinatorics::CResult::Next;
				}
				result[scene_idx] = actor;
			}
			assert(std::find(result.begin(), result.end(), nullptr) == result.end());
			ret = result;
			return Combinatorics::CResult::Stop;
		});
		if (ret.empty()) {
			return std::nullopt;
		}
		return ret;
	}

	std::optional<std::vector<RE::Actor*>> Scene::SortActorsFB(std::vector<std::pair<RE::Actor*, Registry::PositionFragment>> a_positions) const
	{
		auto ret = SortActors(a_positions);
		if (ret)
			return ret;

		for (auto&& [actor, fragment] : a_positions) {
			auto e = stl::enumeration(fragment);
			if (e.all(PositionFragment::Human, PositionFragment::Female) && e.none(PositionFragment::Male)) {
				e.reset(PositionFragment::Female);
				e.set(PositionFragment::Male);
			}
			fragment = e.get();
			ret = SortActors(a_positions);
			if (ret)
				return ret;
		}
		return std::nullopt;
	}

	size_t Scene::GetNumLinkedStages(const Stage* a_stage) const
	{
		const auto where = graph.find(a_stage);
		if (where == graph.end())
			return 0;
		
		return where->second.size();
	}

	const Stage* Scene::GetNthLinkedStage(const Stage* a_stage, size_t n) const
	{
		const auto where = graph.find(a_stage);
		if (where == graph.end())
			return 0;

		if (n < 0 || n >= where->second.size())
			return 0;
		
		return where->second[n];
	}

	RE::BSFixedString Scene::GetNthAnimationEvent(const Stage* a_stage, size_t n) const
	{
		if (n < 0 || n >= a_stage->positions.size())
			return "";
		std::string ret{ hash };
		return ret + a_stage->positions[n].event.data();
	}

	std::vector<RE::BSFixedString> Scene::GetAnimationEvents(const Stage* a_stage) const
	{
		std::vector<RE::BSFixedString> ret{};
		ret.reserve(a_stage->positions.size());
		for (auto&& position : a_stage->positions) {
			std::string event{ hash };
			ret.push_back(event + position.event.data());
		}
		return ret;
	}

	Scene::NodeType Scene::GetStageNodeType(const Stage* a_stage) const
	{
		if (a_stage == start_animation)
			return NodeType::Root;
		
		const auto where = graph.find(a_stage);
		if (where == graph.end())
			return NodeType::None;
		
		return where->second.size() == 0 ? NodeType::Sink : NodeType::Default;
	}

	std::vector<const Stage*> Scene::GetLongestPath(const Stage* a_src) const
	{
		if (GetStageNodeType(a_src) == NodeType::Sink)
			return { a_src };

		std::set<const Stage*> visited{};
		std::function<std::vector<const Stage*>(const Stage*)> DFS = [&](const Stage* src) -> std::vector<const Stage*> {
			if (visited.contains(src))
				return {};
			visited.insert(src);

			std::vector<const Stage*> longest_path{ src };
			const auto& neighbours = this->graph.find(src);
			assert(neighbours != this->graph.end());
			for (auto&& n : neighbours->second) {
				const auto cmp = DFS(n);
				if (cmp.size() + 1 > longest_path.size()) {
					longest_path.assign(cmp.begin(), cmp.end());
					longest_path.insert(longest_path.begin(), src);
				}
			}
			return longest_path;
		};
		return DFS(a_src);
	}

	std::vector<const Stage*> Scene::GetShortestPath(const Stage* a_src) const
	{
		if (GetStageNodeType(a_src) == NodeType::Sink)
			return { a_src };

		std::function<std::vector<const Stage*>(const Stage*)> BFS = [&](const Stage* src) -> std::vector<const Stage*> {
			std::set<const Stage*> visited{ src };
			std::map<const Stage*, const Stage*> pred{ { src, nullptr } };
			std::queue<const Stage*> queue{ { src } };
			while (!queue.empty()) {
				const auto it = queue.front();
				const auto neighbours = graph.find(it);
				assert(neighbours != this->graph.end());
				for (auto&& n : neighbours->second) {
					if (visited.contains(n))
						continue;
					if (GetStageNodeType(n) == NodeType::Sink) {
						std::vector<const Stage*> ret{};
						auto p = pred.at(it);
						while (p != nullptr) {
							ret.push_back(p);
							p = pred.at(p);
						}
						return { ret.rbegin(), ret.rend() };
					}
					pred.emplace(n, it);
					visited.insert(n);
					queue.push(n);
				}
				queue.pop();
			}
			return { src };
		};
		return BFS(a_src);
	}

	std::vector<const Stage*> Scene::GetClimaxStages() const
	{
		std::vector<const Stage*> ret{};
		for (auto&& stage : stages) {
			for (auto&& position : stage->positions) {
				if (position.climax) {
					ret.push_back(stage.get());
					break;
				}
			}
		}
		return ret;
	}

	std::vector<const Stage*> Scene::GetFixedLengthStages() const
	{
		std::vector<const Stage*> ret{};
		for (auto&& stage : stages) {
			if (stage->fixedlength)
				ret.push_back(stage.get());
		}
		return ret;
	}

}	 // namespace Registry

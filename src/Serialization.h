#pragma once

#include "Registry/Stats.h"

namespace Serialization
{
	inline std::string GetTypeName(uint32_t a_type)
	{
		constexpr auto size = sizeof(uint32_t);
		std::string ret{};
		ret.resize(size);
		const char* it = reinterpret_cast<char*>(&a_type);
		for (size_t i = 0, j = size - 2; i < size - 1; i++, j--)
			ret[j] = it[i];

		return ret;
	}

	class Serialize final :
		public Singleton<Serialize>
	{
	public:
		enum : std::uint32_t
		{
			_Version = 1,

			_Statistics = 'stcs'
		};

		static void SaveCallback(SKSE::SerializationInterface* a_intfc)
		{
			if (!a_intfc->OpenRecord(_Statistics, _Version))
				logger::error("Failed to open record <Statistics>");
			else
				Registry::Statistics::StatisticsData::GetSingleton()->Save(a_intfc);
		}

		static void LoadCallback(SKSE::SerializationInterface* a_intfc)
		{
			uint32_t type;
			uint32_t version;
			uint32_t length;
			while (a_intfc->GetNextRecordInfo(type, version, length)) {
				if (version != _Version) {
					logger::info("Invalid Version for loaded Data of Type = {}. Expected = {}; Got = {}", GetTypeName(type), _Version, version);
					continue;
				}
				logger::info("Loading record {}", GetTypeName(type));
				switch (type) {
				case _Statistics:
					Registry::Statistics::StatisticsData::GetSingleton()->Load(a_intfc);
					break;
				default:
					break;
				}
			}

			logger::info("Finished loading data from cosave");
		}

		static void RevertCallback(SKSE::SerializationInterface* a_intfc)
		{
			Registry::Statistics::StatisticsData::GetSingleton()->Revert(a_intfc);
		}

		static void FormDeleteCallback(RE::VMHandle)
		{
		}


	};	// class Serialize

}	 // namespace Serialization

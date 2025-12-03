#include "Bridge/Consts.h"
#include "RE/Fallout.h"

namespace Papyrus
{
	bool RegisterNAFUtilsFunctions(RE::BSScript::IVirtualMachine* a_VM);
	std::string RemoveAllChar(std::monostate, std::string sStr, std::string sChar);
	std::string CacheActorsToString(std::monostate, std::vector<RE::Actor*> akActors);
	std::vector<RE::Actor*> GetActorsFromString(std::monostate, std::string sPackedActors);
	std::string CacheFormlistToString(std::monostate, RE::BGSListForm* akFormList);
	void FillFormlistFromString(std::monostate, RE::BGSListForm* akFormList, std::string sPackedObjects);
	std::vector<RE::TESForm*> GetEquipmentFromString(std::monostate, std::string sPackedString);
	bool SetINISettingToFile(std::monostate, std::string file, std::string section, std::string setting, std::string value);
	std::string JoinStringArray(std::monostate, std::vector<std::string> arr, std::string delim);
	void AddKeywordToActors(std::monostate, std::vector<RE::Actor*> akActors, RE::BGSKeyword* keyword);
	void RemoveKeywordFromActors(std::monostate, std::vector<RE::Actor*> akActors, RE::BGSKeyword* keyword);
	std::string NormalizeTag(std::monostate, std::string sTag);
	bool ContainsPlayer(std::monostate, std::vector<RE::Actor*> akActors);
	std::vector<RE::Actor*> FilterActors(std::monostate, std::vector<RE::Actor*> akActors, RE::BGSKeyword* kBlockedKeyword, RE::BGSKeyword* kBusyKeyword);
	std::vector<RE::Actor*> SwapActorsAtIndices(std::monostate, std::vector<RE::Actor*> akActors, std::int32_t index1, std::int32_t index2);
	bool IsFemale(std::monostate, RE::Actor* akActor);
	std::string AddTag(std::monostate, std::string sTags, std::string sNewTag);
	std::string RemoveTag(std::monostate, std::string sTags, std::string sTagToRemove, int removeAll);
	bool ContainsTag(std::monostate, std::string sTags, std::string sTag);
}

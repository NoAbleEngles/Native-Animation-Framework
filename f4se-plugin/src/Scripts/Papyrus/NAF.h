#pragma once
#include <random>
#include "Structs.h"

//NAF BRIDGE
extern int PRINT_LOG;

namespace Papyrus::NAF
{
	constexpr auto SCRIPT_NAME{ "NAF"sv };

	using namespace RE::BSScript;
	using SceneId = structure_wrapper<"NAF", "SceneId">;
	using StartSceneResult = structure_wrapper<"NAF", "StartSceneResult">;
	using SceneSettings = structure_wrapper<"NAF", "SceneSettings">;

	/*
	|--------------------|
	| Internal Functions |
	|--------------------|
	*/

	std::vector<Variable*> MakeVarArray(size_t s)
	{
		std::vector<Variable*> arr(s, nullptr);
		for (size_t i = 0; i < s; i++) {
			arr[i] = new Variable();
		}
		return arr;
	}

	struct SceneData
	{
		Scene::StartResult result;
		Scene::SceneSettings settings;
	};

	SceneData GetSceneData(std::vector<RE::Actor*>& a_actors, std::optional<SceneSettings>& a_settings)
	{
		SceneData resultData;
		auto& settings = resultData.settings;

		if (a_actors.size() < 1) {
			resultData.result = { Scene::kNoActors };
			return resultData;
		}

		std::string a_position = "";
		std::string includeTags = "";
		std::string excludeTags = "";
		std::string requireTags = "";
		RE::TESObjectREFR* posRefr = nullptr;

		if (a_settings.has_value()) {
			auto& s = a_settings.value();
			if (auto v = s.find<std::string>("position", true); v) {
				a_position = v.value();
			}
			if (auto v = s.find<float>("duration", true); v) {
				settings.duration = v.value();
			}
			if (auto v = s.find<RE::TESObjectREFR*>("positionRef", true); v) {
				posRefr = v.value();
			}
			if (auto v = s.find<std::string>("includeTags", true); v) {
				includeTags = v.value();
			}
			if (auto v = s.find<std::string>("excludeTags", true); v) {
				excludeTags = v.value();
			}
			if (auto v = s.find<std::string>("requireTags", true); v) {
				requireTags = v.value();
			}
			if (auto v = s.find<bool>("forceNPCControlled", true); v) {
				settings.autoAdvance = v.value();
			}
			if (auto v = s.find<bool>("ignoreCombat", true); v) {
				settings.ignoreCombat = v.value();
			}
		}

		std::optional<Data::Global::TagFilter> tFilter;
		if (includeTags.size() > 0 || excludeTags.size() > 0 || requireTags.size() > 0) {
			Utility::TransformStringToLower(includeTags);
			Utility::TransformStringToLower(excludeTags);
			Utility::TransformStringToLower(requireTags);
			tFilter.emplace(
				Utility::SplitString(includeTags, ","),
				Utility::SplitString(excludeTags, ","),
				Utility::SplitString(requireTags, ","));
		}

		Data::AnimationFilter filter(a_actors);
		if (filter.numTotalActors != a_actors.size()) {
			resultData.result = { Scene::kInvalidActor };
			return resultData;
		}
		auto positions = Data::Global::GetFilteredPositions(filter, a_position.size() < 1, false, posRefr, false, tFilter);

		if (a_position.size() < 1) {
			if (positions.size() > 0) {
				a_position = Utility::SelectRandom(positions);
			} else {
				resultData.result = { Scene::kNoAvailablePositions };
				return resultData;
			}
		} else if (!Utility::VectorContains(positions, a_position)) {
			resultData.result = { Scene::kSpecifiedPositionNotAvailable };
			return resultData;
		}

		auto targetPos = Data::GetPosition(a_position);
		if (targetPos == nullptr) {
			resultData.result = { Scene::kNullPosition };
			return resultData;
		}

		settings.startPosition = targetPos;

		if (posRefr == nullptr) {
			settings.locationRefr = a_actors[0]->GetHandle();
		} else {
			settings.locationRefr = posRefr->GetHandle();
		}

		settings.actors.reserve(a_actors.size());
		for (auto& a : a_actors) {
			settings.actors.push_back(a->GetActorHandle());
		}

		return resultData;
	}

	/*
	|-------------------|
	| General Functions |
	|-------------------|
	*/

	void ToggleMenu(std::monostate)
	{
		auto UI = RE::UI::GetSingleton();
		auto UIMessageQueue = RE::UIMessageQueue::GetSingleton();

		if (UI && UIMessageQueue) {
			UIMessageQueue->AddMessage("NAFMenu", UI->GetMenuOpen("NAFMenu") ? RE::UI_MESSAGE_TYPE::kHide : RE::UI_MESSAGE_TYPE::kShow);
		}
	}

	bool GetDisableRescaler(std::monostate)
	{
		return Data::Settings::Values.bDisableRescaler;
	}

	void SetDisableRescaler(std::monostate, bool a_setting)
	{
		Data::Settings::Values.bDisableRescaler = a_setting;
	}

	bool IsActorUsable(std::monostate, RE::Actor* a_actor)
	{
		if (!a_actor) {
			return false;
		}

		return !Scene::SceneManager::IsActorInScene(a_actor);
	}

	void SetActorUsable(std::monostate, RE::Actor* a_actor, bool a_usable)
	{
		if (!a_actor || !Data::Forms::NAFDoNotUseKW) {
			return;
		}

		a_actor->ModifyKeyword(Data::Forms::NAFDoNotUseKW, !a_usable);
	}

	bool SetPackageOverride(std::monostate, RE::Actor* a_actor, RE::TESPackage* a_pkg)
	{
		return PackageOverride::Set(a_actor->GetActorHandle(), a_pkg);
	}

	void ClearPackageOverride(std::monostate, RE::Actor* a_actor)
	{
		if (!a_actor) {
			return;
		}

		PackageOverride::Clear(a_actor->GetActorHandle());
	}

	void PlayDynamicIdle(std::monostate, RE::Actor* a_actor, std::string a_fileName, std::string a_animEvent, std::string a_behvGraph)
	{
		if (!a_actor) {
			return;
		}

		Scene::DynamicIdle::Play(a_actor, a_fileName, a_animEvent, a_behvGraph);
	}

	/*
	|-----------------|
	| Scene Functions |
	|-----------------|
	*/

	StartSceneResult StartScene(IVirtualMachine& a_VM, uint32_t a_stackID, std::monostate, std::vector<RE::Actor*> a_actors, std::optional<SceneSettings> a_settings)
	{
		auto data = GetSceneData(a_actors, a_settings);
		if (!data.result) {
			a_VM.PostError(data.result.GetErrorMessage(), a_stackID, ErrorLogger::Severity::kInfo);
			return PackSceneResult(false);
		}

		uint64_t sceneId;
		if (auto res = Scene::SceneManager::StartScene(data.settings, sceneId); !res) {
			a_VM.PostError(res.GetErrorMessage(), a_stackID, ErrorLogger::Severity::kInfo);
			return PackSceneResult(false);
		}

		return PackSceneResult(true, sceneId);
	}

	StartSceneResult WalkToAndStartScene(IVirtualMachine& a_VM, uint32_t a_stackID, std::monostate, std::vector<RE::Actor*> a_actors, std::optional<SceneSettings> a_settings)
	{
		auto data = GetSceneData(a_actors, a_settings);
		if (!data.result) {
			a_VM.PostError(data.result.GetErrorMessage(), a_stackID, ErrorLogger::Severity::kInfo);
			return PackSceneResult(false);
		}

		uint64_t sceneId;
		if (auto res = Scene::SceneManager::WalkToAndStartScene(data.settings, sceneId); !res) {
			a_VM.PostError(res.GetErrorMessage(), a_stackID, ErrorLogger::Severity::kInfo);
			return PackSceneResult(false);
		}

		return PackSceneResult(true, sceneId);
	}

	void StopScene(std::monostate, SceneId a_id)
	{
		auto sceneId = UnpackSceneId(a_id);
		Scene::SceneManager::StopScene(sceneId);
	}

	bool SetScenePosition(std::monostate, SceneId a_id, std::string a_position)
	{
		bool successful = false;
		Scene::SceneManager::VisitScene(UnpackSceneId(a_id), [&](Scene::IScene* scn) {
			successful = scn->SetPosition(a_position);
		});
		return successful;
	}

	std::vector<RE::Actor*> GetSceneActors(std::monostate, SceneId a_id)
	{
		std::vector<RE::NiPointer<RE::Actor>> actors;
		Scene::SceneManager::VisitScene(
			UnpackSceneId(a_id), [&](Scene::IScene* scn) {
				actors = Scene::GetActorsInOrder(scn->actors);
			},
			true);

		std::vector<RE::Actor*> result;
		result.reserve(actors.size());
		for (auto& a : actors) {
			result.push_back(a.get());
		}

		return result;
	}

	std::vector<std::string> GetSceneHKXs(std::monostate, SceneId a_id)
	{
		std::vector<std::string> result;
		Scene::SceneManager::VisitScene(
			UnpackSceneId(a_id), [&](Scene::IScene* scn) {
				result = scn->QCachedHKXStrings();
			},
			true);
		return result;
	}

	std::vector<std::string> GetSceneTags(std::monostate, SceneId a_id)
	{
		std::string id = "";
		Scene::SceneManager::VisitScene(
			UnpackSceneId(a_id), [&](Scene::IScene* scn) {
				id = scn->controlSystem->QSystemID();
			},
			true);
		std::shared_ptr<const Data::Position> pos = Data::GetPosition(id);
		std::vector<std::string> result;
		if (pos != nullptr) {
			for (const auto& t : pos->tags) {
				result.emplace_back(t);
			}
		}
		return result;
	}

	void SetSceneSpeed(std::monostate, SceneId a_id, float a_speed)
	{
		auto sceneId = UnpackSceneId(a_id);
		Scene::SceneManager::VisitScene(sceneId, [&](Scene::IScene* scn) {
			scn->SetAnimMult(a_speed);
		});
	}

	float GetSceneSpeed(std::monostate, SceneId a_id)
	{
		auto sceneId = UnpackSceneId(a_id);
		float speed = 0.0f;
		Scene::SceneManager::VisitScene(
			sceneId, [&](Scene::IScene* scn) {
				speed = scn->animMult;
			},
			true);
		return speed;
	}

	SceneId GetSceneFromActor(std::monostate, RE::Actor* a_actor)
	{
		if (!a_actor) {
			return PackSceneId(0);
		}

		uint64_t targetSceneId = 0;
		Scene::SceneManager::VisitAllScenes([&](Scene::IScene* scn) {
			if (targetSceneId == 0) {
				scn->ForEachActor([&](RE::Actor* currentActor, Scene::ActorPropertyMap&) {
					if (currentActor == a_actor) {
						targetSceneId = scn->uid;
					}
				});
			}
		},
			true);

		return PackSceneId(targetSceneId);
	}

	bool IsSceneRunning(std::monostate, SceneId a_id)
	{
		bool running = false;
		Scene::SceneManager::VisitScene(UnpackSceneId(a_id), [&](Scene::IScene*) {
			running = true;
		});
		return running;
	}

	static const std::unordered_map<std::string, std::function<void(Scene::IScene*, Variable*)>> ScenePropGetters = {
		{ "locationref", [](Scene::IScene* scn, Variable* res) {
			 PackVariable(*res, scn->settings.locationRefr.get().get());
		 } },
		{ "totalduration", [](Scene::IScene* scn, Variable* res) {
			 PackVariable(*res, scn->settings.duration);
		 } },
		{ "remainingduration", [](Scene::IScene* scn, Variable* res) {
			 PackVariable(*res, scn->GetRemainingDuration());
		 } },
		{ "status", [](Scene::IScene* scn, Variable* res) {
			 PackVariable(*res, static_cast<int32_t>(scn->status));
		 } },
		{ "syncstatus", [](Scene::IScene* scn, Variable* res) {
			 PackVariable(*res, static_cast<int32_t>(scn->syncStatus));
		 } },
		{ "animationtime", [](Scene::IScene* scn, Variable* res) {
			 PackVariable(*res, scn->animTime);
		 } },
		{ "startequipset", [](Scene::IScene* scn, Variable* res) {
			 PackVariable(*res, scn->startEquipSet);
		 } },
		{ "stopequipset", [](Scene::IScene* scn, Variable* res) {
			 PackVariable(*res, scn->stopEquipSet);
		 } },
		{ "autoadvance", [](Scene::IScene* scn, Variable* res) {
			 PackVariable(*res, scn->QAutoAdvance());
		 } },
		{ "ignorecombat", [](Scene::IScene* scn, Variable* res) {
			 PackVariable(*res, scn->settings.ignoreCombat);
		 } },
		{ "positiontype", [](Scene::IScene* scn, Variable* res) {
			 PackVariable(*res, scn->controlSystem->QTypeName());
		 } },
		{ "animationid", [](Scene::IScene* scn, Variable* res) {
			 PackVariable(*res, scn->controlSystem->QAnimationID());
		 } },
		{ "positionid", [](Scene::IScene* scn, Variable* res) {
			 PackVariable(*res, scn->controlSystem->QSystemID());
		 } },
		{ "location", [](Scene::IScene* scn, Variable* res) {
			 auto arr = MakeVarArray(3);
			 *arr[0] = scn->location.x;
			 *arr[1] = scn->location.y;
			 *arr[2] = scn->location.z;
			 PackVariable(*res, std::move(arr));
		 } },
		{ "angle", [](Scene::IScene* scn, Variable* res) {
			 auto arr = MakeVarArray(3);
			 *arr[0] = scn->angle.x;
			 *arr[1] = scn->angle.y;
			 *arr[2] = scn->angle.z;
			 PackVariable(*res, std::move(arr));
		 } }
	};

	const RE::BSScript::Variable* GetSceneProperty(std::monostate, SceneId a_id, std::string a_prop)
	{
		Variable* result = new Variable();
		uint64_t id = UnpackSceneId(a_id);
		Utility::TransformStringToLower(a_prop);

		if (a_prop == "hasstarted") {
			uint8_t walkInstResult = 0;
			Scene::SceneManager::VisitWalkInstance(id, [&](Scene::SceneManager::WalkInfo&) {
				walkInstResult = 1;
			});
			if (walkInstResult == 0) {
				Scene::SceneManager::VisitScene(
					id, [&](Scene::IScene*) {
						walkInstResult = 2;
					},
					true);
			}
			if (walkInstResult != 0) {
				PackVariable(*result, walkInstResult != 1);
			}
		} else {
			auto prop = ScenePropGetters.find(a_prop);
			if (prop != ScenePropGetters.end()) {
				Scene::SceneManager::VisitScene(
					id, [&](Scene::IScene* scn) {
						prop->second(scn, result);
					},
					true);
			}
		}
		return result;
	}

	std::string_view ValidateSceneParams(std::monostate, std::vector<RE::Actor*> a_actors, std::optional<SceneSettings> a_settings)
	{
		auto data = GetSceneData(a_actors, a_settings);
		if (!data.result) {
			return data.result.GetErrorMessage();
		}

		if (auto res = Scene::SceneManager::ValidateStartSceneArgs(data.settings, false); !res) {
			return res.GetErrorMessage();
		}

		return "";
	}

	void ReEnablePlayerInput(std::monostate)
	{
		player->ReenableInputForPlayer();
	}

	/*
	|----------------|
	| Data Functions |
	|----------------|
	*/

	//Not yet implemented in PSC
	bool PositionExists(std::monostate, std::string a_pos)
	{
		return (Data::GetPosition(a_pos) != nullptr);
	}

	//Not yet implemented in PSC
	int32_t GetPositionType(std::monostate, std::string a_pos)
	{
		int32_t result = 0;
		if (auto pos = Data::GetPosition(a_pos); pos != nullptr) {
			result = pos->posType;
		}
		return result;
	}

	//Not yet implemented in PSC
	std::string GetPositionBaseAnimation(std::monostate, std::string a_pos)
	{
		std::string result = "";
		if (auto pos = Data::GetPosition(a_pos); pos != nullptr) {
			if (auto anim = pos->GetBaseAnimation(); anim != nullptr) {
				result = anim->id;
			}
		}
		return result;
	}

	//Not yet implemented in PSC
	bool AnimationExists(std::monostate, std::string a_anim)
	{
		return (Data::GetAnimation(a_anim) != nullptr);
	}

	/*
	|--------------------------|
	| Body Animation Functions |
	|--------------------------|
	*/

	bool PlayNANIM(std::monostate, RE::Actor* a_actor, std::string a_filePath, std::string a_animID, float a_transitionTime)
	{
		return BodyAnimation::GraphHook::LoadAndPlayAnimation(a_actor, USERDATA_DIR + a_filePath, a_transitionTime, a_animID);
	}

	bool StopNANIM(std::monostate, RE::Actor* a_actor, float a_transitionTime)
	{
		return BodyAnimation::GraphHook::StopAnimation(a_actor, a_transitionTime);
	}

	void SetIKChainTarget(std::monostate, RE::Actor* a_actor, std::string a_chainName, RE::TESObjectREFR* a_target)
	{
		if (!a_actor || !a_target)
			return;

		auto _3d = a_target->Get3D();
		if (!_3d)
			return;

		BodyAnimation::GraphHook::VisitGraph(a_actor, [&](BodyAnimation::NodeAnimationGraph* g) {
			g->ikManager.SetChainEnabled(a_chainName, true);
			g->ikManager.SetChainTarget(a_chainName, _3d->world);
		});
	}

	void SetIKChainEnabled(std::monostate, RE::Actor* a_actor, std::string a_chainName, bool a_enabled)
	{
		if (!a_actor)
			return;

		BodyAnimation::GraphHook::VisitGraph(a_actor, [&](BodyAnimation::NodeAnimationGraph* g) {
			g->ikManager.SetChainEnabled(a_chainName, a_enabled);
		});
	}

	/*
	|--------------------------|
	| Face Animation Functions |
	|--------------------------|
	*/

	void SetEyeCoordOverride(std::monostate, RE::Actor* a_actor, float a_eyeX, float a_eyeY)
	{
		if (!a_actor) {
			return;
		}
		FaceAnimation::FaceUpdateHook::SetEyeOverride(a_actor->GetActorHandle(), a_eyeX, a_eyeY);
	}

	void ClearEyeCoordOverride(std::monostate, RE::Actor* a_actor)
	{
		if (!a_actor) {
			return;
		}
		FaceAnimation::FaceUpdateHook::ClearEyeOverride(a_actor->GetActorHandle());
	}

	bool PlayFaceAnimation(std::monostate, RE::Actor* a_actor, std::string a_id, bool a_loop, bool a_bodySync)
	{
		if (!a_actor) {
			return false;
		}
		return FaceAnimation::FaceUpdateHook::LoadAndPlayAnimation(a_actor->GetActorHandle(), a_id, a_loop, a_bodySync);
	}

	void StopFaceAnimation(std::monostate, RE::Actor* a_actor)
	{
		if (!a_actor) {
			return;
		}
		FaceAnimation::FaceUpdateHook::StopAnimation(a_actor->GetActorHandle());
	}

	/*
	|---------------|
	| HUD Functions |
	|---------------|
	*/

	int32_t DrawRectangle(std::monostate, float a_X, float a_Y, float a_width, float a_height, int32_t a_color, float a_alpha, float a_borderSize, int32_t a_borderColor, float a_borderAlpha)
	{
		return Menu::HUDManager::DrawRectangle(a_X, a_Y, a_width, a_height, a_color, a_alpha, a_borderSize, a_borderColor, a_borderAlpha);
	}

	int32_t DrawText(std::monostate, std::string a_text, float a_X, float a_Y, float a_txtSize, int32_t a_color, float a_alpha, bool a_bold, bool a_italic, bool a_underline)
	{
		return Menu::HUDManager::DrawText(a_text, a_X, a_Y, a_txtSize, a_color, a_alpha, a_bold, a_italic, a_underline);
	}

	int32_t DrawLine(std::monostate, float a_startX, float a_startY, float a_endX, float a_endY, float a_thickness, int32_t a_color, float a_alpha)
	{
		return Menu::HUDManager::DrawLine(a_startX, a_startY, a_endX, a_endY, a_thickness, a_color, a_alpha);
	}

	void SetElementPosition(std::monostate, int32_t a_handle, float a_X, float a_Y)
	{
		Menu::HUDManager::SetElementPosition(a_handle, a_X, a_Y);
	}

	void TranslateElementTo(std::monostate, int32_t a_handle, float a_endX, float a_endY, float a_seconds)
	{
		Menu::HUDManager::TranslateElementTo(a_handle, a_endX, a_endY, a_seconds);
	}

	void StopElementTranslation(std::monostate, int32_t a_handle)
	{
		Menu::HUDManager::StopElementTranslation(a_handle);
	}

	void RemoveElement(std::monostate, int32_t a_handle)
	{
		Menu::HUDManager::RemoveElement(a_handle);
	}

	float GetElementWidth(std::monostate, int32_t a_handle)
	{
		return Menu::HUDManager::GetElementWidth(a_handle);
	}

	float GetElementHeight(std::monostate, int32_t a_handle)
	{
		return Menu::HUDManager::GetElementHeight(a_handle);
	}

	void MoveElementToFront(std::monostate, int32_t a_handle)
	{
		Menu::HUDManager::MoveElementToFront(a_handle);
	}

	void MoveElementToBack(std::monostate, int32_t a_handle)
	{
		Menu::HUDManager::MoveElementToBack(a_handle);
	}

	void SetElementMask(std::monostate, int32_t a_handle, int32_t a_maskHandle, bool a_remove)
	{
		Menu::HUDManager::SetElementMask(a_handle, a_remove ? 0 : a_maskHandle);
	}

	void AttachElementTo(std::monostate, int32_t a_handle, int32_t a_targetHandle, bool a_detach)
	{
		Menu::HUDManager::AttachElementTo(a_handle, a_detach ? 0 : a_targetHandle);
	}

	/*
	|-------------------|
	| Utility Functions |
	|-------------------|
	*/

	std::vector<float> GetLocalTransform(std::monostate, RE::TESObjectREFR* a_object, RE::TESObjectREFR* a_parent)
	{
		std::vector<float> result(6ui64, 0.0f);
		if (!a_object || !a_parent)
			return result;

		auto object3d = a_object->Get3D();
		auto parent3d = a_parent->Get3D();

		if (!object3d || !parent3d)
			return result;

		RE::NiTransform transform = object3d->world.WorldToLocal(parent3d->world);

		if (a_object->Is<RE::Actor>()) {
			transform.rotate.ToEulerAnglesZXY(result[2], result[0], result[1]);
		} else {
			transform.rotate.ToEulerAnglesXYZ(result[0], result[1], result[2]);
		}

		result[3] = transform.translate.x;
		result[4] = transform.translate.y;
		result[5] = transform.translate.z;

		return result;
	}

	bool SynchronizeAnimations(std::monostate, std::vector<RE::Actor*> a_actors)
	{
		if (a_actors.size() < 2)
			return false;

		for (auto a : a_actors) {
			if (!a)
				return false;
		}

		GameUtil::GraphTime baseTime;
		if (!BodyAnimation::SmartIdle::GetGraphTime(a_actors[0], baseTime)) {
			return false;
		}

		GameUtil::GraphTime time;
		bool allSuccessful = true;
		for (size_t i = 1; i < a_actors.size(); i++) {
			auto a = a_actors[i];
			if (!BodyAnimation::SmartIdle::GetGraphTime(a, time) || time.total < baseTime.current) {
				allSuccessful = false;
				continue;
			}
			if (!BodyAnimation::SmartIdle::SetGraphTime(a, baseTime.current)) {
				allSuccessful = false;
			}
		}

		return allSuccessful;
	}
}
	/*
	|-------------------|
	| BRIDGE		    |
	|-------------------|
	*/
namespace Papyrus::NAFBridge
{
	using namespace RE::BSScript;
	using SceneId = structure_wrapper<"NAF", "SceneId">;
	using StartSceneResult = structure_wrapper<"NAF", "StartSceneResult">;
	using SceneSettings = structure_wrapper<"NAF", "SceneSettings">;
	using SceneData = Papyrus::NAF::SceneData;
	
	void ApplyEquipmentSet(std::monostate, RE::Actor* akActor, std::string equipmentSetId)
	{
		if ((akActor == nullptr) || equipmentSetId.empty())
			return;
		Data::ApplyEquipmentSet(akActor, equipmentSetId);
	}

	bool GetPositionInstalled(std::monostate, std::string id)
	{
		if (id.empty())
			return false;
		return Data::GetPosition(id) == nullptr ? false : true;
	}

	bool ApplyMorphSet(std::monostate, RE::Actor* akActor, std::string id)
	{
		if ((akActor == nullptr) || id.empty())
			return false;
		return Data::ApplyMorphSet(akActor, id);
	}

	bool CompleteWalkForActor(std::monostate, RE::Actor* akActor)
	{
		if (!akActor) {
			return false;
		}
		bool result = false;

		Scene::SceneManager::GetSingleton()->VisitAllScenes([&](Scene::IScene* scn) {
			scn->ForEachActor([&](RE::Actor* currentActor, Scene::ActorPropertyMap&) {
				if (currentActor == akActor) {
					Scene::SceneManager::CompleteWalk(scn->uid);
					result = true;
				}
			});
		},
			true);

		return result;
	}

	bool CompleteWalkForScene(std::monostate, std::optional<SceneId> id)
	{
		if (!id || !id.has_value()) {
			return false;
		}
		uint64_t id64 = Papyrus::NAF::UnpackSceneId(id.value());
		return Scene::SceneManager::GetSingleton()->CompleteWalk(id64);
	}

	std::string FindPositionBySceneSettings(std::monostate, std::vector<RE::Actor*> a_actors, std::optional<SceneSettings> a_settings)
	{
		std::string result;
		if (a_actors.size() < 1) {
			result = { Scene::kNoActors };
			if (PRINT_LOG) 
				logger::info("Bridge : FindPositionBySceneSettings failed : kNoActors : {}", result);
			return "";
		}

		std::string a_position = "";
		std::string includeTags = "";
		std::string excludeTags = "";
		std::string requireTags = "";
		RE::TESObjectREFR* posRefr = nullptr;

		if (a_settings.has_value()) {
			auto& s = a_settings.value();
			if (auto v = s.find<std::string>("position", true); v) {
				a_position = v.value();
				if (!a_position.empty()) {
					return a_position;
				}
			}
			if (auto v = s.find<RE::TESObjectREFR*>("positionRef", true); v) {
				posRefr = v.value();
			}
			if (auto v = s.find<std::string>("includeTags", true); v) {
				includeTags = v.value();
			}
			if (auto v = s.find<std::string>("excludeTags", true); v) {
				excludeTags = v.value();
			}
			if (auto v = s.find<std::string>("requireTags", true); v) {
				requireTags = v.value();
			}
		}

		std::optional<Data::Global::TagFilter> tFilter;
		if (includeTags.size() > 0 || excludeTags.size() > 0 || requireTags.size() > 0) {
			Utility::TransformStringToLower(includeTags);
			Utility::TransformStringToLower(excludeTags);
			Utility::TransformStringToLower(requireTags);
			tFilter.emplace(
				Utility::SplitString(includeTags, ","),
				Utility::SplitString(excludeTags, ","),
				Utility::SplitString(requireTags, ","));
		}

		Data::AnimationFilter filter(a_actors);
		if (filter.numTotalActors != a_actors.size()) {
			result = { Scene::kInvalidActor };
			if (PRINT_LOG) 
				logger::info("Bridge : FindPositionBySceneSettings failed : kInvalidActor : {}", result);
			return "";
		}
		auto positions = Data::Global::GetFilteredPositions(filter, a_position.size() < 1, false, posRefr, false, tFilter);

		if (a_position.size() < 1) {
			if (positions.size() > 0) {
				return Utility::SelectRandom(positions);
			} else {
				result = { Scene::kNoAvailablePositions };
				if (PRINT_LOG)
					logger::info("Bridge : FindPositionBySceneSettings failed : kNoAvailablePositions : {}", result);
				return "";
			}
		} else if (!Utility::VectorContains(positions, a_position)) {
			result = { Scene::kSpecifiedPositionNotAvailable };
			if (PRINT_LOG)
				logger::info("Bridge : FindPositionBySceneSettings failed : kSpecifiedPositionNotAvailable : {}", result);
			return "";
		}

		if (PRINT_LOG)
			logger::info("Bridge : FindPositionBySceneSettings failed : unknown reason");
		return "";
	}

	std::string_view ValidateSceneParamsIgnoreInScene(std::monostate, std::vector<RE::Actor*> a_actors, std::optional<SceneSettings> a_settings)
	{
		auto data = Papyrus::NAF::GetSceneData(a_actors, a_settings);
		if (!data.result) {
			return std::string(data.result.GetErrorMessage()).c_str();
		}

		if (auto res = Scene::SceneManager::GetSingleton()->ValidateStartSceneArgs(data.settings, true); !res) {
			return std::string(res.GetErrorMessage()).c_str();
		}

		return "";
	}

	SceneData GetSceneFromSceneSettings(std::vector<RE::Actor*>& a_actors, std::optional<SceneSettings>& a_settings)
	{
		SceneData resultData;
		Scene::SceneSettings& settings = resultData.settings;

		if (a_actors.size() < 1) {
			resultData.result = { Scene::kNoActors };
			return resultData;
		}

		std::string a_position = "";
		std::string includeTags = "";
		std::string excludeTags = "";
		std::string requireTags = "";
		RE::TESObjectREFR* posRefr = nullptr;

		if (a_settings.has_value()) {
			auto& s = a_settings.value();
			if (auto v = s.find<std::string>("position", true); v) {
				a_position = v.value();
			}
			if (auto v = s.find<float>("duration", true); v) {
				settings.duration = v.value();
			}
			if (auto v = s.find<RE::TESObjectREFR*>("positionRef", true); v) {
				posRefr = v.value();
			}
			if (auto v = s.find<std::string>("includeTags", true); v) {
				includeTags = v.value();
			}
			if (auto v = s.find<std::string>("excludeTags", true); v) {
				excludeTags = v.value();
			}
			if (auto v = s.find<std::string>("requireTags", true); v) {
				requireTags = v.value();
			}
			if (auto v = s.find<bool>("forceNPCControlled", true); v) {
				settings.autoAdvance = v.value();
			}
			if (auto v = s.find<bool>("ignoreCombat", true); v) {
				settings.ignoreCombat = v.value();
			}
		}

		std::string position = Papyrus::NAFBridge::FindPositionBySceneSettings(std::monostate(), a_actors, a_settings);
		if (position == "") {
			resultData.result = { Scene::kNoAvailablePositions };
			return resultData;
		}
		auto targetPosition = Data::GetPosition(position);
		if (!targetPosition) {
			resultData.result = { Scene::kNoAvailablePositions };
			return resultData;
		}

		settings.startPosition = std::shared_ptr<const Data::Position>(targetPosition);

		if (posRefr == nullptr) {
			settings.locationRefr = a_actors[0]->GetHandle();
		} else {
			settings.locationRefr = posRefr->GetHandle();
		}

		settings.actors.reserve(a_actors.size());
		for (auto& a : a_actors) {
			settings.actors.push_back(a->GetActorHandle());
		}

		return resultData;
	}

	RE::BGSListForm* GetFurnitureList(std::monostate, RE::BGSListForm* formlist)
	{
		if (formlist == nullptr)
			return nullptr;
		formlist->ClearData();

		for (auto& el : Data::Global::Furnitures) {
			auto furniture = el.second.second;
			for (auto& form : furniture.get()->forms)
			{
				formlist->arrayOfForms.push_back(form.get());
			}
		}
		return formlist;
	}

	using OverlaySet = RE::BSScript::structure_wrapper<"NAFBridge", "OverlaySet">;

	OverlaySet GetOverlay(std::monostate, std::string id)
	{
		OverlaySet res;

		auto ovrl = Data::GetOverlaySet(id);

		auto setNull = [&]() {
			res.insert("template", ""s);
			res.insert("duration", 0);
			res.insert("isFemale", false);
			res.insert("alpha", 0);
			logger::critical("GetOverlay returned empty struct for id {}!", id);
		};

		if (ovrl && (ovrl->overlays.size() > 0)) {
			std::random_device rd;
			std::mt19937 gen(rd());
			std::uniform_int_distribution<> distr(0, (ovrl->overlays.size() - 1));
			int rnd = distr(gen);

			if (ovrl->overlays[rnd].Template != "") {
				auto tmplt = ovrl->overlays[rnd];

				res.insert("template", tmplt.Template);
				res.insert("duration", ovrl->duration);
				res.insert("isFemale", tmplt.isFemale);
				res.insert("alpha", tmplt.alpha);
				if (PRINT_LOG) {
					logger::info("GetOverlay request {}, result {}, dur:{}, isFemale:{}, alpha:{}", id, tmplt.Template, ovrl->duration, tmplt.isFemale, tmplt.alpha);
				}
			} else
				setNull();
		} else {
			setNull();
		}
		return res;
	}
}

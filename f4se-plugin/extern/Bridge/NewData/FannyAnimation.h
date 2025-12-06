#pragma once
#include "LooksMenu/LooksMenu.h"
#include "Bridge/IniParser/Ini.h"
#include "Bridge/Consts.h"

namespace Data
{
	/* Класс FannyAnimation для анимации для вагины и ануса. Во время входа пениса в вагину/анус - выполнять анимацию расширения вагины/ануса,
	* Во время выхода - анимацию сужения. То же самое нужно делать для рук.
	* Нужно получать список актёров и команду на начало/окончание проникновения, после этого отслеживать положение пениса и рук относительно вагины/ануса, и при достаточной близости запускать анимации сужения/расширения.
	* Для этого нужно знать:
		* Мы знаем что узел кончика пениса называется "Penis_05", 
		* Глубокая точка вагины это "Pelvis_skin"
		* Глубокая точка ануса это "Pelvis_Rear_skin"
		* Узел руки "LArm_Hand" и "RArm_Hand"
		* Морф вагины BodyMorphInterface - "VaginaPenetrate":
		* Полностью раскрытая вагина: 1.0
		* Закрытая вагина: 0
		* Морфы ануса "ButtcheeksSpread", "AnusPenetrate", "AnusBack" 
		* Полностью раскрытый "ButtcheeksSpread": 0.5
		* Полностью раскрытый "AnusPenetrate": 1.0
		* Полностью раскрытый "AnusBack": 1.0
		* Закрытый "ButtcheeksSpread": 0.0
		* Закрытый "AnusPenetrate": 0.0
		* Закрытый "AnusBack": 0.0
		* 
		* Для работы с морфами использовать LooksMenu
		* 
	* Что бы сократить лишнюю нагрузку нужно не производить лишние рассчёты можно показывать анимацию лишь когда камера находится не далее чем 200 ед. расстояния от точки вагины в пространстве. Камера не может моментально поменять положение (точнее это редкость) по этому допустимо пересчитывать расстояние раз в 1 секунду.
	*/

	class FannyAnimation
	{
	public:
		// Node names
		static constexpr const char* NODE_PENIS_TIP = "Penis_05";
		static constexpr const char* NODE_VAGINA_DEEP = "Pelvis_skin";
		static constexpr const char* NODE_ANUS_DEEP = "Pelvis_Rear_skin";
		static constexpr const char* NODE_LEFT_FINGER = "LArm_Finger33";
		static constexpr const char* NODE_RIGHT_FINGER = "RArm_Finger33";

		// Morph names
		static constexpr const char* MORPH_VAGINA_PENETRATE = "VaginaPenetrate";
		static constexpr const char* MORPH_BUTTCHEEKS_SPREAD = "ButtcheeksSpread";
		static constexpr const char* MORPH_ANUS_PENETRATE = "AnusPenetrate";
		static constexpr const char* MORPH_ANUS_BACK = "AnusBack";

		// Morph max values
		static constexpr float MORPH_VAGINA_MAX = 1.0f;
		static constexpr float MORPH_BUTTCHEEKS_SPREAD_MAX = 0.5f;
		static constexpr float MORPH_ANUS_PENETRATE_MAX = 1.0f;
		static constexpr float MORPH_ANUS_BACK_MAX = 1.0f;

		// Distance thresholds for PENIS (in game units)
		static constexpr float PENIS_PENETRATION_START_DISTANCE = 15.0f;
		static constexpr float PENIS_PENETRATION_FULL_DISTANCE = 2.0f;

		// Distance thresholds for FINGER (smaller, ~1/4 of penis)
		static constexpr float FINGER_PENETRATION_START_DISTANCE = 45.0f;
		static constexpr float FINGER_PENETRATION_FULL_DISTANCE = 15.f;

		// Camera distance optimization
		static constexpr float CAMERA_MAX_DISTANCE = 200.0f;
		static constexpr float CAMERA_CHECK_INTERVAL = 1.0f;

		static RE::BGSKeyword* GetMorphKWD()
		{
			static auto kwd = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSKeyword>(0x800, "AAF.esm");
			return kwd;
		};

		enum class PenetratorType
		{
			None,
			Penis,
			LeftFinger,
			RightFinger
		};

		enum class TargetType
		{
			None,
			Vagina,
			Anus
		};

		// Tracks which penetrator is assigned to which target for a receiver
		struct PenetratorAssignment
		{
			RE::ActorHandle penetratorActor;
			PenetratorType penetratorType = PenetratorType::None;
			TargetType targetType = TargetType::None;
			float currentFactor = 0.0f;  // Current penetration factor (0-1)
		};

		struct ReceiverState
		{
			RE::ActorHandle receiverHandle;
			std::vector<PenetratorAssignment> assignments;  // Active penetrator assignments
			float currentVaginaMorph = 0.0f;
			float currentButtcheeksMorph = 0.0f;
			float currentAnusPenetrateMorph = 0.0f;
			float currentAnusBackMorph = 0.0f;
			bool isActive = false;
			bool isCameraClose = true;
			float cameraCheckTimer = 0.0f;
		};

	private:
		std::vector<ReceiverState> receiverStates;
		std::vector<RE::ActorHandle> allActors;  // All actors in scene
		bool isEnabled = false;
		float morphTransitionSpeed = 5.0f;

	public:
		FannyAnimation() = default;
		~FannyAnimation()
		{
			StopTracking();
		};

		void StartTracking(const std::vector<RE::NiPointer<RE::Actor>>& actors)
		{
			if (!LooksMenu::isInstalled) {
				return;
			}

			if (!GetMorphKWD()) {
				logger::error("FannyAnimation: Morph keyword not found, cannot start tracking.");
				return;
			}

			bool enable = true;
			std::string file;
			if (std::filesystem::exists(MCM_INI_PATH)) {
				file = MCM_INI_PATH;
			} else if (std::filesystem::exists(MCM_INI_PATH_ALT)) {
				file = MCM_INI_PATH_ALT;
			}

			if (!file.empty()) {
				ini::map map(file);
				enable = map.at("Settings/bAnimateFannies", false);
			}

			if (!enable) {
				return;
			}

			receiverStates.clear();
			allActors.clear();
			isEnabled = true;

			// Store all actor handles
			for (const auto& actor : actors) {
				if (actor) {
					allActors.push_back(actor->GetActorHandle());
				}
			}

			// Create receiver states for actors that have receiving nodes
			for (const auto& actor : actors) {
				if (!actor)
					continue;

				if (HasReceivingNodes(actor.get())) {
					ReceiverState state;
					state.receiverHandle = actor->GetActorHandle();
					state.isActive = true;
					state.isCameraClose = true;
					state.cameraCheckTimer = 0.0f;
					receiverStates.push_back(state);
				}
			}
		}

		void StopTracking()
		{
			if (!LooksMenu::isInstalled) {
				return;
			}

			for (auto& state : receiverStates) {
				if (auto receiver = state.receiverHandle.get(); receiver != nullptr) {
					ResetMorphs(receiver.get());
				}
			}

			receiverStates.clear();
			allActors.clear();
			isEnabled = false;
		}

		void Update(float deltaTime)
		{
			if (!isEnabled || !LooksMenu::isInstalled) {
				return;
			}

			for (auto& state : receiverStates) {
				if (!state.isActive)
					continue;

				auto receiver = state.receiverHandle.get();
				if (!receiver) {
					state.isActive = false;
					continue;
				}

				// Update camera distance check timer
				state.cameraCheckTimer -= deltaTime;
				if (state.cameraCheckTimer <= 0.0f) {
					state.cameraCheckTimer = CAMERA_CHECK_INTERVAL;
					state.isCameraClose = IsCameraCloseToReceiver(receiver.get());
				}

				if (state.isCameraClose) {
					UpdateReceiverState(state, deltaTime);
				}
			}
		}

		void SetMorphTransitionSpeed(float speed) { morphTransitionSpeed = speed; }
		float GetMorphTransitionSpeed() const { return morphTransitionSpeed; }
		bool IsEnabled() const { return isEnabled; }

	private:
		static RE::NiPoint3 GetCameraPosition()
		{
			auto playerCamera = RE::PlayerCamera::GetSingleton();
			if (playerCamera && playerCamera->cameraRoot) {
				return playerCamera->cameraRoot->world.translate;
			}
			return RE::NiPoint3();
		}

		static bool IsCameraCloseToReceiver(RE::Actor* receiver)
		{
			if (!receiver)
				return false;

			RE::NiPoint3 cameraPos = GetCameraPosition();
			RE::NiPoint3 receiverPos = GetNodeWorldPosition(receiver, NODE_VAGINA_DEEP);

			if (IsZeroPosition(receiverPos)) {
				receiverPos = GetNodeWorldPosition(receiver, NODE_ANUS_DEEP);
			}

			float distance = CalculateDistance(cameraPos, receiverPos);
			return distance <= CAMERA_MAX_DISTANCE;
		}

		static bool IsZeroPosition(const RE::NiPoint3& pos)
		{
			return pos.x == 0.0f && pos.y == 0.0f && pos.z == 0.0f;
		}

		static bool HasReceivingNodes(RE::Actor* actor)
		{
			if (!actor)
				return false;
			auto root = actor->Get3D();
			if (!root)
				return false;

			return root->GetObjectByName(NODE_VAGINA_DEEP) != nullptr ||
			       root->GetObjectByName(NODE_ANUS_DEEP) != nullptr;
		}

		static RE::NiPoint3 GetNodeWorldPosition(RE::Actor* actor, const char* nodeName)
		{
			if (!actor)
				return RE::NiPoint3();
			auto root = actor->Get3D();
			if (!root)
				return RE::NiPoint3();

			auto node = root->GetObjectByName(nodeName);
			if (!node)
				return RE::NiPoint3();

			return node->world.translate;
		}

		static float CalculateDistance(const RE::NiPoint3& a, const RE::NiPoint3& b)
		{
			float dx = a.x - b.x;
			float dy = a.y - b.y;
			float dz = a.z - b.z;
			return std::sqrt(dx * dx + dy * dy + dz * dz);
		}

		static float CalculatePenetrationFactor(float distance, PenetratorType type)
		{
			float startDist, fullDist;

			if (type == PenetratorType::Penis) {
				startDist = PENIS_PENETRATION_START_DISTANCE;
				fullDist = PENIS_PENETRATION_FULL_DISTANCE;
			} else {
				// Fingers are thinner
				startDist = FINGER_PENETRATION_START_DISTANCE;
				fullDist = FINGER_PENETRATION_FULL_DISTANCE;
			}

			if (distance >= startDist) {
				return 0.0f;
			}
			if (distance <= fullDist) {
				return 1.0f;
			}

			float range = startDist - fullDist;
			float normalizedDistance = distance - fullDist;
			return 1.0f - (normalizedDistance / range);
		}

		static float LerpMorph(float current, float target, float speed, float deltaTime)
		{
			float diff = target - current;
			float maxChange = speed * deltaTime;

			if (std::abs(diff) <= maxChange) {
				return target;
			}

			return current + (diff > 0 ? maxChange : -maxChange);
		}

		struct PenetratorInfo
		{
			RE::ActorHandle actorHandle;
			PenetratorType type;
			RE::NiPoint3 position;
			bool isValid;
		};

		std::vector<PenetratorInfo> GatherPenetrators(RE::Actor* receiver)
		{
			std::vector<PenetratorInfo> penetrators;
			RE::ActorHandle receiverHandle = receiver->GetActorHandle();

			for (const auto& actorHandle : allActors) {
				auto actor = actorHandle.get();
				if (!actor)
					continue;

				auto root = actor->Get3D();
				if (!root)
					continue;

				bool isSelf = (actorHandle == receiverHandle);

				// Penis - only from OTHER actors (no self-penetration with penis)
				if (!isSelf && root->GetObjectByName(NODE_PENIS_TIP)) {
					PenetratorInfo info;
					info.actorHandle = actorHandle;
					info.type = PenetratorType::Penis;
					info.position = GetNodeWorldPosition(actor.get(), NODE_PENIS_TIP);
					info.isValid = !IsZeroPosition(info.position);
					if (info.isValid) {
						penetrators.push_back(info);
					}
				}

				// Left finger - allowed from ANY actor (including self for masturbation)
				if (root->GetObjectByName(NODE_LEFT_FINGER)) {
					PenetratorInfo info;
					info.actorHandle = actorHandle;
					info.type = PenetratorType::LeftFinger;
					info.position = GetNodeWorldPosition(actor.get(), NODE_LEFT_FINGER);
					info.isValid = !IsZeroPosition(info.position);
					if (info.isValid) {
						penetrators.push_back(info);
					}
				}

				// Right finger - allowed from ANY actor (including self for masturbation)  
				if (root->GetObjectByName(NODE_RIGHT_FINGER)) {
					PenetratorInfo info;
					info.actorHandle = actorHandle;
					info.type = PenetratorType::RightFinger;
					info.position = GetNodeWorldPosition(actor.get(), NODE_RIGHT_FINGER);
					info.isValid = !IsZeroPosition(info.position);
					if (info.isValid) {
						penetrators.push_back(info);
					}
				}
			}

			return penetrators;
		}

		void UpdateReceiverState(ReceiverState& state, float deltaTime)
		{
			auto receiver = state.receiverHandle.get();
			if (!receiver)
				return;

			auto receiverRoot = receiver->Get3D();
			if (!receiverRoot)
				return;

			// Get target positions
			RE::NiPoint3 vaginaPos = GetNodeWorldPosition(receiver.get(), NODE_VAGINA_DEEP);
			RE::NiPoint3 anusPos = GetNodeWorldPosition(receiver.get(), NODE_ANUS_DEEP);
			bool hasVagina = !IsZeroPosition(vaginaPos) && receiverRoot->GetObjectByName(NODE_VAGINA_DEEP);
			bool hasAnus = !IsZeroPosition(anusPos) && receiverRoot->GetObjectByName(NODE_ANUS_DEEP);

			// Gather all potential penetrators (excluding self)
			auto penetrators = GatherPenetrators(receiver.get());

			// Clear old assignments
			state.assignments.clear();

			// For each penetrator, find the closest valid target
			// A penetrator can only be assigned to ONE target (vagina OR anus)
			for (const auto& pen : penetrators) {
				float distToVagina = hasVagina ? CalculateDistance(pen.position, vaginaPos) : 9999.0f;
				float distToAnus = hasAnus ? CalculateDistance(pen.position, anusPos) : 9999.0f;

				// Determine which target is closer and within range
				float startDist = (pen.type == PenetratorType::Penis) ?
				                      PENIS_PENETRATION_START_DISTANCE :
				                      FINGER_PENETRATION_START_DISTANCE;

				TargetType target = TargetType::None;
				float distance = 9999.0f;

				if (distToVagina < distToAnus && distToVagina < startDist) {
					target = TargetType::Vagina;
					distance = distToVagina;
				} else if (distToAnus < distToVagina && distToAnus < startDist) {
					target = TargetType::Anus;
					distance = distToAnus;
				}

				if (target != TargetType::None) {
					PenetratorAssignment assignment;
					assignment.penetratorActor = pen.actorHandle;
					assignment.penetratorType = pen.type;
					assignment.targetType = target;
					assignment.currentFactor = CalculatePenetrationFactor(distance, pen.type);
					state.assignments.push_back(assignment);
				}
			}

			// Calculate total penetration factors for each target
			// Take the maximum factor from all penetrators targeting each hole
			float maxVaginaFactor = 0.0f;
			float maxAnusFactor = 0.0f;

			for (const auto& assignment : state.assignments) {
				if (assignment.targetType == TargetType::Vagina) {
					maxVaginaFactor = std::max(maxVaginaFactor, assignment.currentFactor);
				} else if (assignment.targetType == TargetType::Anus) {
					maxAnusFactor = std::max(maxAnusFactor, assignment.currentFactor);
				}
			}

			// Calculate target morph values
			float targetVaginaMorph = maxVaginaFactor * MORPH_VAGINA_MAX;
			float targetButtcheeksMorph = maxAnusFactor * MORPH_BUTTCHEEKS_SPREAD_MAX;
			float targetAnusPenetrateMorph = maxAnusFactor * MORPH_ANUS_PENETRATE_MAX;
			float targetAnusBackMorph = maxAnusFactor * MORPH_ANUS_BACK_MAX;

			// Smoothly transition morphs
			state.currentVaginaMorph = LerpMorph(state.currentVaginaMorph, targetVaginaMorph, morphTransitionSpeed, deltaTime);
			state.currentButtcheeksMorph = LerpMorph(state.currentButtcheeksMorph, targetButtcheeksMorph, morphTransitionSpeed, deltaTime);
			state.currentAnusPenetrateMorph = LerpMorph(state.currentAnusPenetrateMorph, targetAnusPenetrateMorph, morphTransitionSpeed, deltaTime);
			state.currentAnusBackMorph = LerpMorph(state.currentAnusBackMorph, targetAnusBackMorph, morphTransitionSpeed, deltaTime);

			// Apply morphs
			ApplyMorphs(receiver.get(), state);
		}

		static void ApplyMorphs(RE::Actor* actor, const ReceiverState& state)
		{
			if (!actor || !LooksMenu::isInstalled)
				return;

			// Apply vagina morph
			if (state.currentVaginaMorph > 0.001f) {
				LooksMenu::SetMorph(actor, MORPH_VAGINA_PENETRATE, GetMorphKWD(), state.currentVaginaMorph);
			} else {
				LooksMenu::RemoveMorphsByName(actor, MORPH_VAGINA_PENETRATE);
			}

			// Apply anus morphs
			if (state.currentButtcheeksMorph > 0.001f || state.currentAnusPenetrateMorph > 0.001f || state.currentAnusBackMorph > 0.001f) {
				LooksMenu::SetMorph(actor, MORPH_BUTTCHEEKS_SPREAD, GetMorphKWD(), state.currentButtcheeksMorph);
				LooksMenu::SetMorph(actor, MORPH_ANUS_PENETRATE, GetMorphKWD(), state.currentAnusPenetrateMorph);
				LooksMenu::SetMorph(actor, MORPH_ANUS_BACK, GetMorphKWD(), state.currentAnusBackMorph);
			} else {
				LooksMenu::RemoveMorphsByName(actor, MORPH_BUTTCHEEKS_SPREAD);
				LooksMenu::RemoveMorphsByName(actor, MORPH_ANUS_PENETRATE);
				LooksMenu::RemoveMorphsByName(actor, MORPH_ANUS_BACK);
			}

			LooksMenu::UpdateMorphs(actor);
		}

		static void ResetMorphs(RE::Actor* actor)
		{
			if (!actor || !LooksMenu::isInstalled)
				return;

			LooksMenu::RemoveMorphsByKeyword(actor, GetMorphKWD());
			LooksMenu::UpdateMorphs(actor);
		}
	};
}

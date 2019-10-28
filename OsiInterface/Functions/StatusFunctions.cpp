#include <stdafx.h>
#include "FunctionLibrary.h"
#include <OsirisProxy.h>
#include "PropertyMaps.h"

namespace osidbg
{
	FunctionHandle StatusIteratorEventHandle;
	FunctionHandle StatusAttemptEventHandle;
	FunctionHandle HitPrepareEventHandle;
	FunctionHandle HitEventHandle;
	FunctionHandle HealEventHandle;

	namespace func
	{
		void IterateCharacterStatuses(OsiArgumentDesc const & args)
		{
			auto characterGuid = args[0].String;
			auto character = FindCharacterByNameGuid(characterGuid);
			if (character == nullptr) {
				OsiError("Character " << characterGuid << " does not exist!");
				return;
			}

			auto eventName = args[1].String;
			if (character->StatusMachine != nullptr) {
				auto & statuses = character->StatusMachine->Statuses.Set;
				for (uint32_t index = 0; index < statuses.Size; index++) {
					auto status = statuses.Buf[index];
					auto eventArgs = OsiArgumentDesc::Create(OsiArgumentValue{ ValueType::String, eventName });
					eventArgs->Add(OsiArgumentValue{ ValueType::GuidString, characterGuid });
					eventArgs->Add(OsiArgumentValue{ ValueType::String, status->StatusId.Str });
					eventArgs->Add(OsiArgumentValue{ (int64_t)status->StatusHandle });

					gOsirisProxy->GetCustomFunctionInjector().ThrowEvent(StatusIteratorEventHandle, eventArgs);

					delete eventArgs;
				}
			}
		}

		esv::Status * GetStatusHelper(OsiArgumentDesc const & args)
		{
			auto character = FindCharacterByNameGuid(args[0].String);
			if (character == nullptr) {
				OsiError("Character " << args[0].String << " does not exist!");
				return nullptr;
			}

			ObjectHandle statusHandle{ args[1].Int64 };
			auto status = character->GetStatusByHandle(statusHandle);
			if (status == nullptr) {
				OsiError("No status found with handle " << (int64_t)statusHandle);
				return nullptr;
			}

			return status;
		}

		bool StatusGetHandle(OsiArgumentDesc & args)
		{
			auto character = FindCharacterByNameGuid(args[0].String);
			if (character == nullptr) {
				OsiError("Character " << args[0].String << " does not exist!");
				return false;
			}

			if (character->StatusMachine == nullptr) {
				OsiError("Character " << args[0].String << " has no StatusManager!");
				return false;
			}

			auto statusId = ToFixedString(args[1].String);
			if (!statusId) {
				// No fixed string with this ID --> invalid status name
				OsiWarn("Status " << args[1].String << " not in string table, possibly invalid status name?");
				return false;
			}

			auto & statuses = character->StatusMachine->Statuses.Set;
			for (uint32_t index = 0; index < statuses.Size; index++) {
				auto status = statuses.Buf[index];
				if (status->StatusId == statusId) {
					args[2].Int64 = (int64_t)status->StatusHandle;
					return true;
				}
			}

			return false;
		}

		template <OsiPropertyMapType Type>
		bool StatusGetAttribute(OsiArgumentDesc & args)
		{
			auto status = GetStatusHelper(args);
			if (status == nullptr) {
				return false;
			}

			switch (status->GetStatusId()) {
				case StatusType::Hit:
				{
					auto hit = static_cast<esv::StatusHit *>(status);
					if (OsirisPropertyMapGet(gHitDamageInfoPropertyMap, &hit->DamageInfo, args, 2, Type, false)) {
						return true;
					} else {
						return OsirisPropertyMapGet(gStatusHitPropertyMap, hit, args, 2, Type);
					}
				}

				case StatusType::Heal:
				{
					auto heal = static_cast<esv::StatusHeal *>(status);
					return OsirisPropertyMapGet(gStatusHealPropertyMap, heal, args, 2, Type);
				}

				case StatusType::Healing:
				{
					auto healing = static_cast<esv::StatusHealing *>(status);
					return OsirisPropertyMapGet(gStatusHealingPropertyMap, healing, args, 2, Type);
				}

				case StatusType::Consume:
				case StatusType::DarkAvenger:
				case StatusType::DecayingTouch:
				case StatusType::Remorse:
				case StatusType::WindWalker:
				case StatusType::Floating:
				case StatusType::Incapacitated:
				case StatusType::Polymorphed:
				case StatusType::ShacklesOfPain:
				case StatusType::ShacklesOfPainCaster:
				case StatusType::Charmed:
				case StatusType::ExtraTurn:
				case StatusType::Damage:
				case StatusType::Blind:
				case StatusType::Encumbered:
				case StatusType::Fear:
				case StatusType::InfectiousDiseased:
				case StatusType::Invisible:
				case StatusType::Muted:
				case StatusType::Stance:
				case StatusType::Leadership:
				case StatusType::Adrenaline:
				case StatusType::LingeringWounds:
				case StatusType::SpiritVision:
				case StatusType::Overpowered:
				case StatusType::Combustion:
				case StatusType::GuardianAngel:
				case StatusType::Challenge:
				case StatusType::Disarmed:
				case StatusType::HealSharing:
				case StatusType::HealSharingCaster:
				case StatusType::ActiveDefense:
				case StatusType::Spark:
				case StatusType::PlayDead:
				case StatusType::Deactivated:
				{
					auto consume = static_cast<esv::StatusConsume *>(status);
					return OsirisPropertyMapGet(gStatusConsumePropertyMap, consume, args, 2, Type);
				}

				default:
					return OsirisPropertyMapGet(gStatusPropertyMap, status, args, 2, Type);
			}
		}

		template <OsiPropertyMapType Type>
		void StatusSetAttribute(OsiArgumentDesc const & args)
		{
			auto status = GetStatusHelper(args);
			if (status == nullptr) {
				return;
			}

			switch (status->GetStatusId()) {
				case StatusType::Hit:
				{
					auto hit = static_cast<esv::StatusHit *>(status);
					if (!OsirisPropertyMapSet(gHitDamageInfoPropertyMap, &hit->DamageInfo, args, 2, Type, false)) {
						OsirisPropertyMapSet(gStatusHitPropertyMap, hit, args, 2, Type);
					}
					break;
				}

				case StatusType::Heal:
				{
					auto heal = static_cast<esv::StatusHeal *>(status);
					OsirisPropertyMapSet(gStatusHealPropertyMap, heal, args, 2, Type);
					break;
				}

				case StatusType::Healing:
				{
					auto healing = static_cast<esv::StatusHealing *>(status);
					OsirisPropertyMapSet(gStatusHealingPropertyMap, healing, args, 2, Type);
					break;
				}

				case StatusType::Consume:
				case StatusType::DarkAvenger:
				case StatusType::DecayingTouch:
				case StatusType::Remorse:
				case StatusType::WindWalker:
				case StatusType::Floating:
				case StatusType::Incapacitated:
				case StatusType::Polymorphed:
				case StatusType::ShacklesOfPain:
				case StatusType::ShacklesOfPainCaster:
				case StatusType::Charmed:
				case StatusType::ExtraTurn:
				case StatusType::Damage:
				case StatusType::Blind:
				case StatusType::Encumbered:
				case StatusType::Fear:
				case StatusType::InfectiousDiseased:
				case StatusType::Invisible:
				case StatusType::Muted:
				case StatusType::Stance:
				case StatusType::Leadership:
				case StatusType::Adrenaline:
				case StatusType::LingeringWounds:
				case StatusType::SpiritVision:
				case StatusType::Overpowered:
				case StatusType::Combustion:
				case StatusType::GuardianAngel:
				case StatusType::Challenge:
				case StatusType::Disarmed:
				case StatusType::HealSharing:
				case StatusType::HealSharingCaster:
				case StatusType::ActiveDefense:
				case StatusType::Spark:
				case StatusType::PlayDead:
				case StatusType::Deactivated:
				{
					auto consume = static_cast<esv::StatusConsume *>(status);
					OsirisPropertyMapSet(gStatusConsumePropertyMap, consume, args, 2, Type);
					break;
				}

				default:
					OsirisPropertyMapSet(gStatusPropertyMap, status, args, 2, Type);
					break;
			}
		}

		void StatusPreventApply(OsiArgumentDesc const & args)
		{
			auto character = FindCharacterByNameGuid(args[0].String);
			auto statusHandle = ObjectHandle{ args[1].Int64 };
			auto preventApply = args[2].Int32;

			if (character == nullptr) {
				OsiError("Character " << args[0].String << " does not exist!");
				return;
			}

			auto status = gPendingStatuses.Find(character, statusHandle);
			if (status == nullptr) {
				OsiError("No pending status found with handle " << (int64_t)statusHandle);
				return;
			}

			status->PreventApply = (preventApply != 0);
		}

		template <class T>
		T * ConstructStatus(esv::StatusMachine * statusMachine, char const * statusId, StatusType type)
		{
			auto statusIdFs = ToFixedString(statusId);
			T * status{ nullptr };

			if (statusIdFs) {
				status = (T *)gOsirisProxy->GetLibraryManager().StatusMachineCreateStatus(statusMachine, statusIdFs, 0);
			}

			if (status == nullptr) {
				OsiError("Status does not exist: " << statusId);
				return nullptr;
			}

			if (status->GetStatusId() != type) {
				// TODO - dangling status ptr!
				OsiError("Status has incorrect type: " << statusId);
				return nullptr;
			}

			return status;
		}


		bool ApplyActiveDefense(OsiArgumentDesc & args)
		{
			auto characterGuid = args[0].String;
			auto statusId = args[1].String;
			auto lifeTime = args[2].Float;

			auto character = FindCharacterByNameGuid(characterGuid);
			if (character == nullptr) {
				OsiError("Character " << characterGuid << " does not exist!");
				return false;
			}

			auto statusMachine = character->StatusMachine;
			if (!statusMachine) {
				OsiError("Character has no StatusMachine!");
				return false;
			}

			auto status = ConstructStatus<esv::StatusActiveDefense>(statusMachine, statusId, StatusType::ActiveDefense);
			if (status == nullptr) {
				return false;
			}
			
			if (lifeTime < 0.0f) {
				status->Flags0 |= SF0_KeepAlive;
				status->CurrentLifeTime = 1.0f;
			} else {
				status->Flags0 |= SF0_IsLifeTimeSet;
				status->LifeTime = lifeTime;
				status->CurrentLifeTime = lifeTime;
			}

			ObjectHandle handle;
			character->GetObjectHandle(&handle);

			status->TargetHandle = handle;
			status->TargetPos = *character->GetTranslate();

			gOsirisProxy->GetLibraryManager().StatusMachineApplyStatus(statusMachine, status);
			args[3].Int64 = (int64_t)status->StatusHandle;
			return true;
		}


		bool ApplyDamageOnMove(OsiArgumentDesc & args)
		{
			auto characterGuid = args[0].String;
			auto statusId = args[1].String;
			auto sourceCharacterGuid = args[2].String;
			auto lifeTime = args[3].Float;
			auto distancePerDamage = args[4].Float;

			auto character = FindCharacterByNameGuid(characterGuid);
			if (character == nullptr) {
				OsiError("Character " << characterGuid << " does not exist!");
				return false;
			}

			auto statusMachine = character->StatusMachine;
			if (!statusMachine) {
				OsiError("Character has no StatusMachine!");
				return false;
			}

			if (distancePerDamage <= 0.01) {
				OsiError("DistancePerDamage must be a positive value!");
				return false;
			}

			auto status = ConstructStatus<esv::StatusDamageOnMove>(statusMachine, statusId, StatusType::DamageOnMove);
			if (status == nullptr) {
				return false;
			}

			if (lifeTime < 0.0f) {
				status->Flags0 |= SF0_KeepAlive;
				status->CurrentLifeTime = 1.0f;
			} else {
				status->Flags0 |= SF0_IsLifeTimeSet;
				status->LifeTime = lifeTime;
				status->CurrentLifeTime = lifeTime;
			}

			auto sourceCharacter = FindCharacterByNameGuid(sourceCharacterGuid);
			if (sourceCharacter == nullptr) {
				status->StatusSourceHandle = ObjectHandle{};
			} else {
				ObjectHandle sourceHandle;
				sourceCharacter->GetObjectHandle(&sourceHandle);
				status->StatusSourceHandle = sourceHandle;
			}

			status->StartTimer = 0.0f;
			status->StatsMultiplier = 1.0f;
			status->DistancePerDamage = distancePerDamage;

			gOsirisProxy->GetLibraryManager().StatusMachineApplyStatus(statusMachine, status);
			args[5].Int64 = (int64_t)status->StatusHandle;
			return true;
		}
	}


	void CustomFunctionLibrary::OnStatusHitEnter(esv::Status * status)
	{
		auto statusHit = static_cast<esv::StatusHit *>(status);
		if (statusHit->DamageInfo.EffectFlags & HitFlag::HF_NoEvents) {
			return;
		}

		auto target = FindCharacterByHandle(status->TargetCIHandle);
		if (target == nullptr) {
			OsiError("Status has no target?");
			return;
		}

		char const * sourceGuid = "NULL_00000000-0000-0000-0000-000000000000";
		auto source = FindCharacterByHandle(status->StatusSourceHandle);
		if (source != nullptr) {
			sourceGuid = source->GetGuid()->Str;
		}

		auto eventArgs = OsiArgumentDesc::Create(OsiArgumentValue{ ValueType::GuidString, target->GetGuid()->Str });
		eventArgs->Add(OsiArgumentValue{ ValueType::GuidString, sourceGuid });
		eventArgs->Add(OsiArgumentValue{ (int32_t)statusHit->DamageInfo.TotalDamageDone });
		eventArgs->Add(OsiArgumentValue{ (int64_t)status->StatusHandle });

		gOsirisProxy->GetCustomFunctionInjector().ThrowEvent(HitEventHandle, eventArgs);

		delete eventArgs;
	}


	void CustomFunctionLibrary::OnStatusHealEnter(esv::Status * status)
	{
		auto statusHeal = static_cast<esv::StatusHeal *>(status);

		auto target = FindCharacterByHandle(status->TargetCIHandle);
		if (target == nullptr) {
			OsiError("Status has no target?");
			return;
		}

		char const * sourceGuid = "NULL_00000000-0000-0000-0000-000000000000";
		auto source = FindCharacterByHandle(status->StatusSourceHandle);
		if (source != nullptr) {
			sourceGuid = source->GetGuid()->Str;
		}

		auto eventArgs = OsiArgumentDesc::Create(OsiArgumentValue{ ValueType::GuidString, target->GetGuid()->Str });
		eventArgs->Add(OsiArgumentValue{ ValueType::GuidString, sourceGuid });
		eventArgs->Add(OsiArgumentValue{ (int32_t)statusHeal->HealAmount });
		eventArgs->Add(OsiArgumentValue{ (int64_t)status->StatusHandle });

		gOsirisProxy->GetCustomFunctionInjector().ThrowEvent(HealEventHandle, eventArgs);

		delete eventArgs;
	}


	void CustomFunctionLibrary::OnCharacterHit(esv::Character__Hit wrappedHit, esv::Character * self, CDivinityStats_Character * attackerStats,
		CDivinityStats_Item * itemStats, DamagePairList * damageList, HitType hitType, bool rollForDamage,
		HitDamageInfo * damageInfo, int forceReduceDurability, void * skillProperties, HighGroundBonus highGround,
		bool procWindWalker, CriticalRoll criticalRoll)
	{
		if (damageInfo->EffectFlags & HitFlag::HF_NoEvents) {
			wrappedHit(self, attackerStats, itemStats, damageList, hitType, rollForDamage,
				damageInfo, forceReduceDurability, skillProperties, highGround,
				procWindWalker, criticalRoll);
			return;
		}

		char const * sourceGuid = "NULL_00000000-0000-0000-0000-000000000000";
		if (attackerStats != nullptr
			&& attackerStats->Character != nullptr) {
			sourceGuid = attackerStats->Character->GetGuid()->Str;
		}

		int32_t totalDamage{ 0 };
		for (uint32_t i = 0; i < damageList->Size; i++) {
			totalDamage += damageList->Buf[i].Amount;
		}

		auto helper = gOsirisProxy->GetExtensionState().DamageHelpers.Create();
		helper->Type = DamageHelpers::HT_PrepareHitEvent;
		helper->Target = self;
		if (attackerStats != nullptr) {
			helper->Source = attackerStats->Character;
		}

		// TODO - allow disabling CallCharacterHit & not call the original func?
		helper->CallCharacterHit = true;
		helper->HitType = hitType;
		helper->RollForDamage = rollForDamage;
		helper->ProcWindWalker = procWindWalker;
		helper->HighGround = highGround;
		helper->Critical = criticalRoll;
		helper->ForceReduceDurability = (bool)forceReduceDurability;
		helper->SetExternalDamageInfo(damageInfo, damageList);

		auto eventArgs = OsiArgumentDesc::Create(OsiArgumentValue{ ValueType::GuidString, self->GetGuid()->Str });
		eventArgs->Add(OsiArgumentValue{ ValueType::GuidString, sourceGuid });
		eventArgs->Add(OsiArgumentValue{ (int32_t)totalDamage });
		eventArgs->Add(OsiArgumentValue{ helper->Handle });

		gOsirisProxy->GetCustomFunctionInjector().ThrowEvent(HitPrepareEventHandle, eventArgs);

		delete eventArgs;

		gOsirisProxy->GetExtensionState().DamageHelpers.Destroy(helper->Handle);

		wrappedHit(self, attackerStats, itemStats, damageList, helper->HitType, helper->RollForDamage,
			damageInfo, helper->ForceReduceDurability, skillProperties, helper->HighGround, 
			helper->ProcWindWalker, helper->Critical);
	}


	void CustomFunctionLibrary::OnApplyStatus(esv::StatusMachine__ApplyStatus wrappedApply, esv::StatusMachine * self, esv::Status * status)
	{
		char const * targetGuid{ nullptr };
		auto targetCharacter = FindCharacterByHandle(self->OwnerObjectHandle);
		if (targetCharacter != nullptr) {
			targetGuid = targetCharacter->GetGuid()->Str;
		} else {
			auto targetItem = FindItemByHandle(self->OwnerObjectHandle);
			if (targetItem != nullptr) {
				targetGuid = targetItem->GetGuid()->Str;
			} else {
				OsiError("Can't throw ApplyStatus event - target handle could not be resolved.");
			}
		}

		bool eventThrown{ false };
		if (targetGuid != nullptr) {
			char const * sourceGuid = "NULL_00000000-0000-0000-0000-000000000000";
			if (status->StatusSourceHandle) {
				auto sourceCharacter = FindCharacterByHandle(status->StatusSourceHandle);
				if (sourceCharacter != nullptr) {
					sourceGuid = sourceCharacter->GetGuid()->Str;
				} else {
					auto sourceItem = FindItemByHandle(status->StatusSourceHandle);
					if (sourceItem != nullptr) {
						sourceGuid = sourceItem->GetGuid()->Str;
					}
				}
			}

			auto eventArgs = OsiArgumentDesc::Create(OsiArgumentValue{ ValueType::GuidString, targetGuid });
			eventArgs->Add(OsiArgumentValue{ ValueType::String, status->StatusId.Str });
			eventArgs->Add(OsiArgumentValue{ (int64_t)status->StatusHandle });
			eventArgs->Add(OsiArgumentValue{ ValueType::GuidString, sourceGuid });

			gPendingStatuses.Add(status);
			eventThrown = true;
			gOsirisProxy->GetCustomFunctionInjector().ThrowEvent(StatusAttemptEventHandle, eventArgs);

			delete eventArgs;
		}

		bool previousPreventApplyState = self->PreventStatusApply;
		if (eventThrown) {
			auto pendingStatus = gPendingStatuses.Find(targetCharacter, status->StatusHandle);
			if (pendingStatus != nullptr) {
				self->PreventStatusApply = pendingStatus->PreventApply;
			} else {
				OsiError("Status not found in pending status list during ApplyStatus ?!");
			}
		}

		wrappedApply(self, status);

		self->PreventStatusApply = previousPreventApplyState;

		if (eventThrown) {
			gPendingStatuses.Remove(status);
		}
	}


	void CustomFunctionLibrary::RegisterStatusFunctions()
	{
		auto & functionMgr = osiris_.GetCustomFunctionManager();

		auto iterateCharacterStatuses = std::make_unique<CustomCall>(
			"NRD_IterateCharacterStatuses",
			std::vector<CustomFunctionParam>{
				{ "CharacterGuid", ValueType::CharacterGuid, FunctionArgumentDirection::In },
				{ "Event", ValueType::String, FunctionArgumentDirection::In }
			},
			&func::IterateCharacterStatuses
		);
		functionMgr.Register(std::move(iterateCharacterStatuses));

		auto getStatusHandle = std::make_unique<CustomQuery>(
			"NRD_StatusGetHandle",
			std::vector<CustomFunctionParam>{
				{ "Character", ValueType::CharacterGuid, FunctionArgumentDirection::In },
				{ "StatusId", ValueType::String, FunctionArgumentDirection::In },
				{ "StatusHandle", ValueType::Integer64, FunctionArgumentDirection::Out },
			},
			&func::StatusGetHandle
		);
		functionMgr.Register(std::move(getStatusHandle));

		auto getStatusAttributeInt = std::make_unique<CustomQuery>(
			"NRD_StatusGetInt",
			std::vector<CustomFunctionParam>{
				{ "Character", ValueType::CharacterGuid, FunctionArgumentDirection::In },
				{ "StatusHandle", ValueType::Integer64, FunctionArgumentDirection::In },
				{ "Attribute", ValueType::String, FunctionArgumentDirection::In },
				{ "Value", ValueType::Integer, FunctionArgumentDirection::Out },
			},
			&func::StatusGetAttribute<OsiPropertyMapType::Integer>
		);
		functionMgr.Register(std::move(getStatusAttributeInt));

		auto getStatusAttributeReal = std::make_unique<CustomQuery>(
			"NRD_StatusGetReal",
			std::vector<CustomFunctionParam>{
				{ "Character", ValueType::CharacterGuid, FunctionArgumentDirection::In },
				{ "StatusHandle", ValueType::Integer64, FunctionArgumentDirection::In },
				{ "Attribute", ValueType::String, FunctionArgumentDirection::In },
				{ "Value", ValueType::Real, FunctionArgumentDirection::Out },
			},
			&func::StatusGetAttribute<OsiPropertyMapType::Real>
		);
		functionMgr.Register(std::move(getStatusAttributeReal));

		auto getStatusAttributeString = std::make_unique<CustomQuery>(
			"NRD_StatusGetString",
			std::vector<CustomFunctionParam>{
				{ "Character", ValueType::CharacterGuid, FunctionArgumentDirection::In },
				{ "StatusHandle", ValueType::Integer64, FunctionArgumentDirection::In },
				{ "Attribute", ValueType::String, FunctionArgumentDirection::In },
				{ "Value", ValueType::String, FunctionArgumentDirection::Out },
			},
			&func::StatusGetAttribute<OsiPropertyMapType::String>
		);
		functionMgr.Register(std::move(getStatusAttributeString));

		auto getStatusAttributeGuidString = std::make_unique<CustomQuery>(
			"NRD_StatusGetGuidString",
			std::vector<CustomFunctionParam>{
				{ "Character", ValueType::CharacterGuid, FunctionArgumentDirection::In },
				{ "StatusHandle", ValueType::Integer64, FunctionArgumentDirection::In },
				{ "Attribute", ValueType::String, FunctionArgumentDirection::In },
				{ "Value", ValueType::GuidString, FunctionArgumentDirection::Out },
			},
			&func::StatusGetAttribute<OsiPropertyMapType::GuidStringHandle>
		);
		functionMgr.Register(std::move(getStatusAttributeGuidString));

		auto setStatusAttributeInt = std::make_unique<CustomCall>(
			"NRD_StatusSetInt",
			std::vector<CustomFunctionParam>{
				{ "Character", ValueType::CharacterGuid, FunctionArgumentDirection::In },
				{ "StatusHandle", ValueType::Integer64, FunctionArgumentDirection::In },
				{ "Attribute", ValueType::String, FunctionArgumentDirection::In },
				{ "Value", ValueType::Integer, FunctionArgumentDirection::In },
			},
			&func::StatusSetAttribute<OsiPropertyMapType::Integer>
		);
		functionMgr.Register(std::move(setStatusAttributeInt));

		auto setStatusAttributeReal = std::make_unique<CustomCall>(
			"NRD_StatusSetReal",
			std::vector<CustomFunctionParam>{
				{ "Character", ValueType::CharacterGuid, FunctionArgumentDirection::In },
				{ "StatusHandle", ValueType::Integer64, FunctionArgumentDirection::In },
				{ "Attribute", ValueType::String, FunctionArgumentDirection::In },
				{ "Value", ValueType::Real, FunctionArgumentDirection::In },
			},
			&func::StatusSetAttribute<OsiPropertyMapType::Real>
		);
		functionMgr.Register(std::move(setStatusAttributeReal));

		auto setStatusAttributeString = std::make_unique<CustomCall>(
			"NRD_StatusSetString",
			std::vector<CustomFunctionParam>{
				{ "Character", ValueType::CharacterGuid, FunctionArgumentDirection::In },
				{ "StatusHandle", ValueType::Integer64, FunctionArgumentDirection::In },
				{ "Attribute", ValueType::String, FunctionArgumentDirection::In },
				{ "Value", ValueType::String, FunctionArgumentDirection::In },
			},
			&func::StatusSetAttribute<OsiPropertyMapType::String>
		);
		functionMgr.Register(std::move(setStatusAttributeString));

		auto setStatusAttributeGuidString = std::make_unique<CustomCall>(
			"NRD_StatusSetGuidString",
			std::vector<CustomFunctionParam>{
				{ "Character", ValueType::CharacterGuid, FunctionArgumentDirection::In },
				{ "StatusHandle", ValueType::Integer64, FunctionArgumentDirection::In },
				{ "Attribute", ValueType::String, FunctionArgumentDirection::In },
				{ "Value", ValueType::GuidString, FunctionArgumentDirection::In },
			},
			&func::StatusSetAttribute<OsiPropertyMapType::GuidStringHandle>
		);
		functionMgr.Register(std::move(setStatusAttributeGuidString));

		auto setStatusAttributeVector3 = std::make_unique<CustomCall>(
			"NRD_StatusSetVector3",
			std::vector<CustomFunctionParam>{
				{ "Character", ValueType::CharacterGuid, FunctionArgumentDirection::In },
				{ "StatusHandle", ValueType::Integer64, FunctionArgumentDirection::In },
				{ "Attribute", ValueType::String, FunctionArgumentDirection::In },
				{ "X", ValueType::Real, FunctionArgumentDirection::In },
				{ "Y", ValueType::Real, FunctionArgumentDirection::In },
				{ "Z", ValueType::Real, FunctionArgumentDirection::In },
			},
			&func::StatusSetAttribute<OsiPropertyMapType::Vector3>
		);
		functionMgr.Register(std::move(setStatusAttributeVector3));

		auto statusPreventApply = std::make_unique<CustomCall>(
			"NRD_StatusPreventApply",
			std::vector<CustomFunctionParam>{
				{ "Character", ValueType::CharacterGuid, FunctionArgumentDirection::In },
				{ "StatusHandle", ValueType::Integer64, FunctionArgumentDirection::In },
				{ "PreventApply", ValueType::Integer, FunctionArgumentDirection::In }
			},
			&func::StatusPreventApply
		);
		functionMgr.Register(std::move(statusPreventApply));

		auto statusIteratorEvent = std::make_unique<CustomEvent>(
			"NRD_StatusIteratorEvent",
			std::vector<CustomFunctionParam>{
				{ "Event", ValueType::String, FunctionArgumentDirection::In },
				{ "Character", ValueType::CharacterGuid, FunctionArgumentDirection::In },
				{ "StatusId", ValueType::String, FunctionArgumentDirection::In },
				{ "StatusHandle", ValueType::Integer64, FunctionArgumentDirection::In },
			}
		);
		StatusIteratorEventHandle = functionMgr.Register(std::move(statusIteratorEvent));

		auto statusAttemptEvent = std::make_unique<CustomEvent>(
			"NRD_OnStatusAttempt",
			std::vector<CustomFunctionParam>{
				{ "Target", ValueType::GuidString, FunctionArgumentDirection::In },
				{ "StatusId", ValueType::String, FunctionArgumentDirection::In },
				{ "StatusHandle", ValueType::Integer64, FunctionArgumentDirection::In },
				{ "Instigator", ValueType::GuidString, FunctionArgumentDirection::In },
			}
		);
		StatusAttemptEventHandle = functionMgr.Register(std::move(statusAttemptEvent));

		auto applyActiveDefense = std::make_unique<CustomQuery>(
			"NRD_ApplyActiveDefense",
			std::vector<CustomFunctionParam>{
				{ "Character", ValueType::CharacterGuid, FunctionArgumentDirection::In },
				{ "StatusId", ValueType::String, FunctionArgumentDirection::In },
				{ "LifeTime", ValueType::Real, FunctionArgumentDirection::In },
				{ "StatusHandle", ValueType::Integer64, FunctionArgumentDirection::Out },
			},
			&func::ApplyActiveDefense
		);
		functionMgr.Register(std::move(applyActiveDefense));

		auto applyDamageOnMove = std::make_unique<CustomQuery>(
			"NRD_ApplyDamageOnMove",
			std::vector<CustomFunctionParam>{
				{ "Character", ValueType::CharacterGuid, FunctionArgumentDirection::In },
				{ "StatusId", ValueType::String, FunctionArgumentDirection::In },
				{ "SourceCharacter", ValueType::CharacterGuid, FunctionArgumentDirection::In },
				{ "LifeTime", ValueType::Real, FunctionArgumentDirection::In },
				{ "DistancePerDamage", ValueType::Real, FunctionArgumentDirection::In },
				{ "StatusHandle", ValueType::Integer64, FunctionArgumentDirection::Out },
			},
			&func::ApplyDamageOnMove
		);
		functionMgr.Register(std::move(applyDamageOnMove));

		auto hitPrepareEvent = std::make_unique<CustomEvent>(
			"NRD_OnPrepareHit",
			std::vector<CustomFunctionParam>{
				{ "Target", ValueType::CharacterGuid, FunctionArgumentDirection::In },
				{ "Instigator", ValueType::GuidString, FunctionArgumentDirection::In },
				{ "Damage", ValueType::Integer, FunctionArgumentDirection::In },
				{ "HitHandle", ValueType::Integer64, FunctionArgumentDirection::In },
			}
		);
		HitPrepareEventHandle = functionMgr.Register(std::move(hitPrepareEvent));

		auto hitEvent = std::make_unique<CustomEvent>(
			"NRD_OnHit",
			std::vector<CustomFunctionParam>{
				{ "Target", ValueType::CharacterGuid, FunctionArgumentDirection::In },
				{ "Instigator", ValueType::GuidString, FunctionArgumentDirection::In },
				{ "Damage", ValueType::Integer, FunctionArgumentDirection::In },
				{ "StatusHandle", ValueType::Integer64, FunctionArgumentDirection::In },
			}
		);
		HitEventHandle = functionMgr.Register(std::move(hitEvent));

		auto healEvent = std::make_unique<CustomEvent>(
			"NRD_OnHeal",
			std::vector<CustomFunctionParam>{
				{ "Target", ValueType::CharacterGuid, FunctionArgumentDirection::In },
				{ "Instigator", ValueType::GuidString, FunctionArgumentDirection::In },
				{ "Amount", ValueType::Integer, FunctionArgumentDirection::In },
				{ "StatusHandle", ValueType::Integer64, FunctionArgumentDirection::In },
			}
		);
		HealEventHandle = functionMgr.Register(std::move(healEvent));
	}

}

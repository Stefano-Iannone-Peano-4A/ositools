#pragma once

#include "BaseTypes.h"
#include "Enumerations.h"
#include "Net.h"
#include "Module.h"
#include "Stats.h"

namespace dse
{

#pragma pack(push, 1)
	template <class T, uint32_t TypeIndex>
	struct NetworkObjectFactory : public ObjectFactory<T, TypeIndex>
	{
		FixedStringMapBase<T *> ObjectMap;
		uint32_t _Pad2;
		FixedStringMapBase<void *> ObjectMap2;
		uint32_t _Pad3;
		FixedStringMapBase<void *> ObjectMap3;
		uint32_t _Pad4;
		ObjectSet<uint32_t> Unknown4;
		Array<uint16_t> Unknown5;
		uint16_t Unknown6;
		uint8_t Unknown7;
		uint8_t _Pad5[5];
	};

	struct ComponentTypeEntry
	{
		// todo
		Component * component;
		uint64_t dummy[31];
	};

	struct ComponentLayout
	{
		struct LayoutEntry
		{
			uint64_t unkn1;
			ComponentHandle Handle;
		};

		Array<LayoutEntry> Entries;
	};

	struct SystemTypeEntry
	{
		void * System;
		int64_t Unkn1;
		uint32_t Unkn2;
		uint8_t _Pad[4];
		void * PrimitiveSetVMT;
		PrimitiveSet<uint64_t> PSet;
		uint8_t Unkn3;
		uint8_t _Pad2[7];
	};

	struct EntityEntry
	{
		void * VMT;
		ComponentLayout Layout;
	};

	namespace esv
	{
		struct CustomStatDefinitionComponent;
		struct CustomStatSystem;
		struct NetComponent;
		struct Item;
		struct Character;
		struct Inventory;
	}

	namespace eoc
	{
		struct CombatComponent;
		struct CustomStatsComponent;
	}
	
	struct BaseComponentProcessingSystem
	{
		void * VMT;
		void * field_8;
	};

	struct EntityWorld : public ProtectedGameObject<EntityWorld>
	{
		void * VMT;
		Array<EntityEntry *> EntityEntries;
		Array<uint32_t> EntitySalts;
		uint64_t Unkn1[4];
		PrimitiveSet<EntityEntry *> EntityEntries2;
		uint64_t Unkn2;
		uint8_t Unkn3;
		uint8_t _Pad3[3];
		uint32_t Unkn4;
		Array<ComponentTypeEntry> Components;
		ObjectSet<void *> KeepAlives; // ObjectSet<ObjectHandleRefMap<ComponentKeepAliveDesc>>
		Array<SystemTypeEntry> SystemTypes;
		Array<void *> EventTypes; // Array<EventTypeEntry>
		void * EntityWorldManager;
		void * SystemTypeEntry_PrimSetVMT;
		PrimitiveSet<SystemTypeEntry> SystemTypes2;
		uint64_t Unkn5;
		ObjectSet<void *> Funcs; // ObjectSet<function>
		FixedStringRefMap<FixedString, int> RefMap; // ???

		void * GetComponentByEntityHandle(ObjectHandle entityHandle, ComponentType type);
		void * FindComponentByHandle(ObjectHandle componentHandle, ComponentType type);

		inline esv::CustomStatDefinitionComponent * FindCustomStatDefinitionComponentByHandle(ObjectHandle componentHandle)
		{
			auto component = FindComponentByHandle(componentHandle, ComponentType::CustomStatDefinition);
			if (component != nullptr) {
				return (esv::CustomStatDefinitionComponent *)((uint8_t *)component - 80);
			} else {
				return nullptr;
			}
		}

		inline esv::Character * GetCharacterComponentByEntityHandle(ObjectHandle entityHandle)
		{
			auto ptr = GetComponentByEntityHandle(entityHandle, ComponentType::Character);
			if (ptr != nullptr) {
				return (esv::Character *)((uint8_t *)ptr - 8);
			} else {
				return nullptr;
			}
		}

		inline esv::Item * GetItemComponentByEntityHandle(ObjectHandle entityHandle)
		{
			auto ptr = GetComponentByEntityHandle(entityHandle, ComponentType::Item);
			if (ptr != nullptr) {
				return (esv::Item *)((uint8_t *)ptr - 8);
			} else {
				return nullptr;
			}
		}

		inline eoc::CombatComponent * GetCombatComponentByEntityHandle(ObjectHandle entityHandle)
		{
			return (eoc::CombatComponent *)GetComponentByEntityHandle(entityHandle, ComponentType::Combat);
		}

		inline eoc::CustomStatsComponent * GetCustomStatsComponentByEntityHandle(ObjectHandle entityHandle)
		{
			return (eoc::CustomStatsComponent *)GetComponentByEntityHandle(entityHandle, ComponentType::CustomStats);
		}

		inline esv::NetComponent * GetNetComponentByEntityHandle(ObjectHandle entityHandle)
		{
			return (esv::NetComponent *)GetComponentByEntityHandle(entityHandle, ComponentType::Net);
		}

		inline esv::CustomStatSystem * GetCustomStatSystem()
		{
			auto sys = SystemTypes.Buf[(uint32_t)SystemType::CustomStat].System;
			return (esv::CustomStatSystem *)((uint8_t *)sys - 0x18);
		}
	};

	struct CharacterFactory : public NetworkObjectFactory<esv::Character, (uint32_t)ObjectType::Character>
	{
		void * VMT2;
		void * VMT3;
		FixedStringMapBase<void *> FSMap_ReloadComponent;
		uint32_t _Pad4;
		EntityWorld * Entities;
		uint64_t Unkn8[2];
	};

	struct ItemFactory : public NetworkObjectFactory<esv::Item, (uint32_t)ObjectType::Item>
	{
		void * VMT2;
		void * VMT3;
		FixedStringMapBase<void *> FSMap_ReloadComponent;
		uint32_t _Pad4;
		EntityWorld * Entities;
		uint64_t Unkn8[2];
	};

	struct InventoryFactory : public NetworkObjectFactory<esv::Inventory, (uint32_t)ObjectType::Inventory>
	{
		// TODO
	};

	namespace esv
	{
		struct EoCServerObject : public ProtectedGameObject<EoCServerObject>
		{
			virtual ~EoCServerObject() = 0;
			virtual void HandleTextKeyEvent() = 0;
			virtual uint64_t Ret5() = 0;
			virtual void SetObjectHandle(ObjectHandle Handle) = 0;
			virtual void GetObjectHandle(ObjectHandle * Handle) const = 0;
			virtual void SetGuid(FixedString const & fs) = 0;
			virtual FixedString * GetGuid() const = 0;
			virtual void SetNetID(NetId netId) = 0;
			virtual void GetNetID(NetId & netId) const = 0;
			virtual void SetCurrentTemplate(void * esvTemplate) = 0;
			virtual void * GetCurrentTemplate() const = 0;
			virtual void SetGlobal(bool isGlobal) = 0;
			virtual bool IsGlobal() const = 0;
			virtual uint32_t GetComponentType() = 0;
			virtual void * GetEntityObjectByHandle(ObjectHandle handle) = 0;
			virtual STDWString * GetName() = 0;
			virtual void SetFlags(uint64_t flag) = 0;
			virtual void ClearFlags(uint64_t flag) = 0;
			virtual bool HasFlag(uint64_t flag) const = 0;
			virtual void SetAiColliding(bool colliding) = 0;
			virtual void GetTags() = 0;
			virtual void IsTagged() = 0;
			virtual Vector3 const * GetTranslate() const = 0;
			virtual glm::mat3 const * GetRotation() const = 0;
			virtual float GetScale() const = 0;
			virtual void SetTranslate(Vector3 const & translate) = 0;
			virtual void SetRotation(glm::mat3 const & rotate) = 0;
			virtual void SetScale(float scale) = 0;
			virtual Vector3 const * GetVelocity() = 0;
			virtual void SetVelocity(Vector3 const & translate) = 0;
			virtual void LoadVisual() = 0;
			virtual void UnloadVisual() = 0;
			virtual void ReloadVisual() = 0;
			virtual void GetVisual() = 0;
			virtual void GetPhysics() = 0;
			virtual void SetPhysics() = 0;
			virtual void LoadPhysics() = 0;
			virtual void UnloadPhysics() = 0;
			virtual void ReloadPhysics() = 0;
			virtual void GetHeight() = 0;
			virtual void GetParentUUID() = 0;
			virtual FixedString * GetCurrentLevel() const = 0;
			virtual void SetCurrentLevel(FixedString const & level) = 0;
			virtual void AddPeer() = 0;
			virtual void UNK1() = 0;
			virtual void UNK2() = 0;
			virtual void UNK3() = 0;
			virtual void GetAi() = 0;
			virtual void LoadAi() = 0;
			virtual void UnloadAi() = 0;
			virtual TranslatedString * GetDisplayName(TranslatedString * name) = 0;
			virtual void SavegameVisit() = 0;
			virtual void GetEntityNetworkId() = 0;
			virtual void SetTemplate() = 0;
			virtual void SetOriginalTemplate_M() = 0;

			BaseComponent Base;
			FixedString MyGuid;

			NetId NetID;
			uint32_t _Pad1;
			glm::vec3 WorldPos; // Saved
			uint32_t _Pad2;
			uint64_t Flags; // Saved
			uint32_t U2;
			uint32_t _Pad3;
			FixedString CurrentLevel; // Saved
		};

		struct GameStateMachine : public ProtectedGameObject<GameStateMachine>
		{
			uint8_t Unknown;
			uint8_t _Pad1[7];
			void * CurrentState;
			ServerGameState State;
			uint8_t _Pad2[4];
			void ** TargetStates;
			uint32_t TargetStateBufSize;
			uint32_t NumTargetStates;
			uint32_t ReadStateIdx;
			uint32_t WriteStateIdx;
		};

		struct EoCServer
		{
			bool Unknown1;
			uint8_t _Pad1[7];
			uint64_t field_8;
			uint64_t GameTime_M;
			ScratchBuffer ScratchBuffer1;
			uint8_t _Pad2[4];
			ScratchBuffer ScratchBuffer2;
			uint8_t _Pad3[4];
			FixedString FS1;
			FixedString FS2;
			FixedString FS3;
			FixedString FSGUID4;
			GameStateMachine * StateMachine;
			net::GameServer * GameServer;
			void * field_88;
			void * GlobalRandom;
			void * ItemCombinationManager;
			void * CombineManager;
			ModManager * ModManagerServer;
			bool ShutDown;
			uint8_t _Pad4[7];
			void * EntityWorldManager;
			EntityWorld * EntityWorld;
			void * EntityManager;
			void * ArenaManager;
			void * GameMasterLobbyManager;
			void * LobbyManagerOrigins;
			bool field_E8;
		};
	}
#pragma pack(pop)
}
#pragma once

#include <Lua/LuaBinding.h>
#include <GameDefinitions/Stats.h>
#include <GameDefinitions/Osiris.h>
#include <GameDefinitions/TurnManager.h>
#include <CustomFunctions.h>
#include <ExtensionHelpers.h>
#include <OsirisHelpers.h>

namespace dse::lua
{
	void LuaToOsi(lua_State * L, int i, TypedValue & tv, ValueType osiType, bool allowNil = false);
	TypedValue * LuaToOsi(lua_State * L, int i, ValueType osiType, bool allowNil = false);
	void LuaToOsi(lua_State * L, int i, OsiArgumentValue & arg, ValueType osiType, bool allowNil = false);
	void OsiToLua(lua_State * L, OsiArgumentValue const & arg);
	void OsiToLua(lua_State * L, TypedValue const & tv);

	class OsiFunction
	{
	public:
		inline bool IsBound() const
		{
			return function_ != nullptr;
		}

		inline bool IsDB() const
		{
			return IsBound() && function_->Type == FunctionType::Database;
		}

		bool Bind(Function const * func, class ServerState & state);
		void Unbind();

		int LuaCall(lua_State * L);
		int LuaGet(lua_State * L);
		int LuaDelete(lua_State * L);

	private:
		Function const * function_{ nullptr };
		AdapterRef adapter_;
		ServerState * state_;

		void OsiCall(lua_State * L);
		void OsiInsert(lua_State * L, bool deleteTuple);
		int OsiQuery(lua_State * L);
		int OsiUserQuery(lua_State * L);

		bool MatchTuple(lua_State * L, int firstIndex, TupleVec const & tuple);
		void ConstructTuple(lua_State * L, TupleVec const & tuple);
	};

	class OsiFunctionNameProxy : public Userdata<OsiFunctionNameProxy>, public Callable
	{
	public:
		static char const * const MetatableName;
		// Maximum number of OUT params that a query can return.
		// (This setting determines how many function arities we'll check during name lookup)
		static constexpr uint32_t MaxQueryOutParams = 6;

		static void PopulateMetatable(lua_State * L);

		OsiFunctionNameProxy(STDString const & name, ServerState & state);

		void UnbindAll();
		int LuaCall(lua_State * L);

	private:
		STDString name_;
		std::vector<OsiFunction> functions_;
		ServerState & state_;
		uint32_t generationId_;

		static int LuaGet(lua_State * L);
		static int LuaDelete(lua_State * L);
		bool BeforeCall(lua_State * L);
		OsiFunction * TryGetFunction(uint32_t arity);
		OsiFunction * CreateFunctionMapping(uint32_t arity, Function const * func);
		Function const * LookupOsiFunction(uint32_t arity);
	};


	class CustomLuaCall : public CustomCallBase
	{
	public:
		inline CustomLuaCall(STDString const & name, std::vector<CustomFunctionParam> params,
			RegistryEntry handler)
			: CustomCallBase(name, std::move(params)), handler_(std::move(handler))
		{}

		virtual bool Call(OsiArgumentDesc const & params) override;

	private:
		RegistryEntry handler_;
	};


	class CustomLuaQuery : public CustomQueryBase
	{
	public:
		inline CustomLuaQuery(STDString const & name, std::vector<CustomFunctionParam> params,
			RegistryEntry handler)
			: CustomQueryBase(name, std::move(params)), handler_(std::move(handler))
		{}

		virtual bool Query(OsiArgumentDesc & params) override;

	private:
		RegistryEntry handler_;
	};


	class StatusHandleProxy : public Userdata<StatusHandleProxy>, public Indexable, public Pushable<PushPolicy::None>
	{
	public:
		static char const * const MetatableName;

		inline StatusHandleProxy(ObjectHandle character, ObjectHandle status)
			: character_(character), status_(status)
		{}

		int Index(lua_State * L);
		int NewIndex(lua_State * L);

	private:
		ObjectHandle character_;
		ObjectHandle status_;
	};


	class TurnManagerCombatProxy : public Userdata<TurnManagerCombatProxy>, public Indexable, public Pushable<PushPolicy::None>
	{
	public:
		static char const * const MetatableName;

		static void PopulateMetatable(lua_State * L);

		inline TurnManagerCombatProxy(uint8_t combatId)
			: combatId_(combatId)
		{}

		inline esv::TurnManager::Combat * Get()
		{
			return GetTurnManager()->Combats.Find(combatId_);
		}

		int Index(lua_State * L);

	private:
		uint8_t combatId_;

		static int GetCurrentTurnOrder(lua_State * L);
		static int GetNextTurnOrder(lua_State * L);
		static int UpdateCurrentTurnOrder(lua_State * L);
		static int UpdateNextTurnOrder(lua_State * L);
		static int GetAllTeams(lua_State * L);
	};

	class TurnManagerTeamProxy : public Userdata<TurnManagerTeamProxy>, public Indexable, public Pushable<PushPolicy::None>
	{
	public:
		static char const * const MetatableName;

		inline TurnManagerTeamProxy(eoc::CombatTeamId teamId)
			: teamId_(teamId)
		{}

		inline eoc::CombatTeamId TeamId() const
		{
			return teamId_;
		}

		inline esv::TurnManager::CombatTeam * Get()
		{
			auto combat = GetTurnManager()->Combats.Find(teamId_.CombatId);
			if (combat) {
				auto team = combat->Teams.Find((uint32_t)teamId_);
				if (team) {
					return *team;
				} else {
					return nullptr;
				}
			} else {
				return nullptr;
			}
		}

		int Index(lua_State * L);

	private:
		eoc::CombatTeamId teamId_;
	};


	class ExtensionLibraryServer : public ExtensionLibrary
	{
	public:
		void Register(lua_State * L) override;
		void RegisterLib(lua_State * L) override;
		STDString GenerateOsiHelpers();

	private:
		static char const * const NameResolverMetatableName;

		void RegisterNameResolverMetatable(lua_State * L);
		void CreateNameResolver(lua_State * L);

		static int LuaIndexResolverTable(lua_State * L);

		static int NewCall(lua_State * L);
		static int NewQuery(lua_State * L);
		static int NewEvent(lua_State * L);
	};

	inline void OsiReleaseArgument(OsiArgumentDesc & arg)
	{
		arg.NextParam = nullptr;
	}

	inline void OsiReleaseArgument(TypedValue & arg) {}
	inline void OsiReleaseArgument(ListNode<TypedValue *> & arg) {}
	inline void OsiReleaseArgument(ListNode<TupleLL::Item> & arg) {}

	template <class T>
	class OsiArgumentPool
	{
	public:
		OsiArgumentPool()
		{
			argumentPool_.resize(1024);
			usedArguments_ = 0;
		}

		T * AllocateArguments(uint32_t num, uint32_t & tail)
		{
			if (usedArguments_ + num > argumentPool_.size()) {
				throw std::runtime_error("Ran out of argument descriptors");
			}

			tail = usedArguments_;
			auto ptr = argumentPool_.data() + usedArguments_;
			for (uint32_t i = 0; i < num; i++) {
				new (ptr + i) T();
			}

			usedArguments_ += num;
			return ptr;
		}

		void ReleaseArguments(uint32_t tail, uint32_t num)
		{
			if (tail + num != usedArguments_) {
				throw std::runtime_error("Attempted to release arguments out of order");
			}

			for (uint32_t i = 0; i < num; i++) {
				OsiReleaseArgument(argumentPool_[tail + i]);
			}

			usedArguments_ -= num;
		}

	private:
		std::vector<T> argumentPool_;
		uint32_t usedArguments_;
	};

	template <class T>
	class OsiArgumentListPin
	{
	public:
		inline OsiArgumentListPin(OsiArgumentPool<T> & pool, uint32_t numArgs)
			: pool_(pool), numArgs_(numArgs)
		{
			args_ = pool.AllocateArguments(numArgs_, tail_);
		}

		inline ~OsiArgumentListPin()
		{
			pool_.ReleaseArguments(tail_, numArgs_);
		}

		inline T * Args() const
		{
			return args_;
		}

	private:
		OsiArgumentPool<T> & pool_;
		uint32_t numArgs_;
		uint32_t tail_;
		T * args_;
	};


	class ServerState : public State
	{
	public:
		ServerState();
		~ServerState();

		ServerState(ServerState const &) = delete;
		ServerState(ServerState &&) = delete;
		ServerState & operator = (ServerState const &) = delete;
		ServerState & operator = (ServerState &&) = delete;

		inline uint32_t GenerationId() const
		{
			return generationId_;
		}

		inline IdentityAdapterMap & GetIdentityAdapterMap()
		{
			return identityAdapters_;
		}

		inline OsiArgumentPool<OsiArgumentDesc> & GetArgumentDescPool()
		{
			return argDescPool_;
		}

		inline OsiArgumentPool<TypedValue> & GetTypedValuePool()
		{
			return tvPool_;
		}

		inline OsiArgumentPool<ListNode<TypedValue *>> & GetTypedValueNodePool()
		{
			return tvNodePool_;
		}

		inline OsiArgumentPool<ListNode<TupleLL::Item>> & GetTupleNodePool()
		{
			return tupleNodePool_;
		}

		void OnGameSessionLoading() override;

		void StoryLoaded();
		void StoryFunctionMappingsUpdated();

		template <class TArg>
		void Call(char const * func, std::vector<TArg> const & args)
		{
			std::lock_guard lock(mutex_);

			auto L = GetState();
			lua_checkstack(L, (int)args.size() + 1);
			auto stackSize = lua_gettop(L);

			try {
				lua_getglobal(L, func); // stack: func
				for (auto & arg : args) {
					OsiToLua(L, arg); // stack: func, arg0 ... argn
				}

				auto status = CallWithTraceback(L, (int)args.size(), 0);
				if (status != LUA_OK) {
					OsiError("Failed to call function '" << func << "': " << lua_tostring(L, -1));
					// stack: errmsg
					lua_pop(L, 1); // stack: -
				}
			} catch (Exception &) {
				auto stackRemaining = lua_gettop(L) - stackSize;
				if (stackRemaining > 0) {
					OsiError("Call '" << func << "' failed: " << lua_tostring(L, -1));
					lua_pop(L, stackRemaining);
				} else {
					OsiError("Internal error during call '" << func << "'");
				}
			}
		}

		bool Query(STDString const & name, RegistryEntry * func,
			std::vector<CustomFunctionParam> const & signature, OsiArgumentDesc & params);

		std::optional<int32_t> StatusGetEnterChance(esv::Status * status, bool useCharacterStats);
		bool OnUpdateTurnOrder(esv::TurnManager * self, uint8_t combatId);
		bool ComputeCharacterHit(CDivinityStats_Character * self,
			CDivinityStats_Character *attackerStats, CDivinityStats_Item *item, DamagePairList *damageList, HitType hitType, bool noHitRoll,
			bool forceReduceDurability, HitDamageInfo *damageInfo, CRPGStats_Object_Property_List *skillProperties,
			HighGroundBonus highGroundFlag, CriticalRoll criticalRoll);

	private:
		ExtensionLibraryServer library_;
		OsiArgumentPool<OsiArgumentDesc> argDescPool_;
		OsiArgumentPool<TypedValue> tvPool_;
		OsiArgumentPool<ListNode<TypedValue *>> tvNodePool_;
		OsiArgumentPool<ListNode<TupleLL::Item>> tupleNodePool_;
		IdentityAdapterMap identityAdapters_;
		// ID of current story instance.
		// Used to invalidate function/node pointers in Lua userdata objects
		uint32_t generationId_{ 0 };

		bool QueryInternal(STDString const & name, RegistryEntry * func,
			std::vector<CustomFunctionParam> const & signature, OsiArgumentDesc & params);
	};
}

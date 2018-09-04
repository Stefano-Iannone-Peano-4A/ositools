#pragma once

#include <cstdint>
#include "osidebug.pb.h"
#include "DivInterface.h"
#include "DebugMessages.h"

namespace osidbg
{
	enum BreakpointType
	{
		BreakOnValid = 1 << 0,
		BreakOnPushDown = 1 << 1,
		BreakOnInsert = 1 << 2,
		BreakOnRuleAction = 1 << 3,
		BreakOnInitCall = 1 << 4,
		BreakOnExitCall = 1 << 5,
		BreakpointTypeAll = BreakOnValid | BreakOnPushDown | BreakOnInsert
			| BreakOnRuleAction | BreakOnInitCall | BreakOnExitCall
	};

	enum GlobalBreakpointType
	{
		GlobalBreakOnStoryLoaded = 1 << 0,
		GlobalBreakOnValid = 1 << 1,
		GlobalBreakOnPushDown = 1 << 2,
		GlobalBreakOnInsert = 1 << 3,
		GlobalBreakOnRuleAction = 1 << 4,
		GlobalBreakOnInitCall = 1 << 5,
		GlobalBreakOnExitCall = 1 << 6,
		GlobalBreakOnGameInit = 1 << 7,
		GlobalBreakOnGameExit = 1 << 8,
		GlobalBreakpointTypeAll = GlobalBreakOnStoryLoaded | GlobalBreakOnValid | GlobalBreakOnPushDown 
			| GlobalBreakOnInsert | GlobalBreakOnRuleAction | GlobalBreakOnInitCall | GlobalBreakOnExitCall
			| GlobalBreakOnGameInit | GlobalBreakOnGameExit
	};

	// Mapping of a rule action to its call site (rule then part, goal init/exit)
	struct RuleActionMapping
	{
		// Mapped action
		RuleActionNode * action;
		// Calling rule node
		Node * rule;
		// Calling goal
		Goal * goal;
		// Is this an INIT section call?
		bool isInit;
		// Index of action in section
		uint32_t actionIndex;
	};

	struct OsirisGlobals;

	class Debugger
	{
	public:
		Debugger(OsirisGlobals const & globals, DebugMessageHandler & messageHandler);
		~Debugger();

		void StoryLoaded();

		inline bool IsInitialized() const
		{
			return isInitialized_;
		}

		inline bool IsPaused() const
		{
			return isPaused_;
		}

		ResultCode SetGlobalBreakpoints(GlobalBreakpointType type);
		void ClearNodeBreakpoints();
		ResultCode SetBreakpoint(uint32_t nodeId, uint32_t goalId, bool isInit, int32_t actionIndex, BreakpointType type);
		ResultCode ContinueExecution(DbgContinue_Action action);
		void ClearAllBreakpoints();
		void SyncStory();

		void GameInitHook();
		void DeleteAllDataHook();
		void RuleActionPreHook(RuleActionNode * action);
		void RuleActionPostHook(RuleActionNode * action);

	private:
		struct Breakpoint
		{
			uint32_t nodeId;
			uint32_t goalId;
			bool isInit;
			uint32_t actionIndex;
			BreakpointType type;
		};

		enum BreakpointItemType : uint8_t
		{
			BP_Node = 0,
			BP_RuleAction = 1,
			BP_GoalInit = 2,
			BP_GoalExit = 3
		};

		static uint64_t MakeNodeBreakpointId(uint32_t nodeId);
		static uint64_t MakeRuleActionBreakpointId(uint32_t nodeId, uint32_t actionIndex);
		static uint64_t MakeGoalInitBreakpointId(uint32_t goalId, uint32_t actionIndex);
		static uint64_t MakeGoalExitBreakpointId(uint32_t goalId, uint32_t actionIndex);

		OsirisGlobals const & globals_;
		DebugMessageHandler & messageHandler_;
		std::vector<CallStackFrame> callStack_;
		// Mapping of a rule action to its call site (rule then part, goal init/exit)
		std::unordered_map<RuleActionNode *, RuleActionMapping> ruleActionMappings_;
		// Did the engine call COsiris::InitGame() in this session?
		bool isInitialized_{ false };

		std::mutex breakpointMutex_;
		std::condition_variable breakpointCv_;

		uint32_t globalBreakpoints_;
		std::unordered_map<uint64_t, Breakpoint> breakpoints_;
		bool isPaused_{ false };
		// Forcibly triggers a breakpoint if all breakpoint conditions are met.
		bool forceBreakpoint_{ false };
		// Call stack depth at which we'll trigger a breakpoint
		// (used for step over/into/out)
		uint32_t maxBreakDepth_{ 0 };

		void FinishedSingleStep();
		bool ShouldTriggerBreakpoint(uint64_t bpNodeId, BreakpointType bpType, GlobalBreakpointType globalBpType);
		void ConditionalBreakpointInServerThread(uint64_t bpNodeId, BreakpointType bpType, GlobalBreakpointType globalBpType);
		void BreakpointInServerThread();
		void GlobalBreakpointInServerThread(GlobalBreakpointReason reason);

		void AddRuleActionMappings(Node * node, Goal * goal, bool isInit, RuleActionList * actions);
		void UpdateRuleActionMappings();
		RuleActionMapping const & FindActionMapping(RuleActionNode * action);

		void IsValidPreHook(Node * node, VirtTupleLL * tuple, AdapterRef * adapter);
		void IsValidPostHook(Node * node, VirtTupleLL * tuple, AdapterRef * adapter, bool succeeded);
		void PushDownPreHook(Node * node, VirtTupleLL * tuple, AdapterRef * adapter, EntryPoint entry, bool deleted);
		void PushDownPostHook(Node * node, VirtTupleLL * tuple, AdapterRef * adapter, EntryPoint entry, bool deleted);
		void InsertPreHook(Node * node, TuplePtrLL * tuple, bool deleted);
		void InsertPostHook(Node * node, TuplePtrLL * tuple, bool deleted);

		void PushFrame(CallStackFrame const & frame);
		void PopFrame(CallStackFrame const & frame);
	};
}
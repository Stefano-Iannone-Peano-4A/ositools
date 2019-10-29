#include "stdafx.h"
#include "DataLibraries.h"
#include <string>
#include <functional>
#include <psapi.h>

#if !defined(OSI_EOCAPP)
namespace osidbg
{
	bool LibraryManager::FindEoCPlugin(uint8_t const * & start, size_t & size)
	{
		HMODULE hEoCApp = GetModuleHandleW(L"EoCApp.exe");
		if (hEoCApp != NULL) {
			MessageBoxW(NULL, L"This version of the Osiris Extender can only be used with the editor.", L"Osiris Extender Error", MB_OK | MB_ICONERROR);
			TerminateProcess(GetCurrentProcess(), 1);
		}

		HMODULE hDivinityEngine = GetModuleHandleW(L"DivinityEngine2.exe");
		if (hDivinityEngine == NULL) {
			return false;
		}

		HMODULE hEoCPlugin = LoadLibraryW(L"EoCPlugin.dll");
		if (hEoCPlugin == NULL) {
			return false;
		}

		MODULEINFO moduleInfo;
		if (!GetModuleInformation(GetCurrentProcess(), hEoCPlugin, &moduleInfo, sizeof(moduleInfo))) {
			Fail("Could not get module info of EoCPlugin.dll");
		}

		start = (uint8_t const *)moduleInfo.lpBaseOfDll;
		size = moduleInfo.SizeOfImage;

		HMODULE hCoreLib = LoadLibraryW(L"CoreLib.dll");
		if (hCoreLib == NULL) {
			return false;
		}

		if (!GetModuleInformation(GetCurrentProcess(), hCoreLib, &moduleInfo, sizeof(moduleInfo))) {
			Fail("Could not get module info of CoreLib.dll");
		}

		coreLibStart_ = (uint8_t const *)moduleInfo.lpBaseOfDll;
		coreLibSize_ = moduleInfo.SizeOfImage;

		return true;
	}

	void LibraryManager::FindLibrariesEoCPlugin()
	{
		uint8_t const prologue0[] = {
			0x40, 0x53, // push rbx
			0x48, 0x83, 0xEC, 0x20,  // sub rsp, 20h
			0x8B, 0x05 // mov eax, cs:xxx
		};

		uint8_t const prologue1[] = {
			0x48, 0x8B, 0xD9, // mov rbx, rcx
			0x8B, 0xD0, // mov edx, eax
			0xFF, 0xC0, // inc eax
			0x89, 0x05 // mov cs:xxx, eax
		};

		uint8_t const prologue2[] = {
			0x85, 0xD2, // test edx, edx
			0x75, 0x18, // jnz short loc_xxx
			0x44, 0x8D, 0x42, // lea r8d, [rdx+XXh]
		};

		uint8_t const prologue3[] = {
			0x48, 0x8D, 0x15 // lea rdx, xxx
		};

		uint8_t const * p = (uint8_t const *)moduleStart_;
		uint8_t const * moduleEnd = p + moduleSize_;

		for (; p < moduleEnd - 100; p++) {
			if (*p == 0x40
				&& memcmp(p, prologue0, sizeof(prologue0)) == 0
				&& memcmp(p + 0x0C, prologue1, sizeof(prologue1)) == 0
				&& memcmp(p + 0x19, prologue2, sizeof(prologue2)) == 0
				&& memcmp(p + 0x21, prologue3, sizeof(prologue3)) == 0) {
				int32_t rel1 = *(int32_t *)(p + 0x24);
				int32_t rel2 = *(int32_t *)(p + 0x2B);

				uint8_t const * freeFunc = p + rel1 + 0x21 + 7;
				uint8_t const * initFunc = p + rel2 + 0x28 + 7;

				auto it = libraries_.find(initFunc);
				if (it != libraries_.end()) {
					it->second.refs++;
				}
				else {
					libraries_.insert(std::pair<uint8_t const *, EoCLibraryInfo>(initFunc, { initFunc, freeFunc, 1 }));
				}
			}
		}

#if 0
		Debug("LibraryManager::FindLibrariesEoCPlugin(): Found libraries:");
		for (auto const & v : libraries_) {
			Debug("\t(Init %p; Dtor %p, Refs %d)!", v.second.initFunc, v.second.freeFunc, v.second.refs);
		}
#endif
	}

	void LibraryManager::FindServerGlobalsEoCPlugin()
	{
		EoCLibraryInfo const * serverLib{ nullptr };

		// 1) Look for the string "esv::CustomStatDefinitionComponent"
		Pattern statComponent;
		statComponent.FromRaw("esv::CustomStatDefinitionComponent");
		uint8_t const * statComponentStr = nullptr;
		statComponent.Scan((uint8_t const *)moduleStart_, moduleSize_, [&statComponentStr](const uint8_t * match) {
			statComponentStr = match;
		});

		if (!statComponentStr) Fail("Could not locate esv::CustomStatDefinitionComponent");

		// 2) Look for xrefs to esv::CustomStatDefinitionComponent
		Pattern xref;
		xref.FromString(
			"48 8B C8 " // mov rcx, rax
			"48 8D 15 XX XX XX XX " // lea rdx, xxx
			"FF 15 XX XX XX XX " // call cs:xxx
		);

		uint8_t const * statComponentXref = nullptr;
		xref.Scan((uint8_t const *)moduleStart_, moduleSize_, [&statComponentXref, statComponentStr](const uint8_t * match) {
			int32_t rel = *(int32_t const *)(match + 6);
			uint8_t const * refTo = match + rel + 7 + 3;

			if (refTo == statComponentStr) {
				statComponentXref = match;
			}
		});

		if (!statComponentXref) Fail("Could not locate registration xref for esv::CustomStatDefinitionComponent");

		Pattern p;
		p.FromString(
			"48 85 C0 " // test rax, rax
			"74 11 " // jz  xxx
			"48 8B C8 " // mov rcx, rax
			"E8 XX XX XX XX " // call ctor
			"48 89 05 XX XX XX XX " // mov cs:xxx, rax
		);

		uint32_t i = 0;
		p.Scan(statComponentXref, 0x1200, [this, &i](const uint8_t * match) {
			if (i < std::size(serverGlobals_)) {
				int32_t rel = *(int32_t const *)(match + 16);
				uint8_t const * refTo = match + rel + 7 + 13;
				serverGlobals_[i++] = (uint8_t const **)refTo;
			}
		});

		EsvCharacterFactory = (CharacterFactory **)serverGlobals_[(unsigned)EsvGlobalEoCPlugin::EsvCharacterFactory];
		EsvItemFactory = (ItemFactory **)serverGlobals_[(unsigned)EsvGlobalEoCPlugin::EsvItemFactory];

		// FIXME - HACK
		ShootProjectile = (esv::ProjectileHelpers_ShootProjectile)((uint8_t const *)moduleStart_ + 0xC13440 + 0x1000);
	}

	void LibraryManager::FindEoCGlobalsEoCPlugin()
	{
		uint8_t const * globalsInitCode{ nullptr };
		size_t nextGlobalIndex = 0;
		for (auto const & lib : libraries_) {
			for (auto p = lib.second.initFunc; p < lib.second.initFunc + 0x300; p++) {
				if (p[0] == 0xE8
					&& p[5] == 0xE8
					&& p[10] == 0xE8
					&& p[15] == 0xE8
					&& p[20] == 0xE8
					&& p[25] == 0xE8
					&& p[30] == 0x48 && p[31] == 0x39 && p[32] == 0x1D) { /* cmp cs:xxx, rbx */

					for (auto p2 = p + 0x30; p2 < p + 0x240; p2++) {
						if (p2[0] == 0xE8 /* call xxx */
							&& p2[5] == 0x48 && p2[6] == 0x89 && p2[7] == 0x05 /* mov cs:xxx, rax */
							&& p2[12] == 0xEB /* jmp xxx */) {

							int32_t rel = *(int32_t *)(p2 + 8);
							uint8_t const * globalPtr = p2 + rel + 5 + 7;
							eocGlobals_[nextGlobalIndex++] = (uint8_t const **)globalPtr;
							if (nextGlobalIndex >= std::size(eocGlobals_)) {
								break;
							}
						}
					}

					break;
				}

				if (nextGlobalIndex >= std::size(eocGlobals_)) {
					break;
				}
			}
		}

		if (nextGlobalIndex != std::size(eocGlobals_)) {
			Fail("LibraryManager::FindEoCGlobalsEoCPlugin(): Could not find all eoc globals!");
			return;
		}
	}

	void LibraryManager::FindGlobalStringTableCoreLib()
	{
		Pattern p;
		p.FromString(
			"B9 88 FA 5F 00 " // mov ecx, 5FFA88h
			"41 8D 51 01 " // lea edx, [r9+1]
			"E8 XX XX XX XX " // call alloc
			"48 85 C0 " // test rax, rax
			"74 1F " // jz short xxx
			"48 8B C8 " // mov rcx, rax
			"E8 XX XX XX XX " // call ctor
			"48 8D 4C 24 60 " // rcx, [rsp+58h+arg_0]
			"48 89 05 XX XX XX XX " // mov cs:xxx, rax
		);

		p.Scan((uint8_t const *)coreLibStart_, coreLibSize_, [this](const uint8_t * match) {
			int32_t rel = *(int32_t const *)(match + 35);
			auto refTo = match + rel + 7 + 32;

			GlobalStrings = (GlobalStringTable const **)refTo;
			GlobalStringTable::UseMurmur = true;
		});

		if (GlobalStrings == nullptr) {
			Debug("LibraryManager::FindGlobalStringTableCoreLib(): Could not find global string table");
		}
	}

	void LibraryManager::FindGameActionManagerEoCPlugin()
	{
		Pattern p;
		p.FromString(
			"48 8D 15 XX XX XX XX " // lea     rdx, fsx_GameAction
			"4C 8B 05 XX XX XX XX " // mov     r8, cs:xxx
			"48 8B CF " // mov     rcx, rdi
			"FF 90 80 00 00 00 " // call    qword ptr [rax+80h]
			"84 C0 " // test    al, al
			"0F 84 XX 00 00 00 " // jz      xxx
			"49 8B D6 " // mov     rdx, r14
			"48 8D 4D FF " // lea     rcx, [rbp+57h+Memory]
			"E8 XX XX XX XX " // call xxx
			"45 84 E4 " // test    r12b, r12b
			"74 XX " // jz      short xxx
			"49 8B D6 " // mov     rdx, r14
			"48 8D 4D FF " // lea     rcx, [rbp+57h+Memory]
			"48 8B D8 " // mov     rbx, rax
			"E8 XX XX XX XX " // call    xxx
			"4C 8B 03 " // mov     r8, [rbx]
			"49 8B CD " // mov     rcx, r13
			"8B 50 08 " // mov     edx, [rax+8]
			"E8 XX XX XX XX " // call    esv__GameActionManager__CreateAction
		);

		p.Scan(moduleStart_, moduleSize_, [this](const uint8_t * match) {
			auto fsx = AsmLeaToAbsoluteAddress(match);
			if (IsFixedStringRef(fsx, "GameAction")) {
				auto actionAddr = AsmCallToAbsoluteAddress(match + 72);
				CreateGameAction = (esv::GameActionManager__CreateAction)actionAddr;
			}
		});

		if (CreateGameAction == nullptr) {
			Debug("LibraryManager::FindGameActionManagerEoCPlugin(): Could not find GameActionManager::CreateAction");
		}

		Pattern p2;
		p2.FromString(
			"41 83 C8 FF " // or      r8d, 0FFFFFFFFh
			"48 8D 15 XX XX XX XX " // lea     rdx, fsx_ForceMove
			"48 8B C8 " // mov     rcx, rax
			"E8 XX XX XX XX " // call    eoc__SkillPrototype__GetMappedValue
			"85 C0 " // test    eax, eax
			"75 25 " // jnz     short loc_180EE4875
			"4C 8D 46 70 " // lea     r8, [rsi+70h]
		);

		Pattern p3;
		p3.FromString(
			"48 8B 0D XX XX XX XX " // mov     rcx, cs:ls__gServerLevelAllocator
			"48 89 7C 24 50 " // mov     [rsp+38h+arg_10], rdi
			"E8 XX XX XX XX " // call    esv__LevelManager__GetGameActionManager
			"4C 8B 05 XX XX XX XX " // mov     r8, cs:xxx
			"BA 08 00 00 00 " //  mov     edx, 8
			"48 8B C8 " // mov     rcx, rax
			"48 8B F8 " // mov     rdi, rax
			"4D 8B 00 " // mov     r8, [r8]
			"E8 XX XX XX XX " // call    esv__GameActionManager__CreateAction
			"4C 8D 46 70 " // lea     r8, [rsi+70h]
			"48 8B C8 " // mov     rcx, rax
			"48 8D 56 68 " // lea     rdx, [rsi+68h]
			"48 8B D8 " // mov     rbx, rax
			"E8 XX XX XX XX " // call esv__GameObjectMoveAction__Setup
			"48 8B D3 " // mov     rdx, rbx
			"48 8B CF " // mov     rcx, rdi
			"E8 XX XX XX XX " // call    esv__GameActionManager__AddAction
		);

		p2.Scan(moduleStart_, moduleSize_, [this, &p3](const uint8_t * match) {
			auto fsx = AsmLeaToAbsoluteAddress(match + 4);
			if (IsFixedStringRef(fsx, "ForceMove")) {
				p3.Scan(match, 0x100, [this](const uint8_t * match2) {
					LevelManager = (void **)AsmLeaToAbsoluteAddress(match2);

					auto moveSetupAddr = AsmCallToAbsoluteAddress(match2 + 57);
					GameObjectMoveActionSetup = (esv::GameObjectMoveAction__Setup)moveSetupAddr;

					auto addActionAddr = AsmCallToAbsoluteAddress(match2 + 68);
					AddGameAction = (esv::GameActionManager__AddAction)addActionAddr;
				});
			}
		});

		if (LevelManager == nullptr || AddGameAction == nullptr) {
			Debug("LibraryManager::FindGameActionManagerEoCPlugin(): Could not find esv::LevelManager");
		}
	}

	void LibraryManager::FindGameActionsEoCPlugin()
	{
		Pattern p;
		p.FromString(
			"48 8D 57 50 " // lea     rdx, [rdi+50h]
			"E8 XX XX XX XX " // call    eoc__SkillPrototypeManager__GetPrototype
			"41 83 C8 FF " // or      r8d, 0FFFFFFFFh
			"48 8D 15 XX XX XX XX " // lea     rdx, fsx_RandomPoints
			"48 8B C8 " // mov     rcx, rax
			"48 8B F0 " // mov     rsi, rax
			"E8 XX XX XX XX " // call    eoc__SkillPrototype__GetMappedValue
		);

		Pattern p2;
		p2.FromString(
			"48 8B C4 " // mov     rax, rsp
			"53 " // push    rbx
			"55 " // push    rbp
		);

		p.Scan(moduleStart_, moduleSize_, [this, &p2](const uint8_t * match) {
			auto fsx = AsmLeaToAbsoluteAddress(match + 13);
			if (IsFixedStringRef(fsx, "RandomPoints")) {
				p2.Scan(match - 0x100, 0x100, [this](const uint8_t * match2) {
					TornadoActionSetup = (esv::TornadoAction__Setup)match2;
				});
			}
		});

		if (TornadoActionSetup == nullptr) {
			Debug("LibraryManager::FindGameActionsEoCPlugin(): Could not find TornadoAction");
		}


		Pattern p3;
		p3.FromString(
			"41 83 C8 FF " // or      r8d, 0FFFFFFFFh
			"48 8D 15 XX XX XX XX " // lea     rdx, fs_GrowTimeout
			"48 8B C8 " // mov     rcx, rax
			"48 8B F0 " // mov     rsi, rax
			"E8 XX XX XX XX " // call    eoc__SkillPrototype__GetMappedValueDiv1k
			"44 0F 28 F8 " // movaps  xmm15, xmm0
			"48 8D 15 XX XX XX XX " // lea     rdx, fs_GrowSpeed
			"F3 44 0F 5E 3D XX XX XX XX " // divss   xmm15, cs:dword_1819A9AD0
		);

		Pattern p4;
		p4.FromString(
			"48 8B C4 " // mov     rax, rsp
			"48 89 58 18 " // mov     [rax+18h], rbx
			"48 89 70 20 " // mov     [rax+20h], rsi
		);

		p3.Scan(moduleStart_, moduleSize_, [this, &p4](const uint8_t * match) {
			auto fsx = AsmLeaToAbsoluteAddress(match + 4);
			auto fsx2 = AsmLeaToAbsoluteAddress(match + 26);
			if (IsFixedStringRef(fsx, "GrowTimeout")
				&& IsFixedStringRef(fsx2, "GrowSpeed")) {
				p4.Scan(match - 0x100, 0x100, [this](const uint8_t * match2) {
					WallActionCreateWall = (esv::TornadoAction__Setup)match2;
				});
			}
		});

		if (WallActionCreateWall == nullptr) {
			Debug("LibraryManager::FindGameActionsEoCPlugin(): Could not find WallAction");
		}


		Pattern p5;
		p5.FromString(
			"44 0F 28 94 24 20 01 00 00 " // movaps  xmm10, [rsp+1B0h+var_90]
			"48 8D 15 XX XX XX XX " // lea     rdx, fs_SpawnObject
			"49 8B CE " // mov     rcx, r14
		);

		Pattern p6;
		p6.FromString(
			"48 8D 55 E0 " // lea     rdx, [rbp+0B0h+summonArgs]
			"C6 45 0D 00 " // mov     [rbp+0B0h+summonArgs.MapToAiGrid_M], 0
			"48 8D 4C 24 50 " // lea     rcx, [rsp+1B0h+var_160]
			"E8 XX XX XX XX " // call    esv__SummonHelpers__Summon
			"48 8D 4C 24 50 " // lea     rcx, [rsp+1B0h+var_160]
		);

		p5.Scan(moduleStart_, moduleSize_, [this, &p6](const uint8_t * match) {
			auto fsx = AsmLeaToAbsoluteAddress(match + 9);
			if (IsFixedStringRef(fsx, "SpawnObject")) {
				p6.Scan(match - 0x400, 0x400, [this](const uint8_t * match2) {
					auto summon = AsmCallToAbsoluteAddress(match2 + 13);
					SummonHelpersSummon = (esv::SummonHelpers__Summon)summon;
				});
			}
		});

		if (SummonHelpersSummon == nullptr) {
			Debug("LibraryManager::FindGameActionsEoCPlugin(): Could not find SummonHelpers::Summon");
		}
	}

	void LibraryManager::FindStatusMachineEoCPlugin()
	{
		Pattern p;
		p.FromString(
			"83 7A 1C 00 " // cmp     dword ptr [rdx+1Ch], 0
			"48 8B F2 " // mov     rsi, rdx
			"4C 8B F1 " // mov     r14, rcx
			"7E 7E " // jle     short xxx
			"4C 8B 05 XX XX XX XX " // mov     r8, cs:?Unassigned@ObjectHandle@ls@@2V12@B
			"48 8D 15 XX XX XX XX " // lea     rdx, fs_LIFESTEAL
			"48 89 5C 24 30 " //  mov     [rsp+28h+arg_0], rbx
			"48 89 7C 24 40 " //  mov     [rsp+28h+arg_10], rdi
			"48 8B B9 B0 01 00 00 " //  mov     rdi, [rcx+1B0h]
			"4D 8B 00 " //  mov     r8, [r8]
			"48 8B CF " //  mov     rcx, rdi 
			"E8 XX XX XX XX " //  call    esv__StatusMachine__CreateStatus
		);

		uint8_t const * lastMatch;
		p.Scan(moduleStart_, moduleSize_, [this, &lastMatch](const uint8_t * match) {
			auto fsx = AsmLeaToAbsoluteAddress(match + 19);
			if (IsFixedStringRef(fsx, "LIFESTEAL")) {
				auto actionAddr = AsmCallToAbsoluteAddress(match + 49);
				lastMatch = match + 55;
				StatusMachineCreateStatus = (esv::StatusMachine__CreateStatus)actionAddr;
			}
		});

		if (StatusMachineCreateStatus == nullptr) {
			Debug("LibraryManager::FindStatusMachineEoCPlugin(): Could not find StatusMachine::CreateStatus");
		}

		Pattern p2;
		p2.FromString(
			"C7 43 2C 00 00 00 00 " // mov     dword ptr [rbx+2Ch], 0
			"48 8B CF " // mov     rcx, rdi
			"E8 XX XX XX XX " // call    esv__StatusMachine__ApplyStatus
			"48 8B 7C 24 40 " // mov     rdi, [rsp+28h+arg_10]
		);

		p2.Scan(lastMatch, 0x100, [this](const uint8_t * match) {
			auto actionAddr = AsmCallToAbsoluteAddress(match + 10);
			StatusMachineApplyStatus = (esv::StatusMachine__ApplyStatus)actionAddr;
		});

		if (StatusMachineApplyStatus == nullptr) {
			Debug("LibraryManager::FindStatusMachineEoCPlugin(): Could not find StatusMachine::ApplyStatus");
		}
	}

	void LibraryManager::FindStatusTypesEoCPlugin()
	{
		Pattern p;
		p.FromString(
			"45 33 C9 " // xor     r9d, r9d
			"48 8D 15 XX XX XX XX " // lea     rdx, fsx_TargetDependentHeal
			"48 8B CB " // mov     rcx, rbx
			"FF 90 B0 01 00 00 " // call    qword ptr [rax+1B0h]
		);

		Pattern p2;
		p2.FromString(
			"48 89 5C 24 10 " // mov     [rsp-8+arg_8], rbx
			"48 89 74 24 18 " // mov     [rsp-8+arg_10], rsi
		);

		p.Scan(moduleStart_, moduleSize_, [this, &p2](const uint8_t * match) {
			auto fsx = AsmLeaToAbsoluteAddress(match + 3);
			if (IsFixedStringRef(fsx, "TargetDependentHeal")) {
				p2.Scan(match - 0x200, 0x200, [this](const uint8_t * match) {
					// Look for this function ptr
					auto ptr = (uint64_t)match;
					for (auto p = moduleStart_; p < moduleStart_ + moduleSize_; p += 8) {
						if (*reinterpret_cast<uint64_t const *>(p) == ptr) {
							StatusHealVMT = reinterpret_cast<esv::StatusVMT const *>(p - 25 * 8);
						}
					}
				});
			}
		});

		if (StatusHealVMT == nullptr) {
			Debug("LibraryManager::FindStatusTypesEoCPlugin(): Could not find esv::StatusHeal");
		}

		Pattern p3;
		p3.FromString(
			"4C 8D 0D XX XX XX XX " // lea     r9, fsx_Dummy_BodyFX
			"48 8D 15 XX XX XX XX " // lea     rdx, fsx_RS3_FX_GP_Status_Retaliation_Beam_01
			"E8 XX XX XX XX " // call    esv__EffectFactory__CreateEffectWrapper
			"48 8B D8 " // mov     rbx, rax
		);

		Pattern p4;
		p4.FromString(
			"48 8B C4 " // mov     rax, rsp
			"55 " // push    rbp
			"53 " // push    rbx
		);

		p3.Scan(moduleStart_, moduleSize_, [this, &p4](const uint8_t * match) {
			auto fsx = AsmLeaToAbsoluteAddress(match + 7);
			if (IsFixedStringRef(fsx, "RS3_FX_GP_Status_Retaliation_Beam_01")) {
				p4.Scan(match - 0x600, 0x600, [this](const uint8_t * match) {
					// Look for this function ptr
					auto ptr = (uint64_t)match;
					for (auto p = moduleStart_; p < moduleStart_ + moduleSize_; p += 8) {
						if (*reinterpret_cast<uint64_t const *>(p) == ptr) {
							StatusHitVMT = reinterpret_cast<esv::StatusVMT const *>(p - 12 * 8);
						}
					}
				});
			}
		});

		if (StatusHitVMT == nullptr) {
			Debug("LibraryManager::FindStatusTypesEoCPlugin(): Could not find esv::StatusHit");
		}
	}


	void LibraryManager::FindHitFuncsEoCPlugin()
	{
		Pattern p;
		p.FromString(
			"4C 89 XX 24 18 01 00 00 " // mov     [rsp+150h+var_38], r15
			"E8 XX XX XX XX " // call    eoc__StatusPrototype__GetMappedValue
			"85 C0 " // test    eax, eax
			"48 8D 15 XX XX XX XX " // lea     rdx, fsx_DamageItems
			"48 8B CB " // mov     rcx, rbx
			"40 0F 95 C7 " // setnz   dil
		);

		Pattern p2;
		p2.FromString(
			"48 89 44 24 30 " // mov     qword ptr [rsp+150h+a7], rax
			"C6 44 24 28 00 " // mov     byte ptr [rsp+150h+a6], 0
			"C7 44 24 20 05 00 00 00 " // mov     [rsp+150h+a5], 5
			"E8 XX XX XX XX " // call    esv__Character__Hit
			"XX 8B XX B0 01 00 00 " // mov     r13, [r15+1B0h]
			"EB 66 " // jmp short xxx
		);

		p.Scan(moduleStart_, moduleSize_, [this, &p2](const uint8_t * match) {
			auto fsx = AsmLeaToAbsoluteAddress(match + 15);
			if (IsFixedStringRef(fsx, "DamageItems")) {
				p2.Scan(match, 0x300, [this](const uint8_t * match) {
					auto actionAddr = AsmCallToAbsoluteAddress(match + 18);
					CharacterHit = (esv::Character__Hit)actionAddr;
				});
			}
		});

		if (CharacterHit == nullptr) {
			Debug("LibraryManager::FindHitFuncsEoCPlugin(): Could not find Character::Hit");
		}
	}

	void LibraryManager::FindItemFuncsEoCPlugin()
	{
		Pattern p;
		p.FromString(
			"48 8B C8 " // mov     rcx, rax
			"48 89 5C 24 60 " // mov     [rsp+0B8h+var_58.VMT], rbx
			"E8 XX XX XX XX " // call    esv__ParseItem
			"33 D2 " // xor     edx, edx
			"48 8D 4C 24 60 " // lea     rcx, [rsp+0B8h+var_58]
			"E8 XX XX XX XX " // call    esv__CreateItemFromParsed
		);

		p.Scan(moduleStart_, moduleSize_, [this](const uint8_t * match) {
			auto parseAddr = AsmCallToAbsoluteAddress(match + 8);
			ParseItem = (esv::ParseItem)parseAddr;
			auto createAddr = AsmCallToAbsoluteAddress(match + 20);
			CreateItemFromParsed = (esv::CreateItemFromParsed)createAddr;
		});

		if (ParseItem == nullptr || CreateItemFromParsed == nullptr) {
			Debug("LibraryManager::FindItemFuncsEoCPlugin(): Could not find esv::CreateItemFromParsed");
		}
	}
}
#endif
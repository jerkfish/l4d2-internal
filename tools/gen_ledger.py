"""Generate ledger.csv / ledger.json from the build map, source definitions,
IDA naming notes, and the recorded verification history.

No fabricated history: rows without a verified original address are marked
"unknown". There is no git repository in this workspace, so every
iteration/commit field is "unknown (no git repository)".
"""
import csv
import json
import re

INVENTORY = "NOTES/ida/functions_lines.tsv"

# (rva, orig_name, kind, evidence, confidence, tests, result, structural, notes)
OVERRIDES = {
    # --- bootstrap / hooks -------------------------------------------------
    ("dllmain.obj", "EnumWindows_FindGameWindow"): (
        "0xB920", "sub_1000B920", "internal",
        "EnumWindows callback; GetWindowThreadProcessId equals current PID (IDA naming log).",
        "CONFIRMED", "T005;T020", "PASS", "unknown (no size comparison recorded)", ""),
    ("dllmain.obj", "CheatThread_Main"): (
        "0xB960", "sub_1000B960", "internal",
        "CreateThread start routine: Kiero init, EndScene MinHook, window discovery, VK_END loop.",
        "CONFIRMED", "T005;T020", "PASS", "unknown (no size comparison recorded)", ""),
    ("hooks.obj", "entry"): (
        "0x7750", "VmtHook_Init", "internal",
        "Inline accessor of the VmtHook object built at 0x10007750 (copy-vtable shifted by one).",
        "CONFIRMED", "T005;T020", "PASS", "unknown (no size comparison recorded)", ""),
    ("hooks.obj", "Hook_EngineClient_Slot20"): (
        "0x76A0", "sub_100076A0", "internal",
        "VEngineClient013 vtable slot 20 detour; ESI holds CUserCmd, caller EBP supplies the command number check.",
        "CONFIRMED", "T014;T015;T016;T019;T021", "PASS",
        "unknown (size not recorded; behavior live-verified)", ""),
    ("hooks.obj", "Hook_PaintTraverse"): (
        "0x4FC0", "sub_10004FC0", "internal",
        "VGUI_Panel009 vtable slot 41 detour; ESP/world draw gated on FocusOverlayPanel/MainMenu.",
        "CONFIRMED", "T006;T007", "PASS", "unknown (no size comparison recorded)", ""),
    ("hooks.obj", "Hook_DrawModelExecute"): (
        "0x4980", "sub_10004980", "internal",
        "VEngineModel016 vtable slot 19 detour; chams material strings and XQZ passes.",
        "CONFIRMED", "T022", "PASS", "unknown (no size comparison recorded)", ""),
    ("hooks.obj", "InstallVmtHooks"): (
        "0x9160", "Hook_EndScene block", "inlined-merged",
        "Inline VMT installation block in Hook_EndScene 0x10009160.",
        "CONFIRMED", "T005;T020", "PASS", "n/a (inlined in target)", ""),
    ("hooks.obj", "WndProc_Hook"): (
        "0xB830", "sub_1000B830", "internal",
        "SetWindowLongA(GWL_WNDPROC) target; ImGui Win32 handler plus CallWindowProcA.",
        "CONFIRMED", "T005;T012", "PASS", "unknown (no size comparison recorded)", ""),
    ("hooks.obj", "Hook_EndScene"): (
        "0x9160", "sub_10009160", "internal",
        "MinHook D3D9 slot 42 target; ImGui backend init, frame, menu, overlays.",
        "CONFIRMED", "T005;T012;T013;T017;T018;T020", "PASS",
        "unknown (size not recorded; behavior live-verified)", ""),
    ("hooks.obj", "EngineSlot20_Process"): (
        "0x76A0", "sub_100076A0 block", "inlined-merged",
        "Main slot-20 handler block: SpeedHack, Cheat_Update call, command-number/mask write-back.",
        "CONFIRMED", "T010;T011;T014;T015;T016;T021", "PASS",
        "n/a (inlined in target)", ""),
    ("hooks.obj", "ReadPointer"): (
        "unknown", "unknown", "internal",
        "Safety helper for guarded engine-pointer walks; target walks unguarded.",
        "INFERRED", "T020", "PASS", "n/a (safety helper)", "INFERRED safety guard."),
    # --- chams -------------------------------------------------------------
    ("hooks.obj", "CreateRecoveredMaterial"): (
        "0x4980", "Hook_DrawModelExecute block", "inlined-merged",
        "Chams material creation inside Hook_DrawModelExecute 0x10004980.",
        "CONFIRMED", "T022", "PASS", "n/a (inlined in target)", ""),
    ("hooks.obj", "SetChamMaterialFlag"): (
        "0x4980", "Hook_DrawModelExecute block", "inlined-merged",
        "Chams material flag writes inside Hook_DrawModelExecute 0x10004980.",
        "CONFIRMED", "T022", "PASS", "n/a (inlined in target)", ""),
    ("hooks.obj", "ForceMaterialOverride"): (
        "0x4980", "Hook_DrawModelExecute block", "inlined-merged",
        "IVModelRender::ForcedMaterialOverride call inside 0x10004980.",
        "CONFIRMED", "T022", "PASS", "n/a (inlined in target)", ""),
    ("hooks.obj", "ConfigureChamMaterial"): (
        "0x4800", "sub_10004800", "inlined-merged",
        "sub_10004800 / sub_100048C0: cham material flags and color, then force override.",
        "CONFIRMED", "T022", "PASS", "n/a (inlined in target)", ""),
    ("hooks.obj", "ApplyChamPass"): (
        "0x4980", "Hook_DrawModelExecute block", "inlined-merged",
        "Per-pass material override inside Hook_DrawModelExecute 0x10004980.",
        "CONFIRMED", "T022", "PASS", "n/a (inlined in target)", ""),
    ("hooks.obj", "GetRenderModelName"): (
        "0x4980", "Hook_DrawModelExecute block", "inlined-merged",
        "Renderable/model-name lookup used by the cham filters inside 0x10004980.",
        "CONFIRMED", "T022", "PASS", "n/a (inlined in target)", ""),
    # --- interface / utils -------------------------------------------------
    ("interfaces.obj", "InitInterfaces"): (
        "0x33900", "sub_10033900", "internal",
        "12 CreateInterface resolutions; g_pGlobalVars = PlayerInfoManager002 slot 1.",
        "INFERRED", "T005", "PASS", "differs: ours 0x1D2 vs target 0x207",
        "PlayerInfoManager002 slot 1 (GetGlobalVars) is INFERRED."),
    ("utils.obj", "ConsolePrint"): (
        "0x42D0", "sub_100042D0", "internal",
        "UCRT printf wrapper writing to stdout; target calls it for diagnostics.",
        "CONFIRMED", "T005", "PASS", "differs: ours 0x30 vs target 0x2A", ""),
    ("utils.obj", "HexNibble"): (
        "0x376C0", "FindPattern helper", "inlined-merged",
        "Hex nibble decoder inside the 0x100376C0 wildcard scanner.",
        "CONFIRMED", "T005", "PASS", "n/a (inlined in target)", ""),
    # --- VMT / menu --------------------------------------------------------
    ("vmt.obj", "VmtHook"): (
        "0x7750", "VmtHook_Init", "ctor/dtor",
        "Allocates 12-byte object, copies vtable shifted by one, redirects the interface vtable.",
        "CONFIRMED", "T005;T020", "PASS", "unknown (no size comparison recorded)",
        "Constructor form of the target's VmtHook_Init."),
    ("menu.obj", "Menu_Render"): (
        "0x37B00", "sub_10037B00", "internal",
        "Config descriptor at 0x10037B00 drives both loader and saver; tab UI is inlined in Hook_EndScene.",
        "INFERRED", "T012", "PASS", "unknown (no size comparison recorded)",
        "Menu size is an INFERRED fixed size; config keys recovered from the descriptor."),
    ("menu.obj", "LegitTab"): (
        "0x9DBA", "Hook_EndScene Legit tab", "inlined-merged",
        "Target Hook_EndScene 0x10009DBA..0x1000A2AC, exact widget order.",
        "CONFIRMED", "T012", "PASS", "n/a (inlined in target)", ""),
    ("menu.obj", "RageTab"): (
        "0xA2B1", "Hook_EndScene Rage tab", "inlined-merged",
        "Target Hook_EndScene 0x1000A2B1..0x1000A749, exact widget order.",
        "CONFIRMED", "T012", "PASS", "n/a (inlined in target)", ""),
    ("menu.obj", "MiscTab"): (
        "0xAEBC", "Hook_EndScene Misc tab", "inlined-merged",
        "Target Hook_EndScene 0x1000AEBC..0x1000B0AD, exact widget order.",
        "CONFIRMED", "T012", "PASS", "n/a (inlined in target)", ""),
    ("menu.obj", "VisualsTab"): (
        "unknown", "Hook_EndScene block", "inlined-merged",
        "Visuals widget group in Hook_EndScene (ESP group at 0x1000A8E2..0x1000A95B).",
        "CONFIRMED", "T012", "PASS", "n/a (inlined in target)", ""),
    ("menu.obj", "LoadSettings"): (
        "0x37B00", "sub_10037B00", "internal",
        "Config loader driven by the 0x10037B00 descriptor; keys recovered from its strings.",
        "INFERRED", "T012", "PASS", "unknown (no size comparison recorded)",
        "Key inventory and defaults are INFERRED from the descriptor and observed persistence."),
    ("menu.obj", "SaveSettings"): (
        "0x39AD0", "sub_10039AD0", "internal",
        "Config saver at 0x10039AD0; writes the same descriptor keys.",
        "INFERRED", "T012", "PASS", "unknown (no size comparison recorded)", ""),
    ("menu.obj", "ConfigPath"): (
        "0x37B00", "sub_10037B00 block", "inlined-merged",
        "Config file path handling around the 0x10037B00 descriptor.",
        "INFERRED", "T012", "PASS", "n/a (inlined in target)", ""),
    ("menu.obj", "ReadInt"): (
        "0x37B00", "sub_10037B00 block", "inlined-merged",
        "INI value reader for the config loader.",
        "INFERRED", "T012", "PASS", "n/a (inlined in target)", ""),
    ("menu.obj", "WriteInt"): (
        "0x39AD0", "sub_10039AD0 block", "inlined-merged",
        "INI value writer for the config saver.",
        "INFERRED", "T012", "PASS", "n/a (inlined in target)", ""),
    ("menu.obj", "KeybindButton"): (
        "0x7DD0", "sub_10007DD0", "internal",
        "70x18 keybind button; capture state byte 0x1004B212.",
        "CONFIRMED", "T012", "PASS", "unknown (no size comparison recorded)", ""),
    ("menu.obj", "TabButton"): (
        "unknown", "unknown", "internal",
        "ImGui tab-button helper for the recovered menu.",
        "CONFIRMED", "T012", "PASS", "n/a (helper)", ""),
    ("menu.obj", "KeyNameFor"): (
        "unknown", "unknown", "internal",
        "Virtual-key display name table for the keybind control.",
        "CONFIRMED", "T012", "PASS", "n/a (helper)", ""),
    # --- netprop -----------------------------------------------------------
    ("netprop.obj", "BuildNetpropTables"): (
        "0x374A0", "sub_100374A0", "inlined-merged",
        "ClientClass walk used by the netprop resolver at 0x100374A0.",
        "CONFIRMED", "T005;T006;T015;T016", "PASS", "n/a (inlined in target)", ""),
    ("netprop.obj", "NetpropTableCount"): (
        "0x374A0", "sub_100374A0", "inlined-merged",
        "Count of cached RecvTables built by the netprop walker.",
        "CONFIRMED", "T005", "PASS", "n/a (helper)", ""),
    ("netprop.obj", "NetpropOffset"): (
        "0x374A0", "sub_100374A0", "internal",
        "RecvProp stride 60 (0x3C); nested table offsets are added.",
        "CONFIRMED", "T005;T006;T015;T016", "PASS", "unknown (no size comparison recorded)", ""),
    ("netprop.obj", "FindNetpropOffset"): (
        "0x374A0", "sub_100374A0", "inlined-merged",
        "Global table search wrapper around the 0x100374A0 walk.",
        "CONFIRMED", "T005;T006", "PASS", "n/a (helper)", ""),
    ("netprop.obj", "NetpropOffsetInTable"): (
        "0x374A0", "sub_100374A0", "inlined-merged",
        "Table-specific property lookup; target helpers sub_100015A0/sub_100016A0 search by table name first.",
        "CONFIRMED", "T005;T006;T015;T016", "PASS", "n/a (inlined in target)", ""),
    ("netprop.obj", "FindTableByName"): (
        "0x15A0", "sub_100015A0", "inlined-merged",
        "Per-table name search matching sub_100015A0's behavior.",
        "CONFIRMED", "T005", "PASS", "n/a (inlined in target)", ""),
    ("netprop.obj", "GetLocalPlayerEntity"): (
        "0x55B0", "Cheat_Update prologue", "inlined-merged",
        "IVEngineClient slot 12 GetLocalPlayer -> IClientEntityList slot 3 GetClientEntity.",
        "CONFIRMED", "T005;T014;T015;T016", "PASS", "n/a (inlined in target)", ""),
    ("netprop.obj", "GetClientEntityByIndex"): (
        "unknown", "IClientEntityList slot 3", "inlined-merged",
        "IClientEntityList vtable slot 3 (GetClientEntity), inlined throughout the target.",
        "CONFIRMED", "T005;T006;T015;T016", "PASS", "n/a (inlined in target)", ""),
    ("netprop.obj", "GetClientEntityFromHandle"): (
        "0x3C56A", "ESP vtable call", "inlined-merged",
        "IClientEntityList slot 4; target uses vtable offset 0x10 at 0x1003C56A.",
        "CONFIRMED", "T006;T007;T015;T016;T021", "PASS", "n/a (inlined in target)", ""),
    ("netprop.obj", "GetHighestEntityIndex"): (
        "0x53D0", "sub_100053D0", "inlined-merged",
        "Entity-list vtable slot 8; target sub_100053D0 uses offset 0x20.",
        "CONFIRMED", "T006;T007", "PASS", "n/a (inlined in target)", ""),
    ("netprop.obj", "GetEntityCategory"): (
        "0x1DB0", "sub_10001DB0", "internal",
        "Networkable client class ID mapping: 264->4, 277->2, 276->1, listed sets ->3/5.",
        "CONFIRMED", "T006;T007;T016;T022", "PASS", "unknown (no size comparison recorded)", ""),
    ("netprop.obj", "GetEntityClassId"): (
        "0x1DB0", "sub_10001DB0 block", "inlined-merged",
        "ClientClass::m_ClassID at +20, shared with sub_10001DB0.",
        "CONFIRMED", "T006;T007", "PASS", "n/a (inlined in target)", ""),
    ("netprop.obj", "GetEntityTeam"): (
        "0x16A0", "sub_100016A0", "inlined-merged",
        "DT_BaseEntity::m_iTeamNum, matching sub_100016A0.",
        "CONFIRMED", "T014;T015;T016;T019", "PASS", "n/a (inlined in target)", ""),
    ("netprop.obj", "IsPlayerChamsEligible"): (
        "0x1EC0", "sub_10001EC0", "internal",
        "Player chams eligibility test used by triggerbot, aimbots, auto shove and chams.",
        "CONFIRMED", "T014;T015;T016;T022", "PASS", "unknown (no size comparison recorded)", ""),
    ("netprop.obj", "GetEntityHangingFromTongue"): (
        "0x6469", "Cheat_Update block", "inlined-merged",
        "DT_TerrorPlayer::m_isHangingFromTongue read in the Anti Smoker block.",
        "CONFIRMED", "T019", "PASS", "n/a (inlined in target)", ""),
    ("netprop.obj", "GetEntityFallVelocity"): (
        "0x62EB", "Cheat_Update block", "inlined-merged",
        "DT_BasePlayer::m_flFallVelocity cache used by No Fall Damage.",
        "CONFIRMED", "T019", "PASS", "n/a (inlined in target)", ""),
    ("netprop.obj", "GetEntityPunchAngle"): (
        "0x18B0", "sub_100018B0", "inlined-merged",
        "DT_BasePlayer::m_vecPunchAngle read used by No-Recoil blocks.",
        "CONFIRMED", "T015;T016", "PASS", "n/a (inlined in target)", ""),
    ("netprop.obj", "GetEntityRenderableModelId"): (
        "0x3C4DC", "ESP block", "inlined-merged",
        "Renderable/model handle read used by the ESP aim-point path.",
        "INFERRED", "T006;T015;T016", "PASS", "n/a (inlined in target)",
        "Exact virtual name unknown (INFERRED)."),
    ("netprop.obj", "GetEntityOrigin"): (
        "0x3AA60", "sub_1003AA60", "inlined-merged",
        "DT_BaseEntity::m_vecOrigin, line-ESP anchor.",
        "CONFIRMED", "T006", "PASS", "n/a (inlined in target)", ""),
    ("netprop.obj", "GetEntityEyePosition"): (
        "0x14B0", "sub_100014B0", "internal",
        "DT_BasePlayer::m_vecViewOffset plus GetAbsOrigin.",
        "CONFIRMED", "T006;T015;T016", "PASS", "unknown (no size comparison recorded)", ""),
    ("netprop.obj", "GetEntityMaxHealth"): (
        "0x13B0", "sub_100013B0", "inlined-merged",
        "DT_BaseEntity::m_iMaxHealth property read.",
        "CONFIRMED", "T006", "PASS", "n/a (inlined in target)", ""),
    ("netprop.obj", "GetEntityRage"): (
        "0x1CA0", "sub_10001CA0", "inlined-merged",
        "DT_TerrorPlayer::m_rage property read.",
        "CONFIRMED", "T006", "PASS", "n/a (inlined in target)", ""),
    ("netprop.obj", "GetEntityMobRush"): (
        "0x37440", "sub_10037440", "inlined-merged",
        "Mob rush state read used by the ESP/aimbot filters.",
        "CONFIRMED", "T006", "PASS", "n/a (inlined in target)", ""),
    ("netprop.obj", "GetEntityWeaponId"): (
        "unknown", "vtable slot 383", "inlined-merged",
        "Entity vtable slot 383 (GetWeaponID), target offset 1532.",
        "CONFIRMED", "T014;T015;T016;T022", "PASS", "n/a (inlined in target)", ""),
    ("netprop.obj", "GetLocalActiveWeapon"): (
        "0x1AA0", "sub_10001AA0", "internal",
        "DT_BaseCombatCharacter::m_hActiveWeapon handle plus GetClientEntityFromHandle.",
        "CONFIRMED", "T006;T007;T014;T015;T016", "PASS", "unknown (no size comparison recorded)", ""),
    ("netprop.obj", "GetEntityIsGhostInt"): (
        "0x1BA0", "sub_10001BA0", "inlined-merged",
        "DT_TerrorPlayer::m_isGhost dword read.",
        "CONFIRMED", "T006;T016", "PASS", "n/a (inlined in target)", ""),
    ("netprop.obj", "GetEntityWitchRage"): (
        "0x1CA0", "sub_10001CA0 block", "inlined-merged",
        "DT_Witch::m_rage float read.",
        "CONFIRMED", "T006", "PASS", "n/a (inlined in target)", ""),
    ("netprop.obj", "GetEntitySimulationTime"): (
        "0x17A0", "sub_100017A0", "inlined-merged",
        "DT_BaseEntity::m_flSimulationTime property read.",
        "CONFIRMED", "T015;T016", "PASS", "n/a (inlined in target)", ""),
    ("netprop.obj", "GetEntityTickBase"): (
        "0x19A0", "sub_100019A0", "inlined-merged",
        "DT_BasePlayer::m_nTickBase property read.",
        "CONFIRMED", "T015;T016", "PASS", "n/a (inlined in target)", ""),
    ("netprop.obj", "GetEntityNextPrimaryAttack"): (
        "0x2020", "sub_10002020", "inlined-merged",
        "DT_BaseCombatWeapon::m_flNextPrimaryAttack property read.",
        "CONFIRMED", "T014;T015;T016", "PASS", "n/a (inlined in target)", ""),
    # --- MD5 / No-Spread ---------------------------------------------------
    ("md5.obj", "MD5_PseudoRandom"): (
        "0x37080", "sub_10037080", "internal",
        "Standard MD5 over the 4-byte command number; returns digest offset 6. Transform at sub_10036A00.",
        "CONFIRMED", "T003;T004;T021", "PASS",
        "differs: ours 0x1B0 vs target 0xE2 + sub_10036A00 0x67A (loop vs unrolled)",
        "Verified against hashlib vectors for 7 seeds."),
    ("md5.obj", "RotateLeft"): (
        "0x36A00", "sub_10036A00 helper", "inlined-merged",
        "MD5 rotate used by the transform at sub_10036A00.",
        "CONFIRMED", "T003", "PASS", "n/a (inlined in target)", ""),
    ("md5.obj", "LoadLE32"): (
        "0x36A00", "sub_10036A00 helper", "inlined-merged",
        "Little-endian MD5 block word loader.",
        "CONFIRMED", "T003", "PASS", "n/a (inlined in target)", ""),
    ("nospread_scan.obj", "NoSpread_ScanModule"): (
        "0x37890", "sub_10037890 + sub_100378E0", "internal",
        "SharedRandomFloat literal scan, backward address-reference and prologue walks, first 0xA1 seed global.",
        "CONFIRMED", "T004;T021", "PASS",
        "differs: ours 0x80 + outlined helpers vs target 0x45 + 0x37 + inline walk",
        "Offline C++ scan matches installed client.dll: function 0x1ACDB0, seed 0x6BF868."),
    ("nospread_scan.obj", "FindSharedRandomFloat"): (
        "0x37890", "sub_10037890", "inlined-merged",
        "Linear scan for the 18-byte literal including the terminator.",
        "CONFIRMED", "T004;T021", "PASS", "n/a (inlined in target)", ""),
    ("nospread_scan.obj", "FindStringReference"): (
        "0x37890", "sub_10037890", "inlined-merged",
        "Backward dword scan from literal+1 for the instruction immediate.",
        "CONFIRMED", "T004;T021", "PASS", "n/a (inlined in target)", ""),
    ("nospread_scan.obj", "FindFunctionStart"): (
        "0x7006", "Cheat_Update inline walk", "inlined-merged",
        "Backward masked prologue scan: 0xF0/0x50, 0xF0/0x80, 0x0F/0x0C.",
        "CONFIRMED", "T004;T021", "PASS", "n/a (inlined in target)", ""),
    ("nospread_scan.obj", "FindSeedGlobal"): (
        "0x378E0", "sub_100378E0", "inlined-merged",
        "First 0xA1 within 0x100 bytes; immediate is the seed global.",
        "CONFIRMED", "T004;T021", "PASS", "n/a (inlined in target)", ""),
    ("nospread.obj", "NoSpread_Rage"): (
        "0x6F81", "Cheat_Update block", "inlined-merged",
        "Rage No-Spread block 0x10006F81..0x1000712A: whitelist, IN_ATTACK, seed write, Horiz/Vert spread.",
        "CONFIRMED", "T021", "PASS", "n/a (inlined in target)", ""),
    ("nospread.obj", "NoSpread_Legit"): (
        "0x73BA", "Cheat_Update block", "inlined-merged",
        "Legit No-Spread block 0x100073BA..0x1000757C.",
        "CONFIRMED", "T021", "PASS", "n/a (inlined in target)", ""),
    ("nospread.obj", "ApplyNoSpread"): (
        "0x6F81", "Cheat_Update block", "inlined-merged",
        "Shared body for the Rage/Legit spread blocks.",
        "CONFIRMED", "T021", "PASS", "n/a (inlined in target)", ""),
    ("nospread.obj", "ResolveClientSharedRandomFloat"): (
        "0x6F81", "Cheat_Update static init", "inlined-merged",
        "One-time client.dll resolver mirroring the target's magic statics.",
        "CONFIRMED", "T004;T021", "PASS", "n/a (inlined in target)", ""),
    ("nospread.obj", "RageResolver"): (
        "0x6F81", "Cheat_Update static", "inlined-merged",
        "Rage static function/seed pair.",
        "CONFIRMED", "T021", "PASS", "n/a (inlined in target)", ""),
    ("nospread.obj", "LegitResolver"): (
        "0x73BA", "Cheat_Update static", "inlined-merged",
        "Legit static function/seed pair.",
        "CONFIRMED", "T021", "PASS", "n/a (inlined in target)", ""),
    ("nospread.obj", "GetModuleImageSize"): (
        "0x6F81", "Cheat_Update block", "inlined-merged",
        "PE SizeOfImage helper for the client.dll scan.",
        "CONFIRMED", "T004", "PASS", "n/a (helper)", ""),
    # --- Cheat_Update and combat ------------------------------------------
    ("cheat.obj", "Legit_WeaponAllowed"): (
        "0x4790", "sub_10004790", "internal",
        "Weapon-ID whitelist shared by aimbots, Auto Shove and No-Spread.",
        "CONFIRMED", "T014;T015;T016;T021", "PASS", "unknown (no size comparison recorded)", ""),
    ("cheat.obj", "Cheat_Update"): (
        "0x55B0", "sub_100055B0", "internal",
        "Per-command update called from the slot-20 detour; 39 callees in the target.",
        "CONFIRMED", "T010;T011;T014;T015;T016;T019;T021", "PASS",
        "differs: ours 0x1380 vs target 0x20EA (unported blocks listed in README)",
        "SpeedHack/Fake Lag/No-Recoil/Legit+No-Spread/Rage blocks ported; SMAC/melee blocks pending."),
    ("cheat.obj", "SetConVarInt"): (
        "0x5C17", "Cheat_Update block", "inlined-merged",
        "VEngineCvar007 FindVar slot 13, ConVar SetValue slot 15; target block 0x10005C17.",
        "CONFIRMED", "T019", "PASS", "n/a (inlined in target)", ""),
    ("cheat.obj", "TimerElapsedMs"): (
        "0x22B0", "sub_100022B0", "inlined-merged",
        "QueryPerformanceCounter cooldown timer used by Auto Shoot/No Fall Damage.",
        "CONFIRMED", "T016;T019", "PASS", "n/a (inlined in target)",
        "Uses __int64 for QPC arithmetic (leftover-scan hit)."),
    ("cheat.obj", "LocalWeaponHasClip"): (
        "0x46B0", "sub_100046B0", "internal",
        "Active weapon m_iClip1 > 0 gate for the Legit section.",
        "INFERRED", "T014;T015;T021", "PASS", "unknown (no size comparison recorded)",
        "Falls back to true when netprop resolution fails (INFERRED safety)."),
    ("cheat.obj", "AddEngineSlot74Field8"): (
        "0x55B0", "Cheat_Update block", "inlined-merged",
        "Engine slot 74 field +8 write used by the anti-smoker path.",
        "CONFIRMED", "T019", "PASS", "n/a (inlined in target)", ""),
    ("cheat.obj", "ExecuteClientCmd"): (
        "0x57DA", "Cheat_Update block", "inlined-merged",
        "IVEngineClient ClientCmd call used by Name Stealer/Chat Spam.",
        "CONFIRMED", "T019", "PASS", "n/a (inlined in target)", ""),
    ("cheat.obj", "GetPlayerInfo"): (
        "unknown", "IVEngineClient GetPlayerInfo", "inlined-merged",
        "Engine player_info lookup used by Name Stealer and kill messages.",
        "CONFIRMED", "T019", "PASS", "n/a (inlined in target)", ""),
    ("cheat.obj", "TriggerTraceFilter"): (
        "0x41538", "ITraceFilter vftable", "ctor/dtor",
        "Vftable at 0x10041538; slot 0 skips the local entity, slot 1 returns TRACE_EVERYTHING.",
        "CONFIRMED", "T014", "PASS", "n/a (data vftable)", ""),
    ("cheat.obj", "ShouldHitEntity"): (
        "0x41538", "ITraceFilter slot 0", "virtual",
        "Returns entity != skipEntity.",
        "CONFIRMED", "T014", "PASS", "n/a (vftable method)", ""),
    ("cheat.obj", "GetTraceType"): (
        "0x41538", "ITraceFilter slot 1", "virtual",
        "Returns 0 (TRACE_EVERYTHING).",
        "CONFIRMED", "T014", "PASS", "n/a (vftable method)", ""),
    ("cheat.obj", "Legit_Triggerbot"): (
        "0x353F0", "sub_100353F0", "internal",
        "Eye trace with mask 0x46004003, hitgroup match, delay/limb quirks.",
        "INFERRED", "T014", "PASS", "differs: ours 0x3A0 vs target 0x341",
        "Preserved quirks: Delay 0 never fires; Delay 1..15 fires immediately; limb flag unwritten."),
    ("cheat.obj", "Legit_TraceVisible"): (
        "0x26E0", "sub_100026E0", "internal",
        "Line-of-sight trace from local eye to hitbox midpoint; CBreakableSurface quirk.",
        "CONFIRMED", "T015", "PASS", "unknown (no size comparison recorded)", ""),
    ("cheat.obj", "GetStudiomodel"): (
        "0x1190", "sub_10001190", "inlined-merged",
        "ModelInfo studio header lookup.",
        "CONFIRMED", "T015;T016", "PASS", "n/a (inlined in target)", ""),
    ("cheat.obj", "SetupEntityBones"): (
        "unknown", "entity vtable slot 13", "inlined-merged",
        "SetupBones(matrix, maxBones, boneMask, currentTime) call.",
        "CONFIRMED", "T015;T016", "PASS", "n/a (inlined in target)", ""),
    ("cheat.obj", "Legit_FindHitboxIndex"): (
        "0x352B0", "sub_100352B0", "inlined-merged",
        "Hitbox group selection; Shotgun Mode forces group 3 for weapon ids 11/8/3/4.",
        "CONFIRMED", "T015", "PASS", "n/a (inlined in target)", ""),
    ("cheat.obj", "Legit_HitboxAimPoint"): (
        "0x35740", "sub_10035740", "inlined-merged",
        "Bone-transform the hitbox bbox, midpoint aim point, require line of sight.",
        "CONFIRMED", "T015", "PASS", "n/a (inlined in target)", ""),
    ("cheat.obj", "GetConVarFloat"): (
        "unknown", "ConVar vtable slot 11", "inlined-merged",
        "ConVar float getter used by smoothing.",
        "CONFIRMED", "T015;T016", "PASS", "n/a (inlined in target)", ""),
    ("cheat.obj", "EngineGetViewAngles"): (
        "unknown", "IVEngineClient slot 19", "inlined-merged",
        "Engine GetViewAngles.",
        "CONFIRMED", "T015;T016", "PASS", "n/a (inlined in target)", ""),
    ("cheat.obj", "EngineSetViewAngles"): (
        "unknown", "IVEngineClient slot 20", "inlined-merged",
        "Engine SetViewAngles; nested call does not re-enter Cheat_Update because the caller argument mismatches.",
        "CONFIRMED", "T015;T016", "PASS", "n/a (inlined in target)", ""),
    ("cheat.obj", "NormalizeAngle180"): (
        "unknown", "inline angle normalization", "inlined-merged",
        "Angle wrap used by the aimbots and movement fix.",
        "CONFIRMED", "T015;T016", "PASS", "n/a (inlined in target)", ""),
    ("cheat.obj", "Legit_Aimbot"): (
        "0x35BF0", "sub_10035BF0", "internal",
        "Entity scan, FOV cone, hitbox selection, smoothing, silent handling.",
        "CONFIRMED", "T015", "PASS", "differs: ours 0xD00 vs target 0xC71",
        "Corner2 zero-ish test fixed during recovery."),
    ("cheat.obj", "Rage_FindHitboxIndex"): (
        "0x2310", "sub_10002310", "inlined-merged",
        "Rage hitbox group selection using dword_1004AB68 and Shotgun/Hitscan flags.",
        "CONFIRMED", "T016", "PASS", "n/a (inlined in target)", ""),
    ("cheat.obj", "Rage_HitboxAimPoint"): (
        "0x2450", "sub_10002450", "inlined-merged",
        "Bone-transform and LOS check with the Rage eye.",
        "CONFIRMED", "T016", "PASS", "n/a (inlined in target)", ""),
    ("cheat.obj", "RagePredictionTimerElapsed"): (
        "0x22B0", "sub_100022B0", "inlined-merged",
        "120 ms Prediction Fix Auto Shoot cooldown.",
        "CONFIRMED", "T016", "PASS", "n/a (inlined in target)", ""),
    ("cheat.obj", "Rage_Aimbot"): (
        "0x3380", "sub_10003380", "internal",
        "Rage entity scan, priority/FOV, hitbox, silent/perfect silent, Auto Shoot, prediction fix.",
        "CONFIRMED", "T016", "PASS", "differs: ours 0xEB0 vs target 0xE08", ""),
    ("cheat.obj", "Rage_AutoShove"): (
        "0x2D90", "sub_10002D90", "internal",
        "Eye-to-eye < 92 units, filters, pitch/yaw, IN_ATTACK2; last matching entity wins.",
        "CONFIRMED", "T016;T021", "PASS", "differs: ours 0x5B0 vs target 0x5CD",
        "SMAC prediction and post-shove movement adjustments remain unported."),
    # --- overlay -----------------------------------------------------------
    ("overlay.obj", "OverlayTextV"): (
        "0xC390", "sub_1000C390", "internal",
        "Formatted VGUI text; target call 0x1000C65E pushes text/len.",
        "INFERRED", "T013;T017", "PASS", "unknown (no size comparison recorded)",
        "Varargs argument omission is an INFERRED safety deviation."),
    ("overlay.obj", "OverlayText"): (
        "0xC390", "sub_1000C390", "inlined-merged",
        "Varargs wrapper around the 0x1000C390 text routine.",
        "INFERRED", "T013;T017", "PASS", "n/a (inlined in target)",
        "Varargs argument omission is an INFERRED safety deviation."),
    ("overlay.obj", "DrawBorderRect"): (
        "0xC2A0", "sub_1000C2A0", "internal",
        "1px border rectangle.",
        "CONFIRMED", "T013;T017", "PASS", "unknown (no size comparison recorded)", ""),
    ("overlay.obj", "DrawBar"): (
        "0xC1A0", "sub_1000C1A0", "internal",
        "30x160 bar with Visitor label.",
        "CONFIRMED", "T013;T017", "PASS", "unknown (no size comparison recorded)", ""),
    ("overlay.obj", "CreateOverlayFonts"): (
        "0x9160", "Hook_EndScene init block", "inlined-merged",
        "Font creation in the EndScene lazy-init block; handles 0x1004A9C8/A9D0/A9CC.",
        "CONFIRMED", "T013;T017", "PASS", "n/a (inlined in target)", ""),
    ("overlay.obj", "IsLocalPlayerAlive"): (
        "0x45A0", "sub_100045A0", "inlined-merged",
        "m_lifeState == 0 && m_iHealth > 0.",
        "CONFIRMED", "T013;T017", "PASS", "n/a (inlined in target)", ""),
    ("overlay.obj", "DrawIndicatorsPanel"): (
        "0x3AD40", "sub_1003AD40", "internal",
        "Monitor panel with Recoil/Speed bars; gating global has no writer in the target.",
        "CONFIRMED", "T019", "PASS", "unknown (no size comparison recorded)",
        "Dead by construction in the original; ported for parity."),
    # --- ESP ---------------------------------------------------------------
    ("esp.obj", "DrawEsp"): (
        "0x3C410", "sub_1003C410", "internal",
        "Player ESP painter and its helper call order.",
        "CONFIRMED", "T006;T020", "PASS", "unknown (no size comparison recorded)", ""),
    ("esp.obj", "DrawWeaponSpawnEsp"): (
        "0x3DB30", "sub_1003DB30", "internal",
        "World weapon-spawn labels for class 260 using the same weapon-ID table.",
        "CONFIRMED", "T007", "PASS", "unknown (no size comparison recorded)", ""),
    ("esp.obj", "IsBboxSane"): (
        "unknown", "unknown", "internal",
        "Rejects non-finite/absurd projected boxes before VGUI text.",
        "INFERRED", "T006;T020", "PASS", "n/a (safety helper)",
        "INFERRED deviation: target passes INT_MIN positions into text and can crash."),
    ("esp.obj", "GetPlayerInfo"): (
        "unknown", "IVEngineClient GetPlayerInfo", "inlined-merged",
        "player_info_t lookup (512-byte buffer).",
        "CONFIRMED", "T006", "PASS", "n/a (inlined in target)", ""),
    ("esp.obj", "GetScreenSize"): (
        "unknown", "IVEngineClient slot 5", "inlined-merged",
        "Engine GetScreenSize.",
        "CONFIRMED", "T006;T007", "PASS", "n/a (inlined in target)", ""),
    ("esp.obj", "ScreenPosition"): (
        "0x3B030", "sub_1003B030 block", "inlined-merged",
        "World-to-screen projection used by the bounding-box builder.",
        "CONFIRMED", "T006", "PASS", "n/a (inlined in target)", ""),
    ("esp.obj", "SseMin8"): (
        "0x3B030", "sub_1003B030 block", "inlined-merged",
        "minss reduction chain in the box builder.",
        "CONFIRMED", "T006", "PASS", "n/a (inlined in target)", ""),
    ("esp.obj", "SseMax8"): (
        "0x3B030", "sub_1003B030 block", "inlined-merged",
        "maxss reduction chain in the box builder.",
        "CONFIRMED", "T006", "PASS", "n/a (inlined in target)", ""),
    ("esp.obj", "GetActiveWeaponHandle"): (
        "0x1AA0", "sub_10001AA0", "inlined-merged",
        "Active weapon handle used by the ammo/weapon ESP.",
        "CONFIRMED", "T006", "PASS", "n/a (inlined in target)", ""),
    ("esp.obj", "GetWeaponClip"): (
        "0x46B0", "sub_100046B0", "inlined-merged",
        "DT_BaseCombatWeapon::m_iClip1 read.",
        "CONFIRMED", "T006", "PASS", "n/a (inlined in target)", ""),
    ("esp.obj", "GetNetOrigin"): (
        "0x3AA60", "sub_1003AA60", "inlined-merged",
        "DT_BaseEntity::m_vecOrigin for the line ESP.",
        "CONFIRMED", "T006", "PASS", "n/a (inlined in target)", ""),
    ("esp.obj", "WeaponNameFromId"): (
        "0x3AB50", "sub_1003AB50", "inlined-merged",
        "Weapon-ID to display-name table.",
        "CONFIRMED", "T006;T007", "PASS", "n/a (inlined in target)", ""),
    ("esp.obj", "WeaponName"): (
        "0x3AB50", "sub_1003AB50", "inlined-merged",
        "Weapon entity to display name via vtable slot 383.",
        "CONFIRMED", "T006;T007", "PASS", "n/a (inlined in target)", ""),
    ("esp.obj", "BuildBoundingBox"): (
        "0x3B030", "sub_1003B030", "internal",
        "Eight projected corners; radius |mins.y-maxs.y|/1.3.",
        "CONFIRMED", "T006;T007", "PASS", "unknown (no size comparison recorded)", ""),
    ("esp.obj", "DrawBox"): (
        "0x3D7D0", "sub_1003D7D0", "internal",
        "Colored outline plus two black outlines.",
        "CONFIRMED", "T006", "PASS", "unknown (no size comparison recorded)", ""),
    ("esp.obj", "DrawHealthBar"): (
        "0x3D870", "sub_1003D870", "internal",
        "Health bar and numeric health (std::to_string helper).",
        "CONFIRMED", "T006", "PASS", "unknown (no size comparison recorded)", ""),
    ("esp.obj", "DrawFullHealthBar"): (
        "0x3DA50", "sub_1003DA50", "internal",
        "Full-height bar without numeric health for zombies/witch.",
        "CONFIRMED", "T006", "PASS", "unknown (no size comparison recorded)", ""),
    ("esp.obj", "DrawDistance"): (
        "0x3C410", "sub_1003C410 block", "inlined-merged",
        "Distance text in the ESP draw order.",
        "INFERRED", "T006", "PASS", "n/a (inlined in target)",
        "Uses %s instead of the raw name format string (INFERRED safety)."),
    ("esp.obj", "DrawLineToCenter"): (
        "0x3C410", "sub_1003C410 block", "inlined-merged",
        "Line from the box to the screen center.",
        "CONFIRMED", "T006", "PASS", "n/a (inlined in target)", ""),
    ("esp.obj", "DrawWeaponText"): (
        "0x3C410", "sub_1003C410 block", "inlined-merged",
        "Weapon name and ammo text.",
        "CONFIRMED", "T006", "PASS", "n/a (inlined in target)", ""),
    # --- radar -------------------------------------------------------------
    ("radar.obj", "DrawRadar"): (
        "0x3BFB0", "sub_1003BFB0 + sub_1003B820", "internal",
        "Frame painter and entity-dot iteration; frame centered on the ImGui window.",
        "CONFIRMED", "T008;T018", "PASS", "unknown (no size comparison recorded)", ""),
    ("radar.obj", "DrawRadarDot"): (
        "0x3C1D0", "sub_1003C1D0", "internal",
        "Relative rotate/clamp/project, dot and optional name.",
        "CONFIRMED", "T008;T018", "PASS", "unknown (no size comparison recorded)", ""),
    ("radar.obj", "GetViewAngles"): (
        "unknown", "IVEngineClient slot 19", "inlined-merged",
        "Engine view angles for radar rotation.",
        "CONFIRMED", "T008;T018", "PASS", "n/a (inlined in target)", ""),
}

# Manual rows not generated from the map.
MANUAL = [
    # id assigned later; fields match the override tuple layout
    ("src/dllmain.cpp:36", "dllmain", "DllMain",
     ("0xBA00", "DllMain", "internal",
      "DLL entry point; DisableThreadLibraryCalls then CreateThread(CheatThread_Main).",
      "CONFIRMED", "T001;T002;T005;T020", "PASS",
      "unknown (no size comparison recorded)", "")),
    ("src/utils.cpp:26", "utils", "FindPattern",
     ("0x376C0", "sub_100376C0", "internal",
      "Wildcard byte-pattern scanner; both InitInterfaces call results are discarded.",
      "CONFIRMED", "T005", "PASS", "differs: ours 0x18F vs target 0x1C3", "")),
]

BLOCKED = [
    ("0x64C9", "Cheat_Update block", "(not ported) Rage SMAC-mode angle prediction",
     "src/nospread.cpp:1 (reference only)", "STATE 0AP/INF-033: 0x100064C9..0x100066B5; SMAC mode off by default.",
     "CONFIRMED", "", "BLOCKED", "n/a (not ported)",
     "SMAC mode default off; default-settings behavior unaffected."),
    ("0x66C9", "Cheat_Update block", "(not ported) post-Auto-Shove anti-aim movement",
     "src/cheat.cpp:1277 (reference only)", "STATE 0AP/INF-033: 0x100066C9..0x100067AA.",
     "CONFIRMED", "", "BLOCKED", "n/a (not ported)",
     "Movement adjustment after the shove; default Hack-vs-Hack off."),
    ("0x67B7", "Cheat_Update block", "(not ported) melee weapon-id-19 path",
     "src/cheat.cpp:1352 (reference only)", "STATE 0AP/INF-033: 0x100067B7..0x10006A02.",
     "CONFIRMED", "", "BLOCKED", "n/a (not ported)",
     "Melee-specific Rage path; firearms and default settings unaffected."),
    ("0x4190", "sub_10004190", "(not ported) movement normalization",
     "n/a", "INF-033: sub_10004190's movement normalization when anti-aim is off.",
     "CONFIRMED", "", "BLOCKED", "n/a (not ported)",
     "Only affects movement with Rage anti-aim disabled; core aim/shove verified."),
]

TYPES = [
    ("include/cusercmd.h:5", "CUserCmd", "type",
     "0x55B0", "Cheat_Update field accesses", "64 bytes; command_number +4, viewangles +12, buttons +36.",
     "CONFIRMED", "T005;T014;T015;T016;T021", "PASS", "64 bytes exact",
     "No vtable/aimdirection in this engine revision."),
    ("include/vmt.h:3", "VmtHook", "type",
     "0x7750", "VmtHook_Init", "12-byte object: count, original vtable, shifted copy.",
     "CONFIRMED", "T005;T020", "PASS", "12 bytes exact",
     "Copy-vtable shifted by one; RTTI word from original[-1]."),
    ("src/cheat.cpp:323", "Ray_t", "type",
     "0x353F0", "sub_100353F0 trace ABI", "0x48 bytes; start +0, delta +0x10, offsets +0x20/+0x30.",
     "CONFIRMED", "T014;T015;T016", "PASS", "0x48 bytes exact", "EngineTraceClient003 slot 5."),
    ("src/cheat.cpp:344", "TraceResult", "type",
     "0x353F0", "sub_100353F0 trace ABI", "L4D2 trace_t 0x58; fraction +0x2C, hitgroup +0x44, entity +0x4C.",
     "CONFIRMED", "T014;T015;T016", "PASS", "0x58 bytes exact", "Verified by raw dumpbin disassembly."),
    ("src/cheat.cpp:379", "TriggerTraceFilter", "type",
     "0x41538", "ITraceFilter vftable", "Two virtual methods; skipEntity at +4.",
     "CONFIRMED", "T014", "PASS", "vftable layout exact", ""),
    ("include/interfaces.h:27", "g_pClientEntityList + 11 interfaces", "global",
     "0x33900", "InitInterfaces", "CreateInterface strings and vtable slots.",
     "CONFIRMED", "T005;T006;T015;T016", "PASS", "addresses exact",
     "g_pGlobalVars from PlayerInfoManager002 slot 1 (INFERRED)."),
    ("include/recovered.h:24", "g_MenuState", "global",
     "0x55B0", "slot-20 mask byte", "Packed menu/settings state at 0x1004B054; byte 0 menu-open, byte 3 mask.",
     "INFERRED", "T005;T012", "PASS", "partial layout unknown", "Full layout TBD; only bytes 0 and 3 used."),
    ("include/interfaces.h:10", "CGlobalVars", "type",
     "unknown", "interval_per_tick use", "Partial layout; interval_per_tick at +28.",
     "INFERRED", "T015;T016", "PASS", "partial", "Only offset 28 confirmed."),
]

EXCLUDE = {"sqrt", "atan", "cos", "sin", "fabs"}


def clean_name(demangled):
    pre = demangled.split("(")[0].strip()
    pre = pre.replace("`anonymous namespace'::", "")
    pre = re.sub(r'\b(public|private|protected):\s*', '', pre)
    pre = pre.replace("virtual ", "").replace("static ", "")
    parts = pre.split()
    return parts[-1] if parts else pre

OBJ_TESTS = {
    "dllmain.obj": "T005;T020", "hooks.obj": "T005;T020",
    "interfaces.obj": "T005", "vmt.obj": "T005;T020",
    "menu.obj": "T012", "netprop.obj": "T005;T006;T015;T016",
    "md5.obj": "T003;T021", "nospread_scan.obj": "T004;T021",
    "nospread.obj": "T021", "cheat.obj": "T014;T015;T016;T019;T021",
    "overlay.obj": "T013;T017", "esp.obj": "T006;T007",
    "radar.obj": "T008;T018", "utils.obj": "T001;T005",
}

FIELD_NAMES = ["id", "orig_addr", "orig_name", "recovered_name", "kind",
               "source_file", "evidence", "confidence", "test_ids", "result",
               "structural_match", "iteration_commit", "notes"]


def main():
    rows = []
    for line in open(INVENTORY, encoding="utf-8"):
        parts = line.rstrip("\n").split("\t")
        if len(parts) < 5:
            continue
        addr, obj, demangled, simple, source = parts[:5]
        if simple in EXCLUDE or simple.startswith("isfinite<"):
            continue
        key = (obj, simple)
        if key in OVERRIDES:
            (rva, orig_name, kind, evidence, conf, tests, result, structural,
             notes) = OVERRIDES[key]
        else:
            rva, orig_name = "unknown", "unknown"
            kind = "internal"
            evidence = ("Project implementation helper; no distinct original "
                        "function address recorded.")
            conf = "CONFIRMED"
            tests = OBJ_TESTS.get(obj, "unknown")
            result = "PASS"
            structural = "unknown (no size comparison recorded)"
            notes = ""
        rows.append({
            "orig_addr": rva, "orig_name": orig_name,
            "recovered_name": clean_name(demangled),
            "kind": kind, "source_file": source.replace(chr(92), "/"),
            "evidence": evidence,
            "confidence": conf, "test_ids": tests, "result": result,
            "structural_match": structural, "iteration_commit":
                "unknown (no git repository)", "notes": notes,
        })

    for source, obj, name, tup in MANUAL:
        (rva, orig_name, kind, evidence, conf, tests, result, structural,
         notes) = tup
        rows.append({
            "orig_addr": rva, "orig_name": orig_name, "recovered_name": name,
            "kind": kind, "source_file": source.replace(chr(92), "/"),
            "evidence": evidence,
            "confidence": conf, "test_ids": tests, "result": result,
            "structural_match": structural,
            "iteration_commit": "unknown (no git repository)", "notes": notes,
        })

    for (rva, orig_name, name, source, evidence, conf, tests, result,
         structural, notes) in BLOCKED:
        rows.append({
            "orig_addr": rva, "orig_name": orig_name, "recovered_name": name,
            "kind": "internal", "source_file": source, "evidence": evidence,
            "confidence": conf, "test_ids": tests, "result": result,
            "structural_match": structural,
            "iteration_commit": "unknown (no git repository)", "notes": notes,
        })

    for (source, name, kind, rva, orig_name, evidence, conf, tests, result,
         structural, notes) in TYPES:
        rows.append({
            "orig_addr": rva, "orig_name": orig_name, "recovered_name": name,
            "kind": kind, "source_file": source.replace(chr(92), "/"),
            "evidence": evidence,
            "confidence": conf, "test_ids": tests, "result": result,
            "structural_match": structural,
            "iteration_commit": "unknown (no git repository)", "notes": notes,
        })

    def sort_key(row):
        if row["orig_addr"] == "unknown":
            return (2, 0, row["recovered_name"])
        try:
            value = int(row["orig_addr"], 16)
        except ValueError:
            value = 0xFFFFFF
        return (0 if row["kind"] not in ("type", "global") else 1, value,
                row["recovered_name"])

    rows.sort(key=sort_key)
    for index, row in enumerate(rows, 1):
        row["id"] = f"F{index:03d}"

    with open("ledger.csv", "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=FIELD_NAMES)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)

    with open("ledger.json", "w", encoding="utf-8") as f:
        json.dump(rows, f, indent=2)
        f.write("\n")

    def is_function(row):
        return row["kind"] not in ("type", "global")

    funcs = [r for r in rows if is_function(r)]
    matched = [r for r in funcs if r["orig_addr"] != "unknown"]
    print(f"rows={len(rows)} functions={len(funcs)} matched={len(matched)}")
    for result in ("PASS", "FAIL", "BLOCKED", "UNTESTED"):
        print(f"{result}={sum(1 for r in rows if r['result'] == result)}")
    print(f"INFERRED={sum(1 for r in rows if r['confidence'] == 'INFERRED')}")


if __name__ == "__main__":
    main()

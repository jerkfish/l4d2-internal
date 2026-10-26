// Shared declarations for the recovered VR.dll source.
//
// Naming policy: every identifier that is inferred rather than proven carries
// an INFERRED comment at its definition and an entry in NOTES/inferences.md.
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <cstdint>

// --- Bootstrap state (addresses from original/target.dll) ------------------

// INFERRED: trampoline returned by MinHook for IDirect3DDevice9::EndScene
// (0x1004A98C; called at the end of the EndScene detour).
extern IDirect3DDevice9 *g_pD3D9Device;
extern HRESULT(__stdcall *g_pfnEndScene)(IDirect3DDevice9 *pDevice);

// INFERRED: game window and original window procedure (0x1004B144/0x1004A9B0).
extern HWND g_hGameWindow;
extern WNDPROC g_pfnPrevWndProc;

// INFERRED: packed menu/settings state (0x1004B054). Byte 0 gates mouse
// input capture; byte 3 is set to 1 at the start of Cheat_Update and used as
// a mask in the slot-20 detour. Exact layout still under recovery.
extern uint32_t g_MenuState;

// --- Recovered entry points ------------------------------------------------

DWORD __stdcall CheatThread_Main(LPVOID lpThreadParameter);
BOOL __stdcall EnumWindows_FindGameWindow(HWND hWnd, LPARAM lParam);
LRESULT __stdcall WndProc_Hook(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
HRESULT __stdcall Hook_EndScene(IDirect3DDevice9 *pDevice);

// 0x10037B00: cheat menu builder (86 cheat + 20 config strings). PENDING.
void Menu_Render();

// Recovered per-tick feature state.
extern bool g_BunnyHopEnabled;   // byte_1004ABF0 (AUTOBH, menu "Bunny Hop")
extern bool g_AutoStrafer;       // byte_1004ABF1 (config-only "AutoStrafer")
extern bool g_FovChangerEnabled;
extern int g_FovViewModel;   // dword_1004AB90, slider 51..130
extern int g_FovDesired;     // dword_1004AB8C, slider 90..120

// Legit tab (target .data 0x1004AB3C..0x1004AB7A; defaults from
// sub_1003A300 are all zero, with dword_1004AB8C = 51).
extern bool g_LegitEnabled;             // byte_1004AB3C "ENABLEAIMLEGIT"
extern int g_LegitAimKey;               // dword_1004AB40 "LegitAimKey"
extern int g_LegitAimHitbox;            // dword_1004AB44 "LegitAimHitbox"
extern int g_LegitHitboxMode;           // dword_1004AB48 "LegitAimHitboxMode"
extern bool g_LegitShotgunMode;         // byte_1004AB4C "LegitShotgunMode"
extern bool g_LegitNoRecoil;            // byte_1004AB4D "LEGITNORECOIL"
extern bool g_LegitNoSpread;            // byte_1004AB4E "LEGITNOSPREAD"
extern int g_LegitAimFov;               // dword_1004AB50 "LEGITAIMFOV", 0..15
extern int g_LegitSmoothX;              // dword_1004AB54, saved as "LEGITSMOOTHYAW"
extern int g_LegitSmoothY;              // dword_1004AB58, not persisted (target)
extern bool g_LegitPositionAdjustment;  // byte_1004AB5C, saved as "LegitLagComp"
extern bool g_LegitIgnoreWitch;         // byte_1004AB5D "LegitIGNOREWITCH"
extern bool g_LegitIgnoreTeam;          // byte_1004AB5E "LegitIgnoreTeam"
extern bool g_LegitHitscan;             // byte_1004AB5F "LegitbHitscanMode"
extern bool g_LegitPrioritySpecial;     // byte_1004AB60 "Legitpriorityspecial"
extern bool g_LegitAutoShove;           // byte_1004AB61 "LegitAutoShove"
extern int g_LegitShoveFov;             // dword_1004AB64 "LegitAutoShoveFov", 0..15
extern bool g_LegitFovCircle;           // byte_1004AB7A "FovVisualize"
extern bool g_LegitTriggerbot;          // byte_1004AFFC, not persisted (target)
extern int g_LegitTriggerKey;           // dword_1004ABA4 "TriggKey"
extern int g_LegitTriggerDelay;         // dword_1004B000, not persisted (target)
extern bool g_LegitAimHead;             // byte_1004AFFD "trighead"
extern bool g_LegitAimBody;             // byte_1004AFFE "triggerbody"
// dword_1004B004: compared as float != 0 by sub_100353F0 before accepting
// hitgroups 4..7. The target has no writer (no menu/config reference), so it
// is always zero and the limb group is dead. Preserved for equivalence.
extern float g_LegitTriggerLimbs;
// dword_1004B184: sub_100353F0 delay counter (BSS, starts at zero).
extern int g_LegitTriggerDelayCounter;
extern bool g_AspectRatioEnabled;  // byte_1004AB80
extern int g_AspectRatioValue;     // dword_1004AB84, slider 0..3
extern bool g_NoVisualRecoil;  // byte_1004AB97
extern bool g_PlayerChams;     // byte_1004ABE0
extern int g_PlayerChamR;      // dword_1004AFD8
extern int g_PlayerChamG;      // dword_1004AFDC
extern int g_PlayerChamB;      // dword_1004AFE0
extern bool g_WeaponChams;       // byte_1004ABEC (config "wpnchams")
extern bool g_HandWeaponChams;   // byte_1004ABED (config "Handwpnchams")
extern bool g_SurvivorChams;     // byte_1004ABEE (config "ChamsSURVIVOR")
extern bool g_InfectedChams;     // byte_1004ABEF (config "ChamsINFECTED")
extern bool g_HandChams;       // byte_1004ABE2
extern bool g_XQZChams;        // byte_1004ABE1
extern int g_HandChamR;        // dword_1004AFE4
extern int g_HandChamG;        // dword_1004AFE8
extern int g_HandChamB;        // dword_1004AFEC
extern bool g_RemoveVomit;     // byte_1004B008 low byte
extern bool g_RemoveFog;       // byte_1004ABDE
extern bool g_FullBright;      // byte_1004ABDF
extern bool g_NoFallDamage;    // byte_1004B00D
extern bool g_InstantWeaponSwitch;  // byte_1004B00B
extern bool g_InstantRevive;        // byte_1004B00C
extern bool g_ChatSpam;             // byte_1004AC14
extern int g_ChatSpamMode;          // dword_1004AC18 (0 Advertise, 1 Chat Jammer)
extern bool g_NameStealer;          // byte_1004B015
extern bool g_LagExploit;           // byte_1004B03C
extern int g_LagFast;               // dword_1004B038, slider 0..1000
extern int g_LagKey;                // dword_1004B034, VK code
extern bool g_Indicators;           // byte_1004B06C (menu "Indicators")
// Dead-by-construction settings (flags are read only by inert target code).
extern bool g_InvisibleJockey;      // byte_1004B014 (no config key)
extern bool g_AutoPistol;           // byte_1004AB7C "AutoPistol" (no code)
extern bool g_ServerLagger;         // byte_1004B03D "serverlag" (no code)
// Visuals tab ESP flags (Menu_Render 0x10037B00; all default 0).
extern bool g_EspEnable;       // byte_1004ABCE "Enable ESP"
extern bool g_EspShowTeam;     // byte_1004ABCF "Show Team"
extern bool g_EspName;         // byte_1004ABD5 "Player Name ESP"
extern bool g_EspBox;          // byte_1004ABD2 "Player Box ESP"
extern bool g_EspDistance;     // byte_1004ABD7 "Player Distance ESP"
extern bool g_EspHealth;       // byte_1004ABD8 "Player Health ESP"
extern bool g_EspHealthBar;    // byte_1004ABD9 "Player HealthBar ESP"
extern bool g_EspWeapons;      // byte_1004ABDA "Weapons ESP"
extern bool g_EspWeapons2;     // byte_1004ABDB "Weapons ESP##weapons2"
extern bool g_EspAmmo;         // byte_1004ABDC "Ammo ESP"
extern bool g_EspWitchRage;    // byte_1004ABD1 "Witch Rage Bar ESP"
extern bool g_EspLines;        // byte_1004ABD3 "Lines ESP"
extern bool g_EspBotTag;       // byte_1004ABD6 "EspDanger" ([BOT]/DANGER)
extern bool g_Radar;           // byte_1004AC30 "Radar"
extern bool g_RadarNames;      // byte_1004AC31 "Show names"
extern bool g_Thirdperson;     // byte_1004ABF2 "Thirdperson"
extern int g_ThirdpersonKey;   // dword_1004ABF4 "ThirdpersonKey"
extern bool g_SpeedHack;       // byte_1004B024 "SpeedHack" (PENDING)
extern bool g_AirStuck;        // byte_1004AC1D "Air Stuck"
extern bool g_Teleport;        // byte_1004AC1C "Teleport"
extern bool g_FakeLag;         // byte_1004AC0E "Fake Lag" (PENDING)
extern int g_TeleportKey;      // dword_1004ABAC "TeleportKey"
extern int g_AirStuckKey;      // dword_1004ABA8 "AirstuckKey"
extern int g_SpeedHackValue;   // dword_1004B02C "SpeedhackVal", slider 0..14
extern int g_SpeedHackKey;     // dword_1004B028 "SpeedhackKey", default 'S' (115)
extern int g_FakeLagValue;     // dword_1004AC10 "FakeLagValue", slider 0..16
extern bool g_AntiSmoker;           // byte_1004B00E
extern int g_SmokerMode;            // dword_1004B010 (0 Always, 1 Reach Smoker)
extern bool g_RageEnabled;     // byte_1004ABFC (Hack vs Hack enabled)
extern bool g_SmacMode;        // word_1004AC0C low byte
extern int g_RagePitch;        // dword_1004AC00 "Pitch"
extern int g_RageRealYaw;      // dword_1004AC04 "Yaw"
extern int g_RageDesyncYaw;    // dword_1004AC08 "FakeYaw"
// Rage tab (target Hook_EndScene 0x1000A2B1..0x1000A749; defaults all 0).
extern bool g_RageAimbotEnabled;      // byte_1004AB7B "Aimbot"
extern int g_RageAimKey;              // dword_1004AB9C "AimKey"
extern int g_RageHitbox;              // dword_1004AB6C (not persisted)
extern bool g_RageAutoShoot;          // byte_1004ABC0 "AutoFire"
extern bool g_RageSilentAim;          // byte_1004ABC2 "Silent"
extern bool g_RagePerfectSilentAim;   // byte_1004ABC3 "PSilent"
extern bool g_RageShotgunMode;        // byte_1004AB79 "ShotgunMode"
extern bool g_RageAutoShove;          // byte_1004ABC4 "AutoShove"
extern bool g_RageAutoMelee;          // byte_1004ABC5 "AutoMelee"
extern int g_RageAimFov;              // dword_1004ABB0 "AimFov", 0..180
extern int g_RageAimHitboxResolved;   // dword_1004AB68 "AimHitbox" (runtime
                                      // auto-mapping target; persisted)
extern int g_RageSmooth;              // dword_1004AB70 "Smooth" (no UI;
                                      // persisted config-only smoothing)
extern bool g_RageNoRecoil;           // byte_1004ABBE "NoRecoil"
extern bool g_RageNoSpread;           // byte_1004ABBF "NoSpread"
extern bool g_RagePositionAdjustment; // byte_1004AB7E "LagComp"
extern bool g_RagePredictionFix;      // byte_1004AB99 "PredictionFix"
extern bool g_RagePrioritySpecial;    // byte_1004ABC1 "priorityspecial"
extern bool g_RageHitscan;            // byte_1004AB78 "bHitscanMode"
extern bool g_RageIgnoreTeam;         // byte_1004ABBD "IgnoreTeam"
extern bool g_RageIgnoreWitch;        // byte_1004ABC6 "IgnoreWitch"

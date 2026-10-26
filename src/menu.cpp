// Cheat menu (0x10037B00 config + Hook_EndScene inline UI).
//
// Recovered: window title "L4D2 bot by space785", flags 8234, tabs
// Legit/Rage/Visuals/Misc, INSERT toggle. Legit tab widgets recovered from
// Hook_EndScene case 0 — sub_10030180(label, bool*) is ImGui::Checkbox.
// Combo/slider helpers (sub_100080D0 / sub_100312F0) are PENDING.
#include "recovered.h"

#include <windows.h>

#include "imgui.h"

// --- Config persistence (INFERRED wiring; keys from the loader 0x10037B00) --
namespace {

// 0x1004B070: selected config profile (0=Legit, 1=Rage, 2=HvH).
int g_Profile = 0;
const char *const kProfileItems[3] = {"Legit", "Rage", "HvH"};

const char *ConfigPath() {
    static const char *const paths[3] = {"C:\\L4D2space\\legit.ini",
                                         "C:\\L4D2space\\rage.ini",
                                         "C:\\L4D2space\\hvh.ini"};
    return paths[g_Profile];
}

int ReadInt(const char *key, int fallback) {
    return GetPrivateProfileIntA("Config", key, fallback, ConfigPath());
}

void WriteInt(const char *key, int value) {
    char buffer[16];
    wsprintfA(buffer, "%d", value);
    WritePrivateProfileStringA("Config", key, buffer, ConfigPath());
}

bool g_ConfigLoaded = false;

} // namespace

// 0x1004A99C: selected tab (0=Legit, 1=Rage, 2=Visuals, 3=Misc).
static int g_SelectedTab = 0;

static void LoadSettings();
static void SaveSettings();
static void KeybindButton(int *key, bool *capturing);

static void TabButton(int index, const char *label) {
    bool selected = (g_SelectedTab == index);
    if (selected)
        ImGui::PushStyleColor(ImGuiCol_Button,
                               ImVec4(0.26f, 0.59f, 0.98f, 1.0f));

    if (ImGui::Button(label, ImVec2(140.0f, 30.0f)))
        g_SelectedTab = index;

    if (selected)
        ImGui::PopStyleColor();
}

static const char *const kHitboxItems[2] = {"Auto", "Head Only"};
static bool g_LegitAimKeyCapturing = false;
static bool g_LegitTriggerKeyCapturing = false;

// Target Hook_EndScene Legit tab 0x10009DBA..0x1000A2AC, exact group and
// widget order. The target draws the keybind button inline after Enabled;
// SameLine keeps the menu compact like the original.
static void LegitTab() {
    // Target Hook_EndScene 0x10009DBA: while the Legit tab is drawn with
    // Legit enabled, the Rage aimbot is switched off.
    if (g_LegitEnabled)
        g_RageAimbotEnabled = false;
    ImGui::Text("Aimbot");
    ImGui::Separator();
    ImGui::Checkbox("Enabled##legit", &g_LegitEnabled);
    ImGui::SameLine();
    KeybindButton(&g_LegitAimKey, &g_LegitAimKeyCapturing);
    ImGui::Combo("Hitbox", &g_LegitHitboxMode, kHitboxItems, 2);
    ImGui::Checkbox("Shotgun Mode", &g_LegitShotgunMode);
    ImGui::SliderInt("FOV##legitfov", &g_LegitAimFov, 0, 15);
    ImGui::SliderInt("Smooth Y", &g_LegitSmoothY, 0, 20);
    ImGui::SliderInt("Smooth X", &g_LegitSmoothX, 0, 20);

    ImGui::Text("Accuracy");
    ImGui::Separator();
    ImGui::Checkbox("No-Recoil", &g_LegitNoRecoil);
    ImGui::Checkbox("No-Spread", &g_LegitNoSpread);
    ImGui::Checkbox("Position Adjustment", &g_LegitPositionAdjustment);
    ImGui::Checkbox("FOV Circle", &g_LegitFovCircle);

    ImGui::Text("Target");
    ImGui::Separator();
    ImGui::Checkbox("Priority Special", &g_LegitPrioritySpecial);
    ImGui::Checkbox("Hitscan", &g_LegitHitscan);
    ImGui::Checkbox("Ignore Team", &g_LegitIgnoreTeam);
    ImGui::Checkbox("Ignore Idle Witch", &g_LegitIgnoreWitch);

    ImGui::Text("Triggerbot");
    ImGui::Separator();
    ImGui::Checkbox("Enabled##trigger", &g_LegitTriggerbot);
    ImGui::SameLine();
    KeybindButton(&g_LegitTriggerKey, &g_LegitTriggerKeyCapturing);
    ImGui::SliderInt("Delay", &g_LegitTriggerDelay, 0, 15);
    ImGui::Checkbox("Aim At Head", &g_LegitAimHead);
    ImGui::Checkbox("Aim At Body", &g_LegitAimBody);
    ImGui::Checkbox("Auto Shove", &g_LegitAutoShove);
    ImGui::SliderInt("Shove FOV", &g_LegitShoveFov, 0, 15);
}

static const char *const kSpamModeItems[2] = {"Advertise", "Chat Jammer"};
static const char *const kSmokerModeItems[2] = {"Always", "Reach Smoker"};

// VK-name table copied from the target's off_100421D0 (124 entries).
static const char *const kKeyNames[124] = {
    "",           "Mouse 1",  "Mouse 2",  "Cancel",    "Middle Mouse",
    "Mouse 4",    "Mouse 5",  "",         "Backspace", "Tab",
    "",           "",         "Clear",    "Enter",     "",
    "",           "Shift",    "Control",  "Alt",       "Pause",
    "Caps",       "",         "",         "",          "",
    "",           "",         "Escape",   "",          "",
    "",           "",         "Space",    "Page Up",   "Page Down",
    "End",        "Home",     "Left",     "Up",        "Right",
    "Down",       "",         "",         "",          "Print",
    "Insert",     "Delete",   "",         "0",         "1",
    "2",          "3",        "4",        "5",         "6",
    "7",          "8",        "9",        "",          "",
    "",           "",         "",         "",          "",
    "A",          "B",        "C",        "D",         "E",
    "F",          "G",        "H",        "I",         "J",
    "K",          "L",        "M",        "N",         "O",
    "P",          "Q",        "R",        "S",         "T",
    "U",          "V",        "W",        "X",         "Y",
    "Z",          "",         "",         "",          "",
    "",           "Numpad 0", "Numpad 1", "Numpad 2",  "Numpad 3",
    "Numpad 4",   "Numpad 5", "Numpad 6", "Numpad 7",  "Numpad 8",
    "Numpad 9",   "Multiply", "Add",      "",          "Subtract",
    "Decimal",    "Divide",   "F1",       "F2",        "F3",
    "F4",         "F5",       "F6",       "F7",        "F8",
    "F9",         "F10",      "F11",      "F12",
};

static bool g_LagKeyCapturing = false;
static bool g_ThirdpersonKeyCapturing = false;
static bool g_AirStuckKeyCapturing = false;
static bool g_TeleportKeyCapturing = false;
static bool g_SpeedHackKeyCapturing = false;

static const char *KeyNameFor(int key) {
    if (key < 0 || key >= 124)
        return kKeyNames[0];
    return kKeyNames[key];
}

// sub_10007DD0: 70x18 keybind button. Click toggles capture; while
// capturing, the first held VK 1..123 is stored (ESC stores 0). The target's
// capture-state byte is 0x1004B212.
static void KeybindButton(int *key, bool *capturing) {
    const char *label = *capturing ? "..." : KeyNameFor(*key);
    bool wasCapturing = *capturing;
    if (ImGui::Button(label, ImVec2(70.0f, 18.0f)))
        *capturing = !*capturing;

    if (wasCapturing) {
        for (int vk = 1; vk < 0x7C; ++vk) {
            if ((GetAsyncKeyState(vk) & 0x8000) != 0) {
                *key = (vk == 27) ? 0 : vk;
                *capturing = false;
                break;
            }
        }
    }
}

static bool g_MiscSmac = false;

static const char *const kPitchItems[4] = {"None", "Fake Up", "Fake Down",
                                           "Fake Zero"};
static const char *const kYawItems[4] = {"None", "Left", "Right", "Backwards"};
static const char *const kRageHitboxItems[2] = {"Auto", "Head Only"};
static bool g_RageAimKeyCapturing = false;

// Target Hook_EndScene Rage tab 0x1000A2B1..0x1000A749, exact group order:
// Aimbot, Accuracy, Target, Hack vs Hack. Only the Hack vs Hack (anti-aim)
// block is implemented in Cheat_Update today; the other checkboxes are
// bound to their real globals but the aimbot logic is still PENDING.
static void RageTab() {
    // Target Hook_EndScene 0x1000A2B1: while the Rage tab is drawn with the
    // Rage aimbot enabled, the Legit aimbot is switched off.
    if (g_RageAimbotEnabled)
        g_LegitEnabled = false;
    ImGui::Text("Aimbot");
    ImGui::Separator();
    ImGui::Checkbox("Enabled##rage", &g_RageAimbotEnabled);
    ImGui::SameLine();
    KeybindButton(&g_RageAimKey, &g_RageAimKeyCapturing);
    ImGui::Combo("Hitbox##rage", &g_RageHitbox, kRageHitboxItems, 2);
    ImGui::Checkbox("Auto Shoot", &g_RageAutoShoot);
    ImGui::Checkbox("Silent Aim", &g_RageSilentAim);
    ImGui::Checkbox("Perfect Silent Aim", &g_RagePerfectSilentAim);
    ImGui::Checkbox("Shotgun Mode##rage", &g_RageShotgunMode);
    ImGui::Checkbox("Auto Shove##rage", &g_RageAutoShove);
    ImGui::Checkbox("Auto Melee", &g_RageAutoMelee);
    ImGui::SliderInt("FOV##ragefov", &g_RageAimFov, 0, 180);

    ImGui::Text("Accuracy");
    ImGui::Separator();
    ImGui::Checkbox("No-Recoil##rage", &g_RageNoRecoil);
    ImGui::Checkbox("No-Spread##rage", &g_RageNoSpread);
    ImGui::Checkbox("Position Adjustment##rage", &g_RagePositionAdjustment);
    ImGui::Checkbox("Prediction Fix", &g_RagePredictionFix);

    ImGui::Text("Target");
    ImGui::Separator();
    ImGui::Checkbox("Priority Special##rage", &g_RagePrioritySpecial);
    ImGui::Checkbox("Hitscan##rage", &g_RageHitscan);
    ImGui::Checkbox("Ignore Team##rage", &g_RageIgnoreTeam);
    ImGui::Checkbox("Ignore Idle Witch##rage", &g_RageIgnoreWitch);

    ImGui::Text("Hack vs Hack");
    ImGui::Separator();
    ImGui::Checkbox("Enabled##antiaim", &g_RageEnabled);
    ImGui::Combo("Pitch###Pitchemz", &g_RagePitch, kPitchItems, 4);
    ImGui::Combo("Real Yaw###Yaw", &g_RageRealYaw, kYawItems, 4);
    ImGui::Combo("Desync Yaw###YawFake", &g_RageDesyncYaw, kYawItems, 4);
}

static void VisualsTab() {
    // Recovered buttons (sub_1002F1F0 = Button(label, size)); material mode
    // selection behaviour is PENDING.
    ImGui::Button("Main##chasdjas", ImVec2(120.0f, 20.0f));
    ImGui::SameLine();
    ImGui::Button("Materials##dashkas", ImVec2(120.0f, 20.0f));
    ImGui::Separator();
    // ESP group (EndScene 0x1000A8E2..0x1000A95B). The tick-box labels and
    // order match the target's "Others" visual group.
    ImGui::Checkbox("Enable ESP", &g_EspEnable);
    ImGui::Checkbox("Show Team", &g_EspShowTeam);
    ImGui::Checkbox("Player Name ESP", &g_EspName);
    ImGui::Checkbox("Player Box ESP", &g_EspBox);
    ImGui::Checkbox("Player Distance ESP", &g_EspDistance);
    ImGui::Checkbox("Player Health ESP", &g_EspHealth);
    ImGui::Checkbox("Player HealthBar ESP", &g_EspHealthBar);
    ImGui::Checkbox("Weapons ESP", &g_EspWeapons);
    ImGui::Checkbox("Ammo ESP", &g_EspAmmo);
    ImGui::Checkbox("Witch Rage Bar ESP", &g_EspWitchRage);
    ImGui::Checkbox("Lines ESP", &g_EspLines);
    ImGui::Checkbox("Radar", &g_Radar);
    ImGui::Checkbox("Show names", &g_RadarNames);
    ImGui::Checkbox("Weapons ESP##weapons2", &g_EspWeapons2);
    ImGui::Checkbox("Thirdperson", &g_Thirdperson);
    KeybindButton(&g_ThirdpersonKey, &g_ThirdpersonKeyCapturing);
    ImGui::Separator();
    // Target shows one chams set depending on the material mode; the mode
    // switch itself is PENDING. Player chams logic is still PENDING; Hand and
    // XQZ chams are live.
    ImGui::Checkbox("Player Chams", &g_PlayerChams);
    ImGui::Checkbox("Hand Chams", &g_HandChams);
    ImGui::Checkbox("XQZ Chams", &g_XQZChams);
    ImGui::Separator();
    ImGui::Text("Player Chams Colors");
    ImGui::SliderInt("R##ColorP1", &g_PlayerChamR, 0, 255);
    ImGui::SliderInt("G##ColorP2", &g_PlayerChamG, 0, 255);
    ImGui::SliderInt("B##ColorP3", &g_PlayerChamB, 0, 255);
    ImGui::Separator();
    ImGui::Text("Hand Chams Colors");
    ImGui::SliderInt("R##ColorH1", &g_HandChamR, 0, 255);
    ImGui::SliderInt("G##ColorH2", &g_HandChamG, 0, 255);
    ImGui::SliderInt("B##ColorH3", &g_HandChamB, 0, 255);
    ImGui::Separator();

    // 0x1000AD86..0x1000AE33: exact widget order, labels, and ranges.
    ImGui::Checkbox("FOV Changer", &g_FovChangerEnabled);
    ImGui::Checkbox("Aspect Ratio", &g_AspectRatioEnabled);
    ImGui::Checkbox("No Visual Recoil", &g_NoVisualRecoil);
    ImGui::SliderInt("Player FOV", &g_FovDesired, 90, 120);
    ImGui::SliderInt("Viewmodel FOV", &g_FovViewModel, 51, 130);
    ImGui::SliderInt("Aspect Ratio##ATRO", &g_AspectRatioValue, 0, 3);
}

static void MiscTab() {
    // Target Hook_EndScene 0x1000AEBC..0x1000B0AD widget order:
    //   Bunny Hop, SpeedHack + keybind, Air Stuck + keybind, Teleport +
    //   keybind, Fake Lag, Fake Lag slider 0..16, SpeedHack slider 0..14.
    // The two sliders are intentionally after the Fake Lag checkbox in the
    // target, not next to their own features.
    ImGui::Checkbox("Bunny Hop", &g_BunnyHopEnabled);
    ImGui::Checkbox("SpeedHack", &g_SpeedHack);
    KeybindButton(&g_SpeedHackKey, &g_SpeedHackKeyCapturing);
    ImGui::Checkbox("Air Stuck", &g_AirStuck);
    KeybindButton(&g_AirStuckKey, &g_AirStuckKeyCapturing);
    ImGui::Checkbox("Teleport##jfsdagkfs", &g_Teleport);
    KeybindButton(&g_TeleportKey, &g_TeleportKeyCapturing);
    ImGui::Checkbox("Fake Lag", &g_FakeLag);
    ImGui::SliderInt("Fake Lag##LagValue", &g_FakeLagValue, 0, 16);
    ImGui::SliderInt("SpeedHack##SpeedValue", &g_SpeedHackValue, 0, 14);
    ImGui::Separator();
    ImGui::Checkbox("Chat Spam", &g_ChatSpam);
    ImGui::Combo("Spam Mode###SpamChatzz", &g_ChatSpamMode, kSpamModeItems, 2);
    ImGui::Checkbox("Remove Vomit", &g_RemoveVomit);
    ImGui::Checkbox("Remove Fog", &g_RemoveFog);
    ImGui::Checkbox("Full Bright", &g_FullBright);
    ImGui::Separator();
    ImGui::Checkbox("Invisible Jockey", &g_InvisibleJockey);
    ImGui::Checkbox("Lag Exploit", &g_LagExploit);
    KeybindButton(&g_LagKey, &g_LagKeyCapturing);
    ImGui::Checkbox("Name Stealer", &g_NameStealer);
    ImGui::Checkbox("Server Lagger##xczvcxv", &g_ServerLagger);
    ImGui::SliderInt("Lag Exploit##LagExValue", &g_LagFast, 0, 1000);
    ImGui::Checkbox("No Fall Damage", &g_NoFallDamage);
    ImGui::Separator();
    ImGui::Checkbox("Auto Pistol", &g_AutoPistol);
    ImGui::Checkbox("Instant Weapon Switch", &g_InstantWeaponSwitch);
    ImGui::Checkbox("Instant Revive", &g_InstantRevive);
    ImGui::Checkbox("Anti Smoker", &g_AntiSmoker);
    ImGui::Combo("Smoker Mode", &g_SmokerMode, kSmokerModeItems, 2);
    ImGui::Checkbox("Indicators", &g_Indicators);
    ImGui::Checkbox("SMAC Mode", &g_SmacMode);
    ImGui::Separator();
    ImGui::Combo("Config###Configz", &g_Profile,
                 const_cast<const char *const *>(kProfileItems), 3);
    if (ImGui::Button("load", ImVec2(120.0f, 25.0f)))
        LoadSettings();
    ImGui::SameLine();
    if (ImGui::Button("save", ImVec2(120.0f, 25.0f)))
        SaveSettings();
}

static void LoadSettings() {
    // Legit tab keys from the target descriptor (Menu_Render 0x10037B00).
    // Quirks preserved: Smooth X uses "LEGITSMOOTHYAW", Position Adjustment
    // uses "LegitLagComp", and Smooth Y / Triggerbot Enabled / Delay are not
    // persisted by the target at all. Aim At Head is "trighead".
    g_LegitEnabled = ReadInt("ENABLEAIMLEGIT", 0) != 0;
    g_LegitAimKey = ReadInt("LegitAimKey", 0);
    g_LegitAimHitbox = ReadInt("LegitAimHitbox", 0);
    g_LegitHitboxMode = ReadInt("LegitAimHitboxMode", 0);
    g_LegitShotgunMode = ReadInt("LegitShotgunMode", 0) != 0;
    g_LegitNoRecoil = ReadInt("LEGITNORECOIL", 0) != 0;
    g_LegitNoSpread = ReadInt("LEGITNOSPREAD", 0) != 0;
    g_LegitAimFov = ReadInt("LEGITAIMFOV", 0);
    g_LegitSmoothX = ReadInt("LEGITSMOOTHYAW", 0);
    g_LegitPositionAdjustment = ReadInt("LegitLagComp", 0) != 0;
    g_LegitIgnoreWitch = ReadInt("LegitIGNOREWITCH", 0) != 0;
    g_LegitIgnoreTeam = ReadInt("LegitIgnoreTeam", 0) != 0;
    g_LegitHitscan = ReadInt("LegitbHitscanMode", 0) != 0;
    g_LegitPrioritySpecial = ReadInt("Legitpriorityspecial", 0) != 0;
    g_LegitAutoShove = ReadInt("LegitAutoShove", 0) != 0;
    g_LegitShoveFov = ReadInt("LegitAutoShoveFov", 0);
    g_LegitTriggerKey = ReadInt("TriggKey", 0);
    g_LegitAimHead = ReadInt("trighead", 0) != 0;
    g_LegitAimBody = ReadInt("triggerbody", 0) != 0;
    g_LegitFovCircle = ReadInt("FovVisualize", 0) != 0;
    // Rage tab keys from the target descriptor (Menu_Render 0x10037B00).
    // Quirk: the Rage "Enabled" flag is stored under the group-label string
    // "Aimbot"; Hitbox (AB6C) has no descriptor entry and is not persisted
    // (the descriptor's unused "AimHitbox" maps to dword_1004AB68).
    g_RageAimbotEnabled = ReadInt("Aimbot", 0) != 0;
    g_RageAimKey = ReadInt("AimKey", 0);
    g_RageAimFov = ReadInt("AimFov", 0);
    g_RageAimHitboxResolved = ReadInt("AimHitbox", 0);
    g_RageSmooth = ReadInt("Smooth", 0);
    g_RageAutoShoot = ReadInt("AutoFire", 0) != 0;
    g_RageSilentAim = ReadInt("Silent", 0) != 0;
    g_RagePerfectSilentAim = ReadInt("PSilent", 0) != 0;
    g_RageShotgunMode = ReadInt("ShotgunMode", 0) != 0;
    g_RageAutoShove = ReadInt("AutoShove", 0) != 0;
    g_RageAutoMelee = ReadInt("AutoMelee", 0) != 0;
    g_RageNoRecoil = ReadInt("NoRecoil", 0) != 0;
    g_RageNoSpread = ReadInt("NoSpread", 0) != 0;
    g_RagePositionAdjustment = ReadInt("LagComp", 0) != 0;
    g_RagePredictionFix = ReadInt("PredictionFix", 0) != 0;
    g_RagePrioritySpecial = ReadInt("priorityspecial", 0) != 0;
    g_RageHitscan = ReadInt("bHitscanMode", 0) != 0;
    g_RageIgnoreTeam = ReadInt("IgnoreTeam", 0) != 0;
    g_RageIgnoreWitch = ReadInt("IgnoreWitch", 0) != 0;
    g_RageEnabled = ReadInt("AntiAim", 0) != 0;
    g_RagePitch = ReadInt("Pitch", 0);
    g_RageRealYaw = ReadInt("Yaw", 0);
    g_RageDesyncYaw = ReadInt("FakeYaw", 0);

    // Keys and order recovered from the config section of 0x10037B00.
    g_NoVisualRecoil = ReadInt("NoVisRecoil", 0) != 0;
    g_AspectRatioEnabled = ReadInt("AspectRatio", 0) != 0;
    g_AspectRatioValue = ReadInt("AspectRatioValue", g_AspectRatioValue);
    g_FovChangerEnabled = ReadInt("FovChanger", 0) != 0;
    g_FovDesired = ReadInt("FovChangerValue", g_FovDesired);
    g_FovViewModel = ReadInt("ViewFovChangerValue", g_FovViewModel);
    g_PlayerChams = ReadInt("EspChams", 0) != 0;
    g_PlayerChamR = ReadInt("chamscolor", g_PlayerChamR);
    g_PlayerChamG = ReadInt("chamscolor1", g_PlayerChamG);
    g_PlayerChamB = ReadInt("chamscolor2", g_PlayerChamB);
    g_HandChams = ReadInt("ChamsHands", 0) != 0;
    g_XQZChams = ReadInt("CHAMSXQZ", 0) != 0;
    g_WeaponChams = ReadInt("wpnchams", 0) != 0;
    g_HandWeaponChams = ReadInt("Handwpnchams", 0) != 0;
    g_SurvivorChams = ReadInt("ChamsSURVIVOR", 0) != 0;
    g_InfectedChams = ReadInt("ChamsINFECTED", 0) != 0;
    // ESP keys present in the target's config string block 0x457A0..0x45818.
    // No key exists for Player Distance ESP or Lines ESP; the target does not
    // persist them (verified by the absence of matching strings).
    g_EspEnable = ReadInt("EspToggle", 0) != 0;
    g_EspShowTeam = ReadInt("EspShowTeam", 0) != 0;
    g_EspBotTag = ReadInt("EspDanger", 0) != 0;
    g_EspName = ReadInt("EspName", 0) != 0;
    g_EspBox = ReadInt("EspBox", 0) != 0;
    g_EspHealth = ReadInt("EspHealth", 0) != 0;
    g_EspHealthBar = ReadInt("ESPHEALTH2", 0) != 0;
    g_EspWeapons = ReadInt("ESPWEAPON", 0) != 0;
    g_EspWeapons2 = ReadInt("ESPWEAPONSPAWN", 0) != 0;
    g_EspAmmo = ReadInt("ESPWEAPONSAMMO", 0) != 0;
    g_EspWitchRage = ReadInt("ESPWITCHRAGEBAR", 0) != 0;
    g_Radar = ReadInt("radar", 0) != 0;
    g_RadarNames = ReadInt("radarnames", 0) != 0;
    g_Indicators = ReadInt("indicators", 0) != 0;
    g_HandChamR = ReadInt("handchamscolor", g_HandChamR);
    g_HandChamG = ReadInt("handchamscolor1", g_HandChamG);
    g_HandChamB = ReadInt("handchamscolor2", g_HandChamB);
    g_RemoveVomit = ReadInt("NoVomit", 0) != 0;
    g_AntiSmoker = ReadInt("antismoker", 0) != 0;
    g_NoFallDamage = ReadInt("NoFallDamage", 0) != 0;
    g_AutoPistol = ReadInt("AutoPistol", 0) != 0;
    g_ServerLagger = ReadInt("serverlag", 0) != 0;
    g_BunnyHopEnabled = ReadInt("AUTOBH", 0) != 0;
    g_AutoStrafer = ReadInt("AutoStrafer", 0) != 0;
    g_ChatSpam = ReadInt("Chatspam", 0) != 0;
    g_ChatSpamMode = ReadInt("ChatspamMode", g_ChatSpamMode);
    g_LagExploit = ReadInt("LAGEXPLOIT", 0) != 0;
    g_LagFast = ReadInt("LagFast", g_LagFast);
    g_LagKey = ReadInt("LagKey", g_LagKey);
    g_ThirdpersonKey = ReadInt("ThirdpersonKey", 0);
    // Target settings descriptor (Menu_Render 0x10037B00, built once from
    // sub_10001050 and consumed by SaveSettings 0x10039AD0 / LoadSettings
    // 0x10039F20) includes these Misc keys.
    g_TeleportKey = ReadInt("TeleportKey", 0);
    g_AirStuckKey = ReadInt("AirstuckKey", 0);
    g_Teleport = ReadInt("Teleport", 0) != 0;
    g_AirStuck = ReadInt("Airstuck", 0) != 0;
    g_SpeedHack = ReadInt("SpeedHack", 0) != 0;
    g_SpeedHackValue = ReadInt("SpeedhackVal", 0);
    g_SpeedHackKey = ReadInt("SpeedhackKey", 115);
    g_FakeLag = ReadInt("FakeLag", 0) != 0;
    g_FakeLagValue = ReadInt("FakeLagValue", 14);
}

static void SaveSettings() {
    CreateDirectoryA("C:\\L4D2space", nullptr);
    WriteInt("ENABLEAIMLEGIT", g_LegitEnabled ? 1 : 0);
    WriteInt("LegitAimKey", g_LegitAimKey);
    WriteInt("LegitAimHitbox", g_LegitAimHitbox);
    WriteInt("LegitAimHitboxMode", g_LegitHitboxMode);
    WriteInt("LegitShotgunMode", g_LegitShotgunMode ? 1 : 0);
    WriteInt("LEGITNORECOIL", g_LegitNoRecoil ? 1 : 0);
    WriteInt("LEGITNOSPREAD", g_LegitNoSpread ? 1 : 0);
    WriteInt("LEGITAIMFOV", g_LegitAimFov);
    WriteInt("LEGITSMOOTHYAW", g_LegitSmoothX);
    WriteInt("LegitLagComp", g_LegitPositionAdjustment ? 1 : 0);
    WriteInt("LegitIGNOREWITCH", g_LegitIgnoreWitch ? 1 : 0);
    WriteInt("LegitIgnoreTeam", g_LegitIgnoreTeam ? 1 : 0);
    WriteInt("LegitbHitscanMode", g_LegitHitscan ? 1 : 0);
    WriteInt("Legitpriorityspecial", g_LegitPrioritySpecial ? 1 : 0);
    WriteInt("LegitAutoShove", g_LegitAutoShove ? 1 : 0);
    WriteInt("LegitAutoShoveFov", g_LegitShoveFov);
    WriteInt("TriggKey", g_LegitTriggerKey);
    WriteInt("trighead", g_LegitAimHead ? 1 : 0);
    WriteInt("triggerbody", g_LegitAimBody ? 1 : 0);
    WriteInt("FovVisualize", g_LegitFovCircle ? 1 : 0);
    WriteInt("Aimbot", g_RageAimbotEnabled ? 1 : 0);
    WriteInt("AimKey", g_RageAimKey);
    WriteInt("AimFov", g_RageAimFov);
    WriteInt("AimHitbox", g_RageAimHitboxResolved);
    WriteInt("Smooth", g_RageSmooth);
    WriteInt("AutoFire", g_RageAutoShoot ? 1 : 0);
    WriteInt("Silent", g_RageSilentAim ? 1 : 0);
    WriteInt("PSilent", g_RagePerfectSilentAim ? 1 : 0);
    WriteInt("ShotgunMode", g_RageShotgunMode ? 1 : 0);
    WriteInt("AutoShove", g_RageAutoShove ? 1 : 0);
    WriteInt("AutoMelee", g_RageAutoMelee ? 1 : 0);
    WriteInt("NoRecoil", g_RageNoRecoil ? 1 : 0);
    WriteInt("NoSpread", g_RageNoSpread ? 1 : 0);
    WriteInt("LagComp", g_RagePositionAdjustment ? 1 : 0);
    WriteInt("PredictionFix", g_RagePredictionFix ? 1 : 0);
    WriteInt("priorityspecial", g_RagePrioritySpecial ? 1 : 0);
    WriteInt("bHitscanMode", g_RageHitscan ? 1 : 0);
    WriteInt("IgnoreTeam", g_RageIgnoreTeam ? 1 : 0);
    WriteInt("IgnoreWitch", g_RageIgnoreWitch ? 1 : 0);
    WriteInt("AntiAim", g_RageEnabled ? 1 : 0);
    WriteInt("Pitch", g_RagePitch);
    WriteInt("Yaw", g_RageRealYaw);
    WriteInt("FakeYaw", g_RageDesyncYaw);

    WriteInt("NoVisRecoil", g_NoVisualRecoil ? 1 : 0);
    WriteInt("AspectRatio", g_AspectRatioEnabled ? 1 : 0);
    WriteInt("AspectRatioValue", g_AspectRatioValue);
    WriteInt("FovChanger", g_FovChangerEnabled ? 1 : 0);
    WriteInt("FovChangerValue", g_FovDesired);
    WriteInt("ViewFovChangerValue", g_FovViewModel);
    WriteInt("EspChams", g_PlayerChams ? 1 : 0);
    WriteInt("chamscolor", g_PlayerChamR);
    WriteInt("chamscolor1", g_PlayerChamG);
    WriteInt("chamscolor2", g_PlayerChamB);
    WriteInt("ChamsHands", g_HandChams ? 1 : 0);
    WriteInt("CHAMSXQZ", g_XQZChams ? 1 : 0);
    WriteInt("wpnchams", g_WeaponChams ? 1 : 0);
    WriteInt("Handwpnchams", g_HandWeaponChams ? 1 : 0);
    WriteInt("ChamsSURVIVOR", g_SurvivorChams ? 1 : 0);
    WriteInt("ChamsINFECTED", g_InfectedChams ? 1 : 0);
    WriteInt("EspToggle", g_EspEnable ? 1 : 0);
    WriteInt("EspShowTeam", g_EspShowTeam ? 1 : 0);
    WriteInt("EspDanger", g_EspBotTag ? 1 : 0);
    WriteInt("EspName", g_EspName ? 1 : 0);
    WriteInt("EspBox", g_EspBox ? 1 : 0);
    WriteInt("EspHealth", g_EspHealth ? 1 : 0);
    WriteInt("ESPHEALTH2", g_EspHealthBar ? 1 : 0);
    WriteInt("ESPWEAPON", g_EspWeapons ? 1 : 0);
    WriteInt("ESPWEAPONSPAWN", g_EspWeapons2 ? 1 : 0);
    WriteInt("ESPWEAPONSAMMO", g_EspAmmo ? 1 : 0);
    WriteInt("ESPWITCHRAGEBAR", g_EspWitchRage ? 1 : 0);
    WriteInt("radar", g_Radar ? 1 : 0);
    WriteInt("radarnames", g_RadarNames ? 1 : 0);
    WriteInt("indicators", g_Indicators ? 1 : 0);
    WriteInt("handchamscolor", g_HandChamR);
    WriteInt("handchamscolor1", g_HandChamG);
    WriteInt("handchamscolor2", g_HandChamB);
    WriteInt("NoVomit", g_RemoveVomit ? 1 : 0);
    WriteInt("antismoker", g_AntiSmoker ? 1 : 0);
    WriteInt("NoFallDamage", g_NoFallDamage ? 1 : 0);
    WriteInt("AutoPistol", g_AutoPistol ? 1 : 0);
    WriteInt("serverlag", g_ServerLagger ? 1 : 0);
    WriteInt("AUTOBH", g_BunnyHopEnabled ? 1 : 0);
    WriteInt("AutoStrafer", g_AutoStrafer ? 1 : 0);
    WriteInt("Chatspam", g_ChatSpam ? 1 : 0);
    WriteInt("ChatspamMode", g_ChatSpamMode);
    WriteInt("LAGEXPLOIT", g_LagExploit ? 1 : 0);
    WriteInt("LagFast", g_LagFast);
    WriteInt("LagKey", g_LagKey);
    WriteInt("ThirdpersonKey", g_ThirdpersonKey);
    // The target descriptor feeds both save and load, so the Misc checkboxes
    // and sliders are persisted under these exact keys.
    WriteInt("TeleportKey", g_TeleportKey);
    WriteInt("AirstuckKey", g_AirStuckKey);
    WriteInt("Teleport", g_Teleport ? 1 : 0);
    WriteInt("Airstuck", g_AirStuck ? 1 : 0);
    WriteInt("SpeedHack", g_SpeedHack ? 1 : 0);
    WriteInt("SpeedhackVal", g_SpeedHackValue);
    WriteInt("SpeedhackKey", g_SpeedHackKey);
    WriteInt("FakeLag", g_FakeLag ? 1 : 0);
    WriteInt("FakeLagValue", g_FakeLagValue);
}

void Menu_Render() {
    if (!g_ConfigLoaded) {
        LoadSettings();
        g_ConfigLoaded = true;
    }

    // INFERRED size: the target sets/ships a fixed menu size; our earlier
    // shell-only run persisted Size=600,69 in imgui.ini, which clips the tab
    // content because the window is NoResize. Force a size that fits the
    // recovered Legit tree until the original size data is recovered.
    ImGui::SetNextWindowSize(ImVec2(600.0f, 650.0f), ImGuiCond_Always);

    if (!ImGui::Begin("L4D2 bot by space785", nullptr,
                      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
                          ImGuiWindowFlags_NoCollapse |
                          ImGuiWindowFlags_NoBringToFrontOnFocus))
        return;

    ImGui::BeginGroup();
    TabButton(0, "Legit");
    ImGui::SameLine();
    TabButton(1, "Rage");
    ImGui::SameLine();
    TabButton(2, "Visuals");
    ImGui::SameLine();
    TabButton(3, "Misc");
    ImGui::EndGroup();
    ImGui::Separator();

    switch (g_SelectedTab) {
    case 0:
        LegitTab();
        break;
    case 1:
        RageTab();
        break;
    case 2:
        VisualsTab();
        break;
    case 3:
        MiscTab();
        break;
    default:
        break;
    }

    ImGui::End();
}

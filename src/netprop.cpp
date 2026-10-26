#include "netprop.h"
#include "interfaces.h"

#include <cstring>
#include <vector>

namespace {
std::vector<RecvTable *> g_NetpropTables;

RecvTable *FindTableByName(const char *tableName) {
    if (!tableName)
        return nullptr;
    for (RecvTable *table : g_NetpropTables) {
        if (!table || IsBadReadPtr(table, 16))
            continue;
        const char *name = *reinterpret_cast<const char **>(
            reinterpret_cast<char *>(table) + 12);
        if (!name || IsBadReadPtr(name, 1))
            continue;
        if (_stricmp(name, tableName) == 0)
            return table;
    }
    return nullptr;
}
} // namespace

void BuildNetpropTables() {
    g_NetpropTables.clear();
    if (!g_pClient)
        return;

    typedef void *(__thiscall * GetAllClassesFn)(void *);
    void **vtable = *reinterpret_cast<void ***>(g_pClient);
    if (IsBadReadPtr(vtable, sizeof(void *) * 8))
        return;
    void *node = reinterpret_cast<GetAllClassesFn>(vtable[7])(g_pClient);

    for (int guard = 0; node && guard < 512; ++guard) {
        if (IsBadReadPtr(node, 20))
            break;
        RecvTable *table = *reinterpret_cast<RecvTable **>(
            static_cast<char *>(node) + 12);
        if (table && !IsBadReadPtr(table, sizeof(RecvTable)))
            g_NetpropTables.push_back(table);
        node = *reinterpret_cast<void **>(static_cast<char *>(node) + 16);
    }
}

int NetpropTableCount() {
    return static_cast<int>(g_NetpropTables.size());
}

// 0x100374A0. RecvProp stride is 60 bytes (0x3C); nested table offsets add.
int NetpropOffset(RecvTable *table, const char *name) {
    int accumulated = 0;
    if (!table || IsBadReadPtr(table, sizeof(RecvTable)) ||
        table->m_nProps <= 0 || table->m_nProps > 4096)
        return 0;

    for (int i = 0; i < table->m_nProps; ++i) {
        RecvProp *prop = reinterpret_cast<RecvProp *>(
            reinterpret_cast<char *>(table->m_pProps) + i * 60);

        if (IsBadReadPtr(prop, sizeof(RecvProp)))
            return accumulated;

        if (prop->m_pDataTable && !IsBadReadPtr(prop->m_pDataTable, 8) &&
            prop->m_pDataTable->m_nProps > 0) {
            int nested = NetpropOffset(prop->m_pDataTable, name);
            if (nested)
                accumulated += nested + prop->m_Offset;
        }

        if (!_stricmp(prop->m_pVarName, name))
            return accumulated + prop->m_Offset;
    }
    return accumulated;
}

int FindNetpropOffset(const char *name) {
    for (RecvTable *table : g_NetpropTables) {
        int offset = NetpropOffset(table, name);
        if (offset)
            return offset;
    }
    return 0;
}

// RecvTable::m_pNetTableName lives at +12. The target's per-feature helpers
// (sub_100015A0 / sub_100016A0 / the m_flFallVelocity cache) search for a
// specific table by name before resolving the property.
int NetpropOffsetInTable(const char *tableName, const char *propName) {
    RecvTable *table = FindTableByName(tableName);
    return table ? NetpropOffset(table, propName) : 0;
}

void *GetLocalPlayerEntity() {
    if (!g_pEngineClient || !g_pClientEntityList)
        return nullptr;

    typedef int(__thiscall * GetLocalPlayerFn)(void *);
    void **engineVt = *reinterpret_cast<void ***>(g_pEngineClient);
    int index = reinterpret_cast<GetLocalPlayerFn>(engineVt[12])(g_pEngineClient);

    typedef void *(__thiscall * GetClientEntityFn)(void *, int);
    void **listVt = *reinterpret_cast<void ***>(g_pClientEntityList);
    return reinterpret_cast<GetClientEntityFn>(listVt[3])(g_pClientEntityList,
                                                         index);
}

void *GetClientEntityByIndex(int index) {
    if (!g_pClientEntityList ||
        IsBadReadPtr(g_pClientEntityList, sizeof(void *)))
        return nullptr;
    void **listVt = *reinterpret_cast<void ***>(g_pClientEntityList);
    if (!listVt || IsBadReadPtr(listVt, sizeof(void *) * 4))
        return nullptr;
    typedef void *(__thiscall * GetClientEntityFn)(void *, int);
    return reinterpret_cast<GetClientEntityFn>(listVt[3])(g_pClientEntityList,
                                                         index);
}

// IClientEntityList slot 4: GetClientEntityFromHandle (CBaseHandle, 32-bit).
// Used by the ESP painter to resolve m_hActiveWeapon (target uses vtable
// offset 0x10 at 0x1003c56a).
void *GetClientEntityFromHandle(int handle) {
    if (!g_pClientEntityList ||
        IsBadReadPtr(g_pClientEntityList, sizeof(void *)))
        return nullptr;
    void **listVt = *reinterpret_cast<void ***>(g_pClientEntityList);
    if (!listVt || IsBadReadPtr(listVt, sizeof(void *) * 5))
        return nullptr;
    typedef void *(__thiscall * GetFromHandleFn)(void *, int);
    return reinterpret_cast<GetFromHandleFn>(listVt[4])(g_pClientEntityList,
                                                        handle);
}

// Target sub_100053D0 uses entity-list vtable +0x20 (slot 8). Sanity-cap the
// result so a bad slot cannot produce an unbounded loop.
int GetHighestEntityIndex() {
    if (!g_pClientEntityList ||
        IsBadReadPtr(g_pClientEntityList, sizeof(void *)))
        return 0;
    void **listVt = *reinterpret_cast<void ***>(g_pClientEntityList);
    if (!listVt || IsBadReadPtr(listVt, sizeof(void *) * 9))
        return 0;
    typedef int(__thiscall * GetHighestEntityIndexFn)(void *);
    int highest =
        reinterpret_cast<GetHighestEntityIndexFn>(listVt[8])(g_pClientEntityList);
    if (highest < 1 || highest > 4096)
        return 0;
    return highest;
}

// 0x10001DB0: entity category from the networkable's client class ID.
// 1/2/4 = special cases, 3 = listed IDs, 5 = player classes (232/275).
int GetEntityCategory(void *entity) {
    if (!entity || IsBadReadPtr(entity, 0x10))
        return 0;
    // Target sub_10001DB0: the dword at entity+8 is the vtable pointer and
    // the virtual is called with this = entity+8.
    char *object = static_cast<char *>(entity) + 8;
    void **vtable = *reinterpret_cast<void ***>(object);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 2))
        return 0;
    typedef void *(__thiscall * GetClientClassFn)(void *);
    void *clientClass =
        reinterpret_cast<GetClientClassFn>(vtable[1])(object);
    if (!clientClass ||
        IsBadReadPtr(static_cast<char *>(clientClass) + 20, sizeof(int)))
        return 0;
    int classId = *reinterpret_cast<int *>(static_cast<char *>(clientClass) + 20);

    switch (classId) {
    case 264:
        return 4;
    case 277:
        return 2;
    case 276:
        return 1;
    default:
        break;
    }

    static const int kCategory3[] = {0, 99, 270, 263, 265, 272};
    for (int i = 0; i < 6; ++i) {
        if (kCategory3[i] == classId)
            return 3;
    }
    static const int kCategory5[] = {232, 275};
    for (int i = 0; i < 2; ++i) {
        if (kCategory5[i] == classId)
            return 5;
    }
    return 0;
}

// ClientClass::m_ClassID at +20 (sub_10001DB0 / sub_1003B820 use the raw ID).
int GetEntityClassId(void *entity) {
    if (!entity || IsBadReadPtr(entity, 0x10))
        return -1;
    char *object = static_cast<char *>(entity) + 8;
    void **vtable = *reinterpret_cast<void ***>(object);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 2))
        return -1;
    typedef void *(__thiscall * GetClientClassFn)(void *);
    void *clientClass =
        reinterpret_cast<GetClientClassFn>(vtable[1])(object);
    if (!clientClass ||
        IsBadReadPtr(static_cast<char *>(clientClass) + 20, sizeof(int)))
        return -1;
    return *reinterpret_cast<int *>(static_cast<char *>(clientClass) + 20);
}

int GetEntityFlags(void *entity) {
    static int offset = -1;
    if (offset < 0)
        offset = FindNetpropOffset("m_fFlags");
    if (!entity || offset <= 0 || offset > 0x4000)
        return 0;
    if (IsBadReadPtr(static_cast<char *>(entity) + offset, sizeof(int)))
        return 0;
    return *reinterpret_cast<int *>(static_cast<char *>(entity) + offset);
}

// sub_100016A0: DT_BaseEntity::m_iTeamNum.
int GetEntityTeam(void *entity) {
    static int offset = -1;
    if (offset < 0)
        offset = NetpropOffsetInTable("DT_BaseEntity", "m_iTeamNum");
    if (!entity || offset <= 0 || offset > 0x4000)
        return 0;
    if (IsBadReadPtr(static_cast<char *>(entity) + offset, sizeof(int)))
        return 0;
    return *reinterpret_cast<int *>(static_cast<char *>(entity) + offset);
}

// sub_10001EC0 prerequisites.
int GetEntitySequence(void *entity) {
    static int offset = -1;
    if (offset < 0)
        offset = NetpropOffsetInTable("DT_BaseAnimating", "m_nSequence");
    if (!entity || offset <= 0 || offset > 0x4000)
        return 0;
    if (IsBadReadPtr(static_cast<char *>(entity) + offset, sizeof(int)))
        return 0;
    return *reinterpret_cast<int *>(static_cast<char *>(entity) + offset);
}

unsigned short GetEntitySolidFlags(void *entity) {
    static int offset = -1;
    if (offset < 0)
        offset = NetpropOffsetInTable("DT_BaseEntity", "m_usSolidFlags");
    if (!entity || offset <= 0 || offset > 0x4000)
        return 0;
    if (IsBadReadPtr(static_cast<char *>(entity) + offset,
                     sizeof(unsigned short)))
        return 0;
    return *reinterpret_cast<unsigned short *>(static_cast<char *>(entity) +
                                               offset);
}

// 0x10001EC0: animation/solid-state eligibility used by Player Chams.
bool IsPlayerChamsEligible(void *entity) {
    if (!entity || IsBadReadPtr(entity, 0x10))
        return false;

    // Target sub_10001EC0: entity+8 is the vtable pointer and the virtual at
    // slot 7 is called with this = entity+8.
    char *object = static_cast<char *>(entity) + 8;
    void **vtable = *reinterpret_cast<void ***>(object);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 8))
        return false;

    typedef unsigned char(__thiscall *StateFn)(void *);
    if (reinterpret_cast<StateFn>(vtable[7])(object))
        return false;

    int team = GetEntityTeam(entity);
    if (team != 2 && team != 3)
        return false;

    int category = GetEntityCategory(entity);
    if (!category)
        return false;

    int sequence = GetEntitySequence(entity);
    unsigned short solid = GetEntitySolidFlags(entity);

    switch (category) {
    case 1:
    case 2:
        return (solid & 4) == 0 && sequence <= 0x46;
    case 3:
        return (solid & 4) == 0;
    case 4:
        if ((solid & 4) != 0 || sequence > 0x131)
            return false;
        return solid <= 0x14;
    default:
        return true;
    }
}

// Cheat_Update 0x10006469: DT_TerrorPlayer::m_isHangingFromTongue.
int GetEntityHangingFromTongue(void *entity) {
    static int offset = -1;
    if (offset < 0)
        offset = NetpropOffsetInTable("DT_TerrorPlayer",
                                      "m_isHangingFromTongue");
    if (!entity || offset <= 0 || offset > 0x4000)
        return 0;
    if (IsBadReadPtr(static_cast<char *>(entity) + offset, sizeof(int)))
        return 0;
    return *reinterpret_cast<int *>(static_cast<char *>(entity) + offset);
}

// Cheat_Update 0x100062EB: DT_BasePlayer::m_flFallVelocity.
float GetEntityFallVelocity(void *entity) {
    static int offset = -1;
    if (offset < 0)
        offset = NetpropOffsetInTable("DT_BasePlayer", "m_flFallVelocity");
    if (!entity || offset <= 0 || offset > 0x4000)
        return 0.0f;
    if (IsBadReadPtr(static_cast<char *>(entity) + offset, sizeof(float)))
        return 0.0f;
    return *reinterpret_cast<float *>(static_cast<char *>(entity) + offset);
}

// sub_100018B0: DT_BasePlayer::m_vecPunchAngle (QAngle).
void GetEntityPunchAngle(void *entity, float out[3]) {
    static int offset = -1;
    if (offset < 0)
        offset = NetpropOffsetInTable("DT_BasePlayer", "m_vecPunchAngle");
    if (!entity || offset <= 0 || offset > 0x4000 ||
        IsBadReadPtr(static_cast<char *>(entity) + offset, sizeof(float) * 3)) {
        out[0] = out[1] = out[2] = 0.0f;
        return;
    }
    memcpy(out, static_cast<char *>(entity) + offset, sizeof(float) * 3);
}

int GetEntityLifeState(void *entity) {
    static int offset = -1;
    if (offset < 0)
        offset = NetpropOffsetInTable("DT_BasePlayer", "m_lifeState");
    if (!entity || offset <= 0 || offset > 0x4000)
        return 1;
    // Target sub_100045A0 reads a single byte (`v1[offset]`), not a dword.
    if (IsBadReadPtr(static_cast<char *>(entity) + offset, 1))
        return 1;
    return *reinterpret_cast<unsigned char *>(static_cast<char *>(entity) +
                                              offset);
}

int GetEntityHealth(void *entity) {
    static int offset = -1;
    if (offset < 0)
        offset = NetpropOffsetInTable("DT_BasePlayer", "m_iHealth");
    if (!entity || offset <= 0 || offset > 0x4000)
        return 0;
    if (IsBadReadPtr(static_cast<char *>(entity) + offset, sizeof(int)))
        return 0;
    return *reinterpret_cast<int *>(static_cast<char *>(entity) + offset);
}

void GetEntityVelocity(void *entity, float out[3]) {
    static int offset = -1;
    if (offset < 0)
        offset = FindNetpropOffset("m_vecVelocity[0]");
    if (!entity || offset <= 0 || offset > 0x4000 ||
        IsBadReadPtr(static_cast<char *>(entity) + offset, sizeof(float) * 3)) {
        out[0] = out[1] = out[2] = 0.0f;
        return;
    }
    memcpy(out, static_cast<char *>(entity) + offset, sizeof(float) * 3);
}

// --- ESP helpers -----------------------------------------------------------

// Entity+8 vtable slot 7 (IClientNetworkable::IsDormant). Same layout proven
// for sub_10001EC0: entity+8 is the vtable pointer and `this` is entity+8.
bool IsEntityDormant(void *entity) {
    if (!entity || IsBadReadPtr(entity, 0x10))
        return true;
    char *object = static_cast<char *>(entity) + 8;
    void **vtable = *reinterpret_cast<void ***>(object);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 8))
        return true;
    typedef unsigned char(__thiscall * IsDormantFn)(void *);
    return reinterpret_cast<IsDormantFn>(vtable[7])(object) != 0;
}

// Entity+4 vtable slot 8; the painter skips entities whose result is 0
// (0x1003c4dc). INFERRED: renderable/model handle, exact virtual name unknown.
int GetEntityRenderableModelId(void *entity) {
    if (!entity || IsBadReadPtr(entity, 0x10))
        return 0;
    char *object = static_cast<char *>(entity) + 4;
    void **vtable = *reinterpret_cast<void ***>(object);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 9))
        return 0;
    typedef int(__thiscall * ModelFn)(void *);
    return reinterpret_cast<ModelFn>(vtable[8])(object);
}

// sub_100014B0/sub_1003B030: entity->GetAbsOrigin(), vtable at entity+0 slot
// 11 (offset 44).
void GetEntityOrigin(void *entity, float out[3]) {
    out[0] = out[1] = out[2] = 0.0f;
    if (!entity || IsBadReadPtr(entity, sizeof(void *)))
        return;
    void **vtable = *reinterpret_cast<void ***>(entity);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 12))
        return;
    typedef float *(__thiscall * GetAbsOriginFn)(void *);
    float *origin =
        reinterpret_cast<GetAbsOriginFn>(vtable[11])(entity);
    if (!origin || IsBadReadPtr(origin, sizeof(float) * 3))
        return;
    memcpy(out, origin, sizeof(float) * 3);
}

// sub_100014B0: GetAbsOrigin + DT_BasePlayer::m_vecViewOffset.
void GetEntityEyePosition(void *entity, float out[3]) {
    GetEntityOrigin(entity, out);
    static int offset = -1;
    if (offset < 0)
        offset = NetpropOffsetInTable("DT_BasePlayer", "m_vecViewOffset[0]");
    if (!entity || offset <= 0 || offset > 0x4000 ||
        IsBadReadPtr(static_cast<char *>(entity) + offset, sizeof(float) * 3))
        return;
    const float *viewOffset =
        reinterpret_cast<const float *>(static_cast<char *>(entity) + offset);
    out[0] += viewOffset[0];
    out[1] += viewOffset[1];
    out[2] += viewOffset[2];
}

void GetEntityMins(void *entity, float out[3]) {
    out[0] = out[1] = out[2] = 0.0f;
    static int offset = -1;
    if (offset < 0)
        offset = NetpropOffsetInTable("DT_BaseEntity", "m_vecMins");
    if (!entity || offset <= 0 || offset > 0x4000 ||
        IsBadReadPtr(static_cast<char *>(entity) + offset, sizeof(float) * 3))
        return;
    memcpy(out, static_cast<char *>(entity) + offset, sizeof(float) * 3);
}

void GetEntityMaxs(void *entity, float out[3]) {
    out[0] = out[1] = out[2] = 0.0f;
    static int offset = -1;
    if (offset < 0)
        offset = NetpropOffsetInTable("DT_BaseEntity", "m_vecMaxs");
    if (!entity || offset <= 0 || offset > 0x4000 ||
        IsBadReadPtr(static_cast<char *>(entity) + offset, sizeof(float) * 3))
        return;
    memcpy(out, static_cast<char *>(entity) + offset, sizeof(float) * 3);
}

// sub_100013B0: DT_TerrorPlayer::m_iMaxHealth.
int GetEntityMaxHealth(void *entity) {
    static int offset = -1;
    if (offset < 0)
        offset = NetpropOffsetInTable("DT_TerrorPlayer", "m_iMaxHealth");
    if (!entity || offset <= 0 || offset > 0x4000)
        return 0;
    if (IsBadReadPtr(static_cast<char *>(entity) + offset, sizeof(int)))
        return 0;
    return *reinterpret_cast<int *>(static_cast<char *>(entity) + offset);
}

// sub_10001CA0: DT_Witch::m_rage.
float GetEntityRage(void *entity) {
    static int offset = -1;
    if (offset < 0)
        offset = NetpropOffsetInTable("DT_Witch", "m_rage");
    if (!entity || offset <= 0 || offset > 0x4000)
        return 0.0f;
    if (IsBadReadPtr(static_cast<char *>(entity) + offset, sizeof(float)))
        return 0.0f;
    return *reinterpret_cast<float *>(static_cast<char *>(entity) + offset);
}

// sub_10037440("DT_Infected","m_mobRush"): zombie rush flag.
int GetEntityMobRush(void *entity) {
    static int offset = -1;
    if (offset < 0)
        offset = NetpropOffsetInTable("DT_Infected", "m_mobRush");
    if (!entity || offset <= 0 || offset > 0x4000)
        return 0;
    if (IsBadReadPtr(static_cast<char *>(entity) + offset, sizeof(int)))
        return 0;
    return *reinterpret_cast<int *>(static_cast<char *>(entity) + offset);
}

// sub_1003AB50: weapon entity vtable slot 383 (offset 1532) = weapon ID.
int GetEntityWeaponId(void *entity) {
    if (!entity || IsBadReadPtr(entity, sizeof(void *)))
        return -1;
    void **vtable = *reinterpret_cast<void ***>(entity);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 384))
        return -1;
    typedef int(__thiscall * GetWeaponIdFn)(void *);
    return reinterpret_cast<GetWeaponIdFn>(vtable[383])(entity);
}

// sub_10001AA0 + IClientEntityList::GetClientEntityFromHandle: the local
// player's DT_BaseCombatCharacter::m_hActiveWeapon entity.
void *GetLocalActiveWeapon(void *local) {
    static int offset = -1;
    if (offset < 0)
        offset = NetpropOffsetInTable("DT_BaseCombatCharacter",
                                      "m_hActiveWeapon");
    if (!local || offset <= 0x100 || offset >= 0x4000)
        return nullptr;
    if (IsBadReadPtr(static_cast<char *>(local) + offset, sizeof(int)))
        return nullptr;
    int handle = *reinterpret_cast<int *>(static_cast<char *>(local) + offset);
    return GetClientEntityFromHandle(handle);
}

// sub_10001BA0: DT_TerrorPlayer::m_isGhost. The target reads a full dword at
// the netprop offset and compares packed values, so we do the same.
int GetEntityIsGhostInt(void *entity) {
    static int offset = -1;
    if (offset < 0)
        offset = NetpropOffsetInTable("DT_TerrorPlayer", "m_isGhost");
    if (!entity || offset <= 0 || offset > 0x4000)
        return 0;
    if (IsBadReadPtr(static_cast<char *>(entity) + offset, sizeof(int)))
        return 0;
    return *reinterpret_cast<int *>(static_cast<char *>(entity) + offset);
}

// sub_10001CA0: DT_Witch::m_rage, read as a float by the aimbot (the target
// returns the raw dword and the caller reinterprets it as float).
float GetEntityWitchRage(void *entity) {
    static int offset = -1;
    if (offset < 0)
        offset = NetpropOffsetInTable("DT_Witch", "m_rage");
    if (!entity || offset <= 0 || offset > 0x4000)
        return 0.0f;
    if (IsBadReadPtr(static_cast<char *>(entity) + offset, sizeof(float)))
        return 0.0f;
    return *reinterpret_cast<float *>(static_cast<char *>(entity) + offset);
}

// sub_100017A0: DT_BasePlayer::m_flSimulationTime (Position Adjustment).
float GetEntitySimulationTime(void *entity) {
    static int offset = -1;
    if (offset < 0)
        offset = NetpropOffsetInTable("DT_BasePlayer", "m_flSimulationTime");
    if (!entity || offset <= 0 || offset > 0x4000)
        return 0.0f;
    if (IsBadReadPtr(static_cast<char *>(entity) + offset, sizeof(float)))
        return 0.0f;
    return *reinterpret_cast<float *>(static_cast<char *>(entity) + offset);
}

// sub_100019A0: DT_BasePlayer::m_nTickBase (weapon-ready gate).
int GetEntityTickBase(void *entity) {
    static int offset = -1;
    if (offset < 0)
        offset = NetpropOffsetInTable("DT_BasePlayer", "m_nTickBase");
    if (!entity || offset <= 0 || offset > 0x4000)
        return 0;
    if (IsBadReadPtr(static_cast<char *>(entity) + offset, sizeof(int)))
        return 0;
    return *reinterpret_cast<int *>(static_cast<char *>(entity) + offset);
}

// sub_10002020: DT_BaseCombatWeapon::m_flNextPrimaryAttack.
float GetEntityNextPrimaryAttack(void *weapon) {
    static int offset = -1;
    if (offset < 0)
        offset = NetpropOffsetInTable("DT_BaseCombatWeapon",
                                      "m_flNextPrimaryAttack");
    if (!weapon || offset <= 0 || offset > 0x4000)
        return 0.0f;
    if (IsBadReadPtr(static_cast<char *>(weapon) + offset, sizeof(float)))
        return 0.0f;
    return *reinterpret_cast<float *>(static_cast<char *>(weapon) + offset);
}

// ClientClass::m_pNetworkName at +8, reached through entity+8 vtable slot 1
// (GetClientClass). Used by the LOS check (CBreakableSurface) and cached in
// dword_1004B05C by the aimbot.
const char *GetEntityNetworkName(void *entity) {
    if (!entity || IsBadReadPtr(entity, 0x10))
        return nullptr;
    char *object = static_cast<char *>(entity) + 8;
    void **vtable = *reinterpret_cast<void ***>(object);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 2))
        return nullptr;
    typedef void *(__thiscall * GetClientClassFn)(void *);
    void *clientClass =
        reinterpret_cast<GetClientClassFn>(vtable[1])(object);
    if (!clientClass ||
        IsBadReadPtr(static_cast<char *>(clientClass) + 8, sizeof(void *)))
        return nullptr;
    const char *name =
        *reinterpret_cast<const char **>(static_cast<char *>(clientClass) + 8);
    if (!name || IsBadReadPtr(name, 1))
        return nullptr;
    return name;
}

// sub_10002D90: DT_TerrorPlayer::m_isIncapacitated, read as a dword.
int GetEntityIsIncapacitated(void *entity) {
    static int offset = -1;
    if (offset < 0)
        offset = NetpropOffsetInTable("DT_TerrorPlayer",
                                      "m_isIncapacitated");
    if (!entity || offset <= 0 || offset > 0x4000)
        return 0;
    if (IsBadReadPtr(static_cast<char *>(entity) + offset, sizeof(int)))
        return 0;
    return *reinterpret_cast<int *>(static_cast<char *>(entity) + offset);
}

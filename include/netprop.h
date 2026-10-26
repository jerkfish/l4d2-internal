// Netprop table walking, recovered from sub_100374A0 and the EndScene
// class-table build (lines 253-279).
#pragma once

struct RecvTable;

struct RecvProp {
    const char *m_pVarName;   // 0
    char pad0[36];            // 4..39
    RecvTable *m_pDataTable;  // 40
    int m_Offset;             // 44
    char pad1[12];            // 48..59
};

struct RecvTable {
    RecvProp *m_pProps;  // 0
    int m_nProps;        // 4
};

void BuildNetpropTables();
int NetpropTableCount();
int NetpropOffset(RecvTable *table, const char *name);
int FindNetpropOffset(const char *name);

int NetpropOffsetInTable(const char *tableName, const char *propName);

void *GetLocalPlayerEntity();
void *GetClientEntityByIndex(int index);
void *GetClientEntityFromHandle(int handle);
void *GetLocalActiveWeapon(void *local);
int GetHighestEntityIndex();
int GetEntityFlags(void *entity);
int GetEntityTeam(void *entity);
int GetEntityCategory(void *entity);
int GetEntityClassId(void *entity);
int GetEntitySequence(void *entity);
unsigned short GetEntitySolidFlags(void *entity);
int GetEntityIsGhostInt(void *entity);       // DT_TerrorPlayer::m_isGhost (int read)
int GetEntityIsIncapacitated(void *entity);  // DT_TerrorPlayer::m_isIncapacitated
float GetEntityWitchRage(void *entity);      // DT_Witch::m_rage
float GetEntitySimulationTime(void *entity); // DT_BasePlayer::m_flSimulationTime
int GetEntityTickBase(void *entity);         // DT_BasePlayer::m_nTickBase
float GetEntityNextPrimaryAttack(void *weapon); // DT_BaseCombatWeapon
const char *GetEntityNetworkName(void *entity); // ClientClass::m_pNetworkName
bool IsPlayerChamsEligible(void *entity);
int GetEntityHangingFromTongue(void *entity);
float GetEntityFallVelocity(void *entity);
void GetEntityVelocity(void *entity, float out[3]);
void GetEntityPunchAngle(void *entity, float out[3]);
int GetEntityLifeState(void *entity);
int GetEntityHealth(void *entity);

// ESP helpers (sub_1003B030 / sub_100014B0 / sub_1003AA60 / sub_10001AA0 /
// sub_100012B0 / sub_100013B0 / sub_10001CA0).
bool IsEntityDormant(void *entity);          // entity+8 vtable slot 7
int GetEntityRenderableModelId(void *entity); // entity+4 vtable slot 8
void GetEntityOrigin(void *entity, float out[3]);      // vtable slot 11
void GetEntityEyePosition(void *entity, float out[3]); // origin + view offset
void GetEntityMins(void *entity, float out[3]);
void GetEntityMaxs(void *entity, float out[3]);
int GetEntityMaxHealth(void *entity);
float GetEntityRage(void *entity);
int GetEntityMobRush(void *entity);
int GetEntityWeaponId(void *entity); // vtable slot 383

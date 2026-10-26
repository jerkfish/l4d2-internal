// No-Spread ports of the Rage (0x10006F81) and Legit (0x100073BA) blocks.
//
// Both blocks resolve the engine's SharedRandomFloat function and its seed
// global at runtime from client.dll, seed it with
// MD5_PseudoRandom(command_number) & 0x7FFFFFFF, and subtract the returned
// horizontal/vertical spread from the command view angles.
#pragma once

struct CUserCmd;

void NoSpread_Rage(CUserCmd *cmd, void *local, void *cachedWeapon);
void NoSpread_Legit(CUserCmd *cmd, void *local, void *cachedWeapon);

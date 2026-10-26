// Player/world ESP overlay recovered from sub_1003C410 and its helpers.
#pragma once

// PaintTraverse calls the target painter with this=local entity whenever any
// of Player Box / Player Name / Player Distance ESP is enabled; the painter
// then gates every draw behind Enable ESP (byte_1004ABCE).
void DrawEsp(void *localEntity);

// TEMP crash diagnostics: formats the last ESP entity/operation into buffer.

// sub_1003DB30: world weapon-spawn labels ("Weapons ESP##weapons2",
// byte_1004ABDB). Only draws on-screen boxes (sub_1003B030 visibility flag).
void DrawWeaponSpawnEsp(void *localEntity);

#include "UnitAPI.h"
#include "Lua.h"
#include "Hooks.h"
#include "NamePlates.h"

#include <cmath>

namespace {
    bool checkToken(lua_State* L, const char* token, guid_t guid) {
        if (const guid_t guid_t = ObjectMgr::GetGuidByUnitID(token); guid_t == guid) {
            Lua::lua_pushstring(L, token);
            return true;
        }
        return false;
    }

    bool checkIndexedTokens(lua_State* L, const char* base, int start, int end, guid_t guid) {
        char token[16];
        for (int i = start; i <= end; ++i) {
            snprintf(token, sizeof(token), "%s%d", base, i);
            if (checkToken(L, token, guid)) return true;
        }
        return false;
    }

    int unitHasFlag(lua_State* L, uint32_t flag) {
        CGUnit_C* unit = ObjectMgr::Get<CGUnit_C>(ObjectMgr::GetGuidByUnitID(Lua::luaL_checkstring(L, 1)), TYPEMASK_UNIT);
        if (unit && (unit->GetEntry<UnitEntry>()->m_flags & flag)) {
            Lua::lua_pushnumber(L, 1);
            return 1;
        }
        return 0;
    }

    int lua_UnitIsControlled(lua_State* L) {
        return unitHasFlag(L, UNIT_FLAG_FLEEING | UNIT_FLAG_CONFUSED | UNIT_FLAG_STUNNED | UNIT_FLAG_PACIFIED);
    }

    int lua_UnitIsDisarmed(lua_State* L) {
        return unitHasFlag(L, UNIT_FLAG_DISARMED);
    }

    int lua_UnitIsSilenced(lua_State* L) {
        return unitHasFlag(L, UNIT_FLAG_SILENCED);
    }

    /*
    enum NPCFlags : uint32_t
    {
        UNIT_NPC_FLAG_NONE = 0x00000000,       // SKIP
        UNIT_NPC_FLAG_GOSSIP = 0x00000001,       // TITLE has gossip menu DESCRIPTION 100%
        UNIT_NPC_FLAG_QUESTGIVER = 0x00000002,       // TITLE is quest giver DESCRIPTION guessed, probably ok
        UNIT_NPC_FLAG_UNK1 = 0x00000004,
        UNIT_NPC_FLAG_UNK2 = 0x00000008,
        UNIT_NPC_FLAG_TRAINER = 0x00000010,       // TITLE is trainer DESCRIPTION 100%
        UNIT_NPC_FLAG_TRAINER_CLASS = 0x00000020,       // TITLE is class trainer DESCRIPTION 100%
        UNIT_NPC_FLAG_TRAINER_PROFESSION = 0x00000040,       // TITLE is profession trainer DESCRIPTION 100%
        UNIT_NPC_FLAG_VENDOR = 0x00000080,       // TITLE is vendor (generic) DESCRIPTION 100%
        UNIT_NPC_FLAG_VENDOR_AMMO = 0x00000100,       // TITLE is vendor (ammo) DESCRIPTION 100%, general goods vendor
        UNIT_NPC_FLAG_VENDOR_FOOD = 0x00000200,       // TITLE is vendor (food) DESCRIPTION 100%
        UNIT_NPC_FLAG_VENDOR_POISON = 0x00000400,       // TITLE is vendor (poison) DESCRIPTION guessed
        UNIT_NPC_FLAG_VENDOR_REAGENT = 0x00000800,       // TITLE is vendor (reagents) DESCRIPTION 100%
        UNIT_NPC_FLAG_REPAIR = 0x00001000,       // TITLE can repair DESCRIPTION 100%
        UNIT_NPC_FLAG_FLIGHTMASTER = 0x00002000,       // TITLE is flight master DESCRIPTION 100%
        UNIT_NPC_FLAG_SPIRITHEALER = 0x00004000,       // TITLE is spirit healer DESCRIPTION guessed
        UNIT_NPC_FLAG_SPIRITGUIDE = 0x00008000,       // TITLE is spirit guide DESCRIPTION guessed
        UNIT_NPC_FLAG_INNKEEPER = 0x00010000,       // TITLE is innkeeper
        UNIT_NPC_FLAG_BANKER = 0x00020000,       // TITLE is banker DESCRIPTION 100%
        UNIT_NPC_FLAG_PETITIONER = 0x00040000,       // TITLE handles guild/arena petitions DESCRIPTION 100% 0xC0000 = guild petitions, 0x40000 = arena team petitions
        UNIT_NPC_FLAG_TABARDDESIGNER = 0x00080000,       // TITLE is guild tabard designer DESCRIPTION 100%
        UNIT_NPC_FLAG_BATTLEMASTER = 0x00100000,       // TITLE is battlemaster DESCRIPTION 100%
        UNIT_NPC_FLAG_AUCTIONEER = 0x00200000,       // TITLE is auctioneer DESCRIPTION 100%
        UNIT_NPC_FLAG_STABLEMASTER = 0x00400000,       // TITLE is stable master DESCRIPTION 100%
        UNIT_NPC_FLAG_GUILD_BANKER = 0x00800000,       // TITLE is guild banker DESCRIPTION cause client to send 997 opcode
        UNIT_NPC_FLAG_SPELLCLICK = 0x01000000,       // TITLE has spell click enabled DESCRIPTION cause client to send 1015 opcode (spell click)
        UNIT_NPC_FLAG_PLAYER_VEHICLE = 0x02000000,       // TITLE is player vehicle DESCRIPTION players with mounts that have vehicle data should have it set
        UNIT_NPC_FLAG_MAILBOX = 0x04000000        // TITLE is mailbox
    };
    */
    int lua_UnitOccupations(lua_State* L) {
        CGUnit_C* unit = ObjectMgr::Get<CGUnit_C>(ObjectMgr::GetGuidByUnitID(Lua::luaL_checkstring(L, 1)), TYPEMASK_UNIT);
        if (!unit) return 0;
        Lua::lua_pushnumber(L, unit->GetEntry<UnitEntry>()->m_npc_flags);
        return 1;
    }

    int lua_UnitOwner(lua_State* L) {
        CGUnit_C* unit = ObjectMgr::Get<CGUnit_C>(ObjectMgr::GetGuidByUnitID(Lua::luaL_checkstring(L, 1)), TYPEMASK_UNIT);
        if (!unit) return 0;
        auto e = unit->GetEntry<UnitEntry>();
        guid_t ownerGuid = e->m_summonedBy ? e->m_summonedBy : e->m_createdBy;
        if (!ownerGuid) return 0;

        char guidStr[32];
        snprintf(guidStr, sizeof(guidStr), "0x%016llX", ownerGuid);
        CGUnit_C* owner = ObjectMgr::Get<CGUnit_C>(ownerGuid, TYPEMASK_UNIT);
        if (!owner) return 0;
        const char* name = owner->GetObjectName();
        Lua::lua_pushstring(L, name ? name : "UNKNOWN");
        Lua::lua_pushstring(L, guidStr);
        return 2;
    }

    int lua_UnitTokenFromGUID(lua_State* L) {
        guid_t guid = ObjectMgr::HexString2Guid(Lua::luaL_checkstring(L, 1));
        if (!guid || !(ObjectMgr::Get<CGUnit_C>(guid, TYPEMASK_UNIT))) return 0;

        for (const char* token : { "player", "vehicle", "pet", "target", "focus", "mouseover" }) {
            if (checkToken(L, token, guid)) return 1;
        }

        if (checkIndexedTokens(L, "party", 1, 4, guid)) return 1;
        if (checkIndexedTokens(L, "partypet", 1, 4, guid)) return 1;
        if (checkIndexedTokens(L, "raid", 1, 40, guid)) return 1;
        if (checkIndexedTokens(L, "raidpet", 1, 40, guid)) return 1;
        if (checkIndexedTokens(L, "arena", 1, 5, guid)) return 1;
        if (checkIndexedTokens(L, "arenapet", 1, 5, guid)) return 1;
        if (checkIndexedTokens(L, "boss", 1, 5, guid)) return 1;

        int tokenId = NamePlates::GetTokenId(guid);
        if (tokenId >= 0) {
            char token[16];
            snprintf(token, sizeof(token), "nameplate%d", tokenId + 1);
            Lua::lua_pushstring(L, token);
            return 1;
        }
        return 0;
    }

    int lua_UnitInLOS(lua_State* L) {
        const char* targetToken = Lua::luaL_checkstring(L, 1);
        guid_t targetGuid = ObjectMgr::GetGuidByUnitID(targetToken);
        if (!targetGuid) return 0;

        CGUnit_C* target = ObjectMgr::Get<CGUnit_C>(targetGuid, TYPEMASK_UNIT);
        if (!target) return 0;

        CGPlayer_C* player = ObjectMgr::Get<CGPlayer_C>(ObjectMgr::GetPlayerGuid(), TYPEMASK_PLAYER);
        if (!player) return 0;

        C3Vector playerPos, targetPos;
        player->GetPosition(playerPos);
        target->GetPosition(targetPos);

        // Offset positions to approximate eye level for both units
        playerPos.Z += player->m_unitHeight * 0.5f;
        targetPos.Z += target->m_unitHeight * 0.5f;

        constexpr uint32_t LOS_FLAGS = 0x00100110;
        C3Vector hitPos{};
        float hitDist = 1.0f; // Must be initialised to 1.0f per TraceLine contract

        bool blocked = CGGameUI::TraceLine(playerPos, targetPos, LOS_FLAGS, hitPos, hitDist);

        // TraceLine returns true when the ray HIT something (LOS blocked)
        if (!blocked) Lua::lua_pushnumber(L, 1);
        return blocked ? 0 : 1;
    }

    int lua_UnitPosition(lua_State* L) {
        const char* token = Lua::luaL_checkstring(L, 1);
        guid_t guid = ObjectMgr::GetGuidByUnitID(token);
        if (!guid) return 0;

        CGUnit_C* unit = ObjectMgr::Get<CGUnit_C>(guid, TYPEMASK_UNIT);
        if (!unit) return 0;

        C3Vector pos;
        unit->GetPosition(pos);

        Lua::lua_pushnumber(L, pos.X);
        Lua::lua_pushnumber(L, pos.Y);
        Lua::lua_pushnumber(L, pos.Z);
        return 3;
    }

    int lua_UnitFacing(lua_State* L) {
        const char* token = Lua::luaL_checkstring(L, 1);
        guid_t guid = ObjectMgr::GetGuidByUnitID(token);
        if (!guid) return 0;

        CGUnit_C* unit = ObjectMgr::Get<CGUnit_C>(guid, TYPEMASK_UNIT);
        if (!unit) return 0;

        float facing = unit->GetFacing();
        if (!std::isfinite(facing)) return 0;

        // Match the documented UnitFacing contract: counterclockwise radians,
        // normalized to the [0, 2*pi) interval.
        constexpr float TWO_PI = 6.28318530717958647692f;
        facing = std::fmod(facing, TWO_PI);
        if (facing < 0.0f) facing += TWO_PI;

        Lua::lua_pushnumber(L, facing);
        return 1;
    }

    int lua_AmBehind(lua_State* L) {
        const char* token = Lua::luaL_checkstring(L, 1);

        // AmBehind is intentionally tied to the player's current target.
        // A different token may be supplied, but it must resolve to the same
        // GUID as the current target.
        guid_t currentTargetGuid = ObjectMgr::GetTargetGuid();
        guid_t unitGuid = ObjectMgr::GetGuidByUnitID(token);
        if (!currentTargetGuid || !unitGuid || unitGuid != currentTargetGuid) {
            Lua::lua_pushboolean(L, false);
            return 1;
        }

        CGPlayer_C* player = ObjectMgr::Get<CGPlayer_C>(
            ObjectMgr::GetPlayerGuid(), TYPEMASK_PLAYER);
        CGUnit_C* target = ObjectMgr::Get<CGUnit_C>(unitGuid, TYPEMASK_UNIT);
        if (!player || !target) {
            Lua::lua_pushboolean(L, false);
            return 1;
        }

        C3Vector playerPos{}, targetPos{};
        player->GetPosition(playerPos);
        target->GetPosition(targetPos);

        // Horizontal direction from the target's center to the player.
        const float dx = playerPos.X - targetPos.X;
        const float dy = playerPos.Y - targetPos.Y;
        const float distanceSquared = dx * dx + dy * dy;

        // At effectively identical horizontal coordinates, the rear arc is
        // undefined. Return false rather than allowing floating-point noise
        // to choose a side.
        constexpr float MIN_DISTANCE_SQUARED = 0.000001f;
        if (distanceSquared <= MIN_DISTANCE_SQUARED) {
            Lua::lua_pushboolean(L, false);
            return 1;
        }

        const float facing = target->GetFacing();
        if (!std::isfinite(facing)) {
            Lua::lua_pushboolean(L, false);
            return 1;
        }

        const float inverseDistance = 1.0f / std::sqrt(distanceSquared);
        const float targetToPlayerX = dx * inverseDistance;
        const float targetToPlayerY = dy * inverseDistance;

        // The client uses (cos(facing), sin(facing)) as the unit's forward
        // vector. A negative dot product places the player in the target's
        // rear 180-degree hemisphere, meaning the target faces away.
        const float forwardX = std::cos(facing);
        const float forwardY = std::sin(facing);
        const float facingDot =
            targetToPlayerX * forwardX + targetToPlayerY * forwardY;

        // Keep the exact side boundary out of the rear arc and absorb tiny
        // floating-point fluctuations around zero.
        constexpr float REAR_ARC_EPSILON = 0.0001f;
        Lua::lua_pushboolean(L, facingDot < -REAR_ARC_EPSILON);
        return 1;
    }

    int lua_UnitExistsGUID(lua_State* L) {
        const char* guidStr = Lua::luaL_checkstring(L, 1);
        guid_t guid = ObjectMgr::HexString2Guid(guidStr);
        if (!guid) return 0;

        CGUnit_C* unit = ObjectMgr::Get<CGUnit_C>(guid, TYPEMASK_UNIT);
        if (!unit) return 0;

        Lua::lua_pushnumber(L, 1);
        return 1;
    }

    int lua_TargetUnitByGUID(lua_State* L) {
        const char* guidStr = Lua::luaL_checkstring(L, 1);
        guid_t guid = ObjectMgr::HexString2Guid(guidStr);
        if (!guid) return 0;

        CGUnit_C* unit = ObjectMgr::Get<CGUnit_C>(guid, TYPEMASK_UNIT);
        if (!unit) return 0;

        CGGameUI::TargetFn(guid);
        return 0;
    }

    int lua_UnitGUIDByName(lua_State* L) {
        const char* name = Lua::luaL_checkstring(L, 1);
        if (!name || !name[0]) return 0;

        guid_t result = 0;
        ObjectMgr::EnumObjects([&](guid_t guid) -> bool {
            CGUnit_C* unit = ObjectMgr::Get<CGUnit_C>(guid, TYPEMASK_UNIT);
            if (!unit) return true;
            const char* unitName = unit->GetObjectName();
            if (unitName && strcmp(unitName, name) == 0) {
                result = guid;
                return false; // stop enumeration
            }
            return true;
        });

        if (!result) return 0;

        char guidStr[32];
        snprintf(guidStr, sizeof(guidStr), "0x%016llX", result);
        Lua::lua_pushstring(L, guidStr);
        return 1;
    }

    // Tracks last LOS update time
    static uint32_t s_lastLOSUpdate = 0;
    constexpr uint32_t LOS_UPDATE_INTERVAL_MS = 500;

    void OnUpdate_LOSCache() {
        uint32_t now = GetTickCount();
        if (now - s_lastLOSUpdate < LOS_UPDATE_INTERVAL_MS) return;
        s_lastLOSUpdate = now;

        lua_State* L = Lua::GetLuaState();
        if (!L) return;

        Lua::lua_getglobal(L, "Tic");
        if (!Lua::lua_istable(L, -1)) { Lua::lua_pop(L, 1); return; }

        Lua::lua_getfield(L, -1, "Heartbeat");
        if (!Lua::lua_isfunction(L, -1)) { Lua::lua_pop(L, 2); return; }

        Lua::lua_pushvalue(L, -2); // push Tic as self
        Lua::lua_pcall(L, 1, 0, 0);
        Lua::lua_pop(L, 1); // pop Tic table
    }

    int lua_openunitlib(lua_State* L) {
        Lua::luaL_Reg funcs[] = {
            { "UnitIsControlled", lua_UnitIsControlled },
            { "UnitIsDisarmed", lua_UnitIsDisarmed },
            { "UnitIsSilenced", lua_UnitIsSilenced },
            { "UnitOccupations", lua_UnitOccupations },
            { "UnitOwner", lua_UnitOwner },
            { "UnitTokenFromGUID", lua_UnitTokenFromGUID },
            { "UnitInLOS", lua_UnitInLOS },
            { "UnitPosition", lua_UnitPosition },
            { "UnitFacing", lua_UnitFacing },
            { "AmBehind", lua_AmBehind },
            { "UnitExistsGUID", lua_UnitExistsGUID },
            { "TargetUnitByGUID", lua_TargetUnitByGUID },
            { "UnitGUIDByName", lua_UnitGUIDByName },
        };
        for (auto& [name, func] : funcs) {
            Lua::lua_pushcfunction(L, func);
            Lua::lua_setglobal(L, name);
        }
        Hooks::FrameScript::registerOnUpdate(OnUpdate_LOSCache);
        return 0;
    }
}

void UnitAPI::initialize() {
    Hooks::FrameXML::registerLuaLib(lua_openunitlib);
}

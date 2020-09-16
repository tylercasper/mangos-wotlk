/*
 * This file is part of the CMaNGOS Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "Entities/Player.h"
#include "Entities/GameObject.h"
#include "BattleGround.h"
#include "BattleGroundIC.h"
#include "Tools/Language.h"
#include "Globals/ObjectMgr.h"

BattleGroundIC::BattleGroundIC() : m_hordeInnerGateGuid(ObjectGuid()), m_allianceInnerGate1Guid(ObjectGuid()), m_allianceInnerGate2Guid(ObjectGuid()), m_closeDoorTimer(0)
{
    m_StartMessageIds[BG_STARTING_EVENT_FIRST]  = LANG_BG_IC_START_TWO_MINUTES;
    m_StartMessageIds[BG_STARTING_EVENT_SECOND] = LANG_BG_IC_START_ONE_MINUTE;
    m_StartMessageIds[BG_STARTING_EVENT_THIRD]  = LANG_BG_IC_START_HALF_MINUTE;
    // ToDo: use the text LANG_BG_IC_START_15_SECONDS
    m_StartMessageIds[BG_STARTING_EVENT_FOURTH] = LANG_BG_IC_BEGIN;
}

void BattleGroundIC::Reset()
{
    // call parent's class reset
    BattleGround::Reset();

    for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
        m_reinforcements[i] = BG_IC_MAX_REINFORCEMENTS;

    // setup the owner and state for all objectives
    for (uint8 i = 0; i < BG_IC_MAX_OBJECTIVES; ++i)
    {
        switch (i)
        {
            case BG_IC_OBJECTIVE_KEEP_ALLY:
                m_objectiveOwner[i]  = TEAM_INDEX_ALLIANCE;
                m_objectiveConquerer[i] = TEAM_INDEX_ALLIANCE;
                break;
            case BG_IC_OBJECTIVE_KEEP_HORDE:
                m_objectiveOwner[i] = TEAM_INDEX_HORDE;
                m_objectiveConquerer[i] = TEAM_INDEX_HORDE;
                break;
            default:
                m_objectiveOwner[i] = TEAM_INDEX_NEUTRAL;
                break;
        }

        m_objectiveConquerer[i] = TEAM_INDEX_NEUTRAL;
        m_objectiveState[i] = iocDefaultStates[i];
        m_objectiveTimer[i] = 0;
    }

    // setup the state for the keep walls
    for (uint8 i = 0; i < BG_IC_MAX_KEEP_GATES; ++i)
        m_gatesAllianceState[i] = isleAllianceWallsData[i].stateClosed;

    for (uint8 i = 0; i < BG_IC_MAX_KEEP_GATES; ++i)
        m_gatesHordeState[i] = isleHordeWallsData[i].stateClosed;

    m_closeDoorTimer = 0;
    m_isKeepInvaded[TEAM_INDEX_ALLIANCE] = false;
    m_isKeepInvaded[TEAM_INDEX_HORDE] = false;

    // setup initial graveyards
    sObjectMgr.SetGraveYardLinkTeam(BG_IC_GRAVEYARD_ID_KEEP_ALLY, BG_IC_ZONE_ID_ISLE, ALLIANCE);
    sObjectMgr.SetGraveYardLinkTeam(BG_IC_GRAVEYARD_ID_KEEP_HORDE, BG_IC_ZONE_ID_ISLE, HORDE);

    sObjectMgr.SetGraveYardLinkTeam(BG_IC_GRAVEYARD_ID_DOCKS, BG_IC_ZONE_ID_ISLE, TEAM_INVALID);
    sObjectMgr.SetGraveYardLinkTeam(BG_IC_GRAVEYARD_ID_HANGAR, BG_IC_ZONE_ID_ISLE, TEAM_INVALID);
    sObjectMgr.SetGraveYardLinkTeam(BG_IC_GRAVEYARD_ID_WORKSHOP, BG_IC_ZONE_ID_ISLE, TEAM_INVALID);
}

void BattleGroundIC::AddPlayer(Player* plr)
{
    BattleGround::AddPlayer(plr);
    // create score and add it to map, default values are set in constructor
    BattleGroundICScore* sc = new BattleGroundICScore;

    m_PlayerScores[plr->GetObjectGuid()] = sc;
}

void BattleGroundIC::StartingEventOpenDoors()
{
    // open alliance gates
    for (const auto& i : m_keepGatesGuid[TEAM_INDEX_ALLIANCE])
        if (GameObject* pGate = GetBgMap()->GetGameObject(i))
            pGate->UseDoorOrButton();

    // open horde gates
    for (const auto& i : m_keepGatesGuid[TEAM_INDEX_HORDE])
        if (GameObject* pGate = GetBgMap()->GetGameObject(i))
            pGate->UseDoorOrButton();

    // open tower gates
    for (const auto& guid : m_towerGatesGuids)
        if (GameObject* pGate = GetBgMap()->GetGameObject(guid))
            pGate->UseDoorOrButton();

    // enable all teleporters
    for (const auto& guid : m_teleporterGuids)
        if (GameObject* pTele = GetBgMap()->GetGameObject(guid))
            pTele->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);

    // enable all animations
    for (const auto& guid : m_teleporterAnimGuids)
        if (GameObject* pTele = GetBgMap()->GetGameObject(guid))
            pTele->UseDoorOrButton();

    m_closeDoorTimer = BG_IC_CLOSE_DOORS_TIME;
}

void BattleGroundIC::UpdatePlayerScore(Player* source, uint32 type, uint32 value)
{
    BattleGroundScoreMap::iterator itr = m_PlayerScores.find(source->GetObjectGuid());

    if (itr == m_PlayerScores.end())                        // player not found...
        return;

    switch (type)
    {
        case SCORE_BASES_ASSAULTED:
            ((BattleGroundICScore*)itr->second)->BasesAssaulted += value;
            break;
        case SCORE_BASES_DEFENDED:
            ((BattleGroundICScore*)itr->second)->BasesDefended += value;
            break;
        default:
            BattleGround::UpdatePlayerScore(source, type, value);
            break;
    }
}

void BattleGroundIC::FillInitialWorldStates(WorldPacket& data, uint32& count)
{
    // show the reinforcements
    FillInitialWorldState(data, count, BG_IC_STATE_ALLY_REINFORCE_SHOW, 1);
    FillInitialWorldState(data, count, BG_IC_STATE_HORDE_REINFORCE_SHOW, 1);
    FillInitialWorldState(data, count, BG_IC_STATE_ALLY_REINFORCE_COUNT, m_reinforcements[TEAM_INDEX_ALLIANCE]);
    FillInitialWorldState(data, count, BG_IC_STATE_HORDE_REINFORCE_COUNT, m_reinforcements[TEAM_INDEX_ALLIANCE]);

    // show the capturable bases
    for (uint8 i = 0; i < BG_IC_MAX_OBJECTIVES; ++i)
        FillInitialWorldState(data, count, m_objectiveState[i], 1);

    // show the walls
    for (uint8 i = 0; i < BG_IC_MAX_KEEP_GATES; ++i)
        FillInitialWorldState(data, count, m_gatesAllianceState[i], 1);

    for (uint8 i = 0; i < BG_IC_MAX_KEEP_GATES; ++i)
        FillInitialWorldState(data, count, m_gatesHordeState[i], 1);
}

// process the gate events
bool BattleGroundIC::HandleEvent(uint32 eventId, GameObject* go, Unit* invoker)
{
    DEBUG_LOG("BattleGroundIC: Handle event for gameobject entry %u.", go->GetEntry());

    for (uint8 i = 0; i < BG_IC_MAX_KEEP_GATES; ++i)
    {
        if (eventId == isleAllianceWallsData[i].eventId)
        {
            UpdateWorldState(m_gatesAllianceState[i], 0);
            m_gatesAllianceState[i] = isleAllianceWallsData[i].stateOpened;
            UpdateWorldState(m_gatesAllianceState[i], 1);

            if (GameObject* pGate = GetBgMap()->GetGameObject(m_keepGatesGuid[TEAM_INDEX_ALLIANCE][i]))
                pGate->UseDoorOrButton(0, true);

            if (!m_isKeepInvaded[TEAM_INDEX_ALLIANCE])
            {
                // open the inner keep gates
                if (GameObject* pGate = GetBgMap()->GetGameObject(m_allianceInnerGate1Guid))
                    pGate->UseDoorOrButton();
                if (GameObject* pGate = GetBgMap()->GetGameObject(m_allianceInnerGate2Guid))
                    pGate->UseDoorOrButton();

                // spawn the boss
                for (const auto& i : iocAllianceKeepSpawns)
                    go->SummonCreature(i.entry, i.x, i.y, i.z, i.o, TEMPSPAWN_DEAD_DESPAWN, 0);

                // allow the graveyard to be captured
                if (GameObject* pFlag = GetSingleGameObjectFromStorage(BG_IC_GO_BANNER_ALLIANCE_KEEP_A))
                    pFlag->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
            }

            m_isKeepInvaded[TEAM_INDEX_ALLIANCE] = true;

            return true;
        }

        if (eventId == isleHordeWallsData[i].eventId)
        {
            UpdateWorldState(m_gatesHordeState[i], 0);
            m_gatesHordeState[i] = isleHordeWallsData[i].stateOpened;
            UpdateWorldState(m_gatesHordeState[i], 1);

            if (GameObject* pGate = GetBgMap()->GetGameObject(m_keepGatesGuid[TEAM_INDEX_HORDE][i]))
                pGate->UseDoorOrButton(0, true);

            if (!m_isKeepInvaded[TEAM_INDEX_HORDE])
            {
                // open the inner keep gates
                if (GameObject* pGate = GetBgMap()->GetGameObject(m_hordeInnerGateGuid))
                    pGate->UseDoorOrButton();

                // spawn the boss
                for (const auto& i : iocHordeKeepSpawns)
                    go->SummonCreature(i.entry, i.x, i.y, i.z, i.o, TEMPSPAWN_DEAD_DESPAWN, 0);

                // allow the graveyard to be captured
                if (GameObject* pFlag = GetSingleGameObjectFromStorage(BG_IC_GO_BANNER_HORDE_KEEP_H))
                    pFlag->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
            }

            m_isKeepInvaded[TEAM_INDEX_HORDE] = true;

            return true;
        }
    }

    return false;
}

// Called when a player clicks a capturable banner
void BattleGroundIC::EventPlayerClickedOnFlag(Player* player, GameObject* go)
{
    DEBUG_LOG("BattleGroundIC: Handle flag clicked for gameobject entry %u.", go->GetEntry());

    PvpTeamIndex newOwnerIdx = GetTeamIndexByTeamId(player->GetTeam());
    uint8 objectiveId       = 0;
    uint32 newWorldState    = 0;
    uint32 soundId          = 0;
    uint32 nextFlagEntry    = 0;
    int32 textEntry         = 0;

    // *** Check if status changed from neutral to contested ***
    for (const auto& i : isleGameObjectNeutralData)
    {
        if (go->GetEntry() == i.objectEntry)
        {
            UpdatePlayerScore(player, SCORE_BASES_ASSAULTED, 1);

            objectiveId = i.objectiveId;
            newWorldState = newOwnerIdx == TEAM_INDEX_ALLIANCE ? i.nextWorldStateAlly : i.nextWorldStateHorde;

            textEntry = LANG_BG_IC_NODE_ASSAULTED;
            soundId = newOwnerIdx == TEAM_INDEX_ALLIANCE ? BG_IC_SOUND_NODE_ASSAULTED_ALLIANCE : BG_IC_SOUND_NODE_ASSAULTED_HORDE;

            nextFlagEntry = newOwnerIdx == TEAM_INDEX_ALLIANCE ? i.nextObjectAlly : i.nextObjectHorde;
            break;
        }
    }

    if (!objectiveId)
    {
        // *** Check if status changed from owned to contested ***
        for (const auto& i : isleGameObjectOwnedData)
        {
            if (go->GetEntry() == i.objectEntry)
            {
                UpdatePlayerScore(player, SCORE_BASES_ASSAULTED, 1);

                objectiveId = i.objectiveId;
                newWorldState = i.nextState;

                textEntry = LANG_BG_IC_NODE_ASSAULTED;
                soundId = newOwnerIdx == TEAM_INDEX_ALLIANCE ? BG_IC_SOUND_NODE_ASSAULTED_ALLIANCE : BG_IC_SOUND_NODE_ASSAULTED_HORDE;

                nextFlagEntry = i.nextObject;

                // reset the objective
                DoResetObjective(IsleObjective(objectiveId));
                break;
            }
        }
    }

    if (!objectiveId)
    {
        // *** Check if status changed from contested to owned / contested ***
        for (const auto& i : isleGameObjectContestedData)
        {
            if (go->GetEntry() == i.objectEntry)
            {
                objectiveId = i.objectiveId;

                UpdatePlayerScore(player, m_objectiveConquerer[objectiveId] == newOwnerIdx ? SCORE_BASES_DEFENDED : SCORE_BASES_ASSAULTED, 1);

                newWorldState = m_objectiveConquerer[objectiveId] == newOwnerIdx ? i.nextStateDefend : i.nextStateAssault;

                textEntry = m_objectiveConquerer[objectiveId] == newOwnerIdx ? LANG_BG_IC_NODE_DEFENDED : LANG_BG_IC_NODE_ASSAULTED;
                soundId = newOwnerIdx == TEAM_INDEX_ALLIANCE ? BG_IC_SOUND_NODE_ASSAULTED_ALLIANCE : BG_IC_SOUND_NODE_ASSAULTED_HORDE;

                nextFlagEntry = m_objectiveConquerer[objectiveId] == newOwnerIdx ? i.nextObjectDefend : i.nextObjectDefend;
                break;
            }
        }
    }

    // only process the event if needed
    if (objectiveId)
    {
        m_objectiveOwner[objectiveId] = newOwnerIdx;

        // update world states
        UpdateWorldState(m_objectiveState[objectiveId], 0);
        m_objectiveState[objectiveId] = newWorldState;
        UpdateWorldState(m_objectiveState[objectiveId], 1);

        // start timer
        m_objectiveTimer[objectiveId] = BG_IC_FLAG_CAPTURING_TIME;

        // send the zone message and sound
        ChatMsg chatSystem = newOwnerIdx == TEAM_INDEX_ALLIANCE ? CHAT_MSG_BG_SYSTEM_ALLIANCE : CHAT_MSG_BG_SYSTEM_HORDE;
        uint32 factionStrig = newOwnerIdx == TEAM_INDEX_ALLIANCE ? LANG_BG_ALLY : LANG_BG_HORDE;

        SendMessage2ToAll(textEntry, chatSystem, player, isleObjectiveData[objectiveId].message, factionStrig);
        PlaySoundToAll(soundId);

        // despawn the current flag
        go->SetLootState(GO_JUST_DEACTIVATED);

        // respawn the new flag
        if (GameObject* pFlag = GetSingleGameObjectFromStorage(nextFlagEntry))
        {
            m_currentFlagGuid[objectiveId] = pFlag->GetObjectGuid();
            pFlag->SetRespawnTime(60 * MINUTE);
            pFlag->Refresh();
        }
    }
}

void BattleGroundIC::HandleKillUnit(Creature* creature, Player* killer)
{
    DEBUG_LOG("BattleGroundIC: Handle unit kill for creature entry %u.", creature->GetEntry());

    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    // end battle if boss was killed
    switch (creature->GetEntry())
    {
        case BG_IC_NPC_COMMANDER_WYRMBANE:
            EndBattleGround(HORDE);
            break;
        case BG_IC_NPC_OVERLORD_AGMAR:
            EndBattleGround(ALLIANCE);
            break;
        case BG_IC_VEHICLE_DEMOLISHER:
        {
            PvpTeamIndex ownerTeam = creature->getFaction() == BG_IC_FACTION_ID_ALLIANCE ? TEAM_INDEX_ALLIANCE : TEAM_INDEX_HORDE;
            m_workshopSpawnsGuids[ownerTeam].remove(creature->GetObjectGuid());
            break;
        }
        case BG_IC_VEHICLE_SIEGE_ENGINE_A:
            m_workshopSpawnsGuids[TEAM_INDEX_ALLIANCE].remove(creature->GetObjectGuid());
            break;
        case BG_IC_VEHICLE_SIEGE_ENGINE_H:
            m_workshopSpawnsGuids[TEAM_INDEX_HORDE].remove(creature->GetObjectGuid());
            break;
        case BG_IC_VEHICLE_CATAPULT:
        {
            PvpTeamIndex ownerTeam = creature->getFaction() == BG_IC_FACTION_ID_ALLIANCE ? TEAM_INDEX_ALLIANCE : TEAM_INDEX_HORDE;
            m_docksSpawnsGuids[ownerTeam].push_back(creature->GetObjectGuid());
            break;
        }
        case BG_IC_VEHICLE_GLAIVE_THROWER_A:
            m_docksSpawnsGuids[TEAM_INDEX_ALLIANCE].remove(creature->GetObjectGuid());
            break;
        case BG_IC_VEHICLE_GLAIVE_THROWER_H:
            m_docksSpawnsGuids[TEAM_INDEX_HORDE].remove(creature->GetObjectGuid());
            break;
    }
}

void BattleGroundIC::HandleKillPlayer(Player* player, Player* killer)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    BattleGround::HandleKillPlayer(player, killer);

    // remove one resource for each player killed
    PvpTeamIndex killedPlayerIdx = GetTeamIndexByTeamId(player->GetTeam());
    --m_reinforcements[killedPlayerIdx];

    UpdateWorldState(killedPlayerIdx == TEAM_INDEX_ALLIANCE ? BG_IC_STATE_ALLY_REINFORCE_COUNT : BG_IC_STATE_HORDE_REINFORCE_COUNT, m_reinforcements[killedPlayerIdx]);

    // if reached 0, the other team wins
    if (!m_reinforcements[killedPlayerIdx])
        EndBattleGround(killer->GetTeam());
}

void BattleGroundIC::HandleCreatureCreate(Creature* creature)
{
    switch (creature->GetEntry())
    {
        case BG_IC_VEHICLE_KEEP_CANNON:
            creature->SetFactionTemporary(creature->GetPositionX() < 500.0f ? BG_IC_FACTION_ID_ALLIANCE : BG_IC_FACTION_ID_HORDE, TEMPFACTION_NONE);
            break;
        case BG_IC_VEHICLE_DEMOLISHER:
        {
            PvpTeamIndex ownerTeam = m_objectiveOwner[BG_IC_OBJECTIVE_WORKSHOP];
            creature->SetFactionTemporary(iocTeamFactions[ownerTeam], TEMPFACTION_NONE);
            m_workshopSpawnsGuids[ownerTeam].push_back(creature->GetObjectGuid());
            break;
        }
        case BG_IC_VEHICLE_SIEGE_ENGINE_A:
        case BG_IC_NPC_GNOMISH_MECHANIC:
            m_workshopSpawnsGuids[TEAM_INDEX_ALLIANCE].push_back(creature->GetObjectGuid());
            break;
        case BG_IC_VEHICLE_SIEGE_ENGINE_H:
        case BG_IC_NPC_GOBLIN_MECHANIC:
            m_workshopSpawnsGuids[TEAM_INDEX_HORDE].push_back(creature->GetObjectGuid());
            break;
        case BG_IC_VEHICLE_CATAPULT:
        {
            PvpTeamIndex ownerTeam = m_objectiveOwner[BG_IC_OBJECTIVE_DOCKS];
            creature->SetFactionTemporary(iocTeamFactions[ownerTeam], TEMPFACTION_NONE);
            m_docksSpawnsGuids[ownerTeam].push_back(creature->GetObjectGuid());
            break;
        }
        case BG_IC_VEHICLE_GLAIVE_THROWER_A:
            m_docksSpawnsGuids[TEAM_INDEX_ALLIANCE].push_back(creature->GetObjectGuid());
            break;
        case BG_IC_VEHICLE_GLAIVE_THROWER_H:
            m_docksSpawnsGuids[TEAM_INDEX_HORDE].push_back(creature->GetObjectGuid());
            break;
    }
}

void BattleGroundIC::HandleGameObjectCreate(GameObject* go)
{
    switch (go->GetEntry())
    {
        case BG_IC_GO_GUNSHIP_A:
            m_gunshipGuid[TEAM_INDEX_ALLIANCE] = go->GetObjectGuid();
            break;
        case BG_IC_GO_GUNSHIP_H:
            m_gunshipGuid[TEAM_INDEX_HORDE] = go->GetObjectGuid();
            break;
        case BG_IC_GO_SEAFORIUM_BOMBS:
            m_bombsGuids.push_back(go->GetObjectGuid());
            break;
        case BG_IC_GO_PORTCULLIS_GATE_A:
            // sort each portculis gate, to use it later in script
            if (go->GetPositionX() > 400.0f)
                m_keepGatesGuid[TEAM_INDEX_ALLIANCE][BG_IC_GATE_FRONT] = go->GetObjectGuid();
            else
            {
                if (go->GetPositionY() > -800.0f)
                    m_keepGatesGuid[TEAM_INDEX_ALLIANCE][BG_IC_GATE_WEST] = go->GetObjectGuid();
                else
                    m_keepGatesGuid[TEAM_INDEX_ALLIANCE][BG_IC_GATE_EAST] = go->GetObjectGuid();
            }
            break;
        case BG_IC_GO_PORTCULLIS_GATE_H:
            // sort each portculis gate, to use it later in script
            if (go->GetPositionX() < 1200.0f)
                m_keepGatesGuid[TEAM_INDEX_HORDE][BG_IC_GATE_FRONT] = go->GetObjectGuid();
            else
            {
                if (go->GetPositionY() > -700.0f)
                    m_keepGatesGuid[TEAM_INDEX_HORDE][BG_IC_GATE_WEST] = go->GetObjectGuid();
                else
                    m_keepGatesGuid[TEAM_INDEX_HORDE][BG_IC_GATE_EAST] = go->GetObjectGuid();
            }
            break;
        case BG_IC_GO_PORTCULLIS_TOWER_A:
        case BG_IC_GO_PORTCULLIS_TOWER_H:
            m_towerGatesGuids.push_back(go->GetObjectGuid());
            break;
        case BG_IC_GO_TELEPORTER_OUTSIDE_H:
        case BG_IC_GO_TELEPORTER_INSIDE_H:
        case BG_IC_GO_TELEPORTER_OUTSIDE_A:
        case BG_IC_GO_TELEPORTER_INSIDE_A:
            m_teleporterGuids.push_back(go->GetObjectGuid());
            break;
        case BG_IC_GO_TELEPORTER_EFFECTS_H:
        case BG_IC_GO_TELEPORTER_EFFECTS_A:
            m_teleporterAnimGuids.push_back(go->GetObjectGuid());
            break;
        case BG_IC_GO_PORTCULLIS_KEEP_H:
            m_hordeInnerGateGuid = go->GetObjectGuid();
            break;
        case BG_IC_GO_PORTCULLIS_KEEP_A1:
            m_allianceInnerGate1Guid = go->GetObjectGuid();
            break;
        case BG_IC_GO_PORTCULLIS_KEEP_A2:
            m_allianceInnerGate2Guid = go->GetObjectGuid();
            break;
        case BG_IC_GO_GUNSHIP_GROUND_PORTAL_A:
            m_hangarPortalsGuids[TEAM_INDEX_ALLIANCE].push_back(go->GetObjectGuid());
            break;
        case BG_IC_GO_GUNSHIP_GROUND_PORTAL_H:
            m_hangarPortalsGuids[TEAM_INDEX_HORDE].push_back(go->GetObjectGuid());
            break;
        case BG_IC_GO_GUNSHIP_PORTAL_EFFECTS_A:
            m_hangarAnimGuids[TEAM_INDEX_ALLIANCE].push_back(go->GetObjectGuid());
            break;
        case BG_IC_GO_GUNSHIP_PORTAL_EFFECTS_H:
            m_hangarAnimGuids[TEAM_INDEX_HORDE].push_back(go->GetObjectGuid());
            break;
        case BG_IC_GO_BANNER_ALLIANCE_KEEP_A:
        case BG_IC_GO_BANNER_ALLIANCE_KEEP_A_GREY:
        case BG_IC_GO_BANNER_ALLIANCE_KEEP_H:
        case BG_IC_GO_BANNER_ALLIANCE_KEEP_H_GREY:
        case BG_IC_GO_BANNER_HORDE_KEEP_A:
        case BG_IC_GO_BANNER_HORDE_KEEP_A_GREY:
        case BG_IC_GO_BANNER_HORDE_KEEP_H:
        case BG_IC_GO_BANNER_HORDE_KEEP_H_GREY:
        case BG_IC_GO_BANNER_WORKSHOP_A:
        case BG_IC_GO_BANNER_WORKSHOP_A_GREY:
        case BG_IC_GO_BANNER_WORKSHOP_H:
        case BG_IC_GO_BANNER_WORKSHOP_H_GREY:
        case BG_IC_GO_BANNER_DOCKS_A:
        case BG_IC_GO_BANNER_DOCKS_A_GREY:
        case BG_IC_GO_BANNER_DOCKS_H:
        case BG_IC_GO_BANNER_DOCKS_H_GREY:
        case BG_IC_GO_BANNER_HANGAR_A:
        case BG_IC_GO_BANNER_HANGAR_A_GREY:
        case BG_IC_GO_BANNER_HANGAR_H:
        case BG_IC_GO_BANNER_HANGAR_H_GREY:
        case BG_IC_GO_BANNER_REFINERY_A:
        case BG_IC_GO_BANNER_REFINERY_A_GREY:
        case BG_IC_GO_BANNER_REFINERY_H:
        case BG_IC_GO_BANNER_REFINERY_H_GREY:
        case BG_IC_GO_BANNER_QUARRY_A:
        case BG_IC_GO_BANNER_QUARRY_A_GREY:
        case BG_IC_GO_BANNER_QUARRY_H:
        case BG_IC_GO_BANNER_QUARRY_H_GREY:
            m_goEntryGuidStore[go->GetEntry()] = go->GetObjectGuid();
            break;
    }
}

void BattleGroundIC::EndBattleGround(Team winner)
{
    // win reward
    if (winner == ALLIANCE)
        RewardHonorToTeam(GetBonusHonorFromKill(1), ALLIANCE);
    if (winner == HORDE)
        RewardHonorToTeam(GetBonusHonorFromKill(1), HORDE);

    // complete map_end rewards (even if no team wins)
    RewardHonorToTeam(GetBonusHonorFromKill(1), HORDE);
    RewardHonorToTeam(GetBonusHonorFromKill(1), ALLIANCE);

    BattleGround::EndBattleGround(winner);
}

bool BattleGroundIC::CheckAchievementCriteriaMeet(uint32 criteria_id, Player const* source, Unit const* target, uint32 miscvalue1)
{
    switch (criteria_id)
    {
        case BG_IC_CRIT_RESOURCE_GLUT_A:
            return m_objectiveOwner[BG_IC_OBJECTIVE_QUARY] == TEAM_INDEX_ALLIANCE && m_objectiveOwner[BG_IC_OBJECTIVE_REFINERY] == TEAM_INDEX_ALLIANCE;
        case BG_IC_CRIT_RESOURCE_GLUT_H:
            return m_objectiveOwner[BG_IC_OBJECTIVE_QUARY] == TEAM_INDEX_HORDE && m_objectiveOwner[BG_IC_OBJECTIVE_REFINERY] == TEAM_INDEX_HORDE;
    }

    return false;
}

// Function that handles the completion of objective
void BattleGroundIC::DoCaptureObjective(IsleObjective objective)
{
    GameObject* pOriginalFlag = GetBgMap()->GetGameObject(m_currentFlagGuid[objective]);
    if (!pOriginalFlag)
        return;

    // Loop through the list of objects in the Conquer data list
    for (const auto& i : isleGameObjectConquerData)
    {
        if (pOriginalFlag->GetEntry() == i.objectEntry)
        {
            uint8 objId = i.objectiveId;
            PvpTeamIndex ownerIdx = m_objectiveOwner[objId];
            m_objectiveConquerer[objId] = ownerIdx;

            // update world states
            UpdateWorldState(m_objectiveState[objId], 0);
            m_objectiveState[objId] = i.nextState;
            UpdateWorldState(m_objectiveState[objId], 1);

            ChatMsg chatSystem = ownerIdx == TEAM_INDEX_ALLIANCE ? CHAT_MSG_BG_SYSTEM_ALLIANCE : CHAT_MSG_BG_SYSTEM_HORDE;
            uint32 factionStrig = ownerIdx == TEAM_INDEX_ALLIANCE ? LANG_BG_ALLY : LANG_BG_HORDE;

            // send zone message; the AB string is the same for IC
            SendMessage2ToAll(LANG_BG_AB_NODE_TAKEN, chatSystem, nullptr, factionStrig, isleObjectiveData[objId].message);

            // play sound is
            uint32 soundId = ownerIdx == TEAM_INDEX_ALLIANCE ? BG_IC_SOUND_NODE_CAPTURED_ALLIANCE : BG_IC_SOUND_NODE_CAPTURED_HORDE;
            PlaySoundToAll(soundId);

            // change flag object
            pOriginalFlag->SetLootState(GO_JUST_DEACTIVATED);

            // respawn the new flag
            if (GameObject* pFlag = GetSingleGameObjectFromStorage(i.nextObject))
            {
                m_currentFlagGuid[objId] = pFlag->GetObjectGuid();
                pFlag->SetRespawnTime(60 * MINUTE);
                pFlag->Refresh();
            }

            // if spell is provided, apply spell
            // ToDo: apply aura on owned vehicles as well
            uint32 spellId = isleObjectiveData[objId].spellEntry;
            if (spellId)
                CastSpellOnTeam(spellId, GetTeamIdByTeamIndex(ownerIdx));

            // if graveyard is provided, link the graveyard
            uint32 graveyardId = isleObjectiveData[objId].graveyardId;
            if (graveyardId)
                sObjectMgr.SetGraveYardLinkTeam(graveyardId, BG_IC_ZONE_ID_ISLE, GetTeamIdByTeamIndex(ownerIdx));

            // spawn the vehicles / enable the gunship
            switch (objId)
            {
                case BG_IC_OBJECTIVE_WORKSHOP:
                {
                    // summon the vehicles and the mechanic
                    for (uint8 i = 0; i < 6; ++i)
                        pOriginalFlag->SummonCreature(ownerIdx == TEAM_INDEX_ALLIANCE ? iocWorkshopSpawns[i].entryAlly : iocWorkshopSpawns[i].entryHorde, iocWorkshopSpawns[i].x, iocWorkshopSpawns[i].y, iocWorkshopSpawns[i].z, iocWorkshopSpawns[i].o, TEMPSPAWN_DEAD_DESPAWN, 0);

                    // respawn the workshop bombs and give them the right faction
                    for (const auto& guid : m_bombsGuids)
                    {
                        if (GameObject* pBomb = GetBgMap()->GetGameObject(guid))
                        {
                            pBomb->SetFaction(iocTeamFactions[ownerIdx]);
                            pBomb->SetRespawnTime(60 * MINUTE);
                            pBomb->Refresh();
                        }
                    }

                    break;
                }
                case BG_IC_OBJECTIVE_DOCKS:
                {
                    // summon the vehicles
                    for (uint8 i = 0; i < 6; ++i)
                        pOriginalFlag->SummonCreature(ownerIdx == TEAM_INDEX_ALLIANCE ? iocDocksSpawns[i].entryAlly : iocDocksSpawns[i].entryHorde, iocDocksSpawns[i].x, iocDocksSpawns[i].y, iocDocksSpawns[i].z, iocDocksSpawns[i].o, TEMPSPAWN_DEAD_DESPAWN, 0);
                    break;
                }
                case BG_IC_OBJECTIVE_HANGAR:
                {
                    // respawn portals
                    for (const auto& guid : m_hangarPortalsGuids[ownerIdx])
                    {
                        if (GameObject* pPortal = GetBgMap()->GetGameObject(guid))
                        {
                            pPortal->SetRespawnTime(60 * MINUTE);
                            pPortal->Refresh();
                        }
                    }

                    // respawn and enable the animations
                    for (const auto& guid : m_hangarAnimGuids[ownerIdx])
                    {
                        if (GameObject* pAnim = GetBgMap()->GetGameObject(guid))
                        {
                            pAnim->SetRespawnTime(60 * MINUTE);
                            pAnim->Refresh();
                            pAnim->UseDoorOrButton();
                        }
                    }
                    break;
                }
            }
        }
    }

    // despawn flag
    pOriginalFlag->SetLootState(GO_JUST_DEACTIVATED);
}

// Function that handles the reset of objective
void BattleGroundIC::DoResetObjective(IsleObjective objective)
{
    PvpTeamIndex ownerIdx = m_objectiveOwner[objective];

    switch (objective)
    {
        case BG_IC_OBJECTIVE_WORKSHOP:
        {
            // despwn bombs
            for (const auto& guid : m_bombsGuids)
            {
                if (GameObject* pBomb = GetBgMap()->GetGameObject(guid))
                    pBomb->SetLootState(GO_JUST_DEACTIVATED);
            }

            // despwn the vehicles if not already in use
            for (const auto& guid : m_workshopSpawnsGuids[ownerIdx])
            {
                if (Creature* pCreature = GetBgMap()->GetCreature(guid))
                {
                    if (!pCreature->IsVehicle() || (pCreature->IsVehicle() && !pCreature->IsBoarded()))
                        pCreature->ForcedDespawn();
                }
            }

            m_workshopSpawnsGuids[ownerIdx].clear();
            break;
        }
        case BG_IC_OBJECTIVE_DOCKS:
        {
            // despwn the vehicles if not already in use
            for (const auto& guid : m_docksSpawnsGuids[ownerIdx])
            {
                if (Creature* pVehicle = GetBgMap()->GetCreature(guid))
                {
                    if (!pVehicle->IsBoarded())
                        pVehicle->ForcedDespawn();
                }
            }

            m_docksSpawnsGuids[ownerIdx].clear();
            break;
        }
        case BG_IC_OBJECTIVE_HANGAR:
        {
            // reset the animations and depswn the portals
            for (const auto& guid : m_hangarPortalsGuids[ownerIdx])
            {
                if (GameObject* pPortal = GetBgMap()->GetGameObject(guid))
                    pPortal->SetLootState(GO_JUST_DEACTIVATED);
            }

            for (const auto& guid : m_hangarAnimGuids[ownerIdx])
            {
                if (GameObject* pAnim = GetBgMap()->GetGameObject(guid))
                {
                    pAnim->ResetDoorOrButton();
                    pAnim->SetLootState(GO_JUST_DEACTIVATED);
                }
            }
            break;
        }
        case BG_IC_OBJECTIVE_REFINERY:
        case BG_IC_OBJECTIVE_QUARY:
        {
            // remove spell aura
            // ToDo: remove vehicle auras
            for (BattleGroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
            {
                if (Player* plr = sObjectMgr.GetPlayer(itr->first))
                {
                    if (plr->GetTeam() == GetTeamIdByTeamIndex(ownerIdx))
                        plr->RemoveAurasDueToSpell(isleObjectiveData[objective].spellEntry);
                }
            }
            break;
        }
    }
}

void BattleGroundIC::Update(uint32 diff)
{
    BattleGround::Update(diff);

    // close keep gates
    if (m_closeDoorTimer)
    {
        if (m_closeDoorTimer <= diff)
        {
            //  reset alliance and horde gates
            for (const auto& i : m_keepGatesGuid[TEAM_INDEX_ALLIANCE])
                if (GameObject* pGate = GetBgMap()->GetGameObject(i))
                    pGate->ResetDoorOrButton();

            for (const auto& i : m_keepGatesGuid[TEAM_INDEX_HORDE])
                if (GameObject* pGate = GetBgMap()->GetGameObject(i))
                    pGate->ResetDoorOrButton();

            m_closeDoorTimer = 0;
        }
        else
            m_closeDoorTimer -= diff;
    }

    // objective timers
    {
        for (uint8 i = 0; i < BG_IC_MAX_OBJECTIVES; ++i)
        {
            if (m_objectiveTimer[i])
            {
                if (m_objectiveTimer[i] <= diff)
                {
                    DoCaptureObjective(IsleObjective(i));
                    m_objectiveTimer[i] = 0;
                }
                else
                    m_objectiveTimer[i] -= diff;
            }
        }
    }
}

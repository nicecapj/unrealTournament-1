// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTReplicatedLoadoutInfo.h"
#include "UTGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTeamSideSwapDelegate, uint8, Offset);

class AUTGameMode;
class AUTReplicatedMapInfo;

struct FLoadoutInfo;

UCLASS(Config = Game)
class UNREALTOURNAMENT_API AUTGameState : public AGameState
{
	GENERATED_UCLASS_BODY()

	/** server settings */
	UPROPERTY(Replicated, GlobalConfig, EditAnywhere, BlueprintReadWrite, Category = ServerInfo, replicatedUsing = OnRep_ServerName)
	FString ServerName;
	
	// The message of the day
	UPROPERTY(Replicated, GlobalConfig, EditAnywhere, BlueprintReadWrite, Category = ServerInfo, replicatedUsing = OnRep_ServerMOTD)
	FString ServerMOTD;

	UFUNCTION()
	virtual void OnRep_ServerName();

	UFUNCTION()
	virtual void OnRep_ServerMOTD();

	// A quick string field for the scoreboard and other browsers that contains description of the server
	UPROPERTY(Replicated, GlobalConfig, EditAnywhere, BlueprintReadWrite, Category = ServerInfo)
	FString ServerDescription;

	/** teams, if the game type has them */
	UPROPERTY(BlueprintReadOnly, Category = GameState)
	TArray<AUTTeamInfo*> Teams;

	/** If TRUE, then we weapon pick ups to stay on their base */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = GameState)
	uint32 bWeaponStay:1;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = GameState)
	uint32 bTeamGame : 1;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = GameState)
	uint32 bRankedSession : 1;

	/** True if players are allowed to switch teams (if team game). */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = GameState)
	uint32 bAllowTeamSwitches : 1;

	/** If true, we will stop the game clock */
	UPROPERTY(Replicated, BlueprintReadWrite, Category = GameState)
	uint32 bStopGameClock : 1;

	/**If enabled, the server grants special control for casters*/
	UPROPERTY(Replicated, BlueprintReadOnly, Category = GameState)
	uint32 bCasterControl : 1;

	/**If true, had to force balance teams. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = GameState)
	uint32 bForcedBalance : 1;
	
	/** If true, the intro cinematic will play just before the countdown to begin */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = GameState)
	uint32 bPlayPlayerIntro : 1;

	/** If true, kill icon messages persist through a round/ */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameState)
		uint32 bPersistentKillIconMessages : 1;

	/** If a single player's (or team's) score hits this limited, the game is over */
	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadOnly, Category = GameState)
	int32 GoalScore;

	/** The maximum amount of time the game will be */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = GameState)
	int32 TimeLimit;

	/** amount of time after a player spawns where they are immune to damage from enemies */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = GameState)
	float SpawnProtectionTime;

	/** Number of winners to display in EOM summary. */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = GameState)
		uint8 NumWinnersToShow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameState)
	TSubclassOf<UUTLocalMessage> MultiKillMessageClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameState)
	TSubclassOf<UUTLocalMessage> SpreeMessageClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameState)
		FText GoalScoreText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameState)
		FText GameOverStatus;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameState)
		FText MapVoteStatus;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameState)
		FText PreGameStatus;

	/** amount of time between kills to qualify as a multikill */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameState)
	float MultiKillDelay;

	// Used to sync the time on clients to the server. -- See DefaultTimer()
	UPROPERTY(Replicated)
	int32 RemainingMinute;

	// Tell clients if more players are needed before match starts
	UPROPERTY(Replicated)
	int32 PlayersNeeded;

	/** How much time is remaining in this match. */
	UPROPERTY(Replicated, ReplicatedUsing = OnRep_RemainingTime, BlueprintReadOnly, Category = GameState)
	int32 RemainingTime;

	/** local world time that game ended (i.e. relative to World->TimeSeconds) */
	UPROPERTY(BlueprintReadOnly, Category = GameState)
	float MatchEndTime;

	UFUNCTION()
	virtual void OnRep_RemainingTime();

	/** Returns time in seconds that should be displayed on game clock. */
	virtual float GetClockTime();

	// How long must a player wait before respawning
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = GameState)
	float RespawnWaitTime;

	// How long a player can wait before being forced respawned.  Set to 0 for no delay.
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = GameState)
	float ForceRespawnTime;

	/** offset to level placed team IDs for the purposes of swapping/rotating sides
	 * i.e. if this value is 1 and there are 4 teams, team 0 objects become owned by team 1, team 1 objects become owned by team 2... team 3 objects become owned by team 0
	 */
	UPROPERTY(ReplicatedUsing = OnTeamSideSwap, BlueprintReadOnly, Category = GameState)
	uint8 TeamSwapSidesOffset;
	/** previous value, so we know how much we're changing by */
	UPROPERTY()
	uint8 PrevTeamSwapSidesOffset;

	/** changes team sides; generally offset should be 1 unless it's a 3+ team game and you want to rotate more than one spot */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = GameState)
	virtual void ChangeTeamSides(uint8 Offset = 1);

	UFUNCTION()
	virtual void OnTeamSideSwap();

	UPROPERTY(BlueprintAssignable)
	FTeamSideSwapDelegate TeamSideSwapDelegate;

	UPROPERTY(Replicated, BlueprintReadOnly, ReplicatedUsing = OnWinnerReceived, Category = GameState)
	AUTPlayerState* WinnerPlayerState;

	/** Holds the team of the winning team */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = GameState)
	AUTTeamInfo* WinningTeam;

	UFUNCTION()
	virtual void OnWinnerReceived();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = GameState)
	virtual void SetTimeLimit(int32 NewTimeLimit);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = GameState)
	virtual void SetGoalScore(int32 NewGoalScore);

	UFUNCTION()
	virtual void SetWinner(AUTPlayerState* NewWinner);

	/** Called once per second (or so depending on TimeDilation) after RemainingTime() has been replicated */
	virtual void DefaultTimer();

	/** called to check for time message broadcasts ("3 minutes remain", etc) */
	virtual void CheckTimerMessage();

	/** Determines if a player is on the same team */
	UFUNCTION(BlueprintCallable, Category = GameState)
	virtual bool OnSameTeam(const AActor* Actor1, const AActor* Actor2);

	/** Determines if 2 PlayerStates are in score order */
	virtual bool InOrder( class AUTPlayerState* P1, class AUTPlayerState* P2 );

	/** Sorts the Player State Array */
	virtual void SortPRIArray();

	/** Returns true if the match state is InProgress or later */
	UFUNCTION(BlueprintCallable, Category = GameState)
	virtual bool HasMatchStarted() const;

	UFUNCTION(BlueprintCallable, Category = GameState)
	virtual bool IsMatchInProgress() const;

	UFUNCTION(BlueprintCallable, Category = GameState)
	virtual bool IsMatchInOvertime() const;

	UFUNCTION(BlueprintCallable, Category = GameState)
	virtual bool IsMatchInCountdown() const;

	UFUNCTION(BlueprintCallable, Category = GameState)
	virtual bool IsMatchIntermission() const;

	virtual void BeginPlay() override;

	/** Return largest SpectatingId value in current PlayerArray. */
	virtual int32 GetMaxSpectatingId();

	/** Return largest SpectatingIdTeam value in current PlayerArray. */
	virtual int32 GetMaxTeamSpectatingId(int32 TeamNum);

	/** add an overlay to the OverlayMaterials list */
	UFUNCTION(Meta = (DeprecatedFunction, DeprecationMessage = "Use AddOverlayEffect"), BlueprintCallable, BlueprintAuthorityOnly, Category = Effects)
	virtual void AddOverlayMaterial(UMaterialInterface* NewOverlay, UMaterialInterface* NewOverlay1P = NULL);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = Effects)
	virtual void AddOverlayEffect(const FOverlayEffect& NewOverlay, const FOverlayEffect& NewOverlay1P
#if CPP // UHT is dumb
	= FOverlayEffect()
#endif
	);
	/** find an overlay in the OverlayMaterials list, return its index */
	int32 FindOverlayMaterial(UMaterialInterface* TestOverlay) const
	{
		for (int32 i = 0; i < ARRAY_COUNT(OverlayEffects); i++)
		{
			if (OverlayEffects[i].Material == TestOverlay)
			{
				return i;
			}
		}
		return INDEX_NONE;
	}
	int32 FindOverlayEffect(const FOverlayEffect& TestEffect) const
	{
		for (int32 i = 0; i < ARRAY_COUNT(OverlayEffects); i++)
		{
			if (OverlayEffects[i] == TestEffect)
			{
				return i;
			}
		}
		return INDEX_NONE;
	}
	/** get overlay material from index */
	FOverlayEffect GetOverlayMaterial(int32 Index, bool bFirstPerson)
	{
		if (Index >= 0 && Index < ARRAY_COUNT(OverlayEffects))
		{
			return (bFirstPerson && OverlayEffects1P[Index].IsValid()) ? OverlayEffects1P[Index] : OverlayEffects[Index];
		}
		else
		{
			return FOverlayEffect();
		}
	}
	/** returns first active overlay material given the passed in flags */
	FOverlayEffect GetFirstOverlay(uint16 Flags, bool bFirstPerson)
	{
		// early out
		if (Flags == 0)
		{
			return FOverlayEffect();
		}
		else
		{
			for (int32 i = 0; i < ARRAY_COUNT(OverlayEffects); i++)
			{
				if (Flags & (1 << i))
				{
					return (bFirstPerson && OverlayEffects1P[i].IsValid()) ? OverlayEffects1P[i] : OverlayEffects[i];
				}
			}
			return FOverlayEffect();
		}
	}

	/**
	 *	This is called from the UTPlayerCameraManage to allow the game to force an override to the current player camera to make it easier for
	 *  Presentation to be controlled by the server.
	 **/
	
	virtual FName OverrideCameraStyle(APlayerController* PCOwner, FName CurrentCameraStyle);

	// Returns the rules for this server.
	virtual FText ServerRules();

	/** used on clients to know when all TeamInfos have been received */
	UPROPERTY(Replicated)
	uint8 NumTeams;

	virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;

	virtual void ReceivedGameModeClass() override;

	virtual FText GetGameStatusText();

	virtual void OnRep_MatchState() override;

	virtual void AddPlayerState(class APlayerState* PlayerState) override;

	/** rearrange any players' SpectatingID so that the list of values is continuous starting from 1
	 * generally should not be called during gameplay as reshuffling this list unnecessarily defeats the point
	 */
	virtual void CompactSpectatingIDs();

	UPROPERTY()
	FName SecondaryAttackerStat;

protected:
	static const uint8 MAX_OVERLAY_MATERIALS = 16;
	/** overlay materials, mapped to bits in UTCharacter's OverlayFlags/WeaponOverlayFlags and used to efficiently handle character/weapon overlays
	 * only replicated at startup so set any used materials via BeginPlay()
	 */
	UPROPERTY(ReplicatedUsing = OnRep_OverlayEffects)
	FOverlayEffect OverlayEffects[MAX_OVERLAY_MATERIALS];
	UPROPERTY(ReplicatedUsing = OnRep_OverlayEffects)
	FOverlayEffect OverlayEffects1P[MAX_OVERLAY_MATERIALS];

	virtual void HandleMatchHasEnded() override
	{
		MatchEndTime = GetWorld()->TimeSeconds;
	}

	UFUNCTION()
	virtual void OnRep_OverlayEffects();

public:
	// Will be true if this is an instanced server from a lobby.
	UPROPERTY(Replicated)
	bool bIsInstanceServer;

	// the GUID of the hub the player should return when they leave.  
	UPROPERTY(Replicated)
	FGuid HubGuid;

	// Holds a list of weapons available for loadouts
	UPROPERTY(Replicated)
	TArray<AUTReplicatedLoadoutInfo*> AvailableLoadout;

	// Adds a weapon to the list of possible loadout weapons.
	virtual void AddLoadoutItem(const FLoadoutInfo& Loadout);

	// Finds a loadout item
	AUTReplicatedLoadoutInfo* FindLoadoutItem(const FName& ItemTag);

	// Adjusts the cost of a weapon available for loadouts
	virtual void AdjustLoadoutCost(TSubclassOf<AUTInventory> ItemClass, float NewCost);

	/** Game specific rating of a player as a desireable camera focus for spectators. */
	virtual float ScoreCameraView(AUTPlayerState* InPS, AUTCharacter *Character)
	{
		return 0.f;
	};

public:
	// These IDs are banned for the remainder of the match
	TArray<TSharedPtr<const FUniqueNetId>> TempBans;

	// Returns true if this player has been temp banned from this server/instance
	bool IsTempBanned(const TSharedPtr<const FUniqueNetId>& UniqueId);

	// Registers a vote for temp banning a player.  If the player goes above the threashhold, they will be banned for the remainder of the match
	void VoteForTempBan(AUTPlayerState* BadGuy, AUTPlayerState* Voter);

	UPROPERTY(Config)
	float KickThreshold;

	/** Returns which team side InActor is closest to.   255 = no team. */
	virtual uint8 NearestTeamSide(AActor* InActor)
	{
		return 255;
	};

	// Used to get a list of game modes and maps that can be choosen from the menu.  Typically, this just pulls all of 
	// available local content however, in hubs it will be from data replicated from the server.
	virtual void GetAvailableGameData(TArray<UClass*>& GameModes, TArray<UClass*>& MutatorList);

	virtual void ScanForMaps(const TArray<FString>& AllowedMapPrefixes, TArray<FAssetData>& MapList);

	// Create a replicated map info from a map's asset registry data
	virtual AUTReplicatedMapInfo* CreateMapInfo(const FAssetData& MapAsset);

	/** Used to translate replicated FName refs to highlights into text. */
	TMap< FName, FText> HighlightMap;

	/** Used to translate replicated FName refs to highlights into text. */
	TMap< FName, FText> ShortHighlightMap;

	/** Used to prioritize which highlights to show (lower value = higher priority). */
	TMap< FName, float> HighlightPriority;

	/** Clear highlights array. */
	UFUNCTION(BlueprintCallable, Category = "Game")
		virtual void ClearHighlights();

	virtual void UpdateMatchHighlights();

	/** On server side - generate a list of highlights for each player.  Every UTPlayerStates' MatchHighlights array will have been cleared when this is called. */
	UFUNCTION(BlueprintNativeEvent, Category = "Game")
		void UpdateHighlights();

	/** On client side, returns an array of text based on the PlayerStates Highlights. */
	UFUNCTION(BlueprintNativeEvent, Category = "Game")
		TArray<FText> GetPlayerHighlights(AUTPlayerState* PlayerState);

	/** After all major highlights added, fill in some minor ones if there is space left. */
	UFUNCTION(BlueprintNativeEvent, Category = "Game")
		void AddMinorHighlights(AUTPlayerState* PS);

	virtual FText FormatPlayerHighlightText(AUTPlayerState* PS, int32 Index);

	/** Return short version of top highlight for that player. */
	virtual FText ShortPlayerHighlightText(AUTPlayerState* PS);

	/** Return a score value for the "impressiveness" of the Match highlights for PS. */
	virtual float MatchHighlightScore(AUTPlayerState* PS);
	
	UPROPERTY()
		TArray<FName> GameScoreStats;

	UPROPERTY()
		TArray<FName> TeamStats;

	UPROPERTY()
		TArray<FName> WeaponStats;

	UPROPERTY()
		TArray<FName> RewardStats;

	UPROPERTY()
		TArray<FName> MovementStats;

	UPROPERTY(Replicated)
	TArray<AUTReplicatedMapInfo*> MapVoteList;

	virtual void CreateMapVoteInfo(const FString& MapPackage,const FString& MapTitle, const FString& MapScreenshotReference);
	void SortVotes();

	// The # of seconds left for voting for a map.
	UPROPERTY(Replicated)
	int32 VoteTimer;

	/** Returns a list of important pickups for this gametype
	*	Used to gather pickups for the spectator slideout
	*	For now, do gamytype specific team sorting here
	*   NOTE: return value is a workaround for blueprint bugs involving ref parameters and is not used
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = GameState)
	bool GetImportantPickups(UPARAM(ref) TArray<class AUTPickup*>& PickupList);

	/**
	 *	The server will replicate it's session id.  Clients, upon recieving it, will check to make sure they are in that session
	 *  and if not, add themselves.  If at all possible, clients should add themselves to the session before joining a server
	 *  however there are times where that isn't possible (join via IP) and this will act as a catch all.
	 **/
	UPROPERTY(Replicated, ReplicatedUsing = OnRep_ServerSessionId)
	FString ServerSessionId;

protected:
	UFUNCTION()
	virtual void OnRep_ServerSessionId();


	/** map of additional stats to hold match total stats*/
	TMap< FName, float > StatsData;

public:
	UPROPERTY()
	float LastScoreStatsUpdateTime;

	/** Accessors for StatsData. */
	float GetStatsValue(FName StatsName);
	void SetStatsValue(FName StatsName, float NewValue);
	void ModifyStatsValue(FName StatsName, float Change);

	/** Returns true if all players are ready */
	UFUNCTION(BlueprintCallable, Category = GameState)
	bool AreAllPlayersReady();

	/** returns whether the player can choose to spawn at the passed in start point (for game modes that allow players to pick)
	 * valid on both client and server
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = GameState)
	bool IsAllowedSpawnPoint(AUTPlayerState* Chooser, APlayerStart* DesiredStart) const;


	/** Current index to use as basis for next selection in Taunt list. */
	UPROPERTY()
		int32 TauntSelectionIndex;

	virtual void FillOutRconPlayerList(TArray<FRconPlayerData>& PlayerList);

	/** hook for blueprints */
	UFUNCTION(BlueprintCallable, Category = GameState)
	TSubclassOf<AGameMode> GetGameModeClass() const
	{
		return GameModeClass;
	}

	virtual void MakeJsonReport(TSharedPtr<FJsonObject> JsonObject);

	UPROPERTY(Replicated)
	TArray<FLoadoutPackReplicatedInfo> SpawnPacks;
};




// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTTeamGameMode.h"
#include "UTTeamInfo.h"
#include "UTTeamPlayerStart.h"
#include "SlateBasics.h"
#include "UTAnalytics.h"
#include "UTGameMessage.h"
#include "UTCTFGameMessage.h"
#include "UTCTFMajorMessage.h"
#include "UTCTFRewardMessage.h"
#include "SUWindowsStyle.h"
#include "SlateGameResources.h"
#include "SNumericEntryBox.h"
#include "StatNames.h"
#include "UTGameSessionRanked.h"
#include "UTBotCharacter.h"
#include "AnalyticsEventAttribute.h"
#include "IAnalyticsProvider.h"

UUTTeamInterface::UUTTeamInterface(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

AUTTeamGameMode::AUTTeamGameMode(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	NumTeams = 2;
	bBalanceTeams = true;
	new(TeamColors) FLinearColor(1.0f, 0.05f, 0.0f, 1.0f);
	new(TeamColors) FLinearColor(0.1f, 0.1f, 1.0f, 1.0f);
	new(TeamColors) FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);
	new(TeamColors) FLinearColor(1.0f, 1.0f, 0.0f, 1.0f);

	TeamNames.Add(NSLOCTEXT("UTTeamGameMode", "Team0Name", "Red"));
	TeamNames.Add(NSLOCTEXT("UTTeamGameMode", "Team1Name", "Blue"));
	TeamNames.Add(NSLOCTEXT("UTTeamGameMode"," Team2Name", "Green"));
	TeamNames.Add(NSLOCTEXT("UTTeamGameMode", "Team3Name", "Gold"));

	TeamMomentumPct = 0.75f;
	WallRunMomentumPct = 0.5f;
	bTeamGame = true;
	bHasBroadcastDominating = false;
	bAnnounceTeam = true;
	bHighScorerPerTeamBasis = true;
	ScoringPlaysDisplayTime = 6.f;
}

void AUTTeamGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	bBalanceTeams = !bDevServer && !bOfflineChallenge && EvalBoolOptions(UGameplayStatics::ParseOption(Options, TEXT("BalanceTeams")), bBalanceTeams);

	if (bAllowURLTeamCountOverride)
	{
		NumTeams = UGameplayStatics::GetIntOption(Options, TEXT("NumTeams"), NumTeams);
	}
	NumTeams = FMath::Max<uint8>(NumTeams, 2);

	if (TeamClass == NULL)
	{
		TeamClass = AUTTeamInfo::StaticClass();
	}
	for (uint8 i = 0; i < NumTeams; i++)
	{
		AUTTeamInfo* NewTeam = GetWorld()->SpawnActor<AUTTeamInfo>(TeamClass);
		NewTeam->TeamIndex = i;
		if (TeamColors.IsValidIndex(i))
		{
			NewTeam->TeamColor = TeamColors[i];
		}

		if (TeamNames.IsValidIndex(i))
		{
			NewTeam->TeamName = TeamNames[i];
		}

		Teams.Add(NewTeam);
		checkSlow(Teams[i] == NewTeam);
	}

	MercyScore = FMath::Max(0, UGameplayStatics::GetIntOption(Options, TEXT("MercyScore"), MercyScore));
}

void AUTTeamGameMode::InitGameState()
{
	Super::InitGameState();
	Cast<AUTGameState>(GameState)->Teams = Teams;
}

void AUTTeamGameMode::AnnounceMatchStart()
{
	if (bAnnounceTeam)
	{
		for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			AUTPlayerController* NextPlayer = Cast<AUTPlayerController>(*Iterator);
			AUTTeamInfo* Team = (NextPlayer && Cast<AUTPlayerState>(NextPlayer->PlayerState)) ? Cast<AUTPlayerState>(NextPlayer->PlayerState)->Team : NULL;
			if (Team)
			{
				int32 Switch = (Team->TeamIndex == 0) ? 9 : 10;
				NextPlayer->ClientReceiveLocalizedMessage(UUTGameMessage::StaticClass(), Switch, NextPlayer->PlayerState, NULL, NULL);
			}
		}
	}
	else
	{
		Super::AnnounceMatchStart();
	}
}

APlayerController* AUTTeamGameMode::Login(class UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal, const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	APlayerController* PC = Super::Login(NewPlayer, InRemoteRole, Portal, Options, UniqueId, ErrorMessage);

	if (PC != NULL && !PC->PlayerState->bOnlySpectator)
	{
		if (!bRankedSession && !bIsQuickMatch)
		{
			// FIXMESTEVE Does team get overwritten in postlogin if inactive player?
			uint8 DesiredTeam = (GetNetMode() == NM_Standalone) ? 1 : uint8(FMath::Clamp<int32>(UGameplayStatics::GetIntOption(Options, TEXT("Team"), 255), 0, 255));
			ChangeTeam(PC, DesiredTeam, false);
		}
		else
		{
			uint8 DesiredTeam = 0;
			AUTGameSessionRanked* UTGameSession = Cast<AUTGameSessionRanked>(GameSession);
			if (UTGameSession)
			{
				DesiredTeam = UTGameSession->GetTeamForPlayer(UniqueId);
			}
			ChangeTeam(PC, DesiredTeam, false);
		}
	}

	return PC;
}

bool AUTTeamGameMode::PlayerWonChallenge()
{
	AUTTeamInfo* BestTeam = NULL;
	bool bTied = false;
	for (int32 i = 0; i < UTGameState->Teams.Num(); i++)
	{
		if (UTGameState->Teams[i] != NULL)
		{
			if (BestTeam == NULL || UTGameState->Teams[i]->Score > BestTeam->Score)
			{
				BestTeam = UTGameState->Teams[i];
				bTied = false;
			}
			else if (UTGameState->Teams[i]->Score == BestTeam->Score)
			{
				bTied = true;
			}
		}
	}

	if (bTied || !BestTeam)
	{
		return false;
	}

	// make sure player is on best team
	APlayerController* LocalPC = GEngine->GetFirstLocalPlayerController(GetWorld());
	AUTPlayerState* PS = LocalPC ? Cast<AUTPlayerState>(LocalPC->PlayerState) : NULL;
	return PS && PS->Team && (PS->Team == BestTeam);
}

bool AUTTeamGameMode::ShouldBalanceTeams(bool bInitialTeam) const
{
	return !bRankedSession && bBalanceTeams && (!bInitialTeam || HasMatchStarted() || GetMatchState() == MatchState::CountdownToBegin);
}

bool AUTTeamGameMode::ChangeTeam(AController* Player, uint8 NewTeam, bool bBroadcast)
{
	if (Player == NULL)
	{
		return false;
	}
	else
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(Player->PlayerState);
		if (PS == NULL || PS->bOnlySpectator)
		{
			return false;
		}
		else
		{
			if ((bOfflineChallenge || bBasicTrainingGame) && PS->Team)
			{
				return false;
			}

			bool bForceTeam = false;
			if (!Teams.IsValidIndex(NewTeam))
			{
				bForceTeam = true;
			}
			else
			{
				// see if someone is willing to switch
				for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
				{
					AUTPlayerController* NextPlayer = Cast<AUTPlayerController>(*Iterator);
					AUTPlayerState* SwitchingPS = NextPlayer ? Cast<AUTPlayerState>(NextPlayer->PlayerState) : NULL;
					if (SwitchingPS && SwitchingPS->bPendingTeamSwitch && (SwitchingPS->Team == Teams[NewTeam]) && Teams.IsValidIndex(1-NewTeam))
					{
						// Found someone who wants to leave team, so just replace them
						MovePlayerToTeam(NextPlayer, SwitchingPS, 1 - NewTeam);
						SwitchingPS->HandleTeamChanged(NextPlayer);
						MovePlayerToTeam(Player, PS, NewTeam);
						return true;
					}
				}

				if (ShouldBalanceTeams(PS->Team == NULL))
				{
					for (int32 i = 0; i < Teams.Num(); i++)
					{
						// don't allow switching to a team with more players, or equal players if the player is on a team now
						if (i != NewTeam && Teams[i]->GetNumHumans() - ((PS->Team != NULL && PS->Team->TeamIndex == i) ? 1 : 0)  < Teams[NewTeam]->GetNumHumans())
						{
							bForceTeam = true;
							break;
						}
					}
				}
			}
			if (bForceTeam)
			{
				NewTeam = PickBalancedTeam(PS, NewTeam);
			}
		
			if (MovePlayerToTeam(Player, PS, NewTeam))
			{
				AUTPlayerController* PC = Cast<AUTPlayerController>(Player);
				if (PC && !HasMatchStarted() && bUseTeamStarts)
				{
					AActor* const StartSpot = FindPlayerStart(PC);
					if (StartSpot != NULL)
					{
						PC->StartSpot = StartSpot;
						PC->ViewStartSpot();
					}
				}
				return true;
			}

			PS->bPendingTeamSwitch = true;
			PS->ForceNetUpdate();
			return false;
		}
	}
}

bool AUTTeamGameMode::MovePlayerToTeam(AController* Player, AUTPlayerState* PS, uint8 NewTeam)
{
	if (Teams.IsValidIndex(NewTeam) && (PS->Team == NULL || PS->Team->TeamIndex != NewTeam))
	{
		//Make sure we kill the player before they switch sides so the correct team loses the point
		AUTCharacter* UTC = Cast<AUTCharacter>(Player->GetPawn());
		if (UTC != nullptr)
		{
			UTC->PlayerSuicide();
		}

		if (PS->Team != NULL)
		{
			PS->Team->RemoveFromTeam(Player);
		}
		Teams[NewTeam]->AddToTeam(Player);
		PS->bPendingTeamSwitch = false;
		PS->ForceNetUpdate();

		// Clear the player's gameplay mute list.

		APlayerController* PlayerController = Cast<APlayerController>(Player);
		AUTGameState* MyGameState = GetWorld()->GetGameState<AUTGameState>();

		if (PlayerController && MyGameState)
		{
			for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				AUTPlayerController* NextPlayer = Cast<AUTPlayerController>(*Iterator);
				if (NextPlayer)
				{
					TSharedPtr<const FUniqueNetId> Id = NextPlayer->PlayerState->UniqueId.GetUniqueNetId();
					bool bIsMuted = Id.IsValid() && PlayerController->IsPlayerMuted(Id.ToSharedRef().Get());

					bool bOnSameTeam = MyGameState->OnSameTeam(PlayerController, NextPlayer);
					if (bIsMuted && bOnSameTeam) 
					{
						PlayerController->GameplayUnmutePlayer(NextPlayer->PlayerState->UniqueId);
						NextPlayer->GameplayUnmutePlayer(PlayerController->PlayerState->UniqueId);
					}
					if (!bIsMuted && !bOnSameTeam) 
					{
						PlayerController->GameplayMutePlayer(NextPlayer->PlayerState->UniqueId);
						NextPlayer->GameplayMutePlayer(PlayerController->PlayerState->UniqueId);
					}
					
				}
			}
		}

		return true;
	}
	return false;
}

uint8 AUTTeamGameMode::PickBalancedTeam(AUTPlayerState* PS, uint8 RequestedTeam)
{
	TArray< AUTTeamInfo*, TInlineAllocator<4> > BestTeams;
	int32 BestSize = -1;

	for (int32 i = 0; i < Teams.Num(); i++)
	{
		int32 TestSize = Teams[i]->GetSize();
		if (Teams[i] == PS->Team)
		{
			// player will be leaving this team so count its size as post-departure
			TestSize--;
		}
		if (BestTeams.Num() == 0 || TestSize < BestSize)
		{
			BestTeams.Empty();
			BestTeams.Add(Teams[i]);
			BestSize = TestSize;
		}
		else if (TestSize == BestSize)
		{
			BestTeams.Add(Teams[i]);
		}
	}

	// if in doubt choose team with bots on it as the bots will leave if necessary to balance
	{
		TArray< AUTTeamInfo*, TInlineAllocator<4> > TeamsWithBots;
		for (AUTTeamInfo* TestTeam : BestTeams)
		{
			bool bHasBots = false;
			TArray<AController*> Members = TestTeam->GetTeamMembers();
			for (AController* C : Members)
			{
				if (Cast<AUTBot>(C) != NULL)
				{
					bHasBots = true;
					break;
				}
			}
			if (bHasBots)
			{
				TeamsWithBots.Add(TestTeam);
			}
		}
		if (TeamsWithBots.Num() > 0)
		{
			for (int32 i = 0; i < TeamsWithBots.Num(); i++)
			{
				if (TeamsWithBots[i]->TeamIndex == RequestedTeam)
				{
					return RequestedTeam;
				}
			}

			return TeamsWithBots[FMath::RandHelper(TeamsWithBots.Num())]->TeamIndex;
		}
	}

	// if match is in progress, try to put on lower score team
	AUTTeamInfo* WorstTeam = nullptr;
	if (IsMatchInProgress())
	{
		int32 WorstScore = 0;
		for (AUTTeamInfo* TestTeam : BestTeams)
		{
			if (!WorstTeam || (TestTeam->Score < WorstScore))
			{
				WorstTeam = TestTeam;
				WorstScore = TestTeam->Score;
			}
			else if (WorstTeam && (TestTeam->Score == WorstScore))
			{
				WorstTeam = nullptr;
				break;
			}
		}
	}
	if (WorstTeam)
	{
		return WorstTeam->TeamIndex;
	}

	// Balance by Elo if no request and teams are same size 
	int32 WorstElo = 0;
	int32 AverageElo = 0;
	WorstTeam = nullptr;
	for (AUTTeamInfo* TestTeam : BestTeams)
	{
		int32 TeamElo = TestTeam->AverageEloFor(this);
		if (!WorstTeam || (TeamElo < WorstElo))
		{
			WorstTeam = TestTeam;
			WorstElo = TeamElo;
		}
		AverageElo += TeamElo;
	}
	AverageElo = (BestTeams.Num() > 0) ? AverageElo / BestTeams.Num() : AverageElo;

	// If match in progress, put on worst Elo team.  before match, try to improve Elo average
	if (WorstTeam)
	{
		if (IsMatchInProgress())
		{
			return WorstTeam->TeamIndex;
		}
		else if (BestSize > 0)
		{
			int32 NewElo = GetEloFor(PS, bRankedSession);
			if (IsValidElo(PS, bRankedSession) && NewElo > AverageElo)
			{
				return WorstTeam->TeamIndex;
			}
			for (AUTTeamInfo* TestTeam : BestTeams)
			{
				if (TestTeam != WorstTeam)
				{
					return TestTeam->TeamIndex;
				}
			}
		}
	}

	for (int32 i = 0; i < BestTeams.Num(); i++)
	{
		if (BestTeams[i]->TeamIndex == RequestedTeam)
		{
			return RequestedTeam;
		}
	}

	return BestTeams[FMath::RandHelper(BestTeams.Num())]->TeamIndex;
}

void AUTTeamGameMode::HandlePlayerIntro()
{
	// we ignore balancing when applying players' URL specified value during prematch
	// make sure we're balanced now before the game begins
	if (bBalanceTeams)
	{
		TArray<AUTTeamInfo*> SortedTeams = UTGameState->Teams;
		SortedTeams.Sort([](AUTTeamInfo& A, AUTTeamInfo& B) { return A.GetSize() > B.GetSize(); });
		for (int32 i = 0; i < SortedTeams.Num() - 1; i++)
		{
			if (SortedTeams[i]->GetSize() > 1)
			{
				// sort players on this team by Elo
				for (int32 j = i + 1; j < SortedTeams.Num(); j++)
				{
					while (SortedTeams[i]->GetSize() > SortedTeams[j]->GetSize() + 1)
					{
						// Calc team Elos, move player who will result in best team average Elo match.
						UTGameState->bForcedBalance = true;
						int32 SourceTeamElo = SortedTeams[i]->AverageEloFor(this);
						int32 EloDiff = SourceTeamElo - SortedTeams[j]->AverageEloFor(this);
						int32 SizeDiff = SortedTeams[i]->GetSize() - SortedTeams[j]->GetSize();
						int32 DesiredElo = SourceTeamElo + EloDiff*SortedTeams[j]->GetSize() / 2 * SizeDiff;
						AController* ToBeMoved = SortedTeams[i]->MemberClosestToElo(this, DesiredElo);
						if (!ChangeTeam(ToBeMoved, j) || UTGameState->OnSameTeam(ToBeMoved, SortedTeams[i]))
						{
							// abort if failed to actually change team so we don't end up in recursion
							break;
						}
						SortedTeams.Sort([](AUTTeamInfo& A, AUTTeamInfo& B) { return A.GetSize() > B.GetSize(); });
					}
				}
			}
		}
	}

	Super::HandlePlayerIntro();
}

UUTBotCharacter* AUTTeamGameMode::ChooseRandomCharacter(uint8 TeamNum)
{
	if (!Teams.IsValidIndex(TeamNum) || Teams.Num() < 2 || Teams[TeamNum] == nullptr || EligibleBots.Num() == 0)
	{
		return Super::ChooseRandomCharacter(TeamNum);
	}
	else
	{
		TArray<float> TeamAvgSkill;
		TeamAvgSkill.AddZeroed(Teams.Num());
		for (int32 i = 0; i < Teams.Num(); i++)
		{
			if (Teams[i] != nullptr)
			{
				int32 BotsOnTeam = 0;
				float Avg = 0.0f;
				for (AController* Member : Teams[i]->GetTeamMembers())
				{
					AUTBot* B = Cast<AUTBot>(Member);
					if (B != nullptr)
					{
						BotsOnTeam++;
						Avg += B->Skill;
					}
				}
				if (BotsOnTeam > 0)
				{
					Avg /= BotsOnTeam;
				}
			}
		}

		int32 ClosestTeam = INDEX_NONE;
		float ClosestSkillDiff = FLT_MAX;
		for (int32 i = 0; i < Teams.Num(); i++)
		{
			if (FMath::Abs<float>(TeamAvgSkill[i] - TeamAvgSkill[TeamNum]) < ClosestSkillDiff)
			{
				ClosestTeam = i;
				ClosestSkillDiff = TeamAvgSkill[i] - TeamAvgSkill[TeamNum];
			}
		}

		int32 BestMatch = 0;
		for (int32 i = 0; i < EligibleBots.Num(); i++)
		{
			if (EligibleBots[i]->Skill >= GameDifficulty)
			{
				BestMatch = i;
				break;
			}
		}
		int32 Index = FMath::Clamp(BestMatch + FMath::RandHelper(5) - 2, 0, EligibleBots.Num() - 1);
		// shift to bot with different skill to balance this team's average against the other teams
		if (ClosestSkillDiff < 0.0f)
		{
			while (Index > 0 && EligibleBots[Index]->Skill > TeamAvgSkill[TeamNum])
			{
				Index--;
			}
		}
		else
		{
			while (Index < EligibleBots.Num() - 1 && EligibleBots[Index]->Skill < TeamAvgSkill[TeamNum])
			{
				Index++;
			}
		}
		UUTBotCharacter* ChosenCharacter = EligibleBots[Index];
		EligibleBots.RemoveAt(Index);
		return ChosenCharacter;
	}
}

void AUTTeamGameMode::CheckBotCount()
{
	if (NumPlayers + NumBots > BotFillCount)
	{
		TArray<AUTTeamInfo*> SortedTeams = UTGameState->Teams;
		SortedTeams.Sort([](AUTTeamInfo& A, AUTTeamInfo& B) { return A.GetSize() > B.GetSize(); });

		// try to remove bots from team with the most players
		for (AUTTeamInfo* Team : SortedTeams)
		{
			bool bFound = false;
			TArray<AController*> Members = Team->GetTeamMembers();
			for (AController* C : Members)
			{
				AUTBotPlayer* B = Cast<AUTBotPlayer>(C);
				if (B != NULL)
				{
					if (AllowRemovingBot(B))
					{
						B->Destroy();
					}
					// note that we break from the loop on finding a bot even if we can't remove it yet, as it's still the best choice when it becomes available to remove (dies, etc)
					bFound = true;
					break;
				}
			}
			if (bFound)
			{
				break;
			}
		}
	}
	else while (NumPlayers + NumBots < BotFillCount)
	{
		AddBot();
	}
}

void AUTTeamGameMode::DefaultTimer()
{
	Super::DefaultTimer();
	UTGameState->bTeamProjHits = UTGameState->bTeamProjHits || (TeamDamagePct > 0.f); // @TODO FIXMESTEVE make TeamDamagePct protected so don't need to set here

	// check if bots should switch teams for balancing
	if (bBalanceTeams && NumBots > 0)
	{
		TArray<AUTTeamInfo*> SortedTeams = UTGameState->Teams;
		SortedTeams.Sort([](AUTTeamInfo& A, AUTTeamInfo& B) { return A.GetSize() > B.GetSize(); });

		for (int32 i = 1; i < SortedTeams.Num(); i++)
		{
			if (SortedTeams[i - 1]->GetSize() > SortedTeams[i]->GetSize() + 1)
			{
				TArray<AController*> Members = SortedTeams[i - 1]->GetTeamMembers();
				for (AController* C : Members)
				{
					AUTBot* B = Cast<AUTBot>(C);
					if (B != NULL && B->GetPawn() == NULL)
					{
						ChangeTeam(B, SortedTeams[i]->GetTeamNum(), true);
						break;
					}
				}
			}
		}
	}
}

void AUTTeamGameMode::BroadcastDeathMessage(AController* Killer, AController* Other, TSubclassOf<UDamageType> DamageType)
{
	TSubclassOf<UUTDamageType> UTDamage(*DamageType);
	if (Killer && UTDamage && UTDamage.GetDefaultObject()->bCausedByWorld && !IsEnemy(Killer, Other) && (TeamDamagePct == 0.f))
	{
		// Don't show "killer" in death message if death was actually from environmental damage
		Killer = NULL;
	}
	Super::BroadcastDeathMessage(Killer, Other, DamageType);
}

bool AUTTeamGameMode::ModifyDamage_Implementation(int32& Damage, FVector& Momentum, APawn* Injured, AController* InstigatedBy, const FHitResult& HitInfo, AActor* DamageCauser, TSubclassOf<UDamageType> DamageType)
{
	if (InstigatedBy != NULL && InstigatedBy != Injured->Controller && Cast<AUTGameState>(GameState)->OnSameTeam(Injured, InstigatedBy))
	{
		Damage *= TeamDamagePct;
		Momentum *= TeamMomentumPct;
		AUTCharacter* InjuredChar = Cast<AUTCharacter>(Injured);
		if (InjuredChar && InjuredChar->bApplyWallSlide)
		{
			Momentum *= WallRunMomentumPct;
		}
		AUTPlayerController* InstigatorPC = Cast<AUTPlayerController>(InstigatedBy);
		if (InstigatorPC && Cast<AUTPlayerState>(Injured->PlayerState))
		{
			((AUTPlayerState *)(Injured->PlayerState))->AnnounceSameTeam(InstigatorPC);
		}
	}
	Super::ModifyDamage_Implementation(Damage, Momentum, Injured, InstigatedBy, HitInfo, DamageCauser, DamageType);
	return true;
}

float AUTTeamGameMode::RatePlayerStart(APlayerStart* P, AController* Player)
{
	float Result = Super::RatePlayerStart(P, Player);
	if (bUseTeamStarts && Player != NULL)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(Player->PlayerState);
		if (AvoidPlayerStart(Cast<AUTPlayerStart>(P)) || (PS != NULL && PS->Team != NULL && (Cast<AUTTeamPlayerStart>(P) == NULL || ((AUTTeamPlayerStart*)P)->TeamNum != PS->Team->TeamIndex)))
		{
			// never ever use wrong team playerstart
			Result = -20.f;
		}
	}
	return Result;
}

bool AUTTeamGameMode::AvoidPlayerStart(AUTPlayerStart* P)
{
	return P && (!bUseTeamStarts && P->bIgnoreInNonTeamGame);
}

bool AUTTeamGameMode::CheckScore_Implementation(AUTPlayerState* Scorer)
{
	AUTTeamInfo* WinningTeam = NULL;

	if (MercyScore > 0)
	{
		int32 Spread = Scorer->Team->Score;
		for (AUTTeamInfo* OtherTeam : Teams)
		{
			if (OtherTeam != Scorer->Team)
			{
				Spread = FMath::Min<int32>(Spread, Scorer->Team->Score - OtherTeam->Score);
			}
		}
		if (Spread >= MercyScore)
		{
			EndGame(Scorer, FName(TEXT("MercyScore")));
			return true;
		}
	}

	// Unlimited play
	if (GoalScore <= 0)
	{
		return false;
	}

	for (int32 i = 0; i < Teams.Num(); i++)
	{
		if (Teams[i]->Score >= GoalScore)
		{
			WinningTeam = Teams[i];
			break;
		}
	}

	if (WinningTeam != NULL)
	{
		AUTPlayerState* BestPlayer = FindBestPlayerOnTeam(WinningTeam->GetTeamNum());
		if (BestPlayer == NULL) BestPlayer = Scorer;
		EndGame(BestPlayer, TEXT("fraglimit")); 
		return true;
	}
	else
	{
		return false;
	}
}

void AUTTeamGameMode::CreateGameURLOptions(TArray<TSharedPtr<TAttributePropertyBase>>& MenuProps)
{
	Super::CreateGameURLOptions(MenuProps);
	MenuProps.Add(MakeShareable(new TAttributePropertyBool(this, &bBalanceTeams, TEXT("BalanceTeams"))));
}

#if !UE_SERVER
void AUTTeamGameMode::CreateConfigWidgets(TSharedPtr<class SVerticalBox> MenuSpace, bool bCreateReadOnly, TArray< TSharedPtr<TAttributePropertyBase> >& ConfigProps, int32 MinimumPlayers)
{
	Super::CreateConfigWidgets(MenuSpace, bCreateReadOnly, ConfigProps, MinimumPlayers);

	TSharedPtr< TAttributePropertyBool > BalanceTeamsAttr = StaticCastSharedPtr<TAttributePropertyBool>(FindGameURLOption(ConfigProps, TEXT("BalanceTeams")));

	if (BalanceTeamsAttr.IsValid())
	{
		MenuSpace->AddSlot()
		.Padding(0.0f,0.0f,0.0f,5.0f)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(350)
				[
					SNew(STextBlock)
					.TextStyle(SUWindowsStyle::Get(),"UT.Common.NormalText")
					.Text(NSLOCTEXT("UTTeamGameMode", "BalanceTeams", "Balance Teams"))
				]
			]
			+ SHorizontalBox::Slot()
			.Padding(20.0f,0.0f,0.0f,0.0f)
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(300)
				[
					bCreateReadOnly ?
					StaticCastSharedRef<SWidget>(
						SNew(SCheckBox)
						.IsChecked(BalanceTeamsAttr.ToSharedRef(), &TAttributePropertyBool::GetAsCheckBox)
						.Type(ESlateCheckBoxType::CheckBox)
						.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
					) :
					StaticCastSharedRef<SWidget>(
						SNew(SCheckBox)
						.IsChecked(BalanceTeamsAttr.ToSharedRef(), &TAttributePropertyBool::GetAsCheckBox)
						.OnCheckStateChanged(BalanceTeamsAttr.ToSharedRef(), &TAttributePropertyBool::SetFromCheckBox)
						.Type(ESlateCheckBoxType::CheckBox)
						.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
					)
				]
			]
		];
	}
}
#endif

AUTPlayerState* AUTTeamGameMode::FindBestPlayerOnTeam(int32 TeamNumToTest)
{
	AUTPlayerState* Best = NULL;
	for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
		if (PS != NULL && PS->GetTeamNum() == TeamNumToTest && (Best == NULL || Best->Score < PS->Score))
		{
			Best = PS;
		}
	}
	return Best;
}

void AUTTeamGameMode::BroadcastScoreUpdate(APlayerState* ScoringPlayer, AUTTeamInfo* ScoringTeam, int32 OldScore)
{
	// find best competing score - assume this is called after scores are updated.
	int32 BestScore = 0;
	for (int32 i = 0; i < Teams.Num(); i++)
	{
		if ((Teams[i] != ScoringTeam) && (Teams[i]->Score >= BestScore))
		{
			BestScore = Teams[i]->Score;
		}
	}
	BroadcastLocalized(this, UUTCTFRewardMessage::StaticClass(), 3+ScoringTeam->TeamIndex, ScoringPlayer, NULL, ScoringTeam);

	if ((OldScore > BestScore) && (OldScore <= BestScore + 2) && (ScoringTeam->Score > BestScore + 2))
	{
		BroadcastLocalized(this, UUTCTFGameMessage::StaticClass(), 8, ScoringPlayer, NULL, ScoringTeam);
	}
	else if (ScoringTeam->Score >= ((MercyScore > 0) ? (BestScore + MercyScore - 1) : (BestScore + 4)))
	{
		BroadcastLocalized(this, UUTCTFGameMessage::StaticClass(), bHasBroadcastDominating ? 2 : 9, ScoringPlayer, NULL, ScoringTeam);
		bHasBroadcastDominating = true;
	}
	else
	{
		bHasBroadcastDominating = false; // since other team scored, need new reminder if mercy rule might be hit again
		BroadcastLocalized(this, UUTCTFGameMessage::StaticClass(), 2, ScoringPlayer, NULL, ScoringTeam);
	}
}

void AUTTeamGameMode::PlayEndOfMatchMessage()
{
	if (UTGameState && UTGameState->WinningTeam)
	{
		for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			APlayerController* Controller = Iterator->Get();
			if (Controller && Controller->IsA(AUTPlayerController::StaticClass()))
			{
				AUTPlayerController* PC = Cast<AUTPlayerController>(Controller);
				if (PC && Cast<AUTPlayerState>(PC->PlayerState))
				{
					PC->ClientReceiveLocalizedMessage(VictoryMessageClass, ((UTGameState->WinningTeam == Cast<AUTPlayerState>(PC->PlayerState)->Team) ? 1 : 0), UTGameState->WinnerPlayerState, PC->PlayerState, UTGameState->WinningTeam);
				}
			}
		}
	}
}

//Special markup for Analytics event so they show up properly in grafana. Should be eventually moved to UTAnalytics.
/*
* @EventName EndTeamMatch
*
* @Trigger Sent at the end of a game
*
* @Type Sent by the server
*
* @EventParam Reason string Reason the game ended
* @EventParam TeamCount int32 number of teams in the game
*
* @Comments
*/
void AUTTeamGameMode::SendEndOfGameStats(FName Reason)
{
	if (FUTAnalytics::IsAvailable())
	{
		if (GetWorld()->GetNetMode() != NM_Standalone)
		{
			TArray<FAnalyticsEventAttribute> ParamArray;
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("Reason"), Reason.ToString()));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("TeamCount"), UTGameState->Teams.Num()));
			for (int32 i=0;i<UTGameState->Teams.Num();i++)
			{
				FString TeamName = FString::Printf(TEXT("TeamScore%i"), i);
				ParamArray.Add(FAnalyticsEventAttribute(TeamName, UTGameState->Teams[i]->Score));
			}
			FUTAnalytics::GetProvider().RecordEvent(TEXT("EndTeamMatch"), ParamArray);
		}
	}
	
	if (!bDisableCloudStats)
	{
		AwardXP();

		UpdateSkillRating();

		const double CloudStatsStartTime = FPlatformTime::Seconds();
		for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
			if (PS != NULL)
			{
				PS->SetStatsValue(NAME_MatchesPlayed, 1);
				PS->SetStatsValue(NAME_TimePlayed, PS->ElapsedTime);

				if (UTGameState->WinningTeam == PS->Team)
				{
					PS->SetStatsValue(NAME_Wins, 1);
				}
				else
				{
					PS->SetStatsValue(NAME_Losses, 1);
				}

				PS->AddMatchToStats(GetWorld()->GetMapName(), GetClass()->GetPathName(), &Teams, &UTGameState->PlayerArray, &InactivePlayerArray);
				
				PS->WriteStatsToCloud();
			}
		}

		for (int32 i = 0; i < InactivePlayerArray.Num(); i++)
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(InactivePlayerArray[i]);
			if (PS != nullptr && !PS->HasWrittenStatsToCloud())
			{
				if (!PS->bAllowedEarlyLeave)
				{
					PS->SetStatsValue(NAME_MatchesQuit, 1);
				}

				PS->SetStatsValue(NAME_MatchesPlayed, 1);
				PS->SetStatsValue(NAME_TimePlayed, PS->ElapsedTime);

				if (UTGameState->WinningTeam == PS->Team)
				{
					PS->SetStatsValue(NAME_Wins, 1);
				}
				else
				{
					PS->SetStatsValue(NAME_Losses, 1);
				}

				PS->AddMatchToStats(GetWorld()->GetMapName(), GetClass()->GetPathName(), &Teams, &UTGameState->PlayerArray, &InactivePlayerArray);
				
				PS->WriteStatsToCloud();
			}
		}

		const double CloudStatsTime = FPlatformTime::Seconds() - CloudStatsStartTime;
		UE_LOG(UT, Log, TEXT("Cloud stats write time %.3f"), CloudStatsTime);
	}
}

void AUTTeamGameMode::FindAndMarkHighScorer()
{
	// Some game modes like Duel may not want every team to have a high scorer
	if (!bHighScorerPerTeamBasis)
	{
		Super::FindAndMarkHighScorer();
		return;
	}

	for (int32 i = 0; i < Teams.Num(); i++)
	{
		int32 BestScore = 0;

		for (int32 PlayerIdx = 0; PlayerIdx < Teams[i]->GetTeamMembers().Num(); PlayerIdx++)
		{
			if (Teams[i]->GetTeamMembers()[PlayerIdx] != nullptr)
			{
				AUTPlayerState *PS = Cast<AUTPlayerState>(Teams[i]->GetTeamMembers()[PlayerIdx]->PlayerState);
				if (PS != nullptr)
				{
					if (BestScore == 0 || PS->Score > BestScore)
					{
						BestScore = PS->Score;
					}
				}
			}
		}

		for (int32 PlayerIdx = 0; PlayerIdx < Teams[i]->GetTeamMembers().Num(); PlayerIdx++)
		{
			if (Teams[i]->GetTeamMembers()[PlayerIdx] != nullptr)
			{
				AUTPlayerState *PS = Cast<AUTPlayerState>(Teams[i]->GetTeamMembers()[PlayerIdx]->PlayerState);
				if (PS != nullptr)
				{
					bool bOldHighScorer = PS->bHasHighScore;
					PS->bHasHighScore = (BestScore == PS->Score);
					if ((bOldHighScorer != PS->bHasHighScore) && (GetNetMode() != NM_DedicatedServer))
					{
						PS->OnRep_HasHighScore();
					}
				}
			}
		}
	}
}

void AUTTeamGameMode::UpdateLobbyScore(FMatchUpdate& MatchUpdate)
{
	for (int32 i = 0; i < UTGameState->Teams.Num(); i++)
	{
		MatchUpdate.TeamScores.Add(UTGameState->Teams[i]->Score);
	}
}

void AUTTeamGameMode::GetGood()
{
#if !(UE_BUILD_SHIPPING)
	if (GetNetMode() == NM_Standalone)
	{
		Super::GetGood();
		Teams[0]->Score = 1;
		Teams[1]->Score = 99;
	}
#endif
}

void AUTTeamGameMode::SendComsMessage( AUTPlayerController* Sender, AUTPlayerState* Target, int32 Switch)
{
	AUTPlayerState* UTPlayerState = Cast<AUTPlayerState>(Sender->PlayerState);
	if (UTPlayerState)
	{
		if (Target != nullptr)
		{
			AUTPlayerController* UTPlayerController = Cast<AUTPlayerController>(Target->GetOwner());
			if (UTPlayerController)
			{
				UTPlayerController->ClientReceiveLocalizedMessage(UTPlayerState->GetCharacterVoiceClass(), Switch, UTPlayerState, nullptr, UTPlayerState->LastKnownLocation);
			}
			Sender->ClientReceiveLocalizedMessage(UTPlayerState->GetCharacterVoiceClass(), Switch, UTPlayerState, nullptr, UTPlayerState->LastKnownLocation);
		}
		else
		{
			for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
			{
				AUTPlayerController* UTPlayerController = Cast<AUTPlayerController>(It->Get());
				if ( UTPlayerController != NULL && UTPlayerController->GetTeamNum() == Sender->GetTeamNum())
				{
					UTPlayerController->ClientReceiveLocalizedMessage(UTPlayerState->GetCharacterVoiceClass(), Switch, UTPlayerState, nullptr, UTPlayerState->LastKnownLocation);
				}
			}
		}
	}
}


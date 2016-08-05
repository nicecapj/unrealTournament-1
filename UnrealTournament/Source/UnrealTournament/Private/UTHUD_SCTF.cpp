// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUD_SCTF.h"
#include "UTSCTFGameState.h"
#include "UTSCTFFlag.h"

AUTHUD_SCTF::AUTHUD_SCTF(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void AUTHUD_SCTF::DrawHUD()
{
	Super::DrawHUD();
}

void AUTHUD_SCTF::DrawMinimapSpectatorIcons()
{
	AUTSCTFGameState* GS = Cast<AUTSCTFGameState>(GetWorld()->GetGameState());
	if (GS == NULL) return;

	AUTPlayerController* PC = Cast<AUTPlayerController>(PlayerOwner);
	if (PC == nullptr) return;

	const float RenderScale = float(Canvas->SizeY) / 1080.0f;

	Super::DrawMinimapSpectatorIcons();

	if (GS && GS->Flag && (GS->Flag->GetTeamNum() != 1 - PC->GetTeamNum() || GS->Flag->ObjectState != CarriedObjectState::Held) )
	{
		FVector2D Pos = WorldToMapToScreen(GS->Flag->GetActorLocation());
		uint8 FlagTeam = GS->Flag->GetTeamNum();
		FColor FlagColor = FlagTeam == 255 ? FColor(0,255,0,255) : GS->Teams[FlagTeam]->TeamColor.ToFColor(true);
		DrawMinimapIcon(HUDAtlas, Pos, FVector2D(30.f, 30.f), FVector2D(843.f, 87.f), FVector2D(43.f, 41.f), FlagColor, true);
	}

}


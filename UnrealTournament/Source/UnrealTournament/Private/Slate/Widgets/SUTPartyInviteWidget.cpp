// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "SUTPartyInviteWidget.h"
#include "BlueprintContextLibrary.h"
#include "PartyContext.h"
#include "SUTComboButton.h"
#include "UTParty.h"
#include "UTGameInstance.h"
#include "../SUTStyle.h"

#if WITH_SOCIAL
#include "Social.h"
#endif

#if !UE_SERVER

void SUTPartyInviteWidget::Construct(const FArguments& InArgs, const FLocalPlayerContext& InCtx)
{
	Ctx = InCtx;

#if WITH_SOCIAL
	if (!ISocialModule::Get().GetFriendsAndChatManager()->GetNotificationService()->OnSendNotification().IsBoundToObject(this))
	{
		ISocialModule::Get().GetFriendsAndChatManager()->GetNotificationService()->OnSendNotification().AddSP(this, &SUTPartyInviteWidget::HandleFriendsActionNotification);
	}
#endif

	ChildSlot
	.HAlign(HAlign_Right)
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		.BorderBackgroundColor(FLinearColor::Gray) // Darken the outer border
		.Visibility(this, &SUTPartyInviteWidget::ShouldShowInviteWidget)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.Padding(5.0f)
			[
				SAssignNew(InviteMessage, STextBlock)
				.Text(FText::Format(NSLOCTEXT("SUTPartyInviteWidget", "InviteMessage", "{0} has invited you to their party."), FText::FromString(TEXT("BobLife"))))
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
			]
			+SVerticalBox::Slot()
			.Padding(5.0f)
			[
				SNew(SHorizontalBox)	
				+ SHorizontalBox::Slot()
				.Padding(5.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(96)
					.HeightOverride(48)
					[
						SNew(SButton)
						.ButtonStyle(SUTStyle::Get(),"UT.SimpleButton")
						.ContentPadding(FMargin(5.0,0.0,5.0,0.0))
						.OnClicked(this, &SUTPartyInviteWidget::AcceptInvite)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							.AutoHeight()
							[
								SNew(STextBlock)
								.Text(NSLOCTEXT("SUTPartyInviteWidget", "Accept", "Accept"))
								.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
								.Justification(ETextJustify::Center)
							]
						]
					]
				]
				+ SHorizontalBox::Slot()
				.Padding(5.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(96)
					.HeightOverride(48)
					[
						SNew(SButton)
						.ButtonStyle(SUTStyle::Get(),"UT.SimpleButton")
						.ContentPadding(FMargin(5.0, 0.0, 5.0, 0.0))
						.OnClicked(this, &SUTPartyInviteWidget::RejectInvite)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.VAlign(VAlign_Center)
							.AutoHeight()
							[
								SNew(STextBlock)
								.Text(NSLOCTEXT("SUTPartyInviteWidget", "Reject", "Reject"))
								.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
								.Justification(ETextJustify::Center)
							]
						]
					]
				]
			]
		]
	];
}

EVisibility SUTPartyInviteWidget::ShouldShowInviteWidget() const
{
	if (LastInviteContent.IsEmpty() || (FDateTime::Now() - LastInviteTime).GetMinutes() >= 1)
	{
		return EVisibility::Collapsed;
	}

	return EVisibility::Visible;
}

void SUTPartyInviteWidget::HandleFriendsActionNotification(TSharedRef<FFriendsAndChatMessage> FriendsAndChatMessage)
{
#if WITH_SOCIAL
	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Ctx.GetLocalPlayer());
	if (FriendsAndChatMessage->GetMessageType() == EMessageType::GameInvite && LP->IsMenuGame())
	{
		LastInviteContent = FriendsAndChatMessage->GetMessage();
		LastInviteUniqueID = FriendsAndChatMessage->GetUniqueID()->ToString();
		LastInviteTime = FDateTime::Now();

		InviteMessage->SetText(FText::Format(NSLOCTEXT("SUTPartyInviteWidget", "InviteMessage", "{0}"), FText::FromString(LastInviteContent)));
	}
#endif
}

FReply SUTPartyInviteWidget::AcceptInvite()
{
#if WITH_SOCIAL
	if (!LastInviteUniqueID.IsEmpty())
	{
		TSharedPtr<IGameAndPartyService> GameAndPartyService = ISocialModule::Get().GetFriendsAndChatManager()->GetGameAndPartyService();
		TSharedPtr< IFriendItem > User = ISocialModule::Get().GetFriendsAndChatManager()->GetFriendsService()->FindUser(FUniqueNetIdString(LastInviteUniqueID));
		GameAndPartyService->AcceptGameInvite(User);
	}
#endif
	LastInviteTime = 0;
	LastInviteContent.Empty();
	LastInviteUniqueID.Empty();

	return FReply::Handled();
}

FReply SUTPartyInviteWidget::RejectInvite()
{
	LastInviteTime = 0;
	LastInviteContent.Empty();
	LastInviteUniqueID.Empty();

	return FReply::Handled();
}

SUTPartyInviteWidget::~SUTPartyInviteWidget()
{
}

#endif
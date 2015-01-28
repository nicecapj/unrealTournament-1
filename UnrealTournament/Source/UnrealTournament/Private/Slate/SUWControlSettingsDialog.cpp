// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "SUWControlSettingsDialog.h"
#include "SUWindowsStyle.h"
#include "SKeyBind.h"

#if !UE_SERVER

FSimpleBind::FSimpleBind(const FText& InDisplayName)
{
	DisplayName = InDisplayName.ToString();
	bHeader = false;
	Key = MakeShareable(new FKey());
	AltKey = MakeShareable(new FKey());
}

FSimpleBind* FSimpleBind::AddMapping(const FString& Mapping, float Scale)
{
	UInputSettings* InputSettings = UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>();

	bool bFound = false;
	//Add any ActionMappings
	for (int32 i = 0; i < InputSettings->ActionMappings.Num(); i++)
	{
		FInputActionKeyMapping Action = InputSettings->ActionMappings[i];
		if (Mapping.Compare(Action.ActionName.ToString()) == 0 && !Action.Key.IsGamepadKey())
		{
			ActionMappings.Add(Action);

			//Fill in the first 2 keys we find from the ini
			if (*Key == FKey())
			{
				*Key = Action.Key;
			}
			else if (*AltKey == FKey() && Action.Key != *Key)
			{
				*AltKey = Action.Key;
			}
			bFound = true;
		}
	}
	//Add any AxisMappings
	for (int32 i = 0; i < InputSettings->AxisMappings.Num(); i++)
	{
		FInputAxisKeyMapping Axis = InputSettings->AxisMappings[i];
		if (Mapping.Compare(Axis.AxisName.ToString()) == 0 && Axis.Scale == Scale && !Axis.Key.IsGamepadKey())
		{
			AxisMappings.Add(Axis);

			//Fill in the first 2 keys we find from the ini
			if (*Key == FKey())
			{
				*Key = Axis.Key;
			}
			else if (*AltKey == FKey() && Axis.Key != *Key)
			{
				*AltKey = Axis.Key;
			}
			bFound = true;
		}
	}
	//Add any CustomBinds
	UUTPlayerInput* UTPlayerInput = UUTPlayerInput::StaticClass()->GetDefaultObject<UUTPlayerInput>();
	for (int32 i = 0; i < UTPlayerInput->CustomBinds.Num(); i++)
	{
		FCustomKeyBinding CustomBind = UTPlayerInput->CustomBinds[i];
		if (Mapping.Compare(CustomBind.Command) == 0 && !FKey(CustomBind.KeyName).IsGamepadKey())
		{
			//Fill in the first 2 keys we find from the ini
			if (*Key == FKey())
			{
				*Key = CustomBind.KeyName;
			}
			else if (*AltKey == FKey() && CustomBind.KeyName != *Key)
			{
				*AltKey = CustomBind.KeyName;
			}

			//Check for duplicates and add if unique
			bool bDuplicate = false;
			for (auto Bind : CustomBindings)
			{
				if (Bind.Command.Compare(CustomBind.Command) == 0 && Bind.KeyName.Compare(CustomBind.KeyName) == 0)
				{
					bDuplicate = true;
					break;
				}
			}
			if (!bDuplicate)
			{
				CustomBindings.Add(CustomBind);
			}
			bFound = true;
		}
	}

	//Special Console case
	if (Mapping.Compare(TEXT("Console")) == 0)
	{
		*Key = InputSettings->ConsoleKeys[0];
	}
	else if (!bFound)
	{
		// assume not found items are CustomBinds
		// somewhat hacky, need to figure out a better way to detect and match this stuff to the possible settings
		FCustomKeyBinding* Bind = new(CustomBindings) FCustomKeyBinding;
		Bind->KeyName = NAME_None;
		Bind->EventType = IE_Pressed;
		Bind->Command = Mapping;
	}
	return this;
}

void FSimpleBind::WriteBind()
{
	if (bHeader)
	{
		return;
	}
	UInputSettings* InputSettings = UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>();

	// Collapse the keys if the main key is missing.
	if (*Key == FKey() && *AltKey != FKey())
	{
		Key = AltKey;
		AltKey = MakeShareable(new FKey());
	}

	//Remove the original bindings
	for (auto& Bind : ActionMappings)
	{
		InputSettings->RemoveActionMapping(Bind);
	}
	for (auto& Bind : AxisMappings)
	{
		InputSettings->RemoveAxisMapping(Bind);
	}
	for (TObjectIterator<UUTPlayerInput> It(RF_NoFlags); It; ++It)
	{
		UUTPlayerInput* UTPlayerInput = *It;
		for (auto& Bind : CustomBindings)
		{
			for (int32 i = 0; i < UTPlayerInput->CustomBinds.Num(); i++)
			{
				const FCustomKeyBinding& KeyMapping = UTPlayerInput->CustomBinds[i];
				if (KeyMapping.Command == Bind.Command)
				{
					UTPlayerInput->CustomBinds.RemoveAt(i, 1);
					i--;
				}
			}
		}
	}
	//Set our new keys and readd them
	for (auto Bind : ActionMappings)
	{
		Bind.Key = *Key;
		InputSettings->AddActionMapping(Bind);
		if (*AltKey != FKey())
		{
			Bind.Key = *AltKey;
			InputSettings->AddActionMapping(Bind);
		}
	}
	for (auto Bind : AxisMappings)
	{
		Bind.Key = *Key;
		InputSettings->AddAxisMapping(Bind);
		if (*AltKey != FKey())
		{
			Bind.Key = *AltKey;
			InputSettings->AddAxisMapping(Bind);
		}
	}

	for (TObjectIterator<UUTPlayerInput> It(RF_NoFlags); It; ++It)
	{
		UUTPlayerInput* UTPlayerInput = *It;
		for (auto Bind : CustomBindings)
		{
			Bind.KeyName = FName(*Key->ToString());
			UTPlayerInput->CustomBinds.Add(Bind);
			if (*AltKey != FKey())
			{
				Bind.KeyName = FName(*AltKey->ToString());
				UTPlayerInput->CustomBinds.Add(Bind);
			}
		}
	}

	//Special Console case
	if (DisplayName.Compare(TEXT("Console")) == 0)
	{
		InputSettings->ConsoleKeys.Empty();
		InputSettings->ConsoleKeys.Add(*Key);
	}
}

void SUWControlSettingsDialog::CreateBinds()
{
	//Movement
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Movement", "Movement")))->MakeHeader()));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Move Forward", "Move Forward")))
		->AddMapping("MoveForward", 1.0f)
		->AddMapping("TapForward")
		->AddMapping("TapForwardRelease")
		->AddDefaults(EKeys::W, EKeys::Up)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Move Backward", "Move Backward")))
		->AddMapping("MoveBackward", 1.0f)
		->AddMapping("TapBack")
		->AddMapping("TapBackRelease")
		->AddDefaults(EKeys::S, EKeys::Down)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Move Left", "Move Left")))
		->AddMapping("MoveLeft", 1.0f)
		->AddMapping("TapLeft")
		->AddMapping("TapLeftRelease")
		->AddDefaults(EKeys::A)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Move Right", "Move Right")))
		->AddMapping("MoveRight", 1.0f)
		->AddMapping("TapRight")
		->AddMapping("TapRightRelease")
		->AddDefaults(EKeys::D)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Turn Left", "Turn Left")))
		->AddMapping("TurnRate", -1.0f)
		->AddDefaults(EKeys::Left)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Turn Right", "Turn Right")))
		->AddMapping("TurnRate", 1.0f)
		->AddDefaults(EKeys::Right)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Jump", "Jump")))
		->AddMapping("MoveUp", 1.0f)
		->AddMapping("Jump")
		->AddDefaults(EKeys::SpaceBar)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Crouch", "Crouch")))
		->AddMapping("MoveUp", -1.0f)
		->AddMapping("Crouch")
		->AddDefaults(EKeys::LeftControl, EKeys::C)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Single Tap Dodge", "Single Tap Dodge")))
		->AddMapping("SingleTapDodge")
		->AddDefaults(EKeys::V)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Roll / Slide", "Roll / Slide")))
		->AddMapping("HoldRollSlide")
		->AddDefaults(EKeys::LeftShift)));
	//Weapons
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Weapons", "Weapons")))->MakeHeader()));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Fire", "Fire")))
		->AddMapping("StartFire")
		->AddMapping("StopFire")
		->AddDefaults(EKeys::LeftMouseButton, EKeys::RightControl)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Alt Fire", "Alt Fire")))
		->AddMapping("StartAltFire")
		->AddMapping("StopAltFire")
		->AddDefaults(EKeys::RightMouseButton)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Next Weapon", "Next Weapon")))
		->AddMapping("NextWeapon")
		->AddDefaults(EKeys::MouseScrollUp)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Previous Weapon", "Previous Weapon")))
		->AddMapping("PrevWeapon")
		->AddDefaults(EKeys::MouseScrollDown)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "BestWeapon", "Best Weapon")))
		->AddMapping("SwitchToBestWeapon")
		->AddDefaults(EKeys::E)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Throw Weapon", "Throw Weapon")))
		->AddMapping("ThrowWeapon")
		->AddDefaults(EKeys::M)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Select Translocator", "Select Translocator")))
		->AddMapping("SwitchWeapon 0")
		->AddDefaults(EKeys::Q)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Select Impact Hammer", "Select Impact Hammer")))
		->AddMapping("SwitchWeapon 1")
		->AddDefaults(EKeys::One)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Select Enforcer", "Select Enforcer")))
		->AddMapping("SwitchWeapon 2")
		->AddDefaults(EKeys::Two)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Select Bio Rifle", "Select Bio Rifle")))
		->AddMapping("SwitchWeapon 3")
		->AddDefaults(EKeys::Three)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Select Shock Rifle", "Select Shock Rifle")))
		->AddMapping("SwitchWeapon 4")
		->AddDefaults(EKeys::Four)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Select Link Gun", "Select Link Gun")))
		->AddMapping("SwitchWeapon 5")
		->AddDefaults(EKeys::Five)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Select Stinger", "Select Stinger")))
		->AddMapping("SwitchWeapon 6")
		->AddDefaults(EKeys::Six)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Select Flak Cannon", "Select Flak Cannon")))
		->AddMapping("SwitchWeapon 7")
		->AddDefaults(EKeys::Seven)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Select Rocket Launcher", "Select Rocket Launcher")))
		->AddMapping("SwitchWeapon 8")
		->AddDefaults(EKeys::Eight)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Select Sniper Rifle", "Select Sniper Rifle")))
		->AddMapping("SwitchWeapon 9")
		->AddDefaults(EKeys::Nine)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Select Superweapon", "Select Superweapon")))
		->AddMapping("SwitchWeapon 10")
		->AddDefaults(EKeys::Zero)));
	//Emotes
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Emotes", "Emotes")))->MakeHeader()));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Emote 1", "Emote 1")))
		->AddMapping("PlayEmote1")
		->AddDefaults(EKeys::J)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Emote 2", "Emote 2")))
		->AddMapping("PlayEmote2")
		->AddDefaults(EKeys::K)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Emote 3", "Emote 3")))
		->AddMapping("PlayEmote3")
		->AddDefaults(EKeys::L)));
	//Misc
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Misc", "Misc")))->MakeHeader()));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Feign Death", "Feign Death")))
		->AddMapping("FeignDeath")
		->AddDefaults(EKeys::H)));
	//Hud
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Hud", "Hud")))->MakeHeader()));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Show Scores", "Show Scores")))
		->AddMapping("ShowScores")
		->AddDefaults(EKeys::Escape)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Show Menu", "Show Menu")))
		->AddMapping("ShowMenu")
		->AddDefaults(EKeys::Tab)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Console", "Console")))
		->AddMapping("Console")
		->AddDefaults(EKeys::Tilde)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Talk", "Talk")))
		->AddMapping("Talk")
		->AddDefaults(EKeys::T)));
	Binds.Add(MakeShareable((new FSimpleBind(NSLOCTEXT("KeyBinds", "Team Talk", "Team Talk")))
		->AddMapping("TeamTalk")
		->AddDefaults(EKeys::Y)));

	// TODO: mod binding registration
}

void SUWControlSettingsDialog::Construct(const FArguments& InArgs)
{
	SUWDialog::Construct(SUWDialog::FArguments()
							.PlayerOwner(InArgs._PlayerOwner)
							.DialogTitle(InArgs._DialogTitle)
							.DialogSize(InArgs._DialogSize)
							.bDialogSizeIsRelative(InArgs._bDialogSizeIsRelative)
							.DialogPosition(InArgs._DialogPosition)
							.DialogAnchorPoint(InArgs._DialogAnchorPoint)
							.ContentPadding(InArgs._ContentPadding)
							.ButtonMask(InArgs._ButtonMask)
							.OnDialogResult(InArgs._OnDialogResult)
						);

	MouseSensitivityRange = FVector2D(0.0075f, 0.15f);

	//Is Mouse Inverted
	bool bMouseInverted = false;
	UInputSettings* InputSettings = UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>();
	for (FInputAxisConfigEntry& Entry : InputSettings->AxisConfig)
	{
		if (Entry.AxisKeyName == EKeys::MouseY)
		{
			bMouseInverted = Entry.AxisProperties.bInvert;
			break;
		}
	}
	//Get the dodge settings
	AUTPlayerController* PC = AUTPlayerController::StaticClass()->GetDefaultObject<AUTPlayerController>();
	MaxDodgeClickTimeValue = PC->MaxDodgeClickTime;
	MaxDodgeTapTimeValue = PC->MaxDodgeTapTime;

	CreateBinds();

	if (DialogContent.IsValid())
	{
		const float MessageTextPaddingX = 10.0f;
		TSharedPtr<STextBlock> MessageTextBlock;
		DialogContent->AddSlot()
		[
			SNew(SVerticalBox)

			// Tab bar

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FMargin(0.0f, 0.0f, 0.0f, 0.0f))
				+ SUniformGridPanel::Slot(0, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
					.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
					.Text(NSLOCTEXT("SUWControlSettingsDialog", "ControlTabKeyboard", "Keyboard").ToString())
					.OnClicked(this, &SUWControlSettingsDialog::OnTabClickKeyboard)
				]
				+ SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
					.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
					.Text(NSLOCTEXT("SUWControlSettingsDialog", "ControlTabMouse", "Mouse").ToString())
					.OnClicked(this, &SUWControlSettingsDialog::OnTabClickMouse)
				]
				+ SUniformGridPanel::Slot(2, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
					.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
					.Text(NSLOCTEXT("SUWControlSettingsDialog", "ControlTabMovement", "Movement").ToString())
					.OnClicked(this, &SUWControlSettingsDialog::OnTabClickMovement)
				]
			]

			// Content

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.Padding(5.0f, 0.0f, 5.0f, 0.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding(0.0f, 5.0f, 0.0f, 5.0f)
				.AutoHeight()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				[
					//Keyboard Settings
					SAssignNew(TabWidget, SWidgetSwitcher)

					// Key binds

					+ SWidgetSwitcher::Slot()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(5.0f, 5.0f, 5.0f, 5.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.Padding(10.0f, 0.0f, 10.0f, 0.0f)
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							[
								SNew(STextBlock)
								.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.Dialog.TextStyle")
								.Text(NSLOCTEXT("SUWControlSettingsDialog", "Action", "Action").ToString())
							]
							+ SHorizontalBox::Slot()
							.Padding(10.0f, 0.0f, 10.0f, 0.0f)
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							.FillWidth(1.2f)
							[
								SNew(STextBlock)
								.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.Dialog.TextStyle")
								.Text(NSLOCTEXT("SUWControlSettingsDialog", "KeyBinds", "Key").ToString())
							]
							+ SHorizontalBox::Slot()
							.Padding(10.0f, 0.0f, 10.0f, 0.0f)
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							.FillWidth(1.2f)
							[
								SNew(STextBlock)
								.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.Dialog.TextStyle")
								.Text(NSLOCTEXT("SUWControlSettingsDialog", "KeyBinds", "Alternate Key").ToString())
							]
						]
					
						//Key bind list

						+ SVerticalBox::Slot()
						.Padding(FMargin(10.0f, 15.0f, 10.0f, 5.0f))
						[
							SAssignNew(ScrollBox, SScrollBox)
							+ SScrollBox::Slot()
							[
								SAssignNew(ControlList, SVerticalBox)
							]
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(10.0f, 15.0f, 10.0f, 5.0f))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.Padding(10.0f, 0.0f, 10.0f, 0.0f)
							.AutoHeight()
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							[
								SNew(SButton)
								.HAlign(HAlign_Center)
								.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
								.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
								.Text(NSLOCTEXT("SUWControlSettingsDialog", "BindDefault", "Reset to Default").ToString())
								.OnClicked(this, &SUWControlSettingsDialog::OnBindDefaultClick)
							]
						]
					]

					//Mouse Settings

					+ SWidgetSwitcher::Slot()
					.HAlign(HAlign_Fill)
					[
						SNew(SBox)
						.VAlign(VAlign_Fill)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
							.HAlign(HAlign_Left)
							[
								SAssignNew(MouseSmoothing, SCheckBox)
								.ForegroundColor(FLinearColor::White)
								.IsChecked(GetDefault<UInputSettings>()->bEnableMouseSmoothing ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
								.Content()
								[
									SNew(STextBlock)
									.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.Dialog.TextStyle")
									.Text(NSLOCTEXT("SUWControlSettingsDialog", "MouseSmoothing", "Mouse Smoothing").ToString())
								]
							]
							+ SVerticalBox::Slot()
							.HAlign(HAlign_Left)
							.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
							[
								SAssignNew(MouseInvert, SCheckBox)
								.ForegroundColor(FLinearColor::White)
								.IsChecked(bMouseInverted ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
								.Content()
								[
									SNew(STextBlock)
									.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.Dialog.TextStyle")
									.Text(NSLOCTEXT("SUWControlSettingsDialog", "MouseInvert", "Invert Mouse").ToString())
								]
							]
							+SVerticalBox::Slot()
							.Padding(FMargin(10.0f, 15.0f, 10.0f, 5.0f))
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.Padding(10.0f, 0.0f, 10.0f, 0.0f)
								[
									SNew(STextBlock)					
									.Text(NSLOCTEXT("SUWControlSettingsDialog", "MouseSensitivity", "Mouse Sensitivity (0-20)").ToString())
									.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.Dialog.TextStyle")
								]
								+ SHorizontalBox::Slot()
								.Padding(10.0f, 0.0f, 10.0f, 0.0f)
								.AutoWidth()
								[
									SAssignNew(MouseSensitivityEdit, SEditableTextBox)
									.Text(FText::AsNumber(UUTPlayerInput::StaticClass()->GetDefaultObject<UUTPlayerInput>()->GetMouseSensitivity()))
									.OnTextCommitted(this, &SUWControlSettingsDialog::EditSensitivity)
								]
								+ SHorizontalBox::Slot()
								.Padding(10.0f, 0.0f, 10.0f, 0.0f)
								[
									SNew(SBox)
									.Content()
									[
										SAssignNew(MouseSensitivity, SSlider)
										.Orientation(Orient_Horizontal)
										.Value((UUTPlayerInput::StaticClass()->GetDefaultObject<UUTPlayerInput>()->GetMouseSensitivity() - MouseSensitivityRange.X) / (MouseSensitivityRange.Y - MouseSensitivityRange.X))
									]
								]
							]
						]
					]
					//Movement Settings
					+ SWidgetSwitcher::Slot()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.HAlign(HAlign_Left)
						.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
						[
							SAssignNew(SingleTapWallDodge, SCheckBox)
							.ForegroundColor(FLinearColor::White)
							.IsChecked(PC->bSingleTapWallDodge ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
							.Content()
							[
								SNew(STextBlock)
								.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.Dialog.TextStyle")
								.Text(NSLOCTEXT("SUWControlSettingsDialog", "SingleTapWallDodge", "Enable single tap wall dodge").ToString())
							]
						]
						+ SVerticalBox::Slot()
							.HAlign(HAlign_Left)
							.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
							[
								SAssignNew(SingleTapAfterJump, SCheckBox)
								.ForegroundColor(FLinearColor::White)
								.IsChecked(PC->bSingleTapAfterJump ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
								.Content()
								[
									SNew(STextBlock)
									.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.Dialog.TextStyle")
									.Text(NSLOCTEXT("SUWControlSettingsDialog", "SingleTapAfterJump", "Single tap wall dodge only after jump").ToString())
								]
							]
						+ SVerticalBox::Slot()
							.HAlign(HAlign_Left)
							.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
							[
								SAssignNew(AutoSlide, SCheckBox)
								.ForegroundColor(FLinearColor::White)
								.IsChecked(PC->bAutoSlide ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
								.Content()
								[
									SNew(STextBlock)
									.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.Dialog.TextStyle")
									.Text(NSLOCTEXT("SUWControlSettingsDialog", "AutoSlide", "Automatically engage wall-slide when pressed against wall").ToString())
								]
							]
						+ SVerticalBox::Slot()
							.HAlign(HAlign_Left)
							.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
							[
								SAssignNew(TapCrouchToSlide, SCheckBox)
								.ForegroundColor(FLinearColor::White)
								.IsChecked(PC->bTapCrouchToSlide ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
								.Content()
								[
									SNew(STextBlock)
									.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.Dialog.TextStyle")
									.Text(NSLOCTEXT("SUWControlSettingsDialog", "TapCrouchToSlide", "Tap Crouch To Slide").ToString())
								]
							]
						+ SVerticalBox::Slot()
							.HAlign(HAlign_Left)
						.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.Padding(10.0f, 0.0f, 10.0f, 0.0f)
							.AutoWidth()
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							[
								SNew(STextBlock)
								.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.Dialog.TextStyle")
								.Text(NSLOCTEXT("SUWControlSettingsDialog", "MaxDodgeTapTime", "Single Tap Wall Dodge Hold Time").ToString())
							]
							+ SHorizontalBox::Slot()
							.Padding(10.0f, 0.0f, 10.0f, 0.0f)
							.VAlign(VAlign_Center)
							[
								SAssignNew(MaxDodgeTapTime, SNumericEntryBox<float>)
								.Value(this, &SUWControlSettingsDialog::GetMaxDodgeTapTimeValue)
								.OnValueCommitted(this, &SUWControlSettingsDialog::SetMaxDodgeTapTimeValue)
							]
						]
						+ SVerticalBox::Slot()
						.HAlign(HAlign_Left)
						.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.Padding(10.0f, 0.0f, 10.0f, 0.0f)
							.AutoWidth()
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Left)
							[
								SNew(STextBlock)
								.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.Dialog.TextStyle")
								.Text(NSLOCTEXT("SUWControlSettingsDialog", "MaxDodgeClickTime", "Dodge Double-Click Time").ToString())
							]
							+ SHorizontalBox::Slot()
							.Padding(10.0f, 0.0f, 10.0f, 0.0f)
							.VAlign(VAlign_Center)
							[
								SAssignNew(MaxDodgeClickTime, SNumericEntryBox<float>)
								.Value(this, &SUWControlSettingsDialog::GetMaxDodgeClickTimeValue)
								.OnValueCommitted(this, &SUWControlSettingsDialog::SetMaxDodgeClickTimeValue)
							]			
						]
					]			
				]
			]
		];
	}
	//Create the bind list
	for (const auto& Bind : Binds)
	{
		if (Bind->bHeader)
		{
			ControlList->AddSlot()
			.Padding(FMargin(10.0f, 15.0f, 10.0f, 5.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(10.0f, 0.0f, 10.0f, 0.0f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.Dialog.Title.TextStyle")
					.Text(Bind->DisplayName)
				]
			];
		}
		else
		{
			ControlList->AddSlot()
			.Padding(FMargin(10.0f, 2.0f, 10.0f, 2.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(10.0f, 0.0f, 10.0f, 0.0f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				[
					SNew(STextBlock)
					.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.Dialog.TextStyle")
					.Text(Bind->DisplayName)
				]
				+ SHorizontalBox::Slot()
				.Padding(10.0f, 0.0f, 10.0f, 0.0f)
				[
					SAssignNew(Bind->KeyWidget, SKeyBind)
					.Key(Bind->Key)
					.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
				]
				+ SHorizontalBox::Slot()
				.Padding(10.0f, 0.0f, 10.0f, 0.0f)
				[
					SAssignNew(Bind->AltKeyWidget, SKeyBind)
					.Key(Bind->AltKey)
					.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
				]
			];
		}
	}
}

SVerticalBox::FSlot& SUWControlSettingsDialog::AddHeader(const FString& Header)
{
	return SVerticalBox::Slot()
		.Padding(FMargin(10.0f, 15.0f, 10.0f, 5.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FLinearColor::Black)
				.Text(Header)
			]
		];
}

SVerticalBox::FSlot& SUWControlSettingsDialog::AddKeyBind(TSharedPtr<FSimpleBind> Binds)
{
	return SVerticalBox::Slot()
		.Padding(FMargin(10.0f, 2.0f, 10.0f, 2.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FLinearColor::Black)
				.Text(Binds->DisplayName)
			]
			+ SHorizontalBox::Slot()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			[
				SAssignNew(Binds->KeyWidget, SKeyBind)
				.Key(Binds->Key)
				.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
			]
			+ SHorizontalBox::Slot()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			[
				SAssignNew(Binds->AltKeyWidget, SKeyBind)
				.Key(Binds->AltKey)
				.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
			]
		];
}

FReply SUWControlSettingsDialog::OnBindDefaultClick()
{
	//Reset all the binds to their default value
	for (auto Bind : Binds)
	{
		if (!Bind->bHeader)
		{
			Bind->KeyWidget->SetKey(Bind->DefaultKey, false);
			Bind->AltKeyWidget->SetKey(Bind->DefaultAltKey, false);
		}
	}
	return FReply::Handled();
}

FReply SUWControlSettingsDialog::OKClick()
{
	//Write the binds to the ini
	for (const auto& Bind : Binds)
	{
		Bind->WriteBind();
	}
	UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>()->SaveConfig();
	UUTPlayerInput::StaticClass()->GetDefaultObject<UUTPlayerInput>()->SaveConfig();

	//Update the playing players custom binds
	APlayerController* PC = GetPlayerOwner()->PlayerController;
	if (PC != NULL && Cast<UUTPlayerInput>(PC->PlayerInput) != NULL)
	{
		Cast<UUTPlayerInput>(PC->PlayerInput)->CustomBinds = UUTPlayerInput::StaticClass()->GetDefaultObject<UUTPlayerInput>()->CustomBinds;
	}

	//Mouse Settings
	UUTGameUserSettings* UserSettings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());

	// mouse sensitivity
	float NewSensitivity = MouseSensitivity->GetValue() * (MouseSensitivityRange.Y - MouseSensitivityRange.X) + MouseSensitivityRange.X;
	for (TObjectIterator<UUTPlayerInput> It(RF_NoFlags); It; ++It)
	{
		It->SetMouseSensitivity(NewSensitivity);

		//Invert mouse
		for (FInputAxisConfigEntry& Entry : It->AxisConfig)
		{
			if (Entry.AxisKeyName == EKeys::MouseY)
			{
				Entry.AxisProperties.bInvert = MouseInvert->IsChecked();
			}
		}
	}
	UInputSettings* InputSettings = UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>();
	for (FInputAxisConfigEntry& Entry : InputSettings->AxisConfig)
	{
		if (Entry.AxisKeyName == EKeys::MouseX || Entry.AxisKeyName == EKeys::MouseY)
		{
			Entry.AxisProperties.Sensitivity = NewSensitivity;
		}
	}
	//Invert Mouse
	for (FInputAxisConfigEntry& Entry : InputSettings->AxisConfig)
	{
		if (Entry.AxisKeyName == EKeys::MouseY)
		{
			Entry.AxisProperties.bInvert = MouseInvert->IsChecked();
		}
	}
	//Mouse Smooth
	InputSettings->bEnableMouseSmoothing = MouseSmoothing->IsChecked();
	InputSettings->SaveConfig();

	//Movement settings
	for (TObjectIterator<AUTPlayerController> It(RF_NoFlags); It; ++It)
	{
		It->bSingleTapWallDodge = SingleTapWallDodge->IsChecked();
		It->bTapCrouchToSlide = TapCrouchToSlide->IsChecked();
		It->bAutoSlide = AutoSlide->IsChecked();
		It->bSingleTapAfterJump = SingleTapAfterJump->IsChecked();
		It->MaxDodgeClickTime = MaxDodgeClickTimeValue;
		It->MaxDodgeTapTime = MaxDodgeTapTimeValue;
	}
	AUTPlayerController::StaticClass()->GetDefaultObject<AUTPlayerController>()->SaveConfig();

	GetPlayerOwner()->CloseDialog(SharedThis(this));
	GetPlayerOwner()->SaveProfileSettings();

	return FReply::Handled();
}

FReply SUWControlSettingsDialog::CancelClick()
{
	GetPlayerOwner()->CloseDialog(SharedThis(this));
	return FReply::Handled();
}

FReply SUWControlSettingsDialog::OnButtonClick(uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_OK) OKClick();
	else if (ButtonID == UTDIALOG_BUTTON_CANCEL) CancelClick();
	return FReply::Handled();

}

void SUWControlSettingsDialog::EditSensitivity(const FText& Input, ETextCommit::Type)
{
	float ChangedSensitivity = FCString::Atof(*Input.ToString());

	if (ChangedSensitivity >= 0.0f && ChangedSensitivity <= 20.0f)
	{
		ChangedSensitivity /= 20.0f;
		MouseSensitivity->SetValue(ChangedSensitivity);
	}
}

void SUWControlSettingsDialog::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SUWDialog::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (MouseSensitivityEdit.IsValid() && !MouseSensitivityEdit->HasKeyboardFocus())
	{		
		MouseSensitivityEdit->SetText(FText::FromString(FString::Printf(TEXT("%f"), MouseSensitivity->GetValue() * 20.0f)));
	}
}
#endif
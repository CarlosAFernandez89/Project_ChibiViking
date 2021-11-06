// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "HTNStyle.h"

class FHTNCommonCommands : public TCommands<FHTNCommonCommands>
{
public:
	FHTNCommonCommands();
	
	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenPluginWindow;
};

class FHTNDebuggerCommands : public TCommands<FHTNDebuggerCommands>
{
public:
	FHTNDebuggerCommands();

	/*
	TSharedPtr<FUICommandInfo> BackInto;
	TSharedPtr<FUICommandInfo> BackOver;
	TSharedPtr<FUICommandInfo> ForwardInto;
	TSharedPtr<FUICommandInfo> ForwardOver;
	TSharedPtr<FUICommandInfo> StepOut;
	*/

	TSharedPtr<FUICommandInfo> PausePlaySession;
	TSharedPtr<FUICommandInfo> ResumePlaySession;
	TSharedPtr<FUICommandInfo> StopPlaySession;
	
	TSharedPtr<FUICommandInfo> CurrentValues;
	TSharedPtr<FUICommandInfo> ValuesOfSelectedNode;

	/** Initialize commands */
	virtual void RegisterCommands() override;
};

class FHTNBlackboardCommands : public TCommands<FHTNBlackboardCommands>
{
public:
	FHTNBlackboardCommands();

	TSharedPtr<FUICommandInfo> DeleteEntry;

	/** Initialize commands */
	virtual void RegisterCommands() override;
};
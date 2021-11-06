// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// A helper for customizing the toolbar of an HTN editor.
class FHTNEditorToolbarBuilder : public TSharedFromThis<FHTNEditorToolbarBuilder>
{
public:
	FHTNEditorToolbarBuilder(TSharedPtr<class FHTNEditor> InHTNEditor) : HTNEditorWeakPtr(InHTNEditor) {}
	void AddModesToolbar(TSharedPtr<class FExtender> Extender);
	void AddDebuggerToolbar(TSharedPtr<class FExtender> Extender);
	
private:
	void FillModesToolbar(class FToolBarBuilder& ToolbarBuilder);
	void FillDebuggerToolbar(class FToolBarBuilder& ToolbarBuilder);
	TWeakPtr<FHTNEditor> HTNEditorWeakPtr;
};
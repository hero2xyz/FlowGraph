// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#pragma once

#include "CoreMinimal.h"
#include "SFlowGraphNode.h"
#include "UObject/Object.h"

class FLOWEDITOR_API SFlowGraphNode_Variable : public SFlowGraphNode
{
	virtual const FSlateBrush* GetShadowBrush(bool bSelected) const override;

	virtual void GetDiffHighlightBrushes(const FSlateBrush*& BackgroundOut, const FSlateBrush*& ForegroundOut) const override;

	// SGraphNode interface
	virtual void UpdateGraphNode() override;
	virtual void AddPin(const TSharedRef<SGraphPin>& PinToAdd) override;
	// End of SGraphNode interface

protected:
	FSlateColor GetVariableColor() const;
};

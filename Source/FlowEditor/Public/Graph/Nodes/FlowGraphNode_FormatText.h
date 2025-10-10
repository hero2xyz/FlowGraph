// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#pragma once

#include "FlowGraphNode.h"
#include "FlowGraphNode_FormatText.generated.h"

/**
 *
 */
UCLASS()
class FLOWEDITOR_API UFlowGraphNode_FormatText : public UFlowGraphNode
{
	GENERATED_UCLASS_BODY()
	// UEdGraphNode
	virtual bool IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const override;
	// --
};

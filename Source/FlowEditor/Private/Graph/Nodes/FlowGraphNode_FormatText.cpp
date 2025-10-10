// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#include "Graph/Nodes/FlowGraphNode_FormatText.h"
#include "Nodes/Developer/FlowNode_FormatText.h"

UFlowGraphNode_FormatText::UFlowGraphNode_FormatText(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
	AssignedNodeClasses = {UFlowNode_FormatText::StaticClass()};
}

bool UFlowGraphNode_FormatText::IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const
{
	if (OtherPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
	{
		OutReason = TEXT("Execution pins are not allowed for this node.");
		return true;
	}

	return false;
}

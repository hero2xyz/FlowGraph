// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#include "Graph/Nodes/FlowGraphNode_Conversion.h"
#include "Graph/Widgets/SFlowGraphNode_Variable.h"
#include "Nodes/Route/FlowNode_Conversion.h"

UFlowGraphNode_Conversion::UFlowGraphNode_Conversion(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AssignedNodeClasses = { UFlowNode_Conversion::StaticClass() };
}

TSharedPtr<SGraphNode> UFlowGraphNode_Conversion::CreateVisualWidget()
{
	return SNew(SFlowGraphNode_Variable, this);
}

bool UFlowGraphNode_Conversion::ShouldDrawNodeAsControlPointOnly(int32& OutInputPinIndex, int32& OutOutputPinIndex) const
{
	OutInputPinIndex = 0;
	OutOutputPinIndex = 0;
	return true;
}

FText UFlowGraphNode_Conversion::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText::FromName("<->");
}

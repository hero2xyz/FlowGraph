// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#include "Nodes/Graph/FlowNode_Finish.h"

#include "FlowAsset.h"
#include "Nodes/Graph/FlowNode_SubGraph.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FlowNode_Finish)

UFlowNode_Finish::UFlowNode_Finish(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
#if WITH_EDITOR
	Category = TEXT("Graph");
	NodeDisplayStyle = FlowNodeStyle::InOut;
#endif

	OutputPins = {};
	InputPins = {UFlowNode::DefaultInputPin};
	AllowedDirection = Input;
	AllowedSignalModes = {EFlowSignalMode::Enabled, EFlowSignalMode::Disabled};
}

void UFlowNode_Finish::ExecuteInput(const FName& PinName)
{
	if (!PrepareInputs())
	{
		LogError(FString::Printf(TEXT("FlowNode_Finish %s failed to prepare its inputs. Output data might be incorrect."), *GetNameSafe(this)));
	}

	const UFlowAsset* OwningAssetInstance = GetFlowAsset();
	if (!OwningAssetInstance)
	{
		LogError(TEXT("FlowNode_Finish executed without a valid Owning Flow Asset Instance."), EFlowOnScreenMessageType::Permanent);
		return;
	}

	if (UFlowNode_SubGraph* ParentSubGraphNode = OwningAssetInstance->GetNodeOwningThisAssetInstance())
	{
		ParentSubGraphNode->NotifySubGraphOutput(this, UFlowNode_SubGraph::FinishPin.PinName);
	}
	else
	{
		Finish();
	}
}

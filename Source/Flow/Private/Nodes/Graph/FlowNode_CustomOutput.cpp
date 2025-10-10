// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#include "Nodes/Graph/FlowNode_CustomOutput.h"
#include "FlowAsset.h"
#include "FlowSettings.h"
#include "Nodes/Graph/FlowNode_SubGraph.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FlowNode_CustomOutput)

#define LOCTEXT_NAMESPACE "FlowNode_CustomOutput"

UFlowNode_CustomOutput::UFlowNode_CustomOutput(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	OutputPins.Empty();
	AllowedDirection = Input;
}

void UFlowNode_CustomOutput::ExecuteInput(const FName& PinName)
{
	if (!PrepareInputs())
	{
		LogError(FString::Printf(TEXT("FlowNode_CustomOutput %s failed to prepare its inputs for event %s. Output data might be incorrect."), *GetNameSafe(this), *GetEventName().ToString()));
	}

	UFlowAsset* OwningAssetInstance = GetFlowAsset();
	if (!OwningAssetInstance)
	{
		LogError(TEXT("FlowNode_CustomOutput executed without a valid Owning Flow Asset Instance."), EFlowOnScreenMessageType::Permanent);
		return;
	}

	if (UFlowNode_SubGraph* ParentSubGraphNode = OwningAssetInstance->GetNodeOwningThisAssetInstance())
	{
		ParentSubGraphNode->NotifySubGraphOutput(this, GetEventName());
	}
	else
	{
		OwningAssetInstance->TriggerCustomOutput(GetEventName());
	}
}

#if WITH_EDITOR
FText UFlowNode_CustomOutput::GetNodeTitle() const
{
	if (!EventName.IsNone() && UFlowSettings::Get()->bUseAdaptiveNodeTitles)
	{
		return FText::Format(LOCTEXT("CustomOutputTitle", "{0} Output"), {FText::FromString(EventName.ToString())});
	}

	return Super::GetNodeTitle();
}
#endif

#undef LOCTEXT_NAMESPACE

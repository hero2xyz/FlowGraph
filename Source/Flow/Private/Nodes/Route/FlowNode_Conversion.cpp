// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#include "Nodes/Route/FlowNode_Conversion.h"
#include "FlowAsset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FlowNode_Conversion)

UFlowNode_Conversion::UFlowNode_Conversion(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
#if WITH_EDITOR
	Category = TEXT("Route");
#endif
	InputPins.Empty();
	OutputPins.Empty();

	InputPins = {FFlowPin(TEXT("In"))};
	OutputPins = {FFlowPin(TEXT("Out"))};
	AllowedSignalModes = {EFlowSignalMode::Enabled, EFlowSignalMode::Disabled};
}

void UFlowNode_Conversion::ExecuteInput(const FName& PinName)
{
	TriggerFirstOutput(true);
}

bool UFlowNode_Conversion::EvaluateAndGetOutputValue(const FName OutputPinName, FProperty*& OutProperty, const void*& OutDataPtr)
{
	OutProperty = nullptr;
	OutDataPtr = nullptr;

	if (InputConnections.IsEmpty())
	{
		LogError(FString::Printf(TEXT("Conversion node %s has no input connection to forward data from."), *GetNameSafe(this)));
		return false;
	}

	const auto It = InputConnections.CreateConstIterator();
	const FConnectedPin& SourcePinInfo = It.Value();
	if (!SourcePinInfo.IsValid())
	{
		LogError(FString::Printf(TEXT("Conversion node %s has invalid input connection data."), *GetNameSafe(this)));
		return false;
	}

	const UFlowAsset* OwningAsset = GetFlowAsset();
	if (!OwningAsset)
	{
		return false;
	}
	UFlowNode* SourceNode = OwningAsset->GetNode(SourcePinInfo.NodeGuid);
	if (!SourceNode)
	{
		LogError(FString::Printf(TEXT("Conversion node %s could not find source node %s."), *GetNameSafe(this), *SourcePinInfo.NodeGuid.ToString()));
		return false;
	}

	// Ask the actual source node for the value and its property type.
	const bool bSuccess = SourceNode->EvaluateAndGetOutputValue(SourcePinInfo.PinName, /*out*/ OutProperty, /*out*/ OutDataPtr);

#if !UE_BUILD_SHIPPING
	if (bSuccess)
	{
		RecordPinActivation(this, OutputPinName, EGPD_Output);
	}
#endif

	return bSuccess;
}

void UFlowNode_Conversion::CachePinProperties()
{
	// Conversion nodes don't cache properties themselves
}

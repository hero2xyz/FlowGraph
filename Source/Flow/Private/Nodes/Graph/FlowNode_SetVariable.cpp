// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#include "Nodes/Graph/FlowNode_SetVariable.h"
#include "FlowAsset.h"
#include "FlowSettings.h"
#include "Types/FlowPropertyUtils.h"

UFlowNode_SetVariable::UFlowNode_SetVariable()
{
#if WITH_EDITOR
	Category = TEXT("Variables");
	NodeDisplayStyle = FlowNodeStyle::Logic;
#endif

	InputPins = { UFlowNode::DefaultInputPin };
	OutputPins = { UFlowNode::DefaultOutputPin };
}

bool UFlowNode_SetVariable::PrepareInputs()
{
	Super::PrepareInputs();
	const FConnectedPin* SourcePinInfo = InputConnections.Find(VariableName);
	UFlowAsset* Asset = GetFlowAsset();
	if (!Asset)
	{
		LogError(TEXT("SetVariable: Owning FlowAsset is null during PrepareInputs."));
		return false;
	}

	if (SourcePinInfo)
	{
		UFlowNode* SourceNode = Asset->GetNode(SourcePinInfo->NodeGuid);
		if (!SourceNode)
		{
			LogError(FString::Printf(TEXT("SetVariable: Source node for variable '%s' not found."), *VariableName.ToString()));
			return false;
		}

		const void* SourceDataPtr = nullptr;
		FProperty* SourceOutputProperty = nullptr;
		if (!SourceNode->EvaluateAndGetOutputValue(SourcePinInfo->PinName, /*out*/ SourceOutputProperty, /*out*/ SourceDataPtr))
		{
			LogError(FString::Printf(TEXT("SetVariable: Evaluation failed for variable '%s' from pin '%s' on node '%s'."), *VariableName.ToString(), *SourcePinInfo->PinName.ToString(), *SourceNode->GetName()));
			return false;
		}
		
		const FPropertyBagPropertyDesc* TargetDesc = Asset->GraphVariables.FindPropertyDescByName(VariableName);
		if (!TargetDesc || !TargetDesc->CachedProperty)
		{
			LogError(FString::Printf(TEXT("SetVariable: Target variable '%s' not found in FlowAsset."), *VariableName.ToString()));
			return false;
		}
		
		const FProperty* TargetProperty = TargetDesc->CachedProperty;
		const FStructView MutableValueView = Asset->GraphVariables.GetMutableValue();
		if (!MutableValueView.IsValid())
		{
			LogError(FString::Printf(TEXT("SetVariable: Could not get mutable view for GraphVariables on asset '%s'."), *Asset->GetName()));
			return false;
		}
		void* TargetDataPtr = MutableValueView.GetMemory() + TargetProperty->GetOffset_ForInternal();

		FString TransferErrorMsg;
		const bool bAllowConversion = UFlowSettings::Get()->bAllowImplicitConversion;
		if (!FlowPropertyUtils::PerformTransfer(SourceOutputProperty, SourceDataPtr, const_cast<FProperty*>(TargetProperty), TargetDataPtr, bAllowConversion, TransferErrorMsg))
		{
			LogError(FString::Printf(TEXT("SetVariable: Failed to transfer data for variable '%s'. Reason: %s"), *VariableName.ToString(), *TransferErrorMsg));
			return false;
		}
	}
	else if (!DefaultValue.IsEmpty())
	{
		const FPropertyBagPropertyDesc* TargetDesc = Asset->GraphVariables.FindPropertyDescByName(VariableName);
		if (TargetDesc && TargetDesc->CachedProperty)
		{
			const FProperty* TargetProperty = TargetDesc->CachedProperty;
			const FStructView MutableValueView = Asset->GraphVariables.GetMutableValue();
			if (MutableValueView.IsValid())
			{
				void* TargetDataPtr = MutableValueView.GetMemory() + TargetProperty->GetOffset_ForInternal();
				const FString& ValueStr = DefaultValue;
				TargetProperty->ImportText_Direct(*ValueStr, TargetDataPtr, Asset, PPF_None);
			}
			else
			{
				LogError(FString::Printf(TEXT("SetVariable: Could not get mutable view for GraphVariables to set default value for '%s'."), *VariableName.ToString()));
				return false;
			}
		}
		else
		{
			LogError(FString::Printf(TEXT("SetVariable: Target variable '%s' not found in FlowAsset for default value assignment."), *VariableName.ToString()));
			return false;
		}
	}
	
	// If not connected and no default value, do nothing.
	return true;
}

void UFlowNode_SetVariable::ExecuteInput(const FName& PinName)
{
	TriggerFirstOutput(true);
}

// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#include "Nodes/Graph/FlowNode_GetVariable.h"
#include "FlowAsset.h"

UFlowNode_GetVariable::UFlowNode_GetVariable()
{
#if WITH_EDITOR
	Category = TEXT("Variables");
	NodeDisplayStyle = FlowNodeStyle::Logic;
#endif

	InputPins.Empty();
	OutputPins.Empty();
}

bool UFlowNode_GetVariable::EvaluateAndGetOutputValue(const FName OutputPinName, FProperty*& OutProperty, const void*& OutDataPtr)
{
	const UFlowAsset* Asset = GetFlowAsset();
	if (!Asset || OutputPinName != VariableName)
	{
		return false;
	}

	if (const FPropertyBagPropertyDesc* Desc = Asset->GraphVariables.FindPropertyDescByName(VariableName))
	{
		if (const FProperty* Prop = Desc->CachedProperty)
		{
			const FConstStructView Value = Asset->GraphVariables.GetValue();
			if (Value.IsValid())
			{
				OutProperty = const_cast<FProperty*>(Prop);
				OutDataPtr = Value.GetMemory() + Prop->GetOffset_ForInternal();
#if !UE_BUILD_SHIPPING
				RecordPinActivation(this, VariableName, EGPD_Output);
#endif
				return true;
			}
		}
	}

	return false;
}

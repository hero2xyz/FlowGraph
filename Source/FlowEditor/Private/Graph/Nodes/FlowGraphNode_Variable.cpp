// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#include "Graph/Nodes/FlowGraphNode_Variable.h"
#include "FlowAsset.h"
#include "Graph/Widgets/SFlowGraphNode_Variable.h"
#include "Nodes/Graph/FlowNode_GetVariable.h"
#include "Nodes/Graph/FlowNode_SetVariable.h"

#define LOCTEXT_NAMESPACE "FlowGraphNode_Variable"

UFlowGraphNode_GetVariable::UFlowGraphNode_GetVariable(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AssignedNodeClasses = {UFlowNode_GetVariable::StaticClass()};
}

TSharedPtr<SGraphNode> UFlowGraphNode_GetVariable::CreateVisualWidget()
{
	return SNew(SFlowGraphNode_Variable, this);
}

void UFlowGraphNode_GetVariable::AllocateDefaultPins()
{
	if (const UFlowNode_GetVariable* GetVarNode = Cast<UFlowNode_GetVariable>(GetFlowNodeBase()))
	{
		if (const UFlowAsset* Asset = GetFlowAsset())
		{
			if (const FPropertyBagPropertyDesc* Desc = Asset->GraphVariables.FindPropertyDescByName(GetVarNode->VariableName))
			{
				FEdGraphPinType PinType = GetPropertyDescAsPin(*Desc);
				CreatePin(EGPD_Output, PinType, GetVarNode->VariableName);
			}
		}
	}
}

FText UFlowGraphNode_GetVariable::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (const UFlowNode_GetVariable* GetVarNode = Cast<UFlowNode_GetVariable>(GetFlowNodeBase()))
	{
		return FText::FromName(GetVarNode->VariableName);
	}
	return Super::GetNodeTitle(TitleType);
}

bool UFlowGraphNode_GetVariable::GatherAllExpectedPins(TArray<FFlowPin>& OutInputPins,TArray<FFlowPin>& OutOutputPins) const
{
	Super::GatherAllExpectedPins(OutInputPins, OutOutputPins);
	if (const UFlowNode_GetVariable* GetVarNode = Cast<UFlowNode_GetVariable>(GetFlowNodeBase()))
	{
		if (const UFlowAsset* Asset = GetFlowAsset())
		{
			if (const FPropertyBagPropertyDesc* Desc = Asset->GraphVariables.FindPropertyDescByName(GetVarNode->VariableName))
			{
				FFlowPin NewPin = FFlowPin(GetVarNode->VariableName);
				NewPin.SetPinType(GetPropertyDescAsPin(*Desc));
				OutOutputPins.Add(NewPin);
			}
		}
	}
	return true;
}

UFlowGraphNode_SetVariable::UFlowGraphNode_SetVariable(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AssignedNodeClasses = {UFlowNode_SetVariable::StaticClass()};
}

void UFlowGraphNode_SetVariable::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, TEXT("In"));
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, TEXT("Out"));

	if (const UFlowNode_SetVariable* SetVarNode = Cast<UFlowNode_SetVariable>(GetFlowNodeBase()))
	{
		if (const UFlowAsset* Asset = GetFlowAsset())
		{
			if (const FPropertyBagPropertyDesc* Desc = Asset->GraphVariables.FindPropertyDescByName(SetVarNode->VariableName))
			{
				FEdGraphPinType PinType = GetPropertyDescAsPin(*Desc);
				UEdGraphPin* NewPin = CreatePin(EGPD_Input, PinType, SetVarNode->VariableName);
				if (NewPin && !SetVarNode->DefaultValue.IsEmpty())
				{
					GetSchema()->TrySetDefaultValue(*NewPin, SetVarNode->DefaultValue);
				}
			}
		}
	}
}

TSharedPtr<SGraphNode> UFlowGraphNode_SetVariable::CreateVisualWidget()
{
	return SNew(SFlowGraphNode_Variable, this);
}

FText UFlowGraphNode_SetVariable::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (const UFlowNode_SetVariable* SetVarNode = Cast<UFlowNode_SetVariable>(GetFlowNodeBase()))
	{
		return FText::Format(LOCTEXT("SetVariableTitle", "Set {0}"), FText::FromName(SetVarNode->VariableName));
	}
	return Super::GetNodeTitle(TitleType);
}

void UFlowGraphNode_SetVariable::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	Super::PinDefaultValueChanged(Pin);
	if (UFlowNode_SetVariable* SetVarNode = Cast<UFlowNode_SetVariable>(GetFlowNodeBase()))
	{
		if (Pin && Pin->PinName == SetVarNode->VariableName)
		{
			const FScopedTransaction Transaction(LOCTEXT("SetVariableDefaultValue", "Set Variable Default Value"));
			SetVarNode->Modify();
			if (Pin->LinkedTo.Num() == 0)
			{
				SetVarNode->DefaultValue = Pin->GetDefaultAsString();
			}
			else
			{
				SetVarNode->DefaultValue.Empty();
			}
		}
	}
}

bool UFlowGraphNode_SetVariable::GatherAllExpectedPins(TArray<FFlowPin>& OutInputPins, TArray<FFlowPin>& OutOutputPins) const
{
	Super::GatherAllExpectedPins(OutInputPins, OutOutputPins);

	if (const UFlowNode_SetVariable* SetVarNode = Cast<UFlowNode_SetVariable>(GetFlowNodeBase()))
	{
		if (const UFlowAsset* Asset = GetFlowAsset())
		{
			if (const FPropertyBagPropertyDesc* Desc = Asset->GraphVariables.FindPropertyDescByName(SetVarNode->VariableName))
			{
				FFlowPin NewPin = FFlowPin(SetVarNode->VariableName);
				NewPin.SetPinType(GetPropertyDescAsPin(*Desc));
				OutOutputPins.Add(NewPin);
			}
		}
	}
	return true;
}

#undef LOCTEXT_NAMESPACE

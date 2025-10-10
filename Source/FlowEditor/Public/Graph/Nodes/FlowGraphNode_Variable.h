// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#pragma once

#include "Graph/Nodes/FlowGraphNode.h"
#include "FlowGraphNode_Variable.generated.h"

UCLASS()
class UFlowGraphNode_GetVariable : public UFlowGraphNode
{
	GENERATED_UCLASS_BODY()
	
public:
	virtual TSharedPtr<SGraphNode> CreateVisualWidget() override;
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool GatherAllExpectedPins(TArray<FFlowPin>& OutInputPins, TArray<FFlowPin>& OutOutputPins) const override;
};

UCLASS()
class UFlowGraphNode_SetVariable : public UFlowGraphNode
{
	GENERATED_UCLASS_BODY()
	
public:
	//~ UEdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual TSharedPtr<SGraphNode> CreateVisualWidget() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	//~ End UEdGraphNode Interface

	//~ UFlowGraphNode Interface
	virtual bool GatherAllExpectedPins(TArray<FFlowPin>& OutInputPins, TArray<FFlowPin>& OutOutputPins) const override;
	//~ End UFlowGraphNode Interface
};

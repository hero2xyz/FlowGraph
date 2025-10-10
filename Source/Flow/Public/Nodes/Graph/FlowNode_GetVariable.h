// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#pragma once

#include "Nodes/FlowNode.h"
#include "FlowNode_GetVariable.generated.h"

/**
 * A pure node that retrieves the value of a graph variable.
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Get Variable"))
class FLOW_API UFlowNode_GetVariable : public UFlowNode
{
	GENERATED_BODY()

public:
	UFlowNode_GetVariable();

	/** The name of the variable to get. */
	UPROPERTY()
	FName VariableName;

protected:
	//~ UFlowNode Interface
	virtual bool IsPureNode_Implementation() const override { return true; }
	virtual bool EvaluateAndGetOutputValue(const FName OutputPinName, FProperty*& OutProperty, const void*& OutDataPtr) override;
	//~ End UFlowNode Interface
};

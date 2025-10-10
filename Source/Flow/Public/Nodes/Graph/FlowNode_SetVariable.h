// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#pragma once

#include "Nodes/FlowNode.h"
#include "FlowNode_SetVariable.generated.h"

/**
 * A node that sets the value of a graph variable.
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Set Variable"))
class FLOW_API UFlowNode_SetVariable : public UFlowNode
{
	GENERATED_BODY()

public:
	UFlowNode_SetVariable();

	/** The name of the variable to set. */
	UPROPERTY()
	FName VariableName;
	
	/** Default value to set if the input pin is not connected. */
	UPROPERTY()
	FString DefaultValue;
	
protected:
	//~ UFlowNode Interface
	virtual void ExecuteInput(const FName& PinName) override;
	virtual bool PrepareInputs() override;
	//~ End UFlowNode Interface
};

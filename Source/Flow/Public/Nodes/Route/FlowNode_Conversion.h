// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#pragma once

#include "Nodes/FlowNode.h"
#include "FlowNode_Conversion.generated.h"

/**
 * A node that visually represents a data type conversion.
 * At runtime, it behaves identically to a Reroute node, passing data through.
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Conversion Node"))
class FLOW_API UFlowNode_Conversion : public UFlowNode
{
	GENERATED_UCLASS_BODY()
	
protected:
	virtual void ExecuteInput(const FName& PinName) override;
	virtual bool EvaluateAndGetOutputValue(const FName OutputPinName, FProperty*& OutProperty, const void*& OutDataPtr) override;
	virtual void CachePinProperties() override;
};

// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#pragma once

#include "Nodes/Graph/FlowNode_DefineProperties.h"
#include "FlowNode_Start.generated.h"

/**
 * Execution of the graph always starts from this node
 */
UCLASS(NotBlueprintable, NotPlaceable, meta = (DisplayName = "Start"))
class FLOW_API UFlowNode_Start : public UFlowNode_DefineProperties
{
	GENERATED_UCLASS_BODY()

	friend class UFlowAsset;

public:
	// IFlowCoreExecutableInterface
	virtual void ExecuteInput(const FName& PinName) override;
	virtual bool IsPureNode_Implementation() const override { return false; }
	// --
#if WITH_EDITOR
	virtual bool CanNotifySubGraphs() const override { return true; }
#endif
};

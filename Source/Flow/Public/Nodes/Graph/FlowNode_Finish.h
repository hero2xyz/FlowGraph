// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#pragma once

#include "FlowNode_DefineProperties.h"
#include "FlowNode_Finish.generated.h"

/**
 * Finish execution of this Flow Asset
 * All active nodes and sub graphs will be deactivated
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Finish"))
class FLOW_API UFlowNode_Finish : public UFlowNode_DefineProperties
{
	GENERATED_UCLASS_BODY()

protected:
	virtual bool CanFinishGraph() const override { return true; }
	virtual void ExecuteInput(const FName& PinName) override;
	virtual bool IsPureNode_Implementation() const override { return false; }
#if WITH_EDITOR
	virtual bool CanNotifySubGraphs() const override { return true; }
#endif
};

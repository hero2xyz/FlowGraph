// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#pragma once

#include "FlowNode_DefineProperties.h"
#include "FlowNode_CustomEventBase.generated.h"

/**
 * Base class for nodes used to receive/send events between graphs
 */
UCLASS(Abstract, NotBlueprintable)
class FLOW_API UFlowNode_CustomEventBase : public UFlowNode_DefineProperties
{
	GENERATED_UCLASS_BODY()

protected:
	UPROPERTY()
	FName EventName;

public:
	void SetEventName(const FName& InEventName);
	const FName& GetEventName() const { return EventName; }

	virtual bool IsPureNode_Implementation() const override { return false; }
#if WITH_EDITOR
public:
	virtual FString GetNodeDescription() const override;
	virtual EDataValidationResult ValidateNode() override;
	virtual bool CanNotifySubGraphs() const override { return true; }
#endif
};

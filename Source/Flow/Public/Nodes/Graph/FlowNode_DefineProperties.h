// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#pragma once

#include "Nodes/FlowNode.h"
#include "StructUtils/PropertyBag.h"

#include "FlowNode_DefineProperties.generated.h"

UENUM(BlueprintType)
enum EFlowPropertyPinDirection : uint8
{
	Input = 0,  // Inputs only
	Output = 1, // Output only
	Both = 2,   // Inputs and Outputs
};

/**
 * Flow Node that allows defining a dynamic set of input and/or output data pins using Property Bags.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Define Properties"))
class FLOW_API UFlowNode_DefineProperties : public UFlowNode
{
	GENERATED_UCLASS_BODY()
public:
	FInstancedPropertyBag& GetInputProperties() { return InputProperties; }
	const FInstancedPropertyBag& GetInputProperties() const { return InputProperties; }
	FInstancedPropertyBag& GetOutputProperties() { return OutputProperties; }
	const FInstancedPropertyBag& GetOutputProperties() const { return OutputProperties; }
	EFlowPropertyPinDirection GetAllowedDirection() const { return AllowedDirection; }

private:

	/** The set of properties exposed as Output data pins by this node. */
	UPROPERTY(EditAnywhere, Category = "Properties", meta = (ChildRowFeatures = "Extended", EditCondition = "AllowedDirection==1 || AllowedDirection==2", EditConditionHides, FlowPropertyBag = "Output"))
	FInstancedPropertyBag OutputProperties;
	
	/** The set of properties exposed as Input data pins by this node. */
	UPROPERTY(EditAnywhere, Category = "Properties", meta = (ChildRowFeatures = "Extended", EditCondition = "AllowedDirection==0 || AllowedDirection==2", EditConditionHides, FlowPropertyBag = "Input"))
	FInstancedPropertyBag InputProperties;

protected:
	virtual bool IsPureNode_Implementation() const override { return true; }
	virtual void CachePinProperties() override;
	virtual void* GetPropertyContainer(const FProperty* Property) const override;
#if WITH_EDITOR
	virtual FString GetNodeCategory() const override { return TEXT("Data"); }
	virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;
	virtual bool CanNotifySubGraphs() const { return false; }
#endif

	UPROPERTY(EditDefaultsOnly, Category = "Properties")
	TEnumAsByte<EFlowPropertyPinDirection> AllowedDirection = Output;
};

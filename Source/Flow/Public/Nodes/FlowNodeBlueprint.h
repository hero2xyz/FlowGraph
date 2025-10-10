// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#pragma once

#include "FlowTypes.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "FlowNodeBlueprint.generated.h"

UCLASS(BlueprintType)
class FLOW_API UFlowNodeBaseBlueprint : public UBlueprint
{
	GENERATED_UCLASS_BODY()

public:
#if WITH_EDITOR
	// UBlueprint interface
	virtual UClass* GetBlueprintClass() const override;
	// End UBlueprint interface
	
	virtual bool SupportedByDefaultBlueprintFactory() const override { return false; }
	virtual bool SupportsDelegates() const override { return false; }
	virtual bool CanAlwaysRecompileWhilePlayingInEditor() const override { return true; }
#endif
};

/**
 * Flow Node Blueprint class
 */
UCLASS()
class FLOW_API UFlowNodeBlueprint : public UFlowNodeBaseBlueprint
{
	GENERATED_BODY()
};

/**
 * Blueprint generated class for all UFlowNodeBaseBlueprint-Derived types
 * Stores configuration for variables marked as data pins via metadata.
 */
UCLASS()
class FLOW_API UFlowNodeBaseBlueprintGeneratedClass : public UBlueprintGeneratedClass
{
	GENERATED_BODY()

public:
	/** Configuration map for Flow variables. */
	UPROPERTY()
	TMap<FName, FFlowVarConfig> FlowVarSettings;
};

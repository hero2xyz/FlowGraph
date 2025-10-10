// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#include "Nodes/FlowNodeBlueprint.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FlowNodeBlueprint)

UFlowNodeBaseBlueprint::UFlowNodeBaseBlueprint(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

#if WITH_EDITOR
UClass* UFlowNodeBaseBlueprint::GetBlueprintClass() const
{
	return UFlowNodeBaseBlueprintGeneratedClass::StaticClass();
}
#endif

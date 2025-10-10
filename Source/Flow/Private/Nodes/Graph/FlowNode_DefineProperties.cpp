// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#include "Nodes/Graph/FlowNode_DefineProperties.h"

#include "FlowAsset.h"
#include "Types/FlowPropertyUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FlowNode_DefineProperties)

UFlowNode_DefineProperties::UFlowNode_DefineProperties(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
#if WITH_EDITOR
	NodeDisplayStyle = FlowNodeStyle::InOut;
	Category = TEXT("Graph");
#endif

	InputPins.Empty();
	OutputPins.Empty();
	AllowedSignalModes = {EFlowSignalMode::Enabled, EFlowSignalMode::Disabled};
}

void UFlowNode_DefineProperties::CachePinProperties()
{
	if (AllowedDirection == Input || AllowedDirection == Both)
	{
		InputPropertyCache = PropertyBagUtils::GetCachedPropertiesFromBag(InputProperties);
	}
	if (AllowedDirection == Output || AllowedDirection == Both)
	{
		OutputPropertyCache = PropertyBagUtils::GetCachedPropertiesFromBag(OutputProperties);
	}
}

void* UFlowNode_DefineProperties::GetPropertyContainer(const FProperty* Property) const
{
	if (void* BagContainer = PropertyBagUtils::FindPropertyBagMemoryForProperty(Property, {&InputProperties, &OutputProperties}); BagContainer != nullptr)
	{
		return BagContainer;
	}
	
	return Super::GetPropertyContainer(Property);
}

#if WITH_EDITOR
void UFlowNode_DefineProperties::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
	const FName ChangedPropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (ChangedPropertyName == GET_MEMBER_NAME_CHECKED(UFlowNode_DefineProperties, InputProperties) || ChangedPropertyName == GET_MEMBER_NAME_CHECKED(UFlowNode_DefineProperties, OutputProperties))
	{
		if (CanNotifySubGraphs())
		{
			if (const UFlowAsset* OwningAsset = GetFlowAsset())
			{
				OwningAsset->OnSubGraphReconstructionRequested.Broadcast();
			}
		}
		OnReconstructionRequested.ExecuteIfBound();
	}
}
#endif
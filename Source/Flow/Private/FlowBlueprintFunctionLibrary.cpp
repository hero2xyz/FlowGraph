// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#include "FlowBlueprintFunctionLibrary.h"
#include "FlowSubsystem.h"
#include "FlowAsset.h"
#include "FlowLogChannels.h"
#include "Nodes/Graph/FlowNode_DefineProperties.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/PropertyBag.h"
#include "Engine/GameInstance.h"
#include "UObject/Script.h"
#include "Blueprint/BlueprintExceptionInfo.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FlowBlueprintFunctionLibrary)

#define LOCTEXT_NAMESPACE "FlowBlueprintFunctionLibrary"

FInstancedStruct UFlowBlueprintFunctionLibrary::CreateFlowParameters(UFlowAsset* FlowAsset)
{
    if (!FlowAsset)
    {
        return FInstancedStruct();
    }
    const UFlowNode_DefineProperties* EntryNode = Cast<UFlowNode_DefineProperties>(FlowAsset->GetDefaultEntryNode());
    if (!EntryNode)
    {
        return FInstancedStruct();
    }

    FInstancedStruct RuntimeParametersStruct;
    RuntimeParametersStruct.InitializeAs(FInstancedPropertyBag::StaticStruct());
    if (FInstancedPropertyBag* DestinationBag = RuntimeParametersStruct.GetMutablePtr<FInstancedPropertyBag>())
    {
        const FInstancedPropertyBag& SourceBag = EntryNode->GetOutputProperties();
        *DestinationBag = SourceBag; 
    }
    
    return RuntimeParametersStruct;
}

void UFlowBlueprintFunctionLibrary::K2_SetFlowParameterProperty(FInstancedStruct&, FGuid, const int32&)
{
    checkNoEntry();
}

DEFINE_FUNCTION(UFlowBlueprintFunctionLibrary::execK2_SetFlowParameterProperty)
{
    P_GET_STRUCT_REF(FInstancedStruct, Parameters);
    P_GET_STRUCT(FGuid, PropertyID);

    Stack.StepCompiledIn<FProperty>(nullptr);
    const FProperty* SourceProperty = Stack.MostRecentProperty;
    const void* SourcePtr = Stack.MostRecentPropertyAddress;
    
    P_FINISH;

    if (!SourceProperty || !SourcePtr)
    {
        FBlueprintExceptionInfo ExceptionInfo(EBlueprintExceptionType::AbortExecution, LOCTEXT("SetFlowParameter_InvalidValue", "Failed to resolve Value for Set Flow Parameter."));
        FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
        return;
    }

    P_NATIVE_BEGIN;
    if (FInstancedPropertyBag* Bag = Parameters.GetMutablePtr<FInstancedPropertyBag>())
    {
        const FStructView PropertyBagView = Bag->GetMutableValue();
        const UPropertyBag* PropertyBagStruct = Bag->GetPropertyBagStruct();
        if (PropertyBagView.IsValid() && PropertyBagStruct)
        {
            if (const FPropertyBagPropertyDesc* Desc = PropertyBagStruct->FindPropertyDescByID(PropertyID))
            {
                if (const FProperty* TargetProperty = PropertyBagStruct->FindPropertyByName(Desc->Name))
                {
                    if (SourceProperty->SameType(TargetProperty))
                    {
                        void* TargetPtr = TargetProperty->ContainerPtrToValuePtr<void>(PropertyBagView.GetMemory());
                        TargetProperty->CopyCompleteValue(TargetPtr, SourcePtr);
                    }
                }
            }
        }
    }

    P_NATIVE_END;
}

UFlowAsset* UFlowBlueprintFunctionLibrary::StartFlowWithParameters(UObject* WorldContextObject, UObject* Owner, UFlowAsset* FlowAsset, const FInstancedStruct& Parameters)
{
    if (!WorldContextObject || !Owner || !FlowAsset || !Parameters.IsValid())
    {
        UE_LOG(LogFlow, Warning, TEXT("UFlowBlueprintFunctionLibrary::StartFlowWithParameters: Received invalid arguments (WorldContext, Owner, FlowAsset, or Parameters struct)."));
        return nullptr;
    }

    UFlowSubsystem* FlowSubsystem = UGameInstance::GetSubsystem<UFlowSubsystem>(WorldContextObject->GetWorld()->GetGameInstance());
    if (const FInstancedPropertyBag* Bag = Parameters.GetPtr<FInstancedPropertyBag>())
    {
        return FlowSubsystem->StartRootFlowWithParameters(Owner, FlowAsset, *Bag, true);
    }
    return nullptr;
}

#undef LOCTEXT_NAMESPACE
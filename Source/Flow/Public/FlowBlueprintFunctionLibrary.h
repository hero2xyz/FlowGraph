// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "StructUtils/InstancedStruct.h"
#include "FlowBlueprintFunctionLibrary.generated.h"

class UFlowAsset;
class UFlowSubsystem;

UCLASS(MinimalAPI, Abstract)
class UFlowBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Creates an empty, but valid, FInstancedPropertyBag based on the parameter structure of a Flow Asset's entry node.*/
    UFUNCTION(BlueprintCallable, Category = "Flow", meta = (BlueprintInternalUseOnly = "true"))
    static FLOW_API FInstancedStruct CreateFlowParameters(UFlowAsset* FlowAsset);

    /** Sets a single property within a Flow parameter bag. */
    UFUNCTION(BlueprintCallable, CustomThunk, Category = "Flow", meta = (BlueprintInternalUseOnly = "true", CustomStructureParam = "Value"))
    static FLOW_API void K2_SetFlowParameterProperty(UPARAM(ref) FInstancedStruct& Parameters, FGuid PropertyID, UPARAM(ref) const int32& Value);
    
    /** Starts a Flow Graph using a pre-filled parameter bag.   */
    UFUNCTION(BlueprintCallable, Category = "Flow", meta = (WorldContext = "WorldContextObject", BlueprintInternalUseOnly = "true"))
    static FLOW_API UFlowAsset* StartFlowWithParameters(UObject* WorldContextObject, UObject* Owner, UFlowAsset* FlowAsset, const FInstancedStruct& Parameters);

private:
    DECLARE_FUNCTION(execK2_SetFlowParameterProperty);
};

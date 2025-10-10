// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#pragma once

#include "Nodes/FlowNode.h"
#include "FlowNode_FormatText.generated.h"

/**
 * Formats text by replacing placeholders with values from input pins.
 * Placeholders use the format {Name}.
 * Input pins are dynamically generated based on these placeholders.
 */
UCLASS(meta = (DisplayName = "Format Text"))
class FLOW_API UFlowNode_FormatText : public UFlowNode
{
	GENERATED_UCLASS_BODY()

public:
	/** The format string containing placeholders like {Name} or {Count}. */
	UPROPERTY(EditAnywhere, Category = "Format", meta = (FlowDataPin = "Input", MultiLine = true))
	FText FormatString;

	/** The resulting formatted text. */
	UPROPERTY(VisibleAnywhere, Category = "Format", meta = (FlowDataPin = "Output"))
	FText FormattedText;

protected:
	//~ Begin UFlowNode Interface
#if WITH_EDITOR
	virtual void GetDynamicPins(TArray<FFlowPin>& OutDynamicInputPins, TArray<FFlowPin>& OutDynamicOutputPins) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual bool PrepareInputs() override;
	virtual void CachePinProperties() override;
	virtual bool PerformPureCalculation_Implementation() override;
	virtual bool IsPureNode_Implementation() const override { return true; }
	//~ End UFlowNode Interface

#if WITH_EDITOR
	virtual void UpdateNodeConfigText_Implementation() override;
#endif

private:
	/** Extracts placeholder names (without braces) from the FormatString. */
	TArray<FString> ExtractPlaceholders() const;

	/** Helper to convert a generic FProperty value to FText for FFormatArgumentValue. */
	static FText GetPropertyValueAsText(const FProperty* Property, const void* DataPtr);

	/** Transient storage for input values gathered during the PrepareInputs phase for pure node evaluation. */
	UPROPERTY(Transient)
	TMap<FName, FText> ResolvedInputValues;
};

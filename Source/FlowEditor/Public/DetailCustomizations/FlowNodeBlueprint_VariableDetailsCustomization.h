// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "UObject/WeakFieldPtr.h"

class IDetailLayoutBuilder;
class IBlueprintEditor;
class UBlueprint;

/** Detail customization to add specific metadata to blueprint variables */
class FFlowNodeBlueprint_VariableDetailsCustomization : public IDetailCustomization
{
public:
	/** Metadata key for the variable behavior type (None/Input/Output) */
	static const FName MetadataKeyEnum;

	/** Factory method to create an instance of this class */
	static TSharedPtr<IDetailCustomization> MakeInstance(TSharedPtr<IBlueprintEditor> InBlueprintEditor);

	/** Constructor */
	FFlowNodeBlueprint_VariableDetailsCustomization(TSharedPtr<IBlueprintEditor> InBlueprintEditor, UBlueprint* Blueprint)
	    : BlueprintEditorPtr(InBlueprintEditor), BlueprintPtr(Blueprint)
	{
		InitEnumOptions();
	}

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
	//
private:
	/** Checks if the current Blueprint is a Flow Node */
	bool IsFlowNodeBlueprint() const;

	/** Initialize the options for the enum */
	void InitEnumOptions();

	/** Add the UI for the enum metadata */
	void AddEnumMetadataRow(IDetailLayoutBuilder& DetailLayout, FName VarName);

	/** Callback when the enum value changes */
	void OnEnumValueChanged(const int32 NewValue, const FName VarName);

	/** Gets the current enum index */
	int32 GetCurrentEnumIndex(FName VarName) const;

private:
	/** The Blueprint editor we are embedded in */
	TWeakPtr<IBlueprintEditor> BlueprintEditorPtr;

	/** The blueprint we are editing */
	TWeakObjectPtr<UBlueprint> BlueprintPtr;

	/** Available options for the enum */
	TArray<TSharedPtr<FString>> EnumOptions;
};

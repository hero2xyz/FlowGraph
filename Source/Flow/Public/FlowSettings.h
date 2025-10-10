// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#pragma once

#include "Engine/DeveloperSettings.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPath.h"
#include "FlowSettings.generated.h"

class UFlowNode;

/**
 *
 */
UCLASS(Config = Game, defaultconfig, meta = (DisplayName = "Flow"))
class FLOW_API UFlowSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

	static UFlowSettings* Get() { return CastChecked<UFlowSettings>(UFlowSettings::StaticClass()->GetDefaultObject()); }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// Set if to False, if you don't want to create client-side Flow Graphs
	// And you don't access to the Flow Component registry on clients
	UPROPERTY(Config, EditAnywhere, Category = "Networking")
	bool bCreateFlowSubsystemOnClients;

	UPROPERTY(Config, EditAnywhere, Category = "SaveSystem")
	bool bWarnAboutMissingIdentityTags;

	// If enabled, runtime logs will be added when a flow node signal mode is set to Disabled
	UPROPERTY(Config, EditAnywhere, Category = "Flow")
	bool bLogOnSignalDisabled;

	// If enabled, runtime logs will be added when a flow node signal mode is set to Pass-through
	UPROPERTY(Config, EditAnywhere, Category = "Flow")
	bool bLogOnSignalPassthrough;

	// Adjust the Titles for FlowNodes to be more expressive than default
	// by incorporating data that would otherwise go in the Description
	UPROPERTY(EditAnywhere, config, Category = "Nodes")
	bool bUseAdaptiveNodeTitles;

	/**
	 * Allow connecting data pins where an implicit conversion is possible (e.g., Int to Float, String to Name)
	 * If false, only strictly compatible types can be connected. */
	UPROPERTY(EditAnywhere, config, Category = "Connections", meta = (DisplayName = "Allow Implicit Pin Type Conversions"))
	bool bAllowImplicitConversion = true;

	/**
	 * Allows visual connections between Object and Interface pins (in both directions) when bAllowImplicitConversion is true.
	 * If enabled, the Flow schema allows the connection based on interface validity and relies on runtime checks for compatibility.
	 * If disabled, K2's rules determine compatibility.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Connections", meta = (EditCondition = "bAllowImplicitConversion"))
	bool bAllowObjectInterfaceConnections = false;

#if WITH_EDITOR
	DECLARE_DELEGATE(FFlowSettingsEvent);
	FFlowSettingsEvent OnAdaptiveNodeTitlesChanged;
#endif
	
	// Default class to use as a FlowAsset's "ExpectedOwnerClass" 
	UPROPERTY(EditAnywhere, Config, Category = "Nodes", meta = (MustImplement = "/Script/Flow.FlowOwnerInterface"))
	FSoftClassPath DefaultExpectedOwnerClass;

public:
	UClass* GetDefaultExpectedOwnerClass() const;

	static UClass* TryResolveOrLoadSoftClass(const FSoftClassPath& SoftClassPath);

#if WITH_EDITORONLY_DATA
	virtual FName GetCategoryName() const override { return FName("Flow Graph"); }
	virtual FText GetSectionText() const override { return INVTEXT("Settings"); }
#endif
};

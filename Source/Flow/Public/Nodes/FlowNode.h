// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#pragma once

#include "EdGraph/EdGraphNode.h"
#include "GameplayTagContainer.h"
#include "UObject/TextProperty.h"
#include "VisualLogger/VisualLoggerDebugSnapshotInterface.h"

#include "FlowNodeBase.h"
#include "FlowTypes.h"
#include "Nodes/FlowPin.h"

#include "FlowNode.generated.h"

#if WITH_EDITOR
DECLARE_DELEGATE_OneParam(FOnNodePropertyChanged, const FProperty* /* PropertyChanged */);
#endif

/**
 * A Flow Node is UObject-based node designed to handle entire gameplay feature within single node.
 */
UCLASS(Abstract, Blueprintable, HideCategories = Object)
class FLOW_API UFlowNode : public UFlowNodeBase, public IVisualLoggerDebugSnapshotInterface
{
	GENERATED_UCLASS_BODY()

	friend class SFlowGraphNode;
	friend class UFlowAsset;
	friend class UFlowGraphNode;
	friend class UFlowNodeAddOn;
	friend class SFlowInputPinHandle;
	friend class SFlowOutputPinHandle;

	//////////////////////////////////////////////////////////////////////////
	// Node

#if WITH_EDITORONLY_DATA

protected:
	UPROPERTY()
	TArray<TSubclassOf<UFlowAsset>> AllowedAssetClasses;

	UPROPERTY()
	TArray<TSubclassOf<UFlowAsset>> DeniedAssetClasses;
#endif

public:
	// UFlowNodeBase
	virtual void InitializeInstance() override;
	virtual UFlowNode* GetFlowNodeSelfOrOwner() override { return this; }
	virtual bool IsSupportedInputPinName(const FName& PinName) const override;
	// --

public:
	// UObject
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostLoad() override;
	// --

	virtual EDataValidationResult ValidateNode() { return EDataValidationResult::NotValidated; }

	/** Delegate broadcasted when a property is changed on this node instance in the editor. */
	FOnNodePropertyChanged OnPropertyChangedEvent;

	/** Called when a property value has been changed externally. */
	virtual void OnPropertyChanged(FName PropertyName);

	/**
	 * Allows a node to declare dynamic input/output pins that are
	 * not derived directly from a UPROPERTY or fixed pin.
	 * This will be called during the node reconstruction.
	 * The FFlowPin::PinType provided here can be a base type (e.g., Wildcard).
	 *
	 * @param OutDynamicInputPins Array to populate with FFlowPin definitions for dynamic input pins.
	 * @param OutDynamicOutputPins Array to populate with FFlowPin definitions for dynamic output pins.
	 */
	virtual void GetDynamicPins(TArray<FFlowPin>& OutDynamicInputPins, TArray<FFlowPin>& OutDynamicOutputPins) const {}
#endif

	// Inherits Guid after graph node
	UPROPERTY()
	FGuid NodeGuid;

public:
	UFUNCTION(BlueprintCallable, Category = "FlowNode")
	void SetGuid(const FGuid& NewGuid) { NodeGuid = NewGuid; }

	UFUNCTION(BlueprintPure, Category = "FlowNode")
	const FGuid& GetGuid() const { return NodeGuid; }

	// Returns a random seed suitable for this flow node,
	// by default based on the node Guid,
	// but may be overridden in subclasses to supply some other value.
	virtual int32 GetRandomSeed() const override { return GetTypeHash(NodeGuid); }

public:
	virtual bool CanFinishGraph() const { return false; }

protected:
	UPROPERTY(EditDefaultsOnly, Category = "FlowNode")
	TArray<EFlowSignalMode> AllowedSignalModes;

	// If enabled, signal will pass through node without calling ExecuteInput()
	// Designed to handle patching
	UPROPERTY()
	EFlowSignalMode SignalMode;

	//////////////////////////////////////////////////////////////////////////
	// All created pins (default, class-specific and added by user)

public:
	static FFlowPin DefaultInputPin;
	static FFlowPin DefaultOutputPin;

	// Class-specific and user-added inputs
	UPROPERTY(EditDefaultsOnly, Category = "FlowNode")
	TArray<FFlowPin> InputPins;

	// Class-specific and user-added outputs
	UPROPERTY(EditDefaultsOnly, Category = "FlowNode")
	TArray<FFlowPin> OutputPins;

	void AddInputPins(const TArray<FFlowPin>& Pins);
	void AddOutputPins(const TArray<FFlowPin>& Pins);

#if WITH_EDITOR
	// Utility function to rebuild a pin array in editor (either InputPins or OutputPins, passed as InOutPins)
	// returns true if the InOutPins array was rebuilt
	bool RebuildPinArray(const TArray<FName>& NewPinNames, TArray<FFlowPin>& InOutPins, const FFlowPin& DefaultPin);
	bool RebuildPinArray(const TArray<FFlowPin>& NewPins, TArray<FFlowPin>& InOutPins, const FFlowPin& DefaultPin);
#endif // WITH_EDITOR;

	// always use default range for nodes with user-created outputs i.e. Execution Sequence
	void SetNumberedInputPins(const uint8 FirstNumber = 0, const uint8 LastNumber = 1);
	void SetNumberedOutputPins(const uint8 FirstNumber = 0, const uint8 LastNumber = 1);

	uint8 CountNumberedInputs() const;
	uint8 CountNumberedOutputs() const;

public:
	const TArray<FFlowPin>& GetInputPins() const { return InputPins; }
	const TArray<FFlowPin>& GetOutputPins() const { return OutputPins; }

	UFUNCTION(BlueprintPure, Category = "FlowNode")
	TArray<FName> GetInputNames() const;

	UFUNCTION(BlueprintPure, Category = "FlowNode")
	TArray<FName> GetOutputNames() const;

#if WITH_EDITOR
	// IFlowContextPinSupplierInterface
	virtual bool SupportsContextPins() const override;
	virtual TArray<FFlowPin> GetContextInputs() const override;
	virtual TArray<FFlowPin> GetContextOutputs() const override;
	// --

	virtual bool CanUserAddInput() const;
	virtual bool CanUserAddOutput() const;

	void RemoveUserInput(const FName& PinName);
	void RemoveUserOutput(const FName& PinName);
#endif

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "FlowNode", meta = (DisplayName = "Can User Add Input"))
	bool K2_CanUserAddInput() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "FlowNode", meta = (DisplayName = "Can User Add Output"))
	bool K2_CanUserAddOutput() const;

	//////////////////////////////////////////////////////////////////////////
	// Connections to other nodes

public:
	/** Map input pins TO the node and output pin they are connected FROM. */
	UPROPERTY()
	TMap<FName, FConnectedPin> InputConnections;

	/**
	 * Map output pins TO a list of nodes and input pins they are connected TO.
	 * Supports 1:n connections for data pins.
	 * */
	UPROPERTY()
	TMap<FName, FPinConnectionList> OutputConnections;

	// --- Connection Accessors ---

	/**
	 * Gets the connection info for a specific Input Pin (source node/pin), if connected.
	 * @param InputPinName The name of the input pin on this node.
	 * @return An TOptional containing the connection info if found, otherwise an unset TOptional.
	 */
	TOptional<FConnectedPin> GetInputConnection(const FName InputPinName) const;

	/**
	 * Gets all connection targets for a specific Output Pin.
	 * @param OutputPinName The name of the output pin on this node.
	 * @return An array containing connection info for all targets. Empty if the pin is not connected or doesn't exist.
	 */
	TArray<FConnectedPin> GetOutputConnections(const FName OutputPinName) const;

	/** Checks if the specified Input Pin has any connections TO it. */
	UFUNCTION(BlueprintPure, Category = "FlowNode")
	bool IsInputConnected(const FName& PinName, bool bErrorIfPinNotFound = true);

	/** Checks if the specified Output Pin has any connections FROM it. */
	UFUNCTION(BlueprintPure, Category = "FlowNode")
	bool IsOutputConnected(const FName& PinName, bool bErrorIfPinNotFound = true);

	/** Finds the name of the Input Pin on this node connected FROM the specified node Guid. */
	UFUNCTION(BlueprintPure, Category = "FlowNode")
	FName GetInputPinConnectedFromNode(const FGuid& SourceNodeGuid) const;

	/** Finds the name(s) of the Output Pin(s) on this node connected TO the specified node Guid. */
	UFUNCTION(BlueprintPure, Category = "FlowNode")
	TArray<FName> GetOutputPinsConnectedToNode(const FGuid& TargetNodeGuid) const;

	/** Gathers all unique Flow Nodes directly connected FROM the output pins of this node. */
	UFUNCTION(BlueprintPure, Category = "FlowNode")
	TSet<UFlowNode*> GatherConnectedNodes() const;

	FFlowPin* FindInputPinByName(const FName& PinName);
	FFlowPin* FindInputPinByName(const FName& PinName) const { return const_cast<UFlowNode*>(this)->FindInputPinByName(PinName); }
	FFlowPin* FindOutputPinByName(const FName& PinName);
	FFlowPin* FindOutputPinByName(const FName& PinName) const { return const_cast<UFlowNode*>(this)->FindOutputPinByName(PinName); }

	static void RecursiveFindNodesByClass(UFlowNode* Node, const TSubclassOf<UFlowNode> Class, uint8 Depth, TArray<UFlowNode*>& OutNodes);

	//////////////////////////////////////////////////////////////////////////
	// Data

	/** Cache of Input Data Pin FProperty pointers.  */
	TMap<FName, FProperty*> InputPropertyCache;

	/** Cache of Output Data Pin FProperty pointers. */
	TMap<FName, FProperty*> OutputPropertyCache;

	/**
	 * Checks if this node calculates outputs on demand without execution pins.
	 * Override to return true for pure nodes.
	 * @return True if the node is pure, false otherwise. Default is false.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "FlowNode")
	bool IsPureNode() const;

protected:
	/**
	 * Caches FProperty pointers for members marked as Flow Data Pins "meta=(FlowDataPin="Input"/"Output")"
	 * Blueprint-based nodes have this handled automatically.
	 * C++-based nodes MUST override this function and explicitly register their data pins
	 * using the DECLARE_INPUT_PIN/DECLARE_OUTPUT_PIN macros to ensure they function correctly.
	 */
	virtual void CachePinProperties();

	/** Get the appropriate container for a property.*/
	virtual void* GetPropertyContainer(const FProperty* Property) const;

	/**
	 * Ensures all input data properties connected via InputConnections have their data
	 * fetched/calculated by recursively evaluating source nodes. Called automatically by TriggerInput.
	 * @return True if all required inputs were successfully prepared, false otherwise.
	 */
	virtual bool PrepareInputs();

	/**
	 * Performs the specific calculation logic for a PURE node.
	 * Implement this event in Blueprints that overrides IsPureNode to return true.
	 * @note Assumes inputs have already been prepared. Read input properties and write to output properties here.
	 * @return True if the calculation was successful, false otherwise.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "FlowNode", meta = (DisplayName = "Perform Pure Calculation"))
	bool PerformPureCalculation();

public:
	/**
	 * Evaluates this node (if pure and necessary) and provides access to the value of a specified output property.
	 * This is called by the PrepareInputs function of a node connected to this output pin.
	 * @param OutputPinName The name of the output pin/property to evaluate.
	 * @param OutProperty [out] Receives the FProperty* of the output pin if found.
	 * @param OutDataPtr [out] Receives a const void* pointer directly to the data of the output property within this node instance.
	 * @return True if the evaluation was successful and OutProperty/OutDataPtr are valid, false otherwise.
	 */
	virtual bool EvaluateAndGetOutputValue(const FName OutputPinName, FProperty*& OutProperty, const void*& OutDataPtr);

	/** Helper to safely get a cached input property pointer. */
	FProperty* GetInputProperty(const FName PinName) const;

	/** Helper to safely get a cached output property pointer. */
	FProperty* GetOutputProperty(const FName PinName) const;
	//////////////////////////////////////////////////////////////////////////
	// Debugger

protected:
	static FString MissingIdentityTag;
	static FString MissingNotifyTag;
	static FString MissingClass;
	static FString NoActorsFound;

	//////////////////////////////////////////////////////////////////////////
	// Executing node instance

public:
	bool bPreloaded;

protected:
	UPROPERTY(SaveGame)
	EFlowNodeState ActivationState;

public:
	EFlowNodeState GetActivationState() const { return ActivationState; }
	bool HasFinished() const { return EFlowNodeState_Classifiers::IsFinishedState(ActivationState); }

#if !UE_BUILD_SHIPPING
	TMap<FName, TArray<FPinRecord>> InputRecords;
	TMap<FName, TArray<FPinRecord>> OutputRecords;
#endif

public:
	void TriggerPreload();
	void TriggerFlush();

protected:
	// Trigger execution of input pin
	void TriggerInput(const FName& PinName, const EFlowPinActivationType ActivationType = EFlowPinActivationType::Default);

protected:
	void Deactivate();

public:
	virtual void TriggerFirstOutput(const bool bFinish) override;
	virtual void TriggerOutput(FName PinName, const bool bFinish = false, const EFlowPinActivationType ActivationType = EFlowPinActivationType::Default) override;

	virtual void Finish() override;

private:
	void ResetRecords();

	//////////////////////////////////////////////////////////////////////////
	// SaveGame support

public:
	UFUNCTION(BlueprintCallable, Category = "FlowNode")
	void SaveInstance(FFlowNodeSaveData& NodeRecord);

	UFUNCTION(BlueprintCallable, Category = "FlowNode")
	void LoadInstance(const FFlowNodeSaveData& NodeRecord);

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "FlowNode")
	void OnSave();

	UFUNCTION(BlueprintNativeEvent, Category = "FlowNode")
	void OnLoad();

	UFUNCTION(BlueprintNativeEvent, Category = "FlowNode")
	void OnPassThrough();

	//////////////////////////////////////////////////////////////////////////
	// Utils

#if WITH_EDITOR
public:
	UFlowNode* GetInspectedInstance() const;
	TArray<FPinRecord> GetPinRecords(const FName& PinName, const EEdGraphPinDirection PinDirection) const;

	// Information displayed while node is working - displayed over node as NodeInfoPopup
	FString GetStatusStringForNodeAndAddOns() const;
	virtual bool GetStatusBackgroundColor(FLinearColor& OutColor) const;

	virtual FString GetAssetPath();
	virtual UObject* GetAssetToEdit();
	virtual AActor* GetActorToFocus();
#endif

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "FlowNode", meta = (DisplayName = "Get Status Background Color"))
	bool K2_GetStatusBackgroundColor(FLinearColor& OutColor) const;

	UFUNCTION(BlueprintImplementableEvent, Category = "FlowNode", meta = (DisplayName = "Get Asset Path"))
	FString K2_GetAssetPath();

	UFUNCTION(BlueprintImplementableEvent, Category = "FlowNode", meta = (DisplayName = "Get Asset To Edit"))
	UObject* K2_GetAssetToEdit();

	UFUNCTION(BlueprintImplementableEvent, Category = "FlowNode", meta = (DisplayName = "Get Actor To Focus"))
	AActor* K2_GetActorToFocus();

public:
	UFUNCTION(BlueprintPure, Category = "FlowNode")
	static FString GetIdentityTagDescription(const FGameplayTag& Tag);

	UFUNCTION(BlueprintPure, Category = "FlowNode")
	static FString GetIdentityTagsDescription(const FGameplayTagContainer& Tags);

	UFUNCTION(BlueprintPure, Category = "FlowNode")
	static FString GetNotifyTagsDescription(const FGameplayTagContainer& Tags);

	UFUNCTION(BlueprintPure, Category = "FlowNode")
	static FString GetClassDescription(const TSubclassOf<UObject> Class);

	UFUNCTION(BlueprintPure, Category = "FlowNode")
	static FString GetProgressAsString(float Value);

#if !UE_BUILD_SHIPPING
	/**
	 * Helper for nodes to record pin activations for the debugger.
	 * @param Node The node instance calling this function.
	 * @param PinName The name of the pin being activated.
	 * @param PinDirection The direction of the pin.
	 * @param ActivationType The type of activation (Default, Forced, PassThrough).
	 */
	static void RecordPinActivation(const UFlowNode* Node, FName PinName, EEdGraphPinDirection PinDirection, EFlowPinActivationType ActivationType = EFlowPinActivationType::Default);
#endif
};

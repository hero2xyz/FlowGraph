// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#pragma once

#include "Templates/SubclassOf.h"
#include "UObject/ObjectMacros.h"

#if WITH_EDITOR
#include "EdGraph/EdGraphPin.h"
#endif

#include "FlowPin.generated.h"

/** Enum used to define what container type a pin represents. */
UENUM()
enum class EFlowPinContainerType : uint8
{
	None,
	Array,
	Set,
	Map
};

USTRUCT(BlueprintType)
struct FLOW_API FFlowPin
{
	GENERATED_BODY()

	// A logical name, used during execution of pin
	UPROPERTY(EditDefaultsOnly, Category = FlowPin)
	FName PinName;

	// An optional Display Name, you can use it to override PinName without the need to update graph connections
	UPROPERTY(EditDefaultsOnly, Category = FlowPin)
	FText PinFriendlyName;

	UPROPERTY(EditDefaultsOnly, Category = FlowPin)
	FString PinToolTip;

#if WITH_EDITORONLY_DATA
	/**
	 * Pin type information (category, subcategory, object type, etc.).
	 * Editor-only - use SetPinType() to configure.
	 */
	UPROPERTY()
	FEdGraphPinType PinType;
#endif

	// --- Constructors ---
	FFlowPin()
		: PinName(NAME_None)
#if WITH_EDITORONLY_DATA
		, PinType(PC_Exec, NAME_None, nullptr, EPinContainerType::None, false, FEdGraphTerminalType())
#endif
	{
	}

	explicit FFlowPin(const FName& InPinName)
		: PinName(InPinName)
#if WITH_EDITORONLY_DATA
		, PinType(PC_Exec, NAME_None, nullptr, EPinContainerType::None, false, FEdGraphTerminalType())
#endif
	{
	}

	// Constructor allowing setting all members (except PinType - use SetPinType for that)
	FFlowPin(const FName& InPinName, const FText& InPinFriendlyName, const FString& InPinTooltip)
		: PinName(InPinName), PinFriendlyName(InPinFriendlyName), PinToolTip(InPinTooltip)
#if WITH_EDITORONLY_DATA
		, PinType(PC_Exec, NAME_None, nullptr, EPinContainerType::None, false, FEdGraphTerminalType())
#endif
	{
	}

	// Convenience constructors
	FFlowPin(const FString& InPinName) : FFlowPin(FName(*InPinName)) {}
	FFlowPin(const FText& InPinName) : FFlowPin(FName(*InPinName.ToString())) {}
	FFlowPin(const TCHAR* InPinName) : FFlowPin(FName(InPinName)) {}
	FFlowPin(const uint8& InPinName) : FFlowPin(FName(*FString::FromInt(InPinName))) {}
	FFlowPin(const int32& InPinName) : FFlowPin(FName(*FString::FromInt(InPinName))) {}

	FFlowPin(const FStringView InPinName, const FText& InPinFriendlyName)
		: PinName(InPinName), PinFriendlyName(InPinFriendlyName)
#if WITH_EDITORONLY_DATA
		, PinType(PC_Exec, NAME_None, nullptr, EPinContainerType::None, false, FEdGraphTerminalType())
#endif
	{
	}

	FFlowPin(const FStringView InPinName, const FString& InPinTooltip)
		: PinName(InPinName), PinToolTip(InPinTooltip)
#if WITH_EDITORONLY_DATA
		, PinType(PC_Exec, NAME_None, nullptr, EPinContainerType::None, false, FEdGraphTerminalType())
#endif
	{
	}

	FFlowPin(const FStringView InPinName, const FText& InPinFriendlyName, const FString& InPinTooltip)
		: PinName(InPinName), PinFriendlyName(InPinFriendlyName), PinToolTip(InPinTooltip)
#if WITH_EDITORONLY_DATA
		, PinType(PC_Exec, NAME_None, nullptr, EPinContainerType::None, false, FEdGraphTerminalType())
#endif
	{
	}

	FFlowPin(const FName& InPinName, const FText& InPinFriendlyName)
		: PinName(InPinName), PinFriendlyName(InPinFriendlyName)
#if WITH_EDITORONLY_DATA
		, PinType(PC_Exec, NAME_None, nullptr, EPinContainerType::None, false, FEdGraphTerminalType())
#endif
	{
	}

	// --- Utility Functions ---
	FORCEINLINE bool IsValid() const { return !PinName.IsNone(); }

#if WITH_EDITOR
	bool IsExecPin() const { return PinType.PinCategory == PC_Exec; }
	bool IsDataPin() const { return !IsExecPin(); }

	/** Sets the pin type information (editor-only). */
	void SetPinType(const FEdGraphPinType& InPinType)
	{
		PinType = InPinType;
	}

	/** Gets the current pin type (editor-only). */
	const FEdGraphPinType& GetPinType() const
	{
		return PinType;
	}
#endif

	// --- Operators ---
	FORCEINLINE bool operator==(const FFlowPin& Other) const { return PinName == Other.PinName; }
	FORCEINLINE bool operator!=(const FFlowPin& Other) const { return PinName != Other.PinName; }
	FORCEINLINE bool operator==(const FName& Other) const { return PinName == Other; }
	FORCEINLINE bool operator!=(const FName& Other) const { return PinName != Other; }

	friend uint32 GetTypeHash(const FFlowPin& FlowPin) { return GetTypeHash(FlowPin.PinName); }

	// PinCategory aliases for compatibility with k2 schema.
	static inline FName PC_Exec = TEXT("exec");
	static inline FName PC_Boolean = TEXT("bool");
	static inline FName PC_Byte = TEXT("byte");
	static inline FName PC_Int = TEXT("int");
	static inline FName PC_Int64 = TEXT("int64");
	static inline FName PC_UInt32 = TEXT("uint32");
	static inline FName PC_UInt64 = TEXT("uint64");
	static inline FName PC_Float = TEXT("float");
	static inline FName PC_Double = TEXT("double");
	static inline FName PC_Real = TEXT("real");
	static inline FName PC_Name = TEXT("name");
	static inline FName PC_String = TEXT("string");
	static inline FName PC_Text = TEXT("text");
	static inline FName PC_Enum = TEXT("enum");
	static inline FName PC_Struct = TEXT("struct");
	static inline FName PC_Object = TEXT("object");
	static inline FName PC_SoftObject = TEXT("softobject");
	static inline FName PC_Class = TEXT("class");
	static inline FName PC_SoftClass = TEXT("softclass");
	static inline FName PC_Interface = TEXT("interface");

	static inline FName PC_Wildcard = TEXT("wildcard");

	/**
	 * Metadata key used on UPROPERTY() for C++ Flow Nodes to designate data pins.
	 * Example: UPROPERTY(EditAnywhere, Category = "Flow", meta = (FlowDataPin = "Input || Output"))
	 */
	static inline FName MetadataKey_FlowDataPin = TEXT("FlowDataPin");

	/**
	 * Metadata key used on UPROPERTY() for C++ Flow Nodes to designate property bag data pins.
	 * Example: UPROPERTY(EditAnywhere, Category = "Flow", meta = (FlowPropertyBag = "Input || Output"))
	 */
	static inline FName MetadataKey_FlowPropertyBag = TEXT("FlowPropertyBag");
};

USTRUCT()
struct FLOW_API FFlowPinHandle
{
	GENERATED_BODY()

	// Update SFlowPinHandleBase code if this property name would be ever changed
	UPROPERTY()
	FName PinName;

	FFlowPinHandle()
	    : PinName(NAME_None)
	{
	}
};

USTRUCT(BlueprintType)
struct FLOW_API FFlowInputPinHandle : public FFlowPinHandle
{
	GENERATED_BODY()

	FFlowInputPinHandle()
	{
	}
};

USTRUCT(BlueprintType)
struct FLOW_API FFlowOutputPinHandle : public FFlowPinHandle
{
	GENERATED_BODY()

	FFlowOutputPinHandle()
	{
	}
};

// Processing Flow Nodes creates map of connected pins
USTRUCT()
struct FLOW_API FConnectedPin
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FGuid NodeGuid;

	UPROPERTY()
	FName PinName;

	FConnectedPin()
	    : NodeGuid(FGuid()), PinName(NAME_None)
	{
	}

	FConnectedPin(const FGuid InNodeId, const FName& InPinName)
	    : NodeGuid(InNodeId), PinName(InPinName)
	{
	}

	FORCEINLINE bool IsValid() const
	{
		return NodeGuid.IsValid() && PinName != NAME_None;
	}

	FORCEINLINE bool operator==(const FConnectedPin& Other) const
	{
		return NodeGuid == Other.NodeGuid && PinName == Other.PinName;
	}

	FORCEINLINE bool operator!=(const FConnectedPin& Other) const
	{
		return NodeGuid != Other.NodeGuid || PinName != Other.PinName;
	}

	friend uint32 GetTypeHash(const FConnectedPin& ConnectedPin)
	{
		return HashCombine(GetTypeHash(ConnectedPin.NodeGuid), GetTypeHash(ConnectedPin.PinName));
	}
};

/** Wrapper struct containing a list of connections originating from a single output pin. */
USTRUCT()
struct FLOW_API FPinConnectionList
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FConnectedPin> Connections;
};

UENUM(BlueprintType)
enum class EFlowPinActivationType : uint8
{
	Default,
	Forced,
	PassThrough
};

// Every time pin is activated, we record it and display this data while user hovers mouse over pin
#if !UE_BUILD_SHIPPING
struct FLOW_API FPinRecord
{
	double Time;
	FString HumanReadableTime;
	EFlowPinActivationType ActivationType;

	static FString NoActivations;
	static FString PinActivations;
	static FString ForcedActivation;
	static FString PassThroughActivation;

	FPinRecord();
	FPinRecord(const double InTime, const EFlowPinActivationType InActivationType);

private:
	FORCEINLINE static FString DoubleDigit(const int32 Number);
};
#endif

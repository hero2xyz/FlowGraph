// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/PropertyBag.h"
#include "FlowPropertyUtils.generated.h"

// Forward Declarations
class FProperty;
class UObject;

/** Core utility functions for Flow Graph property operations */
namespace FlowPropertyUtils
{
	/**
	 * Checks if data transfer is possible between two FProperty definitions.
	 * Considers exact type matches, container compatibility, numeric/text conversions,
	 * and object/class/interface inheritance.
	 *
	 * @param SourceProp The source property definition. Cannot be null.
	 * @param TargetProp The target property definition. Cannot be null.
	 * @param bAllowConversion If true, enables implicit conversions (numeric, text, object hierarchy).
	 * @return True if transfer is possible based on type compatibility.
	 * @note Must be in sync with UFlowGraphSchema::CanRuntimePerformImplicitConversion
	 */
	FLOW_API bool ArePropertiesCompatible(const FProperty* SourceProp, const FProperty* TargetProp, bool bAllowConversion = true);

	/**
	 * Performs data transfer between two memory locations defined by FProperty.
	 * Handles direct copy for identical types and supported conversions (numeric, text, object assignment).
	 *
	 * @param SourceProp Definition of the source property. Cannot be null.
	 * @param SourceAddr Memory address of the source data. Cannot be null.
	 * @param TargetProp Definition of the target property. Cannot be null.
	 * @param TargetAddr Memory address of the target data (will be modified). Cannot be null.
	 * @param bAllowConversion If true, enables value conversions (e.g., int to float, numeric to text).
	 * @param OutErrorMessage Error message on failure.
	 * @return True on successful transfer, false on failure.
	 * @note Assumes compatibility has been checked via ArePropertiesCompatible.
	 */
	FLOW_API bool PerformTransfer(const FProperty* SourceProp, const void* SourceAddr, FProperty* TargetProp, void* TargetAddr, const bool bAllowConversion, FString& OutErrorMessage);

	// --- Public Helper Functions ---

	/** Gets the value of a text-based property (Name, String, Text) as an FString. */
	FLOW_API bool GetPropertyValueAsString(const FProperty* Prop, const void* Addr, FString& OutString);

	/** Sets the value of a text-based property (Name, String, Text) from an FString. */
	FLOW_API bool SetPropertyValueFromString(FProperty* Prop, void* Addr, const FString& InValue);

	/** Checks if the FProperty represents a text-based type. */
	FLOW_API bool IsTextBasedProperty(const FProperty* Prop);
} // namespace FlowPropertyUtils

/**
 * Result of a property bag operation.
 */
USTRUCT()
struct FLOW_API FPropertyBagResult
{
	GENERATED_BODY()

	UPROPERTY()
	bool bSuccess = false;
	UPROPERTY()
	EPropertyBagResult ResultCode = EPropertyBagResult::PropertyNotFound;
	UPROPERTY()
	FString Message;
	UPROPERTY()
	FName PropertyName;

	explicit FPropertyBagResult(const bool bInSuccess = false, const EPropertyBagResult InResultCode = EPropertyBagResult::PropertyNotFound, const FName InPropertyName = NAME_None, const FString& InMessage = TEXT(""))
	    : bSuccess(bInSuccess), ResultCode(InResultCode), Message(InMessage), PropertyName(InPropertyName) {}

	static FPropertyBagResult Success(const FName InPropertyName = NAME_None, const FString& InMessage = TEXT("")) { return FPropertyBagResult(true, EPropertyBagResult::Success, InPropertyName, InMessage); }
	static FPropertyBagResult Failure(const EPropertyBagResult InResultCode, const FName InPropertyName = NAME_None, const FString& InMessage = TEXT("")) { return FPropertyBagResult(false, InResultCode, InPropertyName, InMessage); }
	static FPropertyBagResult PropertyNotFound(const FName InPropertyName, const FString& InMessage = TEXT("")) { return Failure(EPropertyBagResult::PropertyNotFound, InPropertyName, InMessage.IsEmpty() ? FString::Printf(TEXT("Property '%s' not found"), *InPropertyName.ToString()) : InMessage); }
	static FPropertyBagResult TypeMismatch(const FName InPropertyName, const FString& InMessage = TEXT("")) { return Failure(EPropertyBagResult::TypeMismatch, InPropertyName, InMessage.IsEmpty() ? FString::Printf(TEXT("Type mismatch for property '%s'"), *InPropertyName.ToString()) : InMessage); }
	static FPropertyBagResult AccessError(const FName InPropertyName, const FString& InMessage) { return Failure(EPropertyBagResult::PropertyNotFound, InPropertyName, InMessage); }

	bool IsSuccess() const { return bSuccess; }
	bool IsFailure() const { return !bSuccess; }
};

/** Utilities for FInstancedPropertyBag operations */
namespace PropertyBagUtils
{
	/**
	 * Copies properties with matching names and compatible types between two property bags.
	 *
	 * @param SourceBag The bag to copy from.
	 * @param TargetBag The bag to copy to.
	 * @param bAllowConversion If true, enables value conversions (e.g., int to float, numeric to text).
	 * @param bCreateMissingProperties If true, creates missing properties in TargetBag (if types are compatible).
	 * @return Result indicating success or failure with details about transfers.
	 */
	FLOW_API FPropertyBagResult CopyMatchingProperties(const FInstancedPropertyBag& SourceBag, FInstancedPropertyBag& TargetBag, const bool bAllowConversion /*= true*/, bool bCreateMissingProperties = false);

	/**
	 * Finds property names that exist in both bags with optionally compatible types.
	 *
	 * @param Bag1 The first property bag.
	 * @param Bag2 The second property bag.
	 * @param bAllowConversion If true, only returns names where types are compatible via ArePropertiesCompatible.
	 * @return Array of matching property names.
	 */
	FLOW_API TArray<FName> FindMatchingPropertyNames(const FInstancedPropertyBag& Bag1, const FInstancedPropertyBag& Bag2, bool bAllowConversion = true);

	/**
	 * Transfers properties between bags using explicit name mappings.
	 *
	 * @param SourceBag The bag to read from.
	 * @param TargetBag The bag to write to.
	 * @param PropertyMappings Array of {SourceName, TargetName} pairs.
	 * @param bCreateMissingProperties If true, creates missing target properties based on the source type.
	 * @return Result aggregating individual transfer results.
	 */
	FLOW_API FPropertyBagResult BatchTransferProperties(const FInstancedPropertyBag& SourceBag, FInstancedPropertyBag& TargetBag, const TArray<TPair<FName, FName>>& PropertyMappings, bool bCreateMissingProperties = false);

	/**
	 * Transfers a single property value between bags.
	 *
	 * @param SourceBag The bag containing the source property.
	 * @param SourceName The source property name.
	 * @param TargetBag The bag to receive the value.
	 * @param TargetName The target property name.
	 * @param bAllowConversion If true, enables implicit conversions.
	 * @return Success if transferred, PropertyNotFound/TypeMismatch/AccessError on failure.
	 * @note Both properties must exist with compatible types and valid memory access.
	 */
	FLOW_API FPropertyBagResult TransferProperty(const FInstancedPropertyBag& SourceBag, const FName SourceName, FInstancedPropertyBag& TargetBag, const FName TargetName, const bool bAllowConversion = true);

	/**
	 * Ensures a property exists in TargetBag with the specified type.
	 * Adds if missing or updates the type if it differs, preserving values when possible.
	 *
	 * @param TargetBag The property bag to modify.
	 * @param PropertyName The property name.
	 * @param NewTypeDesc Desired type descriptor (Name and ID are ignored).
	 * @param bOutDefinitionChanged Optional output, set to true if the bag structure was modified.
	 * @return Result indicating success or failure.
	 */
	FLOW_API FPropertyBagResult EnsurePropertyExistsWithType(FInstancedPropertyBag& TargetBag, FName PropertyName, const FPropertyBagPropertyDesc& NewTypeDesc, bool* bOutDefinitionChanged = nullptr);

	/**
	 * Extracts cached FProperty pointers from a property bag.
	 *
	 * @param Bag The property bag to query.
	 * @return Map of property names to FProperty pointers. Empty if the bag is invalid.
	 * @note Only includes properties with valid cache entries.
	 */
	FLOW_API TMap<FName, FProperty*> GetCachedPropertiesFromBag(const FInstancedPropertyBag& Bag);

	/**
	 * Synchronizes TargetBag structure to match RequiredDescs.
	 * Adds/updates properties, removes obsolete ones, preserves values when possible.
	 *
	 * @param TargetBag The property bag to modify.
	 * @param RequiredDescs Desired final structure.
	 * @return True if the structure was modified.
	 */
	FLOW_API bool SyncBagStructureWithDescriptors(FInstancedPropertyBag& TargetBag, const TConstArrayView<FPropertyBagPropertyDesc> RequiredDescs);

	/**
	 * Finds the memory container for a given FProperty among candidate bags.
	 *
	 * @param Property The FProperty to locate (must be owned by a UPropertyBag).
	 * @param CandidateBags Bags that might contain the property.
	 * @return Mutable memory pointer of the containing bag, or nullptr if not found.
	 */
	FLOW_API void* FindPropertyBagMemoryForProperty(const FProperty* Property, const TConstArrayView<const FInstancedPropertyBag*> CandidateBags);
} // namespace PropertyBagUtils

// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#include "Types/FlowPropertyUtils.h"
#include "Nodes/FlowPin.h"
#include "FlowLogChannels.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "UObject/TextProperty.h"

namespace FlowPropertyUtils
{
	//----------------------------------------------------------------------//
	// Internal Helpers
	//----------------------------------------------------------------------//
	namespace Private
	{
		// Formats transfer error messages with property context
		inline FString FormatTransferError(const FString& CoreMessage, const FProperty* SourceProp, const FProperty* TargetProp, const FString& OptionalReason = TEXT(""))
		{
			const FString SourceName = SourceProp ? SourceProp->GetName() : TEXT("UnknownSource");
			const FString TargetName = TargetProp ? TargetProp->GetName() : TEXT("UnknownTarget");

			FString Error = FString::Printf(TEXT("PerformTransfer: %s from property '%s' to '%s'"), *CoreMessage, *SourceName, *TargetName);
			if (!OptionalReason.IsEmpty())
			{
				Error += FString::Printf(TEXT(". Reason: %s"), *OptionalReason);
			}
			else
			{
				Error += TEXT(".");
			}
			return Error;
		}
		
		/**
		 * Extracts a container type and inner property from a property definition.
		 * For Maps, the ValueProp is used (keys are ignored).
		 * 
		 * @return True if OutInnerProp is valid
		 */
		bool DetermineContainerAndInnerProperty(const FProperty* Property, EFlowPinContainerType& OutContainerType, const FProperty*& OutInnerProp)
		{
			OutContainerType = EFlowPinContainerType::None;
			OutInnerProp = Property;

			if (!Property)
			{
				return false;
			}

			if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
			{
				OutContainerType = EFlowPinContainerType::Array;
				OutInnerProp = ArrayProp->Inner;
			}
			else if (const FSetProperty* SetProp = CastField<FSetProperty>(Property))
			{
				OutContainerType = EFlowPinContainerType::Set;
				OutInnerProp = SetProp->ElementProp;
			}
			else if (const FMapProperty* MapProp = CastField<FMapProperty>(Property))
			{
				OutContainerType = EFlowPinContainerType::Map;
				OutInnerProp = MapProp->ValueProp;
			}

			return (OutInnerProp != nullptr);
		}

		/**
		 * Maps FProperty type to Flow pin category and optional subcategory object.
		 * 
		 * @param OutSubCategoryObject UEnum, UScriptStruct, or UClass for specialized types
		 * @return True if a valid category was assigned (not Wildcard)
		 */
		bool DetermineCategoryAndSubObjectFromInner(const FProperty* InnerProp, FName& OutCategory, TWeakObjectPtr<UObject>& OutSubCategoryObject)
		{
			OutCategory = FFlowPin::PC_Wildcard;
			OutSubCategoryObject = nullptr;

			if (!InnerProp)
			{
				return false;
			}

			if (CastField<FBoolProperty>(InnerProp))
			{
				OutCategory = FFlowPin::PC_Boolean;
			}
			else if (const FByteProperty* ByteProp = CastField<FByteProperty>(InnerProp))
			{
				OutCategory = FFlowPin::PC_Byte;
				OutSubCategoryObject = ByteProp->Enum;
			}
			else if (CastField<FIntProperty>(InnerProp))
			{
				OutCategory = FFlowPin::PC_Int;
			}
			else if (CastField<FInt64Property>(InnerProp))
			{
				OutCategory = FFlowPin::PC_Int64;
			}
			else if (CastField<FUInt32Property>(InnerProp))
			{
				OutCategory = FFlowPin::PC_UInt32;
			}
			else if (CastField<FUInt64Property>(InnerProp))
			{
				OutCategory = FFlowPin::PC_UInt64;
			}
			else if (CastField<FFloatProperty>(InnerProp))
			{
				OutCategory = FFlowPin::PC_Float;
			}
			else if (CastField<FDoubleProperty>(InnerProp))
			{
				OutCategory = FFlowPin::PC_Double;
			}
			else if (CastField<FNameProperty>(InnerProp))
			{
				OutCategory = FFlowPin::PC_Name;
			}
			else if (CastField<FStrProperty>(InnerProp))
			{
				OutCategory = FFlowPin::PC_String;
			}
			else if (CastField<FTextProperty>(InnerProp))
			{
				OutCategory = FFlowPin::PC_Text;
			}
			else if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(InnerProp))
			{
				OutCategory = FFlowPin::PC_Enum;
				OutSubCategoryObject = EnumProp->GetEnum();
			}
			else if (const FStructProperty* StructProp = CastField<FStructProperty>(InnerProp))
			{
				OutCategory = FFlowPin::PC_Struct;
				OutSubCategoryObject = StructProp->Struct;
			}
			else if (const FObjectProperty* ObjectProp = CastField<FObjectProperty>(InnerProp))
			{
				OutCategory = InnerProp->IsA<FClassProperty>() ? FFlowPin::PC_Class : FFlowPin::PC_Object;
				OutSubCategoryObject = ObjectProp->PropertyClass;
			}
			else if (const FSoftObjectProperty* SoftObjectProp = CastField<FSoftObjectProperty>(InnerProp))
			{
				OutCategory = InnerProp->IsA<FSoftClassProperty>() ? FFlowPin::PC_SoftClass : FFlowPin::PC_SoftObject;
				OutSubCategoryObject = SoftObjectProp->PropertyClass;
			}
			else if (const FInterfaceProperty* InterfaceProp = CastField<FInterfaceProperty>(InnerProp))
			{
				OutCategory = FFlowPin::PC_Interface;
				OutSubCategoryObject = InterfaceProp->InterfaceClass;
			}
			else
			{
				OutCategory = FFlowPin::PC_Wildcard;
				UE_LOG(LogFlow, Verbose, TEXT("FPinTypeInfo: Property '%s' of type '%s' resulted in Wildcard."), *InnerProp->GetName(), *InnerProp->GetClass()->GetName());
			}

			return (OutCategory != FFlowPin::PC_Wildcard && OutCategory != NAME_None);
		}

		/**
		 * Lightweight type descriptor for pin compatibility and transfer operations.
		 * Enum/Bool are treated as numeric for conversion purposes.
		 */
		struct FPinTypeInfo
		{
			FName PinCategory = NAME_None;
			TWeakObjectPtr<UObject> PinSubCategoryObject = nullptr;
			EFlowPinContainerType ContainerType = EFlowPinContainerType::None;
			bool bIsNumeric = false;
			bool bIsText = false;
			bool bIsObjectBased = false;

			explicit FPinTypeInfo(const FProperty* Property)
			{
				if (!Property)
				{
					PinCategory = FFlowPin::PC_Wildcard;
					return;
				}

				const FProperty* InnerProp = nullptr;
				if (!DetermineContainerAndInnerProperty(Property, ContainerType, InnerProp))
				{
					PinCategory = FFlowPin::PC_Wildcard;
					return;
				}

				if (!DetermineCategoryAndSubObjectFromInner(InnerProp, PinCategory, PinSubCategoryObject))
				{
					PinCategory = FFlowPin::PC_Wildcard;
				}
				
				bIsNumeric = Property->IsA<FNumericProperty>() || Property->IsA<FEnumProperty>() || Property->IsA<FBoolProperty>();
				bIsText = (PinCategory == FFlowPin::PC_Name || PinCategory == FFlowPin::PC_String || PinCategory == FFlowPin::PC_Text);
				bIsObjectBased = (PinCategory == FFlowPin::PC_Object || PinCategory == FFlowPin::PC_Class || PinCategory == FFlowPin::PC_Interface || PinCategory == FFlowPin::PC_SoftObject || PinCategory == FFlowPin::PC_SoftClass);
			}

			bool IsValid() const { return PinCategory != FFlowPin::PC_Wildcard && PinCategory != NAME_None; }
		};
		
		/**
		 * Normalizes numeric-like properties to unified int64/double representation.
		 * 
		 * @param bIsFloatingPoint True if original value was float/double
		 * @return True if numeric value was extracted
		 */
		bool GetNumericValue(const FProperty* Prop, const void* PropertyAddress, int64& OutInt, double& OutDouble, bool& bIsFloatingPoint)
		{
			if (!Prop || !PropertyAddress)
			{
				return false;
			}

			bIsFloatingPoint = false;
			OutInt = 0;
			OutDouble = 0.0;

			auto HandleIntegerLike = [&](const int64 Value) {
				OutInt = Value;
				OutDouble = static_cast<double>(Value);
				bIsFloatingPoint = false;
				return true;
			};

			if (const FNumericProperty* NumProp = CastField<const FNumericProperty>(Prop))
			{
				if (NumProp->IsFloatingPoint())
				{
					bIsFloatingPoint = true;
					OutDouble = NumProp->GetFloatingPointPropertyValue(PropertyAddress);
					OutInt = static_cast<int64>(OutDouble);
				}
				else
				{
					OutInt = NumProp->GetSignedIntPropertyValue(PropertyAddress);
					OutDouble = static_cast<double>(OutInt);
				}
				return true;
			}
			if (const FBoolProperty* BoolProp = CastField<const FBoolProperty>(Prop))
			{
				return HandleIntegerLike(BoolProp->GetPropertyValue(PropertyAddress) ? 1 : 0);
			}
			if (const FEnumProperty* EnumProp = CastField<const FEnumProperty>(Prop))
			{
				if (const FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty())
				{
					return HandleIntegerLike(UnderlyingProp->GetSignedIntPropertyValue(PropertyAddress));
				}
			}
			else if (const FByteProperty* ByteProp = CastField<const FByteProperty>(Prop))
			{
				return HandleIntegerLike(ByteProp->GetPropertyValue(PropertyAddress));
			}

			return false;
		}
		
		/**
		 * Applies unified numeric value to target property.
		 * 
		 * @param bIsSourceFloatingPoint If true, uses InDouble; otherwise InInt
		 * @return True if value was assigned
		 */
		bool SetNumericValue(FProperty* Prop, void* Addr, const int64 InInt, const double InDouble, const bool bIsSourceFloatingPoint)
		{
			if (const FNumericProperty* NumProp = CastField<FNumericProperty>(Prop))
			{
				if (NumProp->IsFloatingPoint())
				{
					NumProp->SetFloatingPointPropertyValue(Addr, bIsSourceFloatingPoint ? InDouble : static_cast<double>(InInt));
				}
				else
				{
					NumProp->SetIntPropertyValue(Addr, bIsSourceFloatingPoint ? static_cast<int64>(InDouble) : InInt);
				}
				return true;
			}
			return false;
		}

		// Converts text-based properties (Name/String/Text) to FString
		bool GetTextValueAsString(const FProperty* Prop, const void* Addr, FString& OutString)
		{
			if (!Prop || !Addr)
			{
				return false;
			}
			if (const FNameProperty* NameProp = CastField<FNameProperty>(Prop))
			{
				OutString = NameProp->GetPropertyValue(Addr).ToString();
				return true;
			}
			if (const FStrProperty* StrProp = CastField<FStrProperty>(Prop))
			{
				OutString = StrProp->GetPropertyValue(Addr);
				return true;
			}
			if (const FTextProperty* TextProp = CastField<FTextProperty>(Prop))
			{
				OutString = TextProp->GetPropertyValue(Addr).ToString();
				return true;
			}
			return false;
		}
		
		// Assigns FString to a text-based property (Name/String/Text)
		bool SetTextValueFromString(FProperty* Prop, void* Addr, const FString& InString)
		{
			if (!Prop || !Addr)
			{
				return false;
			}
			if (const FNameProperty* NameProp = CastField<FNameProperty>(Prop))
			{
				NameProp->SetPropertyValue(Addr, FName(*InString));
				return true;
			}
			if (const FStrProperty* StrProp = CastField<FStrProperty>(Prop))
			{
				StrProp->SetPropertyValue(Addr, InString);
				return true;
			}
			if (const FTextProperty* TextProp = CastField<FTextProperty>(Prop))
			{
				TextProp->SetPropertyValue(Addr, FText::FromString(InString));
				return true;
			}
			return false;
		}
		
		// Checks if a property is text-based (Name/String/Text)
		bool IsTextBasedProperty(const FProperty* Prop)
		{
			return Prop && (Prop->IsA<FNameProperty>() || Prop->IsA<FStrProperty>() || Prop->IsA<FTextProperty>());
		}

		/**
		 * Assigns UObject references between compatible object-based properties.
		 * Handles FObjectPropertyBase and FInterfaceProperty with inheritance/interface validation.
		 * 
		 * @note Does not copy object data — only assigns references
		 * @return True if assignment succeeded with type validation
		 */
		bool PerformObjectBasedTransfer(const FProperty* SourceProp, const void* SourceAddr, FProperty* TargetProp, void* TargetAddr, FString& OutErrorMessage)
		{
			OutErrorMessage.Empty();
			UObject* ObjectToTransfer;

			// Extract source object
			if (const FInterfaceProperty* SourceInterfaceProp = CastField<FInterfaceProperty>(SourceProp))
			{
				FScriptInterface SourceInterfaceValue;
				SourceInterfaceProp->CopySingleValue(&SourceInterfaceValue, SourceAddr);
				ObjectToTransfer = SourceInterfaceValue.GetObject();
			}
			else if (const FObjectPropertyBase* SourceObjectPropBase = CastField<FObjectPropertyBase>(SourceProp))
			{
				ObjectToTransfer = SourceObjectPropBase->GetObjectPropertyValue(SourceAddr);
			}
			else
			{
				OutErrorMessage = FString::Printf(TEXT("Source property '%s' (Type: %s) is not a supported object-based type (FObjectPropertyBase or FInterfaceProperty)."),
				    SourceProp ? *SourceProp->GetName() : TEXT("UnknownSource"),
				    SourceProp ? *SourceProp->GetClass()->GetName() : TEXT("Unknown"));
				return false;
			}

			// Assign to target with type validation
			if (const FInterfaceProperty* TargetInterfaceProp = CastField<FInterfaceProperty>(TargetProp))
			{
				if (ObjectToTransfer == nullptr)
				{
					TargetInterfaceProp->SetPropertyValue(TargetAddr, FScriptInterface());
					return true;
				}
				if (ObjectToTransfer->GetClass()->ImplementsInterface(TargetInterfaceProp->InterfaceClass))
				{
					TargetInterfaceProp->SetPropertyValue(TargetAddr, FScriptInterface(ObjectToTransfer, ObjectToTransfer->GetInterfaceAddress(TargetInterfaceProp->InterfaceClass)));
					return true;
				}
				OutErrorMessage = FString::Printf(TEXT("Source object '%s' (Class: %s) does not implement target interface '%s'."),
				    *GetNameSafe(ObjectToTransfer),
				    ObjectToTransfer->GetClass() ? *GetNameSafe(ObjectToTransfer->GetClass()) : TEXT("UnknownClass"),
				    TargetInterfaceProp->InterfaceClass ? *TargetInterfaceProp->InterfaceClass->GetName() : TEXT("UnknownInterface"));
				return false;
			}
			if (const FObjectPropertyBase* TargetObjectPropBase = CastField<FObjectPropertyBase>(TargetProp))
			{
				if (ObjectToTransfer == nullptr)
				{
					TargetObjectPropBase->SetObjectPropertyValue(TargetAddr, nullptr);
					return true;
				}

				const UClass* SourceObjectClass = ObjectToTransfer->GetClass();
				const UClass* TargetExpectedClass = TargetObjectPropBase->PropertyClass;
				
				if (TargetExpectedClass == nullptr)
				{
					TargetObjectPropBase->SetObjectPropertyValue(TargetAddr, ObjectToTransfer);
					return true;
				}

				if (!SourceObjectClass)
				{
					OutErrorMessage = FString::Printf(TEXT("Object '%s' has null class, cannot assign to property '%s' (Type: %s)."),
					    *GetNameSafe(ObjectToTransfer), *TargetProp->GetName(), *GetNameSafe(TargetExpectedClass));
					return false;
				}

				if (!SourceObjectClass->IsChildOf(TargetExpectedClass))
				{
					OutErrorMessage = FString::Printf(TEXT("Object '%s' (Class: %s) is not assignable to target property '%s' (Expected: %s)."),
					    *GetNameSafe(ObjectToTransfer), *GetNameSafe(SourceObjectClass),
					    *TargetProp->GetName(), *GetNameSafe(TargetExpectedClass));
					return false;
				}

				TargetObjectPropBase->SetObjectPropertyValue(TargetAddr, ObjectToTransfer);
				return true;
			}

			OutErrorMessage = FString::Printf(TEXT("Target property '%s' (Type: %s) is not a supported object-based type."),
			    TargetProp ? *TargetProp->GetName() : TEXT("UnknownTarget"),
			    TargetProp ? *TargetProp->GetClass()->GetName() : TEXT("Unknown"));
			return false;
		}
	} // namespace Private

	//----------------------------------------------------------------------//
	// Public API Implementation
	//----------------------------------------------------------------------//
	
	bool ArePropertiesCompatible(const FProperty* SourceProp, const FProperty* TargetProp, const bool bAllowConversion /* = true */)
	{
	    if (!SourceProp || !TargetProp)
	    {
	       return false;
	    }

	    const Private::FPinTypeInfo SourceType(SourceProp);
	    const Private::FPinTypeInfo TargetType(TargetProp);

	    if (!SourceType.IsValid() || !TargetType.IsValid())
	    {
	       return false;
	    }
	    if (SourceType.ContainerType != TargetType.ContainerType)
	    {
	       return false;
	    }

	    // Same category compatibility
	    if (SourceType.PinCategory == TargetType.PinCategory)
	    {
	       if (SourceType.bIsObjectBased)
	       {
	          const UClass* SourceClass = Cast<UClass>(SourceType.PinSubCategoryObject.Get());
	          const UClass* TargetClass = Cast<UClass>(TargetType.PinSubCategoryObject.Get());
	          
	          if (!SourceClass)
	          {
	             return true; // Null source allowed
	          }
	          if (!TargetClass)
	          {
	             return false;
	          }
	          if (SourceType.PinCategory == FFlowPin::PC_Interface)
	          {
	             return SourceClass == TargetClass; // Interfaces require an exact match
	          }

	          return SourceClass->IsChildOf(TargetClass);
	       }
	       if (SourceType.PinCategory == FFlowPin::PC_Struct || SourceType.PinCategory == FFlowPin::PC_Enum || SourceType.PinCategory == FFlowPin::PC_Byte)
	       {
	          return SourceType.PinSubCategoryObject == TargetType.PinSubCategoryObject;
	       }

	       return true;
	    }

	    // Cross-category conversions (non-container only)
	    if (bAllowConversion && SourceType.ContainerType == EFlowPinContainerType::None && TargetType.ContainerType == EFlowPinContainerType::None)
	    {
	       if (SourceType.bIsNumeric && TargetType.bIsNumeric)
	       {
	          return true;
	       }
	       if (SourceType.bIsText && TargetType.bIsText)
	       {
	          return true;
	       }
	       if (SourceType.bIsNumeric && TargetType.bIsText)
	       {
	          return true;
	       }
	       if (SourceType.PinCategory == FFlowPin::PC_Object && TargetType.PinCategory == FFlowPin::PC_Interface)
	       {
	          return true;
	       }
	       if (SourceType.PinCategory == FFlowPin::PC_Interface && TargetType.PinCategory == FFlowPin::PC_Object)
	       {
	          return true;
	       }
	    }

	    return false;
	}
	
	bool PerformTransfer(const FProperty* SourceProp, const void* SourceAddr, FProperty* TargetProp, void* TargetAddr, const bool bAllowConversion, FString& OutErrorMessage)
	{
		OutErrorMessage.Empty();
		if (!SourceProp || !TargetProp || !SourceAddr || !TargetAddr)
		{
			OutErrorMessage = TEXT("PerformTransfer: Invalid input parameters (null property or address).");
			return false;
		}

		// Direct copy for identical types
		if (SourceProp->SameType(TargetProp))
		{
			TargetProp->CopyCompleteValue(TargetAddr, SourceAddr);
			return true;
		}

		const Private::FPinTypeInfo SourceTypeInfo(SourceProp);
		const Private::FPinTypeInfo TargetTypeInfo(TargetProp);

		// Container conversions not supported
		if (SourceTypeInfo.ContainerType != EFlowPinContainerType::None || TargetTypeInfo.ContainerType != EFlowPinContainerType::None)
		{
			const FString Msg = FString::Printf(TEXT("Direct transfer for different container types ('%s' to '%s') is not supported beyond SameType()"),
				*SourceProp->GetClass()->GetName(), *TargetProp->GetClass()->GetName());
			OutErrorMessage = Private::FormatTransferError(Msg, SourceProp, TargetProp);
			return false;
		}

		// Object-based assignment (inheritance/interface checks)
		if (SourceTypeInfo.bIsObjectBased && TargetTypeInfo.bIsObjectBased)
		{
			FString ObjectTransferError;
			if (Private::PerformObjectBasedTransfer(SourceProp, SourceAddr, TargetProp, TargetAddr, ObjectTransferError))
			{
				return true;
			}

			if (!ObjectTransferError.IsEmpty())
			{
				OutErrorMessage = Private::FormatTransferError(TEXT("Object-based assignment failed"), SourceProp, TargetProp, ObjectTransferError);
			}
			else
			{
				OutErrorMessage = Private::FormatTransferError(TEXT("Object-based assignment failed with unknown error"), SourceProp, TargetProp);
			}

			return false;
		}

		// Value conversions require explicit opt-in
		if (!bAllowConversion)
		{
			const FString Msg = FString::Printf(TEXT("Types '%s' and '%s' are not directly assignable and conversions are disabled"),
				*SourceProp->GetCPPType(), *TargetProp->GetCPPType());
			OutErrorMessage = Private::FormatTransferError(Msg, SourceProp, TargetProp);
			return false;
		}

		// Numeric <-> Numeric
		if (SourceTypeInfo.bIsNumeric && TargetTypeInfo.bIsNumeric)
		{
			int64 SrcInt = 0;
			double SrcDouble = 0.0;
			bool bIsSrcFloat = false;
			if (Private::GetNumericValue(SourceProp, SourceAddr, SrcInt, SrcDouble, bIsSrcFloat))
			{
				if (Private::SetNumericValue(TargetProp, TargetAddr, SrcInt, SrcDouble, bIsSrcFloat))
				{
					return true;
				}
				OutErrorMessage = Private::FormatTransferError(TEXT("Failed to set numeric value on target after conversion"), SourceProp, TargetProp);
			}
			else
			{
				OutErrorMessage = Private::FormatTransferError(TEXT("Failed to extract numeric value from source"), SourceProp, TargetProp);
			}
			return false;
		}

		// Numeric -> Text
		if (SourceTypeInfo.bIsNumeric && TargetTypeInfo.bIsText)
		{
			int64 SrcInt = 0;
			double SrcDouble = 0.0;
			bool bIsSrcFloat = false;
			if (Private::GetNumericValue(SourceProp, SourceAddr, SrcInt, SrcDouble, bIsSrcFloat))
			{
				const FString NumericAsString = bIsSrcFloat ? FString::SanitizeFloat(SrcDouble) : FString::FromInt(SrcInt);
				if (Private::SetTextValueFromString(TargetProp, TargetAddr, NumericAsString))
				{
					return true;
				}
				OutErrorMessage = Private::FormatTransferError(TEXT("Failed to set numeric-as-text value on target"), SourceProp, TargetProp);
			}
			else
			{
				OutErrorMessage = Private::FormatTransferError(TEXT("Failed to extract numeric value for text conversion"), SourceProp, TargetProp);
			}
			return false;
		}

		// Text <-> Text
		if (SourceTypeInfo.bIsText && TargetTypeInfo.bIsText)
		{
			if (FString TempString; Private::GetTextValueAsString(SourceProp, SourceAddr, TempString))
			{
				if (Private::SetTextValueFromString(TargetProp, TargetAddr, TempString))
				{
					return true;
				}
				OutErrorMessage = Private::FormatTransferError(TEXT("Failed to set text value on target"), SourceProp, TargetProp);
			}
			else
			{
				OutErrorMessage = Private::FormatTransferError(TEXT("Failed to extract text value from source"), SourceProp, TargetProp);
			}
			return false;
		}

		// No conversion path found
		if (OutErrorMessage.IsEmpty())
		{
			const FString Msg = FString::Printf(TEXT("No supported conversion path from '%s' to '%s'"),
				*SourceTypeInfo.PinCategory.ToString(), *TargetTypeInfo.PinCategory.ToString());
			OutErrorMessage = Private::FormatTransferError(Msg, SourceProp, TargetProp);
		}
		return false;
	}

	// --- Public Helpers ---

	bool GetPropertyValueAsString(const FProperty* Prop, const void* Addr, FString& OutString)
	{
		return Private::GetTextValueAsString(Prop, Addr, OutString);
	}

	bool SetPropertyValueFromString(FProperty* Prop, void* Addr, const FString& InValue)
	{
		return Private::SetTextValueFromString(Prop, Addr, InValue);
	}

	bool IsTextBasedProperty(const FProperty* Prop)
	{
		return Private::IsTextBasedProperty(Prop);
	}
} // namespace FlowPropertyUtils

namespace PropertyBagUtils
{
	namespace Private
	{
		// Safely gets descriptor view from a property bag
		TArrayView<const FPropertyBagPropertyDesc> GetCurrentDescriptorsView(const FInstancedPropertyBag& Bag)
		{
			if (const UPropertyBag* CurrentBagStruct = Bag.GetPropertyBagStruct())
			{
				return CurrentBagStruct->GetPropertyDescs();
			}
			return TArrayView<const FPropertyBagPropertyDesc>{};
		}

		// Gets FProperty* from the bag descriptor
		const FProperty* GetPropertyFromDesc(const FInstancedPropertyBag& Bag, const FPropertyBagPropertyDesc* Desc)
		{
			if (!Desc)
			{
				return nullptr;
			}
			if (Desc->CachedProperty)
			{
				return Desc->CachedProperty;
			}
			if (const UPropertyBag* BagStruct = Bag.GetPropertyBagStruct())
			{
				return BagStruct->FindPropertyByName(Desc->Name);
			}
			return nullptr;
		}
	} // namespace Private

	//----------------------------------------------------------------------//
	// Property Transfer Functions Implementation
	//----------------------------------------------------------------------//

	FPropertyBagResult TransferProperty(const FInstancedPropertyBag& SourceBag, const FName SourceName, FInstancedPropertyBag& TargetBag, const FName TargetName, const bool bAllowConversion)
	{
	    const FPropertyBagPropertyDesc* SourceDesc = SourceBag.FindPropertyDescByName(SourceName);
	    const FPropertyBagPropertyDesc* TargetDesc = TargetBag.FindPropertyDescByName(TargetName);
	    if (!SourceDesc)
	    {
	       return FPropertyBagResult::PropertyNotFound(SourceName, TEXT("Source property desc not found."));
	    }
	    if (!TargetDesc)
	    {
	       return FPropertyBagResult::PropertyNotFound(TargetName, TEXT("Target property desc not found."));
	    }

	    const FProperty* SourceProp = Private::GetPropertyFromDesc(SourceBag, SourceDesc);
	    FProperty* TargetProp = const_cast<FProperty*>(Private::GetPropertyFromDesc(TargetBag, TargetDesc));
	    if (!SourceProp)
	    {
	       return FPropertyBagResult::Failure(EPropertyBagResult::PropertyNotFound, SourceName, TEXT("Source FProperty definition missing."));
	    }
	    if (!TargetProp)
	    {
	       return FPropertyBagResult::Failure(EPropertyBagResult::PropertyNotFound, TargetName, TEXT("Target FProperty definition missing."));
	    }

	    if (!FlowPropertyUtils::ArePropertiesCompatible(SourceProp, TargetProp, bAllowConversion))
	    {
	       return FPropertyBagResult::TypeMismatch(TargetName, FString::Printf(TEXT("Incompatible types: %s vs %s"), *SourceName.ToString(), *TargetName.ToString()));
	    }

	    const FConstStructView SourceStructView = SourceBag.GetValue();
	    const FStructView TargetStructView = TargetBag.GetMutableValue();
	    if (!SourceStructView.IsValid() || !TargetStructView.IsValid())
	    {
	       return FPropertyBagResult::AccessError(SourceName, TEXT("Invalid Source or Target StructView."));
	    }

	    if (SourceProp->GetOwnerStruct() != SourceStructView.GetScriptStruct() || TargetProp->GetOwnerStruct() != TargetStructView.GetScriptStruct())
	    {
	       return FPropertyBagResult::AccessError(SourceName, TEXT("Property owner mismatch with StructView script struct."));
	    }

	    const uint8* SourceBaseAddr = SourceStructView.GetMemory();
	    uint8* TargetBaseAddr = TargetStructView.GetMemory();

	    if (!SourceBaseAddr || !TargetBaseAddr)
	    {
	       return FPropertyBagResult::AccessError(SourceName, TEXT("Null memory pointer from StructView."));
	    }

	    const void* SourceAddr = SourceBaseAddr + SourceProp->GetOffset_ForInternal();
	    void* TargetAddr = TargetBaseAddr + TargetProp->GetOffset_ForInternal();

	    if (FString ErrMsg; !FlowPropertyUtils::PerformTransfer(SourceProp, SourceAddr, TargetProp, TargetAddr, bAllowConversion, ErrMsg))
	    {
	       return FPropertyBagResult::Failure(EPropertyBagResult::TypeMismatch, TargetName,
	           FString::Printf(TEXT("Transfer failed from '%s' to '%s'. %s"), *SourceName.ToString(), *TargetName.ToString(), *ErrMsg));
	    }

	    return FPropertyBagResult::Success(TargetName);
	}

	FPropertyBagResult BatchTransferProperties(const FInstancedPropertyBag& SourceBag, FInstancedPropertyBag& TargetBag, const TArray<TPair<FName, FName>>& PropertyMappings, const bool bCreateMissingProperties)
	{
		if (!SourceBag.IsValid())
		{
			return FPropertyBagResult::Failure(EPropertyBagResult::PropertyNotFound, NAME_None, TEXT("SourceBag is invalid."));
		}
		if (!TargetBag.IsValid())
		{
			return FPropertyBagResult::Failure(EPropertyBagResult::PropertyNotFound, NAME_None, TEXT("TargetBag is invalid."));
		}
		if (PropertyMappings.IsEmpty())
		{
			return FPropertyBagResult::Success(NAME_None, TEXT("No properties specified in mappings."));
		}
		
		int32 SuccessCount = 0;
		int32 FailCount = 0;
		TArray<FString> ErrorMessages;

		// Create missing properties if requested
		if (bCreateMissingProperties)
		{
			for (const auto& [SourceName, TargetName] : PropertyMappings)
			{
				if (!TargetBag.FindPropertyDescByName(TargetName))
				{
					if (const FPropertyBagPropertyDesc* SourceDesc = SourceBag.FindPropertyDescByName(SourceName))
					{
						if (SourceDesc->ValueType != EPropertyBagPropertyType::None)
						{
							bool bDefChanged = false;
							FPropertyBagResult CreateResult = EnsurePropertyExistsWithType(TargetBag, TargetName, *SourceDesc, &bDefChanged);
							if (CreateResult.IsFailure())
							{
								ErrorMessages.Add(FString::Printf(TEXT("CreateFail('%s'): %s"), *TargetName.ToString(), *CreateResult.Message));
								FailCount++;
							}
						}
					}
				}
			}
		}

		// Transfer all properties
		for (const auto& [SourceName, TargetName] : PropertyMappings)
		{
			if (TargetBag.FindPropertyDescByName(TargetName))
			{
				FPropertyBagResult TransferResult = TransferProperty(SourceBag, SourceName, TargetBag, TargetName);
				if (TransferResult.IsSuccess())
				{
					SuccessCount++;
				}
				else
				{
					FailCount++;
					ErrorMessages.Add(FString::Printf(TEXT("XferFail('%s'->'%s'): %s (%s)"),
					    *SourceName.ToString(), *TargetName.ToString(),
					    *UEnum::GetValueAsString(TransferResult.ResultCode),
					    *TransferResult.Message));
				}
			}
		}

		if (FailCount == 0 && SuccessCount == PropertyMappings.Num())
		{
			return FPropertyBagResult::Success(NAME_None, FString::Printf(TEXT("Successfully transferred all %d properties."), SuccessCount));
		}

		FString FinalMessage = FString::Printf(TEXT("Batch Transfer: %d Success, %d Failures (of %d mapped)."), SuccessCount, FailCount, PropertyMappings.Num());
		if (!ErrorMessages.IsEmpty())
		{
			FinalMessage += TEXT(" ") + FString::Join(ErrorMessages, TEXT(" "));
		}

		EPropertyBagResult FinalCode = (SuccessCount > 0 || FailCount == 0)
		    ? EPropertyBagResult::TypeMismatch
		    : EPropertyBagResult::PropertyNotFound;

		return FPropertyBagResult::Failure(FinalCode, NAME_None, FinalMessage);
	}

	FPropertyBagResult CopyMatchingProperties(const FInstancedPropertyBag& SourceBag, FInstancedPropertyBag& TargetBag, const bool bAllowConversion /*= true*/, const bool bCreateMissingProperties /*= false*/)
	{
		if (!SourceBag.IsValid())
		{
			return FPropertyBagResult::Failure(EPropertyBagResult::PropertyNotFound, NAME_None, TEXT("SourceBag is invalid."));
		}
		if (!TargetBag.IsValid())
		{
			return FPropertyBagResult::Failure(EPropertyBagResult::PropertyNotFound, NAME_None, TEXT("TargetBag is invalid."));
		}

		TArray<FName> MatchingNames = FindMatchingPropertyNames(SourceBag, TargetBag, bAllowConversion);
		TArray<TPair<FName, FName>> Mappings;
		Mappings.Reserve(MatchingNames.Num());
		for (const FName& Name : MatchingNames)
		{
			Mappings.Emplace(Name, Name);
		}

		// Add missing properties to mappings if creation is enabled
		if (bCreateMissingProperties)
		{
			if (const UPropertyBag* SourceStruct = SourceBag.GetPropertyBagStruct())
			{
				for (const FPropertyBagPropertyDesc& SourceDesc : SourceStruct->GetPropertyDescs())
				{
					if (SourceDesc.ValueType != EPropertyBagPropertyType::None && !MatchingNames.Contains(SourceDesc.Name) && !TargetBag.FindPropertyDescByName(SourceDesc.Name))
					{
						Mappings.Emplace(SourceDesc.Name, SourceDesc.Name);
					}
				}
			}
		}

		if (Mappings.IsEmpty())
		{
			return FPropertyBagResult::Success(NAME_None, TEXT("No matching or creatable properties found."));
		}

		return BatchTransferProperties(SourceBag, TargetBag, Mappings, bCreateMissingProperties);
	}

	TArray<FName> FindMatchingPropertyNames(const FInstancedPropertyBag& Bag1, const FInstancedPropertyBag& Bag2, const bool bAllowConversion /*= true*/)
	{
		TArray<FName> Result;
		if (!Bag1.IsValid() || !Bag2.IsValid())
		{
			return Result;
		}

		const UPropertyBag* Struct1 = Bag1.GetPropertyBagStruct();
		const UPropertyBag* Struct2 = Bag2.GetPropertyBagStruct();
		if (!Struct1 || !Struct2)
		{
			return Result;
		}

		TMap<FName, const FPropertyBagPropertyDesc*> Bag2PropsMap;
		const auto Descs2 = Private::GetCurrentDescriptorsView(Bag2);
		Bag2PropsMap.Reserve(Descs2.Num());
		for (const FPropertyBagPropertyDesc& Desc : Descs2)
		{
			Bag2PropsMap.Add(Desc.Name, &Desc);
		}

		const auto Descs1 = Private::GetCurrentDescriptorsView(Bag1);
		Result.Reserve(FMath::Min(Descs1.Num(), Descs2.Num()));
		for (const FPropertyBagPropertyDesc& Desc1 : Descs1)
		{
			const FPropertyBagPropertyDesc* const* Desc2Ptr = Bag2PropsMap.Find(Desc1.Name);
			if (!Desc2Ptr)
			{
				continue;
			}
			const FPropertyBagPropertyDesc* Desc2 = *Desc2Ptr;

			bool bAreCompatible = true;
			if (bAllowConversion)
			{
				const FProperty* Prop1 = Private::GetPropertyFromDesc(Bag1, &Desc1);
				const FProperty* Prop2 = Private::GetPropertyFromDesc(Bag2, Desc2);
				if (Prop1 && Prop2)
				{
					bAreCompatible = FlowPropertyUtils::ArePropertiesCompatible(Prop1, Prop2, true);
				}
				else
				{
					bAreCompatible = false;
					UE_LOG(LogFlow, Warning, TEXT("FindMatchingPropertyNames: Missing FProperty for '%s'."), *Desc1.Name.ToString());
				}
			}

			if (bAreCompatible)
			{
				Result.Add(Desc1.Name);
			}
		}
		return Result;
	}

	//----------------------------------------------------------------------//
	// Property Discovery and Manipulation
	//----------------------------------------------------------------------//

	FPropertyBagResult EnsurePropertyExistsWithType(FInstancedPropertyBag& TargetBag, FName PropertyName, const FPropertyBagPropertyDesc& NewTypeDesc, bool* bOutDefinitionChanged /*= nullptr*/)
	{
		if (bOutDefinitionChanged)
		{
			*bOutDefinitionChanged = false;
		}

		// Check if the property already exists with the correct type
		const FPropertyBagPropertyDesc* ExistingDesc = TargetBag.FindPropertyDescByName(PropertyName);
		if (ExistingDesc && ExistingDesc->ValueType == NewTypeDesc.ValueType && ExistingDesc->ValueTypeObject == NewTypeDesc.ValueTypeObject && ExistingDesc->ContainerTypes == NewTypeDesc.ContainerTypes)
		{
			return FPropertyBagResult::Success(PropertyName, TEXT("Property already exists with correct type."));
		}

		// Add or update property
		EPropertyBagAlterationResult Result;
		if (NewTypeDesc.ContainerTypes.IsEmpty())
		{
			Result = TargetBag.AddProperty(PropertyName, NewTypeDesc.ValueType, NewTypeDesc.ValueTypeObject);
		}
		else
		{
			Result = TargetBag.AddContainerProperty(PropertyName, NewTypeDesc.ContainerTypes, NewTypeDesc.ValueType, const_cast<UObject*>(NewTypeDesc.ValueTypeObject.Get()));
		}

		if (Result == EPropertyBagAlterationResult::Success)
		{
			if (bOutDefinitionChanged)
			{
				*bOutDefinitionChanged = true;
			}
			return FPropertyBagResult::Success(PropertyName, TEXT("Property added/migrated successfully."));
		}

		return FPropertyBagResult::Failure(EPropertyBagResult::TypeMismatch, PropertyName, TEXT("Failed to add or update property."));
	}

	TMap<FName, FProperty*> GetCachedPropertiesFromBag(const FInstancedPropertyBag& Bag)
	{
		TMap<FName, FProperty*> CachedProperties;
		if (const UPropertyBag* BagStruct = Bag.GetPropertyBagStruct())
		{
			for (const FPropertyBagPropertyDesc& Desc : BagStruct->GetPropertyDescs())
			{
				if (Desc.CachedProperty && !Desc.Name.IsNone())
				{
					CachedProperties.Emplace(Desc.Name, const_cast<FProperty*>(Desc.CachedProperty));
				}
			}
		}
		return CachedProperties;
	}

	bool SyncBagStructureWithDescriptors(FInstancedPropertyBag& TargetBag, const TConstArrayView<FPropertyBagPropertyDesc> RequiredDescs)
	{
		bool bStructureChanged = false;
		
		// Build map of required descriptors
		TMap<FName, const FPropertyBagPropertyDesc*> RequiredDescsMap;
		RequiredDescsMap.Reserve(RequiredDescs.Num());
		for (const FPropertyBagPropertyDesc& Desc : RequiredDescs)
		{
			if (!Desc.Name.IsNone())
			{
				RequiredDescsMap.Add(Desc.Name, &Desc);
			}
		}

		// Ensure all required properties exist
		for (const FPropertyBagPropertyDesc& ReqDesc : RequiredDescs)
		{
			if (ReqDesc.Name.IsNone())
			{
				continue;
			}
			bool bLocalDefinitionChanged = false;
			EnsurePropertyExistsWithType(TargetBag, ReqDesc.Name, ReqDesc, &bLocalDefinitionChanged);
			bStructureChanged |= bLocalDefinitionChanged;
		}

		// Remove obsolete properties
		TArray<FName> PropertiesToRemove;
		const auto CurrentDescs = Private::GetCurrentDescriptorsView(TargetBag);
		for (const FPropertyBagPropertyDesc& CurrentDesc : CurrentDescs)
		{
			if (!CurrentDesc.Name.IsNone() && !RequiredDescsMap.Contains(CurrentDesc.Name))
			{
				PropertiesToRemove.Add(CurrentDesc.Name);
			}
		}

		if (!PropertiesToRemove.IsEmpty())
		{
			TargetBag.RemovePropertiesByName(PropertiesToRemove);
			bStructureChanged = true;
		}
		
		return bStructureChanged;
	}

	void* FindPropertyBagMemoryForProperty(const FProperty* Property, const TConstArrayView<const FInstancedPropertyBag*> CandidateBags)
	{
		if (!Property)
		{
			return nullptr;
		}
		if (const UPropertyBag* BagOwner = Cast<UPropertyBag>(Property->GetOwnerStruct()))
		{
			for (const FInstancedPropertyBag* BagPtr : CandidateBags)
			{
				if (BagPtr && BagPtr->IsValid() && BagPtr->GetPropertyBagStruct() == BagOwner)
				{
					return const_cast<FInstancedPropertyBag*>(BagPtr)->GetMutableValue().GetMemory();
				}
			}
		}
		return nullptr;
	}
} // namespace PropertyBagUtils
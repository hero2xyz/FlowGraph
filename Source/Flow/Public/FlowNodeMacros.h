// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#pragma once

/////////////////////////////////////////////////////////////////////////////
// Macros for declaring runtime data pins in native Flow nodes.
//
// These macros are REQUIRED for any UPROPERTY marked with meta=(FlowDataPin).
// In editor builds, the metadata defines visual pins, but in cooked/shipping builds
// this metadata is stripped.
// Without declaring pins, the runtime cannot cache them, causing data transfer failures.
//
// Usage:
//   - Mark UPROPERTY with meta=(FlowDataPin="Input"/"Output")
//   - Override CachePinProperties() in your node
//   - Inside it, call DECLARE_INPUT_PIN(...) or DECLARE_OUTPUT_PIN(...)
/////////////////////////////////////////////////////////////////////////////

#define DECLARE_INPUT_PIN(PropertyName) \
{ \
    if (FProperty* Prop = GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(ThisClass, PropertyName))) \
    { \
        InputPropertyCache.Emplace(Prop->GetFName(), Prop); \
    } \
}

#define DECLARE_OUTPUT_PIN(PropertyName) \
{ \
    if (FProperty* Prop = GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(ThisClass, PropertyName))) \
    { \
        OutputPropertyCache.Emplace(Prop->GetFName(), Prop); \
    } \
}

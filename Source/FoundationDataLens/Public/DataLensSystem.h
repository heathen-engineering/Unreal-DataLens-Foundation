/**************************************************************************************
*                                                                                     *
* Copyright   2026 by Heathen Engineering Limited, an Irish registered company        *
* # 556277, VAT IE3394133CH, contact Heathen via support@heathen.group                *
*                                                                                     *
***************************************************************************************/

#pragma once

#include <cstdint>
#include "CoreMinimal.h"

// These three enums are cast directly (static_cast<int32_t>) to the matching Core enum's integer
// value when crossing the c_api.h boundary -- no lookup table -- so their declaration ORDER must
// stay identical to Core's own DataSystemOp / DataCompareOp / DataCurveType
// (extern/DataLens/Core/include/datalens/DataStore.h). If Core ever adds/reorders a value, this
// must be updated to match, not the other way around.

/** Mirrors DataSystemOp 0-5 (the bitwise ops 6-9 aren't wired through the Lens's f32/i32 System
 * entry points this binding calls, so they're intentionally not exposed here yet). */
enum class EDataLensSystemOp : uint8
{
	Set,
	Add,
	Sub,
	Mul,
	Min,
	Max,
};

/** Mirrors DataCompareOp 0-6 (the bitmask compares 7-9 aren't exposed here yet, same reasoning). */
enum class EDataLensCompareOp : uint8
{
	Always,
	Equal,
	NotEqual,
	Less,
	LessEqual,
	Greater,
	GreaterEqual,
};

/** Mirrors DataCurveType 0-3. */
enum class EDataLensCurveType : uint8
{
	Linear,
	Power,
	Smoothstep,
	Threshold,
};

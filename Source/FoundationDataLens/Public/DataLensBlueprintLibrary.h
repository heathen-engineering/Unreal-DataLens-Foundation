/**************************************************************************************
*                                                                                     *
* Copyright   2026 by Heathen Engineering Limited, an Irish registered company        *
* # 556277, VAT IE3394133CH, contact Heathen via support@heathen.group                *
*                                                                                     *
***************************************************************************************/

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DataLensSubsystem.h"
#include "DataLensBlueprintLibrary.generated.h"

/**
 * Blueprint-facing mirrors of EDataLensSystemOp / EDataLensCompareOp / EDataLensCurveType. UHT
 * can't reflect the core (non-UENUM) enums directly, and the core API is deliberately kept
 * Blueprint-free (see DataLensSubsystem.h's class comment), so these are separate BlueprintType
 * declarations. Declaration ORDER must stay identical to their core counterparts --
 * UDataLensBlueprintLibrary.cpp static_casts between them positionally, the same convention the
 * core API already uses at its own c_api.h boundary.
 */
UENUM(BlueprintType)
enum class EDataLensSystemOpBP : uint8
{
	Set,
	Add,
	Sub,
	Mul,
	Min,
	Max,
};

UENUM(BlueprintType)
enum class EDataLensCompareOpBP : uint8
{
	Always,
	Equal,
	NotEqual,
	Less,
	LessEqual,
	Greater,
	GreaterEqual,
};

UENUM(BlueprintType)
enum class EDataLensCurveTypeBP : uint8
{
	Linear,
	Power,
	Smoothstep,
	Threshold,
};

UENUM(BlueprintType)
enum class EDataLensColumnTypeBP : uint8
{
	Float,
	Int32,
};

/** Blueprint-facing schema entry: a GameplayTag (the one real tag concept a BP author touches --
 * see DataLensColumn.h) plus its value type. Converted to the core FDataLensColumn array (raw
 * uint64 id, hashed from the tag) only at the CreateStore boundary, never stored as-is. */
USTRUCT(BlueprintType)
struct FOUNDATIONDATALENS_API FDataLensColumnBP
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataLens")
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataLens")
	EDataLensColumnTypeBP Type = EDataLensColumnTypeBP::Float;
};

/**
 * M5: the ONLY place in this plugin that exposes DataLens to Blueprint, and the only place that
 * does int64<->uint64 boundary reinterpretation -- UHT doesn't support uint64 in reflected
 * signatures at all (see ToolkitSteamworks' own int64-for-SteamID precedent). Row indices,
 * revision counters, and System-affected-row counts are all conceptually non-negative uint64 on
 * the core/UDataLensSubsystem side; every wrapper here takes/returns the bit-identical int64
 * instead (a real row count will never approach INT64_MAX, and AllocRow's UINT64_MAX "no room"
 * sentinel round-trips to -1 under two's complement, matched by every compiler UE supports).
 *
 * Column/tag identity never needs a raw-id overload here at all -- FGameplayTag is already a
 * native Blueprint type, so every function below takes tags directly and hashes them via the
 * existing UDataLensSubsystem FGameplayTag overloads. A Blueprint author never sees a raw column
 * id.
 *
 * Deliberately NOT wrapped in M5: CreateView / FDataLensView. The view type is move-only and its
 * zero-copy GetRows<TRow>() is a C++ template with no Blueprint equivalent -- exposing views
 * would mean designing a whole separate copying/marshalling API, not just a boundary cast. Left
 * for a future milestone if a real Blueprint consumer needs per-row bulk reads; SetFloat/GetFloat
 * below already cover single-cell Blueprint access.
 */
UCLASS()
class FOUNDATIONDATALENS_API UDataLensBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Heathen|DataLens")
	static bool CreateStore(UDataLensSubsystem* Subsystem, const TArray<FDataLensColumnBP>& Columns, int64 PreallocRows);

	/** Returns -1 (bit-identical to the core's UINT64_MAX sentinel) if no store exists or it's at capacity. */
	UFUNCTION(BlueprintCallable, Category = "Heathen|DataLens")
	static int64 AllocRow(UDataLensSubsystem* Subsystem);

	UFUNCTION(BlueprintCallable, Category = "Heathen|DataLens")
	static void FreeRow(UDataLensSubsystem* Subsystem, int64 Row);

	UFUNCTION(BlueprintCallable, Category = "Heathen|DataLens")
	static bool SetFloat(UDataLensSubsystem* Subsystem, FGameplayTag ColumnTag, int64 Row, float Value);

	UFUNCTION(BlueprintCallable, Category = "Heathen|DataLens")
	static bool GetFloat(UDataLensSubsystem* Subsystem, FGameplayTag ColumnTag, int64 Row, float& OutValue);

	UFUNCTION(BlueprintCallable, Category = "Heathen|DataLens")
	static bool SetInt32(UDataLensSubsystem* Subsystem, FGameplayTag ColumnTag, int64 Row, int32 Value);

	UFUNCTION(BlueprintCallable, Category = "Heathen|DataLens")
	static bool GetInt32(UDataLensSubsystem* Subsystem, FGameplayTag ColumnTag, int64 Row, int32& OutValue);

	UFUNCTION(BlueprintCallable, Category = "Heathen|DataLens")
	static void SetValid(UDataLensSubsystem* Subsystem, int64 Row, bool bValid);

	UFUNCTION(BlueprintCallable, Category = "Heathen|DataLens")
	static bool IsValid(UDataLensSubsystem* Subsystem, int64 Row);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Heathen|DataLens")
	static bool HasStore(UDataLensSubsystem* Subsystem);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Heathen|DataLens")
	static bool HasColumn(UDataLensSubsystem* Subsystem, FGameplayTag ColumnTag);

	/** Unconditional scalar System. Returns rows affected. */
	UFUNCTION(BlueprintCallable, Category = "Heathen|DataLens")
	static int64 RunSystemFloat(UDataLensSubsystem* Subsystem, FGameplayTag TargetTag, EDataLensSystemOpBP Op, float Operand);

	UFUNCTION(BlueprintCallable, Category = "Heathen|DataLens")
	static int64 RunSystemFloatIf(UDataLensSubsystem* Subsystem, FGameplayTag TargetTag, EDataLensSystemOpBP Op, float Operand,
		FGameplayTag CompareTag, EDataLensCompareOpBP Compare, float Threshold);

	UFUNCTION(BlueprintCallable, Category = "Heathen|DataLens")
	static int64 RunSystemInt32(UDataLensSubsystem* Subsystem, FGameplayTag TargetTag, EDataLensSystemOpBP Op, int32 Operand);

	UFUNCTION(BlueprintCallable, Category = "Heathen|DataLens")
	static int64 RunSystemInt32If(UDataLensSubsystem* Subsystem, FGameplayTag TargetTag, EDataLensSystemOpBP Op, int32 Operand,
		FGameplayTag CompareTag, EDataLensCompareOpBP Compare, int32 Threshold);

	/** Cross-column System: TargetTag = TargetTag OP (per-row value of OperandTag). */
	UFUNCTION(BlueprintCallable, Category = "Heathen|DataLens")
	static int64 RunSystemColumnFloat(UDataLensSubsystem* Subsystem, FGameplayTag TargetTag, EDataLensSystemOpBP Op, FGameplayTag OperandTag);

	UFUNCTION(BlueprintCallable, Category = "Heathen|DataLens")
	static int64 RunSystemColumnFloatIf(UDataLensSubsystem* Subsystem, FGameplayTag TargetTag, EDataLensSystemOpBP Op, FGameplayTag OperandTag,
		FGameplayTag CompareTag, EDataLensCompareOpBP Compare, float Threshold);

	/** Cross-column System with the per-row operand passed through a response curve first. */
	UFUNCTION(BlueprintCallable, Category = "Heathen|DataLens")
	static int64 RunSystemCurvedFloat(UDataLensSubsystem* Subsystem, FGameplayTag TargetTag, EDataLensSystemOpBP Op, FGameplayTag OperandTag,
		EDataLensCurveTypeBP CurveType, float CurveMin, float CurveMax, float CurveP0, float CurveP1, bool bInvertCurve);

	UFUNCTION(BlueprintCallable, Category = "Heathen|DataLens")
	static int64 RunSystemCurvedFloatIf(UDataLensSubsystem* Subsystem, FGameplayTag TargetTag, EDataLensSystemOpBP Op, FGameplayTag OperandTag,
		EDataLensCurveTypeBP CurveType, float CurveMin, float CurveMax, float CurveP0, float CurveP1, bool bInvertCurve,
		FGameplayTag CompareTag, EDataLensCompareOpBP Compare, float Threshold);

	// ---- Replication primitives (M4 surfaced for Blueprint) ----

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Heathen|DataLens")
	static int64 GetRevision(UDataLensSubsystem* Subsystem);

	UFUNCTION(BlueprintCallable, Category = "Heathen|DataLens")
	static void SetRevision(UDataLensSubsystem* Subsystem, int64 Revision);

	UFUNCTION(BlueprintCallable, Category = "Heathen|DataLens")
	static int64 BumpRevision(UDataLensSubsystem* Subsystem);

	UFUNCTION(BlueprintCallable, Category = "Heathen|DataLens")
	static void MarkColumnDirty(UDataLensSubsystem* Subsystem, FGameplayTag ColumnTag);

	UFUNCTION(BlueprintCallable, Category = "Heathen|DataLens")
	static TArray<uint8> Snapshot(UDataLensSubsystem* Subsystem);

	UFUNCTION(BlueprintCallable, Category = "Heathen|DataLens")
	static TArray<uint8> CollectDelta(UDataLensSubsystem* Subsystem, int64 SinceRevision);

	UFUNCTION(BlueprintCallable, Category = "Heathen|DataLens")
	static bool ApplyPayload(UDataLensSubsystem* Subsystem, const TArray<uint8>& Payload);
};

/**************************************************************************************
*                                                                                     *
* Copyright   2026 by Heathen Engineering Limited, an Irish registered company        *
* # 556277, VAT IE3394133CH, contact Heathen via support@heathen.group                *
*                                                                                     *
***************************************************************************************/

#include "DataLensBlueprintLibrary.h"
#include "DataLensColumn.h"

namespace
{
	// Bit-identical round trip: a real row/revision/affected-row count never approaches
	// INT64_MAX, and AllocRow's UINT64_MAX "no room" sentinel becomes -1 here, matched by every
	// compiler UE supports (two's complement).
	FORCEINLINE int64 ToBPValue(uint64 Value)
	{
		return static_cast<int64>(Value);
	}

	FORCEINLINE uint64 ToCoreValue(int64 Value)
	{
		return static_cast<uint64>(Value);
	}

	FDataLensColumn ToCoreColumn(const FDataLensColumnBP& ColumnBP)
	{
		switch (ColumnBP.Type)
		{
		case EDataLensColumnTypeBP::Int32:
			return FDataLensColumn::OfInt32(ColumnBP.Tag);
		case EDataLensColumnTypeBP::Float:
		default:
			return FDataLensColumn::OfFloat(ColumnBP.Tag);
		}
	}
}

bool UDataLensBlueprintLibrary::CreateStore(UDataLensSubsystem* Subsystem, const TArray<FDataLensColumnBP>& Columns, int64 PreallocRows)
{
	if (Subsystem == nullptr)
	{
		return false;
	}

	TArray<FDataLensColumn> CoreColumns;
	CoreColumns.Reserve(Columns.Num());
	for (const FDataLensColumnBP& ColumnBP : Columns)
	{
		CoreColumns.Add(ToCoreColumn(ColumnBP));
	}

	return Subsystem->CreateStore(CoreColumns, ToCoreValue(PreallocRows));
}

int64 UDataLensBlueprintLibrary::AllocRow(UDataLensSubsystem* Subsystem)
{
	return Subsystem != nullptr ? ToBPValue(Subsystem->AllocRow()) : -1;
}

void UDataLensBlueprintLibrary::FreeRow(UDataLensSubsystem* Subsystem, int64 Row)
{
	if (Subsystem != nullptr)
	{
		Subsystem->FreeRow(ToCoreValue(Row));
	}
}

bool UDataLensBlueprintLibrary::SetFloat(UDataLensSubsystem* Subsystem, FGameplayTag ColumnTag, int64 Row, float Value)
{
	return Subsystem != nullptr && Subsystem->SetFloat(ColumnTag, ToCoreValue(Row), Value);
}

bool UDataLensBlueprintLibrary::GetFloat(UDataLensSubsystem* Subsystem, FGameplayTag ColumnTag, int64 Row, float& OutValue)
{
	OutValue = 0.0f;
	return Subsystem != nullptr && Subsystem->GetFloat(ColumnTag, ToCoreValue(Row), OutValue);
}

bool UDataLensBlueprintLibrary::SetInt32(UDataLensSubsystem* Subsystem, FGameplayTag ColumnTag, int64 Row, int32 Value)
{
	return Subsystem != nullptr && Subsystem->SetInt32(ColumnTag, ToCoreValue(Row), Value);
}

bool UDataLensBlueprintLibrary::GetInt32(UDataLensSubsystem* Subsystem, FGameplayTag ColumnTag, int64 Row, int32& OutValue)
{
	OutValue = 0;
	return Subsystem != nullptr && Subsystem->GetInt32(ColumnTag, ToCoreValue(Row), OutValue);
}

void UDataLensBlueprintLibrary::SetValid(UDataLensSubsystem* Subsystem, int64 Row, bool bValid)
{
	if (Subsystem != nullptr)
	{
		Subsystem->SetValid(ToCoreValue(Row), bValid);
	}
}

bool UDataLensBlueprintLibrary::IsValid(UDataLensSubsystem* Subsystem, int64 Row)
{
	return Subsystem != nullptr && Subsystem->IsValid(ToCoreValue(Row));
}

bool UDataLensBlueprintLibrary::HasStore(UDataLensSubsystem* Subsystem)
{
	return Subsystem != nullptr && Subsystem->HasStore();
}

bool UDataLensBlueprintLibrary::HasColumn(UDataLensSubsystem* Subsystem, FGameplayTag ColumnTag)
{
	return Subsystem != nullptr && Subsystem->HasColumn(ColumnTag);
}

int64 UDataLensBlueprintLibrary::RunSystemFloat(UDataLensSubsystem* Subsystem, FGameplayTag TargetTag, EDataLensSystemOpBP Op, float Operand)
{
	return Subsystem != nullptr
		? ToBPValue(Subsystem->RunSystemFloat(TargetTag, static_cast<EDataLensSystemOp>(Op), Operand))
		: 0;
}

int64 UDataLensBlueprintLibrary::RunSystemFloatIf(UDataLensSubsystem* Subsystem, FGameplayTag TargetTag, EDataLensSystemOpBP Op, float Operand,
	FGameplayTag CompareTag, EDataLensCompareOpBP Compare, float Threshold)
{
	return Subsystem != nullptr
		? ToBPValue(Subsystem->RunSystemFloatIf(TargetTag, static_cast<EDataLensSystemOp>(Op), Operand,
			CompareTag, static_cast<EDataLensCompareOp>(Compare), Threshold))
		: 0;
}

int64 UDataLensBlueprintLibrary::RunSystemInt32(UDataLensSubsystem* Subsystem, FGameplayTag TargetTag, EDataLensSystemOpBP Op, int32 Operand)
{
	return Subsystem != nullptr
		? ToBPValue(Subsystem->RunSystemInt32(TargetTag, static_cast<EDataLensSystemOp>(Op), Operand))
		: 0;
}

int64 UDataLensBlueprintLibrary::RunSystemInt32If(UDataLensSubsystem* Subsystem, FGameplayTag TargetTag, EDataLensSystemOpBP Op, int32 Operand,
	FGameplayTag CompareTag, EDataLensCompareOpBP Compare, int32 Threshold)
{
	return Subsystem != nullptr
		? ToBPValue(Subsystem->RunSystemInt32If(TargetTag, static_cast<EDataLensSystemOp>(Op), Operand,
			CompareTag, static_cast<EDataLensCompareOp>(Compare), Threshold))
		: 0;
}

int64 UDataLensBlueprintLibrary::RunSystemColumnFloat(UDataLensSubsystem* Subsystem, FGameplayTag TargetTag, EDataLensSystemOpBP Op, FGameplayTag OperandTag)
{
	return Subsystem != nullptr
		? ToBPValue(Subsystem->RunSystemColumnFloat(TargetTag, static_cast<EDataLensSystemOp>(Op), OperandTag))
		: 0;
}

int64 UDataLensBlueprintLibrary::RunSystemColumnFloatIf(UDataLensSubsystem* Subsystem, FGameplayTag TargetTag, EDataLensSystemOpBP Op, FGameplayTag OperandTag,
	FGameplayTag CompareTag, EDataLensCompareOpBP Compare, float Threshold)
{
	return Subsystem != nullptr
		? ToBPValue(Subsystem->RunSystemColumnFloatIf(TargetTag, static_cast<EDataLensSystemOp>(Op), OperandTag,
			CompareTag, static_cast<EDataLensCompareOp>(Compare), Threshold))
		: 0;
}

int64 UDataLensBlueprintLibrary::RunSystemCurvedFloat(UDataLensSubsystem* Subsystem, FGameplayTag TargetTag, EDataLensSystemOpBP Op, FGameplayTag OperandTag,
	EDataLensCurveTypeBP CurveType, float CurveMin, float CurveMax, float CurveP0, float CurveP1, bool bInvertCurve)
{
	return Subsystem != nullptr
		? ToBPValue(Subsystem->RunSystemCurvedFloat(TargetTag, static_cast<EDataLensSystemOp>(Op), OperandTag,
			static_cast<EDataLensCurveType>(CurveType), CurveMin, CurveMax, CurveP0, CurveP1, bInvertCurve))
		: 0;
}

int64 UDataLensBlueprintLibrary::RunSystemCurvedFloatIf(UDataLensSubsystem* Subsystem, FGameplayTag TargetTag, EDataLensSystemOpBP Op, FGameplayTag OperandTag,
	EDataLensCurveTypeBP CurveType, float CurveMin, float CurveMax, float CurveP0, float CurveP1, bool bInvertCurve,
	FGameplayTag CompareTag, EDataLensCompareOpBP Compare, float Threshold)
{
	return Subsystem != nullptr
		? ToBPValue(Subsystem->RunSystemCurvedFloatIf(TargetTag, static_cast<EDataLensSystemOp>(Op), OperandTag,
			static_cast<EDataLensCurveType>(CurveType), CurveMin, CurveMax, CurveP0, CurveP1, bInvertCurve,
			CompareTag, static_cast<EDataLensCompareOp>(Compare), Threshold))
		: 0;
}

int64 UDataLensBlueprintLibrary::GetRevision(UDataLensSubsystem* Subsystem)
{
	return Subsystem != nullptr ? ToBPValue(Subsystem->GetRevision()) : 0;
}

void UDataLensBlueprintLibrary::SetRevision(UDataLensSubsystem* Subsystem, int64 Revision)
{
	if (Subsystem != nullptr)
	{
		Subsystem->SetRevision(ToCoreValue(Revision));
	}
}

int64 UDataLensBlueprintLibrary::BumpRevision(UDataLensSubsystem* Subsystem)
{
	return Subsystem != nullptr ? ToBPValue(Subsystem->BumpRevision()) : 0;
}

void UDataLensBlueprintLibrary::MarkColumnDirty(UDataLensSubsystem* Subsystem, FGameplayTag ColumnTag)
{
	if (Subsystem != nullptr)
	{
		Subsystem->MarkColumnDirty(ColumnTag);
	}
}

TArray<uint8> UDataLensBlueprintLibrary::Snapshot(UDataLensSubsystem* Subsystem)
{
	return Subsystem != nullptr ? Subsystem->Snapshot() : TArray<uint8>();
}

TArray<uint8> UDataLensBlueprintLibrary::CollectDelta(UDataLensSubsystem* Subsystem, int64 SinceRevision)
{
	return Subsystem != nullptr ? Subsystem->CollectDelta(ToCoreValue(SinceRevision)) : TArray<uint8>();
}

bool UDataLensBlueprintLibrary::ApplyPayload(UDataLensSubsystem* Subsystem, const TArray<uint8>& Payload)
{
	return Subsystem != nullptr && Subsystem->ApplyPayload(Payload);
}

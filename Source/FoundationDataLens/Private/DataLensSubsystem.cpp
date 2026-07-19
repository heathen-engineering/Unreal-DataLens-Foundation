/**************************************************************************************
*                                                                                     *
* Copyright   2026 by Heathen Engineering Limited, an Irish registered company        *
* # 556277, VAT IE3394133CH, contact Heathen via support@heathen.group                *
*                                                                                     *
***************************************************************************************/

#include "DataLensSubsystem.h"
#include "datalens/c_api.h"

void UDataLensSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 0 == use hardware_concurrency, matching Core's own dl_lens_create(0) convention.
	Lens = dl_lens_create(0);
}

void UDataLensSubsystem::Deinitialize()
{
	if (Lens != nullptr)
	{
		dl_lens_destroy(Lens);
		Lens = nullptr;
	}

	if (Store != nullptr)
	{
		dl_store_destroy(Store);
		Store = nullptr;
	}

	ColumnIndexById.Reset();

	Super::Deinitialize();
}

bool UDataLensSubsystem::CreateStore(const TArray<FDataLensColumn>& Columns, uint64 PreallocRows)
{
	if (Store != nullptr || PreallocRows == 0 || Columns.Num() == 0)
	{
		return false;
	}

	// Reject a schema with duplicate column ids before touching the native store at all.
	TSet<uint64> UniqueIds;
	UniqueIds.Reserve(Columns.Num());
	for (const FDataLensColumn& Column : Columns)
	{
		bool bAlreadyInSet = false;
		UniqueIds.Add(Column.Id, &bAlreadyInSet);
		if (bAlreadyInSet)
		{
			return false;
		}
	}

	// uint64_t (cstdint), not UE's own `uint64` -- c_api.h's pointer params are `const uint64_t*`,
	// and on this platform uint64_t is `unsigned long` while UE's uint64 is `unsigned long long`:
	// same width, distinct types, so TArray<uint64>::GetData() won't implicitly convert.
	TArray<uint64_t> ColIds;
	TArray<uint64_t> ColStrides;
	ColIds.Reserve(Columns.Num());
	ColStrides.Reserve(Columns.Num());
	for (const FDataLensColumn& Column : Columns)
	{
		ColIds.Add(Column.Id);
		ColStrides.Add(Column.Stride);
	}

	dl_store* NewStore = dl_store_create(ColIds.GetData(), ColStrides.GetData(), /*colDefaults*/ nullptr,
		Columns.Num(), PreallocRows);
	if (NewStore == nullptr)
	{
		return false;
	}

	Store = NewStore;

	ColumnIndexById.Reset();
	ColumnIndexById.Reserve(Columns.Num());
	for (int32 Index = 0; Index < Columns.Num(); ++Index)
	{
		ColumnIndexById.Add(Columns[Index].Id, Index);
	}

	return true;
}

uint64 UDataLensSubsystem::AllocRow()
{
	if (Store == nullptr)
	{
		return UINT64_MAX;
	}

	return dl_store_alloc_row(Store);
}

void UDataLensSubsystem::FreeRow(uint64 Row)
{
	if (Store == nullptr)
	{
		return;
	}

	dl_store_free_row(Store, Row);
}

bool UDataLensSubsystem::FindColumnIndex(uint64 ColumnId, uint64& OutIndex) const
{
	if (Store == nullptr)
	{
		return false;
	}

	const int32* Index = ColumnIndexById.Find(ColumnId);
	if (Index == nullptr)
	{
		return false;
	}

	OutIndex = static_cast<uint64>(*Index);
	return true;
}

bool UDataLensSubsystem::HasColumn(uint64 ColumnId) const
{
	uint64 Index = 0;
	return FindColumnIndex(ColumnId, Index);
}

bool UDataLensSubsystem::HasColumn(const FGameplayTag& ColumnTag) const
{
	return HasColumn(HashDataLensTag(ColumnTag));
}

bool UDataLensSubsystem::SetFloat(uint64 ColumnId, uint64 Row, float Value)
{
	uint64 Index = 0;
	if (!FindColumnIndex(ColumnId, Index))
	{
		return false;
	}

	return dl_store_set_f32(Store, Row, Index, Value) != 0;
}

bool UDataLensSubsystem::SetFloat(const FGameplayTag& ColumnTag, uint64 Row, float Value)
{
	return SetFloat(HashDataLensTag(ColumnTag), Row, Value);
}

bool UDataLensSubsystem::GetFloat(uint64 ColumnId, uint64 Row, float& OutValue) const
{
	uint64 Index = 0;
	if (!FindColumnIndex(ColumnId, Index))
	{
		return false;
	}

	float Value = 0.0f;
	if (dl_store_get_f32(Store, Row, Index, &Value) == 0)
	{
		return false;
	}

	OutValue = Value;
	return true;
}

bool UDataLensSubsystem::GetFloat(const FGameplayTag& ColumnTag, uint64 Row, float& OutValue) const
{
	return GetFloat(HashDataLensTag(ColumnTag), Row, OutValue);
}

bool UDataLensSubsystem::SetInt32(uint64 ColumnId, uint64 Row, int32 Value)
{
	uint64 Index = 0;
	if (!FindColumnIndex(ColumnId, Index))
	{
		return false;
	}

	return dl_store_set_i32(Store, Row, Index, Value) != 0;
}

bool UDataLensSubsystem::SetInt32(const FGameplayTag& ColumnTag, uint64 Row, int32 Value)
{
	return SetInt32(HashDataLensTag(ColumnTag), Row, Value);
}

bool UDataLensSubsystem::GetInt32(uint64 ColumnId, uint64 Row, int32& OutValue) const
{
	uint64 Index = 0;
	if (!FindColumnIndex(ColumnId, Index))
	{
		return false;
	}

	int32_t Value = 0;
	if (dl_store_get_i32(Store, Row, Index, &Value) == 0)
	{
		return false;
	}

	OutValue = Value;
	return true;
}

bool UDataLensSubsystem::GetInt32(const FGameplayTag& ColumnTag, uint64 Row, int32& OutValue) const
{
	return GetInt32(HashDataLensTag(ColumnTag), Row, OutValue);
}

void UDataLensSubsystem::SetValid(uint64 Row, bool bValid)
{
	if (Store == nullptr)
	{
		return;
	}

	dl_store_set_valid(Store, Row, bValid ? 1 : 0);
}

bool UDataLensSubsystem::IsValid(uint64 Row) const
{
	if (Store == nullptr)
	{
		return false;
	}

	return dl_store_is_valid(Store, Row) != 0;
}

TOptional<FDataLensView> UDataLensSubsystem::CreateView(const TArray<uint64>& ColumnIds) const
{
	if (Store == nullptr || ColumnIds.Num() == 0)
	{
		return TOptional<FDataLensView>();
	}

	TArray<uint64_t> SourceColumns;
	SourceColumns.Reserve(ColumnIds.Num());
	for (uint64 ColumnId : ColumnIds)
	{
		uint64 Index = 0;
		if (!FindColumnIndex(ColumnId, Index))
		{
			return TOptional<FDataLensView>();
		}

		SourceColumns.Add(Index);
	}

	FDataLensView View(SourceColumns);
	View.Refresh(Store);
	return TOptional<FDataLensView>(MoveTemp(View));
}

TOptional<FDataLensView> UDataLensSubsystem::CreateView(const TArray<FGameplayTag>& ColumnTags) const
{
	TArray<uint64> ColumnIds;
	ColumnIds.Reserve(ColumnTags.Num());
	for (const FGameplayTag& ColumnTag : ColumnTags)
	{
		ColumnIds.Add(HashDataLensTag(ColumnTag));
	}

	return CreateView(ColumnIds);
}

void UDataLensSubsystem::EnsureLens()
{
	// Lens is normally created in Initialize(), but a UGameInstanceSubsystem constructed via a raw
	// NewObject<T>(SomeGameInstance) (as opposed to a real UGameInstance::Init() driving its
	// subsystem collection) never invokes Initialize() at all -- Lens would silently stay null in
	// that path. This lazy fallback makes every RunSystem* entry point robust to that regardless of
	// how the subsystem came to exist, rather than requiring every caller to know the difference.
	if (Lens == nullptr)
	{
		Lens = dl_lens_create(0);
	}
}

uint64 UDataLensSubsystem::RunSystemFloat(uint64 TargetId, EDataLensSystemOp Op, float Operand)
{
	EnsureLens();
	uint64 TargetIndex = 0;
	if (!FindColumnIndex(TargetId, TargetIndex))
	{
		return 0;
	}

	return dl_lens_run_f32(Lens, Store, TargetIndex, static_cast<int32_t>(Op), Operand,
		/*hasPredicate*/ 0, /*compareCol*/ 0, /*cmp*/ 0, /*threshold*/ 0.0f);
}

uint64 UDataLensSubsystem::RunSystemFloat(const FGameplayTag& TargetTag, EDataLensSystemOp Op, float Operand)
{
	return RunSystemFloat(HashDataLensTag(TargetTag), Op, Operand);
}

uint64 UDataLensSubsystem::RunSystemFloatIf(uint64 TargetId, EDataLensSystemOp Op, float Operand,
	uint64 CompareId, EDataLensCompareOp Compare, float Threshold)
{
	EnsureLens();
	uint64 TargetIndex = 0;
	uint64 CompareIndex = 0;
	if (!FindColumnIndex(TargetId, TargetIndex) || !FindColumnIndex(CompareId, CompareIndex))
	{
		return 0;
	}

	return dl_lens_run_f32(Lens, Store, TargetIndex, static_cast<int32_t>(Op), Operand,
		/*hasPredicate*/ 1, CompareIndex, static_cast<int32_t>(Compare), Threshold);
}

uint64 UDataLensSubsystem::RunSystemFloatIf(const FGameplayTag& TargetTag, EDataLensSystemOp Op, float Operand,
	const FGameplayTag& CompareTag, EDataLensCompareOp Compare, float Threshold)
{
	return RunSystemFloatIf(HashDataLensTag(TargetTag), Op, Operand, HashDataLensTag(CompareTag), Compare, Threshold);
}

uint64 UDataLensSubsystem::RunSystemInt32(uint64 TargetId, EDataLensSystemOp Op, int32 Operand)
{
	EnsureLens();
	uint64 TargetIndex = 0;
	if (!FindColumnIndex(TargetId, TargetIndex))
	{
		return 0;
	}

	return dl_lens_run_i32(Lens, Store, TargetIndex, static_cast<int32_t>(Op), Operand,
		/*hasPredicate*/ 0, /*compareCol*/ 0, /*cmp*/ 0, /*threshold*/ 0);
}

uint64 UDataLensSubsystem::RunSystemInt32(const FGameplayTag& TargetTag, EDataLensSystemOp Op, int32 Operand)
{
	return RunSystemInt32(HashDataLensTag(TargetTag), Op, Operand);
}

uint64 UDataLensSubsystem::RunSystemInt32If(uint64 TargetId, EDataLensSystemOp Op, int32 Operand,
	uint64 CompareId, EDataLensCompareOp Compare, int32 Threshold)
{
	EnsureLens();
	uint64 TargetIndex = 0;
	uint64 CompareIndex = 0;
	if (!FindColumnIndex(TargetId, TargetIndex) || !FindColumnIndex(CompareId, CompareIndex))
	{
		return 0;
	}

	return dl_lens_run_i32(Lens, Store, TargetIndex, static_cast<int32_t>(Op), Operand,
		/*hasPredicate*/ 1, CompareIndex, static_cast<int32_t>(Compare), Threshold);
}

uint64 UDataLensSubsystem::RunSystemInt32If(const FGameplayTag& TargetTag, EDataLensSystemOp Op, int32 Operand,
	const FGameplayTag& CompareTag, EDataLensCompareOp Compare, int32 Threshold)
{
	return RunSystemInt32If(HashDataLensTag(TargetTag), Op, Operand, HashDataLensTag(CompareTag), Compare, Threshold);
}

uint64 UDataLensSubsystem::RunSystemColumnFloat(uint64 TargetId, EDataLensSystemOp Op, uint64 OperandId)
{
	EnsureLens();
	uint64 TargetIndex = 0;
	uint64 OperandIndex = 0;
	if (!FindColumnIndex(TargetId, TargetIndex) || !FindColumnIndex(OperandId, OperandIndex))
	{
		return 0;
	}

	return dl_lens_run_col_f32(Lens, Store, TargetIndex, static_cast<int32_t>(Op), OperandIndex,
		/*hasPredicate*/ 0, /*compareCol*/ 0, /*cmp*/ 0, /*threshold*/ 0.0f);
}

uint64 UDataLensSubsystem::RunSystemColumnFloat(const FGameplayTag& TargetTag, EDataLensSystemOp Op, const FGameplayTag& OperandTag)
{
	return RunSystemColumnFloat(HashDataLensTag(TargetTag), Op, HashDataLensTag(OperandTag));
}

uint64 UDataLensSubsystem::RunSystemColumnFloatIf(uint64 TargetId, EDataLensSystemOp Op, uint64 OperandId,
	uint64 CompareId, EDataLensCompareOp Compare, float Threshold)
{
	EnsureLens();
	uint64 TargetIndex = 0;
	uint64 OperandIndex = 0;
	uint64 CompareIndex = 0;
	if (!FindColumnIndex(TargetId, TargetIndex) || !FindColumnIndex(OperandId, OperandIndex)
		|| !FindColumnIndex(CompareId, CompareIndex))
	{
		return 0;
	}

	return dl_lens_run_col_f32(Lens, Store, TargetIndex, static_cast<int32_t>(Op), OperandIndex,
		/*hasPredicate*/ 1, CompareIndex, static_cast<int32_t>(Compare), Threshold);
}

uint64 UDataLensSubsystem::RunSystemColumnFloatIf(const FGameplayTag& TargetTag, EDataLensSystemOp Op, const FGameplayTag& OperandTag,
	const FGameplayTag& CompareTag, EDataLensCompareOp Compare, float Threshold)
{
	return RunSystemColumnFloatIf(HashDataLensTag(TargetTag), Op, HashDataLensTag(OperandTag),
		HashDataLensTag(CompareTag), Compare, Threshold);
}

uint64 UDataLensSubsystem::RunSystemCurvedFloat(uint64 TargetId, EDataLensSystemOp Op, uint64 OperandId,
	EDataLensCurveType CurveType, float CurveMin, float CurveMax, float CurveP0, float CurveP1, bool bInvertCurve)
{
	EnsureLens();
	uint64 TargetIndex = 0;
	uint64 OperandIndex = 0;
	if (!FindColumnIndex(TargetId, TargetIndex) || !FindColumnIndex(OperandId, OperandIndex))
	{
		return 0;
	}

	return dl_lens_run_curved_f32(Lens, Store, TargetIndex, static_cast<int32_t>(Op), OperandIndex,
		static_cast<int32_t>(CurveType), CurveMin, CurveMax, CurveP0, CurveP1, bInvertCurve ? 1 : 0,
		/*hasPredicate*/ 0, /*compareCol*/ 0, /*cmp*/ 0, /*threshold*/ 0.0f);
}

uint64 UDataLensSubsystem::RunSystemCurvedFloat(const FGameplayTag& TargetTag, EDataLensSystemOp Op, const FGameplayTag& OperandTag,
	EDataLensCurveType CurveType, float CurveMin, float CurveMax, float CurveP0, float CurveP1, bool bInvertCurve)
{
	return RunSystemCurvedFloat(HashDataLensTag(TargetTag), Op, HashDataLensTag(OperandTag),
		CurveType, CurveMin, CurveMax, CurveP0, CurveP1, bInvertCurve);
}

uint64 UDataLensSubsystem::RunSystemCurvedFloatIf(uint64 TargetId, EDataLensSystemOp Op, uint64 OperandId,
	EDataLensCurveType CurveType, float CurveMin, float CurveMax, float CurveP0, float CurveP1, bool bInvertCurve,
	uint64 CompareId, EDataLensCompareOp Compare, float Threshold)
{
	EnsureLens();
	uint64 TargetIndex = 0;
	uint64 OperandIndex = 0;
	uint64 CompareIndex = 0;
	if (!FindColumnIndex(TargetId, TargetIndex) || !FindColumnIndex(OperandId, OperandIndex)
		|| !FindColumnIndex(CompareId, CompareIndex))
	{
		return 0;
	}

	return dl_lens_run_curved_f32(Lens, Store, TargetIndex, static_cast<int32_t>(Op), OperandIndex,
		static_cast<int32_t>(CurveType), CurveMin, CurveMax, CurveP0, CurveP1, bInvertCurve ? 1 : 0,
		/*hasPredicate*/ 1, CompareIndex, static_cast<int32_t>(Compare), Threshold);
}

uint64 UDataLensSubsystem::RunSystemCurvedFloatIf(const FGameplayTag& TargetTag, EDataLensSystemOp Op, const FGameplayTag& OperandTag,
	EDataLensCurveType CurveType, float CurveMin, float CurveMax, float CurveP0, float CurveP1, bool bInvertCurve,
	const FGameplayTag& CompareTag, EDataLensCompareOp Compare, float Threshold)
{
	return RunSystemCurvedFloatIf(HashDataLensTag(TargetTag), Op, HashDataLensTag(OperandTag),
		CurveType, CurveMin, CurveMax, CurveP0, CurveP1, bInvertCurve,
		HashDataLensTag(CompareTag), Compare, Threshold);
}

uint64 UDataLensSubsystem::GetRevision() const
{
	if (Store == nullptr)
	{
		return 0;
	}

	return dl_store_revision(Store);
}

void UDataLensSubsystem::SetRevision(uint64 Revision)
{
	if (Store == nullptr)
	{
		return;
	}

	dl_store_set_revision(Store, Revision);
}

uint64 UDataLensSubsystem::BumpRevision()
{
	if (Store == nullptr)
	{
		return 0;
	}

	return dl_store_bump_revision(Store);
}

void UDataLensSubsystem::MarkColumnDirty(uint64 ColumnId)
{
	uint64 Index = 0;
	if (!FindColumnIndex(ColumnId, Index))
	{
		return;
	}

	dl_store_mark_column_dirty(Store, Index);
}

void UDataLensSubsystem::MarkColumnDirty(const FGameplayTag& ColumnTag)
{
	MarkColumnDirty(HashDataLensTag(ColumnTag));
}

TArray<uint8> UDataLensSubsystem::Snapshot()
{
	EnsureLens();
	if (Store == nullptr)
	{
		return TArray<uint8>();
	}

	// Query-size-then-fill convention: call with buf==NULL to get the required size, then again
	// with a buffer of at least that size (same pattern as dl_ir_serialize per the ABI comment).
	const uint64_t RequiredSize = dl_lens_snapshot(Lens, Store, /*scopeBits*/ nullptr, /*scopeWords*/ 0, nullptr, 0);
	if (RequiredSize == 0)
	{
		return TArray<uint8>();
	}

	TArray<uint8> Payload;
	Payload.SetNumUninitialized(RequiredSize);
	const uint64_t WrittenSize = dl_lens_snapshot(Lens, Store, nullptr, 0, Payload.GetData(), RequiredSize);
	if (WrittenSize != RequiredSize)
	{
		// Defensive: shouldn't happen given the query-then-fill contract, but never hand back a
		// partially-written buffer if it does.
		return TArray<uint8>();
	}

	return Payload;
}

TArray<uint8> UDataLensSubsystem::CollectDelta(uint64 SinceRevision)
{
	EnsureLens();
	if (Store == nullptr)
	{
		return TArray<uint8>();
	}

	const uint64_t RequiredSize = dl_lens_collect_delta(Lens, Store, SinceRevision,
		/*scopeBits*/ nullptr, /*scopeWords*/ 0, nullptr, 0);
	if (RequiredSize == 0)
	{
		return TArray<uint8>();
	}

	TArray<uint8> Payload;
	Payload.SetNumUninitialized(RequiredSize);
	const uint64_t WrittenSize = dl_lens_collect_delta(Lens, Store, SinceRevision, nullptr, 0, Payload.GetData(), RequiredSize);
	if (WrittenSize != RequiredSize)
	{
		return TArray<uint8>();
	}

	return Payload;
}

bool UDataLensSubsystem::ApplyPayload(const TArray<uint8>& Payload)
{
	EnsureLens();
	if (Store == nullptr || Payload.Num() == 0)
	{
		return false;
	}

	return dl_lens_apply_payload(Lens, Store, Payload.GetData(), static_cast<uint64_t>(Payload.Num())) != 0;
}

/**************************************************************************************
*                                                                                     *
* Copyright   2026 by Heathen Engineering Limited, an Irish registered company        *
* # 556277, VAT IE3394133CH, contact Heathen via support@heathen.group                *
*                                                                                     *
***************************************************************************************/

#pragma once

#include <cstdint>
#include "CoreMinimal.h"
#include "Containers/ArrayView.h"

struct dl_view;
struct dl_store;

/**
 * A read-only, row-major SNAPSHOT of selected columns of a store's live rows (mirrors Core's own
 * datalens::DataView -- see extern/DataLens/Core/include/datalens/DataView.h). It is a COPY, not
 * an alias of the store: Refresh re-gathers the current live rows into this snapshot. Owns a
 * native dl_view handle (move-only).
 */
class FOUNDATIONDATALENS_API FDataLensView
{
public:
	/** SourceColumns are STORE column indices (as resolved via UDataLensSubsystem's tag map), not tags. */
	explicit FDataLensView(const TArray<uint64_t>& SourceColumns);
	~FDataLensView();

	FDataLensView(const FDataLensView&) = delete;
	FDataLensView& operator=(const FDataLensView&) = delete;
	FDataLensView(FDataLensView&& Other) noexcept;
	FDataLensView& operator=(FDataLensView&& Other) noexcept;

	/** Re-gathers the current live rows from Store into this snapshot. */
	void Refresh(dl_store* Store);

	uint64 RowCount() const;
	uint64 ColumnCount() const;
	uint64 RowStride() const;
	uint64 ByteSize() const;
	const void* Data() const;

	bool GetFloat(uint64 Row, uint64 Col, float& OutValue) const;
	bool GetInt32(uint64 Row, uint64 Col, int32& OutValue) const;

	/**
	 * Casts the view's row-major snapshot directly to TArrayView<const TRow> -- "zero-copy"
	 * relative to a per-field marshalling pass, matching the Unity Foundation's DataView<TRow>
	 * "when widths match" contract. Only valid when sizeof(TRow) == RowStride() AND TRow's field
	 * order/widths exactly match this view's column order -- both are the caller's responsibility
	 * to guarantee (there is no reflection-based verification here). Returns an empty view if the
	 * stride doesn't match, rather than silently reinterpreting mismatched data.
	 */
	template <typename TRow>
	TArrayView<const TRow> GetRows() const
	{
		if (sizeof(TRow) != RowStride())
		{
			return TArrayView<const TRow>();
		}

		return TArrayView<const TRow>(reinterpret_cast<const TRow*>(Data()), static_cast<int32>(RowCount()));
	}

private:
	dl_view* View = nullptr;
};

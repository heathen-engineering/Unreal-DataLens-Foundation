/**************************************************************************************
*                                                                                     *
* Copyright   2026 by Heathen Engineering Limited, an Irish registered company        *
* # 556277, VAT IE3394133CH, contact Heathen via support@heathen.group                *
*                                                                                     *
***************************************************************************************/

#include "DataLensView.h"
#include "datalens/c_api.h"

FDataLensView::FDataLensView(const TArray<uint64_t>& SourceColumns)
{
	View = dl_view_create(SourceColumns.GetData(), static_cast<uint64_t>(SourceColumns.Num()));
}

FDataLensView::~FDataLensView()
{
	if (View != nullptr)
	{
		dl_view_destroy(View);
		View = nullptr;
	}
}

FDataLensView::FDataLensView(FDataLensView&& Other) noexcept
	: View(Other.View)
{
	Other.View = nullptr;
}

FDataLensView& FDataLensView::operator=(FDataLensView&& Other) noexcept
{
	if (this != &Other)
	{
		if (View != nullptr)
		{
			dl_view_destroy(View);
		}
		View = Other.View;
		Other.View = nullptr;
	}
	return *this;
}

void FDataLensView::Refresh(dl_store* Store)
{
	if (View != nullptr && Store != nullptr)
	{
		dl_view_refresh(View, Store);
	}
}

uint64 FDataLensView::RowCount() const { return View != nullptr ? dl_view_row_count(View) : 0; }
uint64 FDataLensView::ColumnCount() const { return View != nullptr ? dl_view_column_count(View) : 0; }
uint64 FDataLensView::RowStride() const { return View != nullptr ? dl_view_row_stride(View) : 0; }
uint64 FDataLensView::ByteSize() const { return View != nullptr ? dl_view_byte_size(View) : 0; }
const void* FDataLensView::Data() const { return View != nullptr ? dl_view_data(View) : nullptr; }

bool FDataLensView::GetFloat(uint64 Row, uint64 Col, float& OutValue) const
{
	if (View == nullptr)
	{
		return false;
	}

	float Value = 0.0f;
	if (dl_view_get_f32(View, Row, Col, &Value) == 0)
	{
		return false;
	}

	OutValue = Value;
	return true;
}

bool FDataLensView::GetInt32(uint64 Row, uint64 Col, int32& OutValue) const
{
	if (View == nullptr)
	{
		return false;
	}

	int32_t Value = 0;
	if (dl_view_get_i32(View, Row, Col, &Value) == 0)
	{
		return false;
	}

	OutValue = Value;
	return true;
}

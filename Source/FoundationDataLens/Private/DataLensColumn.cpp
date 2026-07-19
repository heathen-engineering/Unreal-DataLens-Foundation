/**************************************************************************************
*                                                                                     *
* Copyright   2026 by Heathen Engineering Limited, an Irish registered company        *
* # 556277, VAT IE3394133CH, contact Heathen via support@heathen.group                *
*                                                                                     *
***************************************************************************************/

#include "DataLensColumn.h"
#include "Hash/xxhash.h"

uint64 HashDataLensTag(const FGameplayTag& GameplayTag)
{
	const FString TagString = GameplayTag.ToString();
	const FTCHARToUTF8 Utf8Tag(*TagString);
	return FXxHash64::HashBuffer(Utf8Tag.Get(), Utf8Tag.Length()).Hash;
}

FDataLensColumn FDataLensColumn::OfFloat(uint64 ColumnId)
{
	FDataLensColumn Column;
	Column.Id = ColumnId;
	Column.Type = EDataLensColumnType::Float;
	Column.Stride = sizeof(float);
	return Column;
}

FDataLensColumn FDataLensColumn::OfInt32(uint64 ColumnId)
{
	FDataLensColumn Column;
	Column.Id = ColumnId;
	Column.Type = EDataLensColumnType::Int32;
	Column.Stride = sizeof(int32);
	return Column;
}

FDataLensColumn FDataLensColumn::OfFloat(const FGameplayTag& GameplayTag)
{
	return OfFloat(HashDataLensTag(GameplayTag));
}

FDataLensColumn FDataLensColumn::OfInt32(const FGameplayTag& GameplayTag)
{
	return OfInt32(HashDataLensTag(GameplayTag));
}

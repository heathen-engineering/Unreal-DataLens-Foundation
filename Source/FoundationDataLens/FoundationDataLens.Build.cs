/**************************************************************************************
*                                                                                     *
* Copyright   2026 by Heathen Engineering Limited, an Irish registered company        *
* # 556277, VAT IE3394133CH, contact Heathen via support@heathen.group                *
*                                                                                     *
***************************************************************************************/

using UnrealBuildTool;

public class FoundationDataLens : ModuleRules
{
	public FoundationDataLens(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// DataLens Core (extern/DataLens/Core) is compiled directly into this module rather than
		// linked as a prebuilt binary -- UBT already cross-compiles C++ per platform, so there's
		// no need for the vendored-.so/.dll step the managed engines (Unity) require. Core's own
		// sources are mirrored into Private/ThirdParty/DataLensCore/ (see that folder's README)
		// so UBT's module-source discovery picks them up as ordinary module files.
		PublicIncludePaths.AddRange(
			new string[]
			{
				"$(PluginDir)/extern/DataLens/Core/include",
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				// GameplayTagContainer.h is included from this module's own Public headers
				// (DataLensColumn.h), so this needs to be Public, not Private.
				"GameplayTags",
			}
		);

		// DataLens Core's only external dependency is std::thread / a thread pool -- no extra
		// UBT dependency needed, every UE module already links pthreads (Linux/Mac) or the Win32
		// threading API.
		bEnableExceptions = true;
	}
}

## Camera Capture by x264 encoding on Unreal Engine 5.4

# *.Build.cs files
 
    ...
	PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Sockets", "Networking" });

	PrivateDependencyModuleNames.AddRange(new string[] {  });

    PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "ThirdParty/x264/include"));

    PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "ThirdParty/x264/release-x64", "libx264.lib"));
    ...


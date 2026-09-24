// Copyright Epic Games, Inc. All Rights Reserved.

#include "RenderLabRendering.h"

#define LOCTEXT_NAMESPACE "FRenderLabRenderingModule"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

void FRenderLabRenderingModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("RenderLabRendering"));
	check(Plugin.IsValid());
	
	const FString ShaderDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders"));
	check(FPaths::DirectoryExists(ShaderDir));
	
	// 建立虚拟路径映射
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/RenderLabRendering"), ShaderDir);
	UE_LOG(LogTemp, Display, TEXT("[RenderLabRendering] ShaderDir: %s"), *ShaderDir);
}

void FRenderLabRenderingModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FRenderLabRenderingModule, RenderLabRendering)
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "Framework/Commands/Commands.h"
#include "Modules/ModuleManager.h"
#include "HAL/PlatformFileManager.h"

class UStaticMeshComponent;

class SocSceneWriter {
public:
  SocSceneWriter(const FString &Name);
  ~SocSceneWriter();

  void WriteView(const FMatrix &View);
  void WriteViewProj(const FMatrix &ViewProj);
  void WriteObjectNum(uint32 Num);
  void Write(const FMatrix &Local2World, TArray<FVector> Vertices);

private:
  IFileHandle *File;
};

class FSocSceneExporterModule : public IModuleInterface {
public:
  virtual void StartupModule() override;
  virtual void ShutdownModule() override;

  TSharedPtr<FUICommandList> CommandList;

  void OnExportScene();
  void OnExportStaticMeshComponent(UStaticMeshComponent *Component,
                                   SocSceneWriter &Writer);
};

// Copyright Epic Games, Inc. All Rights Reserved.

#include "SocSceneExporter.h"
#include "Components/StaticMeshComponent.h"
#include "EditorStyleSet.h"
#include "Engine/StaticMesh.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "LevelEditor.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FSocSceneExporterModule"

class FExportSocSceneCommands : public TCommands<FExportSocSceneCommands> {
public:
  /** Constructor */
  FExportSocSceneCommands()
      : TCommands<FExportSocSceneCommands>(
            "ExportSocSceneCommands",
            NSLOCTEXT("Contexts", "SocSceneExporter", "SocSceneExporter"),
            NAME_None,
            FEditorStyle::GetStyleSetName()) // Icon Style Set
  {}

  TSharedPtr<FUICommandInfo> ExportSocScene;

  virtual void RegisterCommands() override;
};

void FExportSocSceneCommands::RegisterCommands() {
  UI_COMMAND(ExportSocScene, "Export to SocScene", "Export to SocScene",
             EUserInterfaceActionType::Button, FInputGesture());
}

void FSocSceneExporterModule::StartupModule() {
  FExportSocSceneCommands::Register();
  TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
  CommandList = MakeShareable(new FUICommandList);
  const FExportSocSceneCommands &Commands = FExportSocSceneCommands::Get();
  struct Local {
    static void AddToolbarButton(FToolBarBuilder &ToolbarBuilder) {
      ToolbarBuilder.AddToolBarButton(
          FExportSocSceneCommands::Get().ExportSocScene, NAME_None,
          FText::FromString("   SocExp   "), FText::FromString("SocExporter"),
          FSlateIcon("SubstanceSourceStyle", "SubstanceSourceButtonIcon"));
    }
  };
  ToolbarExtender->AddToolBarExtension(
      "Content", EExtensionHook::After, CommandList,
      FToolBarExtensionDelegate::CreateStatic(&Local::AddToolbarButton));
  CommandList->MapAction(
      Commands.ExportSocScene,
      FExecuteAction::CreateRaw(this, &FSocSceneExporterModule::OnExportScene),
      FCanExecuteAction());
  FLevelEditorModule &LevelEditorModule =
      FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
  LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(
      ToolbarExtender);
}

void FSocSceneExporterModule::ShutdownModule() {}

UWorld *GetEditorWorld() {
  return GEditor ? GEditor->GetEditorWorldContext(false).World() : nullptr;
}

void FSocSceneExporterModule::OnExportScene() {
  UWorld *World = GetEditorWorld();
  SocSceneWriter Writer(TEXT("testScene.bin"));
  Writer.WriteView(FMatrix());
  Writer.WriteViewProj(FMatrix());
  uint32 ObjectCount = 0;
  TArray<UStaticMeshComponent*> Components;
  for (auto Level : World->GetLevels()) {
    for (auto It(Level->Actors.CreateIterator()); It; ++It) {
      if (*It) {
        TInlineComponentArray<UStaticMeshComponent *> ComponentArray;
        (*It)->GetComponents<UStaticMeshComponent>(ComponentArray);
        for (auto &Comp : ComponentArray) {
          if (Comp && Comp->Mobility == EComponentMobility::Static) {
            ObjectCount++;
            Components.Add(Comp);
          }
        } // end component array
      }
    } // end actors iter
  }
  Writer.WriteObjectNum(ObjectCount);
  for (auto Comp : Components) {
    OnExportStaticMeshComponent(Comp, Writer); 
  }
}

void FSocSceneExporterModule::OnExportStaticMeshComponent(
    UStaticMeshComponent *Component, SocSceneWriter &Writer) {
  UStaticMesh *StaticMesh = Component->GetStaticMesh();
  auto &LOD = StaticMesh->GetLODForExport(0);
  TArray<uint32> Indices;
  LOD.IndexBuffer.GetCopy(Indices);
  TArray<FVector> FlattenVertices;
  FlattenVertices.Reserve(Indices.Num());
  for (int TriangleId = 0; TriangleId < Indices.Num() / 3; TriangleId++) {
    uint32 VId0 = Indices[TriangleId * 3];
    uint32 VId1 = Indices[TriangleId * 3 + 1];
    uint32 VId2 = Indices[TriangleId * 3 + 2];
    FlattenVertices.Add(
        LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(VId0));
    FlattenVertices.Add(
        LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(VId1));
    FlattenVertices.Add(
        LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(VId2));
  }
  FMatrix Local2World = Component->GetComponentTransform().ToMatrixWithScale();
  Writer.Write(Local2World, MoveTemp(FlattenVertices));
}

SocSceneWriter::SocSceneWriter(const FString &Name) : File(nullptr) {
  FString Path = FPaths::ProjectSavedDir() / Name;
  File = FPlatformFileManager::Get().GetPlatformFile().OpenWrite(*Path);
}

SocSceneWriter::~SocSceneWriter() {
  if (File) {
    delete File;
    File = nullptr;
  }
}

void SocSceneWriter::WriteView(const FMatrix &View) {
  check(File);
  File->Write((const uint8 *)&View, sizeof(View));
}

void SocSceneWriter::WriteViewProj(const FMatrix &ViewProj) {
  check(File);
  File->Write((const uint8 *)&ViewProj, sizeof(ViewProj));
}

void SocSceneWriter::WriteObjectNum(uint32 Num) {
  check(File);
  File->Write((const uint8 *)&Num, sizeof(Num));
}

void SocSceneWriter::Write(const FMatrix &Local2World,
                           TArray<FVector> Vertices) {
  check(File);
  uint32 NumTris = Vertices.Num() / 3;
  File->Write((const uint8 *)&NumTris, sizeof(NumTris));
  File->Write((const uint8 *)Vertices.GetData(),
              sizeof(FVector) * Vertices.Num());
  File->Write((const uint8 *)&Local2World, sizeof(Local2World));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSocSceneExporterModule, SocSceneExporter)
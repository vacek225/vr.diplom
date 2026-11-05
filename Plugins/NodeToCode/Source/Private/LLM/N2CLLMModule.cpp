// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "LLM/N2CLLMModule.h"

#include "Core/N2CNodeTranslator.h"
#include "Core/N2CSerializer.h"
#include "Core/N2CSettings.h"
#include "LLM/N2CSystemPromptManager.h"
#include "LLM/N2CBaseLLMService.h"
#include "LLM/N2CLLMProviderRegistry.h"
#include "LLM/Providers/N2CAnthropicService.h"
#include "LLM/Providers/N2CDeepSeekService.h"
#include "LLM/Providers/N2CGeminiService.h"
#include "LLM/Providers/N2COpenAIService.h"
#include "LLM/Providers/N2COllamaService.h"
#include "Utils/N2CLogger.h"

UN2CLLMModule* UN2CLLMModule::Get()
{
    static UN2CLLMModule* Instance = nullptr;
    if (!Instance)
    {
        Instance = NewObject<UN2CLLMModule>();
        Instance->AddToRoot(); // Prevent garbage collection
        Instance->CurrentStatus = EN2CSystemStatus::Idle;
        Instance->LatestTranslationPath = TEXT("");
    }
    return Instance;
}

EN2CSystemStatus UN2CLLMModule::GetSystemStatus() const
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
    // UE 5.7 can crash when the Editor Utility Widget status overlay enters
    // non-idle states that drive a BackgroundBlur/GaussianBlur render path.
    if (CurrentStatus == EN2CSystemStatus::Processing ||
        CurrentStatus == EN2CSystemStatus::Error)
    {
        return EN2CSystemStatus::Idle;
    }
#endif

    return CurrentStatus;
}

bool UN2CLLMModule::Initialize()
{
    CurrentStatus = EN2CSystemStatus::Initializing;
    
    // Load settings
    const UN2CSettings* Settings = GetDefault<UN2CSettings>();
    if (!Settings)
    {
        CurrentStatus = EN2CSystemStatus::Error;
        FN2CLogger::Get().LogError(TEXT("Failed to load plugin settings"), TEXT("LLMModule"));
        return false;
    }

    // Create config from settings
    Config.Provider = Settings->Provider;
    Config.ApiKey = Settings->GetActiveApiKey();
    Config.Model = Settings->GetActiveModel();

    // Initialize provider registry
    InitializeProviderRegistry();

    // Initialize components
    if (!InitializeComponents() || !CreateServiceForProvider(Config.Provider))
    {
        CurrentStatus = EN2CSystemStatus::Error;
        return false;
    }

    bIsInitialized = true;
    CurrentStatus = EN2CSystemStatus::Idle;
    FN2CLogger::Get().Log(TEXT("LLM Module initialized successfully"), EN2CLogSeverity::Info, TEXT("LLMModule"));
    return true;
}

void UN2CLLMModule::ProcessN2CJson(
    const FString& JsonInput,
    const FOnLLMResponseReceived& OnComplete)
{
    if (bTranslationRequestInFlight)
    {
        FN2CLogger::Get().LogWarning(TEXT("Translation request already in progress"), TEXT("LLMModule"));
        OnComplete.ExecuteIfBound(TEXT("{\"error\": \"Translation already in progress\"}"));
        return;
    }

    if (!bIsInitialized)
    {
        CurrentStatus = EN2CSystemStatus::Error;
        FN2CLogger::Get().LogError(TEXT("LLM Module not initialized"), TEXT("LLMModule"));
        const bool bExecuted = OnComplete.ExecuteIfBound(TEXT("{\"error\": \"Module not initialized\"}"));
        return;
    }

    CurrentStatus = EN2CSystemStatus::Processing;

    bTranslationRequestInFlight = true;

#if !(ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
    // Broadcast that request is being sent
    OnTranslationRequestSent.Broadcast();
#else
    FN2CLogger::Get().Log(
        TEXT("Skipping processing-status UI broadcast on UE 5.7 to avoid a Slate GaussianBlur crash"),
        EN2CLogSeverity::Warning,
        TEXT("LLMModule"));
#endif

    if (!ActiveService.GetInterface())
    {
        bTranslationRequestInFlight = false;
        FN2CLogger::Get().LogError(TEXT("No active LLM service"), TEXT("LLMModule"));
        const bool bExecuted = OnComplete.ExecuteIfBound(TEXT("{\"error\": \"No active service\"}"));
        return;
    }

    // Get active service
    TScriptInterface<IN2CLLMService> Service = GetActiveService();
    if (!Service.GetInterface())
    {
        bTranslationRequestInFlight = false;
        FN2CLogger::Get().LogError(TEXT("No active service"), TEXT("LLMModule"));
        return;
    }

    // Check if service supports system prompts
    bool bSupportsSystemPrompts = false;
    FString Endpoint, AuthToken;
    Service->GetConfiguration(Endpoint, AuthToken, bSupportsSystemPrompts);

    // Get system prompt with language specification
    const UN2CSettings* Settings = GetDefault<UN2CSettings>();
    FString SystemPrompt = PromptManager->GetLanguageSpecificPrompt(
        TEXT("CodeGen"),
        Settings ? Settings->TargetLanguage : EN2CCodeLanguage::Cpp
    );

    // Connect the HTTP handler's translation response delegate to our module's delegate
    if (HttpHandler)
    {
        HttpHandler->OnTranslationResponseReceived = OnTranslationResponseReceived;
    }

    // Send request through service
    ActiveService->SendRequest(JsonInput, SystemPrompt, FOnLLMResponseReceived::CreateLambda(
        [this](const FString& Response)
        {
            bTranslationRequestInFlight = false;

            // Create translation response struct
            FN2CTranslationResponse TranslationResponse;
            
            // Get active service's response parser
            TScriptInterface<IN2CLLMService> ActiveServiceParser = GetActiveService();
            if (ActiveServiceParser.GetInterface())
            {
                UN2CResponseParserBase* Parser = ActiveServiceParser->GetResponseParser();
                if (Parser)
                {
                    if (Parser->ParseLLMResponse(Response, TranslationResponse))
                    {
                        CurrentStatus = EN2CSystemStatus::Idle;
                            
                        // Save translation to disk
                        const FN2CBlueprint& Blueprint = FN2CNodeTranslator::Get().GetN2CBlueprint();
                        if (SaveTranslationToDisk(TranslationResponse, Blueprint))
                        {
                            FN2CLogger::Get().Log(TEXT("Successfully saved translation to disk"), EN2CLogSeverity::Info);
                        }
                            
                        OnTranslationResponseReceived.Broadcast(TranslationResponse, true);
                        FN2CLogger::Get().Log(TEXT("Successfully parsed LLM response"), EN2CLogSeverity::Info);
                    }
                    else
                    {
                        CurrentStatus = EN2CSystemStatus::Error;
                        FN2CLogger::Get().LogError(TEXT("Failed to parse LLM response"));
                        OnTranslationResponseReceived.Broadcast(TranslationResponse, false);
                    }
                }
                else
                {
                    CurrentStatus = EN2CSystemStatus::Error;
                    FN2CLogger::Get().LogError(TEXT("No response parser available"));
                    OnTranslationResponseReceived.Broadcast(TranslationResponse, false);
                }
            }
            else
            {
                CurrentStatus = EN2CSystemStatus::Error;
                FN2CLogger::Get().LogError(TEXT("No active LLM service"));
                OnTranslationResponseReceived.Broadcast(TranslationResponse, false);
            }
        }));
}

bool UN2CLLMModule::InitializeComponents()
{
    // Create and initialize prompt manager
    PromptManager = NewObject<UN2CSystemPromptManager>(this);
    if (!PromptManager)
    {
        FN2CLogger::Get().LogError(TEXT("Failed to create prompt manager"), TEXT("LLMModule"));
        return false;
    }
    PromptManager->Initialize(Config);

    // Note: HTTP Handler and Response Parser will be created by the specific service
    return true;
}

void UN2CLLMModule::OpenTranslationFolder(bool& Success)
{
    FString PathToOpen = LatestTranslationPath;
    
    if (PathToOpen.IsEmpty())
    {
        FN2CLogger::Get().LogWarning(TEXT("No translation path available, opening the base path"));
        Success = true;
        PathToOpen = GetTranslationBasePath();
    }

    if (!FPaths::DirectoryExists(PathToOpen))
    {
        FN2CLogger::Get().LogError(FString::Printf(TEXT("Translation directory does not exist: %s \n\nOpening the base path"), *PathToOpen));
        Success = true;
        PathToOpen = GetTranslationBasePath();
    }

#if PLATFORM_WINDOWS
    FPlatformProcess::ExploreFolder(*PathToOpen);
    Success = true;
#endif
#if PLATFORM_MAC
    FMacPlatformProcess::ExploreFolder(*PathToOpen);
    Success = true;
#endif
    
}

bool UN2CLLMModule::SaveTranslationToDisk(const FN2CTranslationResponse& Response, const FN2CBlueprint& Blueprint)
{
    // Get blueprint name from metadata
    FString BlueprintName = Blueprint.Metadata.Name;
    if (BlueprintName.IsEmpty())
    {
        BlueprintName = TEXT("UnknownBlueprint");
    }
    
    // Generate root path for this translation
    FString RootPath = GenerateTranslationRootPath(BlueprintName);
    
    // Ensure the directory exists
    if (!EnsureDirectoryExists(RootPath))
    {
        FN2CLogger::Get().LogError(FString::Printf(TEXT("Failed to create translation directory: %s"), *RootPath));
        return false;
    }
    
    // Store the path for later reference
    LatestTranslationPath = RootPath;
    
    // Save the Blueprint JSON (pretty-printed)
    FString JsonFileName = FString::Printf(TEXT("N2C_BP_%s.json"), *FPaths::GetBaseFilename(RootPath));
    FString JsonFilePath = FPaths::Combine(RootPath, JsonFileName);
    
    // Serialize the Blueprint to JSON with pretty printing
    FN2CSerializer::SetPrettyPrint(true);
    FString JsonContent = FN2CSerializer::ToJson(Blueprint);
    
    if (!FFileHelper::SaveStringToFile(JsonContent, *JsonFilePath))
    {
        FN2CLogger::Get().LogError(FString::Printf(TEXT("Failed to save JSON file: %s"), *JsonFilePath));
        return false;
    }
    
    // Save minified version of the Blueprint JSON
    FString MinifiedJsonFileName = FString::Printf(TEXT("N2C_BP_Minified_%s.json"), *FPaths::GetBaseFilename(RootPath));
    FString MinifiedJsonFilePath = FPaths::Combine(RootPath, MinifiedJsonFileName);
    
    // Serialize the Blueprint to JSON without pretty printing
    FN2CSerializer::SetPrettyPrint(false);
    FString MinifiedJsonContent = FN2CSerializer::ToJson(Blueprint);
    
    if (!FFileHelper::SaveStringToFile(MinifiedJsonContent, *MinifiedJsonFilePath))
    {
        FN2CLogger::Get().LogWarning(FString::Printf(TEXT("Failed to save minified JSON file: %s"), *MinifiedJsonFilePath));
        // Continue even if minified version fails
    }
    
    // Save the raw LLM translation response JSON
    FString TranslationJsonFileName = FString::Printf(TEXT("N2C_Translation_%s.json"), *FPaths::GetBaseFilename(RootPath));
    FString TranslationJsonFilePath = FPaths::Combine(RootPath, TranslationJsonFileName);
    
    // Serialize the Translation response to JSON
    TSharedPtr<FJsonObject> TranslationJsonObject = MakeShared<FJsonObject>();
    
    // Create graphs array
    TArray<TSharedPtr<FJsonValue>> GraphsArray;
    for (const FN2CGraphTranslation& Graph : Response.Graphs)
    {
        TSharedPtr<FJsonObject> GraphObject = MakeShared<FJsonObject>();
        GraphObject->SetStringField(TEXT("graph_name"), Graph.GraphName);
        GraphObject->SetStringField(TEXT("graph_type"), Graph.GraphType);
        GraphObject->SetStringField(TEXT("graph_class"), Graph.GraphClass);
        
        // Create code object
        TSharedPtr<FJsonObject> CodeObject = MakeShared<FJsonObject>();
        CodeObject->SetStringField(TEXT("graphDeclaration"), Graph.Code.GraphDeclaration);
        CodeObject->SetStringField(TEXT("graphImplementation"), Graph.Code.GraphImplementation);
        CodeObject->SetStringField(TEXT("implementationNotes"), Graph.Code.ImplementationNotes);
        
        GraphObject->SetObjectField(TEXT("code"), CodeObject);
        GraphsArray.Add(MakeShared<FJsonValueObject>(GraphObject));
    }
    
    TranslationJsonObject->SetArrayField(TEXT("graphs"), GraphsArray);
    
    // Add usage information if available
    if (Response.Usage.InputTokens > 0 || Response.Usage.OutputTokens > 0)
    {
        TSharedPtr<FJsonObject> UsageObject = MakeShared<FJsonObject>();
        UsageObject->SetNumberField(TEXT("input_tokens"), Response.Usage.InputTokens);
        UsageObject->SetNumberField(TEXT("output_tokens"), Response.Usage.OutputTokens);
        TranslationJsonObject->SetObjectField(TEXT("usage"), UsageObject);
    }
    
    // Serialize to string with pretty printing
    FString TranslationJsonContent;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&TranslationJsonContent);
    FJsonSerializer::Serialize(TranslationJsonObject.ToSharedRef(), Writer);
    
    if (!FFileHelper::SaveStringToFile(TranslationJsonContent, *TranslationJsonFilePath))
    {
        FN2CLogger::Get().LogWarning(FString::Printf(TEXT("Failed to save translation JSON file: %s"), *TranslationJsonFilePath));
        // Continue even if translation JSON fails
    }
    
    // Get the target language from settings
    const UN2CSettings* Settings = GetDefault<UN2CSettings>();
    EN2CCodeLanguage TargetLanguage = Settings ? Settings->TargetLanguage : EN2CCodeLanguage::Cpp;
    
    // Save each graph's files
    for (const FN2CGraphTranslation& Graph : Response.Graphs)
    {
        // Skip graphs with empty names
        if (Graph.GraphName.IsEmpty())
        {
            continue;
        }
        
        // Create directory for this graph
        FString GraphDir = FPaths::Combine(RootPath, Graph.GraphName);
        if (!EnsureDirectoryExists(GraphDir))
        {
            FN2CLogger::Get().LogWarning(FString::Printf(TEXT("Failed to create graph directory: %s"), *GraphDir));
            continue;
        }
        
        // Save declaration file (C++ only)
        if (TargetLanguage == EN2CCodeLanguage::Cpp && !Graph.Code.GraphDeclaration.IsEmpty())
        {
            FString HeaderPath = FPaths::Combine(GraphDir, Graph.GraphName + TEXT(".h"));
            if (!FFileHelper::SaveStringToFile(Graph.Code.GraphDeclaration, *HeaderPath))
            {
                FN2CLogger::Get().LogWarning(FString::Printf(TEXT("Failed to save header file: %s"), *HeaderPath));
            }
        }
        
        // Save implementation file with appropriate extension
        if (!Graph.Code.GraphImplementation.IsEmpty())
        {
            FString Extension = GetFileExtensionForLanguage(TargetLanguage);
            FString ImplPath = FPaths::Combine(GraphDir, Graph.GraphName + Extension);
            if (!FFileHelper::SaveStringToFile(Graph.Code.GraphImplementation, *ImplPath))
            {
                FN2CLogger::Get().LogWarning(FString::Printf(TEXT("Failed to save implementation file: %s"), *ImplPath));
            }
        }
        
        // Save implementation notes
        if (!Graph.Code.ImplementationNotes.IsEmpty())
        {
            FString NotesPath = FPaths::Combine(GraphDir, Graph.GraphName + TEXT("_Notes.txt"));
            if (!FFileHelper::SaveStringToFile(Graph.Code.ImplementationNotes, *NotesPath))
            {
                FN2CLogger::Get().LogWarning(FString::Printf(TEXT("Failed to save notes file: %s"), *NotesPath));
            }
        }
    }
    
    FN2CLogger::Get().Log(FString::Printf(TEXT("Translation saved to: %s"), *RootPath), EN2CLogSeverity::Info);
    return true;
}

bool UN2CLLMModule::SaveLocalHandoffToDisk(const FN2CBlueprint& Blueprint)
{
    FString BlueprintName = Blueprint.Metadata.Name;
    if (BlueprintName.IsEmpty())
    {
        BlueprintName = TEXT("UnknownBlueprint");
    }

    const FString RootPath = GenerateLocalHandoffRootPath(BlueprintName);
    if (!EnsureDirectoryExists(RootPath))
    {
        FN2CLogger::Get().LogError(FString::Printf(TEXT("Failed to create local handoff directory: %s"), *RootPath));
        return false;
    }

    LatestTranslationPath = RootPath;

    const FString FolderName = FPaths::GetBaseFilename(RootPath);

    FN2CSerializer::SetPrettyPrint(true);
    const FString PrettyJsonContent = FN2CSerializer::ToJson(Blueprint);
    const FString PrettyJsonPath = FPaths::Combine(RootPath, FString::Printf(TEXT("N2C_BP_%s.json"), *FolderName));
    if (!FFileHelper::SaveStringToFile(PrettyJsonContent, *PrettyJsonPath))
    {
        FN2CLogger::Get().LogError(FString::Printf(TEXT("Failed to save local handoff JSON file: %s"), *PrettyJsonPath));
        return false;
    }

    FN2CSerializer::SetPrettyPrint(false);
    const FString MinifiedJsonContent = FN2CSerializer::ToJson(Blueprint);
    const FString MinifiedJsonPath = FPaths::Combine(RootPath, FString::Printf(TEXT("N2C_BP_Minified_%s.json"), *FolderName));
    if (!FFileHelper::SaveStringToFile(MinifiedJsonContent, *MinifiedJsonPath))
    {
        FN2CLogger::Get().LogWarning(FString::Printf(TEXT("Failed to save local handoff minified JSON file: %s"), *MinifiedJsonPath));
    }

    const FString HandoffDocumentPath = FPaths::Combine(RootPath, TEXT("N2C_Codex_Handoff.md"));
    const FString HandoffDocument = BuildLocalHandoffDocument(Blueprint, PrettyJsonPath, MinifiedJsonPath);
    if (!FFileHelper::SaveStringToFile(HandoffDocument, *HandoffDocumentPath))
    {
        FN2CLogger::Get().LogError(FString::Printf(TEXT("Failed to save local handoff instructions: %s"), *HandoffDocumentPath));
        return false;
    }

    FN2CLogger::Get().Log(
        FString::Printf(TEXT("Local Codex handoff saved to: %s"), *RootPath),
        EN2CLogSeverity::Info,
        TEXT("LLMModule"));
    return true;
}

FString UN2CLLMModule::GenerateTranslationRootPath(const FString& BlueprintName) const
{
    // Get current date/time
    FDateTime Now = FDateTime::Now();
    FString Timestamp = Now.ToString(TEXT("%Y-%m-%d-%H.%M.%S"));
    
    // Create folder name
    FString FolderName = FString::Printf(TEXT("%s_%s"), *BlueprintName, *Timestamp);

    // Get the saved translations base path
    FString BasePath = GetTranslationBasePath();
    
    return FPaths::Combine(BasePath, FolderName);
}

FString UN2CLLMModule::GenerateLocalHandoffRootPath(const FString& BlueprintName) const
{
    const FDateTime Now = FDateTime::Now();
    const FString Timestamp = Now.ToString(TEXT("%Y-%m-%d-%H.%M.%S"));
    const FString FolderName = FString::Printf(TEXT("%s_%s"), *BlueprintName, *Timestamp);

    return FPaths::Combine(GetLocalHandoffBasePath(), FolderName);
}

FString UN2CLLMModule::GetTranslationBasePath() const
{
    // Check if custom output directory is set in settings
    const UN2CSettings* Settings = GetDefault<UN2CSettings>();
    FString BasePath;
    
    if (Settings && !Settings->CustomTranslationOutputDirectory.Path.IsEmpty())
    {
        // Use custom path if specified
        BasePath = Settings->CustomTranslationOutputDirectory.Path;
        FN2CLogger::Get().Log(
            FString::Printf(TEXT("Using custom translation output directory: %s"), *BasePath),
            EN2CLogSeverity::Info);
    }
    else
    {
        // Use default path
        BasePath = FPaths::ProjectSavedDir() / TEXT("NodeToCode") / TEXT("Translations");
    }

    return BasePath;
}

FString UN2CLLMModule::GetLocalHandoffBasePath() const
{
    const UN2CSettings* Settings = GetDefault<UN2CSettings>();

    if (Settings && !Settings->CustomTranslationOutputDirectory.Path.IsEmpty())
    {
        return FPaths::Combine(Settings->CustomTranslationOutputDirectory.Path, TEXT("LocalHandoffs"));
    }

    return FPaths::ProjectSavedDir() / TEXT("NodeToCode") / TEXT("LocalHandoffs");
}

FString UN2CLLMModule::GetFileExtensionForLanguage(EN2CCodeLanguage Language) const
{
    switch (Language)
    {
        case EN2CCodeLanguage::Cpp:
            return TEXT(".cpp");
        case EN2CCodeLanguage::Python:
            return TEXT(".py");
        case EN2CCodeLanguage::JavaScript:
            return TEXT(".js");
        case EN2CCodeLanguage::CSharp:
            return TEXT(".cs");
        case EN2CCodeLanguage::Swift:
            return TEXT(".swift");
        case EN2CCodeLanguage::Pseudocode:
            return TEXT(".md");
        default:
            return TEXT(".txt");
    }
}

FString UN2CLLMModule::BuildLocalHandoffDocument(
    const FN2CBlueprint& Blueprint,
    const FString& PrettyJsonPath,
    const FString& MinifiedJsonPath) const
{
    const UN2CSettings* Settings = GetDefault<UN2CSettings>();
    const EN2CCodeLanguage TargetLanguage = Settings ? Settings->TargetLanguage : EN2CCodeLanguage::Cpp;

    TArray<FString> GraphNames;
    GraphNames.Reserve(Blueprint.Graphs.Num());
    for (const FN2CGraph& Graph : Blueprint.Graphs)
    {
        if (!Graph.Name.IsEmpty())
        {
            GraphNames.Add(Graph.Name);
        }
    }

    const FString GraphList = GraphNames.Num() > 0
        ? FString::Join(GraphNames, TEXT(", "))
        : TEXT("No named graphs were detected.");

    FString TargetLanguageName = TEXT("Cpp");
    if (const UEnum* LanguageEnum = StaticEnum<EN2CCodeLanguage>())
    {
        TargetLanguageName = LanguageEnum->GetNameStringByValue(static_cast<int64>(TargetLanguage));
    }

    const FString BlueprintClass = Blueprint.Metadata.BlueprintClass.IsEmpty()
        ? TEXT("Unknown")
        : Blueprint.Metadata.BlueprintClass;

    return FString::Printf(
        TEXT("# Node to Code Local Codex Handoff\n\n")
        TEXT("This package was created locally by the Node to Code plugin.\n")
        TEXT("No cloud LLM request was sent for this export.\n\n")
        TEXT("## Blueprint Summary\n")
        TEXT("- Blueprint: `%s`\n")
        TEXT("- Blueprint Class: `%s`\n")
        TEXT("- Target Language: `%s`\n")
        TEXT("- Graph Count: `%d`\n")
        TEXT("- Graphs: %s\n\n")
        TEXT("## Saved Artifacts\n")
        TEXT("- Pretty JSON: `%s`\n")
        TEXT("- Minified JSON: `%s`\n\n")
        TEXT("## Recommended Codex Prompt\n")
        TEXT("Use the Blueprint JSON from this handoff package and conservatively translate it into Unreal Engine 5.7-ready %s.\n")
        TEXT("Preserve the Blueprint logic, keep changes minimal, and prefer production-safe UE plugin/project conventions.\n")
        TEXT("If multiple graphs exist, explain how they map into classes, methods, or helper functions before writing code.\n\n")
        TEXT("## Suggested Next Step\n")
        TEXT("In Codex, ask: `Read the latest NodeToCode local handoff for %s and implement the corresponding UE 5.7 %s code.`\n"),
        *Blueprint.Metadata.Name,
        *BlueprintClass,
        *TargetLanguageName,
        Blueprint.Graphs.Num(),
        *GraphList,
        *PrettyJsonPath,
        *MinifiedJsonPath,
        *TargetLanguageName,
        *Blueprint.Metadata.Name,
        *TargetLanguageName);
}

bool UN2CLLMModule::EnsureDirectoryExists(const FString& DirectoryPath) const
{
    if (!FPaths::DirectoryExists(DirectoryPath))
    {
        bool bSuccess = FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*DirectoryPath);
        if (!bSuccess)
        {
            FN2CLogger::Get().LogError(
                FString::Printf(TEXT("Failed to create directory: %s"), *DirectoryPath));
            return false;
        }
        FN2CLogger::Get().Log(
            FString::Printf(TEXT("Created directory: %s"), *DirectoryPath),
            EN2CLogSeverity::Info);
        return true;
    }
    return true;
}

bool UN2CLLMModule::CreateServiceForProvider(EN2CLLMProvider Provider)
{
    // Get the provider registry
    UN2CLLMProviderRegistry* Registry = UN2CLLMProviderRegistry::Get();
    
    // Check if the provider is registered
    if (!Registry->IsProviderRegistered(Provider))
    {
        FN2CLogger::Get().LogError(
            FString::Printf(TEXT("Provider type not registered: %s"), 
                *UEnum::GetValueAsString(Provider)),
            TEXT("LLMModule")
        );
        return false;
    }
    
    // Create the provider service
    TScriptInterface<IN2CLLMService> ServiceInterface = Registry->CreateProvider(Provider, this);
    
    if (!ServiceInterface.GetInterface())
    {
        FN2CLogger::Get().LogError(
            FString::Printf(TEXT("Failed to create service for provider type: %s"), 
                *UEnum::GetValueAsString(Provider)),
            TEXT("LLMModule")
        );
        return false;
    }

    // Initialize service
    if (!ServiceInterface.GetInterface()->Initialize(Config))
    {
        FN2CLogger::Get().LogError(TEXT("Failed to initialize service"), TEXT("LLMModule"));
        return false;
    }

    // Store active service
    ActiveService = ServiceInterface;
    return true;
}
void UN2CLLMModule::InitializeProviderRegistry()
{
    // Get the provider registry
    UN2CLLMProviderRegistry* Registry = UN2CLLMProviderRegistry::Get();
    
    // Register all provider classes
    Registry->RegisterProvider(EN2CLLMProvider::OpenAI, UN2COpenAIService::StaticClass());
    Registry->RegisterProvider(EN2CLLMProvider::Anthropic, UN2CAnthropicService::StaticClass());
    Registry->RegisterProvider(EN2CLLMProvider::Gemini, UN2CGeminiService::StaticClass());
    Registry->RegisterProvider(EN2CLLMProvider::DeepSeek, UN2CDeepSeekService::StaticClass());
    Registry->RegisterProvider(EN2CLLMProvider::Ollama, UN2COllamaService::StaticClass());
    
    FN2CLogger::Get().Log(TEXT("Provider registry initialized"), EN2CLogSeverity::Info, TEXT("LLMModule"));
}

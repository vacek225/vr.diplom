#pragma once

#include "Modules/ModuleManager.h"

class FKemSUinVRModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};

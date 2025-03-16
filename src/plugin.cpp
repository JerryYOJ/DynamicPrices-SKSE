#include <spdlog/sinks/basic_file_sink.h>

#include "hooks/hooks.h"
#include "scaleform/scaleform.h" // Uses classes from hooks.h

void InitializeLog() {
    auto logsFolder = SKSE::log::log_directory();
    if (!logsFolder) SKSE::stl::report_and_fail("SKSE log_directory not provided, logs disabled.");
    auto pluginName = SKSE::PluginDeclaration::GetSingleton()->GetName();
    auto logFilePath = *logsFolder / std::format("{}.log", pluginName);
    auto fileLoggerPtr = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath.string(), true);
    auto loggerPtr = std::make_shared<spdlog::logger>("log", std::move(fileLoggerPtr));
    spdlog::set_default_logger(std::move(loggerPtr));
    spdlog::set_level(spdlog::level::debug);
    spdlog::flush_on(spdlog::level::debug);
}

SKSEPluginLoad(const SKSE::LoadInterface *skse) {
    SKSE::Init(skse);

    InitializeLog();
    SKSE::AllocTrampoline(1 << 7);

    Hooks::Install();
    Scaleform::Install();

    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message) {
        if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            
            Hooks::InstallLate();
            Scaleform::InstallLate();

        }
        else if (message->type == SKSE::MessagingInterface::kPostLoad) {
            
        }
        else if (message->type == SKSE::MessagingInterface::kPostLoadGame) {
            
        }
        });

    return true;
}
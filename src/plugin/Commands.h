/**
 * @file Commands.h
 * @brief Command Declarations for Batch Processing Plugin
 */

#ifndef COMMANDS_H
#define COMMANDS_H

namespace BatchProcessing {
namespace Commands {

// Command registration
void registerCommands();
void unregisterCommands();

// Command implementations
void batchProcessCommand();

} // namespace Commands
} // namespace BatchProcessing

// Helper functions
bool isPluginLoaded();
class QApplication* getQApplication();

#endif // COMMANDS_H

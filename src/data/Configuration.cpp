#include "Configuration.h"
#include "ProcessingOptions.h"
#include <QStandardPaths>
#include <QDir>

void PluginConfiguration::load() {
    QSettings settings("BricsCAD", "BatchProcessingPlugin");
    
    defaultFolder = settings.value("defaultFolder", 
                                 QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();
    createBackup = settings.value("createBackup", true).toBool();
    rememberLastSettings = settings.value("rememberLastSettings", true).toBool();
    
    // TODO: Load lastProcessingOptions from settings
    // Hier werden später alle komplexen Einstellungen geladen
}

void PluginConfiguration::save() const {
    QSettings settings("BricsCAD", "BatchProcessingPlugin");
    
    settings.setValue("defaultFolder", defaultFolder);
    settings.setValue("createBackup", createBackup);
    settings.setValue("rememberLastSettings", rememberLastSettings);
    
    // TODO: Save lastProcessingOptions to settings
    // Hier werden später alle komplexen Einstellungen gespeichert
    
    settings.sync();
}

#ifndef LOGGER_H
#define LOGGER_H

#include <QString>
#include <QDateTime>
#include <QTextStream>
#include <QFile>

/**
 * @brief Einfaches Logging-System
 */
class Logger {
public:
    enum Level {
        Debug,
        Info,
        Warning,
        Error
    };
    
    static void log(Level level, const QString& message);
    static void debug(const QString& message);
    static void info(const QString& message);
    static void warning(const QString& message);
    static void error(const QString& message);
    
    static void setLogFile(const QString& filePath);
    
private:
    static QString s_logFilePath;
    static void writeToFile(const QString& levelStr, const QString& message);
};

#endif // LOGGER_H

/**
 * @file OdsTemplateWriter.h
 * @brief Generischer ODS-Template-Befueller
 *
 * Oeffnet eine ODS-Vorlage (ZIP mit content.xml), erlaubt das Einfuegen
 * von Datenzeilen, und schreibt die modifizierte ODS-Datei.
 *
 * Wiederverwendbar fuer Sensorliste UND IO-Belegungsliste.
 *
 * Abhaengigkeiten: pugixml (XML DOM), QuaZip (ZIP I/O), Qt6
 */

#ifndef ODSTEMPLATEWRITER_H
#define ODSTEMPLATEWRITER_H

#include <QString>
#include <QStringList>
#include <pugixml.hpp>

namespace BatchProcessing {

class OdsTemplateWriter
{
public:
    OdsTemplateWriter() = default;
    ~OdsTemplateWriter() = default;

    /// Oeffnet ODS-Template und bereitet DOM vor
    /// @param templatePath  Pfad zur ODS-Vorlage (wird nicht veraendert)
    /// @param outputPath    Pfad fuer die Ausgabe-ODS
    /// @return true bei Erfolg
    bool openTemplate(const QString& templatePath, const QString& outputPath);

    /// Entfernt eine Zeile per Index (0-basiert) aus der Tabelle
    /// Nuetzlich um die Hinweiszeile zu entfernen
    void removeRow(int rowIndex);

    /// Fuegt eine Datenzeile mit beliebig vielen Zellen hinzu
    /// Zellen erben den Default-Cell-Style der jeweiligen Spalte
    void addRow(const QStringList& cellValues);

    /// Schreibt modifizierte content.xml zurueck ins ODS-ZIP
    /// @return true bei Erfolg
    bool save();

    /// Letzter Fehler
    QString lastError() const { return m_lastError; }

private:
    /// Liest content.xml aus dem ODS-ZIP
    bool readContentXml(const QString& odsPath, QByteArray& xmlData);

    /// Ersetzt content.xml im ODS-ZIP (Rebuild-Strategie)
    bool replaceContentXml(const QByteArray& newXml);

    QString m_outputPath;
    QString m_lastError;
    pugi::xml_document m_doc;
    pugi::xml_node m_table;
};

} // namespace BatchProcessing

#endif // ODSTEMPLATEWRITER_H

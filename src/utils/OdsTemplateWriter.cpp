#include "windows_fix.h"
/**
 * @file OdsTemplateWriter.cpp
 * @brief Implementation - Generischer ODS-Template-Befueller
 *
 * Strategie: ODS ist ein ZIP-Archiv. Wir kopieren das Template,
 * lesen content.xml in einen pugixml DOM, manipulieren ihn,
 * und schreiben das ZIP mit der neuen content.xml zurueck.
 *
 * Da QuaZip kein in-place Replace unterstuetzt, wird ein neues
 * ZIP aufgebaut (alle Eintraege 1:1 kopiert, content.xml ersetzt).
 */

#include "OdsTemplateWriter.h"

#include <QFile>
#include <QDir>
#include <QDebug>

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>
#include <quazip/quazipnewinfo.h>

#include <sstream>

namespace BatchProcessing {

// ============================================================================
// Public API
// ============================================================================

bool OdsTemplateWriter::openTemplate(const QString& templatePath, const QString& outputPath)
{
    m_lastError.clear();
    m_outputPath = outputPath;

    // Ausgabeverzeichnis sicherstellen
    QDir().mkpath(QFileInfo(outputPath).absolutePath());

    // Template nach outputPath kopieren
    if (QFile::exists(outputPath))
        QFile::remove(outputPath);

    if (!QFile::copy(templatePath, outputPath)) {
        m_lastError = QString("Kann Template nicht kopieren: %1 -> %2")
                      .arg(templatePath, outputPath);
        return false;
    }

    // Kopie beschreibbar machen (Template ist evtl. read-only)
    QFile::setPermissions(outputPath,
        QFile::permissions(outputPath) | QFile::WriteOwner | QFile::WriteUser);

    // content.xml aus dem ZIP lesen
    QByteArray xmlData;
    if (!readContentXml(outputPath, xmlData)) {
        return false;
    }

    // In pugixml DOM parsen
    pugi::xml_parse_result result = m_doc.load_buffer(
        xmlData.constData(), xmlData.size());

    if (!result) {
        m_lastError = QString("XML-Parse-Fehler: %1").arg(result.description());
        return false;
    }

    // Tabelle finden (erstes table:table Element)
    // Namespace-Praefixe: pugixml behandelt sie als Teil des Elementnamens
    m_table = m_doc.select_node("//table:table").node();
    if (m_table.empty()) {
        // Fallback: manuell suchen
        pugi::xml_node body = m_doc.child("office:document-content")
                                   .child("office:body")
                                   .child("office:spreadsheet");
        for (pugi::xml_node child = body.first_child(); child; child = child.next_sibling()) {
            if (QString(child.name()) == "table:table") {
                m_table = child;
                break;
            }
        }
    }

    if (m_table.empty()) {
        m_lastError = "Keine table:table in content.xml gefunden";
        return false;
    }

    qDebug() << "[OdsWriter] Template geoeffnet:" << templatePath;
    return true;
}

void OdsTemplateWriter::removeRow(int rowIndex)
{
    if (m_table.empty()) return;

    int idx = 0;
    for (pugi::xml_node row = m_table.first_child(); row; row = row.next_sibling()) {
        if (QString(row.name()) != "table:table-row") continue;

        if (idx == rowIndex) {
            m_table.remove_child(row);
            qDebug() << "[OdsWriter] Zeile" << rowIndex << "entfernt";
            return;
        }
        idx++;
    }
}

void OdsTemplateWriter::addRow(const QStringList& cellValues)
{
    if (m_table.empty()) return;

    // Neue Zeile BEFORE den leeren Platzhalterzeilen einfuegen
    // Finde die letzte nicht-leere Zeile oder append am Ende
    pugi::xml_node lastDataRow;
    pugi::xml_node insertBefore;

    for (pugi::xml_node row = m_table.first_child(); row; row = row.next_sibling()) {
        if (QString(row.name()) != "table:table-row") continue;

        // Pruefen ob Zeile leer ist (hat number-rows-repeated oder keine Textinhalte)
        pugi::xml_attribute repeated = row.attribute("table:number-rows-repeated");
        if (repeated) {
            // Das ist der Block mit leeren Zeilen -> davor einfuegen
            if (!insertBefore) {
                insertBefore = row;
            }
            break;
        }
        lastDataRow = row;
    }

    pugi::xml_node newRow;
    if (insertBefore) {
        newRow = m_table.insert_child_before("table:table-row", insertBefore);
    } else {
        newRow = m_table.append_child("table:table-row");
    }

    // Zellen hinzufuegen
    for (const QString& val : cellValues) {
        pugi::xml_node cell = newRow.append_child("table:table-cell");

        if (!val.isEmpty()) {
            cell.append_attribute("office:value-type") = "string";
            cell.append_attribute("calcext:value-type") = "string";
            pugi::xml_node textP = cell.append_child("text:p");
            textP.text().set(val.toUtf8().constData());
        }
    }
}

bool OdsTemplateWriter::save()
{
    if (m_outputPath.isEmpty()) {
        m_lastError = "Kein Ausgabepfad gesetzt";
        return false;
    }

    // content.xml serialisieren
    std::ostringstream oss;
    m_doc.save(oss, "", pugi::format_raw | pugi::format_no_declaration);

    // XML-Deklaration manuell voranstellen (ODS erwartet UTF-8)
    QByteArray newXml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    newXml += QByteArray::fromStdString(oss.str());

    return replaceContentXml(newXml);
}

// ============================================================================
// Private Helpers
// ============================================================================

bool OdsTemplateWriter::readContentXml(const QString& odsPath, QByteArray& xmlData)
{
    QuaZip zip(odsPath);
    if (!zip.open(QuaZip::mdUnzip)) {
        m_lastError = QString("Kann ODS nicht oeffnen: %1").arg(odsPath);
        return false;
    }

    zip.setCurrentFile("content.xml");
    QuaZipFile zipFile(&zip);

    if (!zipFile.open(QIODevice::ReadOnly)) {
        m_lastError = "Kann content.xml nicht lesen";
        zip.close();
        return false;
    }

    xmlData = zipFile.readAll();
    zipFile.close();
    zip.close();

    if (xmlData.isEmpty()) {
        m_lastError = "content.xml ist leer";
        return false;
    }

    return true;
}

bool OdsTemplateWriter::replaceContentXml(const QByteArray& newXml)
{
    // Strategie: Neues ZIP aufbauen, alle Eintraege 1:1 kopieren,
    // dabei content.xml durch newXml ersetzen.

    QString tempPath = m_outputPath + ".tmp";

    QuaZip zipIn(m_outputPath);
    QuaZip zipOut(tempPath);

    if (!zipIn.open(QuaZip::mdUnzip)) {
        m_lastError = QString("Kann ODS nicht oeffnen zum Lesen: %1").arg(m_outputPath);
        return false;
    }

    if (!zipOut.open(QuaZip::mdCreate)) {
        m_lastError = QString("Kann temp-ODS nicht erstellen: %1").arg(tempPath);
        zipIn.close();
        return false;
    }

    // Alle Eintraege durchgehen
    for (bool more = zipIn.goToFirstFile(); more; more = zipIn.goToNextFile())
    {
        QString name = zipIn.getCurrentFileName();

        QuaZipFile fileIn(&zipIn);
        if (!fileIn.open(QIODevice::ReadOnly)) {
            qWarning() << "[OdsWriter] Kann Eintrag nicht lesen:" << name;
            continue;
        }

        QuaZipNewInfo info(name);
        QuaZipFile fileOut(&zipOut);

        // mimetype muss STORED (unkomprimiert) sein – ODS-Spezifikation
        if (name == "mimetype") {
            if (!fileOut.open(QIODevice::WriteOnly, info, nullptr, 0, 0, 0,
                              0 /* method=STORED */, false)) {
                qWarning() << "[OdsWriter] Kann mimetype nicht schreiben";
                fileIn.close();
                continue;
            }
        } else {
            if (!fileOut.open(QIODevice::WriteOnly, info)) {
                qWarning() << "[OdsWriter] Kann Eintrag nicht schreiben:" << name;
                fileIn.close();
                continue;
            }
        }

        if (name == "content.xml") {
            fileOut.write(newXml);
        } else {
            fileOut.write(fileIn.readAll());
        }

        fileOut.close();
        fileIn.close();
    }

    zipOut.close();
    zipIn.close();

    // Swap: Original loeschen, temp umbenennen
    if (!QFile::remove(m_outputPath)) {
        m_lastError = QString("Kann Original nicht loeschen: %1").arg(m_outputPath);
        QFile::remove(tempPath);
        return false;
    }

    if (!QFile::rename(tempPath, m_outputPath)) {
        m_lastError = QString("Kann temp nicht umbenennen: %1").arg(tempPath);
        return false;
    }

    qDebug() << "[OdsWriter] ODS gespeichert:" << m_outputPath;
    return true;
}

} // namespace BatchProcessing

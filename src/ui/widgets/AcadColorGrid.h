/**
 * @file AcadColorGrid.h
 * @brief AutoCAD Color Index (ACI) Color Selection Widget
 * @version 1.0.0
 * 
 * Implements the complete 255-color AutoCAD Color Index palette
 * with accurate RGB values matching AutoCAD/BricsCAD standards.
 */

#ifndef ACADCOLORGRID_H
#define ACADCOLORGRID_H

// WICHTIG: Platform header MUSS zuerst kommen!
#ifdef __linux__
#include "brx_platform_linux.h"
#else
#include "brx_platform_windows.h"
#endif

#include <QDialog>
#include <QWidget>
#include <QPushButton>
#include <QColor>
#include <QGridLayout>
#include <QLabel>
#include <vector>

namespace BatchProcessing {

/**
 * @brief Structure representing an AutoCAD Color Index color
 */
struct AciColor {
    int index;          // ACI index (1-255)
    QString name;       // Color name or description
    QColor rgb;         // RGB color value
    bool isStandard;    // True for standard colors (1-9)
};

/**
 * @brief Custom button for color selection
 */
class ColorButton : public QPushButton {
    Q_OBJECT

public:
    explicit ColorButton(const AciColor& color, QWidget* parent = nullptr);
    
    int colorIndex() const { return m_color.index; }
    QString colorName() const { return m_color.name; }
    QColor rgbColor() const { return m_color.rgb; }
    
signals:
    void colorSelected(int index, const QString& name, const QColor& rgb);
    
protected:
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    
private:
    AciColor m_color;
    void updateStyle(bool hover = false);
};

/**
 * @brief AutoCAD Color Index Grid Widget
 * 
 * Displays the complete 255-color AutoCAD palette in a grid layout
 * matching the standard AutoCAD/BricsCAD color selection dialog.
 */
class AcadColorGrid : public QWidget {
    Q_OBJECT
    
public:
    explicit AcadColorGrid(QWidget* parent = nullptr);
    ~AcadColorGrid() = default;
    
    // Get selected color info
    int selectedIndex() const { return m_selectedIndex; }
    QString selectedName() const { return m_selectedName; }
    QColor selectedRgb() const { return m_selectedRgb; }
    
    // Set current color
    void setSelectedIndex(int index);
    
signals:
    void colorSelected(int index, const QString& name, const QColor& rgb);
    
private slots:
    void onColorButtonClicked(int index, const QString& name, const QColor& rgb);
    
private:
    void setupUi();
    void initializeColors();
    void createStandardColors();
    void createFullSpectrum();
    void createGrayScale();
    
    // Color data
    std::vector<AciColor> m_colors;
    
    // Selection state
    int m_selectedIndex;
    QString m_selectedName;
    QColor m_selectedRgb;
    
    // UI elements
    QGridLayout* m_gridLayout;
    QLabel* m_infoLabel;
    ColorButton* m_selectedButton;
    
    // Layout constants
    static constexpr int BUTTONS_PER_ROW = 25;
public:
    static constexpr int BUTTON_SIZE = 24;
private:
    static constexpr int BUTTON_SPACING = 2;
};

/**
 * @brief Dialog for selecting AutoCAD colors
 */
class AcadColorDialog : public QDialog {
    Q_OBJECT
    
public:
    explicit AcadColorDialog(QWidget* parent = nullptr, int initialIndex = 7);
    ~AcadColorDialog() = default;
    
    // Static convenience method
    static int getColor(QWidget* parent = nullptr, 
                       const QString& title = "Select Color",
                       int initialIndex = 7);
    
    // Get selected color info
    int selectedIndex() const;
    QString selectedName() const;
    QColor selectedRgb() const;
    
private slots:
    void onColorSelected(int index, const QString& name, const QColor& rgb);
    void onByLayerClicked();
    void onByBlockClicked();
    
private:
    void setupUi();
    
    AcadColorGrid* m_colorGrid;
    QLabel* m_previewLabel;
    QPushButton* m_byLayerButton;
    QPushButton* m_byBlockButton;
    QPushButton* m_okButton;
    QPushButton* m_cancelButton;
};

// Utility functions for ACI colors
namespace AciColorUtils {
    
    /**
     * @brief Get RGB color for ACI index
     * @param index ACI index (1-255, 256=ByLayer, 0=ByBlock)
     * @return QColor RGB value
     */
    QColor getColorByIndex(int index);
    
    /**
     * @brief Get standard color name for ACI index
     * @param index ACI index
     * @return Color name or index as string
     */
    QString getColorName(int index);
    
    /**
     * @brief Check if index is a standard color (1-9)
     * @param index ACI index
     * @return True if standard color
     */
    bool isStandardColor(int index);
    
    /**
     * @brief Get the complete ACI color table
     * @return Vector of all 255 ACI colors
     */
    std::vector<AciColor> getAciColorTable();
    
} // namespace AciColorUtils

} // namespace BatchProcessing

#endif // ACADCOLORGRID_H

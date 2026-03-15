#include "windows_fix.h"  // CRITICAL: Qt 6.8+ fix - MUST be FIRST
/**
 * @file AcadColorGrid.cpp
 * @brief AutoCAD Color Index (ACI) Color Selection Widget Implementation
 * @version 1.0.0
 */

// WICHTIG: Platform header MUSS zuerst kommen!
#ifdef __linux__
#include "brx_platform_linux.h"
#else
#include "brx_platform_windows.h"
#endif

#include "AcadColorGrid.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QScrollArea>
#include <QToolTip>
#include <QApplication>
#include <QStyle>
#include <QPainter>
#include <algorithm>

namespace BatchProcessing {

// ============================================================================
// ACI Color Table - Official AutoCAD Color Index RGB Values
// ============================================================================

static const AciColor ACI_COLOR_TABLE[] = {
    // Standard Colors (1-9) - These are the primary AutoCAD colors
    {1, "Red", QColor(255, 0, 0), true},
    {2, "Yellow", QColor(255, 255, 0), true},
    {3, "Green", QColor(0, 255, 0), true},
    {4, "Cyan", QColor(0, 255, 255), true},
    {5, "Blue", QColor(0, 0, 255), true},
    {6, "Magenta", QColor(255, 0, 255), true},
    {7, "White/Black", QColor(255, 255, 255), true},  // Context-dependent
    {8, "Dark Gray", QColor(128, 128, 128), true},
    {9, "Light Gray", QColor(192, 192, 192), true},
    
    // Colors 10-249 - Full spectrum arranged in columns
    // Red spectrum (10-19)
    {10, "Red 10", QColor(255, 0, 0), false},
    {11, "Red 11", QColor(255, 127, 127), false},
    {12, "Red 12", QColor(165, 0, 0), false},
    {13, "Red 13", QColor(165, 82, 82), false},
    {14, "Red 14", QColor(127, 0, 0), false},
    {15, "Red 15", QColor(127, 63, 63), false},
    {16, "Red 16", QColor(76, 0, 0), false},
    {17, "Red 17", QColor(76, 38, 38), false},
    {18, "Red 18", QColor(38, 0, 0), false},
    {19, "Red 19", QColor(38, 19, 19), false},
    
    // Orange/Red-Yellow (20-29)
    {20, "Orange 20", QColor(255, 63, 0), false},
    {21, "Orange 21", QColor(255, 159, 127), false},
    {22, "Orange 22", QColor(165, 41, 0), false},
    {23, "Orange 23", QColor(165, 103, 82), false},
    {24, "Orange 24", QColor(127, 31, 0), false},
    {25, "Orange 25", QColor(127, 79, 63), false},
    {26, "Orange 26", QColor(76, 19, 0), false},
    {27, "Orange 27", QColor(76, 47, 38), false},
    {28, "Orange 28", QColor(38, 9, 0), false},
    {29, "Orange 29", QColor(38, 23, 19), false},
    
    // Yellow-Orange (30-39)
    {30, "Yellow 30", QColor(255, 127, 0), false},
    {31, "Yellow 31", QColor(255, 191, 127), false},
    {32, "Yellow 32", QColor(165, 82, 0), false},
    {33, "Yellow 33", QColor(165, 124, 82), false},
    {34, "Yellow 34", QColor(127, 63, 0), false},
    {35, "Yellow 35", QColor(127, 95, 63), false},
    {36, "Yellow 36", QColor(76, 38, 0), false},
    {37, "Yellow 37", QColor(76, 57, 38), false},
    {38, "Yellow 38", QColor(38, 19, 0), false},
    {39, "Yellow 39", QColor(38, 28, 19), false},
    
    // Pure Yellow (40-49)
    {40, "Yellow 40", QColor(255, 191, 0), false},
    {41, "Yellow 41", QColor(255, 223, 127), false},
    {42, "Yellow 42", QColor(165, 124, 0), false},
    {43, "Yellow 43", QColor(165, 145, 82), false},
    {44, "Yellow 44", QColor(127, 95, 0), false},
    {45, "Yellow 45", QColor(127, 111, 63), false},
    {46, "Yellow 46", QColor(76, 57, 0), false},
    {47, "Yellow 47", QColor(76, 66, 38), false},
    {48, "Yellow 48", QColor(38, 28, 0), false},
    {49, "Yellow 49", QColor(38, 33, 19), false},
    
    // Yellow-Green (50-59)
    {50, "Yellow-Green 50", QColor(255, 255, 0), false},
    {51, "Yellow-Green 51", QColor(255, 255, 127), false},
    {52, "Yellow-Green 52", QColor(165, 165, 0), false},
    {53, "Yellow-Green 53", QColor(165, 165, 82), false},
    {54, "Yellow-Green 54", QColor(127, 127, 0), false},
    {55, "Yellow-Green 55", QColor(127, 127, 63), false},
    {56, "Yellow-Green 56", QColor(76, 76, 0), false},
    {57, "Yellow-Green 57", QColor(76, 76, 38), false},
    {58, "Yellow-Green 58", QColor(38, 38, 0), false},
    {59, "Yellow-Green 59", QColor(38, 38, 19), false},
    
    // Green-Yellow (60-69)
    {60, "Green 60", QColor(191, 255, 0), false},
    {61, "Green 61", QColor(223, 255, 127), false},
    {62, "Green 62", QColor(124, 165, 0), false},
    {63, "Green 63", QColor(145, 165, 82), false},
    {64, "Green 64", QColor(95, 127, 0), false},
    {65, "Green 65", QColor(111, 127, 63), false},
    {66, "Green 66", QColor(57, 76, 0), false},
    {67, "Green 67", QColor(66, 76, 38), false},
    {68, "Green 68", QColor(28, 38, 0), false},
    {69, "Green 69", QColor(33, 38, 19), false},
    
    // Green (70-79)
    {70, "Green 70", QColor(127, 255, 0), false},
    {71, "Green 71", QColor(191, 255, 127), false},
    {72, "Green 72", QColor(82, 165, 0), false},
    {73, "Green 73", QColor(124, 165, 82), false},
    {74, "Green 74", QColor(63, 127, 0), false},
    {75, "Green 75", QColor(95, 127, 63), false},
    {76, "Green 76", QColor(38, 76, 0), false},
    {77, "Green 77", QColor(57, 76, 38), false},
    {78, "Green 78", QColor(19, 38, 0), false},
    {79, "Green 79", QColor(28, 38, 19), false},
    
    // Green (80-89)
    {80, "Green 80", QColor(63, 255, 0), false},
    {81, "Green 81", QColor(159, 255, 127), false},
    {82, "Green 82", QColor(41, 165, 0), false},
    {83, "Green 83", QColor(103, 165, 82), false},
    {84, "Green 84", QColor(31, 127, 0), false},
    {85, "Green 85", QColor(79, 127, 63), false},
    {86, "Green 86", QColor(19, 76, 0), false},
    {87, "Green 87", QColor(47, 76, 38), false},
    {88, "Green 88", QColor(9, 38, 0), false},
    {89, "Green 89", QColor(23, 38, 19), false},
    
    // Pure Green (90-99)
    {90, "Green 90", QColor(0, 255, 0), false},
    {91, "Green 91", QColor(127, 255, 127), false},
    {92, "Green 92", QColor(0, 165, 0), false},
    {93, "Green 93", QColor(82, 165, 82), false},
    {94, "Green 94", QColor(0, 127, 0), false},
    {95, "Green 95", QColor(63, 127, 63), false},
    {96, "Green 96", QColor(0, 76, 0), false},
    {97, "Green 97", QColor(38, 76, 38), false},
    {98, "Green 98", QColor(0, 38, 0), false},
    {99, "Green 99", QColor(19, 38, 19), false},
    
    // Green-Cyan (100-109)
    {100, "Cyan 100", QColor(0, 255, 63), false},
    {101, "Cyan 101", QColor(127, 255, 159), false},
    {102, "Cyan 102", QColor(0, 165, 41), false},
    {103, "Cyan 103", QColor(82, 165, 103), false},
    {104, "Cyan 104", QColor(0, 127, 31), false},
    {105, "Cyan 105", QColor(63, 127, 79), false},
    {106, "Cyan 106", QColor(0, 76, 19), false},
    {107, "Cyan 107", QColor(38, 76, 47), false},
    {108, "Cyan 108", QColor(0, 38, 9), false},
    {109, "Cyan 109", QColor(19, 38, 23), false},
    
    // Cyan-Green (110-119)
    {110, "Cyan 110", QColor(0, 255, 127), false},
    {111, "Cyan 111", QColor(127, 255, 191), false},
    {112, "Cyan 112", QColor(0, 165, 82), false},
    {113, "Cyan 113", QColor(82, 165, 124), false},
    {114, "Cyan 114", QColor(0, 127, 63), false},
    {115, "Cyan 115", QColor(63, 127, 95), false},
    {116, "Cyan 116", QColor(0, 76, 38), false},
    {117, "Cyan 117", QColor(38, 76, 57), false},
    {118, "Cyan 118", QColor(0, 38, 19), false},
    {119, "Cyan 119", QColor(19, 38, 28), false},
    
    // Cyan (120-129)
    {120, "Cyan 120", QColor(0, 255, 191), false},
    {121, "Cyan 121", QColor(127, 255, 223), false},
    {122, "Cyan 122", QColor(0, 165, 124), false},
    {123, "Cyan 123", QColor(82, 165, 145), false},
    {124, "Cyan 124", QColor(0, 127, 95), false},
    {125, "Cyan 125", QColor(63, 127, 111), false},
    {126, "Cyan 126", QColor(0, 76, 57), false},
    {127, "Cyan 127", QColor(38, 76, 66), false},
    {128, "Cyan 128", QColor(0, 38, 28), false},
    {129, "Cyan 129", QColor(19, 38, 33), false},
    
    // Pure Cyan (130-139)
    {130, "Cyan 130", QColor(0, 255, 255), false},
    {131, "Cyan 131", QColor(127, 255, 255), false},
    {132, "Cyan 132", QColor(0, 165, 165), false},
    {133, "Cyan 133", QColor(82, 165, 165), false},
    {134, "Cyan 134", QColor(0, 127, 127), false},
    {135, "Cyan 135", QColor(63, 127, 127), false},
    {136, "Cyan 136", QColor(0, 76, 76), false},
    {137, "Cyan 137", QColor(38, 76, 76), false},
    {138, "Cyan 138", QColor(0, 38, 38), false},
    {139, "Cyan 139", QColor(19, 38, 38), false},
    
    // Cyan-Blue (140-149)
    {140, "Blue 140", QColor(0, 191, 255), false},
    {141, "Blue 141", QColor(127, 223, 255), false},
    {142, "Blue 142", QColor(0, 124, 165), false},
    {143, "Blue 143", QColor(82, 145, 165), false},
    {144, "Blue 144", QColor(0, 95, 127), false},
    {145, "Blue 145", QColor(63, 111, 127), false},
    {146, "Blue 146", QColor(0, 57, 76), false},
    {147, "Blue 147", QColor(38, 66, 76), false},
    {148, "Blue 148", QColor(0, 28, 38), false},
    {149, "Blue 149", QColor(19, 33, 38), false},
    
    // Blue-Cyan (150-159)
    {150, "Blue 150", QColor(0, 127, 255), false},
    {151, "Blue 151", QColor(127, 191, 255), false},
    {152, "Blue 152", QColor(0, 82, 165), false},
    {153, "Blue 153", QColor(82, 124, 165), false},
    {154, "Blue 154", QColor(0, 63, 127), false},
    {155, "Blue 155", QColor(63, 95, 127), false},
    {156, "Blue 156", QColor(0, 38, 76), false},
    {157, "Blue 157", QColor(38, 57, 76), false},
    {158, "Blue 158", QColor(0, 19, 38), false},
    {159, "Blue 159", QColor(19, 28, 38), false},
    
    // Blue (160-169)
    {160, "Blue 160", QColor(0, 63, 255), false},
    {161, "Blue 161", QColor(127, 159, 255), false},
    {162, "Blue 162", QColor(0, 41, 165), false},
    {163, "Blue 163", QColor(82, 103, 165), false},
    {164, "Blue 164", QColor(0, 31, 127), false},
    {165, "Blue 165", QColor(63, 79, 127), false},
    {166, "Blue 166", QColor(0, 19, 76), false},
    {167, "Blue 167", QColor(38, 47, 76), false},
    {168, "Blue 168", QColor(0, 9, 38), false},
    {169, "Blue 169", QColor(19, 23, 38), false},
    
    // Pure Blue (170-179)
    {170, "Blue 170", QColor(0, 0, 255), false},
    {171, "Blue 171", QColor(127, 127, 255), false},
    {172, "Blue 172", QColor(0, 0, 165), false},
    {173, "Blue 173", QColor(82, 82, 165), false},
    {174, "Blue 174", QColor(0, 0, 127), false},
    {175, "Blue 175", QColor(63, 63, 127), false},
    {176, "Blue 176", QColor(0, 0, 76), false},
    {177, "Blue 177", QColor(38, 38, 76), false},
    {178, "Blue 178", QColor(0, 0, 38), false},
    {179, "Blue 179", QColor(19, 19, 38), false},
    
    // Blue-Magenta (180-189)
    {180, "Magenta 180", QColor(63, 0, 255), false},
    {181, "Magenta 181", QColor(159, 127, 255), false},
    {182, "Magenta 182", QColor(41, 0, 165), false},
    {183, "Magenta 183", QColor(103, 82, 165), false},
    {184, "Magenta 184", QColor(31, 0, 127), false},
    {185, "Magenta 185", QColor(79, 63, 127), false},
    {186, "Magenta 186", QColor(19, 0, 76), false},
    {187, "Magenta 187", QColor(47, 38, 76), false},
    {188, "Magenta 188", QColor(9, 0, 38), false},
    {189, "Magenta 189", QColor(23, 19, 38), false},
    
    // Magenta-Blue (190-199)
    {190, "Magenta 190", QColor(127, 0, 255), false},
    {191, "Magenta 191", QColor(191, 127, 255), false},
    {192, "Magenta 192", QColor(82, 0, 165), false},
    {193, "Magenta 193", QColor(124, 82, 165), false},
    {194, "Magenta 194", QColor(63, 0, 127), false},
    {195, "Magenta 195", QColor(95, 63, 127), false},
    {196, "Magenta 196", QColor(38, 0, 76), false},
    {197, "Magenta 197", QColor(57, 38, 76), false},
    {198, "Magenta 198", QColor(19, 0, 38), false},
    {199, "Magenta 199", QColor(28, 19, 38), false},
    
    // Magenta (200-209)
    {200, "Magenta 200", QColor(191, 0, 255), false},
    {201, "Magenta 201", QColor(223, 127, 255), false},
    {202, "Magenta 202", QColor(124, 0, 165), false},
    {203, "Magenta 203", QColor(145, 82, 165), false},
    {204, "Magenta 204", QColor(95, 0, 127), false},
    {205, "Magenta 205", QColor(111, 63, 127), false},
    {206, "Magenta 206", QColor(57, 0, 76), false},
    {207, "Magenta 207", QColor(66, 38, 76), false},
    {208, "Magenta 208", QColor(28, 0, 38), false},
    {209, "Magenta 209", QColor(33, 19, 38), false},
    
    // Pure Magenta (210-219)
    {210, "Magenta 210", QColor(255, 0, 255), false},
    {211, "Magenta 211", QColor(255, 127, 255), false},
    {212, "Magenta 212", QColor(165, 0, 165), false},
    {213, "Magenta 213", QColor(165, 82, 165), false},
    {214, "Magenta 214", QColor(127, 0, 127), false},
    {215, "Magenta 215", QColor(127, 63, 127), false},
    {216, "Magenta 216", QColor(76, 0, 76), false},
    {217, "Magenta 217", QColor(76, 38, 76), false},
    {218, "Magenta 218", QColor(38, 0, 38), false},
    {219, "Magenta 219", QColor(38, 19, 38), false},
    
    // Magenta-Red (220-229)
    {220, "Red 220", QColor(255, 0, 191), false},
    {221, "Red 221", QColor(255, 127, 223), false},
    {222, "Red 222", QColor(165, 0, 124), false},
    {223, "Red 223", QColor(165, 82, 145), false},
    {224, "Red 224", QColor(127, 0, 95), false},
    {225, "Red 225", QColor(127, 63, 111), false},
    {226, "Red 226", QColor(76, 0, 57), false},
    {227, "Red 227", QColor(76, 38, 66), false},
    {228, "Red 228", QColor(38, 0, 28), false},
    {229, "Red 229", QColor(38, 19, 33), false},
    
    // Red-Magenta (230-239)
    {230, "Red 230", QColor(255, 0, 127), false},
    {231, "Red 231", QColor(255, 127, 191), false},
    {232, "Red 232", QColor(165, 0, 82), false},
    {233, "Red 233", QColor(165, 82, 124), false},
    {234, "Red 234", QColor(127, 0, 63), false},
    {235, "Red 235", QColor(127, 63, 95), false},
    {236, "Red 236", QColor(76, 0, 38), false},
    {237, "Red 237", QColor(76, 38, 57), false},
    {238, "Red 238", QColor(38, 0, 19), false},
    {239, "Red 239", QColor(38, 19, 28), false},
    
    // Red (240-249)
    {240, "Red 240", QColor(255, 0, 63), false},
    {241, "Red 241", QColor(255, 127, 159), false},
    {242, "Red 242", QColor(165, 0, 41), false},
    {243, "Red 243", QColor(165, 82, 103), false},
    {244, "Red 244", QColor(127, 0, 31), false},
    {245, "Red 245", QColor(127, 63, 79), false},
    {246, "Red 246", QColor(76, 0, 19), false},
    {247, "Red 247", QColor(76, 38, 47), false},
    {248, "Red 248", QColor(38, 0, 9), false},
    {249, "Red 249", QColor(38, 19, 23), false},
    
    // Gray Scale (250-255) - These are fixed grayscale values
    {250, "Gray 10%", QColor(51, 51, 51), false},
    {251, "Gray 20%", QColor(80, 80, 80), false},
    {252, "Gray 30%", QColor(105, 105, 105), false},
    {253, "Gray 50%", QColor(130, 130, 130), false},
    {254, "Gray 70%", QColor(180, 180, 180), false},
    {255, "Gray 80%", QColor(205, 205, 205), false}
};

// ============================================================================
// ColorButton Implementation
// ============================================================================

ColorButton::ColorButton(const AciColor& color, QWidget* parent)
    : QPushButton(parent)
    , m_color(color)
{
    setFixedSize(AcadColorGrid::BUTTON_SIZE, AcadColorGrid::BUTTON_SIZE);
    updateStyle();
    
    // Set tooltip
    QString tooltip = QString("%1\nACI: %2\nRGB: (%3, %4, %5)")
                     .arg(m_color.name)
                     .arg(m_color.index)
                     .arg(m_color.rgb.red())
                     .arg(m_color.rgb.green())
                     .arg(m_color.rgb.blue());
    setToolTip(tooltip);
    
    // Connect click signal
    connect(this, &QPushButton::clicked, [this]() {
        emit colorSelected(m_color.index, m_color.name, m_color.rgb);
    });
}

void ColorButton::enterEvent(QEnterEvent* event) {
    Q_UNUSED(event)
    updateStyle(true);
}

void ColorButton::leaveEvent(QEvent* event) {
    Q_UNUSED(event)
    updateStyle(false);
}

void ColorButton::updateStyle(bool hover) {
    // Determine if we need a border for visibility
    bool needsBorder = (m_color.index == 7 || m_color.index >= 250 || 
                       m_color.rgb.lightness() > 240);
    
    QString borderStyle;
    if (hover) {
        borderStyle = "border: 2px solid #000000;";
    } else if (needsBorder) {
        borderStyle = "border: 1px solid #808080;";
    } else {
        borderStyle = "border: 1px solid #404040;";
    }
    
    setStyleSheet(QString(
        "QPushButton {"
        "  background-color: %1;"
        "  %2"
        "  border-radius: 2px;"
        "}"
        "QPushButton:pressed {"
        "  border: 2px solid #0000FF;"
        "}"
    ).arg(m_color.rgb.name()).arg(borderStyle));
}

// ============================================================================
// AcadColorGrid Implementation
// ============================================================================

AcadColorGrid::AcadColorGrid(QWidget* parent)
    : QWidget(parent)
    , m_selectedIndex(7)  // Default to white
    , m_selectedName("White/Black")
    , m_selectedRgb(255, 255, 255)
    , m_selectedButton(nullptr)
{
    initializeColors();
    setupUi();
}

void AcadColorGrid::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(5);
    
    // Header label
    auto* headerLabel = new QLabel("AutoCAD Color Index (ACI)");
    headerLabel->setAlignment(Qt::AlignCenter);
    headerLabel->setStyleSheet("QLabel { font-weight: bold; font-size: 12pt; }");
    mainLayout->addWidget(headerLabel);
    
    // Scroll area for the grid
    auto* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    auto* scrollWidget = new QWidget();
    auto* scrollLayout = new QVBoxLayout(scrollWidget);
    
    // Standard colors section (1-9)
    auto* standardLabel = new QLabel("Standard Colors");
    standardLabel->setStyleSheet("QLabel { font-weight: bold; margin-top: 5px; }");
    scrollLayout->addWidget(standardLabel);
    
    auto* standardLayout = new QHBoxLayout();
    standardLayout->setSpacing(BUTTON_SPACING);
    
    for (const auto& color : m_colors) {
        if (color.index >= 1 && color.index <= 9) {
            auto* button = new ColorButton(color, this);
            connect(button, &ColorButton::colorSelected,
                   this, &AcadColorGrid::onColorButtonClicked);
            standardLayout->addWidget(button);
        }
    }
    standardLayout->addStretch();
    scrollLayout->addLayout(standardLayout);
    
    // Full spectrum section (10-249)
    auto* spectrumLabel = new QLabel("Full Spectrum (10-249)");
    spectrumLabel->setStyleSheet("QLabel { font-weight: bold; margin-top: 10px; }");
    scrollLayout->addWidget(spectrumLabel);
    
    // Create grid for spectrum colors
    auto* spectrumWidget = new QWidget();
    m_gridLayout = new QGridLayout(spectrumWidget);
    m_gridLayout->setSpacing(BUTTON_SPACING);
    m_gridLayout->setContentsMargins(0, 0, 0, 0);
    
    int gridRow = 0;
    int gridCol = 0;
    
    for (const auto& color : m_colors) {
        if (color.index >= 10 && color.index <= 249) {
            auto* button = new ColorButton(color, this);
            connect(button, &ColorButton::colorSelected,
                   this, &AcadColorGrid::onColorButtonClicked);
            
            m_gridLayout->addWidget(button, gridRow, gridCol);
            
            gridCol++;
            if (gridCol >= BUTTONS_PER_ROW) {
                gridCol = 0;
                gridRow++;
            }
        }
    }
    
    scrollLayout->addWidget(spectrumWidget);
    
    // Gray scale section (250-255)
    auto* grayLabel = new QLabel("Gray Scale");
    grayLabel->setStyleSheet("QLabel { font-weight: bold; margin-top: 10px; }");
    scrollLayout->addWidget(grayLabel);
    
    auto* grayLayout = new QHBoxLayout();
    grayLayout->setSpacing(BUTTON_SPACING);
    
    for (const auto& color : m_colors) {
        if (color.index >= 250 && color.index <= 255) {
            auto* button = new ColorButton(color, this);
            connect(button, &ColorButton::colorSelected,
                   this, &AcadColorGrid::onColorButtonClicked);
            grayLayout->addWidget(button);
        }
    }
    grayLayout->addStretch();
    scrollLayout->addLayout(grayLayout);
    
    scrollLayout->addStretch();
    scrollArea->setWidget(scrollWidget);
    mainLayout->addWidget(scrollArea);
    
    // Info label
    m_infoLabel = new QLabel(QString("Selected: %1 (ACI %2)")
                            .arg(m_selectedName).arg(m_selectedIndex));
    m_infoLabel->setAlignment(Qt::AlignCenter);
    m_infoLabel->setStyleSheet("QLabel { padding: 5px; background: #F0F0F0; }");
    mainLayout->addWidget(m_infoLabel);
}

void AcadColorGrid::initializeColors() {
    m_colors.clear();
    
    // Copy all colors from the static table
    for (const auto& color : ACI_COLOR_TABLE) {
        m_colors.push_back(color);
    }
}

void AcadColorGrid::onColorButtonClicked(int index, const QString& name, const QColor& rgb) {
    m_selectedIndex = index;
    m_selectedName = name;
    m_selectedRgb = rgb;
    
    m_infoLabel->setText(QString("Selected: %1 (ACI %2)")
                        .arg(name).arg(index));
    
    emit colorSelected(index, name, rgb);
}

void AcadColorGrid::setSelectedIndex(int index) {
    if (index < 1 || index > 255) return;
    
    for (const auto& color : m_colors) {
        if (color.index == index) {
            m_selectedIndex = index;
            m_selectedName = color.name;
            m_selectedRgb = color.rgb;
            
            m_infoLabel->setText(QString("Selected: %1 (ACI %2)")
                                .arg(m_selectedName).arg(m_selectedIndex));
            break;
        }
    }
}

// ============================================================================
// AcadColorDialog Implementation
// ============================================================================

AcadColorDialog::AcadColorDialog(QWidget* parent, int initialIndex)
    : QDialog(parent)
{
    setupUi();
    setWindowTitle("Select Color");
    setModal(true);
    
    // Set initial color
    m_colorGrid->setSelectedIndex(initialIndex);
}

void AcadColorDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    
    // Color grid
    m_colorGrid = new AcadColorGrid(this);
    connect(m_colorGrid, &AcadColorGrid::colorSelected,
           this, &AcadColorDialog::onColorSelected);
    mainLayout->addWidget(m_colorGrid);
    
    // Special buttons
    auto* specialLayout = new QHBoxLayout();
    
    m_byLayerButton = new QPushButton("ByLayer");
    m_byLayerButton->setToolTip("Color will be determined by layer (256)");
    connect(m_byLayerButton, &QPushButton::clicked,
           this, &AcadColorDialog::onByLayerClicked);
    specialLayout->addWidget(m_byLayerButton);
    
    m_byBlockButton = new QPushButton("ByBlock");
    m_byBlockButton->setToolTip("Color will be determined by block (0)");
    connect(m_byBlockButton, &QPushButton::clicked,
           this, &AcadColorDialog::onByBlockClicked);
    specialLayout->addWidget(m_byBlockButton);
    
    specialLayout->addStretch();
    mainLayout->addLayout(specialLayout);
    
    // Preview area
    auto* previewLayout = new QHBoxLayout();
    previewLayout->addWidget(new QLabel("Preview:"));
    
    m_previewLabel = new QLabel();
    m_previewLabel->setFixedSize(60, 30);
    m_previewLabel->setFrameStyle(QFrame::Box);
    m_previewLabel->setStyleSheet(QString("background-color: %1;")
                                  .arg(m_colorGrid->selectedRgb().name()));
    previewLayout->addWidget(m_previewLabel);
    
    previewLayout->addStretch();
    mainLayout->addLayout(previewLayout);
    
    // Dialog buttons
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
    
    resize(700, 500);
}

void AcadColorDialog::onColorSelected(int index, const QString& name, const QColor& rgb) {
    Q_UNUSED(index)
    Q_UNUSED(name)
    m_previewLabel->setStyleSheet(QString("background-color: %1;").arg(rgb.name()));
}

void AcadColorDialog::onByLayerClicked() {
    // ByLayer is index 256 in AutoCAD
    accept();
}

void AcadColorDialog::onByBlockClicked() {
    // ByBlock is index 0 in AutoCAD
    accept();
}

int AcadColorDialog::getColor(QWidget* parent, const QString& title, int initialIndex) {
    AcadColorDialog dialog(parent, initialIndex);
    if (!title.isEmpty()) {
        dialog.setWindowTitle(title);
    }
    
    if (dialog.exec() == QDialog::Accepted) {
        return dialog.selectedIndex();
    }
    
    return -1;  // Cancelled
}

int AcadColorDialog::selectedIndex() const {
    return m_colorGrid->selectedIndex();
}

QString AcadColorDialog::selectedName() const {
    return m_colorGrid->selectedName();
}

QColor AcadColorDialog::selectedRgb() const {
    return m_colorGrid->selectedRgb();
}

// ============================================================================
// AciColorUtils Implementation
// ============================================================================

namespace AciColorUtils {

QColor getColorByIndex(int index) {
    if (index == 0) {
        // ByBlock
        return QColor(255, 255, 255);  // Default to white
    } else if (index == 256) {
        // ByLayer
        return QColor(255, 255, 255);  // Default to white
    } else if (index >= 1 && index <= 255) {
        for (const auto& color : ACI_COLOR_TABLE) {
            if (color.index == index) {
                return color.rgb;
            }
        }
    }
    
    return QColor();  // Invalid
}

QString getColorName(int index) {
    if (index == 0) {
        return "ByBlock";
    } else if (index == 256) {
        return "ByLayer";
    } else if (index >= 1 && index <= 255) {
        for (const auto& color : ACI_COLOR_TABLE) {
            if (color.index == index) {
                return color.name;
            }
        }
    }
    
    return QString("Color %1").arg(index);
}

bool isStandardColor(int index) {
    return index >= 1 && index <= 9;
}

std::vector<AciColor> getAciColorTable() {
    std::vector<AciColor> colors;
    for (const auto& color : ACI_COLOR_TABLE) {
        colors.push_back(color);
    }
    return colors;
}

} // namespace AciColorUtils

} // namespace BatchProcessing

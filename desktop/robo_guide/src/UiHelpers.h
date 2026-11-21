#pragma once

#include <QLabel>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

namespace ui {

QString styleSheet();

// Page skeleton: heading + vertical layout. Returns the layout to fill.
QVBoxLayout* makePage(QWidget* page, const QString& heading);

// Small explanatory text under a heading.
QLabel* note(const QString& text);

// Coloured status pill text ("ok", "warn", "bad", "idle").
void setPill(QLabel* label, const QString& text, const QString& level);

QString verdictColor(const QString& verdict);

}  // namespace ui

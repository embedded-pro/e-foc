#pragma once

#include <QPlainTextEdit>
#include <QStringList>
#include <QWidget>

class QLineEdit;

namespace virtual_motor::app
{
    class LogView
        : public QWidget
    {
        Q_OBJECT

    public:
        static constexpr int maximumLines{ 5000 };

        explicit LogView(QWidget* parent = nullptr);

        void Append(const QString& line);
        void SetLines(const QStringList& lines);

    private:
        void Refilter();
        void Save();

        QLineEdit* filter;
        QPlainTextEdit* text;
        QStringList lines;
    };
}

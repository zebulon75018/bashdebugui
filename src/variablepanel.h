#pragma once
#include <QWidget>
#include <QTreeWidget>
#include <QLineEdit>
#include <QMap>

class VariablePanel : public QWidget {
    Q_OBJECT
public:
    explicit VariablePanel(QWidget* parent = nullptr);
    void updateVariables(const QMap<QString,QString>& vars);
    void clear();

private slots:
    void onSearch(const QString& text);

private:
    QLineEdit*   m_search;
    QTreeWidget* m_tree;

    QMap<QString,QString> m_lastVars;
    QMap<QString,QString> m_prevVars;

    void populate(const QString& filter = {});

    // Categorise a variable name
    static QString category(const QString& name);
};

#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QLineEdit>
#include <QPushButton>
//#include "bashdbdebugger.h"

struct Variable {
    QString name;
    QString value;
    QString type;   // "local" | "global" | "env"
};

struct StackFrame {
    int     level    { 0 };
    QString function;
    QString file;
    int     line     { 0 };
};



// ─────────────────────────────────────────────────────────────────────────────
// VariablesView – displays locals/globals and watch expressions.
// ─────────────────────────────────────────────────────────────────────────────
class VariablesView : public QWidget
{
    Q_OBJECT

public:
    explicit VariablesView(QWidget *parent = nullptr);

    void updateVariables(const QList<Variable> &vars);
    void clear();

signals:
    /** Emitted when user adds a watch expression */
    void watchAdded(const QString &expr);
    /** Emitted when user requests to print an expression immediately */
    void printRequested(const QString &expr);

private slots:
    void onAddWatch();
    void onItemDoubleClicked(QTreeWidgetItem *item, int column);

private:
    QTreeWidget  *m_tree       { nullptr };
    QLineEdit    *m_watchInput { nullptr };
    QPushButton  *m_addBtn     { nullptr };

    QTreeWidgetItem *m_localsRoot  { nullptr };
    QTreeWidgetItem *m_watchRoot   { nullptr };

    void ensureRoots();
};

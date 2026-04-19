#include "variablesview.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QFont>

// ─────────────────────────────────────────────────────────────────────────────
VariablesView::VariablesView(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // ── Tree ──────────────────────────────────────────────────────────────────
    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(3);
    m_tree->setHeaderLabels({"Name", "Value", "Type"});
    m_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tree->setFont(QFont("Monospace", 10));
    m_tree->setStyleSheet(
        "QTreeWidget {"
        "  background-color: #1E1E1E;"
        "  color: #D4D4D4;"
        "  border: 1px solid #3C3C3C;"
        "  alternate-background-color: #252526;"
        "}"
        "QTreeWidget::item:selected {"
        "  background-color: #264F78;"
        "}"
        "QHeaderView::section {"
        "  background-color: #252526;"
        "  color: #9CDCFE;"
        "  border: 1px solid #3C3C3C;"
        "  padding: 4px;"
        "}"
    );
    m_tree->setAlternatingRowColors(true);
    layout->addWidget(m_tree, 1);

    connect(m_tree, &QTreeWidget::itemDoubleClicked,
            this,   &VariablesView::onItemDoubleClicked);

    // ── Watch input ───────────────────────────────────────────────────────────
    auto *watchLayout = new QHBoxLayout;
    watchLayout->setSpacing(4);

    m_watchInput = new QLineEdit(this);
    m_watchInput->setPlaceholderText("Watch expression…");
    m_watchInput->setStyleSheet(
        "QLineEdit { background:#1E1E1E; color:#9CDCFE;"
        " border:1px solid #3C3C3C; padding:3px; }"
    );
    connect(m_watchInput, &QLineEdit::returnPressed,
            this,         &VariablesView::onAddWatch);

    m_addBtn = new QPushButton("+", this);
    m_addBtn->setFixedWidth(28);
    m_addBtn->setToolTip("Add watch expression");
    m_addBtn->setStyleSheet(
        "QPushButton { background:#3C3C3C; color:#D4D4D4;"
        " border:1px solid #555; border-radius:2px; font-weight:bold; }"
        "QPushButton:hover { background:#505050; }"
    );
    connect(m_addBtn, &QPushButton::clicked,
            this,     &VariablesView::onAddWatch);

    watchLayout->addWidget(new QLabel("Watch:", this));
    watchLayout->addWidget(m_watchInput, 1);
    watchLayout->addWidget(m_addBtn);
    layout->addLayout(watchLayout);

    ensureRoots();
}

// ─────────────────────────────────────────────────────────────────────────────
void VariablesView::ensureRoots()
{
    if (!m_localsRoot) {
        m_localsRoot = new QTreeWidgetItem(m_tree);
        m_localsRoot->setText(0, "Locals");
        m_localsRoot->setForeground(0, QColor("#4EC9B0"));
        m_localsRoot->setExpanded(true);
    }
    if (!m_watchRoot) {
        m_watchRoot = new QTreeWidgetItem(m_tree);
        m_watchRoot->setText(0, "Watch");
        m_watchRoot->setForeground(0, QColor("#C586C0"));
        m_watchRoot->setExpanded(true);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void VariablesView::updateVariables(const QList<Variable> &vars)
{
    // Remove old locals children
    while (m_localsRoot->childCount() > 0)
        delete m_localsRoot->takeChild(0);

    for (const Variable &v : vars) {
        auto *item = new QTreeWidgetItem(m_localsRoot);
        item->setText(0, v.name);
        item->setText(1, v.value);
        //item->setText(2, v.type.isEmpty() ? "local" : v.type);
        item->setForeground(0, QColor("#9CDCFE"));
        item->setForeground(1, QColor("#CE9178"));
        item->setExpanded(true);
    }
    m_localsRoot->setExpanded(true);
}

// ─────────────────────────────────────────────────────────────────────────────
void VariablesView::clear()
{
    while (m_localsRoot->childCount() > 0)
        delete m_localsRoot->takeChild(0);
}

// ─────────────────────────────────────────────────────────────────────────────
void VariablesView::onAddWatch()
{
    const QString expr = m_watchInput->text().trimmed();
    if (expr.isEmpty()) return;

    // Add to watch tree
    auto *item = new QTreeWidgetItem(m_watchRoot);
    item->setText(0, expr);
    item->setText(1, "…");
    item->setForeground(0, QColor("#C586C0"));
    m_watchRoot->setExpanded(true);

    m_watchInput->clear();
    emit watchAdded(expr);
    emit printRequested(expr);
}

void VariablesView::onItemDoubleClicked(QTreeWidgetItem *item, int /*column*/)
{
    if (item && item->parent()) { // not a root item
        emit printRequested(item->text(0));
    }
}

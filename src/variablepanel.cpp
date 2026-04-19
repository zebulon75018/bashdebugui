#include "variablepanel.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QFont>

static const QStringList ENV_KNOWN = {
    "HOME","PATH","USER","SHELL","PWD","OLDPWD","LOGNAME","TERM",
    "LANG","LANGUAGE","LC_ALL","DISPLAY","TMPDIR","EDITOR","VISUAL",
    "HOSTNAME","HOSTTYPE","MACHTYPE","OSTYPE","SHLVL","IFS"
};

static const QStringList BASH_INTERNAL = {
    "BASH","BASHOPTS","BASH_ARGC","BASH_ARGV","BASH_COMMAND",
    "BASH_LINENO","BASH_SOURCE","BASH_SUBSHELL","BASH_VERSION",
    "BASH_VERSINFO","BASHPID","GROUPS","PIPESTATUS","PPID",
    "RANDOM","SECONDS","UID","EUID","LINENO","FUNCNAME",
    "DIRSTACK","HISTCMD","HISTFILE","HISTSIZE","HISTFILESIZE","PS1","PS2","PS4"
};

VariablePanel::VariablePanel(QWidget* parent) : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->setSpacing(4);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText("Filter variables…");
    m_search->setStyleSheet("QLineEdit { background:#1e1e2e; color:#cdd6f4;"
                            " border:1px solid #45475a; border-radius:4px; padding:4px; }");
    connect(m_search, &QLineEdit::textChanged, this, &VariablePanel::onSearch);
    lay->addWidget(m_search);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(2);
    m_tree->setHeaderLabels({"Variable", "Value"});
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tree->header()->setDefaultSectionSize(160);
    m_tree->setAlternatingRowColors(true);
    m_tree->setStyleSheet(
        "QTreeWidget { background:#1e1e2e; alternate-background-color:#181825;"
        " color:#cdd6f4; border:none; }"
        "QTreeWidget::item:selected { background:#313244; }"
        "QHeaderView::section { background:#181825; color:#888; border:none;"
        " padding:4px; font-size:11px; }"
    );

    QFont f = m_tree->font();
    f.setFamily("Monospace");
    f.setPointSize(10);
    m_tree->setFont(f);

    lay->addWidget(m_tree);
}

void VariablePanel::updateVariables(const QMap<QString,QString>& vars)
{
    m_prevVars = m_lastVars;
    m_lastVars = vars;
    populate(m_search->text());
}

void VariablePanel::clear()
{
    m_lastVars.clear();
    m_prevVars.clear();
    m_tree->clear();
}

void VariablePanel::onSearch(const QString& text)
{
    populate(text);
}

void VariablePanel::populate(const QString& filter)
{
    // Save open state
    QSet<QString> openCats;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        auto* cat = m_tree->topLevelItem(i);
        if (cat->isExpanded()) openCats.insert(cat->text(0));
    }

    m_tree->clear();

    // Buckets
    QMap<QString, QMap<QString,QString>> cats;
    for (auto it = m_lastVars.cbegin(); it != m_lastVars.cend(); ++it) {
        if (!filter.isEmpty() && !it.key().contains(filter, Qt::CaseInsensitive)
                              && !it.value().contains(filter, Qt::CaseInsensitive))
            continue;
        cats[category(it.key())][it.key()] = it.value();
    }

    // Category colors
    QMap<QString,QColor> catColors = {
        {"User", QColor("#a6e3a1")},
        {"Environment", QColor("#89dceb")},
        {"Bash Internals", QColor("#f38ba8")},
        {"Other", QColor("#cba6f7")},
    };

    for (auto catIt = cats.cbegin(); catIt != cats.cend(); ++catIt) {
        auto* catItem = new QTreeWidgetItem(m_tree);
        catItem->setText(0, catIt.key());
        catItem->setForeground(0, catColors.value(catIt.key(), QColor("#cdd6f4")));
        QFont bf = catItem->font(0);
        bf.setBold(true);
        catItem->setFont(0, bf);
        catItem->setExpanded(openCats.isEmpty() || openCats.contains(catIt.key()));

        for (auto varIt = catIt.value().cbegin(); varIt != catIt.value().cend(); ++varIt) {
            auto* item = new QTreeWidgetItem(catItem);
            item->setText(0, varIt.key());
            item->setText(1, varIt.value());

            // Highlight changed vars
            if (m_prevVars.contains(varIt.key()) &&
                m_prevVars[varIt.key()] != varIt.value()) {
                item->setForeground(0, QColor("#f9e2af"));
                item->setForeground(1, QColor("#f9e2af"));
                item->setToolTip(1, "Was: " + m_prevVars[varIt.key()]);
            }
        }
    }
}

QString VariablePanel::category(const QString& name)
{
   /* if (BASH_INTERNAL.contains(name)) return "Bash Internals";
    if (ENV_KNOWN.contains(name))     return "Environment";
    if (name == name.toUpper())       return "Environment";
    */
    return "Variables";
}

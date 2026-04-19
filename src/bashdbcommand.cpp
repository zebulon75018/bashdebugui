

#include "bashdbcommand.h"
#include "MainWindow.h"

#include <QRegularExpression>
#include <QFileInfo>
#include <QDebug>


/* =========================
 * BashDbCommand
 * DEPRECATED , Not Used
 * ========================= */
BashDbCommand::BashDbCommand(QObject *parent,const QString& name, const QStringList& args)
    : m_name(name.trimmed())
    , m_args(args)
{

}

BashDbCommand::~BashDbCommand()
{
}

QString BashDbCommand::name() const
{
    return m_name;
}

QStringList BashDbCommand::args() const
{
    return m_args;
}

QString BashDbCommand::serialize() const
{
    if (m_args.isEmpty())
        return m_name;
    return m_name + " " + m_args.join(" ");
}

bool BashDbCommand::isValid() const
{
    return !m_name.isEmpty();
}

bool BashDbCommand::compute(const QString response)
{
    return false;
}
/* =========================
 * SimpleBashDbCommand
 * ========================= */
SimpleBashDbCommand::SimpleBashDbCommand(QObject *parent ,const QString& name, const QStringList& args)
    : BashDbCommand(parent,name, args)
{
}

SimpleBashDbCommand::~SimpleBashDbCommand()
{
}

/* =========================
 * Commandes spécialisées
 * ========================= */
BreakCommand::BreakCommand(QObject *parent,const QStringList& args)
    : BashDbCommand(parent,"break", args)
{
}

QString BreakCommand::serialize() const
{
    // Exemple : possibilité d'ajouter des règles spécifiques.
    return BashDbCommand::serialize();
}

RunCommand::RunCommand(QObject *parent,const QStringList& args)
    : BashDbCommand(parent,"run", args)
{
}

QString RunCommand::serialize() const
{
    return BashDbCommand::serialize();
}

ContinueCommand::ContinueCommand(QObject *parent,const QStringList& args)
    : BashDbCommand(parent,"continue", args)
{
}

NextCommand::NextCommand(QObject *parent,const QStringList& args)
    : BashDbCommand(parent,"next", args)
{
}


bool NextCommand::compute(const QString response)
{
    const QRegularExpression fileLineRe(QStringLiteral(R"(([A-Za-z0-9_./\-]+\.sh):([0-9]+))"));
    //auto it = fileLineRe.globalMatch(text);
    auto it = fileLineRe.globalMatch(response);
    QRegularExpressionMatch last;
    while (it.hasNext()) {
        last = it.next();
    }
    if (last.hasMatch()) {
        qDebug() << QFileInfo(last.captured(1)).absoluteFilePath() << last.captured(2).toInt();
        emit currentLocationChanged(QFileInfo(last.captured(1)).absoluteFilePath(), last.captured(2).toInt());
    }
    return true;
}

StepCommand::StepCommand(QObject *parent,const QStringList& args)
    : BashDbCommand(parent,"step", args)
{
}

bool StepCommand::compute(const QString response)
{
    qDebug() << response;
    return true;
}

PrintCommand::PrintCommand(QObject *parent,const QStringList& args)
    : BashDbCommand(parent,"print", args)
{
}

bool PrintCommand::compute(const QString response)
{
    qDebug() << response;
    return true;
}

InfoCommand::InfoCommand(QObject *parent,const QStringList& args)
    : BashDbCommand(parent,"info", args)
{
}
bool InfoCommand::compute(const QString &response)
{
    QList<QPair<QString, QString>> m_lastLocals;
const QStringList lines = response.split('\n', Qt::SkipEmptyParts);
const QRegularExpression assignRe(QStringLiteral(R"(^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*)\s*$)"));

for (const QString &line : lines) {
    const auto m = assignRe.match(line);
    if (m.hasMatch()) {
        m_lastLocals.append(qMakePair(m.captured(1), m.captured(2)));
    }

    qDebug() << m_lastLocals;
    return true;
 }
return false;
}
//emit variablesParsed(m_lastLocals, m_lastGlobals);

QuitCommand::QuitCommand(QObject *parent,const QStringList& args)
    : BashDbCommand(parent,"quit", args)
{
}

/* =========================
 * Factory
 * ========================= */

QMap<QString, QString> BashDbCommandFactory::buildAliasMap()
{
    QMap<QString, QString> m;

    // Canonical + alias utiles
    m["action"] = "action";
    m["alias"] = "alias";
    m["backtrace"] = "backtrace";
    m["bt"] = "backtrace";
    m["break"] = "break";
    m["b"] = "break";
    m["clear"] = "clear";
    m["commands"] = "commands";
    m["complete"] = "complete";
    m["condition"] = "condition";
    m["continue"] = "continue";
    m["c"] = "continue";
    m["debug"] = "debug";
    m["delete"] = "delete";
    m["disable"] = "disable";
    m["display"] = "display";
    m["down"] = "down";
    m["edit"] = "edit";
    m["enable"] = "enable";
    m["eval"] = "eval";
    m["examine"] = "examine";
    m["x"] = "examine";
    m["export"] = "export";
    m["file"] = "file";
    m["finish"] = "finish";
    m["frame"] = "frame";
    m["handle"] = "handle";
    m["help"] = "help";
    m["history"] = "history";
    m["info"] = "info";
    m["kill"] = "kill";
    m["list"] = "list";
    m["l"] = "list";
    m["load"] = "load";
    m["next"] = "next";
    m["n"] = "next";
    m["print"] = "print";
    m["p"] = "print";
    m["pwd"] = "pwd";
    m["quit"] = "quit";
    m["q"] = "quit";
    m["run"] = "run";
    m["r"] = "run";
    m["return"] = "return";
    m["reverse"] = "reverse";
    m["search"] = "search";
    m["set"] = "set";
    m["show"] = "show";
    m["shell"] = "shell";
    m["signal"] = "signal";
    m["skip"] = "skip";
    m["source"] = "source";
    m["step"] = "step";
    m["s"] = "step";
    m["step+"] = "step+";
    m["step-"] = "step-";
    m["tbreak"] = "tbreak";
    m["trace"] = "trace";
    m["unalias"] = "unalias";
    m["undisplay"] = "undisplay";
    m["untrace"] = "untrace";
    m["up"] = "up";
    m["watch"] = "watch";
    m["watche"] = "watche";

    return m;
}

QStringList BashDbCommandFactory::splitCommandLine(const QString& line)
{
    // Split simple avec support des guillemets.
    QStringList tokens;
    QRegularExpression re(R"((\"[^\"]*\"|'[^']*'|\S+))");
    QRegularExpressionMatchIterator it = re.globalMatch(line.trimmed());

    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        QString t = m.captured(1).trimmed();

        if ((t.startsWith('"') && t.endsWith('"')) ||
            (t.startsWith('\'') && t.endsWith('\''))) {
            t = t.mid(1, t.length() - 2);
        }

        tokens << t;
    }

    return tokens;
}

QString BashDbCommandFactory::normalizeCommand(const QString& command)
{
    static const QMap<QString, QString> aliases = buildAliasMap();
    const QString key = command.trimmed().toLower();

    if (aliases.contains(key))
        return aliases.value(key);

    return key;
}

bool BashDbCommandFactory::isKnownCommand(const QString& command)
{
    static const QMap<QString, QString> aliases = buildAliasMap();
    return aliases.contains(command.trimmed().toLower());
}

BashDbCommand *BashDbCommandFactory::createFromLine(QObject *parent,const QString& line)
{
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty())
        return 0X0;

    const QStringList parts = splitCommandLine(trimmed);
    if (parts.isEmpty())
        return 0X0;

    const QString cmd = parts.first();
    QStringList args = parts;
    args.removeFirst();

    return create(parent,cmd, args);
}

BashDbCommand *BashDbCommandFactory::create(QObject *parent,const QString& command,
                                                           const QStringList& args)
{
    const QString normalized = normalizeCommand(command);

    if (normalized == "break")
        return new BreakCommand(parent,args);

    if (normalized == "run")
        return new RunCommand(parent,args);

    if (normalized == "continue")
        return new ContinueCommand(parent,args);

    if (normalized == "next")
        return new NextCommand(parent,args);

    if (normalized == "step")
        return new StepCommand(parent,args);

    if (normalized == "print")
        return new PrintCommand(parent,args);

    if (normalized == "info")
        return new InfoCommand(parent,args);

    if (normalized == "quit")
        return new QuitCommand(parent,args);

    // Toutes les autres commandes connues passent par la commande simple
    static const QMap<QString, QString> aliases = buildAliasMap();
    if (aliases.values().contains(normalized)) {
        return new SimpleBashDbCommand(parent,normalized, args);
    }

    // Commande inconnue : on la garde quand même telle quelle
    return new SimpleBashDbCommand(parent,command.trimmed(), args);
}


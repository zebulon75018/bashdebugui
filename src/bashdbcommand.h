#ifndef BASHDBCOMMAND_H
#define BASHDBCOMMAND_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>

/*
 * Représente une commande bashdb.
 * Chaque commande connaît :
 *  - son nom canonique
 *  - ses arguments
 *  - comment reconstruire la ligne à envoyer à bashdb
 *  DEPRECATED , Not Used
 */
class BashDbCommand : public QObject
{
  Q_OBJECT

public:
    explicit BashDbCommand(QObject *parent = nullptr,const QString& name = QString(),
                           const QStringList& args = QStringList());
    virtual ~BashDbCommand();

    QString name() const;
    QStringList args() const;

    virtual bool compute(const QString response);

    virtual QString serialize() const;
    virtual bool isValid() const;

signals:
    void currentLocationChanged(const QString &filePath, int line);


protected:
    QString m_name;
    QStringList m_args;
};

/*
 * Classe générique pour la majorité des commandes.
 */
class SimpleBashDbCommand : public BashDbCommand
{
public:
    explicit SimpleBashDbCommand(QObject *parent,const QString& name = QString(),
                                 const QStringList& args = QStringList());
    ~SimpleBashDbCommand() override;
};

/*
 * Quelques commandes spécialisées si tu veux leur ajouter
 * un comportement particulier plus tard.
 */
class BreakCommand : public BashDbCommand
{
public:
    explicit BreakCommand(QObject *parent,const QStringList& args = QStringList());
    QString serialize() const override;
};

class RunCommand : public BashDbCommand
{
public:
    explicit RunCommand(QObject *parent,const QStringList& args = QStringList());
    QString serialize() const override;
};

class ContinueCommand : public BashDbCommand
{
public:
    explicit ContinueCommand(QObject *parent,const QStringList& args = QStringList());
};

class NextCommand : public BashDbCommand
{
public:
    explicit NextCommand(QObject *parent,const QStringList& args = QStringList());
    bool compute(const QString response);
};

class StepCommand : public BashDbCommand
{
public:
    explicit StepCommand(QObject *parent,const QStringList& args = QStringList());
    bool compute(const QString response);
};

class PrintCommand : public BashDbCommand
{
public:
    explicit PrintCommand(QObject *parent,const QStringList& args = QStringList());
    bool compute(const QString response);
};

class InfoCommand : public BashDbCommand
{
public:
    explicit InfoCommand(QObject *parent,const QStringList& args = QStringList());
    bool compute(const QString &response);
};

class QuitCommand : public BashDbCommand
{
public:
    explicit QuitCommand(QObject *parent,const QStringList& args = QStringList());
};

/*
 * Factory de construction.
 */
class BashDbCommandFactory
{
public:
    static BashDbCommand* createFromLine(QObject *parent,const QString& line);
    static BashDbCommand* create(QObject *parent,const QString& command,
                                                const QStringList& args = QStringList());

    static bool isKnownCommand(const QString& command);
    static QString normalizeCommand(const QString& command);

private:
    static QStringList splitCommandLine(const QString& line);
    static QMap<QString, QString> buildAliasMap();
};

#endif // BASHDBCOMMAND_H

/*#ifndef BASHDBCOMMAND_H
#define BASHDBCOMMAND_H

#include <QObject>

class BashDBCommand : public QObject
{
    Q_OBJECT
public:
    explicit BashDBCommand(QObject *parent = nullptr);

signals:
};

#endif // BASHDBCOMMAND_H
*/

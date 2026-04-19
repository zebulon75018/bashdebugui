#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPlainTextEdit>
#include <QProcess>
#include <QSet>
#include <QHash>
#include <QTreeWidget>
#include <QTableWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QToolButton>
#include <QSplitter>
#include <QTabWidget>
#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QLabel>
#include <QPushButton>
#include <QVariantMap>
#include <QVector>
#include <QTextDocument>

#include "bashdbcommand.h"
#include "variablepanel.h"

class CodeEditor;
class LineNumberArea;

class AppLogger : public QObject
{
    Q_OBJECT
public:
    static AppLogger &instance();
    void setLogFile(const QString &filePath);
    void log(const QString &message, const QString &category = QStringLiteral("INFO"));
    QString logFilePath() const { return m_logFilePath; }

signals:
    void logMessage(const QString &formattedMessage);

private:
    explicit AppLogger(QObject *parent = nullptr);
    QString m_logFilePath;
};

class BashSyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
public:
    explicit BashSyntaxHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<HighlightRule> m_rules;
    QTextCharFormat m_commentFormat;
    QTextCharFormat m_stringFormat;
};

class BashDbDebugger : public QObject
{
    Q_OBJECT
public:
    explicit BashDbDebugger(QObject *parent = nullptr);
    ~BashDbDebugger() override;

    void setScriptFile(const QString &scriptFile);
    QString scriptFile() const { return m_scriptFile; }
    bool isRunning() const;
    bool hasPrompt() const { return m_hasPrompt; }

    void start();
    void stop();
    void sendCommand(const QString &command, bool trackForRefresh = false);
    void requestRefresh();

    static QString stripAnsi(QString input);

signals:
    void consoleOutput(const QString &text);
    void debuggerStateChanged(const QString &state);
    void currentLocationChanged(const QString &filePath, int line);
    void promptReady();
    void variablesParsed(const QList<QPair<QString, QString>> &locals,
                         const QList<QPair<QString, QString>> &globals);
    void backtraceParsed(const QStringList &frames);
    void breakpointsParsed(const QList<QVariantMap> &breakpoints);
    void errorOccurred(const QString &message);

public slots:
    void onCurrentLocationChanged(const QString &filePath, int line);

private slots:
    void onReadyReadStdout();
    void onReadyReadStderr();
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);



private:
    void appendOutput(const QString &chunk, bool isErrorChannel = false);
    void processAccumulatedOutput();
    void parseLocation(const QString &text);
    void parseCurrentResponse(const QString &response);
    void parseInfoLocals(const QString &response);
    void parsePrintGlobals(const QString &response);
    void parseBacktrace(const QString &response);
    void parseBreakpoints(const QString &response);
    void updateState(const QString &state);
    QString sanitizeResponse(const QString &response) const;    
    void sendCommandObject(BashDbCommand* command,QString &trimmed);


    QProcess *m_process;
    QString m_scriptFile;
    QString m_stdoutBuffer;
    QString m_stderrBuffer;
    QString m_pendingResponse;
    QStringList m_commandQueue;
    bool m_hasPrompt;
    bool m_refreshRequested;
    bool m_collectGlobals;
    QList<QPair<QString, QString>> m_lastLocals;
    QList<QPair<QString, QString>> m_lastGlobals;

    QString m_lastcmd;
    BashDbCommand* cmd;
};

class LineNumberArea : public QWidget
{
public:
    explicit LineNumberArea(CodeEditor *editor);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    CodeEditor *m_editor;
};

class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT
public:
    explicit CodeEditor(QWidget *parent = nullptr);

    int lineNumberAreaWidth() const;
    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineForY(int y) const;
    void setBreakpoints(const QSet<int> &breakpoints);
    QSet<int> breakpoints() const { return m_breakpoints; }
    void toggleBreakpointAtLine(int line);
    void setCurrentExecutionLine(int line);
    int currentExecutionLine() const { return m_currentExecutionLine; }

signals:
    void breakpointToggled(int line, bool enabled);

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void updateLineNumberArea(const QRect &rect, int dy);
    void highlightCurrentLine();

private:
    QWidget *m_lineNumberArea;
    QSet<int> m_breakpoints;
    int m_currentExecutionLine;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void loadScriptFromArgument(const QString &path);

private slots:
    void chooseScript();
    void loadScriptFile(const QString &filePath = QString());
    void startDebugging();
    void continueExecution();
    void stepInto();
    void nextLine();
    void runToCursor();
    void stopDebugging();
    void toggleBreakpointAtCursor();
    void sendConsoleCommand();
    void handleBreakpointToggled(int line, bool enabled);

    void onConsoleOutput(const QString &text);
    void onDebuggerStateChanged(const QString &state);
    void onCurrentLocationChanged(const QString &filePath, int line);
    void onVariablesParsed(const QList<QPair<QString, QString>> &locals,
                           const QList<QPair<QString, QString>> &globals);
    void onBacktraceParsed(const QStringList &frames);
    void onBreakpointsParsed(const QList<QVariantMap> &breakpoints);
    void onDebuggerError(const QString &message);
    void onLogMessage(const QString &message);

private:
    void createUi();
    void connectSignals();
    void applyDarkTheme();
    void populateCodeEditor();
    void setUiEnabled(bool enabled);
    void updateBreakpointViews();
    void sendBreakpointCommand(int line, bool enabled);    


    BashDbDebugger *m_debugger;
    CodeEditor *m_codeEditor;
    BashSyntaxHighlighter *m_highlighter;

    QLineEdit *m_scriptPathEdit;
    QPushButton *m_browseButton;
    QPushButton *m_runButton;
    QPushButton *m_continueButton;
    QPushButton *m_stepButton;
    QPushButton *m_nextButton;
    QPushButton *m_runToCursorButton;
    QPushButton *m_stopButton;
    QPushButton *m_toggleBreakpointButton;

    VariablePanel  *m_variablesTable; 
    //QTableWidget *m_variablesTable;
    QLineEdit *m_watchInput;
    QPushButton *m_RefreshVarButton;
    QListWidget *m_backtraceList;
    QTableWidget *m_breakpointsTable;
    QPlainTextEdit *m_consoleOutput;
    QLineEdit *m_consoleInput;
    QPlainTextEdit *m_logOutput;
    QLabel *m_statusLabel;
    QLabel *m_currentPosLabel;

    QString m_loadedFilePath;
    QSet<int> m_breakpoints;
};

#endif // MAINWINDOW_H

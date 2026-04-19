#include "MainWindow.h"

#include <QApplication>
#include <QBoxLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QScrollBar>
#include <QStatusBar>
#include <QTextBlock>
#include <QProcessEnvironment>
#include <QDir>
#include <utility>
#include <QDebug>

// ========================= AppLogger =========================

AppLogger::AppLogger(QObject *parent)
    : QObject(parent)
{
}

AppLogger &AppLogger::instance()
{
    static AppLogger logger;
    return logger;
}

void AppLogger::setLogFile(const QString &filePath)
{
    m_logFilePath = filePath;
    QFileInfo info(filePath);
    QDir().mkpath(info.absolutePath());
}

void AppLogger::log(const QString &message, const QString &category)
{
    const QString formatted = QStringLiteral("[%1] [%2] %3")
            .arg(QDateTime::currentDateTime().toString(Qt::ISODate))
            .arg(category)
            .arg(message);

    if (!m_logFilePath.isEmpty()) {
        QFile file(m_logFilePath);
        if (file.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&file);
            out << formatted << '\n';
        }
    }

    emit logMessage(formatted);
}

// ========================= BashSyntaxHighlighter =========================

BashSyntaxHighlighter::BashSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(QColor("#81a1c1"));
    keywordFormat.setFontWeight(QFont::Bold);

    const QStringList keywords = {
        QStringLiteral("\\bif\\b"), QStringLiteral("\\bthen\\b"), QStringLiteral("\\belse\\b"),
        QStringLiteral("\\belif\\b"), QStringLiteral("\\bfi\\b"), QStringLiteral("\\bfor\\b"),
        QStringLiteral("\\bwhile\\b"), QStringLiteral("\\bdo\\b"), QStringLiteral("\\bdone\\b"),
        QStringLiteral("\\bcase\\b"), QStringLiteral("\\besac\\b"), QStringLiteral("\\bfunction\\b"),
        QStringLiteral("\\bin\\b"), QStringLiteral("\\breturn\\b"), QStringLiteral("\\bbreak\\b"),
        QStringLiteral("\\bcontinue\\b"), QStringLiteral("\\blocal\\b"), QStringLiteral("\\bdeclare\\b"),
        QStringLiteral("\\bexport\\b")
    };

    for (const QString &pattern : keywords) {
        HighlightRule rule;
        rule.pattern = QRegularExpression(pattern);
        rule.format = keywordFormat;
        m_rules.push_back(rule);
    }

    QTextCharFormat variableFormat;
    variableFormat.setForeground(QColor("#b48ead"));
    m_rules.push_back({QRegularExpression(QStringLiteral(R"((\$\{?[A-Za-z_][A-Za-z0-9_]*\}?))")), variableFormat});

    QTextCharFormat commandFormat;
    commandFormat.setForeground(QColor("#88c0d0"));
    m_rules.push_back({QRegularExpression(QStringLiteral(R"(`[^`]*`)")), commandFormat});

    m_commentFormat.setForeground(QColor("#616e88"));
    m_stringFormat.setForeground(QColor("#a3be8c"));
}

void BashSyntaxHighlighter::highlightBlock(const QString &text)
{
    for (const HighlightRule &rule : std::as_const(m_rules)) {
        auto matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            const auto match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    auto strings = QRegularExpression(QStringLiteral(R"(("[^"\\]*(\\.[^"\\]*)*")|('[^']*'))"));
    auto stringIt = strings.globalMatch(text);
    while (stringIt.hasNext()) {
        const auto match = stringIt.next();
        setFormat(match.capturedStart(), match.capturedLength(), m_stringFormat);
    }

    const int commentStart = text.indexOf('#');
    if (commentStart >= 0) {
        setFormat(commentStart, text.length() - commentStart, m_commentFormat);
    }
}

// ========================= BashDbDebugger =========================

BashDbDebugger::BashDbDebugger(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_hasPrompt(false)
    , m_refreshRequested(false)
    , m_collectGlobals(false)
    , cmd(0x0)
{
    connect(m_process, &QProcess::readyReadStandardOutput, this, &BashDbDebugger::onReadyReadStdout);
    connect(m_process, &QProcess::readyReadStandardError, this, &BashDbDebugger::onReadyReadStderr);
    connect(m_process, &QProcess::started, this, &BashDbDebugger::onProcessStarted);
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &BashDbDebugger::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &BashDbDebugger::onProcessError);
}

BashDbDebugger::~BashDbDebugger()
{
    stop();
}

void BashDbDebugger::setScriptFile(const QString &scriptFile)
{
    m_scriptFile = scriptFile;
}

bool BashDbDebugger::isRunning() const
{
    return m_process->state() != QProcess::NotRunning;
}

void BashDbDebugger::start()
{
    if (m_scriptFile.isEmpty()) {
        emit errorOccurred(QStringLiteral("Aucun script sélectionné."));
        return;
    }

    if (!QFileInfo::exists(m_scriptFile)) {
        emit errorOccurred(QStringLiteral("Script introuvable : %1").arg(m_scriptFile));
        return;
    }

    if (isRunning()) {
        stop();
    }

    m_stdoutBuffer.clear();
    m_stderrBuffer.clear();
    m_pendingResponse.clear();
    m_commandQueue.clear();
    m_hasPrompt = false;
    m_refreshRequested = false;
    m_collectGlobals = false;
    m_lastLocals.clear();
    m_lastGlobals.clear();

    AppLogger::instance().log(QStringLiteral("Starting bashdb with script: %1").arg(m_scriptFile));
    updateState(QStringLiteral("starting"));

    QString program = QStringLiteral("bashdb");
    QStringList args;
    args << QStringLiteral("-q") << m_scriptFile;

    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    m_process->start(program, args);
}

void BashDbDebugger::stop()
{
    if (!isRunning())
        return;

    AppLogger::instance().log(QStringLiteral("Stopping debugger"));
    m_process->write("quit\n");
    if (!m_process->waitForFinished(700)) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

void BashDbDebugger::sendCommand(const QString &command, bool trackForRefresh)
{
    m_lastcmd = command;
    if (!isRunning()) {
        emit errorOccurred(QStringLiteral("Le debugger n'est pas démarré."));
        return;
    }

    QString trimmed = command.trimmed();
    if (trimmed.isEmpty())
        return;

    if (trackForRefresh) {
        m_refreshRequested = true;
    }

    cmd = BashDbCommandFactory::createFromLine(this,trimmed);

    if (cmd == 0X0 ) {
        //emit logMessage("Ignoring empty/invalid command");
        return;
    }

    connect(cmd, &BashDbCommand::currentLocationChanged, this, &BashDbDebugger::onCurrentLocationChanged);

    sendCommandObject(cmd , trimmed);

    AppLogger::instance().log(QStringLiteral("SEND: %1").arg(trimmed), QStringLiteral("CMD"));
    m_commandQueue << trimmed;
    m_hasPrompt = false;
/*    m_process->write((trimmed + '\n').toUtf8());
*/
}

void BashDbDebugger::onCurrentLocationChanged(const QString &filePath, int line)
{
    // TODO make it better...
      emit currentLocationChanged(filePath, line);
}

void BashDbDebugger::sendCommandObject(BashDbCommand *command, QString &trimmed)
{
    if (m_process->state() != QProcess::Running) {
        emit errorOccurred("bashdb process is not running");
        return;
    }

    AppLogger::instance().log(QStringLiteral("SEND: %1").arg(trimmed), QStringLiteral("CMD"));
    m_commandQueue << trimmed;
    m_hasPrompt = false;
    m_process->write((trimmed + '\n').toUtf8());
/*
    const QString serialized = command->serialize();
    const QByteArray data = (serialized + "\n").toLocal8Bit();

   // emit logMessage(QString("SEND: %1").arg(serialized));
    m_process->write(data);
    m_process->waitForBytesWritten(100);
*/
}

void BashDbDebugger::requestRefresh()
{
    if (!isRunning() || !m_hasPrompt)
        return;

    m_refreshRequested = false;
    sendCommand(QStringLiteral("info locals"));
    sendCommand(QStringLiteral("info variables"));
    sendCommand(QStringLiteral("where"));
    sendCommand(QStringLiteral("info breakpoints"));
}

void BashDbDebugger::onReadyReadStdout()
{
    appendOutput(QString::fromLocal8Bit(m_process->readAllStandardOutput()), false);
}

void BashDbDebugger::onReadyReadStderr()
{
    appendOutput(QString::fromLocal8Bit(m_process->readAllStandardError()), true);
}

void BashDbDebugger::onProcessStarted()
{
    updateState(QStringLiteral("running"));
    emit consoleOutput(QStringLiteral("bashdb démarré.\n"));
}

void BashDbDebugger::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus)
    AppLogger::instance().log(QStringLiteral("Debugger finished with code %1").arg(exitCode));
    updateState(QStringLiteral("stopped"));
    emit consoleOutput(QStringLiteral("\n[process finished: %1]\n").arg(exitCode));
}

void BashDbDebugger::onProcessError(QProcess::ProcessError error)
{
    QString msg;
    switch (error) {
    case QProcess::FailedToStart:
        msg = QStringLiteral("Impossible de lancer bashdb. Vérifiez qu'il est installé et présent dans le PATH.");
        break;
    case QProcess::Crashed:
        msg = QStringLiteral("Le processus bashdb a crashé.");
        break;
    default:
        msg = QStringLiteral("Erreur QProcess : %1").arg(static_cast<int>(error));
        break;
    }

    AppLogger::instance().log(msg, QStringLiteral("ERROR"));
    emit errorOccurred(msg);
    updateState(QStringLiteral("error"));
}

void BashDbDebugger::appendOutput(const QString &chunk, bool isErrorChannel)
{
    if (chunk.isEmpty())
        return;

    if (isErrorChannel) {
        m_stderrBuffer += chunk;
    } else {
        m_stdoutBuffer += chunk;
    }

    emit consoleOutput(chunk);
    AppLogger::instance().log(chunk.trimmed(), isErrorChannel ? QStringLiteral("STDERR") : QStringLiteral("STDOUT"));
    processAccumulatedOutput();
}

void BashDbDebugger::processAccumulatedOutput()
{
    QString combined = m_stdoutBuffer + m_stderrBuffer;
    if (combined.isEmpty())
        return;

    parseLocation(combined);

    if (m_lastcmd =="info variables")
    {
        parseInfoLocals(combined);
    }
    if (cmd != 0x0 ) {
        //emit logMessage("Ignoring empty/invalid command");
        cmd->compute(combined);
        // On  reset.
        cmd = 0x0;
    }




    // bashdb prompt variations: bashdb<, (bashdb) etc.
    const QRegularExpression promptRe(QStringLiteral(R"((bashdb<\d*>\s*|\(bashdb\)\s*))"));
    QRegularExpressionMatch match;
    int lastPromptIndex = -1;
    auto it = promptRe.globalMatch(combined);
    while (it.hasNext()) {
        match = it.next();
        lastPromptIndex = match.capturedStart();
    }

    if (lastPromptIndex >= 0) {
        const QString response = combined.left(lastPromptIndex);
        m_pendingResponse += response;
        parseCurrentResponse(m_pendingResponse);
        m_pendingResponse.clear();
        m_stdoutBuffer.clear();
        m_stderrBuffer.clear();
        m_hasPrompt = true;
        emit promptReady();

        if (m_refreshRequested) {
            requestRefresh();
        }
    } else {
        m_pendingResponse += combined;
        m_stdoutBuffer.clear();
        m_stderrBuffer.clear();
    }
}

QString BashDbDebugger::stripAnsi(QString input)
{
    QRegularExpression ansiRegex("\x1B\\[[0-9;]*[A-Za-z]");
    return input.remove(ansiRegex);
}

void BashDbDebugger::parseLocation(const QString &text)
{
    qDebug() << " parseLocation " << text;

    QString result = BashDbDebugger::stripAnsi(text);

    const QRegularExpression fileLineRe(QStringLiteral(R"(([A-Za-z0-9_./\-]+\.sh):([0-9]+))"));
    //auto it = fileLineRe.globalMatch(text);
    auto it = fileLineRe.globalMatch(result);
    QRegularExpressionMatch last;
    while (it.hasNext()) {
        last = it.next();
    }
    if (last.hasMatch()) {
        emit currentLocationChanged(QFileInfo(last.captured(1)).absoluteFilePath(), last.captured(2).toInt());
    }
}

QString BashDbDebugger::sanitizeResponse(const QString &response) const
{
    QString cleaned = response;

    cleaned.remove(QRegularExpression(QStringLiteral(R"(\x1b\[[0-9;]*[A-Za-z])")));
    return cleaned.trimmed();
}

void BashDbDebugger::parseCurrentResponse(const QString &response)
{
    const QString cleaned = sanitizeResponse(response);
    if (cleaned.isEmpty())
        return;



    if (!m_commandQueue.isEmpty()) {
        const QString cmd = m_commandQueue.takeFirst();
        if (cmd == QStringLiteral("info locals")) {
            parseInfoLocals(cleaned);
        } else if (cmd == QStringLiteral("info variables")) {
            parsePrintGlobals(cleaned);
        } else if (cmd == QStringLiteral("where") || cmd == QStringLiteral("bt")) {
            parseBacktrace(cleaned);
        } else if (cmd == QStringLiteral("info breakpoints")) {
            parseBreakpoints(cleaned);
        }
    }
}

void BashDbDebugger::parseInfoLocals(const QString &response)
{
    m_lastLocals.clear();
    const QStringList lines = response.split('\n', Qt::SkipEmptyParts);
    const QRegularExpression assignRe(QStringLiteral(R"(^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*)\s*$)"));

    for (const QString &line : lines) {
        const auto m = assignRe.match(line);
        if (m.hasMatch()) {
            m_lastLocals.append(qMakePair(m.captured(1), m.captured(2)));
        }
    }
    emit variablesParsed(m_lastLocals, m_lastGlobals);
}

void BashDbDebugger::parsePrintGlobals(const QString &response)
{
    m_lastGlobals.clear();
    const QStringList lines = response.split('\n', Qt::SkipEmptyParts);
    const QRegularExpression assignRe(QStringLiteral(R"(^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*)\s*$)"));
    for (const QString &line : lines) {
        const auto m = assignRe.match(line);
        if (m.hasMatch()) {
            m_lastGlobals.append(qMakePair(m.captured(1), m.captured(2)));
        }
    }
    emit variablesParsed(m_lastLocals, m_lastGlobals);
}

void BashDbDebugger::parseBacktrace(const QString &response)
{
    QStringList frames;
    const QStringList lines = response.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        if (line.contains(QLatin1Char('#')) || line.contains(QStringLiteral(" at ")) || line.contains(QStringLiteral("->"))) {
            frames << line.trimmed();
        }
    }
    emit backtraceParsed(frames);
}

void BashDbDebugger::parseBreakpoints(const QString &response)
{
    QList<QVariantMap> items;
    const QStringList lines = response.split('\n', Qt::SkipEmptyParts);
    const QRegularExpression re(QStringLiteral(R"(^\s*(\d+)\s+(breakpoint)\s+(keep|del)\s+(y|n)\s+.*?([A-Za-z0-9_./\-]+\.sh):([0-9]+).*$)"));
    for (const QString &line : lines) {
        const auto m = re.match(line);
        if (m.hasMatch()) {
            QVariantMap item;
            item[QStringLiteral("id")] = m.captured(1);
            item[QStringLiteral("enabled")] = (m.captured(4) == QStringLiteral("y"));
            item[QStringLiteral("file")] = QFileInfo(m.captured(5)).fileName();
            item[QStringLiteral("line")] = m.captured(6).toInt();
            items.append(item);
        }
    }
    emit breakpointsParsed(items);
}

void BashDbDebugger::updateState(const QString &state)
{
    emit debuggerStateChanged(state);
}

// ========================= LineNumberArea =========================

LineNumberArea::LineNumberArea(CodeEditor *editor)
    : QWidget(editor)
    , m_editor(editor)
{
}

QSize LineNumberArea::sizeHint() const
{
    return QSize(m_editor->lineNumberAreaWidth(), 0);
}

void LineNumberArea::paintEvent(QPaintEvent *event)
{
    m_editor->lineNumberAreaPaintEvent(event);
}

void LineNumberArea::mousePressEvent(QMouseEvent *event)
{
    const int line = m_editor->lineForY(event->pos().y());
    if (line > 0) {
        m_editor->toggleBreakpointAtLine(line);
    }
    QWidget::mousePressEvent(event);
}

// ========================= CodeEditor =========================

CodeEditor::CodeEditor(QWidget *parent)
    : QPlainTextEdit(parent)
    , m_lineNumberArea(new LineNumberArea(this))
    , m_currentExecutionLine(-1)
{
    setLineWrapMode(QPlainTextEdit::NoWrap);
    updateLineNumberAreaWidth(0);

    connect(this, &QPlainTextEdit::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);

    highlightCurrentLine();
}

int CodeEditor::lineNumberAreaWidth() const
{
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    return 24 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits + 16;
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(m_lineNumberArea);
    painter.fillRect(event->rect(), QColor("#2b303b"));

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = static_cast<int>(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + static_cast<int>(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            const int lineNumber = blockNumber + 1;

            if (m_breakpoints.contains(lineNumber)) {
                painter.setBrush(QColor("#bf616a"));
                painter.setPen(Qt::NoPen);
                painter.drawEllipse(4, top + 3, 10, 10);
            }

            if (lineNumber == m_currentExecutionLine) {

                QRect lineRect(0, top, m_lineNumberArea->width(), fontMetrics().height());

                painter.setPen(QColor("#ffff00"));
                QString arrow = "▶";
                painter.drawText(lineRect.adjusted(18, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, arrow);
                /*
                painter.fillRect(0, top, m_lineNumberArea->width(), fontMetrics().height() + 4, QColor(235, 203, 139, 35));
                painter.setBrush(QColor("#ebcb8b"));
                painter.setPen(Qt::NoPen);
                static const QPoint arrow[3] = { QPoint(16, top + 3), QPoint(22, top + 8), QPoint(16, top + 13) };
                painter.drawPolygon(arrow, 3);
                painter.setPen(QColor("#ebcb8b"));
                QFont f = painter.font();
                f.setWeight(QFont::Bold);
                painter.setFont(f);
                */
                //painter.setFont(QFont(font(), QFont::Bold));
            } else {
                painter.setPen(QColor("#d8dee9"));
                painter.setFont(font());
            }

            painter.drawText(28, top, m_lineNumberArea->width() - 4, fontMetrics().height(),
                             Qt::AlignLeft, QString::number(lineNumber));
        }

        block = block.next();
        top = bottom;
        bottom = top + static_cast<int>(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

int CodeEditor::lineForY(int y) const
{
    QTextBlock block = firstVisibleBlock();
    int top = static_cast<int>(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + static_cast<int>(blockBoundingRect(block).height());

    while (block.isValid()) {
        if (y >= top && y <= bottom)
            return block.blockNumber() + 1;
        block = block.next();
        top = bottom;
        bottom = top + static_cast<int>(blockBoundingRect(block).height());
    }
    return -1;
}

void CodeEditor::setBreakpoints(const QSet<int> &breakpoints)
{
    m_breakpoints = breakpoints;
    viewport()->update();
    m_lineNumberArea->update();
}

void CodeEditor::toggleBreakpointAtLine(int line)
{
    if (line <= 0)
        return;

    const bool enabled = !m_breakpoints.contains(line);
    if (enabled)
        m_breakpoints.insert(line);
    else
        m_breakpoints.remove(line);

    emit breakpointToggled(line, enabled);
    m_lineNumberArea->update();
}

void CodeEditor::setCurrentExecutionLine(int line)
{
    m_currentExecutionLine = line;

    QTextBlock block = document()->findBlockByNumber(line - 1);
    if (block.isValid()) {
        QTextCursor cursor(block);
        setTextCursor(cursor);
        centerCursor();
    }

    highlightCurrentLine();
    m_lineNumberArea->update();
}

void CodeEditor::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
    QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::updateLineNumberAreaWidth(int)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        m_lineNumberArea->scroll(0, dy);
    else
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void CodeEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> selections;

    QTextEdit::ExtraSelection cursorLineSelection;
    cursorLineSelection.format.setBackground(QColor(90, 120, 90, 90));
    cursorLineSelection.format.setProperty(QTextFormat::FullWidthSelection, true);
    cursorLineSelection.cursor = textCursor();
    cursorLineSelection.cursor.clearSelection();
    selections.append(cursorLineSelection);

    if (m_currentExecutionLine > 0) {
        QTextBlock block = document()->findBlockByNumber(m_currentExecutionLine - 1);
        if (block.isValid()) {
            QTextEdit::ExtraSelection execSelection;
            execSelection.format.setBackground(QColor(235, 203, 139, 110));
            execSelection.format.setProperty(QTextFormat::FullWidthSelection, true);
            execSelection.format.setProperty(QTextFormat::TextOutline, QPen(QColor("#ebcb8b")));
            execSelection.cursor = QTextCursor(block);
            execSelection.cursor.clearSelection();
            selections.append(execSelection);
        }
    }

    setExtraSelections(selections);
}

// ========================= MainWindow =========================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_debugger(new BashDbDebugger(this))
    , m_codeEditor(nullptr)
    , m_highlighter(nullptr)
{
    AppLogger::instance().setLogFile(QDir::homePath() + QStringLiteral("/.cache/bashdb-gui/bashdb-gui.log"));

    createUi();
    applyDarkTheme();
    connectSignals();

    setWindowTitle(QStringLiteral("BashDB GUI Frontend"));
    resize(1400, 900);
    statusBar()->showMessage(QStringLiteral("Prêt"));
    m_statusLabel->setText(QStringLiteral("stopped"));
    m_currentPosLabel->setText(QStringLiteral("ligne: --"));

    setUiEnabled(true);
}

MainWindow::~MainWindow() = default;

void MainWindow::createUi()
{
    auto *central = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(8);

    auto *fileBarLayout = new QHBoxLayout();
    m_scriptPathEdit = new QLineEdit(this);
    m_scriptPathEdit->setPlaceholderText(QStringLiteral("Choisir un script Bash (.sh)"));
    m_browseButton = new QPushButton(QStringLiteral("Browse"), this);
    fileBarLayout->addWidget(new QLabel(QStringLiteral("Script:"), this));
    fileBarLayout->addWidget(m_scriptPathEdit, 1);
    fileBarLayout->addWidget(m_browseButton);
    rootLayout->addLayout(fileBarLayout);

    auto *controlsLayout = new QHBoxLayout();
    m_runButton = new QPushButton(QStringLiteral("Run"), this);
    m_continueButton = new QPushButton(QStringLiteral("Continue"), this);
    m_stepButton = new QPushButton(QStringLiteral("Step"), this);
    m_nextButton = new QPushButton(QStringLiteral("Next"), this);
    m_runToCursorButton = new QPushButton(QStringLiteral("Run to cursor"), this);
    m_stopButton = new QPushButton(QStringLiteral("Stop / Quit"), this);
    m_toggleBreakpointButton = new QPushButton(QStringLiteral("Toggle breakpoint"), this);
    controlsLayout->addWidget(m_runButton);
    controlsLayout->addWidget(m_continueButton);
    controlsLayout->addWidget(m_stepButton);
    controlsLayout->addWidget(m_nextButton);
    controlsLayout->addWidget(m_runToCursorButton);
    controlsLayout->addWidget(m_toggleBreakpointButton);
    controlsLayout->addWidget(m_stopButton);
    controlsLayout->addStretch(1);
    rootLayout->addLayout(controlsLayout);

    auto *splitter = new QSplitter(Qt::Horizontal, this);

    m_codeEditor = new CodeEditor(this);
    QFont mono(QStringLiteral("Monospace"));
    mono.setStyleHint(QFont::TypeWriter);
    mono.setPointSize(11);
    m_codeEditor->setFont(mono);
    m_highlighter = new BashSyntaxHighlighter(m_codeEditor->document());
    splitter->addWidget(m_codeEditor);

    auto *tabs = new QTabWidget(this);

    auto *variablesTab = new QWidget(this);
    auto *variablesLayout = new QVBoxLayout(variablesTab);
    m_variablesTable = new VariablePanel(this);
    //m_variablesTable = new QTableWidget(0, 3, this);
    //m_variablesTable->setHorizontalHeaderLabels({QStringLiteral("Scope"), QStringLiteral("Name"), QStringLiteral("Value")});
    //m_variablesTable->horizontalHeader()->setStretchLastSection(true);
    //m_variablesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    //m_watchInput = new QLineEdit(this);
    //m_watchInput->setPlaceholderText(QStringLiteral("Watch / print expression"));
    m_RefreshVarButton = new QPushButton(QStringLiteral("Refresh"), this);
    auto *watchLayout = new QHBoxLayout();
    //watchLayout->addWidget(m_watchInput, 1);
    watchLayout->addWidget(m_RefreshVarButton);
    variablesLayout->addLayout(watchLayout);
    variablesLayout->addWidget(m_variablesTable);
    tabs->addTab(variablesTab, QStringLiteral("Variables & Watch"));

    auto *backtraceTab = new QWidget(this);
    auto *backtraceLayout = new QVBoxLayout(backtraceTab);
    m_backtraceList = new QListWidget(this);
    backtraceLayout->addWidget(m_backtraceList);
    //tabs->addTab(backtraceTab, QStringLiteral("Call Stack"));

    auto *breakpointsTab = new QWidget(this);
    auto *breakpointsLayout = new QVBoxLayout(breakpointsTab);
    m_breakpointsTable = new QTableWidget(0, 4, this);
    m_breakpointsTable->setHorizontalHeaderLabels({QStringLiteral("ID"), QStringLiteral("Enabled"), QStringLiteral("File"), QStringLiteral("Line")});
    m_breakpointsTable->horizontalHeader()->setStretchLastSection(true);
    m_breakpointsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    breakpointsLayout->addWidget(m_breakpointsTable);
    //tabs->addTab(breakpointsTab, QStringLiteral("Breakpoints"));

    auto *consoleTab = new QWidget(this);
    auto *consoleLayout = new QVBoxLayout(consoleTab);
    m_consoleOutput = new QPlainTextEdit(this);
    m_consoleOutput->setReadOnly(true);
    m_consoleOutput->setFont(mono);
    m_consoleInput = new QLineEdit(this);
    m_consoleInput->setPlaceholderText(QStringLiteral("Entrer une commande bashdb ou une entrée utilisateur"));
    m_logOutput = new QPlainTextEdit(this);
    m_logOutput->setReadOnly(true);
    m_logOutput->setMaximumBlockCount(1000);
    m_logOutput->setFont(mono);
    consoleLayout->addWidget(new QLabel(QStringLiteral("Console debugger"), this));
    consoleLayout->addWidget(m_consoleOutput, 3);
    consoleLayout->addWidget(m_consoleInput);
    consoleLayout->addWidget(new QLabel(QStringLiteral("Logs internes"), this));
    consoleLayout->addWidget(m_logOutput, 2);
    tabs->addTab(consoleTab, QStringLiteral("Console & Logs"));

    splitter->addWidget(tabs);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    rootLayout->addWidget(splitter, 1);

    setCentralWidget(central);

    m_statusLabel = new QLabel(QStringLiteral("stopped"), this);
    m_currentPosLabel = new QLabel(QStringLiteral("ligne: --"), this);
    statusBar()->addPermanentWidget(new QLabel(QStringLiteral("Debugger:"), this));
    statusBar()->addPermanentWidget(m_statusLabel);
    statusBar()->addPermanentWidget(new QLabel(QStringLiteral(" | Position:"), this));
    statusBar()->addPermanentWidget(m_currentPosLabel);
}

void MainWindow::connectSignals()
{
    connect(m_browseButton, &QPushButton::clicked, this, &MainWindow::chooseScript);
    connect(m_scriptPathEdit, &QLineEdit::editingFinished, this, [this](){ loadScriptFile(m_scriptPathEdit->text().trimmed()); });
    connect(m_runButton, &QPushButton::clicked, this, &MainWindow::startDebugging);
    connect(m_continueButton, &QPushButton::clicked, this, &MainWindow::continueExecution);
    connect(m_stepButton, &QPushButton::clicked, this, &MainWindow::stepInto);
    connect(m_nextButton, &QPushButton::clicked, this, &MainWindow::nextLine);
    connect(m_runToCursorButton, &QPushButton::clicked, this, &MainWindow::runToCursor);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::stopDebugging);
    connect(m_toggleBreakpointButton, &QPushButton::clicked, this, &MainWindow::toggleBreakpointAtCursor);
    connect(m_consoleInput, &QLineEdit::returnPressed, this, &MainWindow::sendConsoleCommand);    
    connect(m_RefreshVarButton, &QPushButton::clicked, this, [this]() {
        m_debugger->sendCommand("info variables");
        /*if (!m_watchInput->text().trimmed().isEmpty()) {
            m_debugger->sendCommand(QStringLiteral("print %1").arg(m_watchInput->text().trimmed()));
        }
        */
    });

    connect(m_codeEditor, &CodeEditor::breakpointToggled, this, &MainWindow::handleBreakpointToggled);

    connect(m_debugger, &BashDbDebugger::consoleOutput, this, &MainWindow::onConsoleOutput);
    connect(m_debugger, &BashDbDebugger::debuggerStateChanged, this, &MainWindow::onDebuggerStateChanged);
    connect(m_debugger, &BashDbDebugger::currentLocationChanged, this, &MainWindow::onCurrentLocationChanged);
    connect(m_debugger, &BashDbDebugger::variablesParsed, this, &MainWindow::onVariablesParsed);
    connect(m_debugger, &BashDbDebugger::backtraceParsed, this, &MainWindow::onBacktraceParsed);
    connect(m_debugger, &BashDbDebugger::breakpointsParsed, this, &MainWindow::onBreakpointsParsed);
    connect(m_debugger, &BashDbDebugger::errorOccurred, this, &MainWindow::onDebuggerError);
    connect(m_debugger, &BashDbDebugger::promptReady, this, [this]() {
        statusBar()->showMessage(QStringLiteral("Prompt bashdb prêt"), 1500);
    });

    connect(&AppLogger::instance(), &AppLogger::logMessage, this, &MainWindow::onLogMessage);

    connect(m_breakpointsTable, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (row < 0)
            return;
        const int line = m_breakpointsTable->item(row, 3)->text().toInt();
        m_codeEditor->toggleBreakpointAtLine(line);
    });
}

void MainWindow::applyDarkTheme()
{
    qApp->setStyle(QStringLiteral("Fusion"));

    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#2e3440"));
    palette.setColor(QPalette::WindowText, QColor("#eceff4"));
    palette.setColor(QPalette::Base, QColor("#1f2430"));
    palette.setColor(QPalette::AlternateBase, QColor("#2b303b"));
    palette.setColor(QPalette::ToolTipBase, QColor("#eceff4"));
    palette.setColor(QPalette::ToolTipText, QColor("#eceff4"));
    palette.setColor(QPalette::Text, QColor("#eceff4"));
    palette.setColor(QPalette::Button, QColor("#3b4252"));
    palette.setColor(QPalette::ButtonText, QColor("#eceff4"));
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Highlight, QColor("#5e81ac"));
    palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    qApp->setPalette(palette);

    setStyleSheet(QStringLiteral(R"(
        QPlainTextEdit, QLineEdit, QListWidget, QTableWidget, QTabWidget::pane {
            border: 1px solid #4c566a;
            border-radius: 4px;
        }
        QPushButton {
            padding: 6px 10px;
            border: 1px solid #4c566a;
            border-radius: 4px;
            background-color: #3b4252;
        }
        QPushButton:hover { background-color: #434c5e; }
        QPushButton:pressed { background-color: #4c566a; }
        QHeaderView::section {
            background-color: #3b4252;
            padding: 4px;
            border: 1px solid #4c566a;
        }
    )"));
}

void MainWindow::populateCodeEditor()
{
    QFile file(m_loadedFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        onDebuggerError(QStringLiteral("Impossible d'ouvrir le script : %1").arg(m_loadedFilePath));
        return;
    }

    m_codeEditor->setPlainText(QString::fromUtf8(file.readAll()));
    statusBar()->showMessage(QStringLiteral("Script chargé : %1").arg(QFileInfo(m_loadedFilePath).fileName()), 2500);
}

void MainWindow::setUiEnabled(bool enabled)
{
    m_continueButton->setEnabled(enabled);
    m_stepButton->setEnabled(enabled);
    m_nextButton->setEnabled(enabled);
    m_runToCursorButton->setEnabled(enabled);
    m_stopButton->setEnabled(enabled);
    m_toggleBreakpointButton->setEnabled(enabled);
    m_consoleInput->setEnabled(enabled);
}

void MainWindow::updateBreakpointViews()
{
    m_codeEditor->setBreakpoints(m_breakpoints);
}

void MainWindow::sendBreakpointCommand(int line, bool enabled)
{
    if (!m_debugger->isRunning())
        return;

    if (enabled) {
        m_debugger->sendCommand(QStringLiteral("break %1:%2").arg(m_loadedFilePath).arg(line));
    } else {
        m_debugger->sendCommand(QStringLiteral("clear %1:%2").arg(m_loadedFilePath).arg(line));
    }
    m_debugger->sendCommand(QStringLiteral("info breakpoints"));
}

void MainWindow::chooseScript()
{
    const QString filePath = QFileDialog::getOpenFileName(this,
                                                          QStringLiteral("Choisir un script Bash"),
                                                          QString(),
                                                          QStringLiteral("Bash Scripts (*.sh);;All Files (*)"));
    if (!filePath.isEmpty()) {
        loadScriptFile(filePath);
    }
}

void MainWindow::loadScriptFile(const QString &filePath)
{
    const QString resolved = filePath.isEmpty() ? m_scriptPathEdit->text().trimmed() : filePath;
    if (resolved.isEmpty())
        return;

    QFileInfo info(resolved);
    if (!info.exists() || !info.isFile()) {
        onDebuggerError(QStringLiteral("Fichier invalide : %1").arg(resolved));
        return;
    }

    m_loadedFilePath = info.absoluteFilePath();
    m_scriptPathEdit->setText(m_loadedFilePath);
    m_debugger->setScriptFile(m_loadedFilePath);
    populateCodeEditor();
    AppLogger::instance().log(QStringLiteral("Loaded script file: %1").arg(m_loadedFilePath));
}

void MainWindow::startDebugging()
{
    if (m_loadedFilePath.isEmpty()) {
        loadScriptFile();
    }
    if (m_loadedFilePath.isEmpty()) {
        onDebuggerError(QStringLiteral("Veuillez charger un script avant de lancer le debugger."));
        return;
    }

    m_consoleOutput->clear();
    m_backtraceList->clear();
    //m_variablesTable->setRowCount(0);
    m_breakpointsTable->setRowCount(0);
    m_debugger->start();
}

void MainWindow::continueExecution()
{
    m_debugger->sendCommand(QStringLiteral("c"), true);
}

void MainWindow::stepInto()
{
    m_debugger->sendCommand(QStringLiteral("s"), true);
}

void MainWindow::nextLine()
{
    m_debugger->sendCommand(QStringLiteral("n"), true);
}

void MainWindow::runToCursor()
{
    if (m_loadedFilePath.isEmpty())
        return;
    const int line = m_codeEditor->textCursor().block().blockNumber() + 1;
    m_debugger->sendCommand(QStringLiteral("tbreak %1:%2").arg(m_loadedFilePath).arg(line));
    m_debugger->sendCommand(QStringLiteral("c"), true);
}

void MainWindow::stopDebugging()
{
    m_debugger->stop();
}

void MainWindow::toggleBreakpointAtCursor()
{
    const int line = m_codeEditor->textCursor().block().blockNumber() + 1;
    m_codeEditor->toggleBreakpointAtLine(line);
}

void MainWindow::sendConsoleCommand()
{
    const QString command = m_consoleInput->text().trimmed();
    if (command.isEmpty())
        return;

    m_consoleOutput->appendPlainText(QStringLiteral("> %1").arg(command));
    m_debugger->sendCommand(command, true);
    m_consoleInput->clear();
}

void MainWindow::handleBreakpointToggled(int line, bool enabled)
{
    if (enabled)
        m_breakpoints.insert(line);
    else
        m_breakpoints.remove(line);

    updateBreakpointViews();
    sendBreakpointCommand(line, enabled);
}


void MainWindow::onConsoleOutput(const QString &text)
{
    m_consoleOutput->moveCursor(QTextCursor::End);
    m_consoleOutput->insertPlainText(BashDbDebugger::stripAnsi(text));
    m_consoleOutput->verticalScrollBar()->setValue(m_consoleOutput->verticalScrollBar()->maximum());
}

void MainWindow::onDebuggerStateChanged(const QString &state)
{
    m_statusLabel->setText(state);
    statusBar()->showMessage(QStringLiteral("Etat debugger : %1").arg(state), 2000);
}

void MainWindow::onCurrentLocationChanged(const QString &filePath, int line)
{
    /*if (!m_loadedFilePath.isEmpty() && QFileInfo(filePath).absoluteFilePath() != m_loadedFilePath) {
        return;
    }*/

    m_codeEditor->setCurrentExecutionLine(line);
    m_currentPosLabel->setText(QStringLiteral("ligne: %1").arg(line));
    statusBar()->showMessage(QStringLiteral("Position courante : ligne %1").arg(line), 2000);
}

void MainWindow::onVariablesParsed(const QList<QPair<QString, QString>> &locals,
                                   const QList<QPair<QString, QString>> &globals)
{
    QMap<QString,QString> vars;
    for (const auto &pair : locals) {
        vars[pair.first] = pair.second;
    }

    for (const auto &pair : globals) {
        vars[pair.first] = pair.second;
    }
    m_variablesTable->updateVariables(vars);
   /*
    m_variablesTable->setRowCount(0);
    auto addRows = [this](const QList<QPair<QString, QString>> &items, const QString &scope) {
        for (const auto &pair : items) {
            const int row = m_variablesTable->rowCount();
            m_variablesTable->insertRow(row);
            m_variablesTable->setItem(row, 0, new QTableWidgetItem(scope));
            m_variablesTable->setItem(row, 1, new QTableWidgetItem(pair.first));
            m_variablesTable->setItem(row, 2, new QTableWidgetItem(pair.second));
        }
    };
    addRows(locals, QStringLiteral("local"));
    addRows(globals, QStringLiteral("global"));
    */
}

void MainWindow::onBacktraceParsed(const QStringList &frames)
{
    m_backtraceList->clear();
    m_backtraceList->addItems(frames);
}

void MainWindow::onBreakpointsParsed(const QList<QVariantMap> &breakpoints)
{
    m_breakpointsTable->setRowCount(0);
    for (const QVariantMap &bp : breakpoints) {
        const int row = m_breakpointsTable->rowCount();
        m_breakpointsTable->insertRow(row);
        m_breakpointsTable->setItem(row, 0, new QTableWidgetItem(bp.value(QStringLiteral("id")).toString()));
        m_breakpointsTable->setItem(row, 1, new QTableWidgetItem(bp.value(QStringLiteral("enabled")).toBool() ? QStringLiteral("Yes") : QStringLiteral("No")));
        m_breakpointsTable->setItem(row, 2, new QTableWidgetItem(bp.value(QStringLiteral("file")).toString()));
        m_breakpointsTable->setItem(row, 3, new QTableWidgetItem(QString::number(bp.value(QStringLiteral("line")).toInt())));
    }
}

void MainWindow::onDebuggerError(const QString &message)
{
    AppLogger::instance().log(message, QStringLiteral("ERROR"));
    QMessageBox::critical(this, QStringLiteral("Erreur"), message);
    statusBar()->showMessage(message, 4000);
}

void MainWindow::onLogMessage(const QString &message)
{
    m_logOutput->appendPlainText(BashDbDebugger::stripAnsi(message));
}
void MainWindow::loadScriptFromArgument(const QString &path)
{
    QFile file(path);

    if (!file.exists()) {
        statusBar()->showMessage("Script not found: " + path);
        return;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        statusBar()->showMessage("Cannot open script: " + path);
        return;
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    // 📌 Charger dans l’éditeur
    m_codeEditor->setPlainText(content);

    // 📌 Sauvegarder le chemin courant
    //m_currentScript = path;

    // 📌 Mettre à jour l'UI
    setWindowTitle("BashDb GUI - " + QFileInfo(path).fileName());
    statusBar()->showMessage("Loaded: " + path);

    // 📌 Optionnel : préparer bashdb automatiquement
    // startDebugger();  // ← si tu veux auto-start
}

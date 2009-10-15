/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** Commercial Usage
**
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at http://qt.nokia.com/contact.
**
**************************************************************************/

#ifndef DEBUGGER_GDBENGINE_H
#define DEBUGGER_GDBENGINE_H

#include "idebuggerengine.h"
#include "debuggermanager.h" // only for StartParameters
#include "gdbmi.h"
#include "watchutils.h"

#include <QtCore/QByteArray>
#include <QtCore/QHash>
#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QProcess>
#include <QtCore/QPoint>
#include <QtCore/QSet>
#include <QtCore/QTextCodec>
#include <QtCore/QTime>
#include <QtCore/QVariant>

QT_BEGIN_NAMESPACE
class QAction;
class QAbstractItemModel;
class QWidget;
class QMainWindow;
QT_END_NAMESPACE

namespace Debugger {
class DebuggerManager;
namespace Internal {

class AbstractGdbAdapter;
class GdbResponse;
class GdbMi;

class BreakpointData;
class WatchData;

class AttachGdbAdapter;
class CoreGdbAdapter;
class PlainGdbAdapter;
class RemoteGdbAdapter;
class TrkGdbAdapter;
struct TrkOptions;

enum DebuggingHelperState
{
    DebuggingHelperUninitialized,
    DebuggingHelperLoadTried,
    DebuggingHelperAvailable,
    DebuggingHelperUnavailable,
};

class GdbEngine : public IDebuggerEngine
{
    Q_OBJECT

public:
    explicit GdbEngine(DebuggerManager *manager);
    ~GdbEngine();

private:
    friend class AbstractGdbAdapter;
    friend class AttachGdbAdapter;
    friend class CoreGdbAdapter;
    friend class PlainGdbAdapter;
    friend class RemoteGdbAdapter;
    friend class TrkGdbAdapter;

private: ////////// General Interface //////////

    virtual void addOptionPages(QList<Core::IOptionsPage*> *opts) const;

    virtual bool checkConfiguration(int toolChain, QString *errorMessage, QString *settingsPage= 0) const;

    virtual bool isGdbEngine() const { return true; }

    virtual void startDebugger(const DebuggerStartParametersPtr &sp);
    virtual void exitDebugger();
    virtual void detachDebugger();
    virtual void shutdown();

    virtual void executeDebuggerCommand(const QString &command);

private: ////////// General State //////////

    void initializeVariables();
    DebuggerStartMode startMode() const;
    const DebuggerStartParameters &startParameters() const
        { return *m_startParameters; }
    Q_SLOT void setAutoDerefPointers(const QVariant &on);

    DebuggerStartParametersPtr m_startParameters;
    QSharedPointer<TrkOptions> m_trkOptions;

private: ////////// Gdb Process Management //////////

    AbstractGdbAdapter *createAdapter(const DebuggerStartParametersPtr &dp);
    void connectAdapter();
    void startInferior();

    void gdbInputAvailable(int channel, const QString &msg)
    { m_manager->showDebuggerInput(channel, msg); }
    void gdbOutputAvailable(int channel, const QString &msg)
    { m_manager->showDebuggerOutput(channel, msg); }

private slots:
    void readGdbStandardOutput();
    void readGdbStandardError();
    void readDebugeeOutput(const QByteArray &data);

    void handleAdapterStarted();
    void handleAdapterStartFailed(const QString &msg, const QString &settingsIdHint = QString());

    void handleInferiorStartFailed(const QString &msg);
    void handleInferiorShutDown();
    void handleInferiorShutdownFailed(const QString &msg);

    void handleAdapterCrashed(const QString &msg);
    void handleAdapterShutDown();
    void handleAdapterShutdownFailed(const QString &msg);

private:
    QTextCodec *m_outputCodec;
    QTextCodec::ConverterState m_outputCodecState;

    QByteArray m_inbuffer;

    AbstractGdbAdapter *m_gdbAdapter;

private: ////////// Gdb Command Management //////////

    public: // otherwise the Qt flag macros are unhappy
    enum GdbCommandFlag {
        NoFlags = 0,
        NeedsStop = 1,    // The command needs a stopped inferior
        Discardable = 2,  // No need to wait for the reply before continuing inferior
        RebuildModel = 4, // Trigger model rebuild when no such commands are pending any more
        WatchUpdate = Discardable | RebuildModel,
        EmbedToken = 8,   // Expand %1 in the command to the command token
        RunRequest = 16,  // Callback expect GdbResultRunning instead of GdbResultDone
        ExitRequest = 32  // Callback expect GdbResultExit instead of GdbResultDone
    };
    Q_DECLARE_FLAGS(GdbCommandFlags, GdbCommandFlag)
    private:

    typedef void (GdbEngine::*GdbCommandCallback)
        (const GdbResponse &response);
    typedef void (AbstractGdbAdapter::*AdapterCallback)
        (const GdbResponse &response);

    struct GdbCommand
    {
        GdbCommand()
            : flags(0), callback(0), adapterCallback(0), callbackName(0)
        {}

        int flags;
        GdbCommandCallback callback;
        AdapterCallback adapterCallback;
        const char *callbackName;
        QString command;
        QVariant cookie;
        QTime postTime;
    };

    // type and cookie are sender-internal data, opaque for the "event
    // queue". resultNeeded == true increments m_pendingResults on
    // send and decrements on receipt, effectively preventing
    // watch model updates before everything is finished.
    void flushCommand(const GdbCommand &cmd);
    void postCommand(const QString &command,
                     GdbCommandFlags flags,
                     GdbCommandCallback callback = 0,
                     const char *callbackName = 0,
                     const QVariant &cookie = QVariant());
    void postCommand(const QString &command,
                     GdbCommandCallback callback = 0,
                     const char *callbackName = 0,
                     const QVariant &cookie = QVariant());
    void postCommand(const QString &command,
                     AdapterCallback callback,
                     const char *callbackName,
                     const QVariant &cookie = QVariant());
    void postCommand(const QString &command,
                     GdbCommandFlags flags,
                     AdapterCallback callback,
                     const char *callbackName,
                     const QVariant &cookie = QVariant());
    void postCommandHelper(const GdbCommand &cmd);
    void setTokenBarrier();

    QHash<int, GdbCommand> m_cookieForToken;

    QByteArray m_pendingConsoleStreamOutput;
    QByteArray m_pendingLogStreamOutput;

    // contains the first token number for the current round
    // of evaluation. Responses with older tokens are considers
    // out of date and discarded.
    int m_oldestAcceptableToken;

    int m_pendingRequests; // Watch updating commands in flight

    typedef void (GdbEngine::*CommandsDoneCallback)();
    // function called after all previous responses have been received
    CommandsDoneCallback m_commandsDoneCallback;

    QList<GdbCommand> m_commandsToRunOnTemporaryBreak;

private: ////////// Gdb Output, State & Capability Handling //////////

    void handleResponse(const QByteArray &buff);
    void handleStopResponse(const GdbMi &data);
    void handleResultRecord(const GdbResponse &response);
    void handleStop1(const GdbResponse &response);
    void handleStop1(const GdbMi &data);
    void handleStop2(const GdbResponse &response);
    void handleStop2(const GdbMi &data);
    StackFrame parseStackFrame(const GdbMi &mi, int level);

    virtual bool isSynchroneous() const;
    bool supportsThreads() const;

    // Gdb initialization sequence
    void handleShowVersion(const GdbResponse &response);
    void handleIsSynchroneous(const GdbResponse &response);

    int m_gdbVersion; // 6.8.0 is 680
    int m_gdbBuildVersion; // MAC only?
    bool m_isSynchroneous; // Can act synchroneously?

private: ////////// Inferior Management //////////

    Q_SLOT virtual void attemptBreakpointSynchronization();

    virtual void stepExec();
    virtual void stepOutExec();
    virtual void nextExec();
    virtual void stepIExec();
    virtual void nextIExec();

    void continueInferiorInternal();
    void autoContinueInferior();
    virtual void continueInferior();
    virtual void interruptInferior();

    virtual void runToLineExec(const QString &fileName, int lineNumber);
    virtual void runToFunctionExec(const QString &functionName);
//    void handleExecRunToFunction(const GdbResponse &response);
    virtual void jumpToLineExec(const QString &fileName, int lineNumber);

    void handleExecContinue(const GdbResponse &response);

    qint64 inferiorPid() const { return m_manager->inferiorPid(); }
    void handleInferiorPidChanged(qint64 pid) { manager()->notifyInferiorPidChanged(pid); }
    void maybeHandleInferiorPidChanged(const QString &pid);

private: ////////// View & Data Stuff //////////

    virtual void selectThread(int index);
    virtual void activateFrame(int index);

    void gotoLocation(const StackFrame &frame, bool setLocationMarker);

    //
    // Breakpoint specific stuff
    //
    void handleBreakList(const GdbResponse &response);
    void handleBreakList(const GdbMi &table);
    void handleBreakIgnore(const GdbResponse &response);
    void handleBreakInsert(const GdbResponse &response);
    void handleBreakInsert1(const GdbResponse &response);
    void handleBreakCondition(const GdbResponse &response);
    void handleBreakInfo(const GdbResponse &response);
    void extractDataFromInfoBreak(const QString &output, BreakpointData *data);
    void breakpointDataFromOutput(BreakpointData *data, const GdbMi &bkpt);
    void sendInsertBreakpoint(int index);

    //
    // Modules specific stuff
    //
    virtual void loadSymbols(const QString &moduleName);
    virtual void loadAllSymbols();
    virtual QList<Symbol> moduleSymbols(const QString &moduleName);
    void reloadModules();
    void handleModulesList(const GdbResponse &response);

    bool m_modulesListOutdated;

    //
    // Register specific stuff
    //
    Q_SLOT void reloadRegisters();
    void setRegisterValue(int nr, const QString &value);
    void handleRegisterListNames(const GdbResponse &response);
    void handleRegisterListValues(const GdbResponse &response);

    //
    // Disassembler specific stuff
    //
    virtual void fetchDisassembler(DisassemblerViewAgent *agent,
        const StackFrame &frame);
    void fetchDisassemblerByAddress(DisassemblerViewAgent *agent,
        bool useMixedMode);
    void handleFetchDisassemblerByLine(const GdbResponse &response);
    void handleFetchDisassemblerByAddress1(const GdbResponse &response);
    void handleFetchDisassemblerByAddress0(const GdbResponse &response);
    QString parseDisassembler(const GdbMi &lines);

    //
    // Source file specific stuff
    //
    void reloadSourceFiles();
    void handleQuerySources(const GdbResponse &response);

    QString fullName(const QString &fileName);
    // get one usable name out of these, try full names first
    QString fullName(const QStringList &candidates);

    // awful hack to keep track of used files
    QMap<QString, QString> m_shortToFullName;
    QMap<QString, QString> m_fullToShortName;

    //
    // Stack specific stuff
    //
    void updateAll();
    void handleStackListFrames(const GdbResponse &response);
    void handleStackSelectThread(const GdbResponse &response);
    void handleStackListThreads(const GdbResponse &response);
    void handleStackFrame1(const GdbResponse &response);
    void handleStackFrame2(const GdbResponse &response);
    Q_SLOT void reloadStack(bool forceGotoLocation);
    Q_SLOT void reloadFullStack();
    int currentFrame() const;

    QList<GdbMi> m_currentFunctionArgs;
    QByteArray m_firstChunk;
    QString m_currentFrame;

    //
    // Watch specific stuff
    //
    virtual void setToolTipExpression(const QPoint &mousePos, TextEditor::ITextEditor *editor, int cursorPos);

    virtual void assignValueInDebugger(const QString &expr, const QString &value);

    virtual void fetchMemory(MemoryViewAgent *agent, quint64 addr, quint64 length);
    void handleFetchMemory(const GdbResponse &response);

    virtual void watchPoint(const QPoint &);
    void handleWatchPoint(const GdbResponse &response);

    // FIXME: BaseClass. called to improve situation for a watch item
    void updateSubItem(const WatchData &data);
    void handleChildren(const WatchData &parent, const GdbMi &child,
        QList<WatchData> *insertions);

    void updateWatchData(const WatchData &data);
    Q_SLOT void updateWatchDataHelper(const WatchData &data);
    void rebuildModel();
    bool showToolTip();

    void insertData(const WatchData &data);
    void sendWatchParameters(const QByteArray &params0);
    void createGdbVariable(const WatchData &data);

    void runDebuggingHelper(const WatchData &data, bool dumpChildren);
    void runDirectDebuggingHelper(const WatchData &data, bool dumpChildren);
    bool hasDebuggingHelperForType(const QString &type) const;

    void handleVarListChildren(const GdbResponse &response);
    void handleVarListChildrenHelper(const GdbMi &child,
        const WatchData &parent);
    void handleVarCreate(const GdbResponse &response);
    void handleVarAssign(const GdbResponse &response);
    void handleEvaluateExpression(const GdbResponse &response);
    //void handleToolTip(const GdbResponse &response);
    void handleQueryDebuggingHelper(const GdbResponse &response);
    void handleDebuggingHelperValue1(const GdbResponse &response);
    void handleDebuggingHelperValue2(const GdbResponse &response);
    void handleDebuggingHelperValue3(const GdbResponse &response);
    void handleDebuggingHelperEditValue(const GdbResponse &response);
    void handleDebuggingHelperSetup(const GdbResponse &response);

    void updateLocals(const QVariant &cookie = QVariant());
    void handleStackListLocals(const GdbResponse &response);
    WatchData localVariable(const GdbMi &item,
                            const QStringList &uninitializedVariables,
                            QMap<QByteArray, int> *seen);
    void setLocals(const QList<GdbMi> &locals);
    void handleStackListArguments(const GdbResponse &response);
    void setWatchDataType(WatchData &data, const GdbMi &mi);
    void setWatchDataDisplayedType(WatchData &data, const GdbMi &mi);

    QSet<QString> m_processedNames;
    QMap<QString, QString> m_varToType;

private: ////////// Dumper Management //////////

    bool startModeAllowsDumpers() const;
    void tryLoadDebuggingHelpers();
    void tryQueryDebuggingHelpers();
    Q_SLOT void recheckDebuggingHelperAvailability();
    void connectDebuggingHelperActions();
    void disconnectDebuggingHelperActions();
    Q_SLOT void setDebugDebuggingHelpers(const QVariant &on);
    Q_SLOT void setUseDebuggingHelpers(const QVariant &on);

    const bool m_dumperInjectionLoad;
    DebuggingHelperState m_debuggingHelperState;
    QtDumperHelper m_dumperHelper;

private: ////////// Convenience Functions //////////

    QString errorMessage(QProcess::ProcessError error);
    void showMessageBox(int icon, const QString &title, const QString &text);
    void debugMessage(const QString &msg);
    QMainWindow *mainWindow() const;

};

} // namespace Internal
} // namespace Debugger

Q_DECLARE_OPERATORS_FOR_FLAGS(Debugger::Internal::GdbEngine::GdbCommandFlags)

#endif // DEBUGGER_GDBENGINE_H

#pragma once
#include <map>
#include <memory>
#include "RVClasses.h"
#include "BreakPoint.h"
#include "Monitor.h"
#include "NetworkController.h"
#include <shared_mutex>

namespace std {
    class mutex;
    class condition_variable;
}

struct RV_VMContext;
class Script;
class VMContext;

struct DebuggerInstructionInfo {
    game_instruction* instruction;
    RV_VMContext* context;
    GameState* gs;
};

extern scriptExecutionContext currentContext;
extern MissionEventType currentEventHandler;

struct BreakStateInfo {
    const DebuggerInstructionInfo* instruction{ nullptr };
    BreakPoint* bp{ nullptr };
};

enum class DebuggerState {
    Uninitialized,
    running,
    breakState,
    stepState
};

enum class VariableScope { //This is a bitflag
    invalid = 0,
    callstack = 1,
    local = 2,
    missionNamespace = 4,
    uiNamespace = 8,
    profileNamespace = 16,
    parsingNamespace = 32
};
inline VariableScope operator | (VariableScope lhs, VariableScope rhs) {
    return static_cast<VariableScope>(static_cast<std::underlying_type_t<VariableScope>>(lhs) | static_cast<std::underlying_type_t<VariableScope>>(rhs));
}

inline std::underlying_type_t<VariableScope> operator & (VariableScope lhs, VariableScope rhs) {
    return (static_cast<std::underlying_type_t<VariableScope>>(lhs) & static_cast<std::underlying_type_t<VariableScope>>(rhs));
}

struct HookIntegrity {
    bool __scriptVMConstructor{ false };
    bool __scriptVMSimulateStart{ false };
    bool __scriptVMSimulateEnd{ false };
    bool __instructionBreakpoint{ false };
    bool __worldSimulate{ false };
    bool __worldMissionEventStart{ false };
    bool __worldMissionEventEnd{ false };
    bool __onScriptError{ false };
    bool scriptPreprocDefine{ false };
    bool scriptPreprocConstr{ false };
    bool scriptAssert{ false };
    bool scriptHalt{ false };
    bool scriptEcho{ false };
    bool engineAlive{ false };
    bool enableMouse{ false };
    bool preprocRedirect{ false };
};



class Debugger {
public:
    Debugger();
    ~Debugger();

    //#TODO Variable read/write breakpoints GameInstructionVariable/GameInstructionAssignment
    void clear();
    std::shared_ptr<VMContext> getVMContext(RV_VMContext* vm);
    void writeFrameToFile(uint32_t frameCounter);
    void onInstruction(DebuggerInstructionInfo& instructionInfo);
    void dumpStackToRPT(GameState* gs);
    static auto_array<std::pair<r_string, uint32_t>> getCallstackRaw(GameState* gs);
    void onScriptError(GameState* gs);
    void onScriptAssert(GameState* gs);
    void onScriptHalt(GameState* gs);
    void checkForBreakpoint(DebuggerInstructionInfo& instructionInfo);
    void onShutdown();
    void onStartup();

    void onHalt(std::shared_ptr<std::pair<std::condition_variable, bool>> waitEvent, BreakPoint* bp, const DebuggerInstructionInfo& info, haltType type); //Breakpoint is halting engine
    void onContinue(); //Breakpoint has stopped halting
    void commandContinue(StepType stepType); //Tells Breakpoint in breakState to Stop halting
    void setGameVersion(const char* productType, const char* productVersion);
    void SerializeHookIntegrity(JsonArchive& answer);
    void onScriptEcho(r_string msg);
    void serializeScriptCommands(JsonArchive& answer);
    std::map<VariableScope, std::vector<r_string>> getAvailableVariables(VariableScope scope_);
    HookIntegrity HI;
    GameState* lastKnownGameState;

    void setHookIntegrity(HookIntegrity hi) { HI = hi; }

    struct VariableInfo {
        VariableInfo() {}
        VariableInfo(const game_variable* _var, VariableScope _ns) : var(_var), ns(_ns) {};
        VariableInfo(r_string _name) : var(nullptr), ns(VariableScope::invalid), notFoundName(_name) {};
        const game_variable* var;
        VariableScope ns;
        r_string notFoundName;
        void Serialize(JsonArchive& ar) const;
    };
    std::vector<VariableInfo> getVariables(VariableScope, std::vector<std::string>& varName) const;
    void grabCurrentCode(JsonArchive& answer,const std::string& file) const;
    std::map<uintptr_t, std::shared_ptr<VMContext>> VMPtrToScript;
    //std::map<std::string, std::vector<BreakPoint>> breakPoints;
    class breakPointList : public std::vector<BreakPoint> {
    public:
        breakPointList() {}
        // breakPointList(const breakPointList& b) = delete;// : _name(b._name) { for (auto &it : b) emplace_back(std::move(it)); } //std::vector like's to still copy while passing rvalue-ref
        //breakPointList(BreakPoint& b) : _name(b.filename) {push_back(std::move(b)); }
        breakPointList(BreakPoint&& b) noexcept : _name(b.filename) { push_back(std::move(b)); }
        breakPointList& operator=(breakPointList&& b) noexcept {
            _name = (b._name); for (auto &it : b) emplace_back(std::move(it));
            return *this;
        }
        r_string _name;
        const char* get_map_key() const { return _name.c_str(); }
        breakPointList(breakPointList& b) {
            if (!b._name.empty() && !_name.empty()) __debugbreak();
            _name = (b._name); for (auto &&it : b) emplace_back(std::move(it));
        }
    private:

        // disable copying

        breakPointList& operator=(const breakPointList&);


    };
    std::shared_mutex breakPointsLock;
    map_string_to_class<breakPointList, auto_array<breakPointList>> breakPoints; //All inputs have to be tolowered before accessing
    std::vector<std::shared_ptr<IMonitorBase>> monitors;
    NetworkController nController;
    std::shared_ptr<std::pair<std::condition_variable, bool>> breakStateContinueEvent;
    DebuggerState state{ DebuggerState::Uninitialized };
    BreakStateInfo breakStateInfo;
    struct {
        StepType stepType;
        uint16_t stepLine;
        uint8_t stepLevel;
        RV_VMContext* context;
    } stepInfo;
    struct productInfoStruct {
        r_string gameType;
        r_string gameVersion;
        void Serialize(JsonArchive &ar);
    } productInfo;
};


#include <fpp-pch.h>

#include <unistd.h>
#include <ifaddrs.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <fstream>
#include <list>
#include <vector>
#include <sstream>
#ifndef PLATFORM_OSX
#include <sys/eventfd.h>
#include <fcntl.h>
#endif
#include <cmath>
#include <cctype>
#include <mutex>
#include "Plugin.h"
#include "common.h"
#include "settings.h"
#include "commands/Commands.h"
#include "fpphttp.h"
#include "FileMonitor.h"
#include <rtmidi/RtMidi.h>
#include "log.h"
#include "util/ExpressionProcessor.cpp"

// MIDI variable names for expression evaluation.
// Keep legacy b1-b5 names for backward compatibility with existing plugin configs.
#define NUM_VARS 13
enum MIDIVarIndex {
    VAR_B1 = 0,
    VAR_B2,
    VAR_B3,
    VAR_B4,
    VAR_B5,
    VAR_NOTE,
    VAR_CHANNEL,
    VAR_PITCH,
    VAR_VELOCITY,
    VAR_NOTE_VAR,
    VAR_CHANNEL_VAR,
    VAR_VELOCITY_VAR,
    VAR_CONTROL
};
const char* vNames[NUM_VARS] = {
    "b1", "b2", "b3", "b4", "b5",
    "note", "channel", "pitch", "velocity",
    "note_var", "channel_var", "velocity_var", "control"
};

class MIDIInputEvent {
public:
    MIDIInputEvent(const std::vector<unsigned char> &m) : params(m) {
    }
    // Fixed-width decimal columns, one for each possible MIDI byte, so the
    // Last Messages panel can show a matching header row. Bytes that were not
    // present in the message are blank rather than 0 - a real 0 byte is common
    // (note-off velocity) and should be distinguishable from "not sent".
    std::string toString() const {
        static const size_t NUM_COLUMNS = 5;
        std::string v;
        char buf[8];
        for (size_t i = 0; i < NUM_COLUMNS; ++i) {
            if (i < params.size()) {
                snprintf(buf, sizeof(buf), "%3u", static_cast<unsigned int>(params[i]));
            } else {
                snprintf(buf, sizeof(buf), "%3s", "");
            }
            if (i != 0) {
                v += " ";
            }
            v += buf;
        }
        // SysEx and other long messages still need to be visible. Anything
        // beyond the labelled columns goes after a separator, in decimal.
        if (params.size() > NUM_COLUMNS) {
            v += " |";
            for (size_t i = NUM_COLUMNS; i < params.size(); ++i) {
                snprintf(buf, sizeof(buf), " %u", static_cast<unsigned int>(params[i]));
                v += buf;
            }
        }
        return v;
    }

    std::vector<unsigned char> params;
};

namespace {

// Column labels for the Last Messages panel. Kept here so the header and the
// values from MIDIInputEvent::toString() use the same three-character columns
// separated by a single space.
const char* const LAST_MESSAGES_HEADER = " B1  B2  B3  B4  B5";

std::string getMidiPath(const HttpRequestPtr& req) {
    std::vector<std::string> pieces = getPathPieces(req->path());
    if (pieces.size() > 1 && pieces[0] == "MIDI") {
        return pieces[1];
    }
    if (pieces.size() > 3 && pieces[0] == "api" && pieces[1] == "plugin-apis" && pieces[2] == "MIDI") {
        return pieces[3];
    }
    return std::string();
}
}


class MIDICondition {
public:
    MIDICondition(Json::Value &v) {
        conditionType = v["condition"].asString();
        compareType = v["conditionCompare"].asString();
        std::string text = v["conditionText"].asString();
        if (text != "") {
            try {
                val = std::stoul(text, nullptr, 0);
            } catch (...) {
                val = 0;
            }
        } else {
            val = 0;
        }
    }
    
    bool matches(MIDIInputEvent &ev) {
        if (ev.params.empty()) {
            return false;
        }

        int v = 0;
        if (conditionType == "noteOn") {
            if (ev.params.size() < 2) {
                return false;
            }
            if ((ev.params[0] & 0xF0) != 0x90) {
                return false;
            }
            v = ev.params[1];
        } else if (conditionType == "noteOff") {
            if (ev.params.size() < 2) {
                return false;
            }
            if ((ev.params[0] & 0xF0) != 0x80) {
                return false;
            }
            v = ev.params[1];
        } else if (conditionType == "channel") {
            v = ev.params[0] & 0xf;
        } else if (conditionType == "velocity") {
            if (ev.params.size() < 3) {
                return false;
            }
            v = ev.params[2];
        } else if (conditionType == "control") {
            if (ev.params.size() < 2) {
                return false;
            }
            if ((ev.params[0] & 0xF0) != 0xB0) {
                return false;
            }
            v = ev.params[1];
        } else if (conditionType == "pitch") {
            if (ev.params.size() < 3) {
                return false;
            }
            if ((ev.params[0] & 0xF0) != 0xE0) {
                return false;
            }
            v = ev.params[2];
            v = v << 7; //7 bit numbers so only shift 7
            v += ev.params[1];
            v -= 0x2000; //range is -8192 - 8192
        } else {
            if (conditionType.size() < 2 || (conditionType[0] != 'p' && conditionType[0] != 'b')) {
                return false;
            }
            int idx = conditionType[1] - '1';
            if (idx < 0 || static_cast<size_t>(idx) >= ev.params.size()) {
                return false;
            }
            v = ev.params[idx];
        }
        return compare(v);
    }
    bool compare(int cv) {
        int tf = val;
        if (compareType == "=") {
            return cv == tf;
        } else if (compareType == "!=") {
            return cv != tf;
        } else if (compareType == ">=") {
            return cv >= tf;
        } else if (compareType == "<=") {
            return cv <= tf;
        } else if (compareType == ">") {
            return cv > tf;
        } else if (compareType == "<") {
            return cv < tf;
        }
        return false;
    }

    std::string conditionType;
    std::string compareType;
    uint32_t val;
};

class MIDICommandArg {
public:
    MIDICommandArg(const std::string &t) : arg(t), expression(t) {
        // TinyExpr only starts identifiers on a lowercase letter, so "=B3/127"
        // would fail to compile and evaluate to an empty string. Fold math
        // expressions to lowercase so variables are effectively case
        // insensitive. Plain strings, including %%name%% substitutions, keep
        // the case the user typed.
        if (isExpression()) {
            for (char &c : expression) {
                c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
            }
        } else {
            // Apply the same rule to %%name%% substitutions, but leave the
            // literal text around them alone: "Matrix-%%B1%%" must stay
            // "Matrix-" with only the name folded to "b1".
            size_t pos = 0;
            while ((pos = expression.find("%%", pos)) != std::string::npos) {
                size_t end = expression.find("%%", pos + 2);
                if (end == std::string::npos) {
                    break;
                }
                for (size_t i = pos + 2; i < end; ++i) {
                    expression[i] = static_cast<char>(tolower(static_cast<unsigned char>(expression[i])));
                }
                pos = end + 2;
            }
        }
    }
    ~MIDICommandArg() {
        if (processor) {
            delete processor;
        }
    }

    // Same form ExpressionProcessor treats as a math expression: a single
    // leading '=' (not '==' or '%%').
    bool isExpression() const {
        return expression.size() > 1 && expression[0] == '=' && expression[1] != '=';
    }

    std::string arg;        // as configured, kept for log messages
    std::string expression; // what is actually compiled
    std::string type;

    // A malformed formula evaluates to "" on every FPP version; see the probe
    // in MIDIEvent's constructor. When false, invoke() substitutes a safe
    // value instead of passing an empty string to a numeric command argument.
    bool valid = true;

    ExpressionProcessor *processor = nullptr;

    std::string evaluate(const std::string &tp) {
        if (processor) {
            std::string s = processor->evaluate(tp);
            return s;
        }
        return "";
    }
};

class MIDIEvent {
public:
    MIDIEvent(Json::Value &v) {
        path = v["path"].asString();
        description = v["description"].asString();
        for (int x = 0; x < v["conditions"].size(); x++) {
            conditions.push_back(MIDICondition(v["conditions"][x]));
        }

        command = v;
        command.removeMember("path");
        command.removeMember("argTypes");
        command.removeMember("args");
        command.removeMember("conditions");
        command.removeMember("description");

        if (v.isMember("args")) {
            for (int x = 0; x < v["args"].size(); x++) {
                args.push_back(MIDICommandArg(v["args"][x].asString()));
            }
        }
        if (v.isMember("argTypes")) {
            for (int x = 0; x < v["argTypes"].size() && x < args.size(); x++) {
                args[x].type = v["argTypes"][x].asString();
            }
        }
        for (auto &a : args) {
            a.processor = new ExpressionProcessor();
        }
        for (int x = 0; x < NUM_VARS; x++) {
            ExpressionProcessor::ExpressionVariable *var = new ExpressionProcessor::ExpressionVariable(vNames[x]);
            variables[x] = var;
            for (auto &a : args) {
                a.processor->bindVariable(var);
            }
        }
        // Defined values before probing the expressions below; invoke() sets
        // the real values for every message.
        for (int x = 0; x < NUM_VARS; x++) {
            variables[x]->setValue("0");
        }
        for (auto &a : args) {
            a.processor->compile(a.expression);
            if (a.isExpression() && a.processor->evaluate("string").empty()) {
                // te_compile() failure yields an empty evaluation on both old
                // and new FPP versions (older compile() even returns false
                // unconditionally), so this is a portable way to detect a bad
                // formula. Report it once here rather than at invoke time.
                a.valid = false;
                LogWarn(VB_PLUGIN, "MIDI: invalid expression '%s' - this argument will use 0\n",
                        a.arg.c_str());
            }
        }
    }
    ~MIDIEvent() {
        conditions.clear();
        args.clear();
        for (int x = 0; x < NUM_VARS; x++) {
            delete variables[x];
        }
    }
    
    bool matches(MIDIInputEvent &ev) {
        for (auto &c : conditions) {
            if (!c.matches(ev)) {
                return false;
            }
        }
        return true;
    }
    
    void invoke(MIDIInputEvent &ev) {
        auto paramOrZero = [&ev](size_t idx) {
            return ev.params.size() > idx ? static_cast<int>(ev.params[idx]) : 0;
        };

        int b1 = paramOrZero(0);
        int b2 = paramOrZero(1);
        int b3 = paramOrZero(2);
        int b4 = paramOrZero(3);
        int b5 = paramOrZero(4);

        int note = b2;
        int velocity = b3;
        int channel = b1 & 0xF;

        // pitch from MIDI LSB/MSB pair, centered around 0.
        int pitch = velocity;
        pitch = pitch << 7; //7 bit numbers so only shift 7
        pitch += note;
        pitch -= 0x2000;

        variables[VAR_B1]->setValue(std::to_string(b1));
        variables[VAR_B2]->setValue(std::to_string(b2));
        variables[VAR_B3]->setValue(std::to_string(b3));
        variables[VAR_B4]->setValue(std::to_string(b4));
        variables[VAR_B5]->setValue(std::to_string(b5));
        variables[VAR_NOTE]->setValue(std::to_string(note));
        variables[VAR_CHANNEL]->setValue(std::to_string(channel));
        variables[VAR_PITCH]->setValue(std::to_string(pitch));
        variables[VAR_VELOCITY]->setValue(std::to_string(velocity));

        // Keep newer alias names in sync.
        variables[VAR_NOTE_VAR]->setValue(std::to_string(note));
        variables[VAR_CHANNEL_VAR]->setValue(std::to_string(channel));
        variables[VAR_VELOCITY_VAR]->setValue(std::to_string(velocity));
        variables[VAR_CONTROL]->setValue(std::to_string(b2));
        
        Json::Value newCommand = command;
        for (auto &a : args) {
            std::string tp = "string";
            if (a.type == "bool" || a.type == "int") {
                tp = a.type;
            }
            
            if (!a.valid) {
                // Preserve the argument count, but never hand an empty string
                // to a command expecting a number - that is what made a typo
                // such as "=B3/127*100" take down fppd on older releases.
                newCommand["args"].append(tp == "string" ? "" : "0");
                continue;
            }
            //printf("Eval p: %s\n", a.arg.c_str());
            std::string r = a.evaluate(tp);
            //printf("        -> %s\n", r.c_str());
            newCommand["args"].append(r);
        }

        CommandManager::INSTANCE.run(newCommand);
    }
    
    std::string path;
    std::string description;
    
    std::list<MIDICondition> conditions;
    
    Json::Value command;
    std::vector<MIDICommandArg> args;
    
    std::array<ExpressionProcessor::ExpressionVariable*, NUM_VARS> variables;
};


class FPPMIDIPlugin : public FPPPlugins::Plugin, public FPPPlugins::APIProviderPlugin {
public:
    int eventFileWrite;
    int eventFileRead;
    std::vector<RtMidiIn *> midiin;
    std::list<MIDIEvent *> events;
    std::mutex queueLock;
    std::list<MIDIInputEvent> incoming;

    std::mutex lastEventsLock;
    std::list<MIDIInputEvent> lastEvents;


    FPPMIDIPlugin() : FPPPlugins::Plugin("fpp-midi"), FPPPlugins::APIProviderPlugin() {
        LogInfo(VB_PLUGIN, "Initializing MIDI Plugin\n");
#ifndef PLATFORM_OSX
        eventFileRead = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        eventFileWrite = eventFileRead;
#else
        int files[2];
        pipe(files);
        eventFileRead = files[0];
        eventFileWrite = files[1];
        fcntl(eventFileRead, F_SETFD, O_NONBLOCK);
        fcntl(eventFileWrite, F_SETFD, O_NONBLOCK);
#endif
        // This plugin's configuration is its own JSON file rather than the
        // key=value settings file FPPPlugins::Plugin watches, so the
        // monitorSettings constructor argument would not see it. Watch it
        // directly, so editing the event map takes effect without restarting
        // fppd. shutdown() gives the watch back - the callback lives here.
        //
        // Only the events are reloaded. The "ports" list decides which MIDI
        // devices are open, and reopening those means stopping and restarting
        // RtMidi's input threads; that is a heavier operation than swapping a
        // lookup table and is deliberately still left to a restart.
        std::function<void()> reload = [this]() {
            LogInfo(VB_PLUGIN, "MIDI: event map changed, reloading\n");
            loadEvents();
        };
        FileMonitor::INSTANCE.AddFile(name, FPP_DIR_CONFIG("/plugin.fpp-midi.json"), reload);

        if (FileExists(FPP_DIR_CONFIG("/plugin.fpp-midi.json"))) {
            Json::Value root;
            bool success =  LoadJsonFromFile(FPP_DIR_CONFIG("/plugin.fpp-midi.json"), root);
            if (root.isMember("events")) {
                for (int x = 0; x < root["events"].size(); x++) {
                    events.push_back(new MIDIEvent(root["events"][x]));
                }
            }
            if (root.isMember("ports")) {
                for (int x = 0; x < root["ports"].size(); x++) {
                    if (root["ports"][x]["enabled"].asBool()) {
                        std::string name = root["ports"][x]["name"].asString();
                        try {
                            RtMidiIn *mi = new RtMidiIn();
                            unsigned int nPorts = mi->getPortCount();
                            for (int x = 0; x < nPorts; x++) {
                                std::string portName = mi->getPortName(x);
                                if (portName.find(name) != std::string::npos) {
                                    mi->openPort(x);
                                    mi->setCallback(&midicallback, this);
                                    bool enSysEx = root["ports"][x]["enableSysEx"].asBool();
                                    bool enTC = root["ports"][x]["enableTimeCode"].asBool();
                                    bool enSense = root["ports"][x]["enableSense"].asBool();
                                    mi->ignoreTypes(!enSysEx, !enTC, !enSense);
                                    midiin.push_back(mi);
                                    mi = nullptr;
                                    break;
                                }
                            }
                            if (mi != nullptr) {
                                LogErr(VB_PLUGIN, "Could not initialize MIDI plugin for port %s\n", name.c_str());
                                delete mi;
                            }
                        } catch (...) {
                            LogErr(VB_PLUGIN, "Could not initialize MIDI plugin for port %s\n", name.c_str());
                        }
                    }
                }
            }
        }
    }

    // Stop MIDI input before anything else. RtMidi delivers on its own thread,
    // straight into midicallback() in this library, so a port left open is a
    // call into the plugin while it is being destroyed - and after the library
    // is unmapped, a call into nothing. cancelCallback() unhooks
    // midicallback(); closePort() stops and joins RtMidi's input thread. Both
    // have returned by the time this does, so no readiness predicate is needed
    // and the destructor below is left to do the actual freeing.
    virtual std::function<bool()> shutdown() override {
        FileMonitor::INSTANCE.RemoveFile(name, FPP_DIR_CONFIG("/plugin.fpp-midi.json"));
        for (auto a : midiin) {
            a->cancelCallback();
            a->closePort();
        }
        return nullptr;
    }

    // Swap the event map for what is on disk now. The incoming-packet path
    // walks 'events' from the main loop (ProcessPacket, via the eventfd), which
    // is where this runs too, so nothing can be mid-iteration.
    void loadEvents() {
        for (auto e : events) {
            delete e;
        }
        events.clear();
        if (FileExists(FPP_DIR_CONFIG("/plugin.fpp-midi.json"))) {
            Json::Value root;
            if (LoadJsonFromFile(FPP_DIR_CONFIG("/plugin.fpp-midi.json"), root) && root.isMember("events")) {
                for (int x = 0; x < root["events"].size(); x++) {
                    events.push_back(new MIDIEvent(root["events"][x]));
                }
            }
        }
    }

    virtual ~FPPMIDIPlugin() {
        for (auto a : midiin) {
            a->closePort(); // no-op if shutdown() already did it
            delete a;
        }
        for (auto e : events) {
            delete e;
        }
        close(eventFileRead);
        if (eventFileRead != eventFileWrite) {
            close(eventFileWrite);
        }
    }

    static void midicallback(double deltatime, std::vector< unsigned char > *message, void *userData) {
        FPPMIDIPlugin *p = (FPPMIDIPlugin*)userData;
        p->incomingPacket(message);
    }

    void incomingPacket(std::vector< unsigned char > *message) {
        LogExcess(VB_PLUGIN, "Incoming packet %d\n", message->size());
        uint64_t v = 1;
        MIDIInputEvent ev(*message);
        std::unique_lock<std::mutex> lock(queueLock);
        incoming.push_back(ev);
        write(eventFileWrite, &v, 8);
    }

    

    void handleMidi(const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
        std::string p1 = getMidiPath(req);
        if (p1 == "Last") {
            std::string v = std::string(LAST_MESSAGES_HEADER) + "\n";
            std::unique_lock<std::mutex> lock(lastEventsLock);
            for (auto &a : lastEvents) {
                v += a.toString() + "\n";
            }
            callback(makeStringResponse(v, 200));
        } else if (p1 == "Devices") {
            try {
                std::string v = "[";
                RtMidiIn *mi = new RtMidiIn();
                if (mi != nullptr) {
                    unsigned int nPorts = mi->getPortCount();
                    for (int x = 0; x < nPorts; x++) {
                        std::string portName = mi->getPortName(x);
                        if (v.size() != 1) {
                            v += ", ";
                        }
                        v += "\"" + portName + "\"";
                    }
                    delete mi;
                }
                v += "]";
                callback(makeStringResponse(v, 200, "application/json"));
            } catch (...) {
                LogErr(VB_PLUGIN, "Could not enumerate MIDI ports\n");
                callback(makeStringResponse("Error", 500));
            }
        } else {
            callback(makeStringResponse("Not Found", 404));
        }
    }


    void unregisterApis() override {
        // Neither returns until no request is inside the handler and the
        // handler itself - this plugin's code - has been destroyed, which is
        // what makes a later dlclose() safe.
        FPPPlugins::unregisterPluginApi("/MIDI/Last");
        FPPPlugins::unregisterPluginApi("/MIDI/Devices");
    }

    void registerApis() override {
        // Only the plain paths are needed: Apache rewrites api/plugin-apis/MIDI/*
        // to localhost:32322/MIDI/*, stripping the plugin-apis/ prefix, so
        // "/api/plugin-apis/MIDI/*" routes would never be reached.
        //
        // Registered through FPP rather than drogon::app() directly: drogon has
        // no route removal, so a handler registered straight with it could never
        // be withdrawn and would pin this plugin in memory for the life of fppd.
        FPPPlugins::registerPluginApi("/MIDI/Last", [this](const HttpRequestPtr& req, HttpCallback&& callback) { handleMidi(req, std::move(callback)); }, {drogon::Get});
        FPPPlugins::registerPluginApi("/MIDI/Devices", [this](const HttpRequestPtr& req, HttpCallback&& callback) { handleMidi(req, std::move(callback)); }, {drogon::Get});
    }

    void addControlCallbacks(std::map<int, std::function<bool(int)>>& callbacks) override {
        callbacks[eventFileRead] = [this](int fd) {
            return ProcessPacket(fd);
        };
    }

    bool ProcessPacket(int i) {
        char buf[256];
        ssize_t s = read(eventFileRead, buf, 256);
        while (s > 0) {
            fcntl(eventFileRead, F_SETFL, O_NONBLOCK);
            s = read(eventFileRead, buf, 256);
        }
        std::unique_lock<std::mutex> lock(queueLock);
        LogExcess(VB_PLUGIN, "MIDI Process Packet : queue size %d\n", incoming.size());
        while (!incoming.empty()) {
            auto midi = incoming.front();
            incoming.pop_front();
            lock.unlock();
            std::unique_lock<std::mutex> elock(lastEventsLock);
            lastEvents.push_back(midi);
            if (lastEvents.size() > 25) {
                lastEvents.pop_front();
            }
            elock.unlock();
            for (auto &a : events) {
                if (a->matches(midi)) {
                    a->invoke(midi);
                }
            }
            lock.lock();
        }
        return false;
    }
};

// Safe to dlclose() on unload: the only threads are RtMidi's input threads, and
// shutdown() unhooks the callback and closes each port, which stops and joins
// them. No timers, no CurlManager requests, no commands and no drogon client
// objects. The routes go through registerPluginApi() and come back in
// unregisterApis(); FPP withdraws the eventfd from its epoll loop, and the
// descriptors and MIDI objects are freed in the destructor, which runs before
// the library is unmapped.
FPP_PLUGIN_SUPPORTS_UNLOAD()

extern "C" {
    FPPPlugins::Plugin *createPlugin() {
        return new FPPMIDIPlugin();
    }
}

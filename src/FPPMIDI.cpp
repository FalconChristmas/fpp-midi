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
#include <httpserver.hpp>
#include <jsoncpp/json/json.h>
#include <sys/eventfd.h>
#include <cmath>
#include <mutex>


#include <rtmidi/RtMidi.h>
#include "FPPMIDI.h"

#include "commands/Commands.h"
#include "common.h"
#include "settings.h"
#include "Plugin.h"
#include "log.h"
#include "util/ExpressionProcessor.cpp"

class MIDIInputEvent {
public:
    MIDIInputEvent(const std::vector<unsigned char> &m) : params(m) {
    }
    std::string toString() {
        std::string v;
        for (auto a : params) {
            char buf[10];
            sprintf(buf, "0x%02X", a);
            if (!v.empty()) {
                v += " ";
            }
            v += buf;
        }
        return v;
    }

    std::vector<unsigned char> params;
};


class MIDICondition {
public:
    MIDICondition(Json::Value &v) {
        conditionType = v["condition"].asString();
        compareType = v["conditionCompare"].asString();
        std::string text = v["conditionText"].asString();
        val = std::stoi(text, nullptr, 0);
    }
    
    bool matches(MIDIInputEvent &ev) {
        int v = 0;
        if (conditionType == "noteOn") {
            if ((ev.params[0] & 0xF0) != 0x90) {
                return false;
            }
            v = ev.params[1];
        } else if (conditionType == "noteOff") {
            if ((ev.params[0] & 0xF0) != 0x80) {
                return false;
            }
            v = ev.params[1];
        } else if (conditionType == "channel") {
            v = ev.params[0] & 0xf;
        } else if (conditionType == "control") {
            if ((ev.params[0] & 0xF0) != 0xB0) {
                return false;
            }
            v = ev.params[1];
        } else if (conditionType == "pitch") {
            if ((ev.params[0] & 0xF0) != 0xE0) {
                return false;
            }
            v = ev.params[2];
            v = v << 7; //7 bit numbers so only shift 7
            v += ev.params[1];
            v -= 0x2000; //range is -8192 - 8192
        } else {
            int idx = conditionType[1] - '1';
            if (idx >= ev.params.size()) {
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
    MIDICommandArg(const std::string &t) : arg(t) {
    }
    ~MIDICommandArg() {
        if (processor) {
            delete processor;
        }
    }
    
    std::string arg;
    std::string type;
    
    ExpressionProcessor *processor = nullptr;
    
    std::string evaluate(const std::string &tp) {
        if (processor) {
            std::string s = processor->evaluate(tp);
            return s;
        }
        return "";
    }
};

static const int NUM_VARS = 9;
static const std::string vNames[] = {
    "b1", "b2", "b3", "b4", "b5",
    "note", "channel", "pitch", "velocity"
};

class MIDIEvent {
public:
    MIDIEvent(Json::Value &v) {
        path = v["path"].asString();
        description = v["description"].asString();
        for (int x = 0; x < v["conditions"].size(); x++) {
            conditions.push_back(MIDICondition(v["conditions"][x]));
        }

        command = v["command"].asString();
        for (int x = 0; x < v["args"].size(); x++) {
            args.push_back(MIDICommandArg(v["args"][x].asString()));
        }
        if (v.isMember("argTypes")) {
            for (int x = 0; x < v["argTypes"].size(); x++) {
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
        for (auto &a : args) {
            a.processor->compile(a.arg);
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
        for (int x = 0; x < ev.params.size(); x++) {
            variables[x]->setValue(std::to_string(ev.params[x]));
        }
        variables[5]->setValue(std::to_string(ev.params[1])); //note var
        variables[8]->setValue(std::to_string(ev.params[2])); //velocity var
        variables[6]->setValue(std::to_string(ev.params[0] & 0xF)); //channel
        //pitch
        int pitch = ev.params[2];
        pitch = pitch << 7; //7 bit numbers so only shift 7
        pitch += ev.params[1];
        pitch -= 0x2000;
        variables[7]->setValue(std::to_string(pitch));
        std::vector<std::string> ar;
        for (auto &a : args) {
            std::string tp = "string";
            if (a.type == "bool" || a.type == "int") {
                tp = a.type;
            }
            
            //printf("Eval p: %s\n", a.arg.c_str());
            std::string r = a.evaluate(tp);
            //printf("        -> %s\n", r.c_str());
            ar.push_back(r);
        }

        CommandManager::INSTANCE.run(command, ar);
    }
    
    std::string path;
    std::string description;
    
    std::list<MIDICondition> conditions;
    
    std::string command;
    std::vector<MIDICommandArg> args;
    
    std::array<ExpressionProcessor::ExpressionVariable*, 12> variables;
};


class FPPMIDIPlugin : public FPPPlugin, public httpserver::http_resource {
public:
    int eventFile;
    std::vector<RtMidiIn *> midiin;
    
    std::list<MIDIEvent *> events;
    std::list<MIDIInputEvent> lastEvents;
    
    
    std::mutex queueLock;
    std::list<MIDIInputEvent> incoming;
    
    
    FPPMIDIPlugin() : FPPPlugin("fpp-midi") {
        LogInfo(VB_PLUGIN, "Initializing MIDI Plugin\n");
        
        eventFile = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (FileExists("/home/fpp/media/config/plugin.fpp-midi.json")) {
            std::ifstream t("/home/fpp/media/config/plugin.fpp-midi.json");
            std::stringstream buffer;
            buffer << t.rdbuf();
            std::string config = buffer.str();
            Json::Value root;
            Json::Reader reader;
            bool success = reader.parse(buffer.str(), root);
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
    virtual ~FPPMIDIPlugin() {
        for (auto a : midiin) {
            a->closePort();
            delete a;
        }
        for (auto e : events) {
            delete e;
        }
        close(eventFile);
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
        write(eventFile, &v, 8);
    }

    virtual const httpserver::http_response render_GET(const httpserver::http_request &req) override {
        std::string v;
        for (auto &a : lastEvents) {
            v += a.toString() + "\n";
        }
        return httpserver::http_response_builder(v, 200);
    }
    bool ProcessPacket(int i) {
        char buf[256];
        ssize_t s = read(eventFile, buf, 256);
        while (s > 0) {
            s = read(eventFile, buf, 256);
        }
        std::unique_lock<std::mutex> lock(queueLock);
        LogExcess(VB_PLUGIN, "MIDI Process Packet : queue size %d\n", incoming.size());
        while (!incoming.empty()) {
            auto midi = incoming.front();
            incoming.pop_front();
            
            lock.unlock();
            lastEvents.push_back(midi);
            if (lastEvents.size() > 25) {
                lastEvents.pop_front();
            }
            for (auto &a : events) {
                if (a->matches(midi)) {
                    a->invoke(midi);
                }
            }
            lock.lock();
        }
        return false;
    }
    void registerApis(httpserver::webserver *m_ws) override {
        m_ws->register_resource("/MIDI", this, true);
    }
    virtual void addControlCallbacks(std::map<int, std::function<bool(int)>> &callbacks) {
        callbacks[eventFile] = [this](int i) {
            return ProcessPacket(i);
        };
    }
};


extern "C" {
    FPPPlugin *createPlugin() {
        return new FPPMIDIPlugin();
    }
}

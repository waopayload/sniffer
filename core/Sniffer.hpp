/*******************************************************************************
 *  Advanced network sniffer
 *  Main module
 *  
 *  © 2013—2018, Sauron
 ******************************************************************************/

#ifndef __CORE_SNIFFER_HPP
#define __CORE_SNIFFER_HPP

#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>
#include "../sniffer.hpp"

/**/
struct Plugin {
    const char * name;
    const char * description;
    int version;
    unsigned flags;
    Protocol::Factory factory;
};

/** Plugin registry **/
class Registry : public std::vector<Plugin> {
public:
    /** Thrown on attempt to access non-existent plugin **/
    class PluginNotFoundException {
    public:
        PluginNotFoundException(const char * name) : name(name) {}
        const char * getName() const { return name; }
        
    private:
        const char * name;
    };
    /** Find plugin by name **/
    const Plugin &operator [](const char * name);
    /** Global plugin registry **/
    static Registry &instance();
};

class OptionsImpl : public Options {
public:
    OptionsImpl() {}
    OptionsImpl(const char * optarg);
    const std::string &get(const char * option) const;
    
private:
    std::map<std::string, std::string> options;
};

/**/
class SnifferBase {
public:
    SnifferBase(class SnifferController &controller);
    virtual ~SnifferBase();
    /** Returns unique instance identifier **/
    unsigned getInstanceId() const { return instanceId; }
    
protected:
    SnifferController &controller;
    unsigned instanceId;
};

/** Abstract protocol sniffer **/
class Sniffer : public SnifferBase {
public:
    /**/
    explicit Sniffer(SnifferController &controller);
    /** Close connection and destroy plugin **/
    virtual ~Sniffer();
    
protected:
    /** Dump next packet **/
    void dump(std::ostream &log, bool incoming, Reader &reader);
    /** Start incoming and outgoing threads **/
    void start(SnifferController &controller);
    /** Output beginning of message to cerr and return it **/
    std::ostream &error() const;
    /** This function should be overridden by subclasses **/
    virtual void threadFunc(std::ostream &log, bool incoming)=0;
    
private:
    /** Protocol plugin instance **/
    Protocol * protocol;
    /** Mutex for synchronization of access to output log **/
    static std::mutex logMutex;
    /** Thread for interception outgoing data **/
    std::thread c2sThread;
    /** Thread for interception incoming data **/
    std::thread s2cThread;
    /** Private thread function **/
    void _threadFunc(SnifferController &controller, bool incoming);
};

/** Object for controlling life cycle of sniffers **/
class SnifferController {
    friend class SnifferBase;
public:
    /**/
    SnifferController(const Plugin &plugin, const OptionsImpl &options,
        std::ostream &output) : maxInstanceId(0), plugin(plugin), options(options),
        output(output), alive(true), gcThread(&SnifferController::gcThreadFunc,
        this) {}
    /**/
    ~SnifferController();
    /** Returns stream where sniffers should write to **/
    std::ostream &getStream() const { return output; }
    /** Create protocol plugin instance **/
    Protocol * newProtocol() const { return plugin.factory(options); }
    /** Called by sniffer to inform that it should be destroyed **/
    void mark(SnifferBase * sniffer);
    
private:
    enum State { Alive, Marked, Deleted };
    
    SnifferController(const SnifferController &)=delete;
    SnifferController &operator =(const SnifferController &)=delete;
    unsigned maxInstanceId;
    const Plugin &plugin;
    OptionsImpl options;
    std::ostream &output;
    bool alive;
    std::mutex gcMutex;
    std::thread gcThread;
    std::condition_variable gc;
    std::map<SnifferBase *, State> sniffers;
    void gcThreadFunc();
    
    /** Called by sniffer to inform that it was created **/
    void add(SnifferBase * sniffer);
    /** Called by sniffer to inform that it was deleted **/
    void remove(SnifferBase * sniffer);
};

#endif

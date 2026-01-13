#ifndef TSUNAMI_VM_HPP
#define TSUNAMI_VM_HPP

#include "tsunami_push.hpp"
#include <unordered_map>
#include <vector>
#include <functional>
#include <string>
#include <memory>

namespace tsunami {

// ==================== VM VALUE TYPE ====================
struct VMValue {
    enum Type {
        NIL,
        BOOLEAN,
        NUMBER,
        STRING,
        FUNCTION,
        TABLE,
        USERDATA,
        LIGHTUSERDATA
    };
    
    Type type;
    union {
        bool boolean;
        double number;
        void* pointer;
    } value;
    std::string string;
    
    // Store TValue for fast conversion
    TValue tvalue;
    
    VMValue() : type(NIL) {
        tvalue = TValue::Nil();
    }
    
    static VMValue Nil() {
        VMValue v;
        v.type = NIL;
        v.tvalue = TValue::Nil();
        return v;
    }
    
    static VMValue Boolean(bool b) {
        VMValue v;
        v.type = BOOLEAN;
        v.value.boolean = b;
        v.tvalue = TValue::Boolean(b);
        return v;
    }
    
    static VMValue Number(double n) {
        VMValue v;
        v.type = NUMBER;
        v.value.number = n;
        v.tvalue = TValue::Number(n);
        return v;
    }
    
    static VMValue String(const std::string& s) {
        VMValue v;
        v.type = STRING;
        v.string = s;
        // Note: tvalue.gcobject will be set when pushing to real Lua state
        return v;
    }
    
    static VMValue LightUserData(void* p) {
        VMValue v;
        v.type = LIGHTUSERDATA;
        v.value.pointer = p;
        v.tvalue = TValue::LightUserData(p);
        return v;
    }
};

// ==================== VM FUNCTION INTERFACE ====================
using VMFunction = std::function<VMValue(const std::vector<VMValue>&)>;

// ==================== CUSTOM VM STATE ====================
class VMState {
private:
    // Custom environment (table-like)
    std::unordered_map<std::string, VMValue> globals;
    std::unordered_map<std::string, VMFunction> functions;
    
    // Stack for execution
    std::vector<VMValue> stack;
    
    // Parent Roblox state for fallback
    lua_State* robloxL;
    tsunami::PushEngine robloxPusher;
    
    // Function to get global from Roblox (your offset)
    using GetGlobalFn = void(*)(lua_State*, const char*);
    GetGlobalFn roblox_getglobal;
    
    // Function to check if something exists in Roblox
    using GetFieldFn = void(*)(lua_State*, int, const char*);
    GetFieldFn roblox_getfield;
    
    // Cached Roblox globals that we've checked
    std::unordered_map<std::string, bool> robloxGlobalCache;
    
    // Configuration
    bool enableRobloxFallback;
    bool cacheRobloxGlobals;
    
public:
    VMState(lua_State* robloxState = nullptr, 
            bool enableFallback = true,
            bool cacheGlobals = true)
        : robloxL(robloxState),
          robloxPusher(robloxState, tsunami::PushEngine::MODE_BYTECODE),
          enableRobloxFallback(enableFallback),
          cacheRobloxGlobals(cacheGlobals) {
        
        // Initialize Roblox function pointers from offsets
        // Replace these with your actual offsets
        roblox_getglobal = reinterpret_cast<GetGlobalFn>(0x100000000); // Your getglobal offset
        roblox_getfield = reinterpret_cast<GetFieldFn>(0x100000008);   // Your getfield offset
        
        // Register built-in functions
        registerBuiltins();
    }
    
    // ==================== STACK OPERATIONS ====================
    void push(const VMValue& value) {
        stack.push_back(value);
    }
    
    VMValue pop() {
        if (stack.empty()) return VMValue::Nil();
        VMValue value = stack.back();
        stack.pop_back();
        return value;
    }
    
    VMValue& top() {
        static VMValue nil = VMValue::Nil();
        if (stack.empty()) return nil;
        return stack.back();
    }
    
    int stackSize() const {
        return static_cast<int>(stack.size());
    }
    
    void clearStack() {
        stack.clear();
    }
    
    // ==================== ENVIRONMENT MANAGEMENT ====================
    void setGlobal(const std::string& name, const VMValue& value) {
        globals[name] = value;
        robloxGlobalCache.erase(name); // Invalidate cache
    }
    
    VMValue getGlobal(const std::string& name) {
        // 1. Check custom VM globals
        auto it = globals.find(name);
        if (it != globals.end()) {
            return it->second;
        }
        
        // 2. Check cached Roblox globals
        if (cacheRobloxGlobals) {
            auto cacheIt = robloxGlobalCache.find(name);
            if (cacheIt != robloxGlobalCache.end() && cacheIt->second) {
                // We know it exists in Roblox, fetch it
                return fetchFromRoblox(name);
            }
        }
        
        // 3. Check if function exists in custom VM
        if (functions.find(name) != functions.end()) {
            // Return a function value
            VMValue func;
            func.type = VMValue::FUNCTION;
            return func;
        }
        
        // 4. Try Roblox fallback
        if (enableRobloxFallback && robloxL && roblox_getglobal) {
            if (existsInRoblox(name)) {
                if (cacheRobloxGlobals) {
                    robloxGlobalCache[name] = true;
                }
                return fetchFromRoblox(name);
            } else {
                if (cacheRobloxGlobals) {
                    robloxGlobalCache[name] = false;
                }
            }
        }
        
        // 5. Not found anywhere
        return VMValue::Nil();
    }
    
    void registerFunction(const std::string& name, VMFunction func) {
        functions[name] = func;
    }
    
    bool existsInVM(const std::string& name) const {
        return globals.find(name) != globals.end() ||
               functions.find(name) != functions.end();
    }
    
    // ==================== FUNCTION EXECUTION ====================
    VMValue call(const std::string& funcName, const std::vector<VMValue>& args = {}) {
        // 1. Check custom VM functions
        auto funcIt = functions.find(funcName);
        if (funcIt != functions.end()) {
            return funcIt->second(args);
        }
        
        // 2. Check Roblox functions via fallback
        if (enableRobloxFallback && robloxL) {
            return callRobloxFunction(funcName, args);
        }
        
        // 3. Function not found
        std::cerr << "Function '" << funcName << "' not found\n";
        return VMValue::Nil();
    }
    
    VMValue call(int numArgs = 0) {
        if (stack.size() < static_cast<size_t>(numArgs + 1)) {
            return VMValue::Nil();
        }
        
        // Get function from stack
        VMValue funcVal = stack[stack.size() - numArgs - 1];
        
        // For now, only handle string-named functions from top of stack
        // More complex implementation would handle actual function values
        return VMValue::Nil();
    }
    
    // ==================== ROBLOX INTEGRATION ====================
private:
    bool existsInRoblox(const std::string& name) {
        if (!robloxL || !roblox_getfield) return false;
        
        // Save stack state
        int top = lua_gettop(robloxL);
        
        // Try to get the global
        roblox_getglobal(robloxL, name.c_str());
        bool exists = !lua_isnil(robloxL, -1);
        
        // Restore stack
        lua_settop(robloxL, top);
        
        return exists;
    }
    
    VMValue fetchFromRoblox(const std::string& name) {
        if (!robloxL || !roblox_getglobal) return VMValue::Nil();
        
        // Save stack state
        int top = lua_gettop(robloxL);
        
        // Get the value from Roblox
        roblox_getglobal(robloxL, name.c_str());
        
        // Convert Lua value to VMValue
        VMValue result = luaToVMValue(-1);
        
        // Restore stack
        lua_settop(robloxL, top);
        
        return result;
    }
    
    VMValue callRobloxFunction(const std::string& funcName, 
                               const std::vector<VMValue>& args) {
        if (!robloxL || !roblox_getglobal) return VMValue::Nil();
        
        // Save stack state
        int top = lua_gettop(robloxL);
        
        // Push function
        roblox_getglobal(robloxL, funcName.c_str());
        
        if (!lua_isfunction(robloxL, -1)) {
            lua_settop(robloxL, top);
            return VMValue::Nil();
        }
        
        // Push arguments
        for (const auto& arg : args) {
            pushVMValueToLua(arg);
        }
        
        // Call function
        int status = lua_pcall(robloxL, args.size(), 1, 0);
        
        VMValue result;
        if (status == LUA_OK) {
            result = luaToVMValue(-1);
        } else {
            std::cerr << "Roblox function error: " << lua_tostring(robloxL, -1) << "\n";
            result = VMValue::Nil();
        }
        
        // Restore stack
        lua_settop(robloxL, top);
        
        return result;
    }
    
    VMValue luaToVMValue(int idx) {
        if (!robloxL) return VMValue::Nil();
        
        int type = lua_type(robloxL, idx);
        
        switch (type) {
            case LUA_TNIL:
                return VMValue::Nil();
                
            case LUA_TBOOLEAN:
                return VMValue::Boolean(lua_toboolean(robloxL, idx) != 0);
                
            case LUA_TNUMBER:
                return VMValue::Number(lua_tonumber(robloxL, idx));
                
            case LUA_TSTRING:
                return VMValue::String(lua_tostring(robloxL, idx));
                
            case LUA_TLIGHTUSERDATA:
                return VMValue::LightUserData(lua_touserdata(robloxL, idx));
                
            default:
                return VMValue::Nil();
        }
    }
    
    void pushVMValueToLua(const VMValue& value) {
        switch (value.type) {
            case VMValue::NIL:
                robloxPusher.pushnil();
                break;
                
            case VMValue::BOOLEAN:
                robloxPusher.pushboolean(value.value.boolean);
                break;
                
            case VMValue::NUMBER:
                robloxPusher.pushnumber(value.value.number);
                break;
                
            case VMValue::STRING:
                robloxPusher.pushstring(value.string);
                break;
                
            case VMValue::LIGHTUSERDATA:
                robloxPusher.getBytecodePusher().executeBytecode(
                    Bytecode::CreatePushString("lightuserdata")
                );
                break;
                
            default:
                robloxPusher.pushnil();
                break;
        }
    }
    
    // ==================== BUILT-IN FUNCTIONS ====================
    void registerBuiltins() {
        // print function
        registerFunction("vmprint", [this](const std::vector<VMValue>& args) -> VMValue {
            for (const auto& arg : args) {
                switch (arg.type) {
                    case VMValue::NIL:
                        std::cout << "nil";
                        break;
                    case VMValue::BOOLEAN:
                        std::cout << (arg.value.boolean ? "true" : "false");
                        break;
                    case VMValue::NUMBER:
                        std::cout << arg.value.number;
                        break;
                    case VMValue::STRING:
                        std::cout << arg.string;
                        break;
                    default:
                        std::cout << "[unknown]";
                        break;
                }
                std::cout << " ";
            }
            std::cout << "\n";
            return VMValue::Nil();
        });
        
        // type function
        registerFunction("vmtype", [](const std::vector<VMValue>& args) -> VMValue {
            if (args.empty()) return VMValue::String("nil");
            
            switch (args[0].type) {
                case VMValue::NIL: return VMValue::String("nil");
                case VMValue::BOOLEAN: return VMValue::String("boolean");
                case VMValue::NUMBER: return VMValue::String("number");
                case VMValue::STRING: return VMValue::String("string");
                case VMValue::FUNCTION: return VMValue::String("function");
                case VMValue::TABLE: return VMValue::String("table");
                case VMValue::USERDATA: return VMValue::String("userdata");
                case VMValue::LIGHTUSERDATA: return VMValue::String("userdata");
                default: return VMValue::String("unknown");
            }
        });
        
        // tostring function
        registerFunction("vmtostring", [](const std::vector<VMValue>& args) -> VMValue {
            if (args.empty()) return VMValue::String("nil");
            
            std::ostringstream oss;
            switch (args[0].type) {
                case VMValue::NIL:
                    oss << "nil";
                    break;
                case VMValue::BOOLEAN:
                    oss << (args[0].value.boolean ? "true" : "false");
                    break;
                case VMValue::NUMBER:
                    oss << args[0].value.number;
                    break;
                case VMValue::STRING:
                    oss << args[0].string;
                    break;
                default:
                    oss << args[0].type;
                    break;
            }
            return VMValue::String(oss.str());
        });
        
        // tonumber function
        registerFunction("vmtonumber", [](const std::vector<VMValue>& args) -> VMValue {
            if (args.empty()) return VMValue::Nil();
            
            if (args[0].type == VMValue::NUMBER) {
                return args[0];
            } else if (args[0].type == VMValue::STRING) {
                try {
                    double num = std::stod(args[0].string);
                    return VMValue::Number(num);
                } catch (...) {
                    return VMValue::Nil();
                }
            } else if (args[0].type == VMValue::BOOLEAN) {
                return VMValue::Number(args[0].value.boolean ? 1.0 : 0.0);
            }
            
            return VMValue::Nil();
        });
    }
    
public:
    // ==================== BYTECODE EXECUTION ====================
    bool executeBytecode(const std::string& bytecode) {
        // Use the bytecode pusher directly
        return robloxPusher.getBytecodePusher().executeBytecode(bytecode);
    }
    
    // ==================== SETTINGS ====================
    void enableFallback(bool enable) {
        enableRobloxFallback = enable;
    }
    
    void enableCaching(bool enable) {
        cacheRobloxGlobals = enable;
    }
    
    void clearCache() {
        robloxGlobalCache.clear();
    }
    
    // ==================== UTILITIES ====================
    void dumpStack() const {
        std::cout << "VM Stack (" << stack.size() << " items):\n";
        for (size_t i = 0; i < stack.size(); i++) {
            std::cout << "  [" << i << "]: ";
            switch (stack[i].type) {
                case VMValue::NIL: std::cout << "nil"; break;
                case VMValue::BOOLEAN: std::cout << (stack[i].value.boolean ? "true" : "false"); break;
                case VMValue::NUMBER: std::cout << stack[i].value.number; break;
                case VMValue::STRING: std::cout << "\"" << stack[i].string << "\""; break;
                case VMValue::FUNCTION: std::cout << "function"; break;
                default: std::cout << "unknown"; break;
            }
            std::cout << "\n";
        }
    }
    
    void dumpGlobals() const {
        std::cout << "VM Globals:\n";
        for (const auto& [name, value] : globals) {
            std::cout << "  " << name << " = ";
            switch (value.type) {
                case VMValue::NIL: std::cout << "nil"; break;
                case VMValue::BOOLEAN: std::cout << (value.value.boolean ? "true" : "false"); break;
                case VMValue::NUMBER: std::cout << value.value.number; break;
                case VMValue::STRING: std::cout << "\"" << value.string << "\""; break;
                default: std::cout << "[" << value.type << "]"; break;
            }
            std::cout << "\n";
        }
    }
};

// ==================== SIMPLE BYTECODE VM ====================
class BytecodeVM {
private:
    VMState vm;
    tsunami::BytecodePusher bytecodePusher;
    
public:
    BytecodeVM(lua_State* robloxState) 
        : vm(robloxState),
          bytecodePusher(robloxState) {}
    
    // Execute bytecode string
    bool execute(const std::string& bytecode) {
        return bytecodePusher.executeBytecode(bytecode);
    }
    
    // Execute Lua source (compiles to bytecode first)
    bool executeSource(const std::string& source) {
        std::string bytecode = Bytecode::Compile(source);
        if (bytecode.empty()) return false;
        return execute(bytecode);
    }
    
    // Register custom function accessible from bytecode
    void registerGlobalFunction(const std::string& name, VMFunction func) {
        vm.registerFunction(name, func);
    }
    
    // Set global value
    void setGlobal(const std::string& name, const VMValue& value) {
        vm.setGlobal(name, value);
    }
    
    // Get global value
    VMValue getGlobal(const std::string& name) {
        return vm.getGlobal(name);
    }
    
    // Direct access to VM
    VMState& getVM() { return vm; }
    
    // Direct access to pusher
    tsunami::BytecodePusher& getPusher() { return bytecodePusher; }
};

} // namespace tsunami

#endif // TSUNAMI_VM_HPP

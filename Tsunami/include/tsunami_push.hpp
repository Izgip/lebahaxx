#ifndef TSUNAMI_PUSH_HPP
#define TSUNAMI_PUSH_HPP

#include "Bytecode.h"
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <lua.hpp>

namespace tsunami {

// ==================== TVALUE STRUCT ====================
#pragma pack(push, 1)
struct TValue {
    union {
        uint64_t gcobject;
        void* p;
        double n;
        int b;
        float v[2];
    } value;
    
    int extra;
    int tt;
    
    static constexpr size_t SIZE = 16;
    
    enum Type {
        LUA_TNIL = 0,
        LUA_TBOOLEAN = 1,
        LUA_TNUMBER = 3,
        LUA_TSTRING = 5,
        LUA_TTABLE = 6,
        LUA_TFUNCTION = 7,
        LUA_TINSTANCE = 41,
    };
    
    // Factory methods
    static TValue Nil() { 
        TValue v{};
        v.tt = LUA_TNIL;
        v.value.n = 0;
        return v;
    }
    
    static TValue Boolean(bool b) { 
        TValue v{};
        v.tt = LUA_TBOOLEAN;
        v.value.b = b ? 1 : 0;
        return v;
    }
    
    static TValue Number(double num) { 
        TValue v{};
        v.tt = LUA_TNUMBER;
        v.value.n = num;
        return v;
    }
    
    static TValue Integer(int64_t num) {
        TValue v{};
        v.tt = LUA_TNUMBER;
        v.value.n = static_cast<double>(num);
        return v;
    }
    
    static TValue String(uint64_t gc) {
        TValue v{};
        v.tt = LUA_TSTRING;
        v.value.gcobject = gc;
        return v;
    }
    
    static TValue Instance(uint64_t gc, int classId) {
        TValue v{};
        v.tt = LUA_TINSTANCE;
        v.value.gcobject = gc;
        v.extra = classId;
        return v;
    }
    
    static TValue Vector(float x, float y) {
        TValue v{};
        v.tt = 4;  // LUA_TVECTOR
        v.value.v[0] = x;
        v.value.v[1] = y;
        return v;
    }
    
    static TValue LightUserData(void* ptr) {
        TValue v{};
        v.tt = 2;  // LUA_TLIGHTUSERDATA
        v.value.p = ptr;
        return v;
    }
};
#pragma pack(pop)

// ==================== BYTECODE PUSH ENGINE ====================
class BytecodePusher {
private:
    lua_State* L;
    Bytecode::BytecodeCache cache;
    
    // Function pointers to Roblox internals
    using LuauLoadFn = int(*)(lua_State*, const char*, size_t, const char*, int);
    using PcallImplFn = int(*)(lua_State*, int, int, int);
    using StrMakerFn = const char*(*)(lua_State*, const char*, size_t);
    
    LuauLoadFn luau_load;
    PcallImplFn pcall_impl;
    StrMakerFn strmaker;
    
    // Get function pointers (you'll need to set these from your offsets)
    void initializeFunctionPointers() {
        // Set these from your known offsets
        luau_load = reinterpret_cast<LuauLoadFn>(0x100dee764);
        pcall_impl = reinterpret_cast<PcallImplFn>(0x1033c1bb8);
        strmaker = reinterpret_cast<StrMakerFn>(0x10000dfd4);
    }
    
public:
    BytecodePusher(lua_State* L) : L(L) {
        initializeFunctionPointers();
    }
    
    // ==================== BYTECODE EXECUTION ====================
    bool executeBytecode(const std::string& bytecode, const char* chunkname = "=tsunami") {
        if (!luau_load || !pcall_impl) {
            return false;
        }
        
        // Decompress if needed
        std::string decompressed = Bytecode::Decompress(bytecode);
        if (decompressed.empty()) {
            decompressed = bytecode;
        }
        
        // Load bytecode
        if (luau_load(L, decompressed.data(), decompressed.size(), chunkname, 0) != 0) {
            return false;
        }
        
        // Execute
        return (pcall_impl(L, 0, 1, 0) == 0);
    }
    
    // ==================== CACHED PUSH OPERATIONS ====================
    bool pushnil() {
        static std::string nilBytecode = Bytecode::CreatePushNil();
        return executeBytecode(nilBytecode);
    }
    
    bool pushboolean(bool value) {
        std::string bytecode = cache.getBoolean(value);
        return executeBytecode(bytecode);
    }
    
    bool pushnumber(double value) {
        std::string bytecode = cache.getNumber(value);
        return executeBytecode(bytecode);
    }
    
    bool pushinteger(int value) {
        std::string bytecode = cache.getInteger(value);
        return executeBytecode(bytecode);
    }
    
    bool pushstring(const std::string& value) {
        std::string bytecode = cache.getString(value);
        return executeBytecode(bytecode);
    }
    
    bool pushstring(const char* value) {
        return pushstring(std::string(value));
    }
    
    // ==================== TABLE OPERATIONS ====================
    bool pushtable(int arraySize = 0, int hashSize = 0) {
        std::string bytecode = Bytecode::CreatePushTable(arraySize, hashSize);
        return executeBytecode(bytecode);
    }
    
    bool pusharray(const std::vector<std::string>& values) {
        std::string bytecode = Bytecode::CreatePushArray(values);
        return executeBytecode(bytecode);
    }
    
    bool pushdictionary(const std::vector<std::pair<std::string, std::string>>& keyValues) {
        std::string bytecode = Bytecode::CreatePushDictionary(keyValues);
        return executeBytecode(bytecode);
    }
    
    bool pushmultiple(const std::vector<std::string>& values) {
        std::string bytecode = Bytecode::CreatePushMultiple(values);
        return executeBytecode(bytecode);
    }
    
    // ==================== ROBOX-SPECIFIC TYPES ====================
    bool pushvector2(float x, float y) {
        std::string bytecode = Bytecode::CreatePushVector2(x, y);
        return executeBytecode(bytecode);
    }
    
    bool pushvector3(float x, float y, float z) {
        std::string bytecode = Bytecode::CreatePushVector3(x, y, z);
        return executeBytecode(bytecode);
    }
    
    bool pushcolor3(float r, float g, float b) {
        std::string bytecode = Bytecode::CreatePushColor3(r, g, b);
        return executeBytecode(bytecode);
    }
    
    // ==================== BATCH OPERATIONS ====================
    template<typename... Args>
    bool pushmultiple(Args... args) {
        std::vector<std::string> values;
        (values.push_back(args), ...);
        return pushmultiple(values);
    }
    
    // ==================== DIRECT BYTECODE ACCESS ====================
    std::string createPushNil() { return Bytecode::CreatePushNil(); }
    std::string createPushBoolean(bool v) { return Bytecode::CreatePushBoolean(v); }
    std::string createPushNumber(double v) { return Bytecode::CreatePushNumber(v); }
    std::string createPushString(const std::string& v) { return Bytecode::CreatePushString(v); }
    std::string createPushTable(int a, int h) { return Bytecode::CreatePushTable(a, h); }
    std::string createPushArray(const std::vector<std::string>& v) { return Bytecode::CreatePushArray(v); }
    
    // ==================== CUSTOM BYTECODE ====================
    bool pushCustom(const std::string& luaCode) {
        std::string bytecode = Bytecode::Compile(luaCode);
        if (bytecode.empty()) return false;
        return executeBytecode(bytecode);
    }
    
    // ==================== DEBUG UTILITIES ====================
    void dump(const std::string& bytecode, size_t maxBytes = 64) {
        std::cout << Bytecode::HexDump(bytecode, maxBytes) << "\n";
    }
    
    bool validate(const std::string& bytecode) {
        return Bytecode::ValidateBytecode(bytecode);
    }
};

// ==================== HYBRID PUSH SYSTEM ====================
class PushEngine {
private:
    lua_State* L;
    BytecodePusher bytecodePusher;
    
    // TValue stack offsets (from your original code)
    static constexpr uintptr_t STACK_BASE_OFFSET = 0x0;
    static constexpr uintptr_t STACK_TOP_OFFSET = 0x8;
    static constexpr uintptr_t STACK_LAST_OFFSET = 0x10;
    
    enum PushMode {
        MODE_TVALUE,    // Fast direct memory write
        MODE_BYTECODE,  // Portable bytecode execution
        MODE_AUTO       // Auto-select based on safety
    };
    
    PushMode mode;
    
    // Get TValue* to stack slot
    TValue* getSlotPtr(int index) {
        TValue** stackBasePtr = reinterpret_cast<TValue**>(
            reinterpret_cast<uintptr_t>(L) + STACK_BASE_OFFSET);
        TValue** topPtr = reinterpret_cast<TValue**>(
            reinterpret_cast<uintptr_t>(L) + STACK_TOP_OFFSET);
        
        TValue* stack = *stackBasePtr;
        TValue* top = *topPtr;
        
        if (index > 0) {
            TValue* ptr = stack + (index - 1);
            return (ptr < top) ? ptr : nullptr;
        } else if (index < 0) {
            TValue* ptr = top + index;
            return (ptr >= stack) ? ptr : nullptr;
        }
        return nullptr;
    }
    
    TValue* getTopPtr() {
        TValue** topPtr = reinterpret_cast<TValue**>(
            reinterpret_cast<uintptr_t>(L) + STACK_TOP_OFFSET);
        return *topPtr;
    }
    
    void setTopPtr(TValue* newTop) {
        TValue** topPtr = reinterpret_cast<TValue**>(
            reinterpret_cast<uintptr_t>(L) + STACK_TOP_OFFSET);
        *topPtr = newTop;
    }
    
    // Fast TValue push (direct memory)
    void pushnil_tvalue() {
        TValue* top = getTopPtr();
        *top = TValue::Nil();
        setTopPtr(top + 1);
    }
    
    void pushboolean_tvalue(bool b) {
        TValue* top = getTopPtr();
        *top = TValue::Boolean(b);
        setTopPtr(top + 1);
    }
    
    void pushnumber_tvalue(double n) {
        TValue* top = getTopPtr();
        *top = TValue::Number(n);
        setTopPtr(top + 1);
    }
    
    void pushstring_tvalue(const std::string& s) {
        if (!strmaker) return;
        
        const char* luaStr = strmaker(L, s.c_str(), s.size());
        if (!luaStr) return;
        
        TValue* top = getTopPtr();
        *top = TValue::String(reinterpret_cast<uint64_t>(luaStr));
        setTopPtr(top + 1);
    }
    
public:
    PushEngine(lua_State* L, PushMode mode = MODE_AUTO) 
        : L(L), bytecodePusher(L), mode(mode) {}
    
    // ==================== SMART PUSH INTERFACE ====================
    bool pushnil() {
        if (mode == MODE_TVALUE || (mode == MODE_AUTO && checkTValueSafe())) {
            pushnil_tvalue();
            return true;
        }
        return bytecodePusher.pushnil();
    }
    
    bool pushboolean(bool value) {
        if (mode == MODE_TVALUE || (mode == MODE_AUTO && checkTValueSafe())) {
            pushboolean_tvalue(value);
            return true;
        }
        return bytecodePusher.pushboolean(value);
    }
    
    bool pushnumber(double value) {
        if (mode == MODE_TVALUE || (mode == MODE_AUTO && checkTValueSafe())) {
            pushnumber_tvalue(value);
            return true;
        }
        return bytecodePusher.pushnumber(value);
    }
    
    bool pushinteger(int value) {
        return pushnumber(static_cast<double>(value));
    }
    
    bool pushstring(const std::string& value) {
        if (mode == MODE_TVALUE || (mode == MODE_AUTO && checkTValueSafe())) {
            pushstring_tvalue(value);
            return true;
        }
        return bytecodePusher.pushstring(value);
    }
    
    bool pushstring(const char* value) {
        return pushstring(std::string(value));
    }
    
    // ==================== TABLE OPERATIONS ====================
    bool pushtable(int arraySize = 0, int hashSize = 0) {
        return bytecodePusher.pushtable(arraySize, hashSize);
    }
    
    bool pusharray(const std::vector<std::string>& values) {
        return bytecodePusher.pusharray(values);
    }
    
    bool pushdictionary(const std::vector<std::pair<std::string, std::string>>& keyValues) {
        return bytecodePusher.pushdictionary(keyValues);
    }
    
    // ==================== ROBOX TYPES ====================
    bool pushvector2(float x, float y) {
        return bytecodePusher.pushvector2(x, y);
    }
    
    bool pushvector3(float x, float y, float z) {
        return bytecodePusher.pushvector3(x, y, z);
    }
    
    bool pushinstance(const std::string& className,
                     const std::vector<std::pair<std::string, std::string>>& props = {}) {
        // Create instance bytecode
        std::string bytecode = Bytecode::CreatePushInstance(className, props);
        return bytecodePusher.executeBytecode(bytecode);
    }
    
    // ==================== UTILITIES ====================
    bool checkTValueSafe() {
        // Check if we can safely use TValue operations
        // This could check if offsets are valid, memory is writable, etc.
        TValue* top = getTopPtr();
        TValue** lastPtr = reinterpret_cast<TValue**>(
            reinterpret_cast<uintptr_t>(L) + STACK_LAST_OFFSET);
        TValue* last = *lastPtr;
        
        return top && last && (top < last);
    }
    
    void setMode(PushMode newMode) {
        mode = newMode;
    }
    
    PushMode getMode() const {
        return mode;
    }
    
    BytecodePusher& getBytecodePusher() {
        return bytecodePusher;
    }
    
    // ==================== STACK MANIPULATION ====================
    int gettop() {
        TValue** stackBasePtr = reinterpret_cast<TValue**>(
            reinterpret_cast<uintptr_t>(L) + STACK_BASE_OFFSET);
        TValue* stack = *stackBasePtr;
        TValue* top = getTopPtr();
        return static_cast<int>(top - stack);
    }
    
    void pop(int n = 1) {
        TValue* top = getTopPtr();
        setTopPtr(top - n);
    }
    
    void settop(int index) {
        TValue** stackBasePtr = reinterpret_cast<TValue**>(
            reinterpret_cast<uintptr_t>(L) + STACK_BASE_OFFSET);
        TValue* stack = *stackBasePtr;
        setTopPtr(stack + index);
    }
    
    bool checkstack(int needed) {
        TValue* top = getTopPtr();
        TValue** lastPtr = reinterpret_cast<TValue**>(
            reinterpret_cast<uintptr_t>(L) + STACK_LAST_OFFSET);
        TValue* last = *lastPtr;
        
        return (top + needed) <= last;
    }
};

// ==================== SIMPLE WRAPPER ====================
class Pusher {
private:
    PushEngine engine;
    
public:
    Pusher(lua_State* L) : engine(L) {}
    
    // Template push for any type
    template<typename T>
    bool push(T value) {
        if constexpr (std::is_same_v<T, std::nullptr_t>) {
            return engine.pushnil();
        } else if constexpr (std::is_same_v<T, bool>) {
            return engine.pushboolean(value);
        } else if constexpr (std::is_integral_v<T>) {
            return engine.pushinteger(static_cast<int>(value));
        } else if constexpr (std::is_floating_point_v<T>) {
            return engine.pushnumber(static_cast<double>(value));
        } else if constexpr (std::is_same_v<T, const char*> || std::is_same_v<T, char*>) {
            return engine.pushstring(value);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return engine.pushstring(value);
        } else {
            return engine.pushnil();
        }
    }
    
    // Convenience methods
    bool nil() { return engine.pushnil(); }
    bool boolean(bool v) { return engine.pushboolean(v); }
    bool number(double v) { return engine.pushnumber(v); }
    bool integer(int v) { return engine.pushinteger(v); }
    bool string(const std::string& v) { return engine.pushstring(v); }
    bool string(const char* v) { return engine.pushstring(v); }
    
    // Table methods
    bool table(int arraySize = 0, int hashSize = 0) {
        return engine.pushtable(arraySize, hashSize);
    }
    
    bool array(const std::vector<std::string>& values) {
        return engine.pusharray(values);
    }
    
    // Access to underlying engine
    PushEngine& getEngine() { return engine; }
};

} // namespace tsunami

#endif // TSUNAMI_PUSH_HPP

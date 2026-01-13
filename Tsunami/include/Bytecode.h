#ifndef BYTECODE_H
#define BYTECODE_H

#include <string>
#include <vector>
#include <cstdint>
#include <utility>
#include <unordered_map>

namespace Bytecode {

// ==================== CONSTANT TYPES ====================
enum ConstantType : uint8_t {
    LBC_CONSTANT_NIL = 0,
    LBC_CONSTANT_BOOLEAN = 1,
    LBC_CONSTANT_NUMBER = 2,
    LBC_CONSTANT_STRING = 3,
    LBC_CONSTANT_IMPORT = 4,
    LBC_CONSTANT_TABLE = 5,
    LBC_CONSTANT_CLOSURE = 6,
};

// ==================== OPCODE ENUM (Luau) ====================
enum LuauOpcode : uint8_t {
    LOP_NOP = 0,
    LOP_LOADNIL = 1,
    LOP_LOADB = 2,
    LOP_LOADN = 3,
    LOP_LOADK = 4,
    LOP_MOVE = 5,
    LOP_GETGLOBAL = 6,
    LOP_SETGLOBAL = 7,
    LOP_GETUPVAL = 8,
    LOP_SETUPVAL = 9,
    LOP_CLOSEUPVALS = 10,
    LOP_GETIMPORT = 11,
    LOP_GETTABLE = 12,
    LOP_SETTABLE = 13,
    LOP_GETTABLKS = 14,
    LOP_SETTABLKS = 15,
    LOP_NAMECALL = 16,
    LOP_CALL = 17,
    LOP_RETURN = 18,
    LOP_JUMP = 19,
    LOP_JUMPBACK = 20,
    LOP_JUMPIF = 21,
    LOP_JUMPIFNOT = 22,
    LOP_JUMPIFEQ = 23,
    LOP_JUMPIFLE = 24,
    LOP_JUMPIFLT = 25,
    LOP_JUMPIFNOTEQ = 26,
    LOP_JUMPIFNOTLE = 27,
    LOP_JUMPIFNOTLT = 28,
    LOP_ADD = 29,
    LOP_SUB = 30,
    LOP_MUL = 31,
    LOP_DIV = 32,
    LOP_MOD = 33,
    LOP_POW = 34,
    LOP_ADDK = 35,
    LOP_SUBK = 36,
    LOP_MULK = 37,
    LOP_DIVK = 38,
    LOP_MODK = 39,
    LOP_POWK = 40,
    LOP_CONCAT = 41,
    LOP_NOT = 42,
    LOP_MINUS = 43,
    LOP_LENGTH = 44,
    LOP_NEWTABLE = 45,
    LOP_DUPTABLE = 46,
    LOP_SETLIST = 47,
    LOP_FORNPREP = 48,
    LOP_FORNLOOP = 49,
    LOP_FORGLOOP = 50,
    LOP_FORGPREP_INEXT = 51,
    LOP_FORGPREP_NEXT = 52,
    LOP_AND = 53,
    LOP_ANDK = 54,
    LOP_OR = 55,
    LOP_ORK = 56,
    LOP_COVERAGE = 57,
    LOP_GETTABLEN = 58,
    LOP_SETTABLEN = 59,
    LOP_FASTCALL = 60,
    LOP_FASTCALL1 = 61,
    LOP_FASTCALL2 = 62,
    LOP_FASTCALL2K = 63,
    LOP_FASTCALL3 = 64,
    LOP_FORGPREP = 65,
    LOP_JUMPIFEQK = 66,
    LOP_JUMPIFNOTEQK = 67,
    LOP_LOADKX = 68,
    LOP_FASTCALL2M = 69,
    LOP_CAPTURE = 70,
    LOP_JUMPX = 71,
    LOP_FASTCALLM = 72,
};

// ==================== BYTECODE HEADER STRUCT ====================
#pragma pack(push, 1)
struct LuauBytecodeHeader {
    uint8_t version;      // 0x02 for current Luau
    uint8_t flags;        // Compilation flags
    uint8_t typesize;     // sizeof(LUA_TYPE) usually 8
    uint8_t numbersize;   // sizeof(LUA_NUMBER) usually 8
    uint32_t hash;        // Bytecode hash
    uint32_t size;        // Size of bytecode data
};

struct RobloxSignature {
    uint8_t magic[4];     // "RBX2"
    uint32_t sig1;        // Signature part 1
    uint32_t sig2;        // Signature part 2  
    uint32_t sig3;        // Signature part 3
    uint32_t sig4;        // Signature part 4
};
#pragma pack(pop)

// ==================== PUBLIC API ====================

// Basic compilation
std::string Compile(const std::string& source);
std::string Decompress(const std::string& signedBytecode);

// Push operations
std::string CreatePushNil();
std::string CreatePushBoolean(bool value);
std::string CreatePushNumber(double value);
std::string CreatePushString(const std::string& value);
std::string CreatePushInteger(int64_t value);

// Table operations
std::string CreatePushTable(int arraySize = 0, int hashSize = 0);
std::string CreatePushArray(const std::vector<std::string>& values);
std::string CreatePushDictionary(const std::vector<std::pair<std::string, std::string>>& keyValues);
std::string CreatePushMultiple(const std::vector<std::string>& values);

// Roblox-specific types
std::string CreatePushVector2(float x, float y);
std::string CreatePushVector3(float x, float y, float z);
std::string CreatePushCFrame(float px, float py, float pz, float rx = 0, float ry = 0, float rz = 0, float rw = 1);
std::string CreatePushColor3(float r, float g, float b);
std::string CreatePushUDim(float scale, int offset);
std::string CreatePushUDim2(float sx, int ox, float sy, int oy);
std::string CreatePushBrickColor(int colorId);
std::string CreatePushInstance(const std::string& className, 
                               const std::vector<std::pair<std::string, std::string>>& properties = {});

// Function calls
std::string CreateFunctionCall(const std::string& functionName, 
                               const std::vector<std::string>& args = {},
                               int numReturns = 1);

// Utility
std::string HexDump(const std::string& data, size_t maxBytes = 64);
bool ValidateBytecode(const std::string& bytecode);
std::string GetBytecodeInfo(const std::string& bytecode);

// Bytecode cache for repeated operations
class BytecodeCache {
private:
    std::unordered_map<bool, std::string> boolCache;
    std::unordered_map<double, std::string> numberCache;
    std::unordered_map<std::string, std::string> stringCache;
    std::unordered_map<int, std::string> integerCache;
    
public:
    std::string getBoolean(bool value);
    std::string getNumber(double value);
    std::string getString(const std::string& value);
    std::string getInteger(int value);
    void clear();
};

// Advanced compiler
class Compiler {
private:
    std::vector<uint8_t> bytecode;
    std::vector<std::string> constants;
    std::vector<double> numberConstants;
    std::vector<bool> boolConstants;
    
    void writeVarInt(uint32_t value);
    void writeString(const std::string& str);
    void writeDouble(double value);
    void writeByte(uint8_t value);
    uint8_t encodeOpcode(uint8_t opcode);
    int addConstant(const std::string& value);
    int addNumberConstant(double value);
    int addBoolConstant(bool value);
    
public:
    Compiler();
    
    // Building instructions
    void addLoadNil(int reg);
    void addLoadBool(int reg, bool value, int jump = 0);
    void addLoadConst(int reg, int constIdx);
    void addLoadK(int reg, int constIdx);
    void addMove(int dest, int src);
    void addNewTable(int reg, int arraySize, int hashSize);
    void addSetTable(int tableReg, int keyReg, int valueReg);
    void addSetList(int tableReg, int startReg, int count, int tableIndex = 0);
    void addReturn(int startReg, int count);
    void addCall(int funcReg, int argCount, int resultCount);
    
    // High-level operations
    void pushNil();
    void pushBoolean(bool value);
    void pushNumber(double value);
    void pushString(const std::string& value);
    void pushTable(int arraySize = 0, int hashSize = 0);
    void pushArray(const std::vector<std::string>& values);
    
    // Finalization
    std::string compile();
    std::string getBytecode() const;
    void clear();
    
    // Debug
    void print() const;
};

} // namespace Bytecode

#endif // BYTECODE_H

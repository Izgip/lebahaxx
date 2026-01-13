#include "Bytecode.h"
#include <cstring>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace Bytecode {

// ==================== UTILITY FUNCTIONS ====================

static uint32_t hashBytecode(const uint8_t* data, size_t size) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < size; i++) {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}

static void writeVarInt(uint32_t value, std::vector<uint8_t>& out) {
    do {
        uint8_t byte = value & 0x7F;
        value >>= 7;
        if (value != 0) byte |= 0x80;
        out.push_back(byte);
    } while (value != 0);
}

static uint8_t encodeOpcode(uint8_t opcode) {
    return (opcode * 227) & 0xFF;  // Same as your Rust code
}

static bool isDoubleByteOpcode(uint8_t opcode) {
    switch (opcode) {
        case LOP_GETGLOBAL:
        case LOP_SETGLOBAL:
        case LOP_GETIMPORT:
        case LOP_GETTABLKS:
        case LOP_SETTABLKS:
        case LOP_NAMECALL:
        case LOP_JUMPIFEQ:
        case LOP_JUMPIFLE:
        case LOP_JUMPIFLT:
        case LOP_JUMPIFNOTEQ:
        case LOP_JUMPIFNOTLE:
        case LOP_JUMPIFNOTLT:
        case LOP_NEWTABLE:
        case LOP_SETLIST:
        case LOP_FORGLOOP:
        case LOP_LOADKX:
        case LOP_JUMPIFEQK:
        case LOP_JUMPIFNOTEQK:
        case LOP_FASTCALL2:
        case LOP_FASTCALL2K:
            return true;
        default:
            return false;
    }
}

// ==================== BASIC PUSH OPERATIONS ====================

std::string CreatePushNil() {
    std::vector<uint8_t> bytecode;
    
    // Header
    LuauBytecodeHeader header = {0x02, 0x00, 0x08, 0x08, 0, 0};
    bytecode.insert(bytecode.end(), 
                   reinterpret_cast<uint8_t*>(&header),
                   reinterpret_cast<uint8_t*>(&header) + sizeof(header));
    
    // Constants: 0
    writeVarInt(0, bytecode);
    
    // Functions: 1
    writeVarInt(1, bytecode);
    
    // Function proto
    writeVarInt(0, bytecode);  // maxstacksize
    writeVarInt(0, bytecode);  // numparams
    writeVarInt(1, bytecode);  // numupvalues
    writeVarInt(0, bytecode);  // is_vararg
    
    // Instructions: 1
    writeVarInt(1, bytecode);
    
    // LOADNIL opcode
    bytecode.push_back(encodeOpcode(LOP_LOADNIL));
    bytecode.push_back(0x00);  // Register A
    
    // SizeK: 0
    writeVarInt(0, bytecode);
    
    // SizeP: 0
    writeVarInt(0, bytecode);
    
    // Debug info
    writeVarInt(0, bytecode);  // linedefined
    writeVarInt(0, bytecode);  // debugname
    bytecode.push_back(0x00);  // lineinfo
    bytecode.push_back(0x00);  // debuginfo
    
    // Update header
    header.size = bytecode.size() - sizeof(header);
    header.hash = hashBytecode(bytecode.data() + sizeof(header), header.size);
    std::memcpy(bytecode.data(), &header, sizeof(header));
    
    return std::string(reinterpret_cast<char*>(bytecode.data()), bytecode.size());
}

std::string CreatePushBoolean(bool value) {
    std::vector<uint8_t> bytecode;
    
    // Header
    LuauBytecodeHeader header = {0x02, 0x00, 0x08, 0x08, 0, 0};
    bytecode.insert(bytecode.end(), 
                   reinterpret_cast<uint8_t*>(&header),
                   reinterpret_cast<uint8_t*>(&header) + sizeof(header));
    
    // Constants: 0 (boolean is inline in LOADB)
    writeVarInt(0, bytecode);
    
    // Functions: 1
    writeVarInt(1, bytecode);
    
    // Function proto
    writeVarInt(1, bytecode);  // maxstacksize
    writeVarInt(0, bytecode);  // numparams
    writeVarInt(0, bytecode);  // numupvalues
    writeVarInt(0, bytecode);  // is_vararg
    
    // Instructions: 1
    writeVarInt(1, bytecode);
    
    // LOADB opcode
    bytecode.push_back(encodeOpcode(LOP_LOADB));
    bytecode.push_back(0x00);  // Register A
    bytecode.push_back(value ? 0x01 : 0x00);  // Boolean value
    bytecode.push_back(0x00);  // Jump (unused)
    
    // SizeK: 0
    writeVarInt(0, bytecode);
    
    // SizeP: 0
    writeVarInt(0, bytecode);
    
    // Debug info
    writeVarInt(0, bytecode);
    writeVarInt(0, bytecode);
    bytecode.push_back(0x00);
    bytecode.push_back(0x00);
    
    // Update header
    header.size = bytecode.size() - sizeof(header);
    header.hash = hashBytecode(bytecode.data() + sizeof(header), header.size);
    std::memcpy(bytecode.data(), &header, sizeof(header));
    
    return std::string(reinterpret_cast<char*>(bytecode.data()), bytecode.size());
}

std::string CreatePushNumber(double value) {
    std::vector<uint8_t> bytecode;
    
    // Header
    LuauBytecodeHeader header = {0x02, 0x00, 0x08, 0x08, 0, 0};
    bytecode.insert(bytecode.end(), 
                   reinterpret_cast<uint8_t*>(&header),
                   reinterpret_cast<uint8_t*>(&header) + sizeof(header));
    
    // Constants: 1 (the number)
    writeVarInt(1, bytecode);
    bytecode.push_back(LBC_CONSTANT_NUMBER);
    
    // Write double
    uint64_t bits;
    std::memcpy(&bits, &value, sizeof(double));
    for (int i = 0; i < 8; i++) {
        bytecode.push_back(static_cast<uint8_t>(bits & 0xFF));
        bits >>= 8;
    }
    
    // Functions: 1
    writeVarInt(1, bytecode);
    
    // Function proto
    writeVarInt(1, bytecode);  // maxstacksize
    writeVarInt(0, bytecode);  // numparams
    writeVarInt(0, bytecode);  // numupvalues
    writeVarInt(0, bytecode);  // is_vararg
    
    // Instructions: 1
    writeVarInt(1, bytecode);
    
    // LOADN opcode
    bytecode.push_back(encodeOpcode(LOP_LOADN));
    bytecode.push_back(0x00);  // Register A
    bytecode.push_back(0x00);  // Constant index 0
    
    // SizeK: 1
    writeVarInt(1, bytecode);
    
    // SizeP: 0
    writeVarInt(0, bytecode);
    
    // Debug info
    writeVarInt(0, bytecode);
    writeVarInt(0, bytecode);
    bytecode.push_back(0x00);
    bytecode.push_back(0x00);
    
    // Update header
    header.size = bytecode.size() - sizeof(header);
    header.hash = hashBytecode(bytecode.data() + sizeof(header), header.size);
    std::memcpy(bytecode.data(), &header, sizeof(header));
    
    return std::string(reinterpret_cast<char*>(bytecode.data()), bytecode.size());
}

std::string CreatePushString(const std::string& value) {
    std::vector<uint8_t> bytecode;
    
    // Header
    LuauBytecodeHeader header = {0x02, 0x00, 0x08, 0x08, 0, 0};
    bytecode.insert(bytecode.end(), 
                   reinterpret_cast<uint8_t*>(&header),
                   reinterpret_cast<uint8_t*>(&header) + sizeof(header));
    
    // Constants: 1 (the string)
    writeVarInt(1, bytecode);
    bytecode.push_back(LBC_CONSTANT_STRING);
    writeVarInt(value.size(), bytecode);
    bytecode.insert(bytecode.end(), value.begin(), value.end());
    
    // Functions: 1
    writeVarInt(1, bytecode);
    
    // Function proto
    writeVarInt(1, bytecode);  // maxstacksize
    writeVarInt(0, bytecode);  // numparams
    writeVarInt(0, bytecode);  // numupvalues
    writeVarInt(0, bytecode);  // is_vararg
    
    // Instructions: 1
    writeVarInt(1, bytecode);
    
    // LOADK opcode
    bytecode.push_back(encodeOpcode(LOP_LOADK));
    bytecode.push_back(0x00);  // Register A
    bytecode.push_back(0x00);  // Constant index 0
    
    // SizeK: 1
    writeVarInt(1, bytecode);
    
    // SizeP: 0
    writeVarInt(0, bytecode);
    
    // Debug info
    writeVarInt(0, bytecode);
    writeVarInt(0, bytecode);
    bytecode.push_back(0x00);
    bytecode.push_back(0x00);
    
    // Update header
    header.size = bytecode.size() - sizeof(header);
    header.hash = hashBytecode(bytecode.data() + sizeof(header), header.size);
    std::memcpy(bytecode.data(), &header, sizeof(header));
    
    return std::string(reinterpret_cast<char*>(bytecode.data()), bytecode.size());
}

// ==================== TABLE OPERATIONS ====================

std::string CreatePushTable(int arraySize, int hashSize) {
    std::vector<uint8_t> bytecode;
    
    // Header
    LuauBytecodeHeader header = {0x02, 0x00, 0x08, 0x08, 0, 0};
    bytecode.insert(bytecode.end(), 
                   reinterpret_cast<uint8_t*>(&header),
                   reinterpret_cast<uint8_t*>(&header) + sizeof(header));
    
    // Constants: 0
    writeVarInt(0, bytecode);
    
    // Functions: 1
    writeVarInt(1, bytecode);
    
    // Function proto
    writeVarInt(1, bytecode);  // maxstacksize
    writeVarInt(0, bytecode);  // numparams
    writeVarInt(0, bytecode);  // numupvalues
    writeVarInt(0, bytecode);  // is_vararg
    
    // Instructions: 1
    writeVarInt(1, bytecode);
    
    // NEWTABLE opcode (2-byte)
    bytecode.push_back(encodeOpcode(LOP_NEWTABLE));
    bytecode.push_back(0x00);  // Register A
    bytecode.push_back(static_cast<uint8_t>(arraySize & 0xFF));  // Array size
    bytecode.push_back(static_cast<uint8_t>(hashSize & 0xFF));   // Hash size
    
    // SizeK: 0
    writeVarInt(0, bytecode);
    
    // SizeP: 0
    writeVarInt(0, bytecode);
    
    // Debug info
    writeVarInt(0, bytecode);
    writeVarInt(0, bytecode);
    bytecode.push_back(0x00);
    bytecode.push_back(0x00);
    
    // Update header
    header.size = bytecode.size() - sizeof(header);
    header.hash = hashBytecode(bytecode.data() + sizeof(header), header.size);
    std::memcpy(bytecode.data(), &header, sizeof(header));
    
    return std::string(reinterpret_cast<char*>(bytecode.data()), bytecode.size());
}

std::string CreatePushArray(const std::vector<std::string>& values) {
    if (values.empty()) {
        return CreatePushTable(0, 0);
    }
    
    std::vector<uint8_t> bytecode;
    
    // Header
    LuauBytecodeHeader header = {0x02, 0x00, 0x08, 0x08, 0, 0};
    bytecode.insert(bytecode.end(), 
                   reinterpret_cast<uint8_t*>(&header),
                   reinterpret_cast<uint8_t*>(&header) + sizeof(header));
    
    // Constants: all string values
    writeVarInt(values.size(), bytecode);
    for (const auto& value : values) {
        bytecode.push_back(LBC_CONSTANT_STRING);
        writeVarInt(value.size(), bytecode);
        bytecode.insert(bytecode.end(), value.begin(), value.end());
    }
    
    // Functions: 1
    writeVarInt(1, bytecode);
    
    // Function proto
    writeVarInt(1 + values.size(), bytecode);  // maxstacksize (table + values)
    writeVarInt(0, bytecode);  // numparams
    writeVarInt(0, bytecode);  // numupvalues
    writeVarInt(0, bytecode);  // is_vararg
    
    // Instructions: values.size() + 2 (NEWTABLE + LOADKs + SETLIST)
    writeVarInt(values.size() + 2, bytecode);
    
    // 1. NEWTABLE
    bytecode.push_back(encodeOpcode(LOP_NEWTABLE));
    bytecode.push_back(0x00);  // Register 0 = table
    bytecode.push_back(static_cast<uint8_t>(values.size()));  // Array size
    bytecode.push_back(0x00);  // Hash size 0
    
    // 2. Load each constant into consecutive registers
    for (size_t i = 0; i < values.size(); i++) {
        bytecode.push_back(encodeOpcode(LOP_LOADK));
        bytecode.push_back(static_cast<uint8_t>(i + 1));  // Registers 1, 2, 3...
        bytecode.push_back(static_cast<uint8_t>(i));      // Constant index
    }
    
    // 3. SETLIST to populate array
    bytecode.push_back(encodeOpcode(LOP_SETLIST));
    bytecode.push_back(0x00);  // Table register
    bytecode.push_back(static_cast<uint8_t>(values.size()));  // Value count
    bytecode.push_back(0x00);  // Table index (start at 1)
    
    // SizeK: values.size()
    writeVarInt(values.size(), bytecode);
    
    // SizeP: 0
    writeVarInt(0, bytecode);
    
    // Debug info
    writeVarInt(0, bytecode);
    writeVarInt(0, bytecode);
    bytecode.push_back(0x00);
    bytecode.push_back(0x00);
    
    // Update header
    header.size = bytecode.size() - sizeof(header);
    header.hash = hashBytecode(bytecode.data() + sizeof(header), header.size);
    std::memcpy(bytecode.data(), &header, sizeof(header));
    
    return std::string(reinterpret_cast<char*>(bytecode.data()), bytecode.size());
}

// ==================== MULTIPLE VALUES ====================

std::string CreatePushMultiple(const std::vector<std::string>& values) {
    if (values.empty()) {
        return CreatePushNil();
    }
    
    std::vector<uint8_t> bytecode;
    
    // Header
    LuauBytecodeHeader header = {0x02, 0x00, 0x08, 0x08, 0, 0};
    bytecode.insert(bytecode.end(), 
                   reinterpret_cast<uint8_t*>(&header),
                   reinterpret_cast<uint8_t*>(&header) + sizeof(header));
    
    // Process values to determine types
    std::vector<bool> isNumber(values.size(), false);
    std::vector<bool> isBool(values.size(), false);
    std::vector<double> numbers;
    std::vector<bool> booleans;
    std::vector<std::string> strings;
    
    for (size_t i = 0; i < values.size(); i++) {
        const auto& val = values[i];
        if (val == "true") {
            isBool[i] = true;
            booleans.push_back(true);
        } else if (val == "false") {
            isBool[i] = true;
            booleans.push_back(false);
        } else {
            // Try to parse as number
            char* end;
            double num = strtod(val.c_str(), &end);
            if (end != val.c_str() && *end == '\0') {
                isNumber[i] = true;
                numbers.push_back(num);
            } else {
                strings.push_back(val);
            }
        }
    }
    
    // Write constants
    uint32_t totalConstants = numbers.size() + booleans.size() + strings.size();
    writeVarInt(totalConstants, bytecode);
    
    // Numbers
    for (double num : numbers) {
        bytecode.push_back(LBC_CONSTANT_NUMBER);
        uint64_t bits;
        std::memcpy(&bits, &num, sizeof(double));
        for (int i = 0; i < 8; i++) {
            bytecode.push_back(static_cast<uint8_t>(bits & 0xFF));
            bits >>= 8;
        }
    }
    
    // Booleans (won't actually be in constants, but we count them)
    for (bool b : booleans) {
        bytecode.push_back(LBC_CONSTANT_BOOLEAN);
        bytecode.push_back(b ? 0x01 : 0x00);
    }
    
    // Strings
    for (const auto& str : strings) {
        bytecode.push_back(LBC_CONSTANT_STRING);
        writeVarInt(str.size(), bytecode);
        bytecode.insert(bytecode.end(), str.begin(), str.end());
    }
    
    // Functions: 1
    writeVarInt(1, bytecode);
    
    // Function proto
    writeVarInt(values.size(), bytecode);  // maxstacksize
    writeVarInt(0, bytecode);  // numparams
    writeVarInt(0, bytecode);  // numupvalues
    writeVarInt(0, bytecode);  // is_vararg
    
    // Instructions: values.size()
    writeVarInt(values.size(), bytecode);
    
    // Generate load instructions
    size_t numIdx = 0, boolIdx = 0, strIdx = 0;
    
    for (size_t i = 0; i < values.size(); i++) {
        if (isBool[i]) {
            // LOADB
            bytecode.push_back(encodeOpcode(LOP_LOADB));
            bytecode.push_back(static_cast<uint8_t>(i));  // Register
            bytecode.push_back(booleans[boolIdx++] ? 0x01 : 0x00);
            bytecode.push_back(0x00);
        } else if (isNumber[i]) {
            // LOADN
            bytecode.push_back(encodeOpcode(LOP_LOADN));
            bytecode.push_back(static_cast<uint8_t>(i));  // Register
            bytecode.push_back(static_cast<uint8_t>(numIdx++));  // Constant index
        } else {
            // LOADK for string
            bytecode.push_back(encodeOpcode(LOP_LOADK));
            bytecode.push_back(static_cast<uint8_t>(i));  // Register
            bytecode.push_back(static_cast<uint8_t>(numbers.size() + booleans.size() + strIdx++));  // Constant index
        }
    }
    
    // SizeK: totalConstants
    writeVarInt(totalConstants, bytecode);
    
    // SizeP: 0
    writeVarInt(0, bytecode);
    
    // Debug info
    writeVarInt(0, bytecode);
    writeVarInt(0, bytecode);
    bytecode.push_back(0x00);
    bytecode.push_back(0x00);
    
    // Update header
    header.size = bytecode.size() - sizeof(header);
    header.hash = hashBytecode(bytecode.data() + sizeof(header), header.size);
    std::memcpy(bytecode.data(), &header, sizeof(header));
    
    return std::string(reinterpret_cast<char*>(bytecode.data()), bytecode.size());
}

// ==================== ROBOX-SPECIFIC TYPES ====================

std::string CreatePushVector2(float x, float y) {
    std::vector<uint8_t> bytecode;
    
    // Header
    LuauBytecodeHeader header = {0x02, 0x00, 0x08, 0x08, 0, 0};
    bytecode.insert(bytecode.end(), 
                   reinterpret_cast<uint8_t*>(&header),
                   reinterpret_cast<uint8_t*>(&header) + sizeof(header));
    
    // Constants: 2 numbers (x, y)
    writeVarInt(2, bytecode);
    
    // X constant
    bytecode.push_back(LBC_CONSTANT_NUMBER);
    double dx = x;
    uint64_t bits;
    std::memcpy(&bits, &dx, sizeof(double));
    for (int i = 0; i < 8; i++) {
        bytecode.push_back(static_cast<uint8_t>(bits & 0xFF));
        bits >>= 8;
    }
    
    // Y constant  
    bytecode.push_back(LBC_CONSTANT_NUMBER);
    double dy = y;
    std::memcpy(&bits, &dy, sizeof(double));
    for (int i = 0; i < 8; i++) {
        bytecode.push_back(static_cast<uint8_t>(bits & 0xFF));
        bits >>= 8;
    }
    
    // Functions: 1
    writeVarInt(1, bytecode);
    
    // Function proto
    writeVarInt(3, bytecode);  // maxstacksize (vector + x + y)
    writeVarInt(0, bytecode);  // numparams
    writeVarInt(0, bytecode);  // numupvalues
    writeVarInt(0, bytecode);  // is_vararg
    
    // Instructions: 5 (load x, load y, get Vector2, call new)
    writeVarInt(5, bytecode);
    
    // 1. Load X into reg 1
    bytecode.push_back(encodeOpcode(LOP_LOADN));
    bytecode.push_back(0x01);
    bytecode.push_back(0x00);
    
    // 2. Load Y into reg 2
    bytecode.push_back(encodeOpcode(LOP_LOADN));
    bytecode.push_back(0x02);
    bytecode.push_back(0x01);
    
    // 3. Get Vector2 global
    // Need to add "Vector2" string constant
    // This is simplified - actual implementation would need proper constant handling
    
    // For now, return a simple table
    return CreatePushTable(2, 0);
}

std::string CreatePushVector3(float x, float y, float z) {
    // Similar to Vector2 but with 3 values
    return CreatePushTable(3, 0);
}

// ==================== UTILITY FUNCTIONS ====================

std::string HexDump(const std::string& data, size_t maxBytes) {
    std::ostringstream oss;
    size_t size = std::min(data.size(), maxBytes);
    
    for (size_t i = 0; i < size; i++) {
        oss << std::hex << std::setw(2) << std::setfill('0') 
            << (static_cast<int>(data[i]) & 0xFF) << " ";
        if ((i + 1) % 16 == 0) oss << "\n";
    }
    if (size % 16 != 0) oss << "\n";
    
    return oss.str();
}

bool ValidateBytecode(const std::string& bytecode) {
    if (bytecode.size() < sizeof(LuauBytecodeHeader)) {
        return false;
    }
    
    const LuauBytecodeHeader* header = 
        reinterpret_cast<const LuauBytecodeHeader*>(bytecode.data());
    
    if (header->version != 0x02) {
        return false;
    }
    
    if (header->size != bytecode.size() - sizeof(LuauBytecodeHeader)) {
        return false;
    }
    
    // Check hash
    uint32_t calculated = hashBytecode(
        reinterpret_cast<const uint8_t*>(bytecode.data() + sizeof(LuauBytecodeHeader)),
        header->size
    );
    
    return header->hash == calculated;
}

std::string Decompress(const std::string& signedBytecode) {
    // Check for Roblox signature
    if (signedBytecode.size() >= 16) {
        const RobloxSignature* sig = 
            reinterpret_cast<const RobloxSignature*>(signedBytecode.data());
        
        // Check for RBX2 magic
        if (sig->magic[0] == 'R' && sig->magic[1] == 'B' && 
            sig->magic[2] == 'X' && sig->magic[3] == '2') {
            // Skip signature
            return signedBytecode.substr(sizeof(RobloxSignature));
        }
    }
    
    // No signature, return as-is
    return signedBytecode;
}

// ==================== BYTECODE CACHE ====================

std::string BytecodeCache::getBoolean(bool value) {
    auto it = boolCache.find(value);
    if (it != boolCache.end()) return it->second;
    
    std::string bytecode = CreatePushBoolean(value);
    boolCache[value] = bytecode;
    return bytecode;
}

std::string BytecodeCache::getNumber(double value) {
    auto it = numberCache.find(value);
    if (it != numberCache.end()) return it->second;
    
    std::string bytecode = CreatePushNumber(value);
    numberCache[value] = bytecode;
    return bytecode;
}

std::string BytecodeCache::getString(const std::string& value) {
    auto it = stringCache.find(value);
    if (it != stringCache.end()) return it->second;
    
    std::string bytecode = CreatePushString(value);
    stringCache[value] = bytecode;
    return bytecode;
}

std::string BytecodeCache::getInteger(int value) {
    auto it = integerCache.find(value);
    if (it != integerCache.end()) return it->second;
    
    std::string bytecode = CreatePushNumber(static_cast<double>(value));
    integerCache[value] = bytecode;
    return bytecode;
}

void BytecodeCache::clear() {
    boolCache.clear();
    numberCache.clear();
    stringCache.clear();
    integerCache.clear();
}

// ==================== COMPILER CLASS ====================

Compiler::Compiler() {
    // Initialize with header
    LuauBytecodeHeader header = {0x02, 0x00, 0x08, 0x08, 0, 0};
    bytecode.insert(bytecode.end(), 
                   reinterpret_cast<uint8_t*>(&header),
                   reinterpret_cast<uint8_t*>(&header) + sizeof(header));
}

void Compiler::writeVarInt(uint32_t value) {
    do {
        uint8_t byte = value & 0x7F;
        value >>= 7;
        if (value != 0) byte |= 0x80;
        bytecode.push_back(byte);
    } while (value != 0);
}

void Compiler::writeString(const std::string& str) {
    writeVarInt(str.size());
    bytecode.insert(bytecode.end(), str.begin(), str.end());
}

void Compiler::writeDouble(double value) {
    uint64_t bits;
    std::memcpy(&bits, &value, sizeof(double));
    for (int i = 0; i < 8; i++) {
        bytecode.push_back(static_cast<uint8_t>(bits & 0xFF));
        bits >>= 8;
    }
}

void Compiler::writeByte(uint8_t value) {
    bytecode.push_back(value);
}

uint8_t Compiler::encodeOpcode(uint8_t opcode) {
    return (opcode * 227) & 0xFF;
}

int Compiler::addConstant(const std::string& value) {
    constants.push_back(value);
    return constants.size() - 1;
}

int Compiler::addNumberConstant(double value) {
    numberConstants.push_back(value);
    return numberConstants.size() - 1;
}

int Compiler::addBoolConstant(bool value) {
    boolConstants.push_back(value);
    return boolConstants.size() - 1;
}

void Compiler::addLoadNil(int reg) {
    bytecode.push_back(encodeOpcode(LOP_LOADNIL));
    bytecode.push_back(static_cast<uint8_t>(reg));
}

void Compiler::addLoadBool(int reg, bool value, int jump) {
    bytecode.push_back(encodeOpcode(LOP_LOADB));
    bytecode.push_back(static_cast<uint8_t>(reg));
    bytecode.push_back(value ? 0x01 : 0x00);
    bytecode.push_back(static_cast<uint8_t>(jump));
}

void Compiler::pushNil() {
    // Add LOADNIL instruction
    bytecode.push_back(encodeOpcode(LOP_LOADNIL));
    bytecode.push_back(0x00);  // Register 0
}

void Compiler::pushBoolean(bool value) {
    // Add LOADB instruction
    bytecode.push_back(encodeOpcode(LOP_LOADB));
    bytecode.push_back(0x00);  // Register 0
    bytecode.push_back(value ? 0x01 : 0x00);
    bytecode.push_back(0x00);  // No jump
}

void Compiler::pushNumber(double value) {
    // Add number constant and LOADN instruction
    int constIdx = addNumberConstant(value);
    
    bytecode.push_back(encodeOpcode(LOP_LOADN));
    bytecode.push_back(0x00);  // Register 0
    bytecode.push_back(static_cast<uint8_t>(constIdx));
}

void Compiler::pushString(const std::string& value) {
    // Add string constant and LOADK instruction
    int constIdx = addConstant(value);
    
    bytecode.push_back(encodeOpcode(LOP_LOADK));
    bytecode.push_back(0x00);  // Register 0
    bytecode.push_back(static_cast<uint8_t>(constIdx));
}

void Compiler::pushTable(int arraySize, int hashSize) {
    // Add NEWTABLE instruction
    bytecode.push_back(encodeOpcode(LOP_NEWTABLE));
    bytecode.push_back(0x00);  // Register 0
    bytecode.push_back(static_cast<uint8_t>(arraySize));
    bytecode.push_back(static_cast<uint8_t>(hashSize));
}

void Compiler::pushArray(const std::vector<std::string>& values) {
    if (values.empty()) {
        pushTable(0, 0);
        return;
    }
    
    // Create table with array size
    pushTable(values.size(), 0);
    
    // Add each value as constant
    for (size_t i = 0; i < values.size(); i++) {
        int constIdx = addConstant(values[i]);
        
        // Load constant into register i+1
        bytecode.push_back(encodeOpcode(LOP_LOADK));
        bytecode.push_back(static_cast<uint8_t>(i + 1));
        bytecode.push_back(static_cast<uint8_t>(constIdx));
    }
    
    // Add SETLIST to populate array
    bytecode.push_back(encodeOpcode(LOP_SETLIST));
    bytecode.push_back(0x00);  // Table register
    bytecode.push_back(static_cast<uint8_t>(values.size()));
    bytecode.push_back(0x00);  // Start index
}

void Compiler::addNewTable(int reg, int arraySize, int hashSize) {
    bytecode.push_back(encodeOpcode(LOP_NEWTABLE));
    bytecode.push_back(static_cast<uint8_t>(reg));
    bytecode.push_back(static_cast<uint8_t>(arraySize));
    bytecode.push_back(static_cast<uint8_t>(hashSize));
}

void Compiler::addSetTable(int tableReg, int keyReg, int valueReg) {
    bytecode.push_back(encodeOpcode(LOP_SETTABLE));
    bytecode.push_back(static_cast<uint8_t>(tableReg));
    bytecode.push_back(static_cast<uint8_t>(keyReg));
    bytecode.push_back(static_cast<uint8_t>(valueReg));
}

void Compiler::addSetList(int tableReg, int startReg, int count, int tableIndex) {
    bytecode.push_back(encodeOpcode(LOP_SETLIST));
    bytecode.push_back(static_cast<uint8_t>(tableReg));
    bytecode.push_back(static_cast<uint8_t>(count));
    bytecode.push_back(static_cast<uint8_t>(tableIndex));
}

void Compiler::addReturn(int startReg, int count) {
    bytecode.push_back(encodeOpcode(LOP_RETURN));
    bytecode.push_back(static_cast<uint8_t>(startReg));
    bytecode.push_back(static_cast<uint8_t>(count));
}

void Compiler::addCall(int funcReg, int argCount, int resultCount) {
    bytecode.push_back(encodeOpcode(LOP_CALL));
    bytecode.push_back(static_cast<uint8_t>(funcReg));
    bytecode.push_back(static_cast<uint8_t>(argCount + 1));  // +1 for function itself
    bytecode.push_back(static_cast<uint8_t>(resultCount + 1));  // +1 for function
}

void Compiler::addLoadConst(int reg, int constIdx) {
    bytecode.push_back(encodeOpcode(LOP_LOADK));
    bytecode.push_back(static_cast<uint8_t>(reg));
    bytecode.push_back(static_cast<uint8_t>(constIdx));
}

void Compiler::addMove(int dest, int src) {
    bytecode.push_back(encodeOpcode(LOP_MOVE));
    bytecode.push_back(static_cast<uint8_t>(dest));
    bytecode.push_back(static_cast<uint8_t>(src));
}

std::string Compiler::compile() {
    // Start over with fresh bytecode
    std::vector<uint8_t> result;
    
    // Write header placeholder
    LuauBytecodeHeader header = {0x02, 0x00, 0x08, 0x08, 0, 0};
    result.insert(result.end(), 
                 reinterpret_cast<uint8_t*>(&header),
                 reinterpret_cast<uint8_t*>(&header) + sizeof(header));
    
    // Write constants
    writeVarInt(constants.size() + numberConstants.size() + boolConstants.size());
    
    // Write number constants
    for (double num : numberConstants) {
        result.push_back(LBC_CONSTANT_NUMBER);
        writeDouble(num);
    }
    
    // Write boolean constants
    for (bool b : boolConstants) {
        result.push_back(LBC_CONSTANT_BOOLEAN);
        result.push_back(b ? 0x01 : 0x00);
    }
    
    // Write string constants
    for (const auto& str : constants) {
        result.push_back(LBC_CONSTANT_STRING);
        writeVarInt(str.size());
        result.insert(result.end(), str.begin(), str.end());
    }
    
    // Write function proto
    // This is simplified - actual implementation would need proper proto structure
    
    // Update header
    header.size = result.size() - sizeof(header);
    header.hash = hashBytecode(result.data() + sizeof(header), header.size);
    std::memcpy(result.data(), &header, sizeof(header));
    
    return std::string(reinterpret_cast<char*>(result.data()), result.size());
}

std::string Compiler::getBytecode() const {
    return std::string(reinterpret_cast<const char*>(bytecode.data()), bytecode.size());
}

void Compiler::clear() {
    bytecode.clear();
    constants.clear();
    numberConstants.clear();
    boolConstants.clear();
    
    // Re-add header
    LuauBytecodeHeader header = {0x02, 0x00, 0x08, 0x08, 0, 0};
    bytecode.insert(bytecode.end(), 
                   reinterpret_cast<uint8_t*>(&header),
                   reinterpret_cast<uint8_t*>(&header) + sizeof(header));
}

// ==================== PUBLIC API WRAPPERS ====================

std::string Compile(const std::string& source) {
    // Very basic compilation - just handles simple return statements
    if (source.find("return ") == 0) {
        std::string value = source.substr(7);
        
        // Check for nil
        if (value == "nil") return CreatePushNil();
        
        // Check for boolean
        if (value == "true") return CreatePushBoolean(true);
        if (value == "false") return CreatePushBoolean(false);
        
        // Check for number
        char* end;
        double num = strtod(value.c_str(), &end);
        if (end != value.c_str() && *end == '\0') {
            return CreatePushNumber(num);
        }
        
        // Check for string (quoted)
        if (value.size() >= 2 && value[0] == '"' && value.back() == '"') {
            return CreatePushString(value.substr(1, value.size() - 2));
        }
    }
    
    // Default to nil
    return CreatePushNil();
}

} // namespace Bytecode

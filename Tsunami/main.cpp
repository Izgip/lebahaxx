// main.cpp
#include "Bytecode.h"
#include <iostream>
#include <fstream>

int main() {
    std::cout << "=== Bytecode Generator Test ===\n\n";
    
    // Test 1: Basic push operations
    std::cout << "1. Basic push operations:\n";
    auto nil_bc = Bytecode::CreatePushNil();
    std::cout << "Push nil: " << nil_bc.size() << " bytes\n";
    
    auto true_bc = Bytecode::CreatePushBoolean(true);
    std::cout << "Push true: " << true_bc.size() << " bytes\n";
    
    auto num_bc = Bytecode::CreatePushNumber(3.14159);
    std::cout << "Push number: " << num_bc.size() << " bytes\n";
    
    auto str_bc = Bytecode::CreatePushString("Hello World");
    std::cout << "Push string: " << str_bc.size() << " bytes\n";
    
    // Test 2: Table operations
    std::cout << "\n2. Table operations:\n";
    auto table_bc = Bytecode::CreatePushTable(5, 3);
    std::cout << "Empty table: " << table_bc.size() << " bytes\n";
    
    std::vector<std::string> items = {"sword", "shield", "potion"};
    auto array_bc = Bytecode::CreatePushArray(items);
    std::cout << "Array table: " << array_bc.size() << " bytes\n";
    
    // Test 3: Multiple values
    std::cout << "\n3. Multiple values:\n";
    std::vector<std::string> values = {"player1", "100", "true", "3.14"};
    auto multi_bc = Bytecode::CreatePushMultiple(values);
    std::cout << "Multiple push: " << multi_bc.size() << " bytes\n";
    
    // Test 4: Hex dump
    std::cout << "\n4. Bytecode hex dump (nil):\n";
    std::cout << Bytecode::HexDump(nil_bc, 32) << "\n";
    
    // Test 5: Validation
    std::cout << "5. Bytecode validation:\n";
    std::cout << "Nil bytecode valid: " << (Bytecode::ValidateBytecode(nil_bc) ? "YES" : "NO") << "\n";
    std::cout << "Random data valid: " << (Bytecode::ValidateBytecode("random") ? "YES" : "NO") << "\n";
    
    // Test 6: Cache
    std::cout << "\n6. Bytecode cache:\n";
    Bytecode::BytecodeCache cache;
    auto cached_num = cache.getNumber(42.0);
    auto cached_str = cache.getString("cached");
    std::cout << "Cached number size: " << cached_num.size() << " bytes\n";
    std::cout << "Cached string size: " << cached_str.size() << " bytes\n";
    
    // Test 7: Save to file
    std::cout << "\n7. Saving bytecode to files...\n";
    std::ofstream nil_file("nil.bc", std::ios::binary);
    nil_file.write(nil_bc.data(), nil_bc.size());
    nil_file.close();
    
    std::ofstream str_file("string.bc", std::ios::binary);
    str_file.write(str_bc.data(), str_bc.size());
    str_file.close();
    
    std::cout << "Saved nil.bc (" << nil_bc.size() << " bytes)\n";
    std::cout << "Saved string.bc (" << str_bc.size() << " bytes)\n";
    
    // Test 8: Compiler class
    std::cout << "\n8. Using Compiler class:\n";
    Bytecode::Compiler compiler;
    compiler.pushNil();
    compiler.pushNumber(100.0);
    compiler.pushString("test");
    auto compiled = compiler.compile();
    std::cout << "Compiler output: " << compiled.size() << " bytes\n";
    
    std::cout << "\n=== All tests completed ===\n";
    
    return 0;
}

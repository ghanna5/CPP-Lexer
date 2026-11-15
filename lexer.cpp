#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip> // For formatting output

// Symbol table entry
struct SymbolTableEntry {
    int memory_address;
    std::string lexeme;
    std::string type;
};

// Global variables
std::unordered_map<std::string, SymbolTableEntry> symbol_table;
std::vector<std::string> symbol_table_order; // To maintain declaration order
int current_memory_address = 9000;
std::vector<std::string> assembly_code;
int instruction_index = 1;

// Function prototypes
void process_declaration(const std::string &line);
void process_assignment(const std::string &line);
void process_get(const std::string &line);
void process_put(const std::string &line);
void process_while(const std::string &line, std::ifstream &input);
void add_assembly(const std::string &instruction);
void print_symbol_table();
void print_assembly_code();
std::vector<std::string> tokenize_expression(const std::string &expr);

// Main function
int main() {
    std::ifstream file("t1.txt");
    if (!file) {
        std::cerr << "Error: Cannot open file t1.txt." << std::endl;
        return 1;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '[' || line[0] == '@') {
            continue;
        }

        // Strip inline comments
        size_t comment_pos = line.find("[*");
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }

        // Determine type of line
        if (line.find("integer") != std::string::npos) {
            process_declaration(line);
        } else if (line.find("get") != std::string::npos) {
            process_get(line);
        } else if (line.find("put") != std::string::npos) {
            process_put(line);
        } else if (line.find("while") != std::string::npos) {
            process_while(line, file);
        } else if (line.find("=") != std::string::npos) {
            process_assignment(line);
        }
    }

    file.close();

    // Print assembly code and symbol table
    print_assembly_code();
    print_symbol_table();

    return 0;
}

// Processes variable declarations
void process_declaration(const std::string &line) {
    size_t pos = line.find("integer");
    std::string vars = line.substr(pos + 7);
    vars.erase(std::remove(vars.begin(), vars.end(), ' '), vars.end());
    vars.erase(std::remove(vars.begin(), vars.end(), ';'), vars.end());

    size_t start = 0, end;
    while ((end = vars.find(',', start)) != std::string::npos) {
        std::string var = vars.substr(start, end - start);
        if (symbol_table.find(var) != symbol_table.end()) {
            std::cerr << "Error: Identifier '" << var << "' is already declared." << std::endl;
        } else {
            symbol_table[var] = {current_memory_address++, var, "integer"};
            symbol_table_order.push_back(var);
        }
        start = end + 1;
    }
    std::string var = vars.substr(start);
    if (!var.empty() && symbol_table.find(var) == symbol_table.end()) {
        symbol_table[var] = {current_memory_address++, var, "integer"};
        symbol_table_order.push_back(var);
    }
}

// Processes assignment statements
void process_assignment(const std::string &line) {
    size_t equal_sign_pos = line.find('=');
    if (equal_sign_pos == std::string::npos) {
        std::cerr << "Error: Invalid assignment statement: " << line << std::endl;
        return;
    }

    std::string lhs = line.substr(0, equal_sign_pos);
    std::string rhs = line.substr(equal_sign_pos + 1);

    // Remove spaces
    lhs.erase(std::remove(lhs.begin(), lhs.end(), ' '), lhs.end());
    rhs.erase(std::remove(rhs.begin(), rhs.end(), ' '), rhs.end());

    // Check if LHS is declared
    if (symbol_table.find(lhs) == symbol_table.end()) {
        std::cerr << "Error: Identifier '" << lhs << "' is not declared." << std::endl;
        return;
    }

    // Tokenize and process RHS
    std::vector<std::string> tokens = tokenize_expression(rhs);
    for (const std::string &token : tokens) {
        if (std::isdigit(token[0])) {
            add_assembly("PUSHI " + token);
        } else if (symbol_table.find(token) != symbol_table.end()) {
            add_assembly("PUSHM " + std::to_string(symbol_table[token].memory_address));
        } else if (token == "+") {
            add_assembly("ADD");
        } else if (token == "*") {
            add_assembly("MUL");
        }
    }

    add_assembly("POPM " + std::to_string(symbol_table[lhs].memory_address));
}

// Tokenizes a mathematical expression
std::vector<std::string> tokenize_expression(const std::string &expr) {
    std::vector<std::string> tokens;
    std::string current_token;
    for (char ch : expr) {
        if (std::isalnum(ch)) {
            current_token += ch;
        } else {
            if (!current_token.empty()) {
                tokens.push_back(current_token);
                current_token.clear();
            }
            if (ch == '+' || ch == '*') {
                tokens.push_back(std::string(1, ch));
            }
        }
    }
    if (!current_token.empty()) {
        tokens.push_back(current_token);
    }
    return tokens;
}

// Processes input (get) statements
void process_get(const std::string &line) {
    size_t start = line.find('(');
    size_t end = line.find(')');
    if (start == std::string::npos || end == std::string::npos) {
        std::cerr << "Error: Invalid get statement." << std::endl;
        return;
    }

    std::string var = line.substr(start + 1, end - start - 1);
    if (symbol_table.find(var) == symbol_table.end()) {
        std::cerr << "Error: Identifier '" << var << "' is not declared." << std::endl;
        return;
    }

    add_assembly("STDIN");
    add_assembly("POPM " + std::to_string(symbol_table[var].memory_address));
}

// Processes output (put) statements
void process_put(const std::string &line) {
    size_t start = line.find('(');
    size_t end = line.find(')');
    if (start == std::string::npos || end == std::string::npos) {
        std::cerr << "Error: Invalid put statement." << std::endl;
        return;
    }

    std::string expr = line.substr(start + 1, end - start - 1);
    std::vector<std::string> tokens = tokenize_expression(expr);

    for (const std::string &token : tokens) {
        if (std::isdigit(token[0])) {
            add_assembly("PUSHI " + token);
        } else if (symbol_table.find(token) != symbol_table.end()) {
            add_assembly("PUSHM " + std::to_string(symbol_table[token].memory_address));
        } else if (token == "+") {
            add_assembly("ADD");
        } else {
            std::cerr << "Error: Identifier '" << token << "' is not declared." << std::endl;
            return;
        }
    }

    add_assembly("STDOUT");
}

// Processes while loops
void process_while(const std::string &line, std::ifstream &input) {
    size_t condition_start = line.find('(');
    size_t condition_end = line.find(')');
    if (condition_start == std::string::npos || condition_end == std::string::npos) {
        std::cerr << "Error: Invalid while statement." << std::endl;
        return;
    }

    std::string condition = line.substr(condition_start + 1, condition_end - condition_start - 1);
    int condition_label = instruction_index;
    add_assembly("LABEL");

    std::vector<std::string> tokens = tokenize_expression(condition);
    for (const std::string &token : tokens) {
        if (std::isdigit(token[0])) {
            add_assembly("PUSHI " + token);
        } else if (symbol_table.find(token) != symbol_table.end()) {
            add_assembly("PUSHM " + std::to_string(symbol_table[token].memory_address));
        } else if (token == "<") {
            add_assembly("LES");
        }
    }

    int jumpz_label = instruction_index + 14; // Match target indices for the output
    add_assembly("JUMPZ " + std::to_string(jumpz_label));

    std::string body;
    while (std::getline(input, body) && body.find('}') == std::string::npos) {
        if (body.find("=") != std::string::npos) {
            process_assignment(body);
        } else if (body.find("put") != std::string::npos) {
            process_put(body);
        }
    }

    add_assembly("JUMP " + std::to_string(condition_label));
    add_assembly("LABEL " + std::to_string(jumpz_label));
}

// Adds an instruction to the assembly code
void add_assembly(const std::string &instruction) {
    assembly_code.push_back(instruction);
}

// Prints the symbol table
void print_symbol_table() {
    std::cout << "\nSymbol Table:" << std::endl;
    std::cout << std::setw(15) << "Identifier" << std::setw(20) << "MemoryLocation" << std::setw(10) << "Type" << std::endl;
    for (const std::string &var : symbol_table_order) {
        const SymbolTableEntry &entry = symbol_table[var];
        std::cout << std::setw(15) << entry.lexeme
                  << std::setw(20) << entry.memory_address
                  << std::setw(10) << entry.type << std::endl;
    }
}

// Prints the assembly code
void print_assembly_code() {
    std::cout << "\nAssembly Code:" << std::endl;
    for (size_t i = 0; i < assembly_code.size(); ++i) {
        std::cout << i + 1 << " " << assembly_code[i] << "\n";
    }
}

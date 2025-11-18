#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/TargetParser/Host.h"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <queue>
#include <string.h>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using namespace llvm;
using namespace llvm::sys;

FILE *pFile;

//===----------------------------------------------------------------------===//
// Lexer
//===----------------------------------------------------------------------===//

// The lexer returns one of these for known things.
enum TOKEN_TYPE {

  IDENT = -1,        // [a-zA-Z_][a-zA-Z_0-9]*
  ASSIGN = int('='), // '='

  // delimiters
  LBRA = int('{'),  // left brace
  RBRA = int('}'),  // right brace
  LPAR = int('('),  // left parenthesis
  LBOX = int('['),  // left bracket
  RBOX = int(']'),  // right bracket
  RPAR = int(')'),  // right parenthesis
  SC = int(';'),    // semicolon
  COMMA = int(','), // comma

  // types
  INT_TOK = -2,   // "int"
  VOID_TOK = -3,  // "void"
  FLOAT_TOK = -4, // "float"
  BOOL_TOK = -5,  // "bool"

  // keywords
  EXTERN = -6,  // "extern"
  IF = -7,      // "if"
  ELSE = -8,    // "else"
  WHILE = -9,   // "while"
  RETURN = -10, // "return"
  // TRUE   = -12,     // "true"
  // FALSE   = -13,     // "false"

  // literals
  INT_LIT = -14,   // [0-9]+
  FLOAT_LIT = -15, // [0-9]+.[0-9]+
  BOOL_LIT = -16,  // "true" or "false" key words

  // logical operators
  AND = -17, // "&&"
  OR = -18,  // "||"

  // operators
  PLUS = int('+'),    // addition or unary plus
  MINUS = int('-'),   // substraction or unary negative
  ASTERIX = int('*'), // multiplication
  DIV = int('/'),     // division
  MOD = int('%'),     // modular
  NOT = int('!'),     // unary negation

  // comparison operators
  EQ = -19,      // equal
  NE = -20,      // not equal
  LE = -21,      // less than or equal to
  LT = int('<'), // less than
  GE = -23,      // greater than or equal to
  GT = int('>'), // greater than

  // special tokens
  EOF_TOK = 0, // signal end of file

  // invalid
  INVALID = -100 // signal invalid token
};

// TOKEN class is used to keep track of information about a token
class TOKEN {
public:
  TOKEN() = default;
  int type = -100;
  std::string lexeme;
  int lineNo;
  int columnNo;
  const std::string getIdentifierStr() const;
  const int getIntVal() const;
  const float getFloatVal() const;
  const bool getBoolVal() const;
};

static std::string globalLexeme;
static int lineNo, columnNo;

const std::string TOKEN::getIdentifierStr() const {
  if (type != IDENT) {
    fprintf(stderr, "%d:%d Error: %s\n", lineNo, columnNo,
            "getIdentifierStr called on non-IDENT token");
    exit(2);
  }
  return lexeme;
}

const int TOKEN::getIntVal() const {
  if (type != INT_LIT) {
    fprintf(stderr, "%d:%d Error: %s\n", lineNo, columnNo,
            "getIntVal called on non-INT_LIT token");
    exit(2);
  }
  return strtod(lexeme.c_str(), nullptr);
}

const float TOKEN::getFloatVal() const {
  if (type != FLOAT_LIT) {
    fprintf(stderr, "%d:%d Error: %s\n", lineNo, columnNo,
            "getFloatVal called on non-FLOAT_LIT token");
    exit(2);
  }
  return strtof(lexeme.c_str(), nullptr);
}

const bool TOKEN::getBoolVal() const {
  if (type != BOOL_LIT) {
    fprintf(stderr, "%d:%d Error: %s\n", lineNo, columnNo,
            "getBoolVal called on non-BOOL_LIT token");
    exit(2);
  }
  return (lexeme == "true");
}

static TOKEN returnTok(std::string lexVal, int tok_type) {
  TOKEN return_tok;
  return_tok.lexeme = lexVal;
  return_tok.type = tok_type;
  return_tok.lineNo = lineNo;
  return_tok.columnNo = columnNo - lexVal.length() - 1;
  return return_tok;
}

// Read file line by line -- or look for \n and if found add 1 to line number
// and reset column number to 0
/// gettok - Return the next token from standard input.
static TOKEN gettok() {

  static int LastChar = ' ';
  static int NextChar = ' ';

  // Skip any whitespace.
  while (isspace(LastChar)) {
    if (LastChar == '\n' || LastChar == '\r') {
      lineNo++;
      columnNo = 1;
    }
    LastChar = getc(pFile);
    columnNo++;
  }

  if (isalpha(LastChar) ||
      (LastChar == '_')) { // identifier: [a-zA-Z_][a-zA-Z_0-9]*
    globalLexeme = LastChar;
    columnNo++;

    while (isalnum((LastChar = getc(pFile))) || (LastChar == '_')) {
      globalLexeme += LastChar;
      columnNo++;
    }

    if (globalLexeme == "int")
      return returnTok("int", INT_TOK);
    if (globalLexeme == "bool")
      return returnTok("bool", BOOL_TOK);
    if (globalLexeme == "float")
      return returnTok("float", FLOAT_TOK);
    if (globalLexeme == "void")
      return returnTok("void", VOID_TOK);
    if (globalLexeme == "bool")
      return returnTok("bool", BOOL_TOK);
    if (globalLexeme == "extern")
      return returnTok("extern", EXTERN);
    if (globalLexeme == "if")
      return returnTok("if", IF);
    if (globalLexeme == "else")
      return returnTok("else", ELSE);
    if (globalLexeme == "while")
      return returnTok("while", WHILE);
    if (globalLexeme == "return")
      return returnTok("return", RETURN);
    if (globalLexeme == "true") {
      //   BoolVal = true;
      return returnTok("true", BOOL_LIT);
    }
    if (globalLexeme == "false") {
      //   BoolVal = false;
      return returnTok("false", BOOL_LIT);
    }
    return returnTok(globalLexeme.c_str(), IDENT);
  }

  if (LastChar == '=') {
    NextChar = getc(pFile);
    if (NextChar == '=') { // EQ: ==
      LastChar = getc(pFile);
      columnNo += 2;
      return returnTok("==", EQ);
    } else {
      LastChar = NextChar;
      columnNo++;
      return returnTok("=", ASSIGN);
    }
  }

  if (LastChar == '{') {
    LastChar = getc(pFile);
    columnNo++;
    return returnTok("{", LBRA);
  }
  if (LastChar == '}') {
    LastChar = getc(pFile);
    columnNo++;
    return returnTok("}", RBRA);
  }
  if (LastChar == '(') {
    LastChar = getc(pFile);
    columnNo++;
    return returnTok("(", LPAR);
  }
  if (LastChar == ')') {
    LastChar = getc(pFile);
    columnNo++;
    return returnTok(")", RPAR);
  }
  if (LastChar == ';') {
    LastChar = getc(pFile);
    columnNo++;
    return returnTok(";", SC);
  }
  if (LastChar == ',') {
    LastChar = getc(pFile);
    columnNo++;
    return returnTok(",", COMMA);
  }

  if (isdigit(LastChar) || LastChar == '.') { // Number: [0-9]+.
    std::string NumStr;

    if (LastChar == '.') { // Floatingpoint Number: .[0-9]+
      do {
        NumStr += LastChar;
        LastChar = getc(pFile);
        columnNo++;
      } while (isdigit(LastChar));

      //   FloatVal = strtof(NumStr.c_str(), nullptr);
      return returnTok(NumStr, FLOAT_LIT);
    } else {
      do { // Start of Number: [0-9]+
        NumStr += LastChar;
        LastChar = getc(pFile);
        columnNo++;
      } while (isdigit(LastChar));

      if (LastChar == '.') { // Floatingpoint Number: [0-9]+.[0-9]+)
        do {
          NumStr += LastChar;
          LastChar = getc(pFile);
          columnNo++;
        } while (isdigit(LastChar));

        // FloatVal = strtof(NumStr.c_str(), nullptr);
        return returnTok(NumStr, FLOAT_LIT);
      } else { // Integer : [0-9]+
        // IntVal = strtod(NumStr.c_str(), nullptr);
        return returnTok(NumStr, INT_LIT);
      }
    }
  }

  if (LastChar == '&') {
    NextChar = getc(pFile);
    if (NextChar == '&') { // AND: &&
      LastChar = getc(pFile);
      columnNo += 2;
      return returnTok("&&", AND);
    } else {
      LastChar = NextChar;
      columnNo++;
      return returnTok("&", int('&'));
    }
  }

  if (LastChar == '|') {
    NextChar = getc(pFile);
    if (NextChar == '|') { // OR: ||
      LastChar = getc(pFile);
      columnNo += 2;
      return returnTok("||", OR);
    } else {
      LastChar = NextChar;
      columnNo++;
      return returnTok("|", int('|'));
    }
  }

  if (LastChar == '!') {
    NextChar = getc(pFile);
    if (NextChar == '=') { // NE: !=
      LastChar = getc(pFile);
      columnNo += 2;
      return returnTok("!=", NE);
    } else {
      LastChar = NextChar;
      columnNo++;
      return returnTok("!", NOT);
      ;
    }
  }

  if (LastChar == '<') {
    NextChar = getc(pFile);
    if (NextChar == '=') { // LE: <=
      LastChar = getc(pFile);
      columnNo += 2;
      return returnTok("<=", LE);
    } else {
      LastChar = NextChar;
      columnNo++;
      return returnTok("<", LT);
    }
  }

  if (LastChar == '>') {
    NextChar = getc(pFile);
    if (NextChar == '=') { // GE: >=
      LastChar = getc(pFile);
      columnNo += 2;
      return returnTok(">=", GE);
    } else {
      LastChar = NextChar;
      columnNo++;
      return returnTok(">", GT);
    }
  }

  if (LastChar == '/') { // could be division or could be the start of a comment
    LastChar = getc(pFile);
    columnNo++;
    if (LastChar == '/') { // definitely a comment
      do {
        LastChar = getc(pFile);
        columnNo++;
      } while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

      if (LastChar != EOF)
        return gettok();
    } else
      return returnTok("/", DIV);
  }

  // Check for end of file.  Don't eat the EOF.
  if (LastChar == EOF) {
    columnNo++;
    return returnTok("0", EOF_TOK);
  }

  // Otherwise, just return the character as its ascii value.
  int ThisChar = LastChar;
  std::string s(1, ThisChar);
  LastChar = getc(pFile);
  columnNo++;
  return returnTok(s, int(ThisChar));
}

//===----------------------------------------------------------------------===//
// Parser
//===----------------------------------------------------------------------===//

/// CurTok/getNextToken - Provide a simple token buffer.  CurTok is the current
/// token the parser is looking at.  getNextToken reads another token from the
/// lexer and updates CurTok with its results.
static TOKEN CurTok;
static std::deque<TOKEN> tok_buffer;

static TOKEN getNextToken() {

  if (tok_buffer.size() == 0)
    tok_buffer.push_back(gettok());

  TOKEN temp = tok_buffer.front();
  tok_buffer.pop_front();

  return CurTok = temp;
}

static void putBackToken(TOKEN tok) { tok_buffer.push_front(tok); }

// Helper functions for FIRST sets (Task 1.2.3) 
static bool isType(int tokType) {
  return (tokType == INT_TOK || tokType == FLOAT_TOK || tokType == BOOL_TOK || tokType == VOID_TOK);
}

static bool isExprStart(int tokType) {
  return (tokType == IDENT || tokType == INT_LIT || tokType == FLOAT_LIT || 
          tokType == BOOL_LIT || tokType == NOT || tokType == MINUS || 
          tokType == LPAR);
}

/// ASTnode (Task 1.2.2)- Base class for all AST nodes.
class ASTnode {

public:
  virtual ~ASTnode() {}
  virtual Value *codegen() { return nullptr; };
  virtual std::string to_string() const { return ""; };
};

/// IntASTnode - Class for integer literals like 1, 2, 10,
class IntASTnode : public ASTnode {
  int Val;
  TOKEN Tok;

public:
  IntASTnode(TOKEN tok, int val) : Val(val), Tok(tok) {}
  virtual std::string to_string() const override { return std::to_string(Val); }
};

/// BoolASTnode - Class for boolean literals true and false,
class BoolASTnode : public ASTnode {
  bool Bool;
  TOKEN Tok;

public:
  BoolASTnode(TOKEN tok, bool B) : Bool(B), Tok(tok) {}
  virtual std::string to_string() const override { return Bool ? "true" : "false"; }
};

/// FloatASTnode - Node class for floating point literals like "1.0".
class FloatASTnode : public ASTnode {
  double Val;
  TOKEN Tok;

public:
  FloatASTnode(TOKEN tok, double Val) : Val(Val), Tok(tok) {}
  virtual std::string to_string() const override { return std::to_string(Val); }
};

/// VariableASTnode - Class for referencing a variable (i.e. identifier), like
/// "a".
enum IDENT_TYPE { IDENTIFIER = 0 };
class VariableASTnode : public ASTnode {
protected:
  TOKEN Tok;
  std::string Name;
  IDENT_TYPE VarType;

public:
  VariableASTnode(TOKEN tok, const std::string &Name)
      : Tok(tok), Name(Name), VarType(IDENT_TYPE::IDENTIFIER) {}
  const std::string &getName() const { return Name; }
  virtual std::string to_string() const override { return Name; }
};

// Binary Expression Node (Task 1.2.2)
class BinaryExprAST : public ASTnode {
  int Op;
  std::unique_ptr<ASTnode> LHS, RHS;
public:
  BinaryExprAST(int op, std::unique_ptr<ASTnode> lhs, std::unique_ptr<ASTnode> rhs)
    : Op(op), LHS(std::move(lhs)), RHS(std::move(rhs)) {}
  virtual std::string to_string() const override {
    return "(" + LHS->to_string() + " op " + RHS->to_string() + ")";
  }
};

// Unary Expression Node (Task 1.2.2)
class UnaryExprAST : public ASTnode {
  int Op;
  std::unique_ptr<ASTnode> Operand;
public:
  UnaryExprAST(int op, std::unique_ptr<ASTnode> operand)
    : Op(op), Operand(std::move(operand)) {}
  virtual std::string to_string() const override {
    return "(Unary op " + Operand->to_string() + ")";
  }
};

// Function Call Node (Task 1.2.2)
class CallExprAST : public ASTnode {
  std::string Callee;
  std::vector<std::unique_ptr<ASTnode>> Args;
public:
  CallExprAST(const std::string &callee, std::vector<std::unique_ptr<ASTnode>> args)
    : Callee(callee), Args(std::move(args)) {}
  virtual std::string to_string() const override {
    return "call " + Callee;
  }
};

/// ParamAST - Class for a parameter declaration
class ParamAST {
  std::string Name;
  std::string Type;

public:
  ParamAST(const std::string &name, const std::string &type)
      : Name(name), Type(type) {}
  const std::string &getName() const { return Name; }
  const std::string &getType() const { return Type; }
};

/// DeclAST - Base class for declarations, variables and functions
class DeclAST : public ASTnode {

public:
  virtual ~DeclAST() {}
};

/// VarDeclAST - Class for a variable declaration
class VarDeclAST : public DeclAST {
  std::unique_ptr<VariableASTnode> Var;
  std::string Type;

public:
  VarDeclAST(std::unique_ptr<VariableASTnode> var, const std::string &type)
      : Var(std::move(var)), Type(type) {}
  const std::string &getName() const { return Var->getName(); }
};

/// GlobVarDeclAST - Class for a Global variable declaration
class GlobVarDeclAST : public DeclAST {
  std::unique_ptr<VariableASTnode> Var;
  std::string Type;

public:
  GlobVarDeclAST(std::unique_ptr<VariableASTnode> var, const std::string &type)
      : Var(std::move(var)), Type(type) {}
};

/// FunctionPrototypeAST - Class for a function declaration's signature
class FunctionPrototypeAST {
  std::string Name;
  std::string Type;
  std::vector<std::unique_ptr<ParamAST>> Params; // vector of parameters

public:
  FunctionPrototypeAST(const std::string &name, const std::string &type,
                       std::vector<std::unique_ptr<ParamAST>> params)
      : Name(name), Type(type), Params(std::move(params)) {}
   
  const std::string &getName() const { return Name; }
  std::vector<std::unique_ptr<ParamAST>> &getParams() { return Params; }
};

/// BlockAST - Class for a block with declarations followed by statements
class BlockAST : public ASTnode {
  std::vector<std::unique_ptr<VarDeclAST>> LocalDecls; // vector of local decls
  std::vector<std::unique_ptr<ASTnode>> Stmts;         // vector of statements

public:
  BlockAST(std::vector<std::unique_ptr<VarDeclAST>> localDecls,
           std::vector<std::unique_ptr<ASTnode>> stmts)
      : LocalDecls(std::move(localDecls)), Stmts(std::move(stmts)) {}
  virtual std::string to_string() const override { return "Block"; }
};

/// FunctionDeclAST - This class represents a function definition itself.
class FunctionDeclAST : public DeclAST {
  std::unique_ptr<FunctionPrototypeAST> Proto;
  std::unique_ptr<ASTnode> Block;

public:
  FunctionDeclAST(std::unique_ptr<FunctionPrototypeAST> Proto,
                  std::unique_ptr<ASTnode> Block)
      : Proto(std::move(Proto)), Block(std::move(Block)) {}
};

/// IfExprAST - Expression class for if/then/else.
class IfExprAST : public ASTnode {
  std::unique_ptr<ASTnode> Cond, Then, Else;
public:
  IfExprAST(std::unique_ptr<ASTnode> Cond, std::unique_ptr<ASTnode> Then,
            std::unique_ptr<ASTnode> Else)
      : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}
  virtual std::string to_string() const override { return "If Statement"; }
};

/// WhileExprAST - Expression class for while.
class WhileExprAST : public ASTnode {
  std::unique_ptr<ASTnode> Cond, Body;
public:
  WhileExprAST(std::unique_ptr<ASTnode> cond, std::unique_ptr<ASTnode> body)
      : Cond(std::move(cond)), Body(std::move(body)) {}
  virtual std::string to_string() const override { return "While Statement"; }
};

/// ReturnAST - Class for a return value
class ReturnAST : public ASTnode {
  std::unique_ptr<ASTnode> Val;
public:
  ReturnAST(std::unique_ptr<ASTnode> value) : Val(std::move(value)) {}
  virtual std::string to_string() const override { return "Return Statement"; }
};

// Updated Error Handling
std::unique_ptr<ASTnode> LogError(TOKEN tok, const char *Str) {
  fprintf(stderr, "%d:%d Error: %s\n", tok.lineNo, tok.columnNo, Str);
  exit(2);
  return nullptr;
}
std::unique_ptr<FunctionPrototypeAST> LogErrorP(TOKEN tok, const char *Str) {
  LogError(tok, Str);
  exit(2);
  return nullptr;
}
std::unique_ptr<ASTnode> LogError(const char *Str) {
  fprintf(stderr, "Error: %s\n", Str);
  exit(2);
  return nullptr;
}

//===----------------------------------------------------------------------===//
// Recursive Descent - Function call for each production
//===----------------------------------------------------------------------===//

// Forward Declarations
static std::unique_ptr<ASTnode> ParseDecl();
static std::unique_ptr<ASTnode> ParseStmt();
static std::unique_ptr<ASTnode> ParseBlock();
static std::unique_ptr<ASTnode> ParseExper(); // Forward declare top level expr
static std::unique_ptr<ParamAST> ParseParam();
static std::unique_ptr<VarDeclAST> ParseLocalDecl();
static std::vector<std::unique_ptr<VarDeclAST>> ParseLocalDecls(); // Added Forward Declaration
static std::vector<std::unique_ptr<ASTnode>> ParseStmtListPrime();

// Precedence Parsers (Task 1.2.3)
static std::unique_ptr<ASTnode> ParseAssignment();
static std::unique_ptr<ASTnode> ParseLogicalOr();
static std::unique_ptr<ASTnode> ParseLogicalAnd();
static std::unique_ptr<ASTnode> ParseEquality();
static std::unique_ptr<ASTnode> ParseRelational();
static std::unique_ptr<ASTnode> ParseAdditive();
static std::unique_ptr<ASTnode> ParseMultiplicative();
static std::unique_ptr<ASTnode> ParseUnary();
static std::unique_ptr<ASTnode> ParsePrimary();

// 9. Primary (Highest Precedence)
static std::unique_ptr<ASTnode> ParsePrimary() {
  if (CurTok.type == INT_LIT) {
    auto Res = std::make_unique<IntASTnode>(CurTok, CurTok.getIntVal());
    getNextToken(); return std::move(Res);
  } 
  else if (CurTok.type == FLOAT_LIT) {
    auto Res = std::make_unique<FloatASTnode>(CurTok, CurTok.getFloatVal());
    getNextToken(); return std::move(Res);
  } 
  else if (CurTok.type == BOOL_LIT) {
    auto Res = std::make_unique<BoolASTnode>(CurTok, CurTok.getBoolVal());
    getNextToken(); return std::move(Res);
  } 
  else if (CurTok.type == LPAR) {
    getNextToken(); // eat (
    auto V = ParseExper();
    if (!V) return nullptr;
    if (CurTok.type != RPAR) return LogError(CurTok, "Expected ')'");
    getNextToken(); // eat )
    return V;
  } 
  else if (CurTok.type == IDENT) {
    std::string IdName = CurTok.getIdentifierStr();
    TOKEN IdTok = CurTok;
    getNextToken(); // eat IDENT
    
    if (CurTok.type == LPAR) { // Function Call
      getNextToken(); // eat (
      std::vector<std::unique_ptr<ASTnode>> Args;
      if (CurTok.type != RPAR) {
        while (true) {
          if (auto Arg = ParseExper()) Args.push_back(std::move(Arg));
          else return nullptr;
          
          if (CurTok.type == RPAR) break;
          if (CurTok.type != COMMA) return LogError(CurTok, "Expected ')' or ',' in argument list");
          getNextToken(); // eat ,
        }
      }
      getNextToken(); // eat )
      return std::make_unique<CallExprAST>(IdName, std::move(Args));
    } 
    else { // Variable access
      return std::make_unique<VariableASTnode>(IdTok, IdName);
    }
  } 
  else {
    return LogError(CurTok, "Unknown token when expecting an expression");
  }
}

// 8. Unary (!, -)
static std::unique_ptr<ASTnode> ParseUnary() {
  if (CurTok.type == NOT || CurTok.type == MINUS) {
    int Opc = CurTok.type;
    getNextToken();
    if (auto Operand = ParseUnary()) { // Recursive for !!x
      return std::make_unique<UnaryExprAST>(Opc, std::move(Operand));
    }
    return nullptr;
  }
  return ParsePrimary();
}

// 7. Multiplicative (*, /, %)
static std::unique_ptr<ASTnode> ParseMultiplicative() {
  auto LHS = ParseUnary();
  if (!LHS) return nullptr;
   
  while (CurTok.type == ASTERIX || CurTok.type == DIV || CurTok.type == MOD) {
    int Opc = CurTok.type;
    getNextToken();
    auto RHS = ParseUnary();
    if (!RHS) return nullptr;
    LHS = std::make_unique<BinaryExprAST>(Opc, std::move(LHS), std::move(RHS));
  }
  return LHS;
}

// 6. Additive (+, -)
static std::unique_ptr<ASTnode> ParseAdditive() {
  auto LHS = ParseMultiplicative();
  if (!LHS) return nullptr;

  while (CurTok.type == PLUS || CurTok.type == MINUS) {
    int Opc = CurTok.type;
    getNextToken();
    auto RHS = ParseMultiplicative(); // Precedence: ParseMultiplicative
    if (!RHS) return nullptr;
    LHS = std::make_unique<BinaryExprAST>(Opc, std::move(LHS), std::move(RHS));
  }
  return LHS;
}

// 5. Relational (<, <=, >, >=)
static std::unique_ptr<ASTnode> ParseRelational() {
  auto LHS = ParseAdditive();
  if (!LHS) return nullptr;

  while (CurTok.type == LT || CurTok.type == LE || CurTok.type == GT || CurTok.type == GE) {
    int Opc = CurTok.type;
    getNextToken();
    auto RHS = ParseAdditive();
    if (!RHS) return nullptr;
    LHS = std::make_unique<BinaryExprAST>(Opc, std::move(LHS), std::move(RHS));
  }
  return LHS;
}

// 4. Equality (==, !=)
static std::unique_ptr<ASTnode> ParseEquality() {
  auto LHS = ParseRelational();
  if (!LHS) return nullptr;

  while (CurTok.type == EQ || CurTok.type == NE) {
    int Opc = CurTok.type;
    getNextToken();
    auto RHS = ParseRelational();
    if (!RHS) return nullptr;
    LHS = std::make_unique<BinaryExprAST>(Opc, std::move(LHS), std::move(RHS));
  }
  return LHS;
}

// 3. Logical AND (&&)
static std::unique_ptr<ASTnode> ParseLogicalAnd() {
  auto LHS = ParseEquality();
  if (!LHS) return nullptr;

  while (CurTok.type == AND) {
    int Opc = CurTok.type;
    getNextToken();
    auto RHS = ParseEquality();
    if (!RHS) return nullptr;
    LHS = std::make_unique<BinaryExprAST>(Opc, std::move(LHS), std::move(RHS));
  }
  return LHS;
}

// 2. Logical OR (||)
static std::unique_ptr<ASTnode> ParseLogicalOr() {
  auto LHS = ParseLogicalAnd();
  if (!LHS) return nullptr;

  while (CurTok.type == OR) {
    int Opc = CurTok.type;
    getNextToken();
    auto RHS = ParseLogicalAnd();
    if (!RHS) return nullptr;
    LHS = std::make_unique<BinaryExprAST>(Opc, std::move(LHS), std::move(RHS));
  }
  return LHS;
}

// 1. Assignment (=)
static std::unique_ptr<ASTnode> ParseAssignment() {
  // Assignment is Right Associative and tricky in LL(1).
  // Grammar: IDENT "=" expr | logical_or_expr
  // Check for IDENT first
  if (CurTok.type == IDENT) {
    // We need to look ahead. But we only have 1 token buffer.
    // We can parse as LogicalOr. If it returns a VariableASTnode and current token is '=', we pivot.
    // Alternatively, we can use the deque buffer to peek.
     
    TOKEN TempId = CurTok; // Save Ident
    getNextToken(); // Eat Ident
     
    if (CurTok.type == ASSIGN) { // It is an assignment!
       getNextToken(); // Eat '='
       auto RHS = ParseExper(); // Recursive for a = b = c
       if (!RHS) return nullptr;
       // Create assignment node. NOTE: Using BinaryExprAST for assignment for now
       return std::make_unique<BinaryExprAST>(ASSIGN, 
            std::make_unique<VariableASTnode>(TempId, TempId.getIdentifierStr()), 
            std::move(RHS));
    } 
    else {
        // It wasn't an assignment. We already ate the IDENT.
        // Put it back!
        putBackToken(CurTok); // Put back whatever followed the ID
        CurTok = TempId;      // Reset current to ID
        // Fall through to standard expression parsing
    }
  }
  return ParseLogicalOr();
}

// Top Level Expression Entry Point
static std::unique_ptr<ASTnode> ParseExper() {
  return ParseAssignment();
}

// --- Statement Parsing ---

static std::unique_ptr<ASTnode> ParseExperStmt() {
  if (CurTok.type == SC) {
    getNextToken();
    return nullptr; // Empty statement
  }
  auto expr = ParseExper();
  if (CurTok.type == SC) {
    getNextToken();
    return expr;
  }
  return LogError(CurTok, "Expected ';' after expression");
}

static std::unique_ptr<ASTnode> ParseBlock() {
  if (CurTok.type != LBRA) return LogError(CurTok, "Expected '{'");
  getNextToken(); // eat {

  std::vector<std::unique_ptr<VarDeclAST>> local_decls = ParseLocalDecls();
  std::vector<std::unique_ptr<ASTnode>> stmt_list = ParseStmtListPrime();

  if (CurTok.type != RBRA) return LogError(CurTok, "Expected '}'");
  getNextToken(); // eat }
   
  return std::make_unique<BlockAST>(std::move(local_decls), std::move(stmt_list));
}

static std::unique_ptr<ASTnode> ParseIfStmt() {
  getNextToken(); // eat if
  if (CurTok.type != LPAR) return LogError(CurTok, "Expected '(' after if");
  getNextToken(); // eat (
  auto Cond = ParseExper();
  if (!Cond) return nullptr;
  if (CurTok.type != RPAR) return LogError(CurTok, "Expected ')' after condition");
  getNextToken(); // eat )
   
  if (CurTok.type != LBRA) return LogError(CurTok, "Expected '{' in if block");
  auto Then = ParseBlock();
  if (!Then) return nullptr;
   
  std::unique_ptr<ASTnode> Else = nullptr;
  if (CurTok.type == ELSE) {
    getNextToken(); // eat else
    if (CurTok.type != LBRA) return LogError(CurTok, "Expected '{' in else block");
    Else = ParseBlock();
    if (!Else) return nullptr;
  }
  return std::make_unique<IfExprAST>(std::move(Cond), std::move(Then), std::move(Else));
}

static std::unique_ptr<ASTnode> ParseWhileStmt() {
  getNextToken(); // eat while
  if (CurTok.type != LPAR) return LogError(CurTok, "Expected '(' after while");
  getNextToken(); // eat (
  auto Cond = ParseExper();
  if (!Cond) return nullptr;
  if (CurTok.type != RPAR) return LogError(CurTok, "Expected ')' after condition");
  getNextToken(); // eat )
   
  if (CurTok.type != LBRA) return LogError(CurTok, "Expected '{' in while block");
  auto Body = ParseStmt(); // Note: Grammar says stmt, usually it's a block
  if (!Body) return nullptr;
   
  return std::make_unique<WhileExprAST>(std::move(Cond), std::move(Body));
}

static std::unique_ptr<ASTnode> ParseReturnStmt() {
  getNextToken(); // eat return
  if (CurTok.type == SC) {
    getNextToken();
    return std::make_unique<ReturnAST>(nullptr);
  }
  if (isExprStart(CurTok.type)) {
    auto val = ParseExper();
    if (CurTok.type == SC) {
      getNextToken();
      return std::make_unique<ReturnAST>(std::move(val));
    }
    return LogError(CurTok, "Expected ';' after return value");
  }
  return LogError(CurTok, "Expected ';' or expression after return");
}

static std::unique_ptr<ASTnode> ParseStmt() {
  if (isExprStart(CurTok.type) || CurTok.type == SC) {
    return ParseExperStmt();
  } else if (CurTok.type == LBRA) {
    return ParseBlock();
  } else if (CurTok.type == IF) {
    return ParseIfStmt();
  } else if (CurTok.type == WHILE) {
    return ParseWhileStmt();
  } else if (CurTok.type == RETURN) {
    return ParseReturnStmt();
  }
  return LogError(CurTok, "Unexpected token in statement");
}

static std::vector<std::unique_ptr<ASTnode>> ParseStmtListPrime() {
  std::vector<std::unique_ptr<ASTnode>> list;
  // FIRST(stmt) check
  while (isExprStart(CurTok.type) || CurTok.type == SC || CurTok.type == LBRA || 
         CurTok.type == IF || CurTok.type == WHILE || CurTok.type == RETURN) {
    auto stmt = ParseStmt();
    if (stmt) list.push_back(std::move(stmt));
    else break;
  }
  return list;
}

// --- Parameter Parsing ---

static std::unique_ptr<ParamAST> ParseParam() {
  std::string Type = CurTok.lexeme;
  getNextToken(); // eat type
  if (CurTok.type == IDENT) {
    std::string Name = CurTok.getIdentifierStr();
    getNextToken();
    return std::make_unique<ParamAST>(Name, Type);
  }
  return nullptr;
}

static std::vector<std::unique_ptr<ParamAST>> ParseParamListPrime() {
  std::vector<std::unique_ptr<ParamAST>> list;
  while (CurTok.type == COMMA) {
    getNextToken(); // eat ,
    if (isType(CurTok.type)) {
        auto param = ParseParam();
        if (param) list.push_back(std::move(param));
    }
  }
  return list;
}

static std::vector<std::unique_ptr<ParamAST>> ParseParams() {
  std::vector<std::unique_ptr<ParamAST>> list;
  if (isType(CurTok.type)) {
    auto param = ParseParam();
    if (param) {
      list.push_back(std::move(param));
      auto others = ParseParamListPrime();
      for(auto &p : others) list.push_back(std::move(p));
    }
  } else if (CurTok.type == VOID_TOK) {
    getNextToken();
    // Check if next is ) inside the caller
  }
  return list;
}

// --- Declaration Parsing ---

static std::unique_ptr<VarDeclAST> ParseLocalDecl() {
  std::string Type = CurTok.lexeme;
  getNextToken(); // eat type
  if (CurTok.type != IDENT) return nullptr; // Error handled in caller logic often or here
   
  std::string Name = CurTok.getIdentifierStr();
  TOKEN Tok = CurTok;
  getNextToken(); // eat ID
   
  if (CurTok.type != SC) { LogError(CurTok, "Expected ';'"); return nullptr; }
  getNextToken(); // eat ;
   
  auto varNode = std::make_unique<VariableASTnode>(Tok, Name);
  return std::make_unique<VarDeclAST>(std::move(varNode), Type);
}

static std::vector<std::unique_ptr<VarDeclAST>> ParseLocalDecls() {
  std::vector<std::unique_ptr<VarDeclAST>> list;
  while (isType(CurTok.type)) {
    auto decl = ParseLocalDecl();
    if (decl) list.push_back(std::move(decl));
  }
  return list;
}

static std::unique_ptr<ASTnode> ParseDecl() {
  TOKEN PrevTok = CurTok;
  getNextToken(); // eat type
  std::string IdName = CurTok.getIdentifierStr();
  TOKEN IdTok = CurTok;
   
  if (CurTok.type == IDENT) {
    getNextToken(); // eat ID
     
    if (CurTok.type == SC) { // Global Var
      getNextToken(); // eat ;
      auto ident = std::make_unique<VariableASTnode>(IdTok, IdName);
      return std::make_unique<GlobVarDeclAST>(std::move(ident), PrevTok.lexeme);
    } 
    else if (CurTok.type == LPAR) { // Function
      getNextToken(); // eat (
      auto params = ParseParams();
      if (CurTok.type != RPAR) return LogError(CurTok, "Expected ')'");
      getNextToken(); // eat )
      
      if (CurTok.type != LBRA) return LogError(CurTok, "Expected '{' for function body");
      auto block = ParseBlock();
      if (!block) return nullptr;
      
      auto proto = std::make_unique<FunctionPrototypeAST>(IdName, PrevTok.lexeme, std::move(params));
      return std::make_unique<FunctionDeclAST>(std::move(proto), std::move(block));
    }
  }
  return LogError(PrevTok, "Expected Identifier");
}

static void ParseDeclList() {
  while (isType(CurTok.type)) {
    auto decl = ParseDecl();
    if (!decl) break;
  }
}

static std::unique_ptr<FunctionPrototypeAST> ParseExtern() {
  getNextToken(); // eat extern
  if (!isType(CurTok.type)) return LogErrorP(CurTok, "Expected type after extern");
   
  std::string Type = CurTok.lexeme;
  getNextToken(); // eat type
   
  if (CurTok.type != IDENT) return LogErrorP(CurTok, "Expected identifier");
  std::string Name = CurTok.getIdentifierStr();
  getNextToken(); // eat ID
   
  if (CurTok.type != LPAR) return LogErrorP(CurTok, "Expected '('");
  getNextToken(); // eat (
   
  auto params = ParseParams();
   
  if (CurTok.type != RPAR) return LogErrorP(CurTok, "Expected ')'");
  getNextToken(); // eat )
   
  if (CurTok.type != SC) return LogErrorP(CurTok, "Expected ';'");
  getNextToken(); // eat ;
   
  return std::make_unique<FunctionPrototypeAST>(Name, Type, std::move(params));
}

static void ParseExternList() {
  while (CurTok.type == EXTERN) {
    ParseExtern();
  }
}

static void parser() {
  getNextToken(); // Prime the pump
  ParseExternList();
  ParseDeclList();
}

//===----------------------------------------------------------------------===//
// Code Generation
//===----------------------------------------------------------------------===//

static LLVMContext TheContext;
static IRBuilder<> Builder(TheContext);
static std::unique_ptr<Module> TheModule;

//===----------------------------------------------------------------------===//
// AST Printer
//===----------------------------------------------------------------------===//

// void IntASTnode::display(int tabs) {
//   printf("%s\n",getType().c_str());
// }

//===----------------------------------------------------------------------===//
// Main driver code.
//===----------------------------------------------------------------------===//

int main(int argc, char **argv) {
  if (argc == 2) {
    pFile = fopen(argv[1], "r");
    if (pFile == NULL) { perror("Error opening file"); return 1; }
    if (pFile == NULL)
      perror("Error opening file");
  } else {
    std::cout << "Usage: ./code InputFile\n";
    return 1;
  }

  // initialize line number and column numbers to zero
  lineNo = 1;
  columnNo = 1;

  // get the first token
  getNextToken();
  while (CurTok.type != EOF_TOK) {
    fprintf(stderr, "Token: %s with type %d\n", CurTok.lexeme.c_str(),
            CurTok.type);
    getNextToken();
  }
  fprintf(stderr, "Lexer Finished\n");

  // Make the module, which holds all the code.
  TheModule = std::make_unique<Module>("mini-c", TheContext);

  // Run the parser
  parser();
   
  fprintf(stderr, "Parsing Finished\n");

  auto Filename = "output.ll";
  std::error_code EC;
  raw_fd_ostream dest(Filename, EC, sys::fs::OF_None);

  if (EC) {
    errs() << "Could not open file: " << EC.message();
    return 1;
  }
  TheModule->print(dest, nullptr);
  fclose(pFile);
  return 0;
}
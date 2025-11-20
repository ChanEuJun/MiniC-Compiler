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

// Read file line by line
static TOKEN gettok() {
  static int LastChar = ' ';

  // Skip any whitespace.
  while (isspace(LastChar)) {
    if (LastChar == '\n' || LastChar == '\r') {
      lineNo++;
      columnNo = 1;
    }
    LastChar = getc(pFile);
    columnNo++;
  }

  if (isalpha(LastChar) || (LastChar == '_')) { 
    globalLexeme = LastChar;
    columnNo++;
    while (isalnum((LastChar = getc(pFile))) || (LastChar == '_')) {
      globalLexeme += LastChar;
      columnNo++;
    }

    if (globalLexeme == "int") return returnTok("int", INT_TOK);
    if (globalLexeme == "bool") return returnTok("bool", BOOL_TOK);
    if (globalLexeme == "float") return returnTok("float", FLOAT_TOK);
    if (globalLexeme == "void") return returnTok("void", VOID_TOK);
    if (globalLexeme == "extern") return returnTok("extern", EXTERN);
    if (globalLexeme == "if") return returnTok("if", IF);
    if (globalLexeme == "else") return returnTok("else", ELSE);
    if (globalLexeme == "while") return returnTok("while", WHILE);
    if (globalLexeme == "return") return returnTok("return", RETURN);
    if (globalLexeme == "true") return returnTok("true", BOOL_LIT);
    if (globalLexeme == "false") return returnTok("false", BOOL_LIT);
    return returnTok(globalLexeme.c_str(), IDENT);
  }

  if (LastChar == '=') {
    int NextChar = getc(pFile);
    if (NextChar == '=') { LastChar = getc(pFile); columnNo += 2; return returnTok("==", EQ); }
    else { LastChar = NextChar; columnNo++; return returnTok("=", ASSIGN); }
  }

  if (LastChar == '{') { LastChar = getc(pFile); columnNo++; return returnTok("{", LBRA); }
  if (LastChar == '}') { LastChar = getc(pFile); columnNo++; return returnTok("}", RBRA); }
  if (LastChar == '(') { LastChar = getc(pFile); columnNo++; return returnTok("(", LPAR); }
  if (LastChar == ')') { LastChar = getc(pFile); columnNo++; return returnTok(")", RPAR); }
  if (LastChar == '[') { LastChar = getc(pFile); columnNo++; return returnTok("[", LBOX); } // Array support
  if (LastChar == ']') { LastChar = getc(pFile); columnNo++; return returnTok("]", RBOX); } // Array support
  if (LastChar == ';') { LastChar = getc(pFile); columnNo++; return returnTok(";", SC); }
  if (LastChar == ',') { LastChar = getc(pFile); columnNo++; return returnTok(",", COMMA); }

  if (isdigit(LastChar) || LastChar == '.') {
    std::string NumStr;
    if (LastChar == '.') {
      do { NumStr += LastChar; LastChar = getc(pFile); columnNo++; } while (isdigit(LastChar));
      return returnTok(NumStr, FLOAT_LIT);
    } else {
      do { NumStr += LastChar; LastChar = getc(pFile); columnNo++; } while (isdigit(LastChar));
      if (LastChar == '.') {
        do { NumStr += LastChar; LastChar = getc(pFile); columnNo++; } while (isdigit(LastChar));
        return returnTok(NumStr, FLOAT_LIT);
      } else {
        return returnTok(NumStr, INT_LIT);
      }
    }
  }

  if (LastChar == '&') {
    int NextChar = getc(pFile);
    if (NextChar == '&') { LastChar = getc(pFile); columnNo += 2; return returnTok("&&", AND); }
    else { LastChar = NextChar; columnNo++; return returnTok("&", int('&')); }
  }

  if (LastChar == '|') {
    int NextChar = getc(pFile);
    if (NextChar == '|') { LastChar = getc(pFile); columnNo += 2; return returnTok("||", OR); }
    else { LastChar = NextChar; columnNo++; return returnTok("|", int('|')); }
  }

  if (LastChar == '!') {
    int NextChar = getc(pFile);
    if (NextChar == '=') { LastChar = getc(pFile); columnNo += 2; return returnTok("!=", NE); }
    else { LastChar = NextChar; columnNo++; return returnTok("!", NOT); }
  }

  if (LastChar == '<') {
    int NextChar = getc(pFile);
    if (NextChar == '=') { LastChar = getc(pFile); columnNo += 2; return returnTok("<=", LE); }
    else { LastChar = NextChar; columnNo++; return returnTok("<", LT); }
  }

  if (LastChar == '>') {
    int NextChar = getc(pFile);
    if (NextChar == '=') { LastChar = getc(pFile); columnNo += 2; return returnTok(">=", GE); }
    else { LastChar = NextChar; columnNo++; return returnTok(">", GT); }
  }

  if (LastChar == '/') {
    LastChar = getc(pFile);
    columnNo++;
    if (LastChar == '/') {
      do { LastChar = getc(pFile); columnNo++; } while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');
      if (LastChar != EOF) return gettok();
    } else return returnTok("/", DIV);
  }

  if (LastChar == EOF) { columnNo++; return returnTok("0", EOF_TOK); }

  int ThisChar = LastChar;
  std::string s(1, ThisChar);
  LastChar = getc(pFile);
  columnNo++;
  return returnTok(s, int(ThisChar));
}

//===----------------------------------------------------------------------===//
// Parser & AST Setup
//===----------------------------------------------------------------------===//

static TOKEN CurTok;
static std::deque<TOKEN> tok_buffer;

static TOKEN getNextToken() {
  if (tok_buffer.size() == 0) tok_buffer.push_back(gettok());
  TOKEN temp = tok_buffer.front();
  tok_buffer.pop_front();
  return CurTok = temp;
}

static void putBackToken(TOKEN tok) { tok_buffer.push_front(tok); }

static bool isType(int tokType) {
  return (tokType == INT_TOK || tokType == FLOAT_TOK || tokType == BOOL_TOK || tokType == VOID_TOK);
}

static bool isExprStart(int tokType) {
  return (tokType == IDENT || tokType == INT_LIT || tokType == FLOAT_LIT || 
          tokType == BOOL_LIT || tokType == NOT || tokType == MINUS || 
          tokType == LPAR);
}

//===----------------------------------------------------------------------===//
// Code Generation Globals
//===----------------------------------------------------------------------===//

static LLVMContext TheContext;
static IRBuilder<> Builder(TheContext);
static std::unique_ptr<Module> TheModule;
static std::map<std::string, AllocaInst *> NamedValues;
static std::map<std::string, GlobalVariable *> GlobalNamedValues;
static std::map<std::string, Function *> FunctionProtos;

Value *LogErrorV(const char *Str) {
  fprintf(stderr, "Error: %s\n", Str);
  return nullptr;
}

// Helper to create an alloca in the entry block of a function.
static AllocaInst *CreateEntryBlockAlloca(Function *TheFunction, StringRef VarName, Type *Ty) {
  IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
  return TmpB.CreateAlloca(Ty, nullptr, VarName);
}

// Helper for Semantic Analysis: Get LLVM Type for MiniC Types
Type *getLLVMType(StringRef typeStr) {
    if (typeStr == "int") return Type::getInt32Ty(TheContext);
    if (typeStr == "float") return Type::getFloatTy(TheContext);
    if (typeStr == "bool") return Type::getInt1Ty(TheContext);
    if (typeStr == "void") return Type::getVoidTy(TheContext);
    return nullptr;
}

//===----------------------------------------------------------------------===//
// AST Nodes
//===----------------------------------------------------------------------===//

// ANSI Color Codes for Clang-like output
const std::string RESET   = "\033[0m";
const std::string GREEN   = "\033[1;32m"; 
const std::string CYAN    = "\033[0;36m"; 
const std::string YELLOW  = "\033[0;33m"; 
const std::string MAGENTA = "\033[0;35m"; 

std::string tokenLabel(int tok) {
  if (tok == PLUS) return "+";
  if (tok == MINUS) return "-";
  if (tok == ASTERIX) return "*";
  if (tok == DIV) return "/";
  if (tok == MOD) return "%";
  if (tok == EQ) return "==";
  if (tok == NE) return "!=";
  if (tok == LT) return "<";
  if (tok == LE) return "<=";
  if (tok == GT) return ">";
  if (tok == GE) return ">=";
  if (tok == AND) return "&&";
  if (tok == OR) return "||";
  if (tok == NOT) return "!";
  if (tok == ASSIGN) return "=";
  return "unknown_op";
}

class VariableASTnode; // Forward Declaration
class ArrayAccessASTnode; // Forward Declaration

class ASTnode {
public:
  virtual ~ASTnode() {}
  virtual Value *codegen() = 0;
  virtual void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const = 0;
  virtual std::string to_string(int d = 0) const { return ""; }
  // Add Helper to cast safely without RTTI
  virtual VariableASTnode *asVariable() { return nullptr; }
  virtual ArrayAccessASTnode *asArrayAccess() { return nullptr; }
};

llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const ASTnode& ast) {
  ast.print(os, "", true);
  return os;
}

void printNode(llvm::raw_ostream &os, std::string prefix, bool isLast, 
               std::string nodeName, int line, int col, std::string extra = "") {
    os << prefix;
    os << (isLast ? "`-" : "|-");
    os << GREEN << nodeName << RESET << " ";
    if (line > 0) os << CYAN << "<line:" << line << ":" << col << ">" << RESET << " ";
    else os << CYAN << "<invalid sloc>" << RESET << " ";
    if (!extra.empty()) os << YELLOW << extra << RESET;
    os << "\n";
}

// === Expressions ===

class IntASTnode : public ASTnode {
  int Val;
  TOKEN Tok;
public:
  IntASTnode(TOKEN tok, int val) : Val(val), Tok(tok) {}
  
  Value *codegen() override {
      return ConstantInt::get(TheContext, APInt(32, Val, true));
  }

  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
    printNode(os, prefix, isLast, "IntegerLiteral", Tok.lineNo, Tok.columnNo, std::to_string(Val));
  }
};

class FloatASTnode : public ASTnode {
  double Val;
  TOKEN Tok;
public:
  FloatASTnode(TOKEN tok, double Val) : Val(Val), Tok(tok) {}
  
  Value *codegen() override {
      return ConstantFP::get(TheContext, APFloat(Val));
  }

  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
    printNode(os, prefix, isLast, "FloatingLiteral", Tok.lineNo, Tok.columnNo, std::to_string(Val));
  }
};

class BoolASTnode : public ASTnode {
  bool Bool;
  TOKEN Tok;
public:
  BoolASTnode(TOKEN tok, bool B) : Bool(B), Tok(tok) {}
  
  Value *codegen() override {
      return ConstantInt::get(TheContext, APInt(1, Bool ? 1 : 0));
  }

  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
    printNode(os, prefix, isLast, "CXXBoolLiteralExpr", Tok.lineNo, Tok.columnNo, Bool ? "true" : "false");
  }
};

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
  int getLine() const { return Tok.lineNo; }
  int getCol() const { return Tok.columnNo; }
  
  // Override to return this
  VariableASTnode *asVariable() override { return this; }

  Value *codegen() override {
      Value *V = NamedValues[Name];
      if (!V) {
          if (GlobalNamedValues.find(Name) != GlobalNamedValues.end()) {
              V = GlobalNamedValues[Name];
          } else {
              return LogErrorV(("Unknown variable name: " + Name).c_str());
          }
      }
      // Check if it's an array being used as a pointer (decay)? 
      // For simple variable access, we just load.
      Type *Ty = nullptr;
      if (auto *AI = dyn_cast<AllocaInst>(V)) Ty = AI->getAllocatedType();
      else if (auto *GV = dyn_cast<GlobalVariable>(V)) Ty = GV->getValueType();
      
      // If it's an array type, we probably want a pointer to the start, not a load of the whole array
      if (Ty && Ty->isArrayTy()) {
          // Array decay: return pointer to first element
          Value *Zero = ConstantInt::get(Type::getInt32Ty(TheContext), 0);
          Value *Args[] = { Zero, Zero };
          return Builder.CreateInBoundsGEP(Ty, V, Args, "arraydecay");
      }

      return Builder.CreateLoad(Ty, V, Name.c_str());
  }

  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
    printNode(os, prefix, isLast, "DeclRefExpr", Tok.lineNo, Tok.columnNo, "'" + Name + "'");
  }
};

// --- New: Array Access AST Node ---
class ArrayAccessASTnode : public ASTnode {
  std::string Name;
  std::vector<std::unique_ptr<ASTnode>> Indices;
  TOKEN Tok;
public:
  ArrayAccessASTnode(TOKEN tok, std::string name, std::vector<std::unique_ptr<ASTnode>> indices)
      : Tok(tok), Name(name), Indices(std::move(indices)) {}
  
  const std::string &getName() const { return Name; }
  ArrayAccessASTnode *asArrayAccess() override { return this; }

  // Helper to get the GEP (pointer to the element)
  Value *codegenPtr() {
      Value *Base = NamedValues[Name];
      if (!Base) {
          if (GlobalNamedValues.find(Name) != GlobalNamedValues.end()) {
              Base = GlobalNamedValues[Name];
          } else {
              return LogErrorV(("Unknown array name: " + Name).c_str());
          }
      }

      // Generate indices
      std::vector<Value*> IdxList;
      IdxList.push_back(ConstantInt::get(Type::getInt32Ty(TheContext), 0)); // De-reference pointer
      for (auto &idx : Indices) {
          Value *Val = idx->codegen();
          if (!Val) return nullptr;
          // Ensure index is int
          if (Val->getType()->isFloatTy())
             Val = Builder.CreateFPToSI(Val, Type::getInt32Ty(TheContext), "idxcast");
          IdxList.push_back(Val);
      }
      
      Type *BaseType = nullptr;
      if(auto *AI = dyn_cast<AllocaInst>(Base)) BaseType = AI->getAllocatedType();
      else if(auto *GV = dyn_cast<GlobalVariable>(Base)) BaseType = GV->getValueType();

      return Builder.CreateInBoundsGEP(BaseType, Base, IdxList, "arrayidx");
  }

  Value *codegen() override {
      Value *Ptr = codegenPtr();
      if (!Ptr) return nullptr;
      // For reading, we load from the calculated address
      // We need to know the element type.
      // GEP returns a pointer. Retrieve the element type from the PointerType.
      Type *PtrTy = Ptr->getType(); 
      // In opaque pointers (LLVM 15+), we might need context. But 11.1 uses typed pointers usually.
      // Actually modern LLVM (like 21.1) uses Opaque Pointers by default so element types must be explicit.
      // We can trace back the array type from NamedValues or assume based on var type?
      // Better: Get type from the Alloca.
      
      Value *Base = NamedValues[Name];
      if(!Base) Base = GlobalNamedValues[Name];
      Type *ArrTy = nullptr;
      if(auto *AI = dyn_cast<AllocaInst>(Base)) ArrTy = AI->getAllocatedType();
      else if(auto *GV = dyn_cast<GlobalVariable>(Base)) ArrTy = GV->getValueType();
      
      // Peel off N layers of ArrayType
      Type *EleTy = ArrTy;
      for(size_t i=0; i<Indices.size(); ++i) {
          if(EleTy->isArrayTy()) EleTy = EleTy->getArrayElementType();
      }

      return Builder.CreateLoad(EleTy, Ptr, "loadarray");
  }

  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
      printNode(os, prefix, isLast, "ArraySubscriptExpr", Tok.lineNo, Tok.columnNo, Name);
      std::string childPrefix = prefix + (isLast ? "  " : "| ");
      for(size_t i=0; i<Indices.size(); ++i) {
          Indices[i]->print(os, childPrefix, i == Indices.size()-1);
      }
  }
};

class BinaryExprAST : public ASTnode {
  int Op;
  std::unique_ptr<ASTnode> LHS, RHS;
public:
  BinaryExprAST(int op, std::unique_ptr<ASTnode> lhs, std::unique_ptr<ASTnode> rhs)
    : Op(op), LHS(std::move(lhs)), RHS(std::move(rhs)) {}
  
  Value *codegen() override {
      // Special Case: Assignment
      if (Op == ASSIGN) {
           Value *Ptr = nullptr;
           std::string VarName;

           if (auto *VarNode = LHS->asVariable()) {
               VarName = VarNode->getName();
               Ptr = NamedValues[VarName];
               if (!Ptr) {
                   if (GlobalNamedValues.find(VarName) != GlobalNamedValues.end())
                       Ptr = GlobalNamedValues[VarName];
                   else return LogErrorV("Unknown variable in assignment");
               }
           } else if (auto *ArrNode = LHS->asArrayAccess()) {
               Ptr = ArrNode->codegenPtr(); // Get address of array element
               if (!Ptr) return nullptr;
           } else {
               return LogErrorV("LHS of '=' must be a variable or array element");
           }
           
           // Generate RHS
           Value *Val = RHS->codegen();
           if (!Val) return nullptr;
           
           // Implicit Widening
           // Get Ptr Element Type.
           // Ptr is a Value*, getType() is PointerType*. 
           // We need the element type it points to. 
           // In new LLVM, we might need to track this explicitly if opaque.
           // For now assume we know it from context or use getNonOpaquePtr logic if available?
           // Simpler: We have the Ptr. Just Store.
           // But for widening, we need to check compatibility.
           // Let's assume simple widening logic:
           if (Val->getType()->isIntegerTy() && Ptr->getType()->isPointerTy()) {
               // This check is tricky with opaque pointers.
               // Let's trust LLVM verification for now or assume we generated correct types.
           }

           Builder.CreateStore(Val, Ptr);
           return Val;
      }

      Value *L = LHS->codegen();
      Value *R = RHS->codegen();
      if (!L || !R) return nullptr;
      
      bool isFloat = L->getType()->isFloatTy() || R->getType()->isFloatTy();
      if (isFloat) {
          if (L->getType()->isIntegerTy()) L = Builder.CreateSIToFP(L, Type::getFloatTy(TheContext), "castL");
          if (R->getType()->isIntegerTy()) R = Builder.CreateSIToFP(R, Type::getFloatTy(TheContext), "castR");
      }

      if (isFloat) {
          switch (Op) {
            case PLUS: return Builder.CreateFAdd(L, R, "addtmp");
            case MINUS: return Builder.CreateFSub(L, R, "subtmp");
            case ASTERIX: return Builder.CreateFMul(L, R, "multmp");
            case DIV: return Builder.CreateFDiv(L, R, "divtmp");
            case LT: L = Builder.CreateFCmpOLT(L, R, "cmptmp"); return Builder.CreateUIToFP(L, Type::getFloatTy(TheContext)); 
            case GT: L = Builder.CreateFCmpOGT(L, R, "cmptmp"); return Builder.CreateUIToFP(L, Type::getFloatTy(TheContext));
            default: return LogErrorV("Invalid binary operator for floating point");
          }
      } else {
          switch (Op) {
            case PLUS: return Builder.CreateAdd(L, R, "addtmp");
            case MINUS: return Builder.CreateSub(L, R, "subtmp");
            case ASTERIX: return Builder.CreateMul(L, R, "multmp");
            case DIV: return Builder.CreateSDiv(L, R, "divtmp");
            case MOD: return Builder.CreateSRem(L, R, "remtmp");
            case LT: return Builder.CreateICmpSLT(L, R, "cmptmp");
            case LE: return Builder.CreateICmpSLE(L, R, "cmptmp");
            case GT: return Builder.CreateICmpSGT(L, R, "cmptmp");
            case GE: return Builder.CreateICmpSGE(L, R, "cmptmp");
            case EQ: return Builder.CreateICmpEQ(L, R, "cmptmp");
            case NE: return Builder.CreateICmpNE(L, R, "cmptmp");
            case AND: return Builder.CreateAnd(L, R, "andtmp");
            case OR: return Builder.CreateOr(L, R, "ortmp");
            default: return LogErrorV("Invalid binary operator");
          }
      }
  }

  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
    printNode(os, prefix, isLast, "BinaryOperator", 0, 0, "'" + tokenLabel(Op) + "'");
    std::string childPrefix = prefix + (isLast ? "  " : "| ");
    if (LHS) LHS->print(os, childPrefix, false);
    if (RHS) RHS->print(os, childPrefix, true);
  }
};

class UnaryExprAST : public ASTnode {
  int Op;
  std::unique_ptr<ASTnode> Operand;
public:
  UnaryExprAST(int op, std::unique_ptr<ASTnode> operand)
    : Op(op), Operand(std::move(operand)) {}
  
  Value *codegen() override {
      Value *OpV = Operand->codegen();
      if (!OpV) return nullptr;
      
      if (Op == MINUS) {
          if (OpV->getType()->isFloatTy()) return Builder.CreateFNeg(OpV, "negtmp");
          return Builder.CreateNeg(OpV, "negtmp");
      } else if (Op == NOT) {
          return Builder.CreateNot(OpV, "nottmp");
      }
      return LogErrorV("Unknown unary operator");
  }

  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
    printNode(os, prefix, isLast, "UnaryOperator", 0, 0, "'" + tokenLabel(Op) + "'");
    std::string childPrefix = prefix + (isLast ? "  " : "| ");
    if (Operand) Operand->print(os, childPrefix, true);
  }
};

class CallExprAST : public ASTnode {
  std::string Callee;
  std::vector<std::unique_ptr<ASTnode>> Args;
public:
  CallExprAST(const std::string &callee, std::vector<std::unique_ptr<ASTnode>> args)
    : Callee(callee), Args(std::move(args)) {}
  
  Value *codegen() override {
      Function *CalleeF = TheModule->getFunction(Callee);
      if (!CalleeF) return LogErrorV("Unknown function referenced");

      if (CalleeF->arg_size() != Args.size())
          return LogErrorV("Incorrect # arguments passed");

      std::vector<Value *> ArgsV;
      for (unsigned i = 0, e = Args.size(); i != e; ++i) {
          Value *ArgVal = Args[i]->codegen();
          if (!ArgVal) return nullptr;
          
          // Widen arg if needed
          Type *ExpectedTy = CalleeF->getArg(i)->getType();
          if (ExpectedTy->isFloatTy() && ArgVal->getType()->isIntegerTy()) {
              ArgVal = Builder.CreateSIToFP(ArgVal, Type::getFloatTy(TheContext), "arg_widen");
          }
          ArgsV.push_back(ArgVal);
      }

      return Builder.CreateCall(CalleeF, ArgsV, "calltmp");
  }

  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
    printNode(os, prefix, isLast, "CallExpr", 0, 0, Callee);
    std::string childPrefix = prefix + (isLast ? "  " : "| ");
    for (size_t i = 0; i < Args.size(); ++i) {
        Args[i]->print(os, childPrefix, i == Args.size() - 1);
    }
  }
};

class ParamAST {
  std::string Name;
  std::string Type;
public:
  ParamAST(const std::string &name, const std::string &type)
      : Name(name), Type(type) {}
  const std::string &getName() const { return Name; }
  const std::string &getType() const { return Type; }
  
  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const {
      os << prefix << (isLast ? "`-" : "|-") << GREEN << "ParmVarDecl" << RESET << " "; 
      os << YELLOW << Name << " '" << Type << "'" << RESET << "\n";
  }
};

class FunctionPrototypeAST : public ASTnode {
  std::string Name;
  std::string Type;
  std::vector<std::unique_ptr<ParamAST>> Params;
public:
  FunctionPrototypeAST(const std::string &name, const std::string &type,
                       std::vector<std::unique_ptr<ParamAST>> params)
      : Name(name), Type(type), Params(std::move(params)) {}
   
  const std::string &getName() const { return Name; }
  const std::string &getType() const { return Type; }
  std::vector<std::unique_ptr<ParamAST>> &getParams() { return Params; }

  Value *codegen() override {
      std::vector<llvm::Type*> Doubles;
      for(auto &p : Params) {
          Doubles.push_back(getLLVMType(p->getType()));
      }

      FunctionType *FT = FunctionType::get(getLLVMType(Type), Doubles, false);
      Function *F = Function::Create(FT, Function::ExternalLinkage, Name, TheModule.get());

      unsigned Idx = 0;
      for (auto &Arg : F->args())
          Arg.setName(Params[Idx++]->getName());
          
      FunctionProtos[Name] = F;
      return F;
  }

  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
      printNode(os, prefix, isLast, "FunctionDecl", 0, 0, Name + " '" + Type + "'");
      std::string childPrefix = prefix + (isLast ? "  " : "| ");
      for(size_t i = 0; i < Params.size(); ++i) {
          Params[i]->print(os, childPrefix, i == Params.size() - 1);
      }
  }
};

class ProgramAST : public ASTnode {
    std::vector<std::unique_ptr<ASTnode>> Elements;
public:
    ProgramAST(std::vector<std::unique_ptr<ASTnode>> elements) 
        : Elements(std::move(elements)) {}
    
    Value *codegen() override {
        for (auto &e : Elements) {
            if(e->codegen() == nullptr) return nullptr;
        }
        return Constant::getNullValue(Type::getInt32Ty(TheContext));
    }

    void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
        printNode(os, prefix, isLast, "TranslationUnitDecl", 0, 0);
        std::string childPrefix = prefix + (isLast ? "  " : "| "); 
        for (size_t i = 0; i < Elements.size(); ++i) {
            Elements[i]->print(os, childPrefix, i == Elements.size() - 1);
        }
    }
};

class DeclAST : public ASTnode {
public:
  virtual ~DeclAST() {}
};

class VarDeclAST : public DeclAST {
  std::unique_ptr<VariableASTnode> Var;
  std::string Type;
  std::vector<int> Dims; // Array Dimensions
public:
  VarDeclAST(std::unique_ptr<VariableASTnode> var, const std::string &type, std::vector<int> dims = {})
      : Var(std::move(var)), Type(type), Dims(std::move(dims)) {}
  
  Value *codegen() override {
      Function *TheFunction = Builder.GetInsertBlock()->getParent();
      llvm::Type* LLVMTy = getLLVMType(Type);
      
      // Handle Arrays
      if (!Dims.empty()) {
          for (auto it = Dims.rbegin(); it != Dims.rend(); ++it) {
              LLVMTy = ArrayType::get(LLVMTy, *it);
          }
      }
      
      AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, Var->getName(), LLVMTy);
      NamedValues[Var->getName()] = Alloca;

      // Init arrays is complex, skip for now or zero init
      if (Dims.empty()) {
         Value *InitVal = Constant::getNullValue(LLVMTy);
         Builder.CreateStore(InitVal, Alloca);
      } else {
         // Zero init for array? 
         // memset? 
      }
      return Alloca;
  }
  
  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
      std::string typeStr = Type;
      for(int d : Dims) typeStr += "[" + std::to_string(d) + "]";
      printNode(os, prefix, isLast, "VarDecl", Var->getLine(), Var->getCol(), Var->getName() + " '" + typeStr + "'");
  }
};

class GlobVarDeclAST : public DeclAST {
  std::unique_ptr<VariableASTnode> Var;
  std::string Type;
  std::vector<int> Dims;
public:
  GlobVarDeclAST(std::unique_ptr<VariableASTnode> var, const std::string &type, std::vector<int> dims = {})
      : Var(std::move(var)), Type(type), Dims(std::move(dims)) {}
  
  Value *codegen() override {
      llvm::Type* LLVMTy = getLLVMType(Type);
      if (!Dims.empty()) {
          for (auto it = Dims.rbegin(); it != Dims.rend(); ++it) {
              LLVMTy = ArrayType::get(LLVMTy, *it);
          }
      }

      TheModule->getOrInsertGlobal(Var->getName(), LLVMTy);
      GlobalVariable *gVar = TheModule->getNamedGlobal(Var->getName());
      gVar->setLinkage(GlobalValue::CommonLinkage);
      gVar->setInitializer(Constant::getNullValue(LLVMTy));
      GlobalNamedValues[Var->getName()] = gVar;
      return gVar;
  }

  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
      std::string typeStr = Type;
      for(int d : Dims) typeStr += "[" + std::to_string(d) + "]";
      printNode(os, prefix, isLast, "VarDecl", Var->getLine(), Var->getCol(), Var->getName() + " '" + typeStr + "'");
  }
};

// === Statements ===

class BlockAST : public ASTnode {
  std::vector<std::unique_ptr<VarDeclAST>> LocalDecls;
  std::vector<std::unique_ptr<ASTnode>> Stmts;
public:
  BlockAST(std::vector<std::unique_ptr<VarDeclAST>> localDecls,
           std::vector<std::unique_ptr<ASTnode>> stmts)
      : LocalDecls(std::move(localDecls)), Stmts(std::move(stmts)) {}
  
  Value *codegen() override {
      for(auto &d : LocalDecls) d->codegen();
      for(auto &s : Stmts) s->codegen();
      return Constant::getNullValue(Type::getInt32Ty(TheContext));
  }

  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
      printNode(os, prefix, isLast, "CompoundStmt", 0, 0);
      std::string childPrefix = prefix + (isLast ? "  " : "| ");
      
      for (size_t i = 0; i < LocalDecls.size(); ++i) {
          bool last = (Stmts.empty() && i == LocalDecls.size() - 1);
          LocalDecls[i]->print(os, childPrefix, last);
      }
      for (size_t i = 0; i < Stmts.size(); ++i) {
          Stmts[i]->print(os, childPrefix, i == Stmts.size() - 1);
      }
  }
};

class FunctionDeclAST : public DeclAST {
  std::unique_ptr<FunctionPrototypeAST> Proto;
  std::unique_ptr<ASTnode> Block;
public:
  FunctionDeclAST(std::unique_ptr<FunctionPrototypeAST> Proto,
                  std::unique_ptr<ASTnode> Block)
      : Proto(std::move(Proto)), Block(std::move(Block)) {}
  
  Value *codegen() override {
      Function *TheFunction = TheModule->getFunction(Proto->getName());
      if (!TheFunction) {
          TheFunction = (Function*)Proto->codegen();
      }
      if (!TheFunction) return nullptr;

      BasicBlock *BB = BasicBlock::Create(TheContext, "entry", TheFunction);
      Builder.SetInsertPoint(BB);

      NamedValues.clear(); 
      
      unsigned Idx = 0;
      auto &Params = Proto->getParams();
      for (auto &Arg : TheFunction->args()) {
          AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, Arg.getName(), Arg.getType());
          Builder.CreateStore(&Arg, Alloca);
          NamedValues[Params[Idx++]->getName()] = Alloca;
      }

      if (Block->codegen()) {
          if(!Builder.GetInsertBlock()->getTerminator()) {
             if (TheFunction->getReturnType()->isVoidTy()) Builder.CreateRetVoid();
             else Builder.CreateRet(Constant::getNullValue(TheFunction->getReturnType()));
          }
          verifyFunction(*TheFunction);
          return TheFunction;
      }

      TheFunction->eraseFromParent();
      return nullptr;
  }

  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
      printNode(os, prefix, isLast, "FunctionDecl", 0, 0, Proto->getName() + " '...'");
      std::string childPrefix = prefix + (isLast ? "  " : "| ");
      auto &Params = Proto->getParams();
      for(auto &p : Params) p->print(os, childPrefix, false);
      Block->print(os, childPrefix, true);
  }
};

class IfExprAST : public ASTnode {
  std::unique_ptr<ASTnode> Cond, Then, Else;
public:
  IfExprAST(std::unique_ptr<ASTnode> Cond, std::unique_ptr<ASTnode> Then,
            std::unique_ptr<ASTnode> Else)
      : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}
  
  Value *codegen() override {
      Value *CondV = Cond->codegen();
      if (!CondV) return nullptr;
      
      if (!CondV->getType()->isIntegerTy(1)) {
          if (CondV->getType()->isFloatTy())
              CondV = Builder.CreateFCmpONE(CondV, ConstantFP::get(TheContext, APFloat(0.0)), "ifcond");
          else
              CondV = Builder.CreateICmpNE(CondV, ConstantInt::get(TheContext, APInt(32, 0)), "ifcond");
      }

      Function *TheFunction = Builder.GetInsertBlock()->getParent();
      
      BasicBlock *ThenBB = BasicBlock::Create(TheContext, "then", TheFunction);
      BasicBlock *ElseBB = BasicBlock::Create(TheContext, "else");
      BasicBlock *MergeBB = BasicBlock::Create(TheContext, "ifcont");

      Builder.CreateCondBr(CondV, ThenBB, ElseBB);
      
      Builder.SetInsertPoint(ThenBB);
      Then->codegen();
      if (!Builder.GetInsertBlock()->getTerminator()) Builder.CreateBr(MergeBB);
      
      ElseBB->insertInto(TheFunction);
      Builder.SetInsertPoint(ElseBB);
      if (Else) Else->codegen();
      if (!Builder.GetInsertBlock()->getTerminator()) Builder.CreateBr(MergeBB);
      
      MergeBB->insertInto(TheFunction);
      Builder.SetInsertPoint(MergeBB);
      
      return Constant::getNullValue(Type::getInt32Ty(TheContext));
  }

  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
      printNode(os, prefix, isLast, "IfStmt", 0, 0);
      std::string childPrefix = prefix + (isLast ? "  " : "| ");
      Cond->print(os, childPrefix, false);
      Then->print(os, childPrefix, Else ? false : true);
      if (Else) Else->print(os, childPrefix, true);
  }
};

class WhileExprAST : public ASTnode {
  std::unique_ptr<ASTnode> Cond, Body;
public:
  WhileExprAST(std::unique_ptr<ASTnode> cond, std::unique_ptr<ASTnode> body)
      : Cond(std::move(cond)), Body(std::move(body)) {}
  
  Value *codegen() override {
      Function *TheFunction = Builder.GetInsertBlock()->getParent();
      BasicBlock *LoopBB = BasicBlock::Create(TheContext, "loop", TheFunction);
      BasicBlock *BodyBB = BasicBlock::Create(TheContext, "loopbody");
      BasicBlock *AfterBB = BasicBlock::Create(TheContext, "afterloop");
      
      Builder.CreateBr(LoopBB);
      Builder.SetInsertPoint(LoopBB);
      
      Value *CondV = Cond->codegen();
      if (!CondV) return nullptr;
      
      if (!CondV->getType()->isIntegerTy(1)) {
          if (CondV->getType()->isFloatTy())
              CondV = Builder.CreateFCmpONE(CondV, ConstantFP::get(TheContext, APFloat(0.0)), "loopcond");
          else
              CondV = Builder.CreateICmpNE(CondV, ConstantInt::get(TheContext, APInt(32, 0)), "loopcond");
      }
      
      Builder.CreateCondBr(CondV, BodyBB, AfterBB);
      
      BodyBB->insertInto(TheFunction);
      Builder.SetInsertPoint(BodyBB);
      Body->codegen();
      Builder.CreateBr(LoopBB);
      
      AfterBB->insertInto(TheFunction);
      Builder.SetInsertPoint(AfterBB);
      
      return Constant::getNullValue(Type::getInt32Ty(TheContext));
  }

  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
      printNode(os, prefix, isLast, "WhileStmt", 0, 0);
      std::string childPrefix = prefix + (isLast ? "  " : "| ");
      Cond->print(os, childPrefix, false);
      Body->print(os, childPrefix, true);
  }
};

class ReturnAST : public ASTnode {
  std::unique_ptr<ASTnode> Val;
public:
  ReturnAST(std::unique_ptr<ASTnode> value) : Val(std::move(value)) {}
  
  Value *codegen() override {
      Value *RetVal = nullptr;
      if (Val) {
          RetVal = Val->codegen();
          if (!RetVal) return nullptr;
          
          Function *F = Builder.GetInsertBlock()->getParent();
          Type *RetTy = F->getReturnType();
          if (RetTy->isFloatTy() && RetVal->getType()->isIntegerTy()) {
              RetVal = Builder.CreateSIToFP(RetVal, RetTy, "ret_cast");
          }
          Builder.CreateRet(RetVal);
      } else {
          Builder.CreateRetVoid();
      }
      return RetVal ? RetVal : Constant::getNullValue(Type::getInt32Ty(TheContext));
  }

  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
      printNode(os, prefix, isLast, "ReturnStmt", 0, 0);
      if (Val) {
          std::string childPrefix = prefix + (isLast ? "  " : "| ");
          Val->print(os, childPrefix, true);
      }
  }
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

// Helper to parse array dimensions [10][5]
static std::vector<int> ParseArrayDims() {
    std::vector<int> dims;
    while (CurTok.type == LBOX) {
        getNextToken(); // eat [
        if (CurTok.type != INT_LIT) {
            LogError(CurTok, "Expected integer literal for array size");
            return dims;
        }
        dims.push_back(CurTok.getIntVal());
        getNextToken(); // eat int
        if (CurTok.type != RBOX) {
            LogError(CurTok, "Expected ']'");
            return dims;
        }
        getNextToken(); // eat ]
    }
    return dims;
}

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
    else if (CurTok.type == LBOX) { // Array Access
        std::vector<std::unique_ptr<ASTnode>> Indices;
        while(CurTok.type == LBOX) {
            getNextToken(); // eat [
            auto Idx = ParseExper();
            if(!Idx) return nullptr;
            Indices.push_back(std::move(Idx));
            if(CurTok.type != RBOX) return LogError(CurTok, "Expected ']'");
            getNextToken(); // eat ]
        }
        return std::make_unique<ArrayAccessASTnode>(IdTok, IdName, std::move(Indices));
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
    // Look ahead
    TOKEN TempId = CurTok; 
    getNextToken(); // Eat Ident

    if (CurTok.type == LBOX) { // Array Assignment? arr[0] = 1;
         // This path is complex for LL(1) without backtracking if we don't know if it's assignment.
         // But array access IS an expression. 
         // So we can actually just parse the LHS as a Primary/Postfix expr (ArrayAccess).
         // Then check for '='.
         // Let's put token back and call higher level parser if not simple ident=
         putBackToken(CurTok);
         CurTok = TempId;
         // Fall through to logical_or, which will call Primary, which handles ArrayAccess
         // BUT standard LogicalOr doesn't handle assignment.
         // We need to handle Array Assignment here.
         
         // Let's parse the Array Access manually here to see if followed by =
         // Or better: Parse LHS fully (as an expression).
         // If LHS is valid L-Value (Var or ArrayAccess) and next is =, it's assignment.
         // But recursive descent structure makes this hard.
         // Hack: Peek ahead?
         // Simpler: Let's assume the grammar provided for expr handles this?
         // The grammar says: expr ::= IDENT "=" expr | assign_expr
         // It doesn't explicitly say arr[i] = expr.
         // BUT usually array access is a primary expr.
         // If we want to support arr[i] = x, we need to extend the grammar rule:
         // expr ::= primary_expr "=" expr | assign_expr
         // But strict grammar compliance might be needed. 
         // Let's assume standard C behavior: lvalue = expr.
         // We implement it by parsing Primary. If it returns Var or ArrayAccess, check for =.
         
         auto LHS = ParsePrimary(); // This eats [indices]
         if (LHS && CurTok.type == ASSIGN) {
             getNextToken(); // eat =
             auto RHS = ParseExper();
             return std::make_unique<BinaryExprAST>(ASSIGN, std::move(LHS), std::move(RHS));
         }
         // If not =, we need to continue parsing this as an expression (e.g. arr[i] + 1)
         // This requires passing LHS into the expression climber.
         // Since our climber structure (ParseLogicalOr etc) starts fresh, we can't easily inject LHS.
         // WORKAROUND: Put tokens back? Hard with complex AST.
         // BETTER: Refactor ParseAssignment to call ParseLogicalOr, and checking if that returned an LValue?
         // No, '=' has lowest precedence.
         // Let's stick to the provided grammar strictness first: expr ::= IDENT "=" expr.
         // If the spec implies we must support arr[i]=5, we must handle it.
         // Given "Grand Finale", let's support it.
         // We already parsed LHS. We can "climb" from there.
         // But our hierarchy calls downwards.
         // Correct way: ParseUnary -> ... -> Primary.
         // If we change expr ::= unary_expr "=" expr, we cover both IDENT and arr[].
         
         // For now, let's use the putback strategy if it's just array access usage, 
         // but implementing the assignment logic if we see '='.
         // Actually, ParsePrimary eats tokens. We can't easily put back AST.
         // Let's use the C++ trick: Assignment is just another binary op, but right associative.
         // We can implement it in ParseExper?
         
         // Let's go with:
         // expr ::= unary_expr assignment_tail
         // assignment_tail ::= "=" expr | epsilon (but lower precedence than ||)
         
         // Reverting to Simple Logic compatible with existing structure:
         // We only look for IDENT = ... or IDENT[...] = ...
         // We parsed Primary. If we see =, treat as assignment.
         // If not, we must feed this AST node into the next level (LogicalOr).
         // This "feeding" requires changing function signatures or a helper.
         
         // Start parsing at Logical Or.
         // Inside Primary, we handle Ident and ArrayAccess.
         // But wait, Assignment is LOWER precedence than OR.
         // So `a = b || c` parses as `a = (b || c)`.
         // So ParseAssignment calls ParseLogicalOr.
         // If ParseLogicalOr returns a node, and we see '=', we wrap it.
         
         auto L = ParseLogicalOr();
         if (CurTok.type == ASSIGN) {
             getNextToken();
             auto R = ParseExper(); // Right associative
             return std::make_unique<BinaryExprAST>(ASSIGN, std::move(L), std::move(R));
         }
         return L;
    }
     
    if (CurTok.type == ASSIGN) { // It is an assignment!
       getNextToken(); // Eat '='
       auto RHS = ParseExper(); // Recursive for a = b = c
       if (!RHS) return nullptr;
       return std::make_unique<BinaryExprAST>(ASSIGN, 
            std::make_unique<VariableASTnode>(TempId, TempId.getIdentifierStr()), 
            std::move(RHS));
    } 
    else {
        putBackToken(CurTok); 
        CurTok = TempId;      
    }
  }
  
  // Default fallback if not simple IDENT assignment
  // This handles array assignment if we implement the logic above correctly.
  // Let's modify ParseAssignment to be:
  auto L = ParseLogicalOr();
  if (CurTok.type == ASSIGN) {
      getNextToken();
      auto R = ParseExper();
      return std::make_unique<BinaryExprAST>(ASSIGN, std::move(L), std::move(R));
  }
  return L;
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
   
  auto Then = ParseStmt(); // Changed from ParseBlock() to ParseStmt()
  if (!Then) return nullptr;
   
  std::unique_ptr<ASTnode> Else = nullptr;
  if (CurTok.type == ELSE) {
    getNextToken(); // eat else
    Else = ParseStmt();
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

  auto Body = ParseStmt(); 
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

  // Check for array dimensions
  std::vector<int> Dims = ParseArrayDims();
   
  if (CurTok.type != SC) { LogError(CurTok, "Expected ';'"); return nullptr; }
  getNextToken(); // eat ;
   
  auto varNode = std::make_unique<VariableASTnode>(Tok, Name);
  return std::make_unique<VarDeclAST>(std::move(varNode), Type, Dims);
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
     
    if (CurTok.type == LBOX || CurTok.type == SC) { // Global Var
      std::vector<int> Dims = ParseArrayDims();
      
      if (CurTok.type != SC) return LogError(CurTok, "Expected ';'");
      getNextToken(); // eat ;

      auto ident = std::make_unique<VariableASTnode>(IdTok, IdName);
      return std::make_unique<GlobVarDeclAST>(std::move(ident), PrevTok.lexeme, Dims);
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

// now appends to elements
static void ParseDeclList(std::vector<std::unique_ptr<ASTnode>> &elements) {
  while (isType(CurTok.type)) {
    auto decl = ParseDecl();
    if (decl) elements.push_back(std::move(decl));
    else break;
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

// now appends to elements
static void ParseExternList(std::vector<std::unique_ptr<ASTnode>> &elements) {
  while (CurTok.type == EXTERN) {
    auto proto = ParseExtern();
    if (proto) elements.push_back(std::move(proto));
  }
}

static std::unique_ptr<ProgramAST> parser() {
  getNextToken(); // Prime the pump
  
  std::vector<std::unique_ptr<ASTnode>> elements;
  
  ParseExternList(elements);
  ParseDeclList(elements);
  
  return std::make_unique<ProgramAST>(std::move(elements));
}

//===----------------------------------------------------------------------===//
// Main driver code.
//===----------------------------------------------------------------------===//

int main(int argc, char **argv) {
  if (argc == 2) {
    pFile = fopen(argv[1], "r");
    if (pFile == NULL) { perror("Error opening file"); return 1; }
  } else {
    std::cout << "Usage: ./mccomp InputFile\n";
    return 1;
  }

  lineNo = 1; columnNo = 1;
  TheModule = std::make_unique<Module>("mini-c", TheContext);

  // Run the parser and capture the Root AST
  auto RootAST = parser();
   
  fprintf(stderr, "Parsing Finished\n");

  // Print the AST (Task 1)
  if (RootAST) {
      llvm::outs() << *RootAST << "\n";
  }

  // Generate IR (Task 2)
  if (RootAST) {
      if (RootAST->codegen()) {
          fprintf(stderr, "Code Generation Finished\n");
      } else {
          fprintf(stderr, "Code Generation Failed\n");
          return 1;
      }
  }

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
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
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
#include <set>

using namespace llvm;
using namespace llvm::sys;

FILE *pFile;

//===----------------------------------------------------------------------===//
// Lexer
//===----------------------------------------------------------------------===//

enum TOKEN_TYPE {
  IDENT = -1,
  ASSIGN = int('='),

  // delimiters
  LBRA = int('{'),
  RBRA = int('}'),
  LPAR = int('('),
  LBOX = int('['),
  RBOX = int(']'),
  RPAR = int(')'),
  SC = int(';'),
  COMMA = int(','),

  // types
  INT_TOK = -2,
  VOID_TOK = -3,
  FLOAT_TOK = -4,
  BOOL_TOK = -5,

  // keywords
  EXTERN = -6,
  IF = -7,
  ELSE = -8,
  WHILE = -9,
  RETURN = -10,

  // literals
  INT_LIT = -14,
  FLOAT_LIT = -15,
  BOOL_LIT = -16,

  // logical operators
  AND = -17,
  OR = -18,

  // operators
  PLUS = int('+'),
  MINUS = int('-'),
  ASTERIX = int('*'),
  DIV = int('/'),
  MOD = int('%'),
  NOT = int('!'),

  // comparison operators
  EQ = -19,
  NE = -20,
  LE = -21,
  LT = int('<'),
  GE = -23,
  GT = int('>'),

  // special tokens
  EOF_TOK = 0,
  INVALID = -100
};

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

const std::string TOKEN::getIdentifierStr() const { return lexeme; }
const int TOKEN::getIntVal() const { return strtod(lexeme.c_str(), nullptr); }
const float TOKEN::getFloatVal() const { return strtof(lexeme.c_str(), nullptr); }
const bool TOKEN::getBoolVal() const { return (lexeme == "true"); }

static TOKEN returnTok(std::string lexVal, int tok_type) {
  TOKEN return_tok;
  return_tok.lexeme = lexVal;
  return_tok.type = tok_type;
  return_tok.lineNo = lineNo;
  return_tok.columnNo = columnNo - lexVal.length() - 1;
  return return_tok;
}

static TOKEN gettok() {
  static int LastChar = ' ';

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
  if (LastChar == '[') { LastChar = getc(pFile); columnNo++; return returnTok("[", LBOX); }
  if (LastChar == ']') { LastChar = getc(pFile); columnNo++; return returnTok("]", RBOX); }
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
static std::map<std::string, Value *> NamedValues;
static std::map<std::string, GlobalVariable *> GlobalNamedValues;
static std::map<std::string, Type *> NamedVarTypes;
static std::set<std::string> LocalScope;
static std::map<std::string, bool> IsFloatMap;

const std::string RESET   = "\033[0m";
const std::string RED_BOLD = "\033[1;31m";
const std::string WHITE_BOLD = "\033[1;37m";
const std::string GREEN   = "\033[1;32m"; 
const std::string YELLOW  = "\033[0;33m"; 

Value *LogErrorV(const char *Str, int Line = 0, int Col = 0) {
  if (Line > 0) {
      llvm::errs() << WHITE_BOLD << Line << ":" << Col << ": " 
                   << RED_BOLD << "Error: " << RESET << Str << "\n";
  } else {
      llvm::errs() << RED_BOLD << "Error: " << RESET << Str << "\n";
  }
  return nullptr;
}

Type *getLLVMType(StringRef typeStr) {
    if (typeStr == "int") return Type::getInt32Ty(TheContext);
    if (typeStr == "float") return Type::getFloatTy(TheContext);
    if (typeStr == "bool") return Type::getInt1Ty(TheContext);
    if (typeStr == "void") return Type::getVoidTy(TheContext);
    return nullptr;
}

//===----------------------------------------------------------------------===//
// Formatting Helpers
//===----------------------------------------------------------------------===//

void printNode(llvm::raw_ostream &os, std::string prefix, bool isLast, 
               std::string nodeName, std::string extra = "") {
    os << prefix;
    os << (isLast ? "`-" : "|-");
    os << GREEN << nodeName << RESET << " ";
    if (!extra.empty()) os << YELLOW << extra << RESET;
    os << "\n";
}

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

//===----------------------------------------------------------------------===//
// AST Nodes
//===----------------------------------------------------------------------===//

class VariableASTnode;
class ArrayAccessASTnode;

class ASTnode {
public:
  virtual ~ASTnode() {}
  virtual Value *codegen() = 0;
  virtual void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const = 0;

  virtual std::string to_string() const {
      std::string buf;
      llvm::raw_string_ostream os(buf);
      print(os, "", true);
      return os.str();
  }
  
  virtual VariableASTnode *asVariable() { return nullptr; }
  virtual ArrayAccessASTnode *asArrayAccess() { return nullptr; }
};

llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const ASTnode& ast) {
  ast.print(os, "", true);
  return os;
}

class IntASTnode : public ASTnode {
  int Val;
public:
  IntASTnode(TOKEN tok, int val) : Val(val) {}
  Value *codegen() override { return ConstantInt::get(TheContext, APInt(32, Val, true)); }
  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
    printNode(os, prefix, isLast, "IntegerLiteral", std::to_string(Val));
  }
};

class FloatASTnode : public ASTnode {
  double Val;
public:
  FloatASTnode(TOKEN tok, double Val) : Val(Val) {}
  Value *codegen() override { return ConstantFP::get(TheContext, APFloat((float)Val)); }
  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
    printNode(os, prefix, isLast, "FloatingLiteral", std::to_string(Val));
  }
};

class BoolASTnode : public ASTnode {
  bool Bool;
public:
  BoolASTnode(TOKEN tok, bool B) : Bool(B) {}
  Value *codegen() override { return ConstantInt::get(TheContext, APInt(1, Bool ? 1 : 0)); }
  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
    printNode(os, prefix, isLast, "CXXBoolLiteralExpr", Bool ? "true" : "false");
  }
};

class VariableASTnode : public ASTnode {
  std::string Name;
  int Line, Col;
public:
  VariableASTnode(TOKEN tok, const std::string &Name) : Name(Name), Line(tok.lineNo), Col(tok.columnNo) {}
  const std::string &getName() const { return Name; }
  VariableASTnode *asVariable() override { return this; }

  Value *codegen() override {
      Value *V = NamedValues[Name];
      if (!V) {
          if (GlobalNamedValues.count(Name)) V = GlobalNamedValues[Name];
          else return LogErrorV(("Unknown variable: " + Name).c_str(), Line, Col);
      }
      Type *Ty = NamedVarTypes[Name];
      if (!Ty) return LogErrorV("Unknown type for variable", Line, Col);
      if (Ty->isArrayTy()) {
          Value *Zero = ConstantInt::get(Type::getInt32Ty(TheContext), 0);
          return Builder.CreateInBoundsGEP(Ty, V, {Zero, Zero}, "arraydecay");
      }
      return Builder.CreateLoad(Ty, V, Name.c_str());
  }
  
  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
    printNode(os, prefix, isLast, "DeclRefExpr", "'" + Name + "'");
  }
};

class ArrayAccessASTnode : public ASTnode {
  std::string Name;
  std::vector<std::unique_ptr<ASTnode>> Indices;
  int Line, Col;
public:
  ArrayAccessASTnode(TOKEN tok, std::string name, std::vector<std::unique_ptr<ASTnode>> indices)
      : Name(name), Indices(std::move(indices)), Line(tok.lineNo), Col(tok.columnNo) {}

  const std::string &getName() const { return Name; }
  ArrayAccessASTnode *asArrayAccess() override { return this; }

  Value *codegenPtr() {
      Value *Base = NamedValues[Name];
      if (!Base) {
          if (GlobalNamedValues.count(Name)) Base = GlobalNamedValues[Name];
          else return LogErrorV(("Unknown array: " + Name).c_str(), Line, Col);
      }

      Type *BaseType = NamedVarTypes[Name];
      if (!BaseType) return LogErrorV("Type unknown for array", Line, Col);

      std::vector<Value*> IdxList;
      if (BaseType->isArrayTy()) {
          IdxList.push_back(ConstantInt::get(Type::getInt32Ty(TheContext), 0));
      } else if (BaseType->isPointerTy()) {
          Base = Builder.CreateLoad(BaseType, Base, "ptrload");
      }

      for (auto &idx : Indices) {
          Value *Val = idx->codegen();
          if (!Val) return nullptr;
          if (Val->getType()->isFloatTy())
             Val = Builder.CreateFPToSI(Val, Type::getInt32Ty(TheContext), "idxcast");
          IdxList.push_back(Val);
      }
      
      Type *SourceTy = BaseType;
      if (BaseType->isPointerTy()) {
          if (IsFloatMap[Name]) SourceTy = Type::getFloatTy(TheContext);
          else SourceTy = Type::getInt32Ty(TheContext);
      }

      return Builder.CreateInBoundsGEP(BaseType->isArrayTy() ? BaseType : SourceTy, Base, IdxList, "arrayidx");
  }

  Value *codegen() override {
      Value *Ptr = codegenPtr();
      if (!Ptr) return nullptr;
      
      Type *LoadTy = IsFloatMap[Name] ? Type::getFloatTy(TheContext) : Type::getInt32Ty(TheContext);
      return Builder.CreateLoad(LoadTy, Ptr, "tmpload");
  }

  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
      printNode(os, prefix, isLast, "ArraySubscriptExpr", Name);
      std::string childPrefix = prefix + (isLast ? "  " : "| ");
      for(size_t i=0; i<Indices.size(); ++i) {
          Indices[i]->print(os, childPrefix, i == Indices.size()-1);
      }
  }
};

class BinaryExprAST : public ASTnode {
  int Op;
  std::unique_ptr<ASTnode> LHS, RHS;
  int Line, Col;
public:
  BinaryExprAST(int op, int line, int col, std::unique_ptr<ASTnode> lhs, std::unique_ptr<ASTnode> rhs)
    : Op(op), Line(line), Col(col), LHS(std::move(lhs)), RHS(std::move(rhs)) {}

  Value *codegen() override {
      // Assignment
      if (Op == ASSIGN) {
           Value *Ptr = nullptr;
           std::string Name;
           if (auto *V = LHS->asVariable()) {
               Name = V->getName();
               Ptr = NamedValues[Name];
               if(!Ptr && GlobalNamedValues.count(Name)) Ptr = GlobalNamedValues[Name];
           } else if (auto *A = LHS->asArrayAccess()) {
               Name = A->getName();
               Ptr = A->codegenPtr();
               if (!Ptr) return nullptr;
           }
           
           if (!Ptr) return LogErrorV("LHS of = must be variable", Line, Col);
           
           Value *Val = RHS->codegen();
           if (!Val) return nullptr;

           // Type Checking
           Type *TargetTy = NamedVarTypes[Name];
           if (TargetTy) {
               while(TargetTy->isArrayTy()) TargetTy = TargetTy->getArrayElementType();
               bool targetIsFloat = TargetTy->isFloatingPointTy();
               if (TargetTy->isPointerTy()) targetIsFloat = IsFloatMap[Name];

               if (targetIsFloat && Val->getType()->isIntegerTy()) {
                   Val = Builder.CreateSIToFP(Val, Type::getFloatTy(TheContext));
               }
               else if (!targetIsFloat && Val->getType()->isFloatTy()) {
                   return LogErrorV("Narrowing conversion (float -> int) in assignment", Line, Col);
               }
               else if (!targetIsFloat && Val->getType()->isIntegerTy(1)) {
                   Val = Builder.CreateZExt(Val, Type::getInt32Ty(TheContext));
               }
           }

           Builder.CreateStore(Val, Ptr);
           return Val;
      }

      // SHORT CIRCUIT
      if (Op == AND || Op == OR) {
          Value *L = LHS->codegen();
          if (!L) return nullptr;
          if (!L->getType()->isIntegerTy(1)) {
              if (L->getType()->isFloatTy()) L = Builder.CreateFCmpONE(L, ConstantFP::get(TheContext, APFloat(0.0)));
              else L = Builder.CreateICmpNE(L, ConstantInt::get(TheContext, APInt(32, 0)));
          }

          Function *F = Builder.GetInsertBlock()->getParent();
          BasicBlock *RHSBB = BasicBlock::Create(TheContext, "sc_rhs", F);
          BasicBlock *MergeBB = BasicBlock::Create(TheContext, "sc_merge");
          BasicBlock *StartBB = Builder.GetInsertBlock();

          Value *ShortCircuitRes = (Op == AND) ? ConstantInt::getFalse(TheContext) : ConstantInt::getTrue(TheContext);
          
          if (Op == AND) Builder.CreateCondBr(L, RHSBB, MergeBB);
          else Builder.CreateCondBr(L, MergeBB, RHSBB);

          Builder.SetInsertPoint(RHSBB);
          Value *R = RHS->codegen();
          if (!R) return nullptr;
          if (!R->getType()->isIntegerTy(1)) {
               if (R->getType()->isFloatTy()) R = Builder.CreateFCmpONE(R, ConstantFP::get(TheContext, APFloat(0.0)));
               else R = Builder.CreateICmpNE(R, ConstantInt::get(TheContext, APInt(32, 0)));
          }
          Builder.CreateBr(MergeBB);
          BasicBlock *RHSEndBB = Builder.GetInsertBlock();

          MergeBB->insertInto(F);
          Builder.SetInsertPoint(MergeBB);
          PHINode *PN = Builder.CreatePHI(Type::getInt1Ty(TheContext), 2);
          PN->addIncoming(ShortCircuitRes, StartBB);
          PN->addIncoming(R, RHSEndBB);
          
          return Builder.CreateZExt(PN, Type::getInt32Ty(TheContext));
      }

      Value *L = LHS->codegen();
      Value *R = RHS->codegen();
      if (!L || !R) return nullptr;
      
      if (L->getType()->isIntegerTy(1)) L = Builder.CreateZExt(L, Type::getInt32Ty(TheContext));
      if (R->getType()->isIntegerTy(1)) R = Builder.CreateZExt(R, Type::getInt32Ty(TheContext));

      bool isFloat = L->getType()->isFloatTy() || R->getType()->isFloatTy();
      if (isFloat) {
          if (L->getType()->isIntegerTy()) L = Builder.CreateSIToFP(L, Type::getFloatTy(TheContext));
          if (R->getType()->isIntegerTy()) R = Builder.CreateSIToFP(R, Type::getFloatTy(TheContext));
          
          switch (Op) {
            case PLUS: return Builder.CreateFAdd(L, R, "addtmp");
            case MINUS: return Builder.CreateFSub(L, R, "subtmp");
            case ASTERIX: return Builder.CreateFMul(L, R, "multmp");
            case DIV: return Builder.CreateFDiv(L, R, "divtmp");
            case LT: return Builder.CreateUIToFP(Builder.CreateFCmpOLT(L,R), Type::getFloatTy(TheContext)); 
            case GT: return Builder.CreateUIToFP(Builder.CreateFCmpOGT(L,R), Type::getFloatTy(TheContext));
            case LE: return Builder.CreateUIToFP(Builder.CreateFCmpOLE(L,R), Type::getFloatTy(TheContext));
            case GE: return Builder.CreateUIToFP(Builder.CreateFCmpOGE(L,R), Type::getFloatTy(TheContext));
            case EQ: return Builder.CreateUIToFP(Builder.CreateFCmpOEQ(L,R), Type::getFloatTy(TheContext));
            case NE: return Builder.CreateUIToFP(Builder.CreateFCmpONE(L,R), Type::getFloatTy(TheContext));
            default: return LogErrorV("Invalid float binary operator", Line, Col);
          }
      } else {
          switch (Op) {
            case PLUS: return Builder.CreateAdd(L, R, "addtmp");
            case MINUS: return Builder.CreateSub(L, R, "subtmp");
            case ASTERIX: return Builder.CreateMul(L, R, "multmp");
            case DIV: return Builder.CreateSDiv(L, R, "divtmp");
            case MOD: return Builder.CreateSRem(L, R, "remtmp");
            case LT: return Builder.CreateZExt(Builder.CreateICmpSLT(L,R), Type::getInt32Ty(TheContext));
            case LE: return Builder.CreateZExt(Builder.CreateICmpSLE(L,R), Type::getInt32Ty(TheContext));
            case GT: return Builder.CreateZExt(Builder.CreateICmpSGT(L,R), Type::getInt32Ty(TheContext));
            case GE: return Builder.CreateZExt(Builder.CreateICmpSGE(L,R), Type::getInt32Ty(TheContext));
            case EQ: return Builder.CreateZExt(Builder.CreateICmpEQ(L,R), Type::getInt32Ty(TheContext));
            case NE: return Builder.CreateZExt(Builder.CreateICmpNE(L,R), Type::getInt32Ty(TheContext));
            default: return LogErrorV("Invalid binary operator", Line, Col);
          }
      }
  }

  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
    printNode(os, prefix, isLast, "BinaryOperator", "'" + tokenLabel(Op) + "'");
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
          if (OpV->getType()->isFloatTy()) OpV = Builder.CreateFCmpONE(OpV, ConstantFP::get(TheContext, APFloat(0.0)));
          else if (OpV->getType()->isIntegerTy(32)) OpV = Builder.CreateICmpNE(OpV, ConstantInt::get(TheContext, APInt(32, 0)));
          
          OpV = Builder.CreateNot(OpV, "nottmp");
          return Builder.CreateZExt(OpV, Type::getInt32Ty(TheContext));
      }
      return LogErrorV("Unknown unary operator");
  }

  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
    printNode(os, prefix, isLast, "UnaryOperator", "'" + tokenLabel(Op) + "'");
    std::string childPrefix = prefix + (isLast ? "  " : "| ");
    if (Operand) Operand->print(os, childPrefix, true);
  }
};

class CallExprAST : public ASTnode {
  std::string Callee;
  int Line, Col;
  std::vector<std::unique_ptr<ASTnode>> Args;
public:
  CallExprAST(TOKEN tok, const std::string &callee, std::vector<std::unique_ptr<ASTnode>> args)
    : Callee(callee), Line(tok.lineNo), Col(tok.columnNo), Args(std::move(args)) {}
  
  Value *codegen() override {
      Function *CalleeF = TheModule->getFunction(Callee);
      if (!CalleeF) return LogErrorV(("Unknown function referenced: " + Callee).c_str(), Line, Col);

      if (!CalleeF->isVarArg() && CalleeF->arg_size() != Args.size())
          return LogErrorV(("Incorrect # arguments passed to " + Callee).c_str(), Line, Col);

      std::vector<Value *> ArgsV;
      for (unsigned i = 0, e = Args.size(); i != e; ++i) {
          Value *ArgVal = Args[i]->codegen();
          if (!ArgVal) return nullptr;
          
          if (i < CalleeF->arg_size()) {
              Type *ExpectedTy = CalleeF->getArg(i)->getType();
              Type *ArgTy = ArgVal->getType();

              if (ExpectedTy->isFloatingPointTy() && ArgTy->isIntegerTy()) {
                  ArgVal = Builder.CreateSIToFP(ArgVal, Type::getFloatTy(TheContext));
              } else if (ExpectedTy->isIntegerTy() && ArgTy->isFloatingPointTy()) {
                  return LogErrorV("Narrowing conversion (float->int) in call", Line, Col);
              } else if (ExpectedTy->isIntegerTy(32) && ArgTy->isIntegerTy(1)) {
                  ArgVal = Builder.CreateZExt(ArgVal, Type::getInt32Ty(TheContext));
              }
          }
          ArgsV.push_back(ArgVal);
      }

      return Builder.CreateCall(CalleeF, ArgsV, "calltmp");
  }

  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
    printNode(os, prefix, isLast, "CallExpr", Callee);
    std::string childPrefix = prefix + (isLast ? "  " : "| ");
    for (size_t i = 0; i < Args.size(); ++i) {
        Args[i]->print(os, childPrefix, i == Args.size() - 1);
    }
  }
};

class ParamAST {
  std::string Name;
  std::string Type;
  std::vector<int> Dims;
public:
  ParamAST(const std::string &name, const std::string &type, std::vector<int> dims)
      : Name(name), Type(type), Dims(std::move(dims)) {}

  const std::string &getName() const { return Name; }
  const std::string &getType() const { return Type; }
  const std::vector<int> &getDims() const { return Dims; }
  
  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const {
      std::string s = Name + " '" + Type;
      for(int d : Dims) s += "[" + std::to_string(d) + "]";
      s += "'";
      printNode(os, prefix, isLast, "ParmVarDecl", s);
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
  std::vector<std::unique_ptr<ParamAST>> &getParams() { return Params; }

  Value *codegen() override {
      std::vector<llvm::Type*> ArgTypes;
      for(auto &p : Params) {
          llvm::Type *T = getLLVMType(p->getType());
          if (!p->getDims().empty()) {
              T = PointerType::getUnqual(TheContext); 
          }
          ArgTypes.push_back(T);
      }

      FunctionType *FT = FunctionType::get(getLLVMType(Type), ArgTypes, false);
      Function *F = Function::Create(FT, Function::ExternalLinkage, Name, TheModule.get());
      
      unsigned Idx = 0;
      for (auto &Arg : F->args()) Arg.setName(Params[Idx++]->getName());
      return F;
  }

  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
      printNode(os, prefix, isLast, "FunctionDecl", Name + " '" + Type + "'");
      std::string childPrefix = prefix + (isLast ? "  " : "| ");
      for(size_t i = 0; i < Params.size(); ++i) {
          Params[i]->print(os, childPrefix, i == Params.size() - 1);
      }
  }
};

class VarDeclAST : public ASTnode {
  std::unique_ptr<VariableASTnode> Var;
  std::string TypeStr;
  std::vector<int> Dims; 
public:
  VarDeclAST(std::unique_ptr<VariableASTnode> var, const std::string &type, std::vector<int> dims = {})
      : Var(std::move(var)), TypeStr(type), Dims(std::move(dims)) {}
  
  Value *codegen() override {
      if (LocalScope.count(Var->getName())) return LogErrorV("Redefinition of variable");
      
      Function *TheFunction = Builder.GetInsertBlock()->getParent();
      llvm::Type* LLVMTy = getLLVMType(TypeStr);
      
      if (!Dims.empty()) {
          for (auto it = Dims.rbegin(); it != Dims.rend(); ++it) {
              LLVMTy = ArrayType::get(LLVMTy, *it);
          }
      }
      
      IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
      AllocaInst *Alloca = TmpB.CreateAlloca(LLVMTy, nullptr, Var->getName());
      NamedValues[Var->getName()] = Alloca;
      NamedVarTypes[Var->getName()] = LLVMTy;
      LocalScope.insert(Var->getName());
      IsFloatMap[Var->getName()] = (TypeStr == "float");

      Value *Size = ConstantInt::get(Type::getInt64Ty(TheContext), TheModule->getDataLayout().getTypeAllocSize(LLVMTy));
      Builder.CreateMemSet(Alloca, ConstantInt::get(Type::getInt8Ty(TheContext), 0), Size, MaybeAlign());
      
      return Alloca;
  }

  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
      std::string typeStr = TypeStr;
      for(int d : Dims) typeStr += "[" + std::to_string(d) + "]";
      printNode(os, prefix, isLast, "VarDecl", Var->getName() + " '" + typeStr + "'");
  }
};

class GlobVarDeclAST : public ASTnode {
  std::unique_ptr<VariableASTnode> Var;
  std::string TypeStr;
  std::vector<int> Dims;
public:
  GlobVarDeclAST(std::unique_ptr<VariableASTnode> var, const std::string &type, std::vector<int> dims = {})
      : Var(std::move(var)), TypeStr(type), Dims(std::move(dims)) {}
  
  Value *codegen() override {
      llvm::Type* LLVMTy = getLLVMType(TypeStr);
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
      NamedVarTypes[Var->getName()] = LLVMTy;
      IsFloatMap[Var->getName()] = (TypeStr == "float");
      return gVar;
  }

  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
      std::string typeStr = TypeStr;
      for(int d : Dims) typeStr += "[" + std::to_string(d) + "]";
      printNode(os, prefix, isLast, "GlobalVarDecl", Var->getName() + " '" + typeStr + "'");
  }
};

class BlockAST : public ASTnode {
  std::vector<std::unique_ptr<VarDeclAST>> LocalDecls;
  std::vector<std::unique_ptr<ASTnode>> Stmts;
public:
  BlockAST(std::vector<std::unique_ptr<VarDeclAST>> localDecls,
           std::vector<std::unique_ptr<ASTnode>> stmts)
      : LocalDecls(std::move(localDecls)), Stmts(std::move(stmts)) {}
  
  Value *codegen() override {
      for(auto &d : LocalDecls) {
          if (!d->codegen()) return nullptr;
      }
      for(auto &s : Stmts) {
          if (!s->codegen()) return nullptr;
      }
      return Constant::getNullValue(Type::getInt32Ty(TheContext));
  }

  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
      printNode(os, prefix, isLast, "CompoundStmt", "");
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

class FunctionDeclAST : public ASTnode {
  std::unique_ptr<FunctionPrototypeAST> Proto;
  std::unique_ptr<ASTnode> Block;
public:
  FunctionDeclAST(std::unique_ptr<FunctionPrototypeAST> Proto,
                  std::unique_ptr<ASTnode> Block)
      : Proto(std::move(Proto)), Block(std::move(Block)) {}
  
  Value *codegen() override {
      Function *TheFunction = TheModule->getFunction(Proto->getName());
      if (!TheFunction) TheFunction = (Function*)Proto->codegen();
      if (!TheFunction) return nullptr;

      BasicBlock *BB = BasicBlock::Create(TheContext, "entry", TheFunction);
      Builder.SetInsertPoint(BB);
      NamedValues.clear();
      LocalScope.clear();
      
      unsigned Idx = 0;
      auto &Params = Proto->getParams();
      for (auto &Arg : TheFunction->args()) {
          llvm::Type *FullTy = getLLVMType(Params[Idx]->getType());
          bool isArray = !Params[Idx]->getDims().empty();
          
          if (isArray) {
               // Array param is passed as Pointer.
               AllocaInst *Alloca = Builder.CreateAlloca(Arg.getType(), nullptr, Params[Idx]->getName());
               Builder.CreateStore(&Arg, Alloca);
               NamedValues[Params[Idx]->getName()] = Alloca;
               NamedVarTypes[Params[Idx]->getName()] = Arg.getType(); 
          } else {
               AllocaInst *Alloca = Builder.CreateAlloca(FullTy, nullptr, Params[Idx]->getName());
               Builder.CreateStore(&Arg, Alloca);
               NamedValues[Params[Idx]->getName()] = Alloca;
               NamedVarTypes[Params[Idx]->getName()] = FullTy;
          }
          IsFloatMap[Params[Idx]->getName()] = (Params[Idx]->getType() == "float");
          LocalScope.insert(Params[Idx]->getName());
          Idx++;
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
      printNode(os, prefix, isLast, "FunctionDecl", "");
      std::string childPrefix = prefix + (isLast ? "  " : "| ");
      Proto->print(os, childPrefix, false);
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
              CondV = Builder.CreateFCmpONE(CondV, ConstantFP::get(TheContext, APFloat(0.0)));
          else
              CondV = Builder.CreateICmpNE(CondV, ConstantInt::get(TheContext, APInt(32, 0)));
      }

      Function *TheFunction = Builder.GetInsertBlock()->getParent();
      
      BasicBlock *ThenBB = BasicBlock::Create(TheContext, "then", TheFunction);
      BasicBlock *ElseBB = BasicBlock::Create(TheContext, "else");
      BasicBlock *MergeBB = BasicBlock::Create(TheContext, "ifcont");

      Builder.CreateCondBr(CondV, ThenBB, ElseBB);
      
      Builder.SetInsertPoint(ThenBB);
      if (!Then->codegen()) return nullptr;
      if (!Builder.GetInsertBlock()->getTerminator()) Builder.CreateBr(MergeBB);
      
      ElseBB->insertInto(TheFunction);
      Builder.SetInsertPoint(ElseBB);
      if (Else) {
          if (!Else->codegen()) return nullptr;
      }
      if (!Builder.GetInsertBlock()->getTerminator()) Builder.CreateBr(MergeBB);
      
      MergeBB->insertInto(TheFunction);
      Builder.SetInsertPoint(MergeBB);
      
      return Constant::getNullValue(Type::getInt32Ty(TheContext));
  }

  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
      printNode(os, prefix, isLast, "IfStmt", "");
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
              CondV = Builder.CreateFCmpONE(CondV, ConstantFP::get(TheContext, APFloat(0.0)));
          else
              CondV = Builder.CreateICmpNE(CondV, ConstantInt::get(TheContext, APInt(32, 0)));
      }
      
      Builder.CreateCondBr(CondV, BodyBB, AfterBB);
      
      BodyBB->insertInto(TheFunction);
      Builder.SetInsertPoint(BodyBB);
      
      if (!Body->codegen()) return nullptr;
      Builder.CreateBr(LoopBB);
      
      AfterBB->insertInto(TheFunction);
      Builder.SetInsertPoint(AfterBB);
      return Constant::getNullValue(Type::getInt32Ty(TheContext));
  }

  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
      printNode(os, prefix, isLast, "WhileStmt", "");
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
          if (F->getReturnType()->isFloatTy() && RetVal->getType()->isIntegerTy())
              RetVal = Builder.CreateSIToFP(RetVal, Type::getFloatTy(TheContext));
          
          Builder.CreateRet(RetVal);
      } else {
          Builder.CreateRetVoid();
      }
      return RetVal ? RetVal : Constant::getNullValue(Type::getInt32Ty(TheContext));
  }

  void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
      printNode(os, prefix, isLast, "ReturnStmt", "");
      if (Val) {
          std::string childPrefix = prefix + (isLast ? "  " : "| ");
          Val->print(os, childPrefix, true);
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
            if (!e->codegen()) return nullptr;
        }
        return Constant::getNullValue(Type::getInt32Ty(TheContext));
    }

    void print(llvm::raw_ostream &os, std::string prefix, bool isLast) const override {
        printNode(os, prefix, isLast, "TranslationUnitDecl", "");
        std::string childPrefix = prefix + (isLast ? "  " : "| "); 
        for (size_t i = 0; i < Elements.size(); ++i) {
            Elements[i]->print(os, childPrefix, i == Elements.size() - 1);
        }
    }
};

std::unique_ptr<ASTnode> LogError(TOKEN tok, const char *Str) {
  fprintf(stderr, "%d:%d Error: %s\n", tok.lineNo, tok.columnNo, Str);
  exit(2);
  return nullptr;
}

//===----------------------------------------------------------------------===//
// Recursive Descent Parser
//===----------------------------------------------------------------------===//

static std::unique_ptr<ASTnode> ParseDecl();
static std::unique_ptr<ASTnode> ParseStmt();
static std::unique_ptr<ASTnode> ParseBlock();
static std::unique_ptr<ASTnode> ParseExper(); 
static std::unique_ptr<ParamAST> ParseParam();
static std::unique_ptr<VarDeclAST> ParseLocalDecl();
static std::vector<std::unique_ptr<VarDeclAST>> ParseLocalDecls(); 
static std::vector<std::unique_ptr<ASTnode>> ParseStmtListPrime();

static std::unique_ptr<ASTnode> ParseAssignment();
static std::unique_ptr<ASTnode> ParseLogicalOr();
static std::unique_ptr<ASTnode> ParseLogicalAnd();
static std::unique_ptr<ASTnode> ParseEquality();
static std::unique_ptr<ASTnode> ParseRelational();
static std::unique_ptr<ASTnode> ParseAdditive();
static std::unique_ptr<ASTnode> ParseMultiplicative();
static std::unique_ptr<ASTnode> ParseUnary();
static std::unique_ptr<ASTnode> ParsePrimary();

static std::vector<int> ParseArrayDims() {
    std::vector<int> dims;
    while (CurTok.type == LBOX) {
        getNextToken();
        if (CurTok.type != INT_LIT) { LogError(CurTok, "Expected integer literal"); return dims; }
        dims.push_back(CurTok.getIntVal());
        getNextToken();
        if (CurTok.type != RBOX) { LogError(CurTok, "Expected ']'"); return dims; }
        getNextToken();
    }
    return dims;
}

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
          if (CurTok.type != COMMA) return LogError(CurTok, "Expected ')' or ','");
          getNextToken(); // eat ,
        }
      }
      getNextToken(); // eat )
      return std::make_unique<CallExprAST>(IdTok, IdName, std::move(Args));
    } 
    else if (CurTok.type == LBOX) { // Array Access
        std::vector<std::unique_ptr<ASTnode>> Indices;
        while(CurTok.type == LBOX) {
            getNextToken(); // eat [
            auto Idx = ParseExper();
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
  return LogError(CurTok, "Unknown token in expression");
}

static std::unique_ptr<ASTnode> ParseUnary() {
  if (CurTok.type == NOT || CurTok.type == MINUS) {
    int Opc = CurTok.type;
    getNextToken();
    if (auto Operand = ParseUnary()) { 
      return std::make_unique<UnaryExprAST>(Opc, std::move(Operand));
    }
    return nullptr;
  }
  return ParsePrimary();
}

static std::unique_ptr<ASTnode> ParseMultiplicative() {
  auto LHS = ParseUnary();
  if (!LHS) return nullptr;
   
  while (CurTok.type == ASTERIX || CurTok.type == DIV || CurTok.type == MOD) {
    int Opc = CurTok.type;
    TOKEN OpTok = CurTok;
    getNextToken();
    auto RHS = ParseUnary();
    if (!RHS) return nullptr;
    LHS = std::make_unique<BinaryExprAST>(Opc, OpTok.lineNo, OpTok.columnNo, std::move(LHS), std::move(RHS));
  }
  return LHS;
}

static std::unique_ptr<ASTnode> ParseAdditive() {
  auto LHS = ParseMultiplicative();
  if (!LHS) return nullptr;

  while (CurTok.type == PLUS || CurTok.type == MINUS) {
    int Opc = CurTok.type;
    TOKEN OpTok = CurTok;
    getNextToken();
    auto RHS = ParseMultiplicative();
    if (!RHS) return nullptr;
    LHS = std::make_unique<BinaryExprAST>(Opc, OpTok.lineNo, OpTok.columnNo, std::move(LHS), std::move(RHS));
  }
  return LHS;
}

static std::unique_ptr<ASTnode> ParseRelational() {
  auto LHS = ParseAdditive();
  if (!LHS) return nullptr;

  while (CurTok.type == LT || CurTok.type == LE || CurTok.type == GT || CurTok.type == GE) {
    int Opc = CurTok.type;
    TOKEN OpTok = CurTok;
    getNextToken();
    auto RHS = ParseAdditive();
    if (!RHS) return nullptr;
    LHS = std::make_unique<BinaryExprAST>(Opc, OpTok.lineNo, OpTok.columnNo, std::move(LHS), std::move(RHS));
  }
  return LHS;
}

static std::unique_ptr<ASTnode> ParseEquality() {
  auto LHS = ParseRelational();
  if (!LHS) return nullptr;

  while (CurTok.type == EQ || CurTok.type == NE) {
    int Opc = CurTok.type;
    TOKEN OpTok = CurTok;
    getNextToken();
    auto RHS = ParseRelational();
    if (!RHS) return nullptr;
    LHS = std::make_unique<BinaryExprAST>(Opc, OpTok.lineNo, OpTok.columnNo, std::move(LHS), std::move(RHS));
  }
  return LHS;
}

static std::unique_ptr<ASTnode> ParseLogicalAnd() {
  auto LHS = ParseEquality();
  if (!LHS) return nullptr;

  while (CurTok.type == AND) {
    int Opc = CurTok.type;
    TOKEN OpTok = CurTok;
    getNextToken();
    auto RHS = ParseEquality();
    if (!RHS) return nullptr;
    LHS = std::make_unique<BinaryExprAST>(Opc, OpTok.lineNo, OpTok.columnNo, std::move(LHS), std::move(RHS));
  }
  return LHS;
}

static std::unique_ptr<ASTnode> ParseLogicalOr() {
  auto LHS = ParseLogicalAnd();
  if (!LHS) return nullptr;

  while (CurTok.type == OR) {
    int Opc = CurTok.type;
    TOKEN OpTok = CurTok;
    getNextToken();
    auto RHS = ParseLogicalAnd();
    if (!RHS) return nullptr;
    LHS = std::make_unique<BinaryExprAST>(Opc, OpTok.lineNo, OpTok.columnNo, std::move(LHS), std::move(RHS));
  }
  return LHS;
}

static std::unique_ptr<ASTnode> ParseAssignment() {
  auto L = ParseLogicalOr();
  
  if (CurTok.type == ASSIGN) {
      TOKEN AssignTok = CurTok;
      getNextToken(); // Eat '='
      auto R = ParseExper(); 
      return std::make_unique<BinaryExprAST>(ASSIGN, AssignTok.lineNo, AssignTok.columnNo, std::move(L), std::move(R));
  }
  return L;
}

static std::unique_ptr<ASTnode> ParseExper() {
  return ParseAssignment();
}

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
   
  auto Then = ParseBlock(); 
  if (!Then) return LogError(CurTok, "Expected '{' (block) after if");
   
  std::unique_ptr<ASTnode> Else = nullptr;
  if (CurTok.type == ELSE) {
    getNextToken(); // eat else
    if (CurTok.type == LBRA) {
        Else = ParseBlock();
    } else {
        return LogError(CurTok, "Expected '{' after else");
    }
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
  return LogError(CurTok, "Expected ';' or expression");
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
  while (isExprStart(CurTok.type) || CurTok.type == SC || CurTok.type == LBRA || 
         CurTok.type == IF || CurTok.type == WHILE || CurTok.type == RETURN) {
    auto stmt = ParseStmt();
    if (stmt) list.push_back(std::move(stmt));
    else break;
  }
  return list;
}

static std::unique_ptr<ParamAST> ParseParam() {
  std::string Type = CurTok.lexeme;
  getNextToken(); // eat type
  if (CurTok.type == IDENT) {
    std::string Name = CurTok.getIdentifierStr();
    getNextToken(); // eat identifier
    std::vector<int> Dims = ParseArrayDims();
    return std::make_unique<ParamAST>(Name, Type, Dims);
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
  }
  return list;
}

static std::unique_ptr<VarDeclAST> ParseLocalDecl() {
  std::string Type = CurTok.lexeme;
  getNextToken(); // eat type
  if (CurTok.type != IDENT) return nullptr; 
   
  std::string Name = CurTok.getIdentifierStr();
  TOKEN Tok = CurTok;
  getNextToken(); // eat ID

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

static void ParseDeclList(std::vector<std::unique_ptr<ASTnode>> &elements) {
  while (isType(CurTok.type)) {
    auto decl = ParseDecl();
    if (decl) elements.push_back(std::move(decl));
    else break;
  }
}

static std::unique_ptr<ASTnode> ParseExtern() {
  getNextToken(); // eat extern
  if (!isType(CurTok.type)) return LogError(CurTok, "Expected type after extern");
   
  std::string Type = CurTok.lexeme;
  getNextToken(); // eat type
   
  if (CurTok.type != IDENT) return LogError(CurTok, "Expected identifier");
  std::string Name = CurTok.getIdentifierStr();
  getNextToken(); // eat ID
   
  if (CurTok.type != LPAR) return LogError(CurTok, "Expected '('");
  getNextToken(); // eat (
   
  auto params = ParseParams();
   
  if (CurTok.type != RPAR) return LogError(CurTok, "Expected ')'");
  getNextToken(); // eat )
   
  if (CurTok.type != SC) return LogError(CurTok, "Expected ';'");
  getNextToken(); // eat ;
   
  return std::make_unique<FunctionPrototypeAST>(Name, Type, std::move(params));
}

static void ParseExternList(std::vector<std::unique_ptr<ASTnode>> &elements) {
  while (CurTok.type == EXTERN) {
    auto proto = ParseExtern();
    if (proto) elements.push_back(std::move(proto));
  }
}

static std::unique_ptr<ProgramAST> parser() {
  getNextToken(); 
  std::vector<std::unique_ptr<ASTnode>> elements;
  ParseExternList(elements);
  ParseDeclList(elements);
  return std::make_unique<ProgramAST>(std::move(elements));
}

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

  auto RootAST = parser();

  if (RootAST) {
      llvm::outs() << *RootAST << "\n";
      if (!RootAST->codegen()) {
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
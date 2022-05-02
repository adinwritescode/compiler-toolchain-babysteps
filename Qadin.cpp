#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include <algorithm>
#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <utility>
#include <cctype>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>
#include <map>

using namespace std;

/* Lexer for simple Qadin language
4/29/2022
Adin Gitig
Sources: https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/LangImpl01.html
*/

// Token Dict
enum Token {
    tok_eof = -1,

    // Commands
    tok_gate = -2,
    tok_extern = -3,
    /* More reserved keywords would be addressed here */

    // Term
    tok_id = -4,
    tok_num = -5,
};

static string IdStr; // Global, metadata for tok_id
static double NumVal; // Global, metadata for tok_num

// Lexer, or gettok() function
static int lexer() {
    static int LastChar = ' ';

    while (isspace(LastChar)) { // Loop to skip whitespace btwn tok's
        LastChar = getchar();
    }

    if (isalpha(LastChar)) { // Loop to get identifiers (must begin with a letter)
        
        /* Build out IdStr */
        IdStr = LastChar;
        while (isalnum((LastChar = getchar()))) {
            IdStr += LastChar;
        }

        /* Check against reserved keywords */
        if (IdStr == "gate") {
            return tok_gate;
        } else if (IdStr == "extern") {
            return tok_extern;
        } else {
            return tok_id; // Is some variable identifier
        }

    } else if (isdigit(LastChar) || LastChar == '.') { // Numbers of form x.y
        string NumStr;
        bool one_dec = (LastChar == '.'); // True => decimal num

        do {
            NumStr += LastChar;
            one_dec = one_dec || (LastChar == '.');
            LastChar = getchar();
        } while (isdigit(LastChar) || (LastChar == '.' && !one_dec));

        NumVal = strtod(NumStr.c_str(), 0);
        return tok_num;

    } else if (LastChar == '#') { // Comments

        do {
            LastChar = getchar();
        } while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

        if (LastChar != EOF) {
            return lexer(); // On next line
        } else {
            return tok_eof;
        }

    } else if (LastChar == EOF) { // EOF
        return tok_eof;
    } else { // Some ASCII character like ';', '+' etc., which we treat as its own tok
        int ThisChar = LastChar;
        LastChar = getchar();
        return ThisChar; // Some tok identifier 0 <= tok <= 255, its ASCII value
    }
}


/* AST forms for simple Qadin language
4/29/2022
Adin Gitig
Sources: https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/LangImpl01.html
*/


/* These AST class types will correspond (roughly) to the tokens and then (higher up)
the productions allowed in Qadin.

As constructs, we'll allow:
- Expressions (arithmetic, assignment etc.)
- Function declarations (prototypes)
- Functions definitions (bodies)
*/


/* 1. Expressions.

expr = expr op expr | num | var | func(args)
args = expr expr | ''
num is any number
var and func are any string
op is yet to be defined

*/

// Parent Class for all Expression ASTs.
class ExprAST {

    public:
        virtual ~ExprAST() {}
        virtual llvm::Value *codegen() = 0;
        virtual void pretty_print(string end) = 0;
        
};

// Number Node, created when a num_tok is read
class NumberExprAST : public ExprAST {
    double Val;
    
    public:
        NumberExprAST(double Val) : Val(Val) {}
        void pretty_print(string end) override {
            printf("(Number = %f)%s", Val, end.c_str()); 
        }
        llvm::Value *codegen() override;
};

// VariableExprAST - for referencing or assigning a variable
class VariableExprAST : public ExprAST {
    string IdName;

    public:
        VariableExprAST(string &IdName) : IdName(IdName) {}
        void pretty_print(string end) override { 
            printf("(id = %s)%s", IdName.c_str(), end.c_str()); 
        }
        llvm::Value *codegen() override;
};

// Binary operations, e.g. 1 + (2 * 3). Notably left recursive, so a recursive 
// descent parser will not work.
class BinaryExprAST : public ExprAST {
    char Op;
    unique_ptr<ExprAST> left, right;

    public:
        BinaryExprAST(char op, unique_ptr<ExprAST> left, unique_ptr<ExprAST> right) :
        Op(op), left(move(left)), right(move(right)) {}

        void pretty_print(string end) override { 
            printf("Binary Expr: "); 
            left->pretty_print(""); 
            printf(" %c ", Op); 
            right->pretty_print("");
            printf("%s", end.c_str());
        }

        llvm::Value *codegen() override;
};

// CallExprAST - Expression class for function calls, allows us to do things like
// 2 * multiply(4, 5) or x = add(3, 2)
class CallExprAST : public ExprAST {
    string Callee;
    vector<unique_ptr<ExprAST>> Args;

    public:
        CallExprAST(const string &Callee, vector<unique_ptr<ExprAST>> Args) :
        Callee(Callee), Args(move(Args)) {}
        void pretty_print(string end) override { 
            printf("%s(", Callee.c_str());
            int len = Args.size();
            for (auto & element : Args) {
                if (element != Args[len - 1]) {
                    element->pretty_print(", ");
                } else {
                    element->pretty_print("");
                }
            }
            printf(")%s", end.c_str());
        }
        llvm::Value *codegen() override;
};


/* 2. Interface with functions, or prototypes 

proto = func(args) 

*/

// PrototypeAST - Represents prototype of a function, including name, arg names
// and thus arg number
class PrototypeAST {
    string Name;
    vector<string> Args;

    public:
        PrototypeAST(const string &name, vector<string> Args) :
        Name(name), Args(move(Args)) {}
    
        const string &getName() const {return Name;} // First non-constructor method!
        void pretty_print(string end) { 
            printf("Prototype: [%s(", Name.c_str());
            int len = Args.size();
            for (auto & element : Args) {
                printf("%s", element.c_str());
                if (element != Args[len - 1]) {
                    printf(", ");
                }
            }
            printf(")]%s", end.c_str());
        }
        llvm::Function *codegen();
};


/* 3. Function Bodies 

body = proto body
body = expr

*/

// FunctionAST - Captures the definition of a function
class FunctionAST {
    unique_ptr<PrototypeAST> Proto;
    unique_ptr<ExprAST> Body;

    public:
        FunctionAST(unique_ptr<PrototypeAST> Proto, unique_ptr<ExprAST> Body) :
        Proto(move(Proto)), Body(move(Body)) {}
        void pretty_print(string end) { 
            printf("Function:\n  "); 
            Proto->pretty_print("\n  "); 
            printf("Body: ["); 
            Body->pretty_print("]\n"); 
            printf("%s", end.c_str());
        }
        llvm::Function *codegen();
};

/* Parser for simple Qadin language
4/29/2022
Adin Gitig
Sources: https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/LangImpl01.html

Grammar to be parsed:

toplevelexpr -> expr

external -> 'extern' prototype

function -> 'gate' prototype expr

prototype -> id(args)

expr -> primary binop_rhs
binop_rhs -> op primary binop_rhs | ''

primary -> idexpr | parenexpr | numberexpr

idexpr -> id | prototype
args -> expr | expr, args | ''

parenexpr -> ( expr )

numberexpr -> number
*/

static int CurTok; // Global lookahead
static int getNextTok() { // Updates CurTok and returns next tok
    return CurTok = lexer();
}


/* Helpers for error handling, for Expr's and Proto's respectively.
Keep in mind that a Func is just a Proto and an Expr together, so these are exhaustive */
unique_ptr<ExprAST> LogError(const char *Str) {
    fprintf(stderr, "LogError: %s\n", Str);
    return nullptr;
}
unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
    LogError(Str);
    return nullptr;
}

/* Forward declaration, as many of these functions recurse between themselves */
static unique_ptr<ExprAST> ParseExpression();
static unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, unique_ptr<ExprAST> LHS);

/* We now begin to fill out the grammar. We start with parsing expressions.
1. numberexpr -> number
*/

static unique_ptr<ExprAST> ParseNumberExpr() { // Call when CurrTok is a tok_num
    auto Result = make_unique<NumberExprAST>(NumVal);
    getNextTok(); // Eat number, advance parser
    return move(Result);
}


/*
2. parenexpr -> ( expression )
*/

static unique_ptr<ExprAST> ParseParenExpr() { // Call when CurrTok is '('
    getNextTok(); // eat '('
    auto V = ParseExpression(); // Parse nested expr
    
    if(!V) {
        return nullptr;
    }
    if (CurTok != ')') {
        return LogError("Syntax Error: expected ')'");
    }

    getNextTok(); // Eat ')', advance parser
    return V;
}


/*
3. identifierexpr -> identifier | identifier(expr*)
The * (as far as I can tell) tells us it can be a list of comma separated expr's
*/

static unique_ptr<ExprAST> ParseIdentifierExpr() { // Call when CurrTok is tok_id
    string IdName = IdStr; // Eat either var or func

    getNextTok(); 

    if (CurTok != '(') { // Just a variable, not func call
        return make_unique<VariableExprAST>(IdName);
    } else {
        getNextTok(); // Eat '(, advance to first arg
        vector<unique_ptr<ExprAST>> Args;
        if (CurTok != ')') {

            while (1) {

                if (auto Arg = ParseExpression()) {
                    Args.push_back(move(Arg));
                } else {
                    return nullptr; // No arguments given
                }

                if (CurTok == ')') {
                    break;
                }

                if (CurTok != ',') {
                    printf("%d\n", CurTok);
                    return LogError("Syntax Error: Expected ')' or ',' in argument list");
                }

                getNextTok();
            }
        }

        getNextTok(); // Eat ')'

        return make_unique<CallExprAST>(IdName, move(Args));
    }
}


/* We now define a helper production and a helper parser to go with it

4. primary = identifierexpr | numberexpr | parenexpr
At this point, nothing is left-recursive and we can fully utilize predictive parsing
*/

static unique_ptr<ExprAST> ParsePrimary() {
    // Note we never eat CurTok, because we've written the sub-parsers to expect to do that
    switch (CurTok) {
        case tok_id:
            return ParseIdentifierExpr();
        case tok_num:
            return ParseNumberExpr();
        case '(':
            return ParseParenExpr();
        default:
            return LogError("Parse Error: Unknown token");
    }
}


/* Next, we need to handle binary expressions which canNOT be predictively parsed,
at least not without restructuring the grammar. They are also ambigous, meaning we
may interpret x + y * z as (x + y) * z or x + (y * z). Instead, we utilize a technique
Operator-Precedence Parsing.

We first have to "install operators" by precedence letter, and then we can establish
productions which can't be "torn apart" by lower levels (see dragon book 48-50) */

static map<char, int> BinOpPrecedence; // Global, holds pre-defined precedence levels

static int GetTokPrecedence() {
    if (!isascii(CurTok)) {
        return -1; // If it is not a binop (tok_num, tok_id etc.)
    }

    int TokPrec = BinOpPrecedence[CurTok];
    if (TokPrec <= 0) return -1; // If it's not installed in the map
    return TokPrec;
}

// Call within main()
void install_binops() {
    BinOpPrecedence['<'] = 10;
    BinOpPrecedence['+'] = 20;
    BinOpPrecedence['-'] = 20;
    BinOpPrecedence['*'] = 40;
    BinOpPrecedence['/'] = 40;
    /*
    .
    .
    .
    */
}


/* Our "highest-ranked" non-terminal is primary, which can't be torn apart by a binop
This is why we've made parenexprs and numexprs productions of primary.
5. expression -> primary binop_rhs 
*/

static unique_ptr<ExprAST> ParseExpression() {
  auto LHS = ParsePrimary(); // Without regard to operator
  if (!LHS) return nullptr; // Expr is just ''. RHS is allowed to be empty (i.e. x), but not LHS

  return ParseBinOpRHS(0, move(LHS));
}


/*
6. binop_rhs -> (op primary)*
Again, * indicates repeating I believe

Does all the hard work for us. Evaluates binop precedence, determines how to
associate sub-exprs.

ExprPrec = minimal operator precedence that function is allowed to evaluate
LHS = part of expression which has been parsed so far.
*/

static unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, unique_ptr<ExprAST> LHS) {
    while (1) {
        int TokPrec = GetTokPrecedence();

        if (TokPrec < ExprPrec) { // Has to wait its turn
            return LHS;
        }

        int BinOp = CurTok; // At this point, we've established it's a BinOp 
        getNextTok();       // and one we're allowed to evaluate, so we move on

        auto RHS = ParsePrimary();
        if (!RHS) return nullptr; // e.g. something like x +

        /* Now, decide how to associate. If this is higher, RHS should 
        right associate. If lower (either not a binop, so -1, or + vs. *) 
        RHS associates left*/
        int NextPrec = GetTokPrecedence();

        if (TokPrec < NextPrec) {
            RHS = ParseBinOpRHS(TokPrec + 1, move(RHS));
            if (!RHS) return nullptr;
        }

        // Now that we're confident we're at the right precedence level, merge L/RHS
        // And loop to beginning of while with new LHS
        LHS = make_unique<BinaryExprAST>(BinOp, move(LHS), move(RHS));
    }
}


/* At this point, arbitrary expressions can be parsed. We move on to functions;
definitions, and then declarations
7. prototype -> id(args) 
*/

unique_ptr<PrototypeAST> ParsePrototype() {
    if (CurTok != tok_id) {
        return LogErrorP("Syntax Error: Expected function name in prototype");
    }

    string func_name = IdStr;
    getNextTok();
    if (CurTok != '(') {
        return LogErrorP("Syntax Error: Expected '(' following function name in Prototype");
    }

    vector<string> argnames;
    while(getNextTok() == tok_id) {
        argnames.push_back(IdStr);
    }

    if (CurTok != ')') {
        return LogErrorP("Syntax Error: Expected ')' following arg list in Prototype");
    }
    getNextTok();

    return make_unique<PrototypeAST>(func_name, move(argnames));
}


/*
Functions, or "gates"
8. function -> 'gate' prototype expression
*/

static unique_ptr<FunctionAST> ParseDefn() {
    getNextTok(); // Eat gate
    auto Proto = ParsePrototype();
    if (!Proto) return nullptr;

    if (auto E = ParseExpression()) {
        return make_unique<FunctionAST>(move(Proto), move(E));
    }
    return nullptr;
}


/*
9. external -> 'extern' prototype
*/

static unique_ptr<PrototypeAST> ParseExtern() {
    getNextTok();
    return ParsePrototype();
}


/* Finally, we can have arbitrary "top-level" expressions. We handle by 
defining anonymous nullary (zero argument) functions for them.

10. toplevelexpr -> expr
*/

static unique_ptr<FunctionAST> ParseTopLevelExpr() {
    if (auto E = ParseExpression()) {
        // Make anonymous prototype with no arguments
        auto Proto = make_unique<PrototypeAST>("__anon_expr", vector<string>());
        return make_unique<FunctionAST>(move(Proto), move(E));
    }
    return nullptr;
}


/* We've now built out a fully fledged parser to our little grammar. The following
section allows us to finally execute and test some of this, with the final production

11. top -> function | external | toplevelexpr | ';'

We simply write a loop which invokes the right parsers in the right situations
*/



static void HandleDefn(bool v) {
    if (auto AST = ParseDefn()) {
        fprintf(stderr, "Parsed a function definition.\n");
        if (v) {
            AST->pretty_print("\n");
        }
        if (auto *IR = AST->codegen()) {
            IR->print(llvm::errs());
            fprintf(stderr, "\n");
        }
    } else {
        // Skip token for error recovery.
        getNextTok();
    }
}

static void HandleExtern(bool v) {
    if (auto AST = ParseExtern()) {
        fprintf(stderr, "Parsed an extern\n");
        if (v) {
            AST->pretty_print("\n");
        }
        if (auto *IR = AST->codegen()) {
            IR->print(llvm::errs());
            fprintf(stderr, "\n");
        }
    } else {
        // Skip token for error recovery.
        getNextTok();
    }
}

static void HandleTopLevelExpr(bool v) {
    // Evaluate a top-level expression into an anonymous function.
    if (auto AST = ParseTopLevelExpr()) {
        fprintf(stderr, "Parsed a top-level expr\n");
        if (v) {
            AST->pretty_print("\n");
        }
        if (auto *IR = AST->codegen()) {
            IR->print(llvm::errs());
            fprintf(stderr, "\n");
        }
    } else {
        // Skip token for error recovery.
        getNextTok();
    }
}


static void MainLoop(bool v) {
    while (1) {
        fprintf(stderr, "Qadin> ");
        switch(CurTok) {
            case tok_eof:
                return;
            case ';':
                getNextTok();
                break;
            case tok_gate:
                HandleDefn(v);
                break;
            case tok_extern:
                HandleExtern(v);
                break;
            default:
                HandleTopLevelExpr(v);
                break;
        }
    }
}


/* IR Codegen for simple Qadin language
5/1/2022
Adin Gitig
Sources: https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/LangImpl01.html
*/


static unique_ptr<llvm::LLVMContext> TheContext; // Useful for APIs apparently
static unique_ptr<llvm::IRBuilder<>> Builder; // Makes it easy to gen LLVM instructions
static unique_ptr<llvm::Module> TheModule; // Contains funcs, global vars
// Which values are defined in curr scope, and what their LLVM rep is. 
// In essence, symbol table
static map<string, llvm::Value *> NamedValues;


llvm::Value *LogErrorV(const char *Str) {
    LogError(Str);
    return nullptr;
}


llvm::Value *NumberExprAST::codegen() {
    return llvm::ConstantFP::get(*TheContext, llvm::APFloat(Val));
}


llvm::Value *VariableExprAST::codegen() {
    llvm::Value *V = NamedValues[IdName];
    if (!V) {
        LogErrorV("Unknown Variable Name");
    }
    return V;
}


llvm::Value *BinaryExprAST::codegen() {
    llvm::Value *L = left->codegen();
    llvm::Value *R = right->codegen();

    if (!L || !R) return nullptr;

    switch (Op) {
        case '+':
            return Builder->CreateFAdd(L, R, "addtmp");
        case '-':
            return Builder->CreateFSub(L, R, "subtmp");
        case '*':
            return Builder->CreateFMul(L, R, "multmp");
        case '<':
            L = Builder->CreateFCmpULT(L, R, "cmptmp");
            return Builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*TheContext), "booltmp");
        default:
            return LogErrorV("invalid BinOp");
    }
}


llvm::Value *CallExprAST::codegen() {
    llvm::Function *CalleeF = TheModule->getFunction(Callee);
    if (!CalleeF) {
        return LogErrorV("Unknown function called");
    }

    if (CalleeF->arg_size() != Args.size()) {
        return LogErrorV("Incorrect number of arguments passed");
    }

    vector<llvm::Value*> ArgsV;
    for (unsigned i = 0, e = Args.size(); i != e; ++i) {
        ArgsV.push_back(Args[i]->codegen());
        if (!ArgsV.back()) {
            return nullptr;
        }
    }

    return Builder->CreateCall(CalleeF, ArgsV, "calltmp");
}


llvm::Function* PrototypeAST::codegen() {
    vector<llvm::Type*> Doubles(Args.size(), llvm::Type::getDoubleTy(*TheContext));

    llvm::FunctionType *FT = llvm::FunctionType::get(llvm::Type::getDoubleTy(*TheContext),
    Doubles, false);

    llvm::Function *F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, Name, TheModule.get());

    unsigned Idx = 0;
    for (auto &Arg : F->args()) {
        Arg.setName(Args[Idx++]);
    }

    return F;
}


llvm::Function *FunctionAST::codegen() {
    llvm::Function *TheFunction = TheModule->getFunction(Proto->getName());

    if (!TheFunction) {
        TheFunction = Proto->codegen();
    }

    if (!TheFunction) {
        return nullptr;
    }

    if (!TheFunction->empty()) {
        return (llvm::Function*)LogErrorV("Function can't be redefined.");
    }

    llvm::BasicBlock *BB = llvm::BasicBlock::Create(*TheContext, "entry", TheFunction);
    Builder->SetInsertPoint(BB);

    NamedValues.clear();
    for (auto &Arg : TheFunction->args()) {
        NamedValues[Arg.getName()] = &Arg;
    }

    if (llvm::Value *RetVal = Body->codegen()) {
        Builder->CreateRet(RetVal);

        llvm::verifyFunction(*TheFunction);

        return TheFunction;
    }

    TheFunction->eraseFromParent();
    return nullptr;
}


/* Initialize llvm fancy stuff that I hate */
//===----------------------------------------------------------------------===//
// Top-Level parsing and JIT Driver
//===----------------------------------------------------------------------===//

static void InitializeModule() {
    // Open a new context and module.
    TheContext = make_unique<llvm::LLVMContext>();
    TheModule = make_unique<llvm::Module>("my cool jit", *TheContext);

    // Create a new builder for the module.
    Builder = make_unique<llvm::IRBuilder<>>(*TheContext);
}

int main(int argc, char** argv) {

    bool verbose = false;

    switch(getopt(argc, argv, "v")) {
        case 'v':
            verbose = true;
    }

    install_binops();

    // Prime the first token.
    fprintf(stderr, "Qadin> ");
    getNextTok();

    // Make the module, which holds all the code.
    InitializeModule();

    // Run the main "interpreter loop" now.
    MainLoop(verbose);

    // Print out all of the generated code.
    TheModule->print(llvm::errs(), nullptr);

    return 0;
}
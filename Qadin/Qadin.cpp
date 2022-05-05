/* Driver Loop for simple Qadin language
5/1/2022
Adin Gitig
Sources: https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/LangImpl01.html
*/

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


#include "lexer.h"
#include "ASTs.h"
#include "parsers.h"

using namespace llvm;


static void HandleDefn(bool v) {
    if (auto AST = ParseDefn()) {
        fprintf(stderr, "Parsed a function definition.\n");
        if (v) {
            AST->pretty_print("\n");
        }
        if (auto *IR = AST->codegen()) {
            IR->print(errs());
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
            IR->print(errs());
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
            IR->print(errs());
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


static std::unique_ptr<LLVMContext> TheContext; // Useful for APIs apparently
static std::unique_ptr<IRBuilder<>> Builder; // Makes it easy to gen LLVM instructions
static std::unique_ptr<Module> TheModule; // Contains funcs, global vars
// Which values are defined in curr scope, and what their LLVM rep is. 
// In essence, symbol table
static std::map<std::string, Value *> NamedValues;


Value *LogErrorV(const char *Str) {
    LogError(Str);
    return nullptr;
}


Value *NumberExprAST::codegen() {
    return ConstantFP::get(*TheContext, APFloat(Val));
}


Value *VariableExprAST::codegen() {
    Value *V = NamedValues[IdName];
    if (!V) {
        LogErrorV("Unknown Variable Name");
    }
    return V;
}


Value *BinaryExprAST::codegen() {
    Value *L = left->codegen();
    Value *R = right->codegen();

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
            return Builder->CreateUIToFP(L, Type::getDoubleTy(*TheContext), "booltmp");
        default:
            return LogErrorV("invalid BinOp");
    }
}


Value *CallExprAST::codegen() {
    Function *CalleeF = TheModule->getFunction(Callee);
    if (!CalleeF) {
        return LogErrorV("Unknown function called");
    }

    if (CalleeF->arg_size() != Args.size()) {
        return LogErrorV("Incorrect number of arguments passed");
    }

    std::vector<Value*> ArgsV;
    for (unsigned i = 0, e = Args.size(); i != e; ++i) {
        ArgsV.push_back(Args[i]->codegen());
        if (!ArgsV.back()) {
            return nullptr;
        }
    }

    return Builder->CreateCall(CalleeF, ArgsV, "calltmp");
}


Function* PrototypeAST::codegen() {
    std::vector<Type*> Doubles(Args.size(), Type::getDoubleTy(*TheContext));

    FunctionType *FT = FunctionType::get(Type::getDoubleTy(*TheContext),
    Doubles, false);

    Function *F = Function::Create(FT, Function::ExternalLinkage, Name, TheModule.get());

    unsigned Idx = 0;
    for (auto &Arg : F->args()) {
        Arg.setName(Args[Idx++]);
    }

    return F;
}


Function *FunctionAST::codegen() {
    Function *TheFunction = TheModule->getFunction(Proto->getName());

    if (!TheFunction) {
        TheFunction = Proto->codegen();
    }

    if (!TheFunction) {
        return nullptr;
    }

    if (!TheFunction->empty()) {
        return (Function*)LogErrorV("Function can't be redefined.");
    }

    BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", TheFunction);
    Builder->SetInsertPoint(BB);

    NamedValues.clear();
    for (auto &Arg : TheFunction->args()) {
        NamedValues[Arg.getName()] = &Arg;
    }

    if (Value *RetVal = Body->codegen()) {
        Builder->CreateRet(RetVal);

        verifyFunction(*TheFunction);

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
    TheContext = std::make_unique<LLVMContext>();
    TheModule = std::make_unique<Module>("my cool jit", *TheContext);

    // Create a new builder for the module.
    Builder = std::make_unique<IRBuilder<>>(*TheContext);
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
    TheModule->print(errs(), nullptr);

    return 0;
}
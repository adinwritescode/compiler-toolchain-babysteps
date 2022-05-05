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

using namespace std;

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
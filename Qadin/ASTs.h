/* AST forms for simple Qadin language
4/29/2022
Adin Gitig
Sources: https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/LangImpl01.html
*/

using namespace std;


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
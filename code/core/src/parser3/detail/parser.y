%skeleton "lalr1.cc" /* -*- C++ -*- */
%require "3.0.4"
%defines
%define parser_class_name {inspire_parser}
%define api.namespace {insieme::core::parser3::detail}
%define api.token.constructor
%define api.value.type variant
%define parse.assert
%code requires
{
    /**
     * this code goes in the header
     */

    # include <string>
    #include "insieme/core/ir.h"

    namespace insieme{
    namespace core{
    namespace parser3{
    namespace detail{

        class inspire_driver;
        class inspire_scanner;

    } // detail
    } // parser3
    } // core
    } // insieme

    using namespace insieme::core;

    namespace {
        struct For_decl{
            VariablePtr it;
            ExpressionPtr low;
            ExpressionPtr up;
            ExpressionPtr step;
        };
    }
}

// The parsing context.
%param { parser3::detail::inspire_driver& driver }
%param { parser3::detail::inspire_scanner& scanner }
%locations
%initial-action
{
  // Initialize the initial location.
  @$.initialize(&driver.file);
};

// DEBUG
//%define parse.trace
%define parse.error verbose
%code
{
    /**
     * this code goes in the cpp file
     */

    #include "insieme/core/parser3/detail/driver.h"
    #include "insieme/core/parser3/detail/scanner.h"
    #include "insieme/core/ir.h"

    #include "insieme/core/annotations/naming.h"
    #include "insieme/core/analysis/ir_utils.h"

    #define INSPIRE_MSG(l, n, msg) \
            if(!n) { driver.error(l, msg); YYABORT; }
    #define INSPIRE_TYPE(l, n, t, msg) \
            if(!n->getType().isa<t>()) { driver.error(l, msg); YYABORT; }
    #define INSPIRE_GUARD(l, n) \
            if(!n) { driver.error(l, "unrecoverable error"); YYABORT; }
    #define INSPIRE_NOT_IMPLEMENTED(l) \
            { driver.error(l, "not supported yet "); YYABORT; }
    #define RULE if(driver.inhibit_building()) { break; }
}

%define api.token.prefix {TOK_}
%token 
  END  0  "end of stream"
  ASSIGN  "="
  MINUS   "-"
  PLUS    "+"
  STAR    "*"
  SLASH   "/"

  LPAREN  "("
  RPAREN  ")"
  LCURBRACKET  "{"
  RCURBRACKET  "}"
  LBRACKET  "["
  RBRACKET  "]"
  DQUOTE  "\""
  QUOTE  "\'"

  LT        "<"     
  GT        ">"     
  LEQ       "<="    
  GEQ       ">="    
  EQ        "=="    
  NEQ       "!="    

  BAND      "&"
  BOR       "|"
  BXOR      "^"

  LAND      "&&"
  LOR       "||"
  LNOT      "!"
                                         
  QMARK   "?"
  COLON   ":"
  NAMESPACE   "::"

  ARROW   "->"
  DARROW  "=>"

  SEMIC   ";"
  COMA    ","
  RANGE   ".."
  DOT     "."
  ADDRESS "$"

  
  PARENT        ".as("
  CAST          "CAST("
  INFINITE      "#inf"
  LET           "let"    
  AUTO          "auto"    
  LAMBDA        "lambda"    

  IF            "if"      
  ELSE          "else"    
  FOR           "for"     
  WHILE         "while"   
  DECL          "decl"
  TRY           "try"   
  CATCH         "catch"   
  RETURN        "return"  
  DEFAULT       "default"  
  CASE          "case"  
  SWITCH        "switch"  
  
  VAR           "var"
  NEW           "new"
  LOC           "loc"
  DELETE        "delete"
  UNDEFINED     "undefined"

  TYPE_LITERAL  "type("
  LITERAL       "lit("
  PARAM         "param("

  TRUE          "true"  
  FALSE         "false"  
  STRUCT        "struct"
  UNION         "union"

  SPAWN         "spawn"
  SYNC          "sync"
  SYNCALL       "syncAll"

  TYPE_ONLY
  EXPRESSION_ONLY
  STMT_ONLY
  FULL_PROGRAM
;

    /* operators: they use string for convenience, instead of creating million rules */
%token <std::string> BIN_OP 


    /* Literals */
%token <std::string> STRING "stringlit"
%token <std::string> CHAR "charlit"
%token <std::string> IDENTIFIER "identifier"
%token <std::string> TYPE_VAR   "type_var"
%token <std::string> BOOL "bool"
%token <std::string> INT "int"
%token <std::string> UINT "uint"
%token <std::string> LONG "long"
%token <std::string> ULONG "ulong"
%token <std::string> FLOAT "float"
%token <std::string> DOUBLE "double"
%token <std::string> PARAMVAR "paramvar"

%type  <std::string> "Number" 
%type  <std::string> "indentifier" 

%type <NodePtr> start_rule
%type <ProgramPtr> program

%type <StatementPtr> statement decl_stmt compound_stmt
%type <StatementList>  statement_list
%type <For_decl> for_decl
%type <VariablePtr> var_decl
%type <VariableList> variable_list 
%type <SwitchCasePtr> switch_case
%type <std::vector<SwitchCasePtr> > switch_case_list
%type <std::vector<CatchClausePtr> > catch_clause_list

%type <ExpressionPtr> expression  lambda_expression markable_expression lambda_expression_aux
%type <ExpressionList> expression_list

%type <std::pair<TypeList, IntParamList> > type_param_list
%type <TypePtr> type tuple_or_function
%type <TypeList> type_list parent_list
%type <bool> func_tok

%type <NamedTypeList> member_list tag_type

%type <NamedValuePtr> assign_field 
%type <NamedValueList> assign_rules 


%printer { yyoutput << $$; } <std::string>



/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
%%

/* ~~~~~~~~~~~~ Super rule, the lexer is prepared to return a spetial token and jump to the right rule ~~~~~~~~~~~~ */

%start start_rule;
start_rule : TYPE_ONLY declarations type             { RULE if(!driver.where_errors())  driver.result = $3; }
           | STMT_ONLY statement                     { RULE if(!driver.where_errors())  driver.result = $2; }
           | EXPRESSION_ONLY declarations expression { RULE if(!driver.where_errors())  driver.result = $3; }
           | FULL_PROGRAM  declarations { RULE driver.open_scope(@$, "program"); }  program { RULE if(!driver.where_errors()) driver.result = $4; }
           ;

/* ~~~~~~~~~~~~~~~~~~~  LET ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

let_defs : "lambda" lambda_expression ";"
         | "lambda" lambda_expression "," let_defs 
         | type ";"                                { RULE driver.add_let_type(@1, $1); }
         | type "," { RULE driver.add_let_type(@1, $1); } let_defs                       
         ;

let_decl : "identifier" { RULE driver.add_let_name(@1,$1); } "=" let_defs  { }
         | "identifier" { RULE driver.add_let_name(@1,$1); } "," let_decl  { }
         ;

let_chain : let_decl { RULE driver.close_let_statement(@$); } 
          | let_decl { RULE driver.close_let_statement(@$); } "let" let_chain { }
          ;

declarations : /* empty */ { } 
             | "let" let_chain { }
             ;

/* ~~~~~~~~~~~~~~~~~~~  PROGRAM ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

program : type "identifier" "(" variable_list ")" "{" compound_stmt "}" { RULE
                             INSPIRE_GUARD(@1, $1); 
                             driver.close_scope(@$, "program");
                             TypeList paramTys;
                             for (const auto& var : $4) paramTys.push_back(var.getType());
                             FunctionTypePtr funcType = driver.builder.functionType(paramTys, $1); 
						     ExpressionPtr main = driver.builder.lambdaExpr(funcType, $4, $7);
						     $$ = driver.builder.createProgram(toVector(main));
                        }
        | type "identifier" "(" variable_list ")" "{" "}" {RULE
                             driver.close_scope(@$, "program");
                             TypeList paramTys;
                             for (const auto& var : $4) paramTys.push_back(var.getType());
                             FunctionTypePtr funcType = driver.builder.functionType(paramTys, $1); 
						     ExpressionPtr main = driver.builder.lambdaExpr(funcType, $4, driver.builder.compoundStmt());
						     $$ = driver.builder.createProgram(toVector(main));
                        }
        | type "identifier" "(" ")" "{" compound_stmt "}"{ RULE
                             INSPIRE_GUARD(@1, $1); 
                             driver.close_scope(@$, "program 2");
                             FunctionTypePtr funcType = driver.builder.functionType(TypeList(), $1); 
						     ExpressionPtr main = driver.builder.lambdaExpr(funcType, VariableList(), $6);
						     $$ = driver.builder.createProgram(toVector(main));
                        }
        | type "identifier" ")" "{" "}"{ RULE
                             INSPIRE_GUARD(@1, $1); 
                             driver.close_scope(@$, "program 2");
                             FunctionTypePtr funcType = driver.builder.functionType(TypeList(), $1); 
						     ExpressionPtr main = driver.builder.lambdaExpr(funcType, VariableList(), driver.builder.compoundStmt());
						     $$ = driver.builder.createProgram(toVector(main));
                        }
        ;

variable_list : var_decl {  RULE $$.push_back($1); }
              | var_decl "," variable_list { RULE $3.insert($3.begin(), $1); std::swap($$, $3); }
              ;

/* ~~~~~~~~~~~~~~~~~~~  TYPES ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

type_param_list : "#inf"     { RULE $$.second.insert($$.second.begin(), driver.builder.infiniteIntTypeParam()); }
                | "paramvar" { RULE $$.second.insert($$.second.begin(), driver.gen_type_param_var(@1,$1)); }
                | "int"      { RULE $$.second.insert($$.second.begin(), driver.builder.concreteIntTypeParam(utils::numeric_cast<uint32_t>($1))); }
                | type       { RULE $$.first.insert($$.first.begin(), $1); }

                | "#inf" "," type_param_list {
                        RULE $3.second.insert($3.second.begin(), driver.builder.infiniteIntTypeParam()); 
                        RULE std::swap($$.first, $3.first);
                        RULE std::swap($$.second, $3.second);
                    }
                | "paramvar" "," type_param_list {
                        RULE $3.second.insert($3.second.begin(), driver.gen_type_param_var(@1,$1)); 
                        RULE std::swap($$.first, $3.first);
                        RULE std::swap($$.second, $3.second);
                    }
                | "int" "," type_param_list {
                        RULE $3.second.insert($3.second.begin(), driver.builder.variableIntTypeParam($1[1])); 
                        RULE std::swap($$.first, $3.first);
                        RULE std::swap($$.second, $3.second);
                    }
                | type "," type_param_list {
                        RULE $3.first.insert($3.first.begin(), $1); 
                        RULE std::swap($$.first, $3.first);
                        RULE std::swap($$.second, $3.second);
                    }
                ;

type_list : type               { RULE $$.insert($$.begin(), $1); }
          | type "," type_list { RULE $3.insert($3.begin(), $1); std::swap($$, $3); }

parent_list : type                 { RULE $$.insert($$.begin(), $1); }
            | type ":" parent_list { RULE $3.insert($3.begin(), $1); std::swap($$, $3); }
            ;

/* is a closure? */
func_tok : "->" { RULE $$ = false; }
         | "=>" { RULE $$ = true; }
         ;

member_list : "}" {}
            | type "identifier" "}" { RULE $$.insert($$.begin(), driver.builder.namedType($2, $1)); }
            | type "identifier" ";" member_list { RULE $4.insert($4.begin(), driver.builder.namedType($2, $1)); std::swap($$, $4); }
            ;

//op_semi_colon : /* empty */ | ";"  // this beauty costs one conflict in grammar

tag_type : "{" member_list   { std::swap($$, $2); }
         ;


tuple_or_function : "(" type_list ")"  { RULE if($2.size() <2) error(@2, "tuple type must have 2 fields or more, sorry");
                                         $$ = driver.builder.tupleType($2); 
                                       }
                  | "(" type_list ")" func_tok type {RULE
                                          $$ = driver.genFuncType(@$, $2, $5, $4); 
                                          INSPIRE_GUARD(@$, $$);
                             }
                  | "("")" func_tok type { $$ = driver.genFuncType(@$, TypeList(), $4, $3); }


type : "identifier" "<" type_param_list ">" { RULE $$ = driver.genGenericType(@$, $1, $3.first, $3.second);  }
             | "identifier" "<" ">" {         RULE $$ = driver.genGenericType(@$, $1, TypeList(), IntParamList());  }
             | "identifier" {                 RULE         
                                              $$ = driver.findType(@1, $1);
                                              if(!$$) $$ = $$ = driver.genGenericType(@$, $1, TypeList(), IntParamList()); 
                                              if(!$$) { driver.error(@$, format("undefined type %s", $1)); YYABORT; } 
                            }
                /* func / closure type */
             | "struct" "identifier" tag_type 
                        { RULE $$ = driver.builder.structType(driver.builder.stringValue($2), $3); }
             | "struct" "identifier" "::" parent_list tag_type 
                        { RULE $$ = driver.builder.structType(driver.builder.stringValue($2), driver.builder.parents($4), $5); }
             | "struct" tag_type              
                        { RULE $$ = driver.builder.structType(driver.builder.stringValue(""), $2); }
             | "union" tag_type   { $$ = driver.builder.unionType($2); }
             | "type_var" { RULE $$ = driver.builder.typeVariable($1);  }
             | tuple_or_function { RULE $$ = $1; }
             ;

/* ~~~~~~~~~~~~~~~~~~~  STATEMENTS  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

statement : ";" { RULE $$ = driver.builder.getNoOp(); } 
          | expression ";" { RULE $$ = $1; }
          | "let" let_decl { RULE driver.close_let_statement(@$); $$ =  driver.builder.getNoOp(); }
                /* compound statement */ 
          | "{" "}" { RULE $$ = driver.builder.compoundStmt(StatementList()); }
          | "{"  { RULE driver.open_scope(@$, "compound"); } compound_stmt "}" {RULE  
                                    driver.close_scope(@$, "compound_end"); $$ =$3; 
                 }

                /* variable declarations */
          | "decl" decl_stmt { RULE $$ = $2;  }

                /* if */
          | "if" "(" expression ")" statement {  RULE $$ = driver.builder.ifStmt($3, $5); }
          | "if" "(" expression ")" statement "else" statement { RULE $$ = driver.builder.ifStmt($3, $5, $7); }

                /* loops */
          | "while" "(" expression ")" statement { RULE $$ = driver.builder.whileStmt($3, $5); }
          | "for" { RULE driver.open_scope(@$, "forDecl"); } for_decl statement { RULE
                $$ = driver.builder.forStmt($3.it, $3.low, $3.up, $3.step, $4);
                driver.close_scope(@$, "for end");
            }
                /* switch */
          | "switch" "(" expression ")" "{" switch_case_list "}" { RULE
			    $$ = driver.builder.switchStmt($3, $6, driver.builder.getNoOp());
            }
          | "switch" "(" expression ")" "{" switch_case_list "default" ":" statement "}" { RULE
			    $$ = driver.builder.switchStmt($3, $6, $9);
            }
                /* exceptions */
          | "try" statement "catch" catch_clause_list { RULE
                if(!$2.isa<CompoundStmtPtr>()) { driver.error(@2, "try body must be a compound"); YYABORT; }
			    $$ = driver.builder.tryCatchStmt($2.as<CompoundStmtPtr>(), $4);
            }   
          | "return" expression ";" { RULE $$ = driver.builder.returnStmt($2); }
          | "return" ";" {  RULE $$ = driver.builder.returnStmt(driver.builder.getLangBasic().getUnitConstant()); }
          ;

catch_clause_list : 
                   "catch" "(" ")" statement { RULE
                            if(!$4.isa<CompoundStmtPtr>()) { driver.error(@4, "catch body must be a compound"); YYABORT; }
                            $$.push_back(driver.builder.catchClause(VariablePtr(), $4.as<CompoundStmtPtr>())); 
                    } 
                  | "(" var_decl ")" statement {  RULE
                            if(!$4.isa<CompoundStmtPtr>()) { driver.error(@4, "catch body must be a compound"); YYABORT; }
                            $$.push_back(driver.builder.catchClause($2, $4.as<CompoundStmtPtr>())); 
                    }
                  | "(" var_decl ")" statement "catch" catch_clause_list{ RULE
                            if(!$4.isa<CompoundStmtPtr>()) { driver.error(@4, "catch body must be a compound"); YYABORT; }
                            $6.insert($6.begin(), driver.builder.catchClause($2, $4.as<CompoundStmtPtr>()));
                            std::swap($$, $6);
                    }
                  ;

decl_stmt : var_decl ";" {RULE
                $$ = driver.builder.declarationStmt($1);
            }
          | var_decl "=" expression {RULE
                $$ = driver.builder.declarationStmt($1, $3);
            }
          | "auto" "identifier" "=" expression {RULE
		        auto var = driver.builder.variable($4.getType());
				annotations::attachName(var, $2);
                driver.add_symb(@$, $2, var);
                $$ = driver.builder.declarationStmt(var, $4);
            }
          ;

var_decl : type "identifier" { RULE
		        $$ = driver.builder.variable($1);
				annotations::attachName( $$, $2);
                driver.add_symb(@$, $2, $$);
            };


for_decl : "(" type "identifier" "=" expression ".." expression ")"  {RULE
		        $$.it = driver.builder.variable($2);
				annotations::attachName( $$.it, $3);
                driver.add_symb(@$, $3, $$.it);
                $$.low = $5;
                $$.up = $7;
                $$.step = driver.builder.literal($2, "1");
           }
         | "(" type "identifier" "=" expression ".." expression ":" expression ")" {RULE
		        $$.it = driver.builder.variable($2);
				annotations::attachName( $$.it, $3);
                driver.add_symb(@$, $3, $$.it);
                $$.low = $5;
                $$.up = $7;
                $$.step = $9;
           }
         ;

switch_case_list : switch_case { RULE $$.push_back($1); }
                 | switch_case switch_case_list { RULE $2.insert($2.begin(), $1); std::swap($$, $2); }
                 ;


switch_case :  "case" expression ":" statement { RULE
                  if(!$2.isa<LiteralPtr>()) { driver.error(@2, "case value must be a literal"); YYABORT; }
                  $$ = driver.builder.switchCase($2.as<LiteralPtr>(), $4); 
               }
            ;

compound_stmt : statement_list { RULE $$ = driver.builder.compoundStmt($1); }

statement_list : statement { RULE $$.push_back($1); }
               | statement statement_list { RULE $2.insert($2.begin(),$1); std::swap($$, $2); }

/* ~~~~~~~~~~~~~~~~~~~  EXPRESSIONS ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

expression : markable_expression  { RULE INSPIRE_GUARD(@1, $1); $$ = $1; } 
           | "$" markable_expression "$" {  RULE INSPIRE_GUARD(@2, $2); $$ = $2; } 

expression_list : expression { RULE $$.push_back($1); }
                | expression "," expression_list  { RULE $3.insert($3.begin(), $1); std::swap($$, $3); }
 
markable_expression : "identifier" { RULE $$ = driver.findSymbol(@$, $1); }
            /* unary */
           | "*" expression { RULE $$ = driver.builder.deref($2); } %prec UDEREF
           | "-" expression { RULE $$ = driver.builder.minus($2); } %prec UMINUS
           | expression "=" expression  { RULE $$ = driver.genBinaryExpression(@$, "=", $1, $3); }
            /* bitwise / logic / arithmetic / geometric */
           | expression "&" expression  { RULE $$ = driver.genBinaryExpression(@$, "&", $1, $3);  }
           | expression "|" expression  { RULE $$ = driver.genBinaryExpression(@$, "|", $1, $3);  }
           | expression "^" expression  { RULE $$ = driver.genBinaryExpression(@$, "^", $1, $3);  }
           | expression "&&" expression { RULE $$ = driver.genBinaryExpression(@$, "&&", $1, $3);  }
           | expression "||" expression { RULE $$ = driver.genBinaryExpression(@$, "||", $1, $3);  }
           | expression "+" expression  { RULE $$ = driver.genBinaryExpression(@$, "+", $1, $3);  }
           | expression "-" expression  { RULE $$ = driver.genBinaryExpression(@$, "-", $1, $3);  }
           | expression "*" expression  { RULE $$ = driver.genBinaryExpression(@$, "*", $1, $3);  }
           | expression "/" expression  { RULE $$ = driver.genBinaryExpression(@$, "/", $1, $3);  }
           | expression "==" expression { RULE $$ = driver.genBinaryExpression(@$, "==", $1, $3);  }
           | expression "!=" expression { RULE $$ = driver.genBinaryExpression(@$, "!=", $1, $3);  }
           | expression "<" expression  { RULE $$ = driver.genBinaryExpression(@$, "<", $1, $3);  }
           | expression ">" expression  { RULE $$ = driver.genBinaryExpression(@$, ">", $1, $3);  }
           | expression "<=" expression { RULE $$ = driver.genBinaryExpression(@$, "<=", $1, $3);  }
           | expression ">=" expression { RULE $$ = driver.genBinaryExpression(@$, ">=", $1, $3);  }
           | expression "[" expression "]" { RULE $$ = driver.genBinaryExpression(@$, "[", $1, $3); }
            /* ternary operator */
           | expression "?" expression ":" expression { RULE
						$$ =  driver.builder.ite($1, driver.builder.wrapLazy($3), driver.builder.wrapLazy($5));
                    }
            /* call expr */
           | expression "(" ")"                 { RULE INSPIRE_TYPE(@1, $1,FunctionTypePtr, "non callable symbol"); 
                                                  $$ = driver.genCall(@$, $1, ExpressionList());  }
           | expression "(" expression_list ")" { RULE INSPIRE_TYPE(@1, $1,FunctionTypePtr, "non callable symbol"); 
                                                  $$ = driver.genCall(@$, $1, $3); }
            /* parenthesis : tuple or expression */
           | "(" expression_list ")"  { RULE  
                                                  if ($2.size() == 1) $$ = $2[0];
                                                  else $$ = driver.builder.tupleExpr($2);
             }
            /* lambda or closure expression: callable expression */
           |  "lambda" lambda_expression  { RULE $$ = $2; }
            /* cast */ 
           | "CAST(" type ")" expression  { RULE $$ = driver.builder.castExpr($2, $4); }
           | expression ".as(" type ")"   { RULE $$  = driver.builder.refParent($1, $3); }
            /* ref mamagement */
           | "undefined" "(" type ")"     { RULE $$ =  driver.builder.undefined( $3 ); }
           | "var" "(" expression ")"     { RULE $$ =  driver.builder.refVar( $3 ); }
           | "new" "(" expression ")"     { RULE $$ =  driver.builder.refNew( $3 ); }
           | "loc" "(" expression ")"     { RULE $$ =  driver.builder.refLoc( $3 ); }
           | "delete" "(" expression ")"  { RULE $$ =  driver.builder.refDelete( $3 ); }
            /* literals */
           | "bool"       { RULE $$ = driver.builder.literal(driver.mgr.getLangBasic().getBool(), $1); }
           | "charlit"    { RULE $$ = driver.builder.literal(driver.mgr.getLangBasic().getChar(), $1); }
           | "int"        { RULE $$ = driver.builder.literal(driver.mgr.getLangBasic().getInt4(), $1); }
           | "uint"       { RULE $$ = driver.builder.literal(driver.mgr.getLangBasic().getUInt4(), $1); }
           | "long"       { RULE $$ = driver.builder.literal(driver.mgr.getLangBasic().getInt8(), $1); }
           | "ulong"      { RULE $$ = driver.builder.literal(driver.mgr.getLangBasic().getUInt8(), $1); }
           | "float"      { RULE $$ = driver.builder.literal(driver.mgr.getLangBasic().getReal4(), $1); }
           | "double"     { RULE $$ = driver.builder.literal(driver.mgr.getLangBasic().getReal8(), $1); }
           | "stringlit"  { RULE $$ = driver.builder.stringLit($1); }
            /* constructed literals */
           | "type(" type ")"         { RULE $$ = driver.builder.getTypeLiteral($2); }
           | "param(" "paramvar" ")"  { RULE $$ = driver.builder.getIntTypeParamLiteral(driver.find_type_param_var(@2, $2)); }
           | "param(" "int" ")"       { RULE $$ = driver.builder.getIntTypeParamLiteral(driver.builder.concreteIntTypeParam(utils::numeric_cast<uint32_t>($2))); }
           | "lit(" "stringlit" ")"          { RULE $$ = driver.builder.getIdentifierLiteral($2); }
           | "lit(" "stringlit" ":" type ")" { RULE $$ = driver.builder.literal($4, $2); }
           | "lit(" type ")"                 { RULE $$ = driver.builder.getTypeLiteral($2); }
            /* struct / union expressions */
           | "struct" type "{" assign_rules { RULE $$ = driver.genTagExpression(@$, $2, $4); }
           | "struct" "{" assign_rules      { RULE $$ = driver.genTagExpression(@$, $3); }
            /* async */
           | "spawn" expression { RULE $$ = driver.builder.parallel($2);  }
           | "sync" expression  { RULE $$ = driver.builder.callExpr(driver.builder.getLangBasic().getUnit(), 
                                                                    driver.builder.getLangBasic().getMerge(), $2);
                                }
           | "syncAll" { RULE 
                    $$ = driver.builder.callExpr(driver.builder.getLangBasic().getUnit(), driver.builder.getLangBasic().getMergeAll()); 
                }
           ;

assign_field : "identifier" "=" expression { RULE $$ = driver.builder.namedValue($1, $3); }

assign_rules : "}" { }
             | assign_field "}" { RULE $$.insert($$.begin(), $1); }
             | assign_field "," assign_rules { RULE $3.insert($3.begin(), $1); std::swap($$, $3); }

        /* TODO:  lambdas require a type, closures dont, this makes no sense, find a uniform way to do it */
lambda_expression_aux : "(" ")" "=>" expression {   RULE  
                            $$ = driver.genClosure(@1, VariableList(), $4);
                            if (driver.in_let) driver.add_let_closure(@$, $$);
                        } 
                      | "(" ")" "=>" "{" "}" {   RULE  
                            $$ = driver.genClosure(@1, VariableList(), driver.builder.compoundStmt());
                            if (driver.in_let) driver.add_let_closure(@$, $$);
                        } 
                      | "(" ")" "=>" "{" compound_stmt "}" {   RULE  
                            $$ = driver.genClosure(@1, VariableList(), $5);
                            if (driver.in_let) driver.add_let_closure(@$, $$);
                        } 
                      | "(" variable_list ")" "=>" expression {  RULE   
                            $$ = driver.genClosure(@1, $2, $5);
                            if (driver.in_let) driver.add_let_closure(@$, $$);
                        } 
                      | "(" variable_list ")" "=>" "{" "}" {   RULE  
                            $$ = driver.genClosure(@1, $2, driver.builder.compoundStmt());
                            if (driver.in_let) driver.add_let_closure(@$, $$);
                        } 
                      | "(" variable_list ")" "=>" "{" compound_stmt "}" { RULE    
                            $$ = driver.genClosure(@1, $2, $6);
                            if (driver.in_let) driver.add_let_closure(@$, $$);
                        } 
                      | "(" ")" "->" type ":" { } expression {  RULE   
                                INSPIRE_NOT_IMPLEMENTED(@$);
                           $$ = driver.genLambda(@$, VariableList(), $4, driver.builder.compoundStmt(driver.builder.returnStmt($7))); 
                        } 
                      | "(" ")" "->" type "{" { } "}" {   RULE  
                            if (driver.in_let){
                                driver.add_let_lambda(@$, @1, @7, $4);
                                driver.set_inhibit(false);
                            }
                            else{
                                $$ = driver.genLambda(@$, VariableList(), $4, driver.builder.compoundStmt()); 
                            }
                        } 
                      | "(" ")" "->" type "{"  { driver.set_inhibit(driver.in_let); } compound_stmt "}" { 
                            if (driver.in_let){
                                driver.add_let_lambda(@$, @1, @8, $4);
                                driver.set_inhibit(false);
                            }
                            else{
                                $$ = driver.genLambda(@$, VariableList(), $4, $7); 
                            }
                        } 
                      | "(" variable_list ")" "->" type ":" { driver.set_inhibit(driver.in_let); } expression {     
                            if (driver.in_let){
                                driver.add_let_lambda(@$, @1, @8, $5, $2);
                                driver.set_inhibit(false);
                            }
                            else{
                                $$ = driver.genLambda(@$, $2, $5, driver.builder.compoundStmt(driver.builder.returnStmt($8))); 
                            }
                        } 
                      | "(" variable_list ")" "->" type "{" { driver.set_inhibit(driver.in_let);} "}" {     
                            if (driver.in_let){
                                driver.add_let_lambda(@$, @1, @8, $5, $2);
                                driver.set_inhibit(false);
                            }
                            else{
                                $$ = driver.genLambda(@$, $2, $5, driver.builder.compoundStmt());
                            }
                        } 
                      | "(" variable_list ")" "->" type "{" { driver.set_inhibit(driver.in_let);} compound_stmt "}" { 
                            if (driver.in_let){
                                driver.add_let_lambda(@$, @1, @9, $5, $2);
                                driver.set_inhibit(false);
                            }
                            else{
                                  $$ = driver.genLambda(@$, $2, $5, $8); 
                            }
                        } 
                     ;

lambda_expression : { RULE driver.open_scope(@$,"lambda expr"); } lambda_expression_aux { RULE driver.close_scope(@$, "lambda expr"); $$ = $2; }
                  ;

%nonassoc "::" ;
%nonassoc ")";
%nonassoc "else";
%right "=>";
%left "spawn" "sync" "syncAll";
%right "->";
%right "?";
%right ":";
%right "[";
%right "catch";
%left "=";
%left ".cast(";
%left ".as(";
%left "==" "!=" "<" "<=" ">" ">=";
%left "+" "-";
%left "*" "/";
%left "&&" "||";
%left "&" "|" "^";
%right "(";
%nonassoc UMINUS;
%nonassoc UDEREF;
%nonassoc BOOL_OP;

%%
// code after the second %% is copyed verbatim at the end of the parser .cpp file

namespace insieme{
namespace core{
namespace parser3{
namespace detail{

    using namespace insieme::core;

    void inspire_parser::error (const location_type& l, const std::string& m) {
      driver.error (l, m);
    } 


} // detail
} // parser3
} // core
} // insieme



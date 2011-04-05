/**
 * Copyright (c) 2002-2013 Distributed and Parallel Systems Group,
 *                Institute of Computer Science,
 *               University of Innsbruck, Austria
 *
 * This file is part of the INSIEME Compiler and Runtime System.
 *
 * We provide the software of this file (below described as "INSIEME")
 * under GPL Version 3.0 on an AS IS basis, and do not warrant its
 * validity or performance.  We reserve the right to update, modify,
 * or discontinue this software at any time.  We shall have no
 * obligation to supply such updates or modifications or any other
 * form of support to you.
 *
 * If you require different license terms for your intended use of the
 * software, e.g. for proprietary commercial or industrial use, please
 * contact us at:
 *                   insieme@dps.uibk.ac.at
 *
 * We kindly ask you to acknowledge the use of this software in any
 * publication or other disclosure of results by referring to the
 * following citation:
 *
 * H. Jordan, P. Thoman, J. Durillo, S. Pellegrini, P. Gschwandtner,
 * T. Fahringer, H. Moritsch. A Multi-Objective Auto-Tuning Framework
 * for Parallel Codes, in Proc. of the Intl. Conference for High
 * Performance Computing, Networking, Storage and Analysis (SC 2012),
 * IEEE Computer Society Press, Nov. 2012, Salt Lake City, USA.
 *
 * All copyright notices must be kept intact.
 *
 * INSIEME depends on several third party software packages. Please 
 * refer to http://www.dps.uibk.ac.at/insieme/license.html for details 
 * regarding third party software licenses.
 */

#pragma once

#include "insieme/core/parser/ir_parse.h"

#include "insieme/utils/map_utils.h"

namespace insieme {
namespace core {
namespace parse {

typedef std::vector<std::pair<ExpressionPtr, ExpressionPtr> > GuardedStmts;
typedef vector<VariablePtr> VariableList;
typedef vector<ExpressionPtr> ExpressionList;
typedef std::map<ExpressionPtr, LambdaPtr, compare_target<ExpressionPtr> > Defs;
typedef std::vector<std::pair<IdentifierPtr, ExpressionPtr> > Members;
#define Rule qi::rule<ParseIt, T(), qi::space_type>

// FW Declaration
template<typename V> struct TypeGrammar;
template<typename T> struct ExpressionGrammarPart;
template<typename T> struct StatementGrammar;
template<typename T> struct OperatorGrammar;

// helper function to be able to use std::make_pair along with ph::push_back
template<typename T, typename U>
std::pair<T, U> makePair (T first, U second) {
    return std::make_pair(first, second);
}

/* moved to ir_parse.h
class VariableTable {
    NodeManager& nodeMan;
    utils::map::PointerMap<IdentifierPtr, VariablePtr> table;

public:
    VariableTable(NodeManager& nodeMan) : nodeMan(nodeMan) { }

    VariablePtr get(const TypePtr& typ, const IdentifierPtr& id);
    VariablePtr lookup(const IdentifierPtr& id);
};
*/

template<typename T>
struct ExpressionGrammar : public qi::grammar<ParseIt, T(), qi::space_type> {
    TypeGrammar<TypePtr> *typeG; // pointer for weak coupling
    ExpressionGrammarPart<T> *exprGpart;
    StatementGrammar<StatementPtr>* stmtG;
    OperatorGrammar<T>* opG;
    VariableTable varTab;
    NodeManager& nodeMan;
    bool deleteStmtG;

    ExpressionGrammar(NodeManager& nodeMan, StatementGrammar<StatementPtr>* stmtGrammar = NULL);
    ~ExpressionGrammar();

    // terminal rules, no skip parsing
    qi::rule<ParseIt, string()> literalString;

    // nonterminal rules with skip parsing
    qi::rule<ParseIt, T(), qi::space_type> literalExpr;
    qi::rule<ParseIt, T(), qi::space_type> opExpr;
    qi::rule<ParseIt, T(), qi::space_type> variableExpr;
    qi::rule<ParseIt, T(), qi::space_type> funVarExpr;

    qi::rule<ParseIt, T(), qi::locals<ExpressionList>, qi::space_type> callExpr;
    qi::rule<ParseIt, T(), qi::space_type> castExpr;

    qi::rule<ParseIt, T(), qi::space_type> expressionRule;

    // literals -----------------------------------------------------------------------------
    qi::rule<ParseIt, T(), qi::space_type> charLiteral;

    // --------------------------------------------------------------------------------------
    qi::rule<ParseIt, LambdaPtr(), qi::locals<ExpressionList>, qi::space_type> lambda;
    qi::rule<ParseIt, LambdaDefinitionPtr(), qi::locals<vector<ExpressionPtr>, vector<LambdaPtr> >, qi::space_type> lambdaDef;
    qi::rule<ParseIt, T(), qi::space_type> lambdaExpr;

    qi::rule<ParseIt, T(), qi::space_type> bindExpr;

    qi::rule<ParseIt, T(), qi::locals<vector<StatementPtr>, GuardedStmts>, qi::space_type> jobExpr;
    qi::rule<ParseIt, T(), qi::space_type> tupleExpr;
    qi::rule<ParseIt, T(), qi::space_type> vectorExpr;
    qi::rule<ParseIt, T(), qi::space_type> structExpr;
    qi::rule<ParseIt, T(), qi::space_type> unionExpr;

    qi::rule<ParseIt, T(), qi::space_type> memberAccessExpr;
    qi::rule<ParseIt, T(), qi::space_type> tupleProjectionExpr;
    qi::rule<ParseIt, T(), qi::space_type> markerExpr;

    qi::rule<ParseIt, T(), qi::space_type> intExpr;
    qi::rule<ParseIt, T(), qi::space_type> doubleExpr;
    qi::rule<ParseIt, T(), qi::space_type> boolExpr;

    // --------------------------------------------------------------------------------------

    // member functions applying the rules
    virtual qi::rule<ParseIt, string()> getLiteralString();
    virtual qi::rule<ParseIt, T(), qi::locals<ExpressionList>, qi::space_type> getCallExpr();
    virtual qi::rule<ParseIt, LambdaPtr(), qi::locals<ExpressionList>, qi::space_type> getLambda();
    virtual qi::rule<ParseIt, LambdaDefinitionPtr(), qi::locals<vector<ExpressionPtr>, vector<LambdaPtr> >, qi::space_type> getLambdaDef();
    virtual qi::rule<ParseIt, T(), qi::locals<vector<StatementPtr>, GuardedStmts>, qi::space_type> getJobExpr();
    #define get(op) virtual Rule get##op ();
    get(LiteralExpr)
    get(CharLiteral)
    get(OpExpr)
    get(VariableExpr)
    get(FunVarExpr)
    get(CastExpr)
    get(BindExpr)
    get(LambdaExpr)
    get(ExpressionRule)
    get(TupleExpr)
    get(VectorExpr)
    get(StructExpr)
    get(UnionExpr)
    get(MemberAccessExpr)
    get(TupleProjectionExpr)
    get(MarkerExpr)
    get(IntExpr)
    get(DoubleExpr)
    get(BoolExpr)
    #undef get

private:
    // member functions providing the rules
    virtual T doubleLiteralHelp(int integer, vector<char> fraction);
    virtual T intLiteralHelp(int val);
    virtual LambdaPtr lambdaHelp(const TypePtr& retType, const ExpressionList& paramsExpr, const StatementPtr& body);
    virtual LambdaDefinitionPtr lambdaDefHelp(const ExpressionList& funVarExpr, vector<LambdaPtr>& lambdaExpr );
    virtual T lambdaExprHelp(T& variableExpr, LambdaDefinitionPtr& def);
    virtual T lambdaExprHelp(LambdaPtr& lambda);
    virtual T jobExprHelp(const T& threadNumRange, const T& defaultStmt, const GuardedStmts guardedStmts, const vector<StatementPtr>& localDeclStmts);
    virtual T callExprHelp(const T& callee, ExpressionList& arguments);
    virtual T boolLiteralHelp(const bool flag);
};

} // namespace parse
} // namespace core
} // namespace insieme

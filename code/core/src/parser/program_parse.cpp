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

#include "insieme/core/parser/statement_parse.h"
#include "insieme/core/parser/expression_parse.h"
#include "insieme/core/parser/program_parse.h"
#include "insieme/core/parser/type_parse.h"
#include "insieme/core/lang/basic.h"
#include "insieme/core/ast_builder.h"

#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/support_ascii.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>

namespace insieme {
namespace core {
namespace parse {

namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;
namespace ph = boost::phoenix;

template <typename ProgramPtr, class ExpressionPtr, class StatementPtr, class TypePtr, class IntTypeParamPtr, class IdentifierPtr, class LambdaPtr,
class LambdaDefinitionPtr>
ProgramPtr ProgramGrammar<ProgramPtr, ExpressionPtr, StatementPtr, TypePtr, IntTypeParamPtr, IdentifierPtr, LambdaPtr, LambdaDefinitionPtr>::
mainProgramHelp(const ExpressionPtr& mainProg) {
    return Program::create(nodeMan, toVector(mainProg), true);
}

template <typename ProgramPtr, class ExpressionPtr, class StatementPtr, class TypePtr, class IntTypeParamPtr, class IdentifierPtr, class LambdaPtr,
class LambdaDefinitionPtr>
ProgramPtr ProgramGrammar<ProgramPtr, ExpressionPtr, StatementPtr, TypePtr, IntTypeParamPtr, IdentifierPtr, LambdaPtr, LambdaDefinitionPtr>::
programHelp(const vector<ExpressionPtr>& progs) {
    return Program::create(nodeMan, progs, false);
}

template <typename P, typename T, typename U, typename V, typename W, typename X, typename Y,  typename Z>
qi::rule<ParseIt, P(), qi::locals<vector<T> >, qi::space_type> ProgramGrammar<P, T, U, V, W, X, Y, Z>::getProgram() {
    return ( qi::lit("main") >> ':' >> exprG->expressionRule )      [ qi::_val = ph::bind(&ProgramGrammar<P, T, U, V, W, X, Y, Z>::mainProgramHelp, this, qi::_1) ]
        | ( *exprG->expressionRule                                  [ ph::push_back(qi::_a, qi::_1) ]
          )                                                         [ qi::_val = ph::bind(&ProgramGrammar<P, T, U, V, W, X, Y, Z>::programHelp, this, qi::_a) ];
}

template <typename P, typename T, typename U, typename V, typename W, typename X, typename Y,  typename Z>
qi::rule<ParseIt, P(), qi::space_type> ProgramGrammar<P, T, U, V, W, X, Y, Z>::getProgramRule() {
    return program                                                  [ qi::_val = ph::construct<ProgramPtr>(qi::_1) ];
}

template <typename P, typename T, typename U, typename V, typename W, typename X, typename Y,  typename Z>
ProgramGrammar<P, T, U, V, W, X, Y, Z>::ProgramGrammar(NodeManager& nMan) : ProgramGrammar::base_type(programRule),
        exprG(new ExpressionGrammar<T, U, V, W, X, Y, Z>(nMan)), nodeMan(nMan) {

    program = getProgram();

    programRule = getProgramRule();

//    BOOST_SPIRIT_DEBUG_NODE(programRule);
}

template <typename P, typename T, typename U, typename V, typename W, typename X, typename Y,  typename Z>
ProgramGrammar<P, T, U, V, W, X, Y, Z>::~ProgramGrammar() {
    delete exprG;
}

// explicit template instantiation
template struct ProgramGrammar<ProgramPtr, ExpressionPtr, StatementPtr, TypePtr, IntTypeParamPtr, IdentifierPtr, LambdaPtr, LambdaDefinitionPtr>;



template <typename P, typename T, typename U, typename V, typename W, typename X, typename Y,  typename Z>
IRGrammar<P, T, U, V, W, X, Y, Z>::IRGrammar(NodeManager& nMan) : IRGrammar::base_type(irRule),
	typeG(new TypeGrammar<V, W, X>(nMan)),
	progG(new ProgramGrammar<P, T, U, V, W, X, Y, Z>(nMan)),
	stmtG(new StatementGrammar<U, T, V, W, X, Y, Z>(nMan, NULL, NULL)),
	nodeMan(nMan) {

	mainProg = ( qi::lit("main") >> ':' >> stmtG->exprG->expressionRule )    [ qi::_val = ph::bind(&ProgramGrammar<P, T, U, V, W, X, Y, Z>::mainProgramHelp, progG, qi::_1) ];

	irRule = getIRRule();


//    BOOST_SPIRIT_DEBUG_NODE(programRule);
}
template <typename P, typename T, typename U, typename V, typename W, typename X, typename Y,  typename Z>
IRGrammar<P, T, U, V, W, X, Y, Z>::~IRGrammar() {
    delete typeG;
    delete progG;
    delete stmtG;
}


template<typename P, typename T, typename U, typename V, typename W, typename X, typename Y,  typename Z>
qi::rule<ParseIt, NodePtr(), qi::space_type> IRGrammar<P, T, U, V, W, X, Y, Z>::getIRRule() {
    return  mainProg                                                         [ qi::_val = ph::construct<P>(qi::_1) ]
    	  | typeG->typeRule                                                  [ qi::_val = ph::construct<V>(qi::_1) ]
		  | stmtG->statementRule                                             [ qi::_val = ph::construct<U>(qi::_1) ];

}


// explicit template instantiation
template struct IRGrammar<ProgramPtr, ExpressionPtr, StatementPtr, TypePtr, IntTypeParamPtr, IdentifierPtr, LambdaPtr, LambdaDefinitionPtr>;

}
}
}

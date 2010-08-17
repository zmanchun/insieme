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

#include "statements.h"
#include "expressions.h"

#include "iterator_utils.h"


// ------------------------------------- Statement ---------------------------------

std::size_t hash_value(const Statement& stmt) {
	return stmt.hash();
}

bool Statement::operator==(const Statement& stmt) const {
	return (typeid(*this) == typeid(stmt)) && (hash() == stmt.hash()) && equals(stmt);
}

Statement::ChildList Statement::getChildren() const {
	return makeChildList();
}


// ------------------------------------- NoOpStmt ---------------------------------

string NoOpStmt::toString() const {
	return "{ /* NoOp */ };";
}

bool NoOpStmt::equals(const Statement&) const {
	return true;
}

std::size_t NoOpStmt::hash() const {
	return HASHVAL_NOOP;
}

NoOpStmt* NoOpStmt::clone() const {
	return new NoOpStmt();
}

NoOpStmtPtr NoOpStmt::get(StatementManager& manager) {
	return manager.getStmtPtr(NoOpStmt());
}

// ------------------------------------- BreakStmt ---------------------------------

string BreakStmt::toString() const {
	return "break;";
}

bool BreakStmt::equals(const Statement&) const {
	return true;
}

std::size_t BreakStmt::hash() const {
	return HASHVAL_BREAK;
}

BreakStmt* BreakStmt::clone() const {
	return new BreakStmt();
}

BreakStmtPtr BreakStmt::get(StatementManager& manager) {
	return manager.getStmtPtr(BreakStmt());
}

// ------------------------------------- ContinueStmt ---------------------------------

string ContinueStmt::toString() const {
	return "continue;";
}

bool ContinueStmt::equals(const Statement&) const {
	return true;
}

std::size_t ContinueStmt::hash() const {
	return HASHVAL_CONTINUE;
}

ContinueStmt* ContinueStmt::clone() const {
	return new ContinueStmt();
}

ContinueStmtPtr ContinueStmt::get(StatementManager& manager) {
	return manager.getStmtPtr(ContinueStmt());
}

// ------------------------------------- DeclarationStmt ---------------------------------

DeclarationStmt::DeclarationStmt(const TypePtr& type, const Identifier& id, const ExprPtr& initExpression) 
	: type(type), id(id), initExpression(initExpression) { 
}

string DeclarationStmt::toString() const {
	return format("%s %s;", type->toString().c_str(), initExpression->toString().c_str());
}

bool DeclarationStmt::equals(const Statement& stmt) const {
	// conversion is guaranteed by base operator==
	const DeclarationStmt& rhs = dynamic_cast<const DeclarationStmt&>(stmt); 
	return (type == rhs.type) && (initExpression == rhs.initExpression);
}

std::size_t DeclarationStmt::hash() const {
	std::size_t seed = HASHVAL_DECLARATION;
    boost::hash_combine(seed, type->hash());
    boost::hash_combine(seed, initExpression->hash());
	return seed;
}

DeclarationStmt* DeclarationStmt::clone() const {
	return new DeclarationStmt(type, id, initExpression);
}

DeclarationStmt::ChildList DeclarationStmt::getChildren() const {
	return makeChildList(initExpression);
}

DeclarationStmtPtr DeclarationStmt::get(StatementManager& manager, const TypePtr& type, const Identifier& id, const ExprPtr& initExpression) {

	TypePtr localType = manager.getTypeManager().getPointer(type);
	ExprPtr localExpr = manager.getStmtPtr(*initExpression);

	return manager.getStmtPtr(DeclarationStmt(localType, id, localExpr));
}

// ------------------------------------- ReturnStmt ---------------------------------

ReturnStmt::ReturnStmt(const ExprPtr& returnExpression)
	: returnExpression(returnExpression) { 
}

string ReturnStmt::toString() const {
	return format("return %s;", returnExpression->toString().c_str());
}

bool ReturnStmt::equals(const Statement& stmt) const {
	// conversion is guaranteed by base operator==
	const ReturnStmt& rhs = dynamic_cast<const ReturnStmt&>(stmt); 
	return (returnExpression == rhs.returnExpression);
}

std::size_t ReturnStmt::hash() const {
	std::size_t seed = HASHVAL_RETURN;
    boost::hash_combine(seed, returnExpression->hash());
	return seed;
}

ReturnStmt* ReturnStmt::clone() const {
	return new ReturnStmt(returnExpression);
}

ReturnStmt::ChildList ReturnStmt::getChildren() const {
	return makeChildList(returnExpression);
}

ReturnStmtPtr ReturnStmt::get(StatementManager& manager, const ExprPtr& returnExpression) {
	return manager.getStmtPtr(ReturnStmt(returnExpression));
}

// ------------------------------------- CompoundStmt ---------------------------------

string CompoundStmt::toString() const {
	vector<string> list;
	std::transform(statements.cbegin(), statements.cend(), back_inserter(list), [](const StmtPtr& cur) {return cur->toString();});
	return format("{\n%s\n}", boost::join(list, "\n").c_str());
}

bool CompoundStmt::equals(const Statement& stmt) const {
	// conversion is guaranteed by base operator==
	const CompoundStmt& rhs = dynamic_cast<const CompoundStmt&>(stmt);
	auto start = make_paired_iterator(statements.begin(), rhs.statements.begin());
	auto end = make_paired_iterator(statements.end(), rhs.statements.end());
	return all(start, end, [](std::pair<StmtPtr,StmtPtr> elems) { return elems.first == elems.second; } );
}

std::size_t CompoundStmt::hash() const {
	std::size_t seed = HASHVAL_COMPOUND;
	std::for_each(statements.begin(), statements.end(), [&seed](StmtPtr cur) { 
		boost::hash_combine(seed, cur->hash());
	} );
	return seed;
}

CompoundStmt* CompoundStmt::clone() const {
	return new CompoundStmt(statements);
}

CompoundStmt::ChildList CompoundStmt::getChildren() const {
	return makeChildList(statements);
}

const StmtPtr&  CompoundStmt::operator[](unsigned index) const {
	return statements[index];
}

CompoundStmtPtr CompoundStmt::get(StatementManager& manager) {
	return manager.getStmtPtr(CompoundStmt());
}
CompoundStmtPtr CompoundStmt::get(StatementManager& manager, const StmtPtr& stmt) {
	return manager.getStmtPtr(CompoundStmt(stmt));
}
CompoundStmtPtr CompoundStmt::get(StatementManager& manager, const vector<StmtPtr>& stmts) {
	return manager.getStmtPtr(CompoundStmt(stmts));
}

// ------------------------------------- WhileStmt ---------------------------------

WhileStmt::WhileStmt(ExprPtr condition, StmtPtr body)
	: condition(condition), body(body) {	
}

string WhileStmt::toString() const {
	return format("while(%s)\n%s", condition->toString().c_str(), body->toString().c_str());
}
	
bool WhileStmt::equals(const Statement& stmt) const {
	// conversion is guaranteed by base operator==
	const WhileStmt& rhs = dynamic_cast<const WhileStmt&>(stmt);
	return (condition == rhs.condition) && (body == rhs.body);
}

std::size_t WhileStmt::hash() const {
	std::size_t seed = HASHVAL_WHILE;
	boost::hash_combine(seed, condition->hash());
	boost::hash_combine(seed, body->hash());
	return seed;
}

WhileStmt* WhileStmt::clone() const {
	return new WhileStmt(condition, body);
}

WhileStmt::ChildList WhileStmt::getChildren() const {
	auto ret = makeChildList(condition);
	ret->push_back(body);
	return ret;
}

WhileStmtPtr WhileStmt::get(StatementManager& manager, ExprPtr condition, StmtPtr body) {
	return manager.getStmtPtr(WhileStmt(condition, body));
}

// ------------------------------------- ForStmt ---------------------------------

//ForStmt(DeclarationStmtPtr declaration, StmtPtr body, ExprPtr end, ExprPtr step) :
//	declaration(declaration), body(body), end(end), step(step) {
//}
//
//	
//string ForStmt::toString() const {
//	return string("for(") + variable->toString() + "=" + start->toString() + ".." + end->toString() + ":"
//			+ step->toString() + ")\n" + body->toString();
//}

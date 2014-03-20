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

#include "insieme/frontend/analysis/loop_analyzer.h"
#include "insieme/frontend/convert.h"

#include "insieme/core/ir_visitor.h"
#include "insieme/core/analysis/ir_utils.h"
#include "insieme/core/arithmetic/arithmetic.h"
#include "insieme/core/arithmetic/arithmetic_utils.h"
#include "insieme/core/transform/node_replacer.h"
#include "insieme/core/types/subtyping.h"
#include "insieme/annotations/c/include.h"
#include "insieme/frontend/utils/cast_tool.h"

#include "insieme/frontend/utils/clang_utils.h"
#include "insieme/frontend/utils/ir_cast.h"

#include <clang/AST/Expr.h>
#include <clang/AST/Stmt.h>
#include <clang/AST/StmtVisitor.h>

using namespace clang;

namespace {

using namespace insieme;

struct InitializationCollector : public core::IRVisitor<bool,core::Address>{
	
	const core::ExpressionPtr& inductionExpr;
	core::ExpressionPtr init;
	core::StatementList leftoverStmts;
	bool isDecl;

	InitializationCollector(const core::ExpressionPtr& ind)
		: core::IRVisitor<bool,core::Address> (), inductionExpr(ind), isDecl(false)
	{
		assert(inductionExpr->getType().isa<core::RefTypePtr>() && "looking for an initialization, has to be writable");
	}

	bool visitStatement(const core::StatementAddress& stmt){

		auto& mgr(stmt->getNodeManager());

		if (stmt.isa<core::CallExprAddress>()){
			if (core::analysis::isCallOf(stmt.as<core::CallExprPtr>(), mgr.getLangBasic().getRefAssign())){
					// if there is comma (,) operator, we will find the assignments enclosed into a labmda, we need to translate the
					// variable names
				core::ExpressionPtr left = stmt.as<core::CallExprPtr>()[0].as<core::ExpressionPtr>();
				if (stmt.as<core::CallExprPtr>()[0].isa<core::VariablePtr>()){
					core::VariableAddress var  = stmt.as<core::CallExprAddress>()[0].as<core::VariableAddress>();
					utils::map::PointerMap<core::VariableAddress, core::VariableAddress> paramName = 
						core::analysis::getRenamedVariableMap(toVector(var));
					left = paramName[var];
				}
				core::ExpressionPtr right = stmt.as<core::CallExprPtr>()[1].as<core::ExpressionPtr>();

				if (left == inductionExpr){
					init = right;
				}
				else {
					core::IRBuilder builder( stmt->getNodeManager() );
					leftoverStmts.push_back( builder.assign (left, right));
				}
				return true;
			}
		}
		return false;
	}

	bool visitDeclarationStmt(const core::DeclarationStmtAddress& declAdr){
		core::DeclarationStmtPtr decl = declAdr.as<core::DeclarationStmtPtr>();
		if (core::VariablePtr var = inductionExpr.isa<core::VariablePtr>()){
				// the initialization must be wrapped into a refvar or something, so we get pure value
			if (decl->getVariable() == var){
				init = decl->getInitialization().as<core::CallExprPtr>()[0];
				isDecl = true;
				return true;
			}
		}
		leftoverStmts.push_back(decl);
		return true;
	}
};

}

namespace insieme {
namespace frontend {
namespace analysis {


LoopAnalyzer::LoopAnalyzer(const clang::ForStmt* forStmt, Converter& convFact): 
	convFact(convFact),
	loopToBoundary (false),
	restoreValue (false){
	
	if ( !forStmt->getInc() )   throw LoopNormalizationError(" no increment expression in loop");
	if ( !forStmt->getCond() )  throw LoopNormalizationError(" no condition expression in loop");

	// we look for the induction variable
	findInductionVariable(forStmt);
	// we know the induction variable, we analyze the increment expression
	handleIncrExpr(forStmt);
	// we look for the condition expression
	handleCondExpr(forStmt);

	// it seems that we can not normalize the thing... just write the expression OLD SCHOOL!!! but only if are pointers
	const core::IRBuilder& builder = convFact.getIRBuilder();
	insieme::core::NodeManager& mgr = convFact.getNodeManager();
	if (frontend::utils::isRefArray(inductionVar->getType())
			|| frontend::utils::isRefArray(endValue->getType())
			|| frontend::utils::isRefArray(initValue->getType())) {
		throw LoopNormalizationError(" pointer for loop not supported yet!"); 
	} else if (!mgr.getLangBasic().isInt(inductionVar->getType())) {
		throw LoopNormalizationError(" iterator for for-loop has to be of integraltype!"); 
	}
	else{
		auto one =  builder.literal("1", inductionVar->getType());

		// if we need to invert the loop and the variable type was an unsigned, change to signed
		if(invertComparisonOp) {
			using namespace insieme::core;
			TypePtr currentType = inductionVar->getType();
			if(mgr.getLangBasic().isUnsignedInt(currentType)) {
				TypePtr newType = builder.genericType("int", TypeList(), toVector(currentType.as<insieme::core::GenericTypePtr>().getIntTypeParameter()->getElement(0)));
				TypePtr commonType = types::getSmallestCommonSuperType(newType, currentType);

				/*
				inductionVar = convFact.getIRBuilder().variable(commonType, inductionVar->getId());
				*/
				if(mgr.getLangBasic().isIntInf(commonType) ) {
					//HACK: problem if we want to get a bigger type than int<8>
					//FIXME find better way than to drop to int<8>
					inductionVar = convFact.getIRBuilder().variable(newType, inductionVar->getId());
					//throw LoopNormalizationError("trying to move from unsigned long to long!"); 
				} else { 
					inductionVar = convFact.getIRBuilder().variable(commonType, inductionVar->getId());
				}

				// if we change the type of the var, we also need to change the type of the literal to make the semantic checker happy
				if(initValue.isa<LiteralPtr>()) {
					LiteralPtr literal = initValue.as<LiteralPtr>();
					int value = literal->getValueAs<int>();
					initValue = convFact.getIRBuilder().intLit(value);
				}
			}
		}

		endValue = frontend::utils::castScalar(inductionVar->getType(), endValue);

		newInductionExpr = inductionVar.as<insieme::core::ExpressionPtr>();  // philgs uncomment

		// if the variable is declared outside, we must give it a final value after all iterations
		if(restoreValue) {
			core::ExpressionPtr tmpEndValue;
			if(loopToBoundary) {
				if(invertComparisonOp)
					tmpEndValue = builder.sub(endValue, one);
				else
					tmpEndValue = builder.add(endValue, one);
			} else
				tmpEndValue = endValue;

			core::StatementPtr assign = builder.assign(originalInductionExpr.as<core::CallExprPtr>()[0], tmpEndValue);
			// if the induction variable is not scope defined, and there is actualy some init value assigned, we should
			// update this variable so the inner loop side effects have access to it
			postStmts.push_back(assign);
			if ( initValue != originalInductionExpr) {
				assign = builder.assign (originalInductionExpr.as<core::CallExprPtr>()[0], (invertComparisonOp) ? convFact.getIRBuilder().invertSign(newInductionExpr) : inductionVar);
				firstStmts.push_back(assign);
			}
		}

		// if the comparison operator was not < or <=, we need to invert to comply with IR loops that have an implicit <
		if(invertComparisonOp) {
			newInductionExpr = convFact.getIRBuilder().invertSign(newInductionExpr);
			stepExpr = convFact.getIRBuilder().invertSign( stepExpr);
			initValue = convFact.getIRBuilder().invertSign( initValue );
			endValue = convFact.getIRBuilder().invertSign( endValue );
		}

		// if the loop iterations include the boundary case, extend range + 1 to comply with IR loops
		if(loopToBoundary)
			endValue = convFact.getIRBuilder().add(endValue, convFact.getIRBuilder().literal("1", inductionVar->getType()));
	}
}

// to identify the induction variable, we cross the expressions in the increment with the expressions in the condition
// if there is a single expression, that is our induction expression
// 		is an expression because it can be a variable or a member access
//
void LoopAnalyzer::findInductionVariable(const clang::ForStmt* forStmt) {

	// convert to IR everything in condition and increment
	core::ExpressionPtr condition = convFact.convertExpr (forStmt->getCond());
	core::ExpressionPtr incrementExpr;

	// we start looking in the increment expression, if there is no increment we can not generate a for loop
	const clang::BinaryOperator* binOp = llvm::dyn_cast<clang::BinaryOperator>( forStmt->getInc());
	const clang::UnaryOperator* unOp = llvm::dyn_cast<clang::UnaryOperator>( forStmt->getInc());
	if (binOp){
		if(binOp->getOpcode() == clang::BO_Comma ) throw LoopNormalizationError("more than one increment expression in loop"); // TODO: we cold support this
		if(binOp->getOpcode() != clang::BO_AddAssign && binOp->getOpcode() != clang::BO_SubAssign)
												   throw LoopNormalizationError("operation not supported for increment expression");
		// left side is our variable
		incrementExpr = convFact.convertExpr(binOp->getLHS());
	}
	else if(unOp) {
		// this has to be the one
		incrementExpr = convFact.convertExpr(unOp->getSubExpr());
	}
	else{
		throw LoopNormalizationError("malformed increment expression for for loop");
	}

	// we cross this expression with the ones evaluated in the condition
	// since the increment modifies the value of the induction var, it should be a ref, in the condition we check
	// the value, so it has to be deref
	const clang::Expr* cond = forStmt->getCond();
	if( const BinaryOperator* binOp = dyn_cast<const BinaryOperator>(cond) ) {
		core::ExpressionPtr left  = convFact.convertExpr(binOp->getLHS());
		core::ExpressionPtr right = convFact.convertExpr(binOp->getRHS());
		if (!incrementExpr->getType().isa<core::RefTypePtr>()) throw LoopNormalizationError("unhandled induction variable type");
		core::ExpressionPtr value = convFact.getIRBuilder().deref(incrementExpr);
			
		bool isRight = false;
		bool isLeft = false;
		core::visitDepthFirstOnce(left,  [&isLeft, &value] (const core::ExpressionPtr& expr){ if (expr == value) isLeft = true; });
		core::visitDepthFirstOnce(right, [&isRight,&value] (const core::ExpressionPtr& expr){ if (expr == value) isRight = true; });

		if (isLeft){
			// left is the induction expression, right is the up boundary
			originalInductionExpr = left;
			endValue = right;
			conditionLeft = true; 
		}
		else{
			if (!isRight) throw LoopNormalizationError("induction variable could not be identified");
			// right is the induction expression, left is the up boundary
			originalInductionExpr = right;
			endValue = left;
			conditionLeft = false; 
		}

		// strip possible casts
		if (core::CallExprPtr call = originalInductionExpr.isa<core::CallExprPtr>()){
			if (convFact.getIRBuilder().getLangBasic().isScalarCast (call.getFunctionExpr())){
				originalInductionExpr = call[0]; 
			}
		}

		if (!core::analysis::isCallOf(originalInductionExpr, convFact.getNodeManager().getLangBasic().getRefDeref())){
			throw LoopNormalizationError("could not determine number of iterations, please simply the for loop condition to see it as a for loop");
		}

		inductionVar =  convFact.getIRBuilder().variable(originalInductionExpr->getType());
	}
	else throw LoopNormalizationError("Not supported condition");

	// now that we know the induction expression and we created a new var, we have to identify the lower bound
	const clang::Stmt* initStmt = forStmt->getInit();

	if (!initStmt){  // there is no init statement, therefore the initial value is the value of the induction expression at the begining of the loop
		initValue = originalInductionExpr;
        restoreValue = true;
	}
	else{
		// could be a declaration or could be an assignment
		core::StatementPtr initIR = convFact.convertStmt(initStmt);
		InitializationCollector collector(incrementExpr);
		visitDepthFirstPrunable(core::NodeAddress (initIR), collector);
		initValue = collector.init;
		preStmts	 = collector.leftoverStmts;
		restoreValue = !collector.isDecl;
		if (!initValue) {
			// if we could not find any suitable init, the initialization is the value of the original variable at the begining of the loop
			initValue = originalInductionExpr;
		}
	}
}

void LoopAnalyzer::handleIncrExpr(const clang::ForStmt* forStmt) {
	assert(inductionVar && "Loop induction variable not found, impossible to handle increment expression.");

	// a normalized loop always steps +1
	// special case for arrays, since we iterate with an scalar, we generate a ponter wide iteration var (UINT 8)
	if (!frontend::utils::isRefArray(inductionVar->getType()))
		incrExpr = convFact.getIRBuilder().literal("1", originalInductionExpr->getType());
	else
		incrExpr = convFact.getIRBuilder().literal("1", convFact.getIRBuilder().getLangBasic().getUInt8());

	// but what is the real step??
	if( const UnaryOperator* unOp = dyn_cast<const UnaryOperator>(forStmt->getInc()) ) {
		switch(unOp->getOpcode()) {
		case UO_PreInc:
		case UO_PostInc:
			stepExpr =  incrExpr;
			return;
		case UO_PreDec:
		case UO_PostDec:
			stepExpr = convFact.getIRBuilder().invertSign( incrExpr );
			return;
		default:
			LoopNormalizationError("UnaryOperator different from post/pre inc/dec (++/--) not supported in loop increment expression");
		}
	}

	if( const BinaryOperator* binOp = dyn_cast<const BinaryOperator>(forStmt->getInc()) ) {
		auto tmpStep = convFact.convertExpr(binOp->getRHS ());
		switch(binOp->getOpcode()) {
		case BO_AddAssign:
			stepExpr = tmpStep;
			return;
		case BO_SubAssign:
			stepExpr = convFact.getIRBuilder().invertSign( tmpStep);
			return;
		default:
			LoopNormalizationError("unable to produce a for loop with " +binOp->getOpcodeStr().str()+"condition");
		}
	}
	throw LoopNormalizationError("unable to use iteration variable increment in for loop");
}


void LoopAnalyzer::handleCondExpr(const clang::ForStmt* forStmt) {
	const clang::Expr* cond = forStmt->getCond();
	
	// if no condition, not FOR
	if (!cond)
		throw LoopNormalizationError("no condition -> no loop");

	// we know the up bonduary from the induction expression lookup, now we just need to determine whenever to stop at boundary or before
	// if comparator is ( it < N ) whileLessThan should be true already (because of left side) we invert it if no
	if( const BinaryOperator* binOp = dyn_cast<const BinaryOperator>(cond) ) {

		invertComparisonOp = false;

		switch(binOp->getOpcode()) {
		case BO_LT:
			whileLessThan = conditionLeft;
			loopToBoundary = false;
			break;
		case BO_GT:
			whileLessThan = !conditionLeft;
			loopToBoundary = false;
			invertComparisonOp = true;
			break;
		case BO_NE:
			whileLessThan = true;
			loopToBoundary = false;
			break;
		case BO_GE:
			whileLessThan = !conditionLeft;
			loopToBoundary = true;
			invertComparisonOp = true;
			break;
		case BO_LE:
			whileLessThan = conditionLeft;
			loopToBoundary = true;
			break;
		case BO_EQ:
			loopToBoundary = true;
			whileLessThan = true;
			break;

		default:
			throw LoopNormalizationError("BinOp ("+binOp->getOpcodeStr().str()+") in ConditionExpression not supported");
		}

		// collect all variables in conditions to later check if modified
		core::VariableList vars;
		auto cond = convFact.convertExpr(binOp);
		visitDepthFirst(cond, [this, &vars] (const core::VariablePtr& var){ vars.push_back(var);});
		conditionVars = vars;
		return;
	}
	throw LoopNormalizationError("unable to identify the upper bonduary for this loop");
}

insieme::core::ForStmtPtr  LoopAnalyzer::getLoop(const insieme::core::StatementPtr& body) const{
	auto& mgr(body->getNodeManager());

	// if any of condition variables is not read only, we can not guarantee the condition of the loop
	for(auto c : conditionVars ) {
		if(!core::analysis::isReadOnly(body, c))  throw analysis::LoopNormalizationError("Variable in condition expr is not readOnly");
	}

	// if the iteration variable is modified during loop body, we can not guarantee for loop number of iterations (not static bounds)
	core::VariableList vars;
	visitDepthFirst(originalInductionExpr, [this, &vars] (const core::VariablePtr& var){ vars.push_back(var);});
	for(auto c : vars ) {
		if(!core::analysis::isReadOnly(body, c))  throw analysis::LoopNormalizationError("Induction variable is not preserved during loop");
	}

	// substitute the induction expression by the induction var
	core::StatementPtr newBody = core::transform::replaceAllGen(mgr, body, originalInductionExpr, newInductionExpr, true);

	// allrighty... green light, append extra code that might be needed and we are done
	// reproduce first stmts (like assign value if not loop local)
	core::StatementList tmp;
	tmp.insert(tmp.end(), firstStmts.begin(), firstStmts.end());
	tmp.push_back(newBody);
	core::CompoundStmtPtr finalBody = convFact.getIRBuilder().compoundStmt(tmp);

	return convFact.getIRBuilder().forStmt(inductionVar, initValue, endValue, stepExpr, finalBody);
}

} // End analysis namespace
} // End froentend namespace
} // End insieme namespace


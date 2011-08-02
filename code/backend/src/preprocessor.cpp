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

#include "insieme/backend/preprocessor.h"

#include "insieme/backend/ir_extensions.h"

#include "insieme/core/ast_node.h"
#include "insieme/core/ast_builder.h"

#include "insieme/core/analysis/ir_utils.h"
#include "insieme/core/transform/node_replacer.h"
#include "insieme/core/transform/manipulation.h"
#include "insieme/core/transform/manipulation_utils.h"
#include "insieme/core/transform/node_mapper_utils.h"

namespace insieme {
namespace backend {


	core::NodePtr PreProcessingSequence::process(core::NodeManager& manager, const core::NodePtr& code) {

		// start by copying code to given target manager
		core::NodePtr res = manager.get(code);

		// apply sequence of pre-processing steps
		for_each(preprocessor, [&](const PreProcessorPtr& cur) {
			res = cur->process(manager, res);
		});

		// return final result
		return res;
	}


	// ------- concrete pre-processing step implementations ---------

	core::NodePtr NoPreProcessing::process(core::NodeManager& manager, const core::NodePtr& code) {
		// just copy to target manager
		return manager.get(code);
	}



	// --------------------------------------------------------------------------------------------------------------
	//      ITE to lazy-ITE Convertion
	// --------------------------------------------------------------------------------------------------------------

	class ITEConverter : public core::transform::CachedNodeMapping {

		const core::LiteralPtr ITE;
		const IRExtensions extensions;

	public:

		ITEConverter(core::NodeManager& manager) :
			ITE(manager.basic.getIfThenElse()),  extensions(manager) {};

		/**
		 * Searches all ITE calls and replaces them by lazyITE calls. It also is aiming on inlining
		 * the resulting call.
		 */
		const core::NodePtr resolveElement(const core::NodePtr& ptr) {
			// do not touch types ...
			if (ptr->getNodeCategory() == core::NC_Type) {
				return ptr;
			}

			// apply recursively - bottom up
			core::NodePtr res = ptr->substitute(ptr->getNodeManager(), *this, true);

			// check current node
			if (!core::analysis::isCallOf(res, ITE)) {
				// no change required
				return res;
			}

			// exchange ITE call
			core::ASTBuilder builder(res->getNodeManager());
			core::CallExprPtr call = static_pointer_cast<const core::CallExpr>(res);
			const vector<core::ExpressionPtr>& args = call->getArguments();
			res = builder.callExpr(extensions.lazyITE, args[0], evalLazy(args[1]), evalLazy(args[2]));

			// migrate annotations
			core::transform::utils::migrateAnnotations(ptr, res);

			// done
			return res;
		}

	private:

		/**
		 * A utility method for inlining the evaluation of lazy functions.
		 */
		core::ExpressionPtr evalLazy(const core::ExpressionPtr& lazy) {

			core::NodeManager& manager = lazy->getNodeManager();

			core::FunctionTypePtr funType = core::dynamic_pointer_cast<const core::FunctionType>(lazy->getType());
			assert(funType && "Illegal lazy type!");

			// form call expression
			core::CallExprPtr call = core::CallExpr::get(manager, funType->getReturnType(), lazy, toVector<core::ExpressionPtr>());
			return core::transform::tryInlineToExpr(manager, call);
		}
	};



	core::NodePtr IfThenElseInlining::process(core::NodeManager& manager, const core::NodePtr& code) {
		// the converter does the magic
		ITEConverter converter(manager);
		return converter.map(code);
	}



	// --------------------------------------------------------------------------------------------------------------
	//      PreProcessor InitZero convert => replaces call by actual value
	// --------------------------------------------------------------------------------------------------------------

	class InitZeroReplacer : public core::transform::CachedNodeMapping {

		const core::LiteralPtr initZero;
		core::NodeManager& manager;
		const core::lang::BasicGenerator& basic;

	public:

		InitZeroReplacer(core::NodeManager& manager) :
			initZero(manager.basic.getInitZero()), manager(manager), basic(manager.getBasicGenerator()) {};

		/**
		 * Searches all ITE calls and replaces them by lazyITE calls. It also is aiming on inlining
		 * the resulting call.
		 */
		const core::NodePtr resolveElement(const core::NodePtr& ptr) {
			// do not touch types ...
			if (ptr->getNodeCategory() == core::NC_Type) {
				return ptr;
			}

			// apply recursively - bottom up
			core::NodePtr res = ptr->substitute(ptr->getNodeManager(), *this, true);

			// check current node
			if (core::analysis::isCallOf(res, initZero)) {
				// replace with equivalent zero value
				core::NodePtr zero = getZero(static_pointer_cast<const core::Expression>(res)->getType());

				// update result if zero computation was successfull
				res = (zero)?zero:res;
			}

			// no change required
			return res;
		}

	private:

		/**
		 * Obtains an expression of the given type representing zero.
		 */
		core::ExpressionPtr getZero(const core::TypePtr& type) {

			// if it is an integer ...
			if (basic.isInt(type)) {
				return core::Literal::get(manager, type, "0");
			}

			// if it is a real ..
			if (basic.isReal(type)) {
				return core::Literal::get(manager, type, "0.0");
			}

			// if it is a struct ...
			if (type->getNodeType() == core::NT_StructType) {

				// extract type and resolve members recursively
				core::StructTypePtr structType = static_pointer_cast<const core::StructType>(type);

				core::StructExpr::Members members;
				for_each(structType->getEntries(), [&](const core::StructType::Entry& cur) {
					members.push_back(std::make_pair(cur.first, getZero(cur.second)));
				});

				return core::StructExpr::get(manager, members);
			}

			// fall-back => no default initalization possible
			return core::ExpressionPtr();

		}

	};


	core::NodePtr InitZeroSubstitution::process(core::NodeManager& manager, const core::NodePtr& code) {
		// the converter does the magic
		InitZeroReplacer converter(manager);
		return converter.map(code);
	}



	// --------------------------------------------------------------------------------------------------------------
	//      Restore Globals
	// --------------------------------------------------------------------------------------------------------------

	bool isZero(const core::ExpressionPtr& value) {

		const core::lang::BasicGenerator& basic = value->getNodeManager().getBasicGenerator();

		// if initialization is zero ...
		if (core::analysis::isCallOf(value, basic.getInitZero())) {
			// no initialization required
			return true;
		}

		// ... or a zero literal ..
		if (value->getNodeType() == core::NT_Literal) {
			const string& strValue = static_pointer_cast<const core::Literal>(value)->getValue();
			if (strValue == "0" || strValue == "0.0") {
				return true;
			}
		}

		// ... or a call to getNull(...)
		if (core::analysis::isCallOf(value, basic.getGetNull())) {
			return true;
		}

		// ... or a vector initialization with a zero value
		if (core::analysis::isCallOf(value, basic.getVectorInitUniform())) {
			return isZero(core::analysis::getArgument(value, 0));
		}

		// TODO: remove this when frontend is fixed!!
		// => compensate for silly stuff like var(*getNull())
		if (core::analysis::isCallOf(value, basic.getRefVar())) {
			core::ExpressionPtr arg = core::analysis::getArgument(value, 0);
			if (core::analysis::isCallOf(arg, basic.getRefDeref())) {
				return isZero(core::analysis::getArgument(arg, 0));
			}
		}

		// otherwise, it is not zero
		return false;
	}


	core::NodePtr RestoreGlobals::process(core::NodeManager& manager, const core::NodePtr& code) {

		// check for the program - only works on the global level
		if (code->getNodeType() != core::NT_Program) {
			return code;
		}

		// check whether it is a main program ...
		const core::ProgramPtr& program = static_pointer_cast<const core::Program>(code);
		if (!(program->isMain() || program->getEntryPoints().size() == static_cast<std::size_t>(1))) {
			return code;
		}

		// search for global struct
		const core::ExpressionPtr& mainExpr = program->getEntryPoints()[0];
		if (mainExpr->getNodeType() != core::NT_LambdaExpr) {
			return code;
		}
		const core::LambdaExprPtr& main = static_pointer_cast<const core::LambdaExpr>(mainExpr);
		const core::StatementPtr& bodyStmt = main->getBody();
		if (bodyStmt->getNodeType() != core::NT_CompoundStmt) {
			return code;
		}
		core::CompoundStmtPtr body = static_pointer_cast<const core::CompoundStmt>(bodyStmt);
		while (body->getStatements().size() == static_cast<std::size_t>(1)
				&& body->getStatements()[0]->getNodeType() == core::NT_CompoundStmt) {
			body = static_pointer_cast<const core::CompoundStmt>(body->getStatements()[0]);
		}

		// global struct initialization is first line ..
		const core::StatementPtr& globalDeclStmt = body->getStatements()[0];
		if (globalDeclStmt->getNodeType() != core::NT_DeclarationStmt) {
			return code;
		}
		const core::DeclarationStmtPtr& globalDecl = static_pointer_cast<const core::DeclarationStmt>(globalDeclStmt);

		// extract variable
		const core::VariablePtr& globals = globalDecl->getVariable();
		const core::TypePtr& globalType = globals->getType();

		// check whether it is really a global struct ...
		if (globalType->getNodeType() != core::NT_RefType) {
			// this is not a global struct ..
			return code;
		}

		const core::TypePtr& structType = static_pointer_cast<const core::RefType>(globalType)->getElementType();
		if (structType->getNodeType() != core::NT_StructType) {
			// this is not a global struct ..
			return code;
		}

		// check initialization
		if (!core::analysis::isCallOf(globalDecl->getInitialization(), manager.basic.getRefNew())) {
			// this is not a global struct ...
			return code;
		}

		core::LiteralPtr replacement = core::Literal::get(manager, globalType, IRExtensions::GLOBAL_ID);

		// replace global declaration statement with initalization block
		IRExtensions extensions(manager);
		core::TypePtr unit = manager.getBasicGenerator().getUnit();
		core::ExpressionPtr initValue = core::analysis::getArgument(globalDecl->getInitialization(), 0);
		core::StatementPtr initGlobal = core::CallExpr::get(manager, unit, extensions.initGlobals, toVector(initValue));


		core::ASTBuilder builder(manager);
		vector<core::StatementPtr> initExpressions;
		{
			// start with initGlobals call (initializes code fragment and adds dependencies)
			initExpressions.push_back(initGlobal);

			// initialize remaining fields of global struct
			core::ExpressionPtr initValue = core::analysis::getArgument(globalDecl->getInitialization(), 0);
			assert(initValue->getNodeType() == core::NT_StructExpr);
			core::StructExprPtr initStruct = static_pointer_cast<const core::StructExpr>(initValue);

			// get some functions used for the pattern matching
			core::ExpressionPtr initUniform = manager.basic.getVectorInitUniform();
			core::ExpressionPtr initZero = manager.basic.getInitZero();

			for_each(initStruct->getMembers(), [&](const core::StructExpr::Member& cur) {

				// ignore zero values => default initialization
				if (isZero(cur.second)) {
					return;
				}

				core::ExpressionPtr access = builder.refMember(replacement, cur.first);
				core::ExpressionPtr assign = builder.assign(access, cur.second);
				initExpressions.push_back(assign);
			});
		}


		// replace declaration with init call
		core::StatementList stmts = body->getStatements();
		stmts[0] = builder.compoundStmt(initExpressions);
		core::StatementPtr newBody = core::CompoundStmt::get(manager,stmts);

		// fix the global variable
		newBody = core::transform::fixVariable(manager, newBody, globals, replacement);
		return core::transform::replaceAll(manager, code, body, newBody);
	}

} // end namespace backend
} // end namespace insieme

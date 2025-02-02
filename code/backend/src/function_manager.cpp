/**
 * Copyright (c) 2002-2015 Distributed and Parallel Systems Group,
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

#include "insieme/backend/function_manager.h"

#include <set>
#include <tuple>
#include <functional>

#include "insieme/backend/converter.h"
#include "insieme/backend/type_manager.h"
#include "insieme/backend/statement_converter.h"
#include "insieme/backend/name_manager.h"
#include "insieme/backend/variable_manager.h"
#include "insieme/backend/c_ast/c_ast_utils.h"

#include "insieme/core/ir_expressions.h"
#include "insieme/core/ir_builder.h"
#include "insieme/core/ir_cached_visitor.h"
#include "insieme/core/analysis/ir_utils.h"
#include "insieme/core/analysis/attributes.h"
#include "insieme/core/analysis/normalize.h"
#include "insieme/core/lang/basic.h"
#include "insieme/core/lang/varargs_extension.h"
#include "insieme/core/transform/manipulation.h"
#include "insieme/core/transform/node_replacer.h"
#include "insieme/core/transform/instantiate.h"
#include "insieme/core/types/type_variable_deduction.h"

#include "insieme/annotations/c/include.h"
#include "insieme/annotations/c/extern_c.h"

#include "insieme/utils/map_utils.h"
#include "insieme/utils/logging.h"
#include "insieme/utils/assert.h"
#include "insieme/utils/name_mangling.h"

namespace insieme {
namespace backend {


	namespace detail {

		template <typename Type>
		struct info_trait;
		template <>
		struct info_trait<core::Literal> {
			typedef FunctionInfo type;
		};
		template <>
		struct info_trait<core::LambdaExpr> {
			typedef LambdaInfo type;
		};
		template <>
		struct info_trait<core::BindExpr> {
			typedef BindInfo type;
		};

		struct FunctionCodeInfo {
			c_ast::FunctionPtr function;
			c_ast::DefinitionPtr definition;
			c_ast::FragmentSet prototypeDependencies;
			c_ast::FragmentSet definitionDependencies;
			std::set<string> includes;
		};

		class FunctionInfoStore {
			const Converter& converter;

			utils::map::PointerMap<core::ExpressionPtr, ElementInfo*> funInfos;

		  public:
			FunctionInfoStore(const Converter& converter) : converter(converter), funInfos() {}

			~FunctionInfoStore() {
				// free all stored type information instances
				for_each(funInfos, [](const std::pair<core::ExpressionPtr, ElementInfo*>& cur) { delete cur.second; });
			}

			template <typename T, typename result_type = typename info_trait<typename std::remove_const<typename T::element_type>::type>::type*>
			result_type resolve(const T& expression, bool isConst = false, bool isVirtual = false) {
				// lookup information using internal mechanism and cast statically
				ElementInfo* info = resolveInternal(expression, isConst, isVirtual);
				assert(dynamic_cast<result_type>(info));
				return static_cast<result_type>(info);
			}

		  protected:
			ElementInfo* resolveInternal(const core::ExpressionPtr& expression, bool isConst = false, bool isVirtual = false);

			ElementInfo* resolveLiteral(const core::LiteralPtr& literal, bool isConst);
			ElementInfo* resolveBind(const core::BindExprPtr& bind);
			ElementInfo* resolveLambda(const core::LambdaExprPtr& lambda, bool isConst, bool isVirtual);
			void resolveLambdaDefinition(const core::LambdaDefinitionPtr& lambdaDefinition, bool isConst, bool isVirtual);

			// -------- utilities -----------

			FunctionCodeInfo resolveFunction(const c_ast::IdentifierPtr name, const core::FunctionTypePtr& funType, const core::LambdaPtr& lambda,
			                                 bool external, bool isConst = false);

			std::pair<c_ast::IdentifierPtr, c_ast::CodeFragmentPtr> resolveLambdaWrapper(const c_ast::FunctionPtr& function,
			                                                                             const core::FunctionTypePtr& funType, bool external);
		};
	}

	FunctionManager::FunctionManager(const Converter& converter)
	    : converter(converter), store(new detail::FunctionInfoStore(converter)), operatorTable(getBasicOperatorTable(converter.getNodeManager())),
	      includeTable(getBasicFunctionIncludeTable()) {}

	FunctionManager::FunctionManager(const Converter& converter, const OperatorConverterTable& operatorTable, const FunctionIncludeTable& includeTable)
	    : converter(converter), store(new detail::FunctionInfoStore(converter)), operatorTable(operatorTable), includeTable(includeTable) {}

	FunctionManager::~FunctionManager() {
		delete store;
	}

	const FunctionInfo& FunctionManager::getInfo(const core::LiteralPtr& literal) {
		return *(store->resolve(literal));
	}

	const FunctionInfo& FunctionManager::getInfo(const core::LiteralPtr& pureVirtualMemberFun, bool isConst) {
		return *(store->resolve(pureVirtualMemberFun, isConst, true));
	}

	const LambdaInfo& FunctionManager::getInfo(const core::LambdaExprPtr& lambda) {
		return *(store->resolve(lambda));
	}

	const LambdaInfo& FunctionManager::getInfo(const core::LambdaExprPtr& memberFun, bool isConst, bool isVirtual) {
		return *(store->resolve(memberFun, isConst, isVirtual));
	}

	const BindInfo& FunctionManager::getInfo(const core::BindExprPtr& bind) {
		return *(store->resolve(bind));
	}

	bool FunctionManager::isBuiltIn(const core::NodePtr& op) const {
		if(op->getNodeCategory() != core::NC_Expression) { return false; }
		return operatorTable.find(op.as<core::ExpressionPtr>()) != operatorTable.end() || annotations::c::hasIncludeAttached(op);
	}

	namespace {

		void appendAsArguments(ConversionContext& context, c_ast::CallPtr& call, const vector<core::ExpressionPtr>& arguments, bool external) {
			// collect some manager references
			const Converter& converter = context.getConverter();
			const c_ast::SharedCNodeManager& manager = converter.getCNodeManager();
			StmtConverter& stmtConverter = converter.getStmtConverter();
			TypeManager& typeManager = converter.getTypeManager();

			auto varlistPack = converter.getNodeManager().getLangExtension<core::lang::VarArgsExtension>().getVarlistPack();

			// create a recursive lambda appending arguments to the caller (descent into varlist-pack calls)
			std::function<void(const core::ExpressionPtr& argument)> append;
			auto recLambda = [&](const core::ExpressionPtr& cur) {

				// test if current argument is a variable argument list
				if(core::analysis::isCallOf(cur, varlistPack)) {
					// inline arguments of varlist-pack call => append arguments directly
					const vector<core::ExpressionPtr>& packed = static_pointer_cast<const core::CallExpr>(cur)->getArguments();

					for_each(static_pointer_cast<const core::TupleExpr>(packed[0])->getExpressions()->getElements(),
					         [&](const core::ExpressionPtr& cur) { append(cur); });
					return;
				}

				// test if the current argument is a type literal
				if(core::analysis::isTypeLiteralType(cur->getType())) {
					return; // skip those parameters
				}

				// simply append the argument (externalize if necessary)
				c_ast::ExpressionPtr res = stmtConverter.convertExpression(context, cur);
				call->arguments.push_back((external) ? typeManager.getTypeInfo(cur->getType()).externalize(manager, res) : res);

			};
			append = recLambda;

			// invoke append for all arguments
			for_each(arguments, [&](const core::ExpressionPtr& cur) { append(cur); });
		}

		core::TagTypePtr getClassType(const core::FunctionTypePtr& funType) {
			core::TypePtr type = funType->getObjectType();
			if (auto tagType = type.isa<core::TagTypePtr>()) {
				if (tagType->isRecursive()) {
					type = tagType->peel();
				}
			}
			return type.as<core::TagTypePtr>();
		}

		c_ast::NodePtr handleMemberCall(const core::CallExprPtr& call, c_ast::CallPtr c_call, ConversionContext& context) {
			// by default, do nothing
			c_ast::ExpressionPtr res = c_call;

			// extract type of target function
			const core::FunctionTypePtr funType = call->getFunctionExpr()->getType().as<core::FunctionTypePtr>();

			// ----------------- constructor call ---------------

			// re-structure call into a constructor call
			if(funType->isConstructor()) {
				vector<c_ast::NodePtr> args = c_call->arguments;
				assert_false(args.empty());

				const auto& refs = call->getNodeManager().getLangExtension<core::lang::ReferenceExtension>();
				auto location = args[0];
				args.erase(args.begin());

				// extract class type
				auto classType = context.getConverter().getTypeManager().getTypeInfo(funType->getObjectType()).lValueType;

				// distinguish memory location to be utilized
				// case a) create object on stack => default

				// case b) create object on heap
				bool isOnHeap = core::analysis::isCallOf(call[0], refs.getRefNewInit());

				// case c) create object in-place (placement new)
				c_ast::ExpressionPtr loc = (!core::analysis::isCallOf(call[0], refs.getRefVarInit()) && !core::analysis::isCallOf(call[0], refs.getRefNewInit()))
				                               ? location.as<c_ast::ExpressionPtr>()
				                               : c_ast::ExpressionPtr();

				// to get support for the placement new the new header is required
				if(loc) { context.addInclude("<new>"); }

				// create constructor call
				res = c_ast::ctorCall(classType, args, loc);

				// add new call if required
				if(isOnHeap) {
					res = c_ast::newCall(res);
				} else if(!loc) {
					// if it is not an in-place construction => add a & operation
					res = c_ast::ref(res);
				}
			}

			// ---------------- destructor call -----------------

			if(funType->isDestructor()) {
				// obtain object
				vector<c_ast::NodePtr> args = c_call->arguments;
				assert_eq(args.size(), 1u);
				auto obj = c_ast::deref(args[0].as<c_ast::ExpressionPtr>());

				// extract class type
				auto classType = context.getConverter().getTypeManager().getTypeInfo(funType->getObjectType()).lValueType;

				// create resulting call
				res = c_ast::dtorCall(classType, obj, false); // it is not a virtual destructor if it is explicitly mentioned
			}

			// --------------- member function call -------------

			// re-structure call in case it is a member function call
			if(funType->isMemberFunction()) {
				vector<c_ast::NodePtr> args = c_call->arguments;
				assert_false(args.empty());

				auto obj = c_ast::deref(args[0].as<c_ast::ExpressionPtr>());
				args.erase(args.begin());

				res = c_ast::memberCall(obj, c_call->function, args);
			}

			// --------------- virtual member function call -------------

			// TODO
			if(funType->isVirtualMemberFunction()) {
				assert_not_implemented();
			}

			// use result
			return res;
		}

		core::CallExprPtr wrapPlainFunctionArguments(const core::CallExprPtr& call) {
			// extract node manager
			auto& manager = call->getNodeManager();
			core::IRBuilder builder(manager);

			// check whether there is a argument which is a vector but the parameter is not
			const core::TypePtr& type = call->getFunctionExpr()->getType();
			assert_eq(type->getNodeType(), core::NT_FunctionType) << "Function should be of a function type!";
			const core::FunctionTypePtr& funType = core::static_pointer_cast<const core::FunctionType>(type);

			const core::TypeList& paramTypes = funType->getParameterTypes()->getElements();
			const core::ExpressionList& args = call->getArguments();

			// check number of arguments
			if(paramTypes.size() != args.size()) {
				// => invalid call, don't touch this
				return call;
			}

			// generate new argument list
			bool changed = false;
			core::ExpressionList newArgs = call->getArguments();
			for(unsigned i = 0; i < newArgs.size(); i++) {
				// get pair of types
				core::FunctionTypePtr paramType = paramTypes[i].isa<core::FunctionTypePtr>();
				core::FunctionTypePtr argType = newArgs[i]->getType().isa<core::FunctionTypePtr>();

				// ignore identical types or non-function types
				if(!paramType || !argType || *paramType == *argType) { continue; }

				// only interested if param is a bind and arg a function
				if(!(paramType->isClosure() && argType->isPlain())) { continue; }

				// create a bind wrapping the targeted function
				core::VariableList bindParams;
				core::ExpressionList argList;
				for(const core::TypePtr& type : argType->getParameterTypes()) {
					auto var = builder.variable(type);
					bindParams.push_back(var);
					argList.push_back(var);
				}

				// the argument needs to be wrapped into a bind
				const core::TypePtr& retType = argType->getReturnType();
				core::FunctionTypePtr newType = builder.functionType(argType->getParameterTypes(), retType, core::FK_CLOSURE);

				newArgs[i] = builder.bindExpr(newType, bindParams, builder.callExpr(retType, newArgs[i], argList));

				// note the change
				changed = true;
			}
			if(!changed) {
				// return original call
				return call;
			}

			// exchange arguments and done
			return core::CallExpr::get(manager, call->getType(), call->getFunctionExpr(), newArgs);
		}
	}


	const c_ast::NodePtr FunctionManager::getCall(const core::CallExprPtr& in, ConversionContext& context) {
		// conducte some cleanup (argument wrapping)
		core::CallExprPtr call = wrapPlainFunctionArguments(in);

		// extract target function
		core::ExpressionPtr fun = core::analysis::stripAttributes(call->getFunctionExpr());

		fun = core::IRBuilder(context.getConverter().getNodeManager()).normalize(fun);

		// 1) see whether call is call to a known operator
		auto pos = operatorTable.find(fun);
		if(pos != operatorTable.end()) {
			// use operator converter
			return pos->second(context, call);
		}

		// 2) test whether target is a literal => external function, direct call
		if(fun->getNodeType() == core::NT_Literal) {
			// obtain literal information
			const FunctionInfo& info = getInfo(static_pointer_cast<const core::Literal>(fun));

			// produce call to external literal
			c_ast::CallPtr res = c_ast::call(info.function->name);
			appendAsArguments(context, res, call->getArguments(), true);

			// add dependencies
			context.getDependencies().insert(info.prototype);

			// return external function call
			auto ret = handleMemberCall(call, res, context);
			return ret;
		}

		// 3) test whether target is a lambda => call lambda directly, without creating a closure
		if(fun->getNodeType() == core::NT_LambdaExpr) {
			// obtain lambda information
			const LambdaInfo& info = getInfo(static_pointer_cast<const core::LambdaExpr>(fun));

			// add dependencies and requirements
			context.getDependencies().insert(info.prototype);
			context.getRequirements().insert(info.definition);

			// deal with different call mechanisms
			auto funType = fun.as<core::LambdaExprPtr>()->getFunctionType();

			// -------------- standard function call ------------

			// produce call to internal lambda
			c_ast::CallPtr c_call = c_ast::call(info.function->name);
			appendAsArguments(context, c_call, call->getArguments(), false);

			// handle potential member calls
			auto ret = handleMemberCall(call, c_call, context);
			return ret;
		}

		core::FunctionTypePtr funType = static_pointer_cast<const core::FunctionType>(fun->getType());

		// 4) test whether target is a plain function pointer => call function pointer, no closure
		if(funType->isPlain()) {
			// add call to function pointer (which is the value)
			c_ast::CallPtr res = c_ast::call(c_ast::parentheses(getValue(call->getFunctionExpr(), context)));
			appendAsArguments(context, res, call->getArguments(), false);
			return res;
		}

		// 5) if is a member function pointer
		if(funType->isMemberFunction()) {
			// add call to function pointer (which is the value)

			// extract first parameter of the function, it is the target object
			c_ast::ExpressionPtr trgObj = converter.getStmtConverter().convertExpression(context, call[0]);

			// make a call to the member pointer executor binary operator
			c_ast::ExpressionPtr funcExpr = c_ast::parentheses(c_ast::pointerToMember(trgObj, getValue(call->getFunctionExpr(), context)));

			// the call is a call to the binary operation al the n-1 tail arguments
			c_ast::CallPtr res = c_ast::call(funcExpr);
			vector<core::ExpressionPtr> args = call->getArguments();
			args.erase(args.begin());
			appendAsArguments(context, res, args, false);
			return res;
		}

		// TODO
		if(funType->isVirtualMemberFunction()) {
			assert_not_implemented();
		}

		// Finally: the generic fall-back solution:
		//		get function as a value and call it using the function-type's caller function

		c_ast::ExpressionPtr value = getValue(call->getFunctionExpr(), context);

		const FunctionTypeInfo& typeInfo = converter.getTypeManager().getTypeInfo(funType);
		c_ast::CallPtr res = c_ast::call(typeInfo.callerName, c_ast::cast(typeInfo.rValueType, value));
		appendAsArguments(context, res, call->getArguments(), false);

		// add dependencies
		context.getDependencies().insert(typeInfo.caller);

		return res;
	}


	const c_ast::ExpressionPtr FunctionManager::getValue(const core::ExpressionPtr& fun, ConversionContext& context) {
		auto manager = converter.getCNodeManager();

		// handle according to node type
		switch(fun->getNodeType()) {
		case core::NT_BindExpr: {
			return getValue(static_pointer_cast<const core::BindExpr>(fun), context);
		}
		case core::NT_Literal: {
			const FunctionInfo& info = getInfo(static_pointer_cast<const core::Literal>(fun));
			if(static_pointer_cast<const core::FunctionType>(fun->getType())->isPlain()) {
				// TODO: also check whether an externalization is required
				context.getDependencies().insert(info.prototype);
				return c_ast::ref(info.function->name);
			}
			context.getDependencies().insert(info.lambdaWrapper);
			return c_ast::ref(info.lambdaWrapperName);
		}
		case core::NT_LambdaExpr: {
			const LambdaInfo& info = getInfo(static_pointer_cast<const core::LambdaExpr>(fun));
			context.getDependencies().insert(info.prototype);

			// FIXME: hack to support member function pointers intialization
			c_ast::NodePtr thing = static_pointer_cast<c_ast::CCodeFragment>(info.definition)->getCode()[1];
			if(c_ast::MemberFunctionPtr mem = thing.isa<c_ast::MemberFunctionPtr>()) { return c_ast::ref(c_ast::scope(mem->className, info.function->name)); }

			return c_ast::ref(info.function->name);
		}
		case core::NT_Variable:
		case core::NT_CallExpr: {
			// variable is already representing a value
			return converter.getStmtConverter().convertExpression(context, fun);
		}
		case core::NT_CastExpr: {
			// function pointer casted to a different type of function pointer
			return converter.getStmtConverter().convertExpression(context, fun.as<core::CastExprPtr>());
		}
		default:
			LOG(FATAL) << "Encountered unsupported node: " << *fun;
			assert_fail() << "Unexpected Node Type!";
			return c_ast::ExpressionPtr();
		}
	}

	const c_ast::ExpressionPtr FunctionManager::getValue(const core::BindExprPtr& bind, ConversionContext& context) {
		auto manager = converter.getCNodeManager();

		// create a value instance by initializing the bind closure using its constructor

		// collect some information
		const BindInfo& info = getInfo(bind);
		const FunctionTypeInfo& typeInfo = converter.getTypeManager().getTypeInfo(static_pointer_cast<const core::FunctionType>(bind->getType()));

		// add dependencies
		c_ast::FragmentSet& dependencies = context.getDependencies();
		dependencies.insert(typeInfo.definition);
		dependencies.insert(typeInfo.constructor);
		dependencies.insert(info.definitions);


		// allocate memory for the bind expression
		context.getIncludes().insert("alloca.h");
		c_ast::ExpressionPtr alloc =
		    c_ast::cast(c_ast::ptr(info.closureType), c_ast::call(manager->create("alloca"), c_ast::unaryOp(c_ast::UnaryOperation::SizeOf, info.closureType)));

		// pre-process target function
		auto fun = bind->getCall()->getFunctionExpr();

		// instantiate generic lambdas if necessary
		if(fun.isa<core::LambdaExprPtr>()) {
			// extract node manager
			auto& mgr = bind->getNodeManager();

			// get type variable substitution for call
			core::types::SubstitutionOpt&& map = core::types::getTypeVariableInstantiation(mgr, bind->getCall());

			// instantiate function expression
			fun = core::transform::instantiate(mgr, fun.as<core::LambdaExprPtr>(), map);
		}

		// create nested closure
		c_ast::ExpressionPtr nested = getValue(fun, context);

		//  create constructor call
		c_ast::CallPtr res = c_ast::call(info.constructorName, alloc, nested);

		// add captured expressions
		auto boundExpression = bind->getBoundExpressions();
		appendAsArguments(context, res, boundExpression, false);

		// done
		return res;
	}

	const boost::optional<string> FunctionManager::getHeaderFor(const string& function) const {
		// try looking up function within the include table
		auto pos = includeTable.find(function);
		if(pos != includeTable.end()) { return pos->second; }
		// not found => return empty optional
		return boost::optional<string>();
	}

	const boost::optional<string> FunctionManager::getHeaderFor(const core::LiteralPtr& function) const {
		// include table has priority
		auto res = getHeaderFor(function->getStringValue());
		if(res) { return res; }

		// check whether there is a annotated header
		if(annotations::c::hasIncludeAttached(function)) { return annotations::c::getAttachedInclude(function); }

		// otherwise there is no header ..
		return res;
	}

	namespace detail {


		ElementInfo* FunctionInfoStore::resolveInternal(const core::ExpressionPtr& expr, bool isConst, bool isVirtual) {
			// normalize member functions to avoid unintended duplication and resulting name collisions
			core::ExpressionPtr expression = expr;
			core::FunctionTypePtr funType = expr->getType().as<core::FunctionTypePtr>();
			if(funType->isMember()) {
				// normalize member
				expression = core::analysis::normalize(expression);

				// make sure the object definition, ctors, dtors and member functions have already been resolved
				// if this would not be the case, we could end up resolving e.g. a ctor while resolving the ctor itself
				converter.getTypeManager().getTypeInfo(funType->getObjectType()); // ignore result
			}

			// lookup information within store
			auto pos = funInfos.find(expression);
			if(pos != funInfos.end()) { return pos->second; }

			// obtain function information
			ElementInfo* info;

			// not known yet => requires some resolution
			switch(expression->getNodeType()) {
			case core::NT_Literal: info = resolveLiteral(static_pointer_cast<const core::Literal>(expression), isConst); break;
			case core::NT_LambdaExpr: info = resolveLambda(static_pointer_cast<const core::LambdaExpr>(expression), isConst, isVirtual); break;
			case core::NT_BindExpr: info = resolveBind(static_pointer_cast<const core::BindExpr>(expression)); break;
			default:
				// this should not happen ...
				assert_fail() << "Unsupported node type encountered!";
				return new ElementInfo();
			}

			// store information
			funInfos.insert(std::make_pair(expression, info));

			// return pointer to obtained information
			return info;
		}

		ElementInfo* FunctionInfoStore::resolveLiteral(const core::LiteralPtr& literal, bool isConst) {
			assert_eq(literal->getType()->getNodeType(), core::NT_FunctionType) << "Only supporting literals with a function type!";

			// some preparation
			auto manager = converter.getCNodeManager();
			core::FunctionTypePtr funType = static_pointer_cast<const core::FunctionType>(literal->getType());
			FunctionInfo* res = new FunctionInfo();

			TypeManager& typeManager = converter.getTypeManager();

			// ------------------------ resolve function ---------------------

			FunctionCodeInfo fun = resolveFunction(manager->create(insieme::utils::demangle(literal->getStringValue())), funType, core::LambdaPtr(), true);

			res->function = fun.function;

			// ------------------------ add prototype -------------------------

			auto header = converter.getFunctionManager().getHeaderFor(literal);
			if(header) {
				// => use prototype of include file
				res->prototype = c_ast::IncludeFragment::createNew(converter.getFragmentManager(), *header);

			} else if(funType->isMemberFunction()) {
				// add pure-virtual member function to class declaration
				const auto& typeInfo = typeManager.getTypeInfo(funType->getObjectType());
				res->prototype = typeInfo.definition;
				res->prototype->addDependencies(fun.prototypeDependencies);

				// add declaration of pure-virtual function
				c_ast::StructTypePtr classDecl = typeInfo.lValueType.as<c_ast::StructTypePtr>();
				auto mFun = manager->create<c_ast::MemberFunction>(classDecl->name, fun.function, isConst);
				classDecl->members.push_back(manager->create<c_ast::MemberFunctionPrototype>(mFun, true, true));

				//TODO
			} else if(funType->isVirtualMemberFunction()) {
				assert_not_implemented();

			} else if(literal->getStringValue().substr(0, 6) == "__sync") {
				// => ignore built-in atomic operations

			} else {
				// => add prototype for this literal
				c_ast::TopLevelElementPtr code = manager->create<c_ast::FunctionPrototype>(fun.function);
				if(annotations::c::isExternC(literal)) { code = manager->create<c_ast::ExternC>(code); }
				res->prototype = c_ast::CCodeFragment::createNew(converter.getFragmentManager(), code);
				res->prototype->addDependencies(fun.prototypeDependencies);
			}

			// -------------------------- add lambda wrapper ---------------------------

			auto wrapper = resolveLambdaWrapper(fun.function, funType, true);
			res->lambdaWrapperName = wrapper.first;
			res->lambdaWrapper = wrapper.second;
			res->lambdaWrapper->addDependencies(fun.prototypeDependencies);
			res->lambdaWrapper->addDependency(res->prototype);

			// done
			return res;
		}

		ElementInfo* FunctionInfoStore::resolveBind(const core::BindExprPtr& bind) {
			// prepare some manager
			NameManager& nameManager = converter.getNameManager();
			TypeManager& typeManager = converter.getTypeManager();
			auto manager = converter.getCNodeManager();

			// create resulting data container
			BindInfo* res = new BindInfo();

			// set up names
			string name = nameManager.getName(bind, "bind");
			res->closureName = manager->create(name + "_closure");
			res->mapperName = manager->create(name + "_mapper");
			res->constructorName = manager->create(name + "_ctr");

			// instantiate nested call
			auto call = bind->getCall();

			// instantiate generic lambdas if necessary
			if(auto fun = call->getFunctionExpr().isa<core::LambdaExprPtr>()) {
				// extract node manager
				auto& mgr = bind->getNodeManager();

				// get type variable substitution for call
				core::types::SubstitutionOpt&& map = core::types::getTypeVariableInstantiation(mgr, bind->getCall());

				// instantiate function expression
				fun = core::transform::instantiate(mgr, fun.as<core::LambdaExprPtr>(), map);

				// replace call with call to instantiated function
				call = core::IRBuilder(call->getNodeManager()).callExpr(call->getType(), fun, call->getArguments());
			}

			// create a map between expressions in the IR and parameter / captured variable names in C
			utils::map::PointerMap<core::ExpressionPtr, c_ast::VariablePtr> variableMap;

			// add parameters
			int paramCounter = 0;
			const vector<core::VariablePtr>& parameter = bind->getParameters()->getElements();
			for_each(parameter, [&](const core::VariablePtr& cur) {
				variableMap[cur] = var(typeManager.getTypeInfo(cur->getType()).rValueType, format("p%d", ++paramCounter));
			});

			// add arguments of call
			int argumentCounter = 0;
			const vector<core::ExpressionPtr>& args = call->getArguments();
			for_each(args, [&](const core::ExpressionPtr& cur) {
				variableMap[cur] = var(typeManager.getTypeInfo(cur->getType()).rValueType, format("c%d", ++argumentCounter));
			});

			// extract captured variables
			vector<core::ExpressionPtr> captured = bind->getBoundExpressions();

			vector<c_ast::VariablePtr> varsCaptured;
			::transform(captured, std::back_inserter(varsCaptured), [&](const core::ExpressionPtr& cur) { return variableMap[cur]; });


			// ----------- define closure type ---------------

			// create closure struct
			c_ast::StructTypePtr closureStruct = manager->create<c_ast::StructType>(res->closureName);

			// get function type of mapper
			core::FunctionTypePtr funType = static_pointer_cast<const core::FunctionType>(bind->getType());
			const FunctionTypeInfo& funInfo = typeManager.getTypeInfo(funType);

			// construct variable / struct entry pointing to the function to be called when processing the closure
			c_ast::FunctionTypePtr mapperType = manager->create<c_ast::FunctionType>(typeManager.getTypeInfo(funType->getReturnType()).rValueType);
			mapperType->parameterTypes.push_back(manager->create<c_ast::PointerType>(closureStruct));
			for_each(parameter,
			         [&](const core::VariablePtr& var) { mapperType->parameterTypes.push_back(typeManager.getTypeInfo(var->getType()).rValueType); });
			c_ast::VariablePtr varCall = c_ast::var(manager->create<c_ast::PointerType>(mapperType), "call");

			// get generic type of nested closure
			core::FunctionTypePtr nestedFunType = static_pointer_cast<const core::FunctionType>(call->getFunctionExpr()->getType());
			const FunctionTypeInfo& nestedClosureInfo = typeManager.getTypeInfo(nestedFunType);

			// define variable / struct entry pointing to the nested closure variable
			c_ast::TypePtr varNestedType = nestedClosureInfo.rValueType;
			if (nestedFunType.isPlain()) varNestedType = c_ast::ptr(varNestedType);
			c_ast::VariablePtr varNested = c_ast::var(varNestedType, "nested");

			// finally, add fields to struct
			closureStruct->elements.push_back(varCall);
			closureStruct->elements.push_back(varNested);
			addAll(closureStruct->elements, varsCaptured);

			c_ast::NodePtr closureDecl = manager->create<c_ast::TypeDeclaration>(closureStruct);
			c_ast::NodePtr closureDef = manager->create<c_ast::TypeDefinition>(closureStruct);
			res->closureType = manager->create<c_ast::NamedType>(res->closureName);


			// --------------------------------- define mapper -------------------------------------
			c_ast::VariablePtr varClosure = var(manager->create<c_ast::PointerType>(res->closureType), "closure");

			c_ast::FunctionPtr mapper;
			{
				bool plain = nestedFunType->isPlain();
				c_ast::TypePtr returnType = mapperType->returnType;

				vector<c_ast::VariablePtr> params;
				params.push_back(varClosure);
				::transform(bind->getParameters()->getElements(), std::back_inserter(params), [&](const core::VariablePtr& cur) { return variableMap[cur]; });

				c_ast::ExpressionPtr fun = indirectAccess(varClosure, "nested");
				if(!plain) { fun = indirectAccess(fun, "call"); }

				c_ast::CallPtr call = manager->create<c_ast::Call>(fun);
				if(!plain) { call->arguments.push_back(indirectAccess(varClosure, "nested")); }

				for_each(args, [&](const core::ExpressionPtr& cur) {
					c_ast::VariablePtr var = variableMap[cur];
					c_ast::ExpressionPtr param = var;
					if(contains(captured, cur, equal_target<core::ExpressionPtr>())) { param = indirectAccess(varClosure, var->name); }
					call->arguments.push_back(param);
				});

				c_ast::StatementPtr body = call;
				if(!isVoid(returnType)) { body = manager->create<c_ast::Return>(call); }

				mapper = manager->create<c_ast::Function>(returnType, res->mapperName, params, body);
			}

			// --------------------------------- define constructor -------------------------------------

			c_ast::NodePtr constructor;
			{
				// the constructor collects captured variables and a pointer to a pre-allocated closure struct
				// and initializes all the closure's fields.

				// create return type
				c_ast::TypePtr returnType = funInfo.rValueType;

				// assemble parameters
				vector<c_ast::VariablePtr> params;
				params.push_back(varClosure);
				params.push_back(varNested);
				addAll(params, varsCaptured);

				// create the body
				c_ast::InitializerPtr init = c_ast::init(res->closureType, c_ast::ref(res->mapperName), varNested);
				addAll(init->values, varsCaptured);
				c_ast::ExpressionPtr assign = c_ast::assign(c_ast::deref(varClosure), init);
				c_ast::StatementPtr body = compound(assign, c_ast::ret(c_ast::cast(returnType, varClosure)));

				// assemble constructor
				constructor =
				    manager->create<c_ast::Function>(c_ast::Function::STATIC | c_ast::Function::INLINE, returnType, res->constructorName, params, body);
			}

			// attach definitions of closure, mapper and constructor
			res->definitions = c_ast::CCodeFragment::createNew(
			    converter.getFragmentManager(),
			    manager->create<c_ast::Comment>("-- Begin - Bind Constructs ------------------------------------------------------------"), closureDecl,
			    closureDef, mapper, constructor,
			    manager->create<c_ast::Comment>("--  End  - Bind Constructs ------------------------------------------------------------"));

			res->definitions->addDependency(funInfo.declaration);
			res->definitions->addDependency(nestedClosureInfo.definition);
			res->definitions->addDependency(nestedClosureInfo.caller);

			// finally - add a dependency to the return type definition since it is returned by value
			res->definitions->addDependency(typeManager.getTypeInfo(call->getType()).definition);

			// done
			return res;
		}

		ElementInfo* FunctionInfoStore::resolveLambda(const core::LambdaExprPtr& lambda, bool isConst, bool isVirtual) {
			// resolve lambda definitions
			resolveLambdaDefinition(lambda->getDefinition(), isConst, isVirtual);

			// look up lambda again
			return resolveInternal(lambda);
		}

		void FunctionInfoStore::resolveLambdaDefinition(const core::LambdaDefinitionPtr& lambdaDefinition, bool isConst, bool isVirtual) {
			// prepare some manager
			NameManager& nameManager = converter.getNameManager();
			core::NodeManager& manager = converter.getNodeManager();
			auto& cManager = converter.getCNodeManager();
			auto& fragmentManager = converter.getFragmentManager();
			auto& typeManager = converter.getTypeManager();

			// create definition and declaration block
			c_ast::CCodeFragmentPtr declarations = c_ast::CCodeFragment::createNew(fragmentManager);
			c_ast::CCodeFragmentPtr definitions = c_ast::CCodeFragment::createNew(fragmentManager);

			// add requirement for definition once been declared
			declarations->addRequirement(definitions);

			declarations->appendCode(cManager->create<c_ast::Comment>("------- Function Prototypes ----------"));
			definitions->appendCode(cManager->create<c_ast::Comment>("------- Function Definitions ---------"));
			definitions->appendCode(converter.convertToComment(lambdaDefinition));

			// A) get list of all lambdas within this recursive group
			vector<std::pair<c_ast::IdentifierPtr, core::LambdaExprPtr>> lambdas;

			::transform(lambdaDefinition->getDefinitions(), std::back_inserter(lambdas),
			            [&](const core::LambdaBindingPtr& cur) -> std::pair<c_ast::IdentifierPtr, core::LambdaExprPtr> {
				            auto lambda = core::LambdaExpr::get(manager, cur->getReference(), lambdaDefinition);
				            return std::make_pair(cManager->create(nameManager.getName(lambda)), lambda);
				        });

			// B) create an entries within info table containing code fragments, wrappers and prototypes
			for_each(lambdas, [&](const std::pair<c_ast::IdentifierPtr, core::LambdaExprPtr>& pair) {

				const c_ast::IdentifierPtr& name = pair.first;
				const core::LambdaExprPtr& lambda = pair.second;

				auto funType = lambda->getFunctionType();
				bool isMember = funType->isConstructor() || funType->isDestructor() || funType->isMemberFunction() || funType->isVirtualMemberFunction();

				// create information
				LambdaInfo* info = new LambdaInfo();
				info->prototype = declarations;
				info->definition = definitions;

				// member functions are declared within object definition
				c_ast::NamedCompositeTypePtr classDecl;
				if(isMember) {
					const auto& typeInfo = typeManager.getTypeInfo(funType->getObjectType());
					info->prototype = typeInfo.definition;
					classDecl = typeInfo.lValueType.as<c_ast::NamedCompositeTypePtr>();
					// add requirement of implementation
					info->prototype->addRequirement(info->definition);
				}

				// if not member and not recursive, skip prototype
				if(!isMember && !lambda->isRecursive()) {
					info->prototype = definitions;
				} else {
					definitions->addDependency(info->prototype);
				}

				// create dummy function ... no body
				core::LambdaPtr body;
				FunctionCodeInfo codeInfo = resolveFunction(name, funType, body, false);
				info->function = codeInfo.function;

				auto wrapper = resolveLambdaWrapper(codeInfo.function, funType, false);
				info->lambdaWrapperName = wrapper.first;
				info->lambdaWrapper = wrapper.second;
				info->lambdaWrapper->addDependency(info->prototype);
				info->lambdaWrapper->addRequirement(info->definition);

				// obtain current lambda and add lambda info
				auto res = funInfos.insert(std::make_pair(lambda, info));

				// if this info is new the same function has been handled while resolving the body
				if(!res.second) {
					// fun info was already there - delete local copy and be done
					delete info;
					return;
				}

				// add prototype ...
				if(isMember) {
					// add declaration
					if(funType.isConstructor()) {
						// add constructor
						auto ctor = cManager->create<c_ast::Constructor>(classDecl->name, info->function);
						classDecl->ctors.push_back(cManager->create<c_ast::ConstructorPrototype>(ctor));
					} else if(funType.isDestructor()) {
						// add destructor
						assert_false(classDecl->dtor) << "Destructor already defined!";
						auto dtor = cManager->create<c_ast::Destructor>(classDecl->name, info->function);
						auto decl = cManager->create<c_ast::DestructorPrototype>(dtor);
						decl->isVirtual = isVirtual;
						classDecl->dtor = decl;
					} else if(funType.isVirtualMemberFunction()) {
						//TODO
						assert_not_implemented();
					} else {
						// add member function
						assert_true(funType.isMemberFunction());
						auto mfun = cManager->create<c_ast::MemberFunction>(classDecl->name, info->function);
						auto decl = cManager->create<c_ast::MemberFunctionPrototype>(mfun);

						mfun->isConstant = isConst;
						decl->isVirtual = isVirtual;

						classDecl->members.push_back(decl);
					}

					// remove dependencies from others to this class (causes cyclic dependencies)
					for(const auto& dep : codeInfo.prototypeDependencies) {
						if(dep.isa<c_ast::IncludeFragmentPtr>()) { dep->remDependency(info->prototype); }
					}

					// add dependencies to class declaration
					info->prototype->addDependencies(codeInfo.prototypeDependencies);

					// add includes
					info->prototype->addIncludes(codeInfo.includes);

				} else {
					// ... to prototype block
					declarations->getCode().push_back(cManager->create<c_ast::FunctionPrototype>(codeInfo.function));
				}

				// import dependency from resolved code fragment
				declarations->addDependencies(codeInfo.prototypeDependencies);

				// add includes
				declarations->addIncludes(codeInfo.includes);
			});

			// C) create function definitions
			for_each(lambdas, [&](const std::pair<c_ast::IdentifierPtr, core::LambdaExprPtr>& pair) {

				const c_ast::IdentifierPtr& name = pair.first;
				const core::LambdaExprPtr& lambda = pair.second;

				// peel function and create function definition
				core::LambdaExprPtr unrolled = lambdaDefinition->peel(manager, lambda->getReference());
				assert_false(unrolled->isRecursive()) << "Peeled function must not be recursive!";

				// resolve function ... now with body
				const core::FunctionTypePtr& funType = static_pointer_cast<const core::FunctionType>(lambda->getType());
				FunctionCodeInfo codeInfo = resolveFunction(name, funType, unrolled->getLambda(), false, isConst);

				// add function
				LambdaInfo* info = static_cast<LambdaInfo*>(funInfos[lambda]);
				info->function = codeInfo.function;

				// add definition to definition block
				definitions->getCode().push_back(codeInfo.definition);

				// add code dependencies
				definitions->addDependencies(codeInfo.definitionDependencies);

				// add includes
				definitions->addIncludes(codeInfo.includes);
			});
		}

		namespace {

			core::NodePtr getAccessedField(const core::VariablePtr& thisVar, const core::ExpressionPtr& candidate) {
				static const core::NodePtr NO_ACCESS;

				// check whether it is accessing an element
				if(candidate->getNodeType() != core::NT_CallExpr) { return NO_ACCESS; }
				core::CallExprPtr call = candidate.as<core::CallExprPtr>();

				// check whether it is a field access
				const auto& refs = thisVar->getNodeManager().getLangExtension<core::lang::ReferenceExtension>();
				if(core::analysis::isCallOf(call, refs.getRefAssign())) {
					core::ExpressionPtr target = call->getArgument(0);
					if(auto refAccess = refs.isCallOfRefMemberAccess(target)) {
						// check whether it is accessing this
						if(auto deref = refs.isCallOfRefDeref(refAccess[0])) {
							if(deref[0] != thisVar) { return NO_ACCESS; }

							// extract identifier name
							return refAccess[1];
						}
					}
				}

				// check whether it is a super-constructor call
				auto funType = call->getFunctionExpr()->getType().as<core::FunctionTypePtr>();
				if(funType->isConstructor()) {
					auto target = call->getArgument(0);

					// test whether argument is this (super-constructor call)
					if(auto deref = refs.isCallOfRefDeref(target)) {
						if(deref[0] == thisVar) { return funType->getObjectType(); }
					}

					// test whether argument is a member (member initializer)
					if(auto refAccess = refs.isCallOfRefMemberAccess(target)) {
						// check whether it is accessing this
						if(auto deref = refs.isCallOfRefDeref(refAccess[0])) {
							if(deref[0] != thisVar) { return NO_ACCESS; }

							// extract identifier name
							return refAccess[1];
						}
					}
				}

				return NO_ACCESS;
			}

			bool valuesDerivedFromParametersOnly(const core::VariablePtr& thisVar, const core::VariableList& params, const core::CallExprPtr& call) {
				assert_true(getAccessedField(thisVar, call)) << "not an access!";

				// collect values
				core::ExpressionList values;
				{
					// in case it is an assignment
					const auto& refExt = call->getNodeManager().getLangExtension<core::lang::ReferenceExtension>();
					if(core::analysis::isCallOf(call, refExt.getRefAssign())) {
						values.push_back(call->getArgument(1)); // that is the value
					} else {
						// it is a constructor call => collect all arguments but the first
						values.insert(values.end(), call->begin() + 1, call->end());
					}
				}

				// check variables within values
				bool parametersOnly = true;

				// build up a checker
				auto check = core::makeCachedLambdaVisitor([&](const core::NodePtr& cur) -> bool {
					static const bool PRUNE = true;
					static const bool CONTINUE = false;

					// see whether a problem has been found before
					if(!parametersOnly) return PRUNE;

					// do not enter nested scopes
					if(cur.getNodeType() == core::NT_LambdaExpr) return PRUNE;

					// only interested in variables
					if(cur.getNodeType() != core::NT_Variable) return CONTINUE;

					// check the variable
					auto curVar = cur.as<core::VariablePtr>();
					// we want only parameters, but make an exception if the thisVar is used
					if(!contains(params, curVar) && (curVar != thisVar)) {
						parametersOnly = false;
						return PRUNE;
					}

					// no problem, continue search
					return CONTINUE;
				}, false);

				// check all the values
				for(const auto& cur : values) {
					if(parametersOnly) { core::visitDepthFirstOncePrunable(cur, check); }
				}

				return parametersOnly;
			}


			struct FirstWriteCollector : public core::IRVisitor<void, core::Address, const core::VariablePtr&, const core::VariableList&, core::NodeSet&,
			                                                    std::vector<core::StatementAddress>&, bool> {
				std::vector<core::StatementAddress> collect(const core::VariablePtr& thisVar, const core::VariableList& params,
				                                            const core::CompoundStmtAddress& body) {
					// prepare context information
					core::NodeSet touched;
					std::vector<core::StatementAddress> res;

					// use visitor infrastructure
					visit(body, thisVar, params, touched, res, false);

					// return result list
					return res;
				}

				void visitCompoundStmt(const core::CompoundStmtAddress& cur, const core::VariablePtr& thisVar, const core::VariableList& params,
				                       core::NodeSet& touched, std::vector<core::StatementAddress>& res, bool iterating) {
					// iterate through sub-statements
					visitAll(cur->getChildList(), thisVar, params, touched, res, iterating);
				}

				void visitIfStmt(const core::IfStmtAddress& cur, const core::VariablePtr& thisVar, const core::VariableList& params, core::NodeSet& touched,
				                 std::vector<core::StatementAddress>& res, bool iterating) {
					// iterate through sub-statements
					visitAll(cur->getChildList(), thisVar, params, touched, res, iterating);
				}

				void visitSwitchStmt(const core::SwitchStmtAddress& cur, const core::VariablePtr& thisVar, const core::VariableList& params,
				                     core::NodeSet& touched, std::vector<core::StatementAddress>& res, bool iterating) {
					// iterate through sub-statements
					visitAll(cur->getChildList(), thisVar, params, touched, res, iterating);
				}

				void visitSwitchCases(const core::SwitchCasesAddress& cur, const core::VariablePtr& thisVar, const core::VariableList& params,
				                      core::NodeSet& touched, std::vector<core::StatementAddress>& res, bool iterating) {
					// iterate through sub-statements
					visitAll(cur->getChildList(), thisVar, params, touched, res, iterating);
				}

				void visitSwitchCase(const core::SwitchCaseAddress& cur, const core::VariablePtr& thisVar, const core::VariableList& params,
				                     core::NodeSet& touched, std::vector<core::StatementAddress>& res, bool iterating) {
					// iterate through sub-statements
					visitAll(cur->getChildList(), thisVar, params, touched, res, iterating);
				}

				void visitForStmt(const core::ForStmtAddress& cur, const core::VariablePtr& thisVar, const core::VariableList& params, core::NodeSet& touched,
				                  std::vector<core::StatementAddress>& res, bool iterating) {
					// iterate through sub-statements
					visitAll(cur->getChildList(), thisVar, params, touched, res, true);
				}

				void visitWhileStmt(const core::WhileStmtAddress& cur, const core::VariablePtr& thisVar, const core::VariableList& params,
				                    core::NodeSet& touched, std::vector<core::StatementAddress>& res, bool iterating) {
					// iterate through sub-statements
					visitAll(cur->getChildList(), thisVar, params, touched, res, true);
				}

				void visitDeclarationStmt(const core::DeclarationStmtAddress& cur, const core::VariablePtr& thisVar, const core::VariableList& params,
				                          core::NodeSet& touched, std::vector<core::StatementAddress>& res, bool iterating) {
					// we can stop here
				}

				void visitTryCatchStmt(const core::TryCatchStmtAddress& cur, const core::VariablePtr& thisVar, const core::VariableList& params,
				                       core::NodeSet& touched, std::vector<core::StatementAddress>& res, bool iterating) {
					// we can stop here
				}

				void visitCallExpr(const core::CallExprAddress& cur, const core::VariablePtr& thisVar, const core::VariableList& params, core::NodeSet& touched,
				                   std::vector<core::StatementAddress>& res, bool iterating) {
					// extract field
					auto field = getAccessedField(thisVar, cur);
					if(!field) {
						return; // not accessing a field
					}

					// check whether field has been touched before
					if(touched.contains(field)) { return; }

					// mark field as being touched
					touched.insert(field);

					// we must not be inside a loop
					if(iterating) { return; }

					// check whether value is only depending on input parameters
					if(!valuesDerivedFromParametersOnly(thisVar, params, cur)) { return; }

					// we have found a first assign
					res.push_back(cur);
				}

				void visitStatement(const core::StatementAddress& cur, const core::VariablePtr& thisVar, const core::VariableList& params,
				                    core::NodeSet& touched, std::vector<core::StatementAddress>& res, bool iterating) {
					// terminate decent here!
				}

				void visitNode(const core::NodeAddress& cur, const core::VariablePtr& thisVar, const core::VariableList& params, core::NodeSet& touched,
				               std::vector<core::StatementAddress>& res, bool iterating) {
					std::cerr << "\n\n --------------------- ASSERTION ERROR -------------------\n";
					std::cerr << "Node of type " << cur->getNodeType() << " should not be reachable!\n";
					assert_fail() << "Must not be reached!";
				}
			};

			c_ast::IdentifierPtr getIdentifierFor(const Converter& converter, const core::NodePtr& node) {
				auto mgr = converter.getCNodeManager();
				switch(node->getNodeType()) {
				case core::NT_TagType:
				case core::NT_GenericType: {
					auto type = converter.getTypeManager().getTypeInfo(node.as<core::TypePtr>());
					if(auto structType = type.lValueType.isa<c_ast::StructTypePtr>()) {
						return mgr->create(toString(*structType));
					} else if(auto namedType = type.lValueType.isa<c_ast::NamedTypePtr>()) {
						return mgr->create(toString(*namedType));
					}
					return mgr->create(converter.getNameManager().getName(node));
				}
				case core::NT_Parent: {
					auto type = converter.getTypeManager().getTypeInfo(node.as<core::ParentPtr>()->getType());
					if(auto structType = type.lValueType.isa<c_ast::StructTypePtr>()) {
						return mgr->create(toString(*structType));
					} else if(auto namedType = type.lValueType.isa<c_ast::NamedTypePtr>()) {
						return mgr->create(toString(*namedType));
					}
					return mgr->create(converter.getNameManager().getName(node.as<core::ParentPtr>()->getType()));
				}
				case core::NT_Field: return mgr->create(node.as<core::FieldPtr>()->getName()->getValue());
				case core::NT_Literal:
					assert(node->getNodeManager().getLangBasic().isIdentifier(node.as<core::ExpressionPtr>()->getType()));
					return mgr->create(node.as<core::LiteralPtr>()->getStringValue());
				default: {}
				}

				std::cerr << "\n\n --------------------- ASSERTION ERROR -------------------\n";
				std::cerr << "Node of type " << node->getNodeType() << " should not be reachable!\n";
				assert_fail() << "Must not be reached!";
				return c_ast::IdentifierPtr();
			}

			std::pair<c_ast::Constructor::InitializationList, core::CompoundStmtPtr> extractInitializer(const Converter& converter, const core::LambdaPtr& ctor,
			                                                                                            ConversionContext& context) {
				auto mgr = converter.getCNodeManager();

				// collect first assignments to fields from body
				c_ast::Constructor::InitializationList initializer;

				// obtain class type
				core::TagTypePtr classType = getClassType(ctor->getType());

				// obtain list of parameters
				core::VariableList params(ctor->getParameters().begin() + 1, ctor->getParameters().end());

				// get list of all parents and fields
				std::vector<c_ast::IdentifierPtr> all;
				if (classType->isStruct()) {
					for(const core::ParentPtr& cur : classType->getStruct()->getParents()) {
						all.push_back(getIdentifierFor(converter, cur));
					}
				}
				for(const core::FieldPtr& cur : classType->getFields()) {
					all.push_back(getIdentifierFor(converter, cur));
				}

				// collect all first write operations only depending on parameters
				auto thisVar = ctor->getParameters()[0];
				core::CompoundStmtAddress oldBody(ctor->getBody());
				auto firstWriteOps = FirstWriteCollector().collect(thisVar, params, oldBody);

				// stop here if there is nothing to do
				if(firstWriteOps.empty()) { return std::make_pair(initializer, oldBody); }

				// remove assignments from body
				core::CompoundStmtPtr newBody = core::transform::remove(ctor->getNodeManager(), firstWriteOps).as<core::CompoundStmtPtr>();

				// assemble initializer list in correct order
				const auto& refExt = thisVar->getNodeManager().getLangExtension<core::lang::ReferenceExtension>();
				for(const c_ast::IdentifierPtr& cur : all) {
					for(const auto& write : firstWriteOps) {
						// check whether write target is current identifier
						core::CallExprPtr call = write.as<core::CallExprPtr>();
						if(cur == getIdentifierFor(converter, getAccessedField(thisVar, call))) {
							// add filed assignment
							if(core::analysis::isCallOf(call, refExt.getRefAssign())) {
								// avoid default inits, those will be done anyway
								if(core::analysis::isCallOf(call[1], call->getNodeManager().getLangBasic().getZero())) {
									continue;
								}

								c_ast::NodePtr value = converter.getStmtConverter().convertExpression(context, call[1]);
								initializer.push_back(c_ast::Constructor::InitializerListEntry(cur, toVector(value)));
							} else {
								// otherwise it needs to be a constructor
								assert(call->getFunctionExpr()->getType().as<core::FunctionTypePtr>()->isConstructor());

								// avoid default inits, those will be done anyway
								if(core::analysis::isCallOf(call, call->getNodeManager().getLangBasic().getZero())) {
									continue;
								}

								c_ast::ExpressionPtr initCall = converter.getStmtConverter().convertExpression(context, call);

								if(initCall->getNodeType() != c_ast::NT_ConstructorCall) {
									assert_eq(initCall->getNodeType(), c_ast::NT_UnaryOperation);
									initCall = initCall.as<c_ast::UnaryOperationPtr>()->operand.as<decltype(initCall)>();
								}

								// convert constructor call as if it would be an in-place constructor (resolves dependencies!)
								assert_eq(initCall->getNodeType(), c_ast::NT_ConstructorCall);
								auto ctorCall = initCall.as<c_ast::ConstructorCallPtr>();
								// add constructor call to initializer list
								initializer.push_back(c_ast::Constructor::InitializerListEntry(cur, ctorCall->arguments));
							}
						}
					}
				}

				// return result
				return std::make_pair(initializer, newBody);
			}


		} // end namespace


		FunctionCodeInfo FunctionInfoStore::resolveFunction(const c_ast::IdentifierPtr name, const core::FunctionTypePtr& funType,
		                                                    const core::LambdaPtr& lambda, bool external, bool isConst) {
			FunctionCodeInfo res;

			// get C node manager
			auto manager = converter.getCNodeManager();

			// get other managers
			TypeManager& typeManager = converter.getTypeManager();
			NameManager& nameManager = converter.getNameManager();

			// check whether this is a member function
			bool isMember = funType->isConstructor() || funType->isDestructor() || funType->isMemberFunction() || funType->isVirtualMemberFunction();

			// resolve return type
			const TypeInfo& returnTypeInfo = typeManager.getTypeInfo(funType->getReturnType());
			res.prototypeDependencies.insert(returnTypeInfo.definition);
			res.definitionDependencies.insert(returnTypeInfo.definition);
			c_ast::TypePtr returnType = (external) ? returnTypeInfo.externalType : returnTypeInfo.rValueType;

			// create a new variable scope for the resolution of the body
			nameManager.pushVarScope(true);

			// resolve parameters
			unsigned counter = 0;
			vector<c_ast::VariablePtr> parameter;
			for_each(funType->getParameterTypes()->getElements(), [&](const core::TypePtr& cur) {

				// skip type literals passed as arguments
				if(core::analysis::isTypeLiteralType(cur)) {
					counter++;
					return;
				}

				// resolve parameter type
				const TypeInfo& paramTypeInfo = typeManager.getTypeInfo(cur);
				res.prototypeDependencies.insert(paramTypeInfo.definition);
				res.definitionDependencies.insert(paramTypeInfo.definition);

				c_ast::TypePtr paramType = (external) ? paramTypeInfo.externalType : paramTypeInfo.rValueType;

				string paramName;
				if(lambda) {
					if(isMember && counter == 0) {
						// first parameter of member functions is this!
						paramName = "this";
						nameManager.setName(lambda->getParameterList()[counter], paramName);
					} else {
						if(!converter.getNodeManager().getLangExtension<core::lang::VarArgsExtension>().isVarList(cur) || counter < lambda->getParameterList().size()) {
							paramName = nameManager.getName(lambda->getParameterList()[counter]);
						}
					}
				} else {
					paramName = format("p%d", counter + 1);
				}
				parameter.push_back(c_ast::var(paramType, manager->create(paramName)));

				counter++;
			});

			// resolve body
			c_ast::StatementPtr cBody;
			c_ast::Constructor::InitializationList initializer;

			if(lambda) {
				// set up variable manager
				ConversionContext context(converter, lambda);
				for_each(lambda->getParameterList(), [&](const core::VariablePtr& cur) {
					context.getVariableManager().addInfo(converter, cur, VariableInfo::DIRECT);
				});

				core::CompoundStmtPtr body = lambda->getBody();

				// extract initializer list
				if(funType->isConstructor()) {
					// collect initializer values + remove assignments from body
					std::tie(initializer, body) = extractInitializer(converter, lambda, context);
				}

				// convert the body code fragment and collect dependencies
				c_ast::NodePtr code = converter.getStmtConverter().convert(context, body);
				cBody = static_pointer_cast<c_ast::Statement>(code);
				res.definitionDependencies.insert(context.getDependencies().begin(), context.getDependencies().end());

				// also attach includes
				res.includes = context.getIncludes();
			}

			// drop nested variable scope
			nameManager.popVarScope();

			// create function
			res.function = manager->create<c_ast::Function>(returnType, name, parameter, cBody);
			res.definition = res.function;

			// a lazy-evaluated utility to obtain the name of a class a member function is associated to
			auto getClassName = [&]() -> c_ast::IdentifierPtr {

				const auto& type = typeManager.getTypeInfo(funType->getObjectType()).lValueType;

				if(const auto& tagType = type.isa<c_ast::NamedCompositeTypePtr>()) { return tagType->name; }

				if(const auto& namedType = type.isa<c_ast::NamedTypePtr>()) { return namedType->name; }
				std::cerr << "Unable to determine class-name for member function: " << funType << "\n";
				assert_fail() << "Unsupported case!";
				return c_ast::IdentifierPtr();
			};

			// modify function if required
			if(funType->isMemberFunction()) {
				// update definition to define a member function
				res.definition = manager->create<c_ast::MemberFunction>(getClassName(), res.function, isConst);

			} else if(funType->isConstructor()) {
				// update definition to define a member function
				res.definition = manager->create<c_ast::Constructor>(getClassName(), res.function, initializer);

			} else if(funType->isDestructor()) {
				// update definition to define a member function
				res.definition = manager->create<c_ast::Destructor>(getClassName(), res.function);

				//TODO
			} else if(funType->isVirtualMemberFunction()) {
				assert_not_implemented();
			}
			return res;
		}

		std::pair<c_ast::IdentifierPtr, c_ast::CodeFragmentPtr> FunctionInfoStore::resolveLambdaWrapper(const c_ast::FunctionPtr& function,
		                                                                                                const core::FunctionTypePtr& funType, bool external) {
			// get C node manager
			auto manager = converter.getCNodeManager();

			// obtain function type information
			core::FunctionTypePtr closureType =
			    core::FunctionType::get(funType->getNodeManager(), funType->getParameterTypes(), funType->getReturnType(), core::FK_CLOSURE);
			TypeManager& typeManager = converter.getTypeManager();
			const FunctionTypeInfo& funTypeInfo = typeManager.getTypeInfo(closureType);

			// create a new function representing the wrapper

			// create list of parameters for wrapper
			vector<c_ast::VariablePtr> parameter;

			// first parameter is the closure
			parameter.push_back(c_ast::var(funTypeInfo.rValueType, manager->create("closure")));

			// resolve parameters
			int counter = 1;
			::transform(funType->getParameterTypes()->getElements(), std::back_inserter(parameter), [&](const core::TypePtr& cur) {
				const TypeInfo& paramTypeInfo = typeManager.getTypeInfo(cur);
				return c_ast::var(paramTypeInfo.rValueType, manager->create(format("p%d", counter++)));
			});

			// pick a name for the wrapper
			c_ast::IdentifierPtr name = manager->create(function->name->name + "_wrap");

			// create a function body (call to the function including wrappers)
			c_ast::CallPtr call = manager->create<c_ast::Call>(function->name);

			// filter out type literal parameters
			vector<core::TypePtr> paramTypes;
			for_each(funType->getParameterTypes()->getElements(), [&](const core::TypePtr& cur) {
				if(!core::analysis::isTypeLiteralType(cur)) { paramTypes.push_back(cur); }
			});

			// add parameters for wrapper
			for(const auto& cur : make_paired_range(paramTypes, function->parameter)) {
				if(external) {
					assert_true(typeManager.getTypeInfo(cur.first).externalize) << "Missing externalizer for type " << *cur.first;
					call->arguments.push_back(typeManager.getTypeInfo(cur.first).externalize(manager, cur.second));
				} else {
					call->arguments.push_back(cur.second);
				}
			}

			c_ast::StatementPtr body = typeManager.getTypeInfo(closureType->getReturnType()).internalize(manager, call);
			if(!c_ast::isVoid(function->returnType)) { body = manager->create<c_ast::Return>(call); }

			c_ast::FunctionPtr wrapper = manager->create<c_ast::Function>(function->returnType, name, parameter, body);

			c_ast::CodeFragmentPtr res = c_ast::CCodeFragment::createNew(converter.getFragmentManager(), wrapper);
			res->addDependency(funTypeInfo.definition);

			return std::make_pair(name, res);
		}
	}

	FunctionIncludeTable getBasicFunctionIncludeTable() {
		// the basic include table is empty
		return FunctionIncludeTable();
	}

} // end namespace backend
} // end namespace insieme

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

#include "insieme/analysis/cba/prototype.h"

#include "insieme/core/ir_visitor.h"
#include "insieme/core/ir_address.h"

#include "insieme/core/analysis/ir_utils.h"
#include "insieme/core/arithmetic/arithmetic_utils.h"

namespace insieme {
namespace analysis {
namespace cba {

	using std::set;
	using std::vector;

	const TypedSetType<core::ExpressionAddress> C("C");
	const TypedSetType<core::ExpressionAddress> c("c");

	const TypedSetType<core::ExpressionPtr> D("D");
	const TypedSetType<core::ExpressionPtr> d("d");

	typedef std::tuple<core::ExpressionAddress, Context, Thread> Location;
	const TypedSetType<Location> R("R");
	const TypedSetType<Location> r("r");

	const TypedSetType<Formula> A("A");
	const TypedSetType<Formula> a("a");

	const TypedSetType<bool> B("B");
	const TypedSetType<bool> b("b");

	using namespace core;

	VariableAddress getDefinitionPoint(const VariableAddress& varAddress) {

		// extract the variable
		VariablePtr var = varAddress.getAddressedNode();

		// start walking up the address
		NodeAddress cur = varAddress;

		// check the parent
		while (!cur.isRoot()) {
			auto pos = cur.getIndex();
			cur = cur.getParentAddress();
			switch(cur->getNodeType()) {

			case NT_Parameters: {
				return varAddress;	// this variable is a parameter definition
			}

			case NT_Lambda: {

				// check parameters
				for(auto param : cur.as<LambdaAddress>()->getParameters()) {
					if (param.as<VariablePtr>() == var) {
						return param;		// found it
					}
				}

				// otherwise continue with parent
				break;
			}

			case NT_LambdaBinding: {
				// check the bound variable
				auto boundVar = cur.as<LambdaBindingAddress>()->getVariable();
				if (boundVar.as<VariablePtr>() == var) {
					return boundVar;
				}

				// keep on searching
				break;
			}

			case NT_BindExpr: {
				// check parameters
				for(auto param : cur.as<BindExprAddress>()->getParameters()) {
					if (param.as<VariablePtr>() == var) {
						return param;		// found it
					}
				}

				// not here
				break;
			}

			case NT_CompoundStmt: {

				// check whether there is an earlier declaration
				auto compound = cur.as<CompoundStmtAddress>();
				for(int i = pos; i >= 0; i--) {
					if (auto decl = compound[i].isa<DeclarationStmtAddress>()) {
						if (decl->getVariable().as<VariablePtr>() == var) {
							return decl->getVariable();
						}
					}
				}

				// otherwise continue with parent
				break;
			}

			default: break;
			}
		}

		// the variable is a free variable in this context
		return VariableAddress(var);
	}


	bool isMemoryConstructor(const StatementAddress& address) {
		StatementPtr stmt = address;

		// literals of a reference type are memory locations
		if (auto lit = stmt.isa<LiteralPtr>()) {
			return lit->getType().isa<RefTypePtr>();
		}

		// memory allocation calls are
		return core::analysis::isCallOf(stmt, stmt->getNodeManager().getLangBasic().getRefAlloc());
	}

	ExpressionAddress getLocationDefinitionPoint(const core::StatementAddress& stmt) {
		assert(isMemoryConstructor(stmt));

		// globals are globals => always the same
		if (auto lit = stmt.isa<LiteralPtr>()) {
			return LiteralAddress(lit);
		}

		// locations created by ref.alloc calls are created at the call side
		assert(stmt.isa<CallExprAddress>());
		return stmt.as<CallExprAddress>();
	}


	namespace {

		LambdaAddress getEnclosingLambda(const NodeAddress& addr) {
			// find lambda body
			NodeAddress cur = addr;
			while(!cur.isRoot() && !cur.isa<LambdaPtr>()) {
				cur = cur.getParentAddress();
			}
			return cur.isa<LambdaAddress>();
		}

		using namespace utils::set_constraint_2;

		template<typename T>
		class BasicDataFlowConstraintCollector : public IRVisitor<void, Address> {

			typedef IRVisitor<void,Address> super;

			set<NodeAddress> processed;

		protected:

			CBAContext& context;
			Constraints& constraints;

			// the list of all terms in the targeted code
			vector<ExpressionAddress> terms;

			// the two set types to deal with
			const TypedSetType<T>& A;		// the value set (labels -> values)
			const TypedSetType<T>& a;		// the variable set (variables -> values)

		public:

			BasicDataFlowConstraintCollector(CBAContext& context, Constraints& contraints, const StatementAddress& root, const TypedSetType<T>& A, const TypedSetType<T>& a)
				: processed(), context(context), constraints(contraints), A(A), a(a) {

				// collect all terms in the code
				visitDepthFirst(root, [&](const ExpressionAddress& cur) {
					if (cur.isa<LambdaExprPtr>() || cur.isa<BindExprPtr>()) {
						// TODO: also add all recursion variations
						terms.push_back(cur);
					}
				});

			};

			virtual void visit(const NodeAddress& node) {
				if (!processed.insert(node).second) return;
				super::visit(node);
			}

			void visitCompoundStmt(const CompoundStmtAddress& compound) {
				// just collect constraints from elements
				// TODO: add data flow constraints
				for(auto cur : compound) visit(cur);
			}

			void visitDeclarationStmt(const DeclarationStmtAddress& decl) {

				// add constraint r(var) \subset C(init)
				auto var = context.getVariable(decl->getVariable());
				auto l_init = context.getLabel(decl->getInitialization());

				// TODO: distinguish between control and data flow!
				auto a_var = context.getSet(a, var);
				auto A_init = context.getSet(A, l_init);
				constraints.add(subset(A_init, a_var));		// TODO: add context (passed by argument)

				// finally, add constraints for init expression
				visit(decl->getInitialization());
			}

			void visitIfStmt(const IfStmtAddress& stmt) {

				// decent into sub-expressions
				visit(stmt->getCondition());
				visit(stmt->getThenBody());
				visit(stmt->getElseBody());

			}

			void visitWhileStmt(const WhileStmtAddress& stmt) {

				// decent into sub-expressions
				visit(stmt->getCondition());
				visit(stmt->getBody());
			}

			void visitReturnStmt(const ReturnStmtAddress& stmt) {

				// link the value of the result set to lambda body

				// find lambda body
				LambdaAddress lambda = getEnclosingLambda(stmt);
				if (!lambda) {
					std::cout << "Encountered free return!!\n";
					return;		// return is not bound
				}

				// and add constraints for return value
				visit(stmt->getReturnExpr());

				auto l_retVal = context.getLabel(stmt->getReturnExpr());
				auto l_body = context.getLabel(lambda->getBody());

				auto A_retVal = context.getSet(A, l_retVal);
				auto A_body = context.getSet(A, l_body);

				// add constraint
				constraints.add(subset(A_retVal, A_body));

			}

			void visitLiteral(const LiteralAddress& literal) {
				// nothing to do by default => should be overloaded by sub-classes
			}

			void visitVariable(const VariableAddress& variable) {

				// add constraint r(var) \subset C(var)
				auto var = context.getVariable(variable);
				auto l_var = context.getLabel(variable);

				auto a_var = context.getSet(a, var);
				auto A_var = context.getSet(A, l_var);

				constraints.add(subset(a_var, A_var));
			}

			void visitLambdaExpr(const LambdaExprAddress& lambda) {
				// TODO: handle recursions

				// and add constraints for the body
				visit(lambda->getBody());
			}

			void visitCallExpr(const CallExprAddress& call) {

				// add constraints for function and argument expressions
				visit(call->getFunctionExpr());
				for(auto arg : call) visit(arg);

				// get values of function
				auto fun = call->getFunctionExpr();
				auto C_fun = context.getSet(C, context.getLabel(fun));

				// value set of call
				auto A_call = context.getSet(A, context.getLabel(call));

				// a utility resolving constraints for the given expression
				auto addConstraints = [&](const ExpressionAddress& expr, bool fixed) {

					// only searching for actual code
					if (!expr.isa<LambdaExprPtr>() && !expr.isa<BindExprPtr>()) return;

					// check whether the term is a function with the right number of arguments
					// TODO: also check type?
					auto funType = expr.as<ExpressionPtr>()->getType().isa<FunctionTypePtr>();
					if(funType->getParameterTypes().size() != call.size()) return;		// this is not a potential function

					assert(expr.isa<LambdaExprPtr>() && "Binds not implemented yet!");

					// add constraints for arguments
					auto lambda = expr.isa<LambdaExprAddress>();
					for(std::size_t i=0; i<call.size(); i++) {

						// add constraint: t \in C(fun) => C(arg) \subset r(param)
						auto l_arg = context.getLabel(call[i]);
						auto param = context.getVariable(lambda->getParameterList()[i]);

						auto A_arg = context.getSet(A, l_arg);
						auto a_param = context.getSet(a, param);
						constraints.add((fixed) ? subset(A_arg, a_param) : subsetIf(expr, C_fun, A_arg, a_param));
					}

					// add constraint for result value
					auto l_ret = context.getLabel(lambda->getBody());
					auto A_ret = context.getSet(A, l_ret);
					constraints.add((fixed)? subset(A_ret, A_call) : subsetIf(expr, C_fun, A_ret, A_call));

				};

				// no constraints for literals ...
				if (fun.isa<LiteralPtr>()) return;

				// if function expression is a lambda or bind => do not iterate through all terms, term is fixed
				if (fun.isa<LambdaExprPtr>() || fun.isa<BindExprPtr>()) {
					addConstraints(fun, true);
					return;
				}

				// fix pass-by-value semantic - by considering all potential terms
				for(auto cur : terms) {
					addConstraints(cur, false);
				}
			}

			void visitNode(const NodeAddress& node) {
				std::cout << "Reached unsupported Node Type: " << node->getNodeType() << "\n";
				assert(false);
			}

		};


		class ControlFlowConstraintCollector : public BasicDataFlowConstraintCollector<core::ExpressionAddress> {

			typedef BasicDataFlowConstraintCollector<core::ExpressionAddress> super;

		public:

			ControlFlowConstraintCollector(CBAContext& context, Constraints& constraints, const StatementAddress& root)
				: BasicDataFlowConstraintCollector<core::ExpressionAddress>(context, constraints, root, C, c) { };

			void visitLiteral(const LiteralAddress& literal) {

				// and default handling
				super::visitLiteral(literal);

				// only interrested in functions ...
				if (!literal->getType().isa<FunctionTypePtr>()) return;

				// add constraint literal \in C(lit)
				auto value = literal.as<ExpressionAddress>();
				auto l_lit = context.getLabel(literal);

				auto C_lit = context.getSet(C, l_lit);
				constraints.add(elem(value, C_lit));

			}

			void visitLambdaExpr(const LambdaExprAddress& lambda) {

				// and default handling
				super::visitLambdaExpr(lambda);

				// add constraint lambda \in C(lambda)
				auto value = lambda.as<ExpressionAddress>();
				auto label = context.getLabel(lambda);

				constraints.add(elem(value, context.getSet(C, label)));

				// TODO: handle recursions

			}

		};

		class ConstantConstraintCollector : public BasicDataFlowConstraintCollector<core::ExpressionPtr> {

			typedef BasicDataFlowConstraintCollector<core::ExpressionPtr> super;

		public:

			ConstantConstraintCollector(CBAContext& context, Constraints& constraints, const StatementAddress& root)
				: BasicDataFlowConstraintCollector<core::ExpressionPtr>(context, constraints, root, D, d) { };

			void visitLiteral(const LiteralAddress& literal) {

				// and default handling
				super::visitLiteral(literal);

				// not interested in functions
				if (literal->getType().isa<FunctionTypePtr>()) return;

				// add constraint literal \in C(lit)
				auto value = literal.as<ExpressionPtr>();
				auto l_lit = context.getLabel(literal);

				auto D_lit = context.getSet(D, l_lit);
				constraints.add(elem(value, D_lit));

			}

			void visitCallExpr(const CallExprAddress& call) {
				auto& base = call->getNodeManager().getLangBasic();

				// conduct std-procedure
				super::visitCallExpr(call);

				// some special cases
				if (base.isIntArithOp(call->getFunctionExpr())) {

					// mark result as being unknown
					auto D_call = context.getSet(D, context.getLabel(call));
					constraints.add(elem(ExpressionPtr(), D_call));

				}

			}

		};


		namespace {

			template<typename A, typename B, typename R>
			class total_binary_op {
				typedef std::function<R(const A&, const B&)> fun_type;
				fun_type fun;
			public:
				total_binary_op(const fun_type& fun) : fun(fun) {}

				R operator()(const A& a, const B& b) const {
					static const R fail;
					if (!a) return fail;
					if (!b) return fail;
					return fun(a,b);
				}
			};

			template<
				typename F,
				typename A = typename std::remove_cv<typename std::remove_reference<typename lambda_traits<F>::arg1_type>::type>::type,
				typename B = typename std::remove_cv<typename std::remove_reference<typename lambda_traits<F>::arg2_type>::type>::type,
				typename R = typename lambda_traits<F>::result_type
			>
			total_binary_op<A,B,R> total(const F& fun) {
				return total_binary_op<A,B,R>(fun);
			}

			template<typename A, typename B, typename R>
			class cartesion_product_binary_op {

				typedef std::function<R(const A&, const B&)> fun_type;
				fun_type fun;

			public:

				cartesion_product_binary_op(const fun_type& fun) : fun(fun) {}

				set<R> operator()(const set<A>& a, const set<B>& b) const {
					set<R> res;

					// if there is any undefined included => it is undefined
					if (any(a, [](const A& a)->bool { return !a; }) || any(b, [](const B& b)->bool { return !b; })) {
						res.insert(R());
						return res;
					}

					// compute teh cross-product
					for(auto& x : a) {
						for (auto& y : b) {
							res.insert(fun(x,y));
						}
					}

					if (res.size() > 10) {
						// build a set only containing the unknown value
						set<R> res;
						res.insert(R());
						return res;
					}

					return res;
				}

			};

			template<
				typename F,
				typename A = typename std::remove_cv<typename std::remove_reference<typename lambda_traits<F>::arg1_type>::type>::type,
				typename B = typename std::remove_cv<typename std::remove_reference<typename lambda_traits<F>::arg2_type>::type>::type,
				typename R = typename lambda_traits<F>::result_type
			>
			cartesion_product_binary_op<A,B,R> cartesion_product(const F& fun) {
				return cartesion_product_binary_op<A,B,R>(fun);
			}


		}


		class ArithmeticConstraintCollector : public BasicDataFlowConstraintCollector<Formula> {

			typedef BasicDataFlowConstraintCollector<Formula> super;

			const core::lang::BasicGenerator& base;

		public:

			ArithmeticConstraintCollector(CBAContext& context, Constraints& constraints, const StatementAddress& root)
				: BasicDataFlowConstraintCollector<Formula>(context, constraints, root, cba::A, cba::a), base(root->getNodeManager().getLangBasic()) { };

			void visitLiteral(const LiteralAddress& literal) {

				// and default handling
				super::visitLiteral(literal);

				// only interested in integer literals
				if (!base.isInt(literal->getType())) return;

				// add constraint literal \in A(lit)
				Formula value = core::arithmetic::toFormula(literal);
				auto l_lit = context.getLabel(literal);

				auto A_lit = context.getSet(A, l_lit);
				constraints.add(elem(value, A_lit));

			}


			void visitCallExpr(const CallExprAddress& call) {
				static const Formula unknown;

				// conduct std-procedure
				super::visitCallExpr(call);

				// only care for integer expressions calling literals
				if (!base.isInt(call->getType())) return;

				// check whether it is a literal => otherwise basic data flow is handling it
				auto fun = call->getFunctionExpr();
				if (!fun.isa<LiteralPtr>()) return;

				// get some labels / ids
				auto A_res = context.getSet(A, context.getLabel(call));

				// handle unary literals
				if (call.size() == 1u) {
					if (base.isRefDeref(fun)) {
						return;		// has been handled by super!
					}
				}

				// and binary operators
				if (call.size() != 2u) {
					// this value is unknown
					constraints.add(elem(unknown, A_res));
					return;
				}

				// get sets for operators
				auto A_lhs = context.getSet(A, context.getLabel(call[0]));
				auto A_rhs = context.getSet(A, context.getLabel(call[1]));

				// special handling for functions
				if (base.isSignedIntAdd(fun) || base.isUnsignedIntAdd(fun)) {
					constraints.add(subsetBinary(A_lhs, A_rhs, A_res, cartesion_product(total([](const Formula& a, const Formula& b)->Formula {
						return *a.formula + *b.formula;
					}))));
					return;
				}

				if (base.isSignedIntSub(fun) || base.isUnsignedIntSub(fun)) {
					constraints.add(subsetBinary(A_lhs, A_rhs, A_res, cartesion_product(total([](const Formula& a, const Formula& b)->Formula {
						return *a.formula - *b.formula;
					}))));
					return;
				}

				if (base.isSignedIntMul(fun) || base.isUnsignedIntMul(fun)) {
					constraints.add(subsetBinary(A_lhs, A_rhs, A_res, cartesion_product(total([](const Formula& a, const Formula& b)->Formula {
						return *a.formula * *b.formula;
					}))));
					return;
				}

				// otherwise it is unknown
				constraints.add(elem(unknown, A_res));
			}

		};

		namespace {

			template<
				typename F,
				typename A = typename std::remove_cv<typename std::remove_reference<typename lambda_traits<F>::arg1_type>::type>::type,
				typename B = typename std::remove_cv<typename std::remove_reference<typename lambda_traits<F>::arg2_type>::type>::type,
				typename R = typename lambda_traits<F>::result_type
			>
			struct pair_wise {
				F f;
				pair_wise(const F& f) : f(f) {}
				set<R> operator()(const set<A>& a, const set<B>& b) const {
					set<R> res;
					for(auto& x : a) {
						for(auto& y : b) {
							res.insert(f(x,y));
						}
					}
					return res;
				}
			};

			template<typename F>
			pair_wise<F> pairwise(const F& f) {
				return pair_wise<F>(f);
			}

			template<typename Comparator>
			std::function<set<bool>(const set<Formula>&,const set<Formula>&)> compareFormula(const Comparator& fun) {
				return [=](const set<Formula>& a, const set<Formula>& b)->set<bool> {
					static const set<bool> unknown({true, false});

					set<bool> res;

					// quick check
					for(auto& x : a) if (!x) return unknown;
					for(auto& x : b) if (!x) return unknown;

					// check out pairs
					bool containsTrue = false;
					bool containsFalse = false;
					for(auto& x : a) {
						for(auto& y : b) {
							if (containsTrue && containsFalse) {
								return res;
							}

							// pair< valid, unsatisfiable >
							pair<bool,bool> validity = fun(*x.formula, *y.formula);
							if (!containsTrue && !validity.second) {
								res.insert(true);
								containsTrue = true;
							}

							if (!containsFalse && !validity.first) {
								res.insert(false);
								containsFalse = true;
							}
						}
					}
					return res;
				};
			}

			bool isBooleanSymbol(const ExpressionPtr& expr) {
				auto& gen = expr->getNodeManager().getLangBasic();
				return expr.isa<LiteralPtr>() && !gen.isTrue(expr) && !gen.isFalse(expr);
			}

		}

		class BooleanConstraintCollector : public BasicDataFlowConstraintCollector<bool> {

			typedef BasicDataFlowConstraintCollector<bool> super;

			const core::lang::BasicGenerator& base;

		public:

			BooleanConstraintCollector(CBAContext& context, Constraints& constraints, const StatementAddress& root)
				: BasicDataFlowConstraintCollector<bool>(context, constraints, root, cba::B, cba::b), base(root->getNodeManager().getLangBasic()) { };

			void visitLiteral(const LiteralAddress& literal) {

				// and default handling
				super::visitLiteral(literal);

				// only interested in boolean literals
				if (!base.isBool(literal->getType())) return;

				// add constraint literal \in A(lit)
				bool isTrue = base.isTrue(literal);
				bool isFalse = base.isFalse(literal);

				auto l_lit = context.getLabel(literal);

				if (isTrue  || (!isTrue && !isFalse)) constraints.add(elem(true, context.getSet(B, l_lit)));
				if (isFalse || (!isTrue && !isFalse)) constraints.add(elem(false, context.getSet(B, l_lit)));

			}


			void visitCallExpr(const CallExprAddress& call) {

				// conduct std-procedure
				super::visitCallExpr(call);

				// only care for integer expressions calling literals
				if (!base.isBool(call->getType())) return;

				// check whether it is a literal => otherwise basic data flow is handling it
				auto fun = call->getFunctionExpr();
				if (!fun.isa<LiteralPtr>()) return;

				// get some labels / ids
				auto B_res = context.getSet(B, context.getLabel(call));

				// handle unary literals
				if (call.size() == 1u) {

					// check whether it is a de-ref
					if (base.isRefDeref(fun)) {
						return;		// has been handled by super!
					}

					// support negation
					if (base.isBoolLNot(fun)) {
						auto B_arg = context.getSet(B, context.getLabel(call[0]));
						constraints.add(subsetUnary(B_arg, B_res, [](const set<bool>& in)->set<bool> {
							set<bool> out;
							for(bool cur : in) out.insert(!cur);
							return out;
						}));
						return;
					}
				}

				// and binary operators
				if (call.size() != 2u) {
					// this value is unknown => might be both
					constraints.add(elem(true, B_res));
					constraints.add(elem(false, B_res));
					return;
				}


				// boolean relations
				{
					// get sets for operators
					auto B_lhs = context.getSet(B, context.getLabel(call[0]));
					auto B_rhs = context.getSet(B, context.getLabel(call[1]));

					if (base.isBoolEq(fun)) {
						// equality is guaranteed if symbols are identical - no matter what the value is
						if (isBooleanSymbol(call[0]) && isBooleanSymbol(call[1])) {
							constraints.add(elem(call[0].as<ExpressionPtr>() == call[1].as<ExpressionPtr>(), B_res));
						} else {
							constraints.add(subsetBinary(B_lhs, B_rhs, B_res, pairwise([](bool a, bool b) { return a == b; })));
						}
						return;
					}

					if (base.isBoolNe(fun)) {
						// equality is guaranteed if symbols are identical - no matter what the value is
						if (isBooleanSymbol(call[0]) && isBooleanSymbol(call[1])) {
							constraints.add(elem(call[0].as<ExpressionPtr>() != call[1].as<ExpressionPtr>(), B_res));
						} else {
							constraints.add(subsetBinary(B_lhs, B_rhs, B_res, pairwise([](bool a, bool b) { return a != b; })));
						}
						return;
					}
				}

				// arithmetic relations
				{
					auto A_lhs = context.getSet(cba::A, context.getLabel(call[0]));
					auto A_rhs = context.getSet(cba::A, context.getLabel(call[1]));

					typedef core::arithmetic::Formula F;
					typedef core::arithmetic::Inequality Inequality;		// shape: formula <= 0

					if(base.isSignedIntLt(fun) || base.isUnsignedIntLt(fun)) {
						constraints.add(subsetBinary(A_lhs, A_rhs, B_res, compareFormula([](const F& a, const F& b) {
							// a < b  ... if !(a >= b) = !(b <= a) = !(b-a <= 0)
							Inequality i(b-a);
							return std::make_pair(i.isUnsatisfiable(), i.isValid());
						})));
						return;
					}

					if(base.isSignedIntLe(fun) || base.isUnsignedIntLe(fun)) {
						constraints.add(subsetBinary(A_lhs, A_rhs, B_res, compareFormula([](const F& a, const F& b) {
							// a <= b ... if (a-b <= 0)
							Inequality i(a-b);
							return std::make_pair(i.isValid(), i.isUnsatisfiable());
						})));
						return;
					}

					if(base.isSignedIntGe(fun) || base.isUnsignedIntGe(fun)) {
						constraints.add(subsetBinary(A_lhs, A_rhs, B_res, compareFormula([](const F& a, const F& b){
							// a >= b ... if (b <= a) = (b-a <= 0)
							Inequality i(b-a);
							return std::make_pair(i.isValid(), i.isUnsatisfiable());
						})));
						return;
					}

					if(base.isSignedIntGt(fun) || base.isUnsignedIntGt(fun)) {
						constraints.add(subsetBinary(A_lhs, A_rhs, B_res, compareFormula([](const F& a, const F& b){
							// a > b ... if !(a <= b) = !(a-b <= 0)
							Inequality i(a-b);
							return std::make_pair(i.isUnsatisfiable(), i.isValid());
						})));
						return;
					}

					if(base.isSignedIntEq(fun) || base.isUnsignedIntEq(fun)) {
						constraints.add(subsetBinary(A_lhs, A_rhs, B_res, compareFormula([](const F& a, const F& b) {
							// just compare formulas (in normal form)
							bool equal = (a==b);
							return std::make_pair(equal, !equal && a.isConstant() && b.isConstant());
						})));
						return;
					}

					if(base.isSignedIntNe(fun) || base.isUnsignedIntNe(fun)) {
						constraints.add(subsetBinary(A_lhs, A_rhs, B_res, compareFormula([](const F& a, const F& b) {
							// just compare formulas (in normal form)
							bool equal = (a==b);
							return std::make_pair(!equal && a.isConstant() && b.isConstant(), equal);
						})));
						return;
					}
				}

				// otherwise it is unknown, hence both may be possible
				constraints.add(elem(true, B_res));
				constraints.add(elem(false, B_res));
			}

		};

		class ReferenceConstraintCollector : public BasicDataFlowConstraintCollector<Location> {

			typedef BasicDataFlowConstraintCollector<Location> super;

		public:

			ReferenceConstraintCollector(CBAContext& context, Constraints& constraints, const StatementAddress& root)
				: BasicDataFlowConstraintCollector<Location>(context, constraints, root, R, r) { };

			void visitLiteral(const LiteralAddress& literal) {

				// and default handling
				super::visitLiteral(literal);

				// only interested in memory location constructors
				if (!isMemoryConstructor(literal)) return;

				// add constraint literal \in R(lit)
				auto value = context.getLocation(literal);
				auto l_lit = context.getLabel(literal);

				auto R_lit = context.getSet(R, l_lit);
				constraints.add(elem(value, R_lit));

			}

			void visitCallExpr(const CallExprAddress& call) {

				// and default handling
				super::visitCallExpr(call);

				// introduce memory location in some cases
				if (!isMemoryConstructor(call)) return;

				// add constraint location \in R(call)
				auto value = context.getLocation(call);
				auto l_lit = context.getLabel(call);

				auto R_lit = context.getSet(R, l_lit);
				constraints.add(elem(value, R_lit));
			}

		};

		template<typename T>
		class ImperativeConstraintCollector : public IRVisitor<void, Address> {

			typedef IRVisitor<void,Address> super;

			CBAContext& context;
			Constraints& constraints;

			const TypedSetType<T>& dataSet;

			// list of all memory location in the processed fragment
			vector<Location> locations;

			set<NodeAddress> processed;

		public:

			ImperativeConstraintCollector(CBAContext& context, Constraints& contraints, const StatementAddress& root, const TypedSetType<T>& dataSet)
				: context(context), constraints(contraints), dataSet(dataSet), processed() {

				// collect all memory location constructors
				visitDepthFirst(root, [&](const ExpressionAddress& cur) {
					// TODO: add context info to locations
					if (isMemoryConstructor(cur)) {
						locations.push_back(context.getLocation(cur));
					}
				});

			};

			virtual void visit(const NodeAddress& node) {
				if (!processed.insert(node).second) return;
				super::visit(node);
			}

			void visitLiteral(const LiteralAddress& lit) {

				// fix: Sin \subset Sout
				auto label = context.getLabel(lit);
				Context c;
				Thread t;
				connectStateSets(Sin, label, c, t, Sout, label, c, t);

			}

			void visitVariable(const VariableAddress& var) {

				// fix: Sin \subset Sout
				auto label = context.getLabel(var);
				Context c;
				Thread t;
				connectStateSets(Sin, label, c, t, Sout, label, c, t);

			}

			void visitCallExpr(const CallExprAddress& call) {
				const auto& base = call->getNodeManager().getLangBasic();

				// recursively process sub-expressions
				visit(call->getFunctionExpr());
				for(auto arg : call) visit(arg);

				// otherwise default handling
				//  - link in of call with in of arguments
				//  - link out of arguments with in of function
				//  - link out of function with out of call

				auto l_call = context.getLabel(call);

				Context c;
				Thread t;

				// link in of call with in of arguments
				for(auto arg : call) {
					auto l_arg = context.getLabel(arg);
					connectStateSets(Sin, l_call, c, t, Sin, l_arg, c, t);
				}

				// and the function
				auto l_fun = context.getLabel(call->getFunctionExpr());
				connectStateSets(Sin, l_call, c, t, Sin, l_fun, c, t);


				// TODO: consider dynamic dispatching
				// link out of arguments with in of function candidates
				if (!call->getFunctionExpr().isa<LambdaExprPtr>() && !call->getFunctionExpr().isa<LiteralPtr>()) {
					std::cout << "WARNING: unsupported call target of type: " << call->getFunctionExpr()->getNodeType() << "\n";
					return;
				}


				if (call->getFunctionExpr().isa<LambdaExprPtr>()){ 		// for now

					// ---- Effect of arguments => in of function ----
					auto lambda = call->getFunctionExpr().as<LambdaExprAddress>();

					auto l_fun = context.getLabel(lambda->getBody());		// here we have to go through list of functions ...
					for (auto arg : call) {
						auto l_arg = context.getLabel(arg);
						connectStateSets(Sout, l_arg, c, t, Sin, l_fun, c, t);
					}

					// also add effects of function-expression evaluation
					connectStateSets(Sout, context.getLabel(lambda), c, t, Sin, l_fun, c, t);

					// ---- Effect of function => out of call ---

					// link out of fun with call out
					connectStateSets(Sout, l_fun, c, t, Sout, l_call, c, t);

					// process function body
					visit(lambda->getBody());

					return;
				}

				// ---- side-effects ----

				// special case: assignments
				if (core::analysis::isCallOf(call.as<CallExprPtr>(), base.getRefAssign())) {

					// ---- S_out of args => S_as of call

					for (auto arg : call) {
						auto l_arg = context.getLabel(arg);
						connectStateSets(Sout, l_arg, c, t, Stmp, l_call, c, t);
					}
					// and the function
					connectStateSets(Sout, l_fun, c, t, Stmp, l_call, c, t);

					// ---- combine S_as to S_out ...

					// add rule: loc \in R[rhs] => A[lhs] \sub Sout[call]
					auto l_rhs = context.getLabel(call[0]);
					auto l_lhs = context.getLabel(call[1]);
					auto R_rhs = context.getSet(R, l_rhs);
					for(auto loc : locations) {

						// TODO: add context

						// if loc is in R(target) then add D[rhs] to Sout[loc]
						auto A_value = context.getSet(dataSet, l_lhs);
						auto S_out = context.getSet(Sout, l_call, c, t, loc, dataSet, c, t);
						constraints.add(subsetIf(loc, R_rhs, A_value, S_out));
					}


					// add rule: |R[rhs]\{loc}| > 0 => Stmp[call] \sub Sout[call]
					for(auto loc : locations) {

						// get Sin set		TODO: add context to locations
						auto s_as = context.getSet(Stmp, l_call, c, t, loc, dataSet);
						auto s_out = context.getSet(Sout, l_call, c, t, loc, dataSet);

						// if more than 1 reference may be assigned => everything that comes in goes out
						constraints.add(subsetIfReducedBigger(R_rhs, loc, 0, s_as, s_out));
					}

				} else {

					// just connect out of arguments to call-out - assume no effects on state
					for (auto arg : call) {
						auto l_arg = context.getLabel(arg);
						connectStateSets(Sout, l_arg, c, t, Sout, l_call, c, t);
					}

					// and the function
					connectStateSets(Sout, l_fun, c, t, Sout, l_call, c, t);
				}

				// special case: read
				if (core::analysis::isCallOf(call.as<CallExprPtr>(), base.getRefDeref())) {

					// read value from memory location
					auto l_trg = context.getLabel(call[0]);
					auto R_trg = context.getSet(R, l_trg);
					for(auto loc : locations) {

						// TODO: add context

						// if loc is in R(target) then add Sin[A,trg] to A[call]
						auto S_in = context.getSet(Sin, l_call, c, t, loc, dataSet, c, t);
						auto A_call = context.getSet(dataSet, l_call);
						constraints.add(subsetIf(loc, R_trg, S_in, A_call));
					}
				}

			}

			void visitLambdaExpr(const LambdaExprAddress& cur) {

				// fix: Sin \subset Sout
				auto label = context.getLabel(cur);
				Context c;
				Thread t;
				connectStateSets(Sin, label, c, t, Sout, label, c, t);


				// TODO: deal with recursion

				// + process body
				for(auto def : cur->getDefinition()) {
					visit(def->getLambda()->getBody());
				}
			}

			void visitCompoundStmt(const CompoundStmtAddress& compound) {

				Context c;
				Thread t;

				// special handling for empty compound = NoOp
				if (compound.empty()) {
					auto l = context.getLabel(compound);
					connectStateSets(Sin, l, c, t, Sout, l, c, t);
					return;
				}

				// connect contained statements
				for(std::size_t i = 0; i < compound.size()-1; i++) {
					auto la = context.getLabel(compound[i]);
					auto lb = context.getLabel(compound[i+1]);
					connectStateSets(Sout, la, c, t, Sin, lb, c, t);
				}

				// connect in-state with in of first statement
				auto l = context.getLabel(compound);
				auto la = context.getLabel(compound[0]);
				connectStateSets(Sin, l, c, t, Sin, la, c, t);

				// connect out-state of last statement with out-state
				auto lb = context.getLabel(compound[compound.size()-1]);
				connectStateSets(Sout, lb, c, t, Sout, l, c, t);

				// and add constraints of all inner statements
				for(auto cur : compound) {
					visit(cur);
				}
			}

			void visitDeclarationStmt(const DeclarationStmtAddress& decl) {

				// just connect in with init value and out of innit value with out
				auto l = context.getLabel(decl);
				auto l_init = context.getLabel(decl->getInitialization());

				Context c;
				Thread t;

				connectStateSets(Sin, l, c, t, Sin, l_init, c, t);
				connectStateSets(Sout, l_init, c, t, Sout, l, c, t);

				// and create constraints for initialization value
				visit(decl->getInitialization());
			}

			void visitReturnStmt(const ReturnStmtAddress& stmt) {
				Context c;
				Thread t;

				// connect Sin with Sin of return expression
				auto l_ret = context.getLabel(stmt);
				auto l_val = context.getLabel(stmt->getReturnExpr());
				connectStateSets(Sin, l_ret, c, t, Sin, l_val, c, t);

				// find enclosing lambda
				LambdaAddress lambda = getEnclosingLambda(stmt);
				if (!lambda) {
					std::cout << "WARNING: encountered free return!\n";
					return;
				}

				// connect Sout of value with Sout of function
				auto l_fun = context.getLabel(lambda->getBody());
				connectStateSets(Sout, l_val, c, t, Sout, l_fun, c, t);

				// fix constraints for return expr
				visit(stmt->getReturnExpr());
			}

			void visitIfStmt(const IfStmtAddress& stmt) {
				Context c;
				Thread t;

				// get some labels
				auto l_if = context.getLabel(stmt);
				auto l_cond = context.getLabel(stmt->getCondition());
				auto l_then = context.getLabel(stmt->getThenBody());
				auto l_else = context.getLabel(stmt->getElseBody());

				auto B_cond = context.getSet(B, l_cond);

				// -- conditional has to be always evaluated --
				connectStateSets(Sin, l_if, c, t, Sin, l_cond, c, t);

				// connect Sout of condition to then and else branch
				connectStateSetsIf(true,  B_cond, Sout, l_cond, c, t, Sin, l_then, c, t);
				connectStateSetsIf(false, B_cond, Sout, l_cond, c, t, Sin, l_else, c, t);

				// connect Sout of then and else branch with Sout of if
				connectStateSetsIf(true,  B_cond, Sout, l_then, c, t, Sout, l_if, c, t);
				connectStateSetsIf(false, B_cond, Sout, l_else, c, t, Sout, l_if, c, t);

				// add constraints recursively
				visit(stmt->getCondition());
				visit(stmt->getThenBody());
				visit(stmt->getElseBody());
			}

			void visitWhileStmt(const WhileStmtAddress& stmt) {
				Context c;
				Thread t;

				// get some labels
				auto l_while = context.getLabel(stmt);
				auto l_cond = context.getLabel(stmt->getCondition());
				auto l_body = context.getLabel(stmt->getBody());

				// do the wiring
				connectStateSets(Sin, l_while, c, t, Sin, l_cond, c, t);
				connectStateSets(Sout, l_cond, c, t, Sin, l_body, c, t);
				connectStateSets(Sout, l_body, c, t, Sin, l_cond, c, t);
				connectStateSets(Sout, l_cond, c, t, Sout, l_while, c, t);

				// add constraints recursively
				visit(stmt->getCondition());
				visit(stmt->getBody());
			}

			void visitNode(const NodeAddress& node) {
				std::cout << "Reached unsupported Node Type: " << node->getNodeType() << "\n";
				assert(false);
			}

		private:

			void connectStateSets(StateSetType a, Label al, const Context& ac, const Thread& at, StateSetType b, Label bl, const Context& bc, const Thread& bt) {

				// general handling - Sin = Sout
				for(auto loc : locations) {

					// get Sin set		TODO: add context to locations
					auto s_in = context.getSet(a, al, ac, at, loc, dataSet);
					auto s_out = context.getSet(b, bl, bc, bt, loc, dataSet);

					// state information entering the set is also leaving it
					constraints.add(subset(s_in, s_out));

				}
			}

			template<typename E>
			void connectStateSetsIf(const E& value, const TypedSetID<E>& set, StateSetType a, Label al, const Context& ac, const Thread& at, StateSetType b, Label bl, const Context& bc, const Thread& bt) {

				// general handling - Sin = Sout
				for(auto loc : locations) {

					// get Sin set		TODO: add context to locations
					auto s_in = context.getSet(a, al, ac, at, loc, dataSet);
					auto s_out = context.getSet(b, bl, bc, bt, loc, dataSet);

					// state information entering the set is also leaving it
					constraints.add(subsetIf(value, set, s_in, s_out));

				}
			}

		};

		template<typename T>
		void addImperativeConstraints(CBAContext& context, Constraints& res, const StatementAddress& root, const TypedSetType<T>& type) {
			ImperativeConstraintCollector<T>(context, res, root, type).visit(root);
		}
	}


	Constraints generateConstraints(CBAContext& context, const StatementPtr& stmt) {

		// create resulting list of constraints
		Constraints res;

		// let constraint collector do the job
		StatementAddress root(stmt);

		// TODO: resolve dependencies between collectors automatically
		ControlFlowConstraintCollector(context, res, root).visit(root);
		ConstantConstraintCollector(context, res, root).visit(root);
		ReferenceConstraintCollector(context, res, root).visit(root);
		ArithmeticConstraintCollector(context, res, root).visit(root);
		BooleanConstraintCollector(context, res, root).visit(root);

		// and the imperative constraints
		addImperativeConstraints(context, res, root, C);
		addImperativeConstraints(context, res, root, D);
		addImperativeConstraints(context, res, root, R);
		addImperativeConstraints(context, res, root, A);
		addImperativeConstraints(context, res, root, B);


		// done
		return res;


	}


	Solution solve(const Constraints& constraints) {
		// just use the utils solver
		return utils::set_constraint_2::solve(constraints);
	}

	namespace {


		const char* getName(StateSetType type) {
			switch(type) {
			case Sin: return "Sin";
			case Stmp: return "Stmp";
			case Sout: return "Sout";
			}
			return "?";
		}

	}

	using namespace utils::set_constraint_2;

	void CBAContext::plot(const Constraints& constraints, std::ostream& out) const {


		auto getAddress = [&](const Label l)->StatementAddress {
			for(auto cur : this->labels) {
				if (cur.second == l) return cur.first;
			}
			for(auto cur : this->vars) {
				if (cur.second == l) return cur.first;
			}
			return StatementAddress();
		};


		out << "digraph G {";

		// name sets
		for(auto cur : sets) {
			string setName = std::get<0>(cur.first)->getName();
			auto pos = getAddress(std::get<1>(cur.first));
			out << "\n\t" << cur.second << " [label=\"s" << cur.second << " = " << setName << "[l" << std::get<1>(cur.first) << " = " << pos->getNodeType() << " : " << pos << "]\"];";
		}

		for(auto cur : stateSets) {
			string setName = getName(std::get<0>(cur.first));
			string dataName = std::get<5>(cur.first)->getName();
			auto pos = getAddress(std::get<1>(cur.first));
			out << "\n\t" << cur.second << " [label=\"s" << cur.second << " = " << setName << "-" << dataName << "@" << std::get<4>(cur.first) << "[l" << std::get<1>(cur.first) << " = " << pos->getNodeType() << " : " << pos << "]\"];";
		}

		// link sets
		for(auto cur : constraints) {
			out << "\n\t";
			cur->writeDotEdge(out);
		}

		out << "\n}\n";

	}

} // end namespace cba
} // end namespace analysis
} // end namespace insieme

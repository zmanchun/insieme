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

#include "insieme/frontend/tu/ir_translation_unit.h"
#include "insieme/frontend/utils/macros.h"

#include "insieme/utils/assert.h"

#include "insieme/core/ir.h"
#include "insieme/core/types/subtyping.h"

#include "insieme/core/annotations/naming.h"
#include "insieme/core/ir_builder.h"
#include "insieme/core/ir_visitor.h"
#include "insieme/core/ir_class_info.h"
#include "insieme/core/lang/ir++_extension.h"
#include "insieme/core/lang/static_vars.h"
#include "insieme/core/analysis/ir_utils.h"
#include "insieme/core/analysis/type_utils.h"
#include "insieme/core/printer/pretty_printer.h"
#include "insieme/core/transform/node_replacer.h"
#include "insieme/core/transform/manipulation.h"
#include "insieme/core/transform/manipulation_utils.h"
#include "insieme/core/encoder/ir_class_info.h"

#include "insieme/core/ir_statistic.h"

#include "insieme/annotations/c/extern.h"
#include "insieme/annotations/c/include.h"

#include "insieme/utils/graph_utils.h"

namespace insieme {
namespace frontend {
namespace tu {

	void IRTranslationUnit::addGlobal(const Global& newGlobal) {
		assert(newGlobal.first && newGlobal.first->getType().isa<core::RefTypePtr>());

		auto git = std::find_if(globals.begin(), globals.end(), [&](const Global& cur)->bool { return *newGlobal.first == *cur.first; });
		if( git == globals.end() ) {
			//global is new >> insert into globalsList
			globals.push_back(Global(mgr->get(newGlobal.first), mgr->get(newGlobal.second)));
		} else {
			//global is already in globalsList, if the "new one" has a initValue update the init
			if(newGlobal.second) {
				git->second = newGlobal.second;
			}
		}
	}

	std::ostream& IRTranslationUnit::printTo(std::ostream& out) const {
		static auto print = [](const core::NodePtr& node) { return core::printer::printInOneLine(node); };
		return out << "TU(\n\tTypes:\n\t\t"
				<< join("\n\t\t", types, [&](std::ostream& out, const std::pair<core::GenericTypePtr, core::TypePtr>& cur) { out << *cur.first << " => " << *cur.second; })
				<< ",\n\tGlobals:\n\t\t"
				<< join("\n\t\t", globals, [&](std::ostream& out, const std::pair<core::LiteralPtr, core::ExpressionPtr>& cur) { out << *cur.first << ":" << *cur.first->getType() << " => "; if (cur.second) out << print(cur.second); else out << "<uninitalized>"; })
				<< ",\n\tInitializer:\n\t\t"
				<< join("\n\t\t", initializer, [&](std::ostream& out, const core::StatementPtr& cur) { out << print(cur); })
				<< ",\n\tFunctions:\n\t\t"
				<< join("\n\t\t", functions, [&](std::ostream& out, const std::pair<core::LiteralPtr, core::ExpressionPtr>& cur) { out << *cur.first << " : " << *cur.first->getType() << " => " << print(cur.second); })
				<< ",\n\tEntry Points:\t{"
				<< join(", ", entryPoints, [&](std::ostream& out, const core::LiteralPtr& cur) { out << *cur; })
				<< "},\n\tMetaInfos:\t{"
				<< join(", ", metaInfos, [&](std::ostream& out, const std::pair<core::TypePtr, std::vector<core::ClassMetaInfo>>& cur) { out << *cur.first << " : " << cur.second; })
				<< "}\n)";
	}

	IRTranslationUnit IRTranslationUnit::toManager(core::NodeManager& manager) const {

		IRTranslationUnit res(manager);

		// copy types
		for(auto cur : getTypes()) {
			res.addType(cur.first, cur.second);
		}

		// copy functions
		for(auto cur : getFunctions()) {
			res.addFunction(cur.first, cur.second);
		}

		// copy globals
		for(auto cur : getGlobals()) {
			res.addGlobal(cur);
		}

		// copy initalizer
		for(auto cur : getInitializer()) {
			res.addInitializer(cur);
		}

		// entry points
		for(auto cur : getEntryPoints()) {
			res.addEntryPoints(cur);
		}

		// copy meta infos 
		for(auto cur : getMetaInfos()) {
			res.addMetaInfo(cur.first, cur.second);
		}

		// done
		return res;
	}

	IRTranslationUnit merge(core::NodeManager& mgr, const IRTranslationUnit& a, const IRTranslationUnit& b) {

		IRTranslationUnit res = a.toManager(mgr);

		// copy types
		for(auto cur : b.getTypes()) {
			if(!res[cur.first]) res.addType(cur.first, cur.second);
		}

		// copy functions
		for(auto cur : b.getFunctions()) {
			if(!res[cur.first]) res.addFunction(cur.first, cur.second);
		}

		// copy globals
		for(auto cur : b.getGlobals()) {
			res.addGlobal(cur);
		}

		// copy initializer
		for(auto cur : b.getInitializer()) {
			res.addInitializer(cur);
		}

		// entry points
		for(auto cur : b.getEntryPoints()) {
			res.addEntryPoints(cur);
		}
		
		// copy meta infos 
		for(auto cur : b.getMetaInfos()) {
			res.addMetaInfo(cur.first, cur.second);
		}

	//	// migrate all meta information
	//	if (&b.getNodeManager() != &mgr) {
	//		auto& mgrB = b.getNodeManager();

	//		// built a visitor searching all meta-info entries and merge them
	//		auto visitor = core::makeLambdaVisitor([&](const core::TypePtr& type) {

	//			// check whether there is a meta-info annotation at the original type
	//			auto other = mgrB.get(type);

	//			// if there is some meta-info
	//			if (core::hasMetaInfo(other)) {
	//				// merge it
	//				core::setMetaInfo(type, core::merge(core::getMetaInfo(type), core::getMetaInfo(other)));
	//			}

	//		});
	//		auto cachedVisitor = core::makeDepthFirstOnceVisitor(visitor);

	//		// apply visitor
	//		res.visitAll(cachedVisitor);
	//	}

		// done
		return res;
	}

	IRTranslationUnit merge(core::NodeManager& mgr, const vector<IRTranslationUnit>& units) {
		IRTranslationUnit res(mgr);
		for(const auto& cur : units) {
			res = merge(mgr, res, cur);
		}
		res.setCXX(any(units, [](const IRTranslationUnit& cur) { return cur.isCXX(); }));
		return res;
	}


	// ------------------------------- to-program conversion ----------------------------


	namespace {

		using namespace core;

		class Resolver : private core::NodeMapping {

			typedef utils::graph::PointerGraph<NodePtr> SymbolDependencyGraph;
			typedef utils::graph::Graph<std::set<NodePtr>> RecComponentGraph;

			NodeManager& mgr;

			IRBuilder builder;

			NodeMap symbolMap;

		public:

			Resolver(NodeManager& mgr, const IRTranslationUnit& unit)
				: mgr(mgr), builder(mgr), symbolMap() {

				// copy type symbols into symbol table
				for(auto cur : unit.getTypes()) {
					symbolMap[mgr.get(cur.first)] = mgr.get(cur.second);
				}

				// copy function symbols into symbol table
				for(auto cur : unit.getFunctions()) {
					symbolMap[mgr.get(cur.first)] = mgr.get(cur.second);
				}
			}

			template<typename T>
			Pointer<T> apply(const Pointer<T>& ptr) {
				return apply(NodePtr(ptr)).as<Pointer<T>>();
			}

			NodePtr apply(const NodePtr& node) {
//std::cout << "Processing " << node << "\n";
				// 1. get set of contained symbols
				const NodeSet& init = getContainedSymbols(node);
//std::cout << "Initial Set: " << init << "\n";
				// 2. build dependency graph
				auto depGraph = getDependencyGraph(init);
//std::cout << "DepGraph: " << depGraph << "\n";
				// 3. compute SCCs graph
				auto components = computeSCCGraph(depGraph);
//std::cout << "Components: " << components << "\n";
				// 4. resolve SCCs components bottom up
				resolveComponents(components);

				// 5. resolve input
				return map(node);
			}

		private:

			// --- Utilities ---

			bool isSymbol(const NodePtr& node) const {
				return symbolMap.find(node) != symbolMap.end();
			}

			NodePtr getDefinition(const NodePtr& symbol) const {
				assert_true(isSymbol(symbol));
				return symbolMap.find(symbol)->second;
			}

			// --- Step 1: Symbol extraction ---

			mutable utils::map::PointerMap<NodePtr, NodeSet> containedSymbols;

			const NodeSet& getContainedSymbols(const NodePtr& node) const {

				// first check cache
				auto pos = containedSymbols.find(node);
				if (pos != containedSymbols.end()) return pos->second;

				// collect contained symbols
				NodeSet res;
				core::visitDepthFirstOnce(node, [&](const NodePtr& cur) {
					if (isSymbol(cur)) res.insert(cur);
				}, true, true);

				// add result to cache and return it
				return containedSymbols[node] = res;
			}

			// --- Step 2: Dependency Graph ---

			SymbolDependencyGraph getDependencyGraph(const NodeSet& seed) const {

				SymbolDependencyGraph res;

				NodeList open;
				for (const auto& cur : seed) {
					if (!isResolved(cur)) open.push_back(cur);
				}

				NodeSet processed;
				while(!open.empty()) {

					// get current element
					NodePtr cur = open.back();
					open.pop_back();

					// skip elements already processed
					if (processed.contains(cur)) continue;
					processed.insert(cur);

					// skip resolved symbols
					if (isResolved(cur)) continue;

					// add vertex (even if there is no edge)
					res.addVertex(cur);

					// add dependencies to graph
					for(const auto& other : getContainedSymbols(getDefinition(cur))) {

						// skip already resolved nodes
						if (isResolved(other)) continue;

						// add dependency to other
						res.addEdge(cur, other);

						// add other to list of open nodes
						open.push_back(other);
					}
				}

				// done
				return res;
			}

			// --- Step 3: SCC computation ---

			RecComponentGraph computeSCCGraph(const SymbolDependencyGraph& graph) {
				// there is a utility for that
				return utils::graph::computeSCCGraph(graph.asBoostGraph());
			}

			// --- Step 4: Component resolution ---

			std::map<NodePtr,NodePtr> resolutionCache;
			bool cachingEnabled;

			bool isResolved(const NodePtr& symbol) const {
				assert_true(isSymbol(symbol)) << "Should only be queried for symbols!";
				return resolutionCache.find(symbol) != resolutionCache.end();
			}

			NodePtr getRecVar(const NodePtr& symbol) {
				assert_true(isSymbol(symbol));

				// create a fresh recursive variable
				if (const GenericTypePtr& type = symbol.isa<GenericTypePtr>()) {
					return builder.typeVariable(type->getFamilyName());
				} else if (const LiteralPtr& fun = symbol.isa<LiteralPtr>()) {
					return builder.variable(map(fun->getType()));
				}

				// otherwise fail!
				assert(false && "Unsupported symbol encountered!");
				return symbol;
			}

			bool isDirectRecursive(const NodePtr& symbol) {
				return getContainedSymbols(getDefinition(symbol)).contains(symbol);
			}

			void resolveComponents(const RecComponentGraph& graph) {

				// compute topological order -- there is a utility for this too
				auto components = utils::graph::getTopologicalOrder(graph);

				// reverse (since dependencies have to be resolved bottom up)
				components = reverse(components);

				// process in reverse order
				for (const auto& cur : components) {

					// sort variables
					vector<NodePtr> vars(cur.begin(), cur.end());
					std::sort(vars.begin(), vars.end(), compare_target<NodePtr>());

					// get first element
					auto first = vars.front();

					// skip this component if it has already been resolved as a side effect of another operation
					if (isResolved(first)) continue;

					// add variables for current components to resolution Cache
					for(const auto& s : vars) {
						resolutionCache[s] = getRecVar(s);
					}

					assert(isResolved(first));

					// resolve definitions (without caching temporal values)
					cachingEnabled = false;

					std::map<NodePtr,NodePtr> resolved;
					for(const auto& s : vars) {
						resolved[s] = map(getDefinition(s));
					}
					// re-enable caching again
					cachingEnabled = true;

					// close recursion if necessary
					if (vars.size() > 1 || isDirectRecursive(first)) {

						// close recursive types
						if (first.isa<GenericTypePtr>()) {

							// build recursive type definition
							vector<RecTypeBindingPtr> bindings;
							for(const auto& cur : vars) {
								bindings.push_back(
										builder.recTypeBinding(
												resolutionCache[cur].as<TypeVariablePtr>(),
												resolved[cur].as<TypePtr>()
										));
							}

							// build recursive type definition
							auto def = builder.recTypeDefinition(bindings);

							// simply construct a recursive type
							for(auto& cur : resolved) {
								auto newType = builder.recType(resolutionCache[cur.first].as<TypeVariablePtr>(), def);
								core::transform::utils::migrateAnnotations(cur.second, newType);
								cur.second = newType;
							}

						} else if (first.isa<LiteralPtr>()) {

							// build recursive lambda definition
							vector<LambdaBindingPtr> bindings;
							for(const auto& cur : vars) {
								bindings.push_back(
										builder.lambdaBinding(
												resolutionCache[cur].as<VariablePtr>(),
												resolved[cur].as<LambdaExprPtr>()->getLambda()
										));
							}

							// build recursive type definition
							auto def = builder.lambdaDefinition(bindings);

							// simply construct a recursive type
							for(auto& cur : resolved) {
								auto newFun = builder.lambdaExpr(resolutionCache[cur.first].as<VariablePtr>(), def);
								core::transform::utils::migrateAnnotations(cur.second, newFun);
								cur.second = newFun;
							}

						} else {
							assert_fail() << "Unsupported Symbol encountered: " << first << " - " << first.getNodeType() << "\n";
						}
					}

					// replace bindings with resolved definitions
					for(const auto& s : vars) {

						// we drop meta infos for now ... TODO: fix this
						if (s.isa<TypePtr>()) core::removeMetaInfo(s.as<TypePtr>());

						// migrate annotations
						core::transform::utils::migrateAnnotations(s, resolved[s]);

						// and add to cache
						resolutionCache[s] = resolved[s];
					}

				}

			}

			virtual const NodePtr mapElement(unsigned, const NodePtr& ptr) {

				// check cache first
				auto pos = resolutionCache.find(ptr);
				if (pos != resolutionCache.end()) return pos->second;

				// compute resolved type recursively
				auto res = ptr->substitute(mgr, *this);

				// Cleanups:
				{
					// special service: get rid of unnecessary casts (which might be introduced due to opaque generic types)
					if (const CastExprPtr& cast = res.isa<CastExprPtr>()) {
						// check whether cast can be skipped
						if (types::isSubTypeOf(cast->getSubExpression()->getType(), cast->getType())) {
							res = cast->getSubExpression();
						}
					}

					// if this is a call to ref member access we rebuild the whole expression to return a correct reference type
					if (core::analysis::isCallOf(res, mgr.getLangBasic().getCompositeRefElem())){
						auto call = res.as<CallExprPtr>();

						if (call[0]->getType().as<RefTypePtr>()->getElementType().isa<StructTypePtr>()){

							assert(call[0]);
							assert(call[1]);

							auto tmp = builder.refMember(call[0], call[1].as<LiteralPtr>()->getValue());
							// type changed... do we have any cppRef to unwrap?
							if (*(tmp->getType()) != *(call->getType())  &&
								core::analysis::isAnyCppRef(tmp->getType().as<RefTypePtr>()->getElementType()))
								res = builder.toIRRef(builder.deref(tmp));
							else
								res = tmp;
						}
					}

					// also if this is a call to member access we rebuild the whole expression to return
					// NON ref type
					if (core::analysis::isCallOf(res, mgr.getLangBasic().getCompositeMemberAccess())){
						auto call = res.as<CallExprPtr>();
						if (call[0]->getType().isa<StructTypePtr>()){
							auto tmp = builder.accessMember(call[0], call[1].as<LiteralPtr>()->getValue());
								// type might changed, we have to unwrap it
							if (core::analysis::isAnyCppRef(tmp->getType()))
								res = builder.deref(builder.toIRRef(tmp));
							else
								res = tmp;
						}
					}

					// also fix type literals
					if (core::analysis::isTypeLiteral(res)) {
						res = builder.getTypeLiteral(core::analysis::getRepresentedType(res.as<ExpressionPtr>()));
					}

					// and also fix generic-zero constructor
					if (core::analysis::isCallOf(res, mgr.getLangBasic().getZero())) {
						res = builder.getZero(core::analysis::getRepresentedType(res.as<CallExprPtr>()[0]));
					}
				}

				// if nothing has changed ..
				if (ptr == res) {
					// .. the result can always be cached (even if caching is disabled)
					return resolutionCache[ptr] = res;
				}

				// migrate annotations
				core::transform::utils::migrateAnnotations(ptr, res);

				// do not save result in cache if caching is disabled
				if (!cachingEnabled) return res;		// this is for temporaries

				// done
				return resolutionCache[ptr] = res;
			}

		};

//		/**
//		 * The class converting a IR-translation-unit into an actual IR program by
//		 * realizing recursive definitions.
//		 */
//		class Resolver : private core::NodeMapping {
//
//			NodeManager& mgr;
//			IRBuilder builder;
//
//			NodeMap cache;
//			NodeMap symbolMap;
//			NodeMap recVarMap;
//			NodeMap recVarResolutions;
//			NodeSet recVars;
//
//			// a map collecting class-meta-info values for lazy integration
//			typedef utils::map::PointerMap<TypePtr, core::ClassMetaInfo> MetaInfoMap;
//			MetaInfoMap metaInfos;
//
//			// a performance utility recording whether sub-trees contain recursive variables or not
//			utils::map::PointerMap<NodePtr, bool> containsRecVars;
//
//			NodeSet all;
//
//		public:
//
//			unsigned stepCounter;
//
//			Resolver(NodeManager& mgr, const IRTranslationUnit& unit)
//				: mgr(mgr), builder(mgr), cache(), symbolMap(), recVarMap(), recVarResolutions(), recVars(), metaInfos(), stepCounter(0) {
//
//				// copy type symbols into symbol table
//				for(auto cur : unit.getTypes()) {
//					symbolMap[mgr.get(cur.first)] = mgr.get(cur.second);
//				}
//
//				// copy function symbols into symbol table
//				for(auto cur : unit.getFunctions()) {
//					symbolMap[mgr.get(cur.first)] = mgr.get(cur.second);
//				}
//			}
//
//			template<typename T>
//			Pointer<T> apply(const Pointer<T>& ptr) {
//				return apply(NodePtr(ptr)).as<Pointer<T>>();
//			}
//
//			NodePtr apply(const NodePtr& node) {
//
//				// check whether meta-infos are empty
//				assert(metaInfos.empty());
//
//				// convert node itself
////std::cout << "   Resolving Symbol: " << node << "\n";
////std::cout << "   Result:\n" << core::IRStatistic::evaluate(node) << "\n";
//				auto res = map(node);
////std::cout << "   Collecting Stats...\n";
////std::cout << "   Result:\n" << core::IRStatistic::evaluate(res) << "\n";
//
//				// copy the source list to avoid invalidation of iterator
//				auto list =metaInfos;
//
//				// clear the meta-info list
//				while(!list.empty()) {
////std::cout << "   Resolving " << list.size() << " meta info annotations ... " << stepCounter << "/" << cache.size() << "/" << all.size() << "\n";
//					// clear meta-info collected in last round
//					metaInfos.clear();
//
//					// re-add meta information
//					for(const auto& cur : list) {
//
//						// encode meta info into pure IR
//						const core::ClassMetaInfo& info = cur.second;
//
////std::cout << "    @" << stepCounter << "\n";
////std::cout << "     Processing info of: " << ((info.getClassType().isa<StructTypePtr>())? info.getClassType().as<core::StructTypePtr>()->getName().getValue():toString(info.getClassType())) << "\n";
//
////std::cout << "        Number of Constructors:     " << info.getConstructors().size() << "\n";
////std::cout << "        Number of Member Functions: " << info.getMemberFunctions().size() << "\n";
//
////std::cout << "     Converting to IR ...\n";
//						auto encoded = core::encoder::toIR(mgr, cur.second);
//
//						// resolve meta info
////std::cout << "     Resolving symbols ...\n";
//						auto resolved = core::encoder::toValue<ClassMetaInfo>(map(encoded));
//
////std::cout << "     Re-attaching meta infos ...\n";
//						// restore resolved meta info for resolved type
//						setMetaInfo(map(cur.first), resolved);
//					}
//
//					// get new list of encountered meta-info data
//					list = metaInfos;
//				}
//
//				// there should not be any unprocessed meta-info instance
//				assert(metaInfos.empty());
//
//				// done
//				return res;
//			}
//
//			virtual const NodePtr mapElement(unsigned, const NodePtr& ptr) {
//				// check whether value is already cached
//				{
//					auto pos = cache.find(ptr);
//					if (pos != cache.end()) {
//						return pos->second;
//					}
//				}
//
//				all.insert(ptr);
//				stepCounter++;
//
//				// strip off class-meta-information
//				if (auto type = ptr.isa<TypePtr>()) {
//					if (hasMetaInfo(type)) {
//						metaInfos[type] = getMetaInfo(type);
//						removeMetaInfo(type);
//					}
//				}
//
//				// init result
//				NodePtr res = ptr;
//
//				// check whether the current node is in the symbol table
//				NodePtr recVar;
//				auto pos = symbolMap.find(ptr);
//				if (pos != symbolMap.end()) {
//
//					// enter a recursive substitute into the cache
//
//					// first check whether this ptr has already been mapped to a recursive variable
//					recVar = recVarMap[ptr];
//
//					// if not, create one of the proper type
//					if (!recVar) {
//						// create a fresh recursive variable
//						if (const GenericTypePtr& symbol = ptr.isa<GenericTypePtr>()) {
//							recVar = builder.typeVariable(symbol->getFamilyName());
//						} else if (const LiteralPtr& symbol = ptr.isa<LiteralPtr>()) {
//							recVar = builder.variable(map(symbol->getType()));
//						} else {
//							assert(false && "Unsupported symbol encountered!");
//						}
//						recVarMap[ptr] = recVar;
//						recVars.insert(recVar);
//					}
//
//					// now the recursive variable should be fixed
//					assert(recVar);
//
//					// check whether the recursive variable has already been completely resolved
//					{
//						auto pos = recVarResolutions.find(recVar);
//						if (pos != recVarResolutions.end()) {
//							// migrate annotations
//							core::transform::utils::migrateAnnotations(ptr, pos->second);
//							cache[ptr] = pos->second;				// for future references
//							return pos->second;
//						}
//					}
//
//					// update cache for current element
//					cache[ptr] = recVar;
//
//					// result will be the substitution
//					res = map(pos->second);
//
//				} else {
//
//					// resolve result recursively
//					res = res->substitute(mgr, *this);
//
//				}
//
//				// fix recursions
//				if (recVar) {
//
//					// check whether nobody has been messing with the cache!
//					assert_eq(cache[ptr], recVar) << "Don't touch this!";
//
//					// remove recursive variable from cache
//					cache.erase(ptr);
//
//					if (TypePtr type = res.isa<TypePtr>()) {
//
//						// fix type recursion
//						res = fixRecursion(type, recVar.as<core::TypeVariablePtr>());
//
//						// migrate annotations
//						core::transform::utils::migrateAnnotations(pos->second, res);
//						if (allRecVarsBound(res.as<TypePtr>())) {
//							cache[ptr] = res;
//
//							// also, register results in recursive variable resolution map
//							if (const auto& recType = res.isa<RecTypePtr>()) {
//								auto definition = recType->getDefinition();
//								if (definition.size() > 1) {
//									for(auto cur : definition) {
//										auto recType = builder.recType(cur->getVariable(), definition);
//										recVarResolutions[cur->getVariable()] = recType;
//										containsRecVars[recType] = false;
//
//										// also remove rec-var from list of recursive types
//										recVars.erase(cur->getVariable());
//									}
//								}
//							}
//
//							// remove all false entries in the rec-var cache (they might become true now)
//							resetContainsRecursiveVariableCache();
//						}
//
//					} else if (LambdaExprPtr lambda = res.isa<LambdaExprPtr>()) {
//
//						// re-build current lambda with correct recursive variable
//						assert_eq(1u, lambda->getDefinition().size());
//						auto var = recVar.as<VariablePtr>();
//						auto binding = builder.lambdaBinding(var, lambda->getLambda());
//						lambda = builder.lambdaExpr(var, builder.lambdaDefinition(toVector(binding)));
//
//						// fix recursions
//						res = fixRecursion(lambda);
//
//						// migrate annotations
//						core::transform::utils::migrateAnnotations(pos->second, res);
//
//						// add final results to cache
//						if (!analysis::hasFreeVariables(res)) {
//							cache[ptr] = res;
//
//							// also, register results in recursive variable resolution map
//							auto definition = res.as<LambdaExprPtr>()->getDefinition();
//							if (definition.size() > 1) {
//								for(auto cur : definition) {
//									auto recFun = builder.lambdaExpr(cur->getVariable(), definition);
//									recVarResolutions[cur->getVariable()] = recFun;
//									containsRecVars[recFun] = false;
//
//									// remove variable from list of active recursive variables
//									recVars.erase(cur->getVariable());
//								}
//							}
//
//							// remove all false entries in the rec-var cache (they might become true now)
//							resetContainsRecursiveVariableCache();
//						}
//
//					} else {
//						assert(false && "Unsupported recursive structure encountered!");
//					}
//				}
//
//				// special service: get rid of unnecessary casts (which might be introduced due to opaque generic types)
//				if (const CastExprPtr& cast = res.isa<CastExprPtr>()) {
//					// check whether cast can be skipped
//					if (types::isSubTypeOf(cast->getSubExpression()->getType(), cast->getType())) {
//						res = cast->getSubExpression();
//					}
//				}
//
//				// if this is a call to ref member access we rebuild the whole expression to return
//				// right ref type
//				if (core::analysis::isCallOf(res, mgr.getLangBasic().getCompositeRefElem())){
//					auto call = res.as<CallExprPtr>();
//
//					if (call[0]->getType().as<RefTypePtr>()->getElementType().isa<StructTypePtr>()){
//
//						assert(call[0]);
//						assert(call[1]);
//
//						auto tmp = builder.refMember(call[0], call[1].as<LiteralPtr>()->getValue());
//						// type changed... do we have any cppRef to unwrap?
//						if (*(tmp->getType()) != *(call->getType())  &&
//							core::analysis::isAnyCppRef(tmp->getType().as<RefTypePtr>()->getElementType()))
//							res = builder.toIRRef(builder.deref(tmp));
//						else
//							res = tmp;
//					}
//				}
//
//				// also if this is a call to member access we rebuild the whole expression to return
//				// NON ref type
//				if (core::analysis::isCallOf(res, mgr.getLangBasic().getCompositeMemberAccess())){
//					auto call = res.as<CallExprPtr>();
//					if (call[0]->getType().isa<StructTypePtr>()){
//						auto tmp = builder.accessMember(call[0], call[1].as<LiteralPtr>()->getValue());
//							// type might changed, we have to unwrap it
//						if (core::analysis::isAnyCppRef(tmp->getType()))
//							res = builder.deref(builder.toIRRef(tmp));
//						else
//							res = tmp;
//					}
//				}
//
//				// also fix type literals
//				if (core::analysis::isTypeLiteral(res)) {
//					res = builder.getTypeLiteral(core::analysis::getRepresentedType(res.as<ExpressionPtr>()));
//					// this result can also be cached
//					cache[ptr] = res;
//				}
//
//				// and also fix generic-zero constructor
//				if (core::analysis::isCallOf(res, mgr.getLangBasic().getZero())) {
//					res = builder.getZero(core::analysis::getRepresentedType(res.as<CallExprPtr>()[0]));
//					// this result can also be cached
//					cache[ptr] = res;
//				}
//
//				// add result to cache if it does not contain recursive parts (hence hasn't changed at all)
//				if (*ptr == *res) {
//					cache[ptr] = res;
//				} else if (!containsRecursiveVariable(res)) {
//					cache[ptr] = res;
//				}
//
//				// simply migrate annotations
//				core::transform::utils::migrateAnnotations(ptr, res);
//
//				// done
//				return res;
//			}
//
//		private:
//
//			bool hasFreeTypeVariables(const NodePtr& node) {
//				return core::analysis::hasFreeTypeVariables(node.isa<TypePtr>());
//			}
//
//			bool hasFreeTypeVariables(const TypePtr& type) {
//				return core::analysis::hasFreeTypeVariables(type);
//			}
//
//			TypePtr fixRecursion(const TypePtr type, const TypeVariablePtr var) {
//
//				// if it is a direct recursion, be done
//				NodeManager& mgr = type.getNodeManager();
//				IRBuilder builder(mgr);
//
//				// make sure it is handling a struct or union type
//				if(!type.isa<StructTypePtr>() && !type.isa<UnionTypePtr>())return type;
//
//				auto containsRecVar = [&](const TypePtr& type)->bool {
//					return visitDepthFirstOnceInterruptible(type, [&](const TypePtr& type)->bool {
//						return type == var;
//					}, true, true);
//				};
//
//				// see whether the recursive variable is present
//				if (!containsRecVar(type)) {
//					return type;
//				}
//
//				// 1) check nested recursive types - those include this type
//
//				// check whether there is nested recursive type specification that equals the current type
//				std::vector<RecTypePtr> recTypes;
//				visitDepthFirstOnce(type, [&](const RecTypePtr& cur) {
//					if (cur->getDefinition()->getDefinitionOf(var)) recTypes.push_back(cur);
//				}, true, true);
//
//				// see whether one of these is matching
//				for(auto cur : recTypes) {
//					// TODO: here it should actually be checked whether the inner one is structurally identical
//					//		 at the moment we relay on the fact that it has the same name
//					return builder.recType(var, cur->getDefinition());
//				}
//
//
//				// 2) normalize recursive type
//
//				// collect all struct types within the given type
//				TypeList structs;
//				visitDepthFirstOncePrunable(type, [&](const TypePtr& cur) {
//					//if (containsVarFree(cur)) ;
//					if (cur.isa<RecTypePtr>()) return !hasFreeTypeVariables(cur);
//					if (cur.isa<NamedCompositeTypePtr>() && !allRecVarsBound(cur)) {
//						structs.push_back(cur.as<TypePtr>());
//					}
//					return false;
//				}, true);
//
//				// check whether there is a recursion at all
//				if (structs.empty()) return type;
//
//				// create de-normalized recursive bindings
//				// beware of unamed structs
//				vector<RecTypeBindingPtr> bindings;
//				unsigned id =0;
//				for(auto cur : structs) {
//					std::string name = cur.as<core::StructTypePtr>()->getName().getValue();
//					if (name.empty()) name = format("t%d", id++);
//					bindings.push_back(builder.recTypeBinding(builder.typeVariable(name), cur));
//				}
//
//				// sort according to variable names
//				std::sort(bindings.begin(), bindings.end(), [](const RecTypeBindingPtr& a, const RecTypeBindingPtr& b) {
//					return a->getVariable()->getVarName()->getValue() < b->getVariable()->getVarName()->getValue();
//				});
//
//				// create definitions
//				RecTypeDefinitionPtr def = builder.recTypeDefinition(bindings);
//
//				// test whether this is actually a closed type ..
//				if(!allRecVarsBound(def)) return type;
//
//				// normalize recursive representation
//				RecTypeDefinitionPtr old;
//				while(old != def) {
//					old = def;
//
//					// set up current variable -> struct definition replacement map
//					NodeMap replacements;
//					for (auto cur : def) {
//						replacements[cur->getType()] = cur->getVariable();
//					}
//
//					// wrap into node mapper
//					auto mapper = makeLambdaMapper([&](int, const NodePtr& cur) {
//						return transform::replaceAllGen(mgr, cur, replacements);
//					});
//
//					// apply mapper to defintions
//					vector<RecTypeBindingPtr> newBindings;
//					for (RecTypeBindingPtr& cur : bindings) {
//						auto newBinding = builder.recTypeBinding(cur->getVariable(), cur->getType()->substitute(mgr, mapper));
//						if (!contains(newBindings, newBinding)) newBindings.push_back(newBinding);
//					}
//					bindings = newBindings;
//
//					// update definitions
//					def = builder.recTypeDefinition(bindings);
//				}
//
//				// convert structs into list of definitions
//
//				// build up new recursive type (only if it is closed)
//				return builder.recType(var, def);
//			}
//
//			/**
//			 * The conversion of recursive function is conducted lazily - first recursive
//			 * functions are build in an unrolled way before they are closed (by combining
//			 * multiple recursive definitions into a single one) by this function.
//			 *
//			 * @param lambda the unrolled recursive definition to be collapsed into a proper format
//			 * @return the proper format
//			 */
//			LambdaExprPtr fixRecursion(const LambdaExprPtr& lambda) {
//
//				// check whether this is the last free variable to be defined
//				VariablePtr recVar = lambda->getVariable();
//				auto freeVars = analysis::getFreeVariables(lambda->getLambda());
//				if (freeVars != toVector(recVar)) {
//					// it is not, delay closing recursion
//					return lambda;
//				}
//
//				// search all directly nested lambdas
//				vector<LambdaExprAddress> inner;
//				visitDepthFirstOncePrunable(NodeAddress(lambda), [&](const LambdaExprAddress& cur) {
//					if (cur.isRoot()) return false;
//					if (!analysis::hasFreeVariables(cur)) return true;
//					if (!recVars.contains(cur.as<LambdaExprPtr>()->getVariable())) return false;
//					inner.push_back(cur);
//					return false;
//				});
//
//				// if there is no inner lambda with free variables it is a simple recursion
//				if (inner.empty()) return lambda;		// => done
//
//				// ---------- build new recursive function ------------
//
//				auto& mgr = lambda.getNodeManager();
//				IRBuilder builder(mgr);
//
//				// build up resulting lambda
//				vector<LambdaBindingPtr> bindings;
//				bindings.push_back(builder.lambdaBinding(recVar, lambda->getLambda()));
//				for(auto cur : inner) {
//					assert(cur->getDefinition().size() == 1u);
//					auto def = cur->getDefinition()[0];
//
//					// only add every variable once
//					if (!any(bindings, [&](const LambdaBindingPtr& binding)->bool { return binding->getVariable() == def.getAddressedNode()->getVariable(); })) {
//						bindings.push_back(def);
//					}
//				}
//
//				LambdaExprPtr res = builder.lambdaExpr(recVar, builder.lambdaDefinition(bindings));
//
//				// collapse recursive definitions
//				while (true) {
//					// search for reductions (lambda => rec_variable)
//					std::map<NodeAddress, NodePtr> replacements;
//					visitDepthFirstPrunable(NodeAddress(res), [&](const LambdaExprAddress& cur) {
//						if (cur.isRoot()) return false;
//						if (!analysis::hasFreeVariables(cur)) return true;
//
//						// only focus on inner lambdas referencing recursive variables
//						auto var = cur.as<LambdaExprPtr>()->getVariable();
//						if (!res->getDefinition()->getBindingOf(var)) return false;
//						replacements[cur] = var;
//						return false;
//					});
//
//					// check whether the job is done
//					if (replacements.empty()) break;
//
//					// apply reductions
//					res = transform::replaceAll(mgr, replacements).as<LambdaExprPtr>();
//				}
//
//				// that's it
//				return res;
//			}
//
//			bool containsRecursiveVariable(const NodePtr& node) {
//
//				// check cached values
//				auto pos = containsRecVars.find(node);
//				if (pos != containsRecVars.end()) return pos->second;
//
//				// determine whether result contains a recursive variable
//				bool res = recVars.contains(node) ||
//						any(node->getChildList(), [&](const NodePtr& cur)->bool { return containsRecursiveVariable(cur); });
//
//				// save result
//				containsRecVars[node] = res;
//
//				// and return it
//				return res;
//			}
//
//			void resetContainsRecursiveVariableCache() {
////				utils::map::PointerMap<NodePtr, bool> newCache;
////				for(const auto& cur : containsRecVars) {
////					if (cur.second) {
////						newCache[cur.first] = true;
////					}
////				}
////				containsRecVars = newCache;
//			}
//
//		private:
//
//
//			bool allRecVarsBound(const TypePtr& type) {
//				// if it is a rec type it has been closed by the rec-type fixer
//				if (type.isa<RecTypePtr>()) return true;
//
//				// otherwise - if it contains any variable those are unbound variables!
//				bool freeVariable = false;
//				visitDepthFirstOncePrunable(type, [&](const TypePtr& type)->bool {
//					if (freeVariable) return true;
//					if (type.isa<TypeVariablePtr>()) {
//						freeVariable = true;
//						return true;
//					}
//
//					if (type.isa<RecTypePtr>()) {
//						return true;		// do not decent into closed recursive types
//					}
//
//					// everything else: continue
//					return false;
//
//				}, true);
//				return !freeVariable;
//			}
//
//			bool allRecVarsBound(const RecTypeDefinitionPtr& def) {
//				bool allRecVarsBound = true;
//				visitDepthFirstOncePrunable(def, [&](const TypePtr& type)->bool {
//
//					// if it already failed => no need to continue search
//					if (!allRecVarsBound) return true;
//
//					/**
//					 * WARNING: this implementation utilizes the fact that the frontend will NEVER
//					 * 		generate any types including type variables - all variables have been
//					 * 		introduced by this resolver as recursive-type variables!!
//					 */
//
//					// all encountered type variables are recursive type variables and need to be defined
//					if (auto typeVar = type.isa<TypeVariablePtr>()) {
//						if (!def->getDefinitionOf(typeVar)) {
//							allRecVarsBound = false;
//							return true;
//						}
//					}
//
//					// nested recursive types should all be closed => we can stop here
//					if (auto recType = type.isa<RecTypePtr>()) return true;
//
//					// otherwise continue
//					return false;
//				});
//				return allRecVarsBound;
//			}
//
//		};


		core::LambdaExprPtr addGlobalsInitialization(const IRTranslationUnit& unit, const core::LambdaExprPtr& mainFunc, Resolver& resolver){

			core::LambdaExprPtr internalMainFunc = mainFunc;

			// we only want to init what we use, so we check it
			core::NodeSet usedLiterals;
			core::visitDepthFirstOnce (internalMainFunc, [&] (const core::LiteralPtr& literal){
				usedLiterals.insert(literal);
			});

			// check all types for dtors which use literals
			for( auto cur : unit.getTypes()) {
				TypePtr def = cur.second;
				if (core::hasMetaInfo(def.isa<TypePtr>())) {
					auto dtor = core::getMetaInfo(def).getDestructor();
					core::visitDepthFirstOnce (dtor, [&] (const core::LiteralPtr& literal){
						usedLiterals.insert(literal);
					});
				}
			}

			core::IRBuilder builder(internalMainFunc->getNodeManager());
			core::StatementList inits;

			// check all used literals if they are used as global and the global type is vector
			// and the usedLiteral type is array, if so replace the used literal type to vector and
			// us ref.vector.to.ref.array
			core::NodeMap replacements;
			for (auto cur : unit.getGlobals()) {

				const LiteralPtr& global = resolver.apply(cur.first).as<LiteralPtr>();
				const TypePtr& globalTy= global->getType();

				if (!globalTy.isa<RefTypePtr>() || !globalTy.as<RefTypePtr>()->getElementType().isa<VectorTypePtr>()) continue;

				auto findLit = [&](const NodePtr& node) {
					const LiteralPtr& usedLit = resolver.apply(node).as<LiteralPtr>();
					const TypePtr& usedLitTy = usedLit->getType();

					if (!usedLitTy.isa<RefTypePtr>()) return false;

					return usedLit->getStringValue() == global->getStringValue() &&
						usedLitTy.as<RefTypePtr>()->getElementType().isa<ArrayTypePtr>() &&
						types::isSubTypeOf(globalTy, usedLitTy);
				};

				if(any(usedLiterals,findLit)) {
					// get the literal
					LiteralPtr toReplace = resolver.apply((*std::find_if(usedLiterals.begin(), usedLiterals.end(), findLit)).as<LiteralPtr>());
					LiteralPtr global = resolver.apply(cur.first);

					//update usedLiterals to the "new" literal
					usedLiterals.erase(toReplace);
					usedLiterals.insert(global);

					//fix the access
					ExpressionPtr replacement = builder.callExpr( toReplace.getType(), builder.getLangBasic().getRefVectorToRefArray(), global);

					replacements.insert( {toReplace, replacement} );
				}
			}
			internalMainFunc = transform::replaceAll(internalMainFunc->getNodeManager(), internalMainFunc, replacements, false).as<LambdaExprPtr>();

			// ~~~~~~~~~~~~~~~~~~ INITIALIZE GLOBALS ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			for (auto cur : unit.getGlobals()) {

				// only consider having an initialization value
				if (!cur.second) continue;

				core::LiteralPtr newLit = resolver.apply(cur.first);

				if (!contains(usedLiterals, newLit)) continue;

				inits.push_back(builder.assign(resolver.apply(newLit), resolver.apply(cur.second)));
			}

			// ~~~~~~~~~~~~~~~~~~ PREPARE STATICS ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			const lang::StaticVariableExtension& ext = internalMainFunc->getNodeManager().getLangExtension<lang::StaticVariableExtension>();
			for (auto cur : usedLiterals) {
				auto lit = cur.as<LiteralPtr>();
				// only consider static variables
				auto type = lit->getType();
				if (!type.isa<RefTypePtr>() || !ext.isStaticType(type.as<RefTypePtr>()->getElementType())) continue;
				// add creation statement
				inits.push_back(builder.createStaticVariable(lit));
			}

			// fix the external markings
			for(auto cur : usedLiterals) {
				auto type = cur.as<LiteralPtr>()->getType();
				insieme::annotations::c::markExtern(cur.as<LiteralPtr>(),
						type.isa<RefTypePtr>() &&
						cur.as<LiteralPtr>()->getStringValue()[0]!='\"' &&  // not an string literal -> "hello world\n"
						!insieme::annotations::c::hasIncludeAttached(cur) &&
						!ext.isStaticType(type.as<RefTypePtr>()->getElementType()) &&
						!any(unit.getGlobals(), [&](const IRTranslationUnit::Global& global) { return *resolver.apply(global.first) == *cur; })
				);
			}

			// build resulting lambda
			if (inits.empty()) return internalMainFunc;

			return core::transform::insert ( internalMainFunc->getNodeManager(), core::LambdaExprAddress(internalMainFunc)->getBody(), inits, 0).as<core::LambdaExprPtr>();
		}

		core::LambdaExprPtr addInitializer(const IRTranslationUnit& unit, const core::LambdaExprPtr& mainFunc) {

			// check whether there are any initializer expressions
			if (unit.getInitializer().empty()) return mainFunc;

			// insert init statements
			auto initStmts = ::transform(unit.getInitializer(), [](const ExpressionPtr& cur)->StatementPtr { return cur; });
			return core::transform::insert( mainFunc->getNodeManager(), core::LambdaExprAddress(mainFunc)->getBody(), initStmts, 0).as<core::LambdaExprPtr>();
		}

	} // end anonymous namespace


	core::NodePtr IRTranslationUnit::resolve(const core::NodePtr& node) const {
		return Resolver(getNodeManager(), *this).apply(node);
	}

	void IRTranslationUnit::extractMetaInfos() const {
		for(auto m : metaInfos) {
			auto classType = m.first;
			auto metaInfoList = m.second;  //metaInfos per classType
			
			//merge metaInfos into one 
			core::ClassMetaInfo metaInfo;
			for(auto m : metaInfoList) {
				metaInfo = core::merge(metaInfo, m);
			}

			//std::cout << classType << " : " << metaInfo <<  std::endl;
			auto resolvedClassType = this->resolve(classType).as<core::TypePtr>();	

			// encode meta info into pure IR
			auto encoded = core::encoder::toIR(getNodeManager(), metaInfo);
			
			// resolve meta info
			auto resolved = core::encoder::toValue<ClassMetaInfo>(this->resolve(encoded).as<core::ExpressionPtr>());

			// attach metainfo to type
			core::setMetaInfo(resolvedClassType, resolved);
		}
	}
	
	void IRTranslationUnit::addMetaInfo(const core::TypePtr& classType, core::ClassMetaInfo metaInfo) {
		metaInfos[classType].push_back(metaInfo);
	}

	void IRTranslationUnit::addMetaInfo(const core::TypePtr& classType, std::vector<core::ClassMetaInfo> metaInfoList) {
		for(auto m : metaInfoList) {
			metaInfos[classType].push_back(m);
		}
	}

	core::ProgramPtr toProgram(core::NodeManager& mgr, const IRTranslationUnit& a, const string& entryPoint) {
		
		//before leaving the realm of the irtu take care of metainfos...
		a.extractMetaInfos();

		// search for entry point
		core::IRBuilder builder(mgr);
		for (auto cur : a.getFunctions()) {

			if (cur.first->getStringValue() == entryPoint) {

				// get the symbol
				core::NodePtr symbol = cur.first;
std::cout << "Starting resolving symbol " << symbol << " ...\n";

				// extract lambda expression
				Resolver resolver(mgr, a);
				core::LambdaExprPtr lambda = resolver.apply(symbol).as<core::LambdaExprPtr>();
std::cout << "Adding initializers ...\n";
				// add initializer
				lambda = addInitializer(a, lambda);
std::cout << "Adding globals ...\n";
				// add global initializers
				lambda = addGlobalsInitialization(a, lambda, resolver);
std::cout << "Almost done ...\n";
				// wrap into program
				return builder.program(toVector<core::ExpressionPtr>(lambda));
			}
		}

		assert_fail() << "No such entry point!\n"
				<< "Searching for: " << entryPoint << "\n";
		return core::ProgramPtr();
	}


	core::ProgramPtr resolveEntryPoints(core::NodeManager& mgr, const IRTranslationUnit& a) {

		//before leaving the realm of the irtu take care of metainfos...
		a.extractMetaInfos();

		// convert entry points stored within TU int a program
		core::ExpressionList entryPoints;
		Resolver creator(mgr, a);
		for(auto cur : a.getEntryPoints()) {
			entryPoints.push_back(creator.apply(cur.as<core::ExpressionPtr>()));
		}
		return core::IRBuilder(mgr).program(entryPoints);
	}


} // end namespace tu
} // end namespace frontend
} // end namespace insieme

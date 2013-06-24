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

#include <algorithm>

#include "insieme/transform/pattern/pattern.h"

#include "insieme/core/ir.h"

#include "insieme/utils/container_utils.h"
#include "insieme/utils/map_utils.h"
#include "insieme/utils/unused.h"

#include "insieme/utils/assert.h"

namespace insieme {

// a translation-unit specific handling of the hashing of node pointers and addresses
namespace core {

	size_t hash_value(const NodePtr& node) {
		return (size_t)(node.ptr);
	}

	size_t hash_value(const NodeAddress& address) {
		return address.hash();
	}

} // end namespace core


namespace transform {
namespace pattern {

	namespace details {

		bool isTypeOrValueOrParam(const core::NodeType type) {
			return core::isA<core::NC_Type>(type) ||
					core::isA<core::NC_IntTypeParam>(type) ||
					core::isA<core::NC_Value>(type);
		}

		/**
		 * This function is implementing the actual matching algorithm.
		 */
		template<typename T, typename target = typename match_target_info<T>::target_type>
		boost::optional<Match<target>> match(const TreePattern& pattern, const T& node);

		/**
		 * This function is implementing the actual matching algorithm for list patterns.
		 */
		template<typename T, typename target = typename match_target_info<T>::target_type>
		boost::optional<Match<target>> match(const ListPattern& pattern, const std::vector<T>& trees);

	}

	MatchOpt TreePattern::matchPointer(const core::NodePtr& node) const {
		return details::match(*this, node);
	}

	AddressMatchOpt TreePattern::matchAddress(const core::NodeAddress& node) const {
		return details::match(*this, node);
	}

	TreeMatchOpt TreePattern::matchTree(const TreePtr& tree) const {
		return details::match(*this, tree);
	}

	TreeMatchOpt ListPattern::match(const vector<TreePtr>& trees) const {
		return details::match(*this, trees);
	}


	const TreePatternPtr any = std::make_shared<tree::Wildcard>();
	const TreePatternPtr recurse = std::make_shared<tree::Recursion>("x");

	const ListPatternPtr anyList = *any;
	const ListPatternPtr empty = std::make_shared<list::Empty>();

	namespace tree {

		const TreePatternPtr Variable::any = pattern::any;

	}

	namespace list {

		const ListPatternPtr Variable::any = pattern::anyList;
	}


	// -------------------------------------------------------------------------------------
	//   Pattern Matcher
	// -------------------------------------------------------------------------------------


	size_t hash_value(const TreePtr& tree) {
		return (size_t)(tree.get());
	}

	// -- Implementation detail -------------------------------------------

	namespace details {


		enum CachedMatchResult {
			Yes, No, Unknown
		};


		template<typename T>
		class MatchContext : public utils::Printable, private boost::noncopyable {

		public:
			typedef typename T::value_type value_type;
			typedef typename T::atom_type atom_type;
			typedef typename T::list_type list_type;
			typedef typename T::list_iterator iterator;

			// a cache for tree patterns not including variables
			typedef std::map<std::pair<const TreePattern*, typename T::atom_type>, bool> tree_pattern_cache;

			struct RecVarInfo {
				TreePatternPtr pattern;
				unsigned level;
				unsigned counter;

				RecVarInfo(TreePatternPtr pattern = TreePatternPtr(), unsigned level = 0)
					: pattern(pattern), level(level), counter(0) {}
			};

		private:

			typedef std::unordered_map<string, RecVarInfo> RecVarMap;

			MatchPath path;

			Match<T> match;

			RecVarMap boundRecursiveVariables;

			tree_pattern_cache treePatternCache;

		public:

			MatchContext(const value_type& root = value_type()) : match(root), treePatternCache() { }


			const Match<T>& getMatch() const {
				return match;
			}

			// -- The Match Path ---------------------------

			void push() {
				path.push(0);
			}

			void inc() {
				path.inc();
			}

			void dec() {
				path.dec();
			}

			void pop() {
				path.pop();
			}

			std::size_t get() const {
				return path.get();
			}

			void set(std::size_t index) {
				path.set(index);
			}

			std::size_t getDepth() const {
				return path.getDepth();
			}

			const MatchPath& getCurrentPath() const {
				return path;
			}

			MatchPath& getCurrentPath() {
				return path;
			}

			void setCurrentPath(const MatchPath& newPath) {
				path = newPath;
			}

			// -- Tree Variables ---------------------------

			bool isTreeVarBound(const std::string& var) const {
				return match.isTreeVarBound(path, var);
			}

			void bindTreeVar(const std::string& var, const value_type& value) {
				match.bindTreeVar(path, var, value);
			}

			void unbindTreeVar(const std::string& var) {
				match.unbindTreeVar(path, var);
			}

			const value_type& getTreeVarBinding(const std::string& var) const {
				return match.getTreeVarBinding(path, var);
			}

			// -- List Variables --------------------------

			bool isListVarBound(const std::string& var) const {
				return match.isListVarBound(path, var);
			}

			void bindListVar(const std::string& var, const iterator& begin, const iterator& end) {
				match.bindListVar(path, var, begin, end);
			}

			void unbindListVar(const std::string& var) {
				match.unbindListVar(path, var);
			}

			list_type getListVarBinding(const std::string& var) const {
				return match.getListVarBinding(path, var);
			}

			// -- Recursive Variables ---------------------------

			bool isRecVarBound(const std::string& var) const {
				return boundRecursiveVariables.find(var) != boundRecursiveVariables.end();
			}

			void bindRecVar(const std::string& var, const TreePatternPtr& pattern) {
				assert(!isRecVarBound(var) && "Variable bound twice");
				boundRecursiveVariables.insert(std::make_pair(var, RecVarInfo(pattern, path.getDepth())));
			}

			void bindRecVar(const std::string& var, const RecVarInfo& info) {
				assert(!isRecVarBound(var) && "Variable bound twice");
				boundRecursiveVariables.insert(std::make_pair(var, info));
			}

			TreePatternPtr getRecVarBinding(const std::string& var) const {
				assert(isRecVarBound(var) && "Requesting bound value for unbound tree variable");
				return boundRecursiveVariables.find(var)->second.pattern;
			}

			const RecVarInfo& getRecVarInfo(const std::string& var) const {
				assert(isRecVarBound(var) && "Requesting bound value for unbound tree variable");
				return boundRecursiveVariables.find(var)->second;
			}

			unsigned getRecVarDepth(const std::string& var) const {
				assert(isRecVarBound(var) && "Requesting bound value for unbound tree variable");
				return boundRecursiveVariables.find(var)->second.level;
			}

			unsigned getRecVarCounter(const std::string& var) const {
				assert(isRecVarBound(var) && "Requesting bound value for unbound tree variable");
				return boundRecursiveVariables.find(var)->second.counter;
			}

			unsigned incRecVarCounter(const std::string& var) {
				assert(isRecVarBound(var) && "Requesting bound value for unbound tree variable");
				return ++(boundRecursiveVariables.find(var)->second.counter);
			}

			void unbindRecVar(const std::string& var) {
				boundRecursiveVariables.erase(var);
			}

			// -- Cached Match Results -----------------------------

			CachedMatchResult cachedMatch(const TreePattern& pattern, const atom_type& node) const {
				assert(pattern.isVariableFree && "Can only cache variable-free pattern fragments!");

				auto pos = treePatternCache.find(std::make_pair(&pattern, node));
				if (pos == treePatternCache.end()) {
					return Unknown;
				}
				return (pos->second)?Yes:No;
			}

			void addToCache(const TreePattern& pattern, const value_type& node, bool match) {
				treePatternCache[std::make_pair(&pattern, node)] = match;
			}

			virtual std::ostream& printTo(std::ostream& out) const {
				out << "Match(";
				out << path << ", ";
				out << match << ", ";
				out << "{" << join(",", boundRecursiveVariables,
						[](std::ostream& out, const std::pair<string, RecVarInfo>& cur) {
							out << cur.first << "=" << cur.second.pattern;
				}) << "}";
				return out << ")";
			}

			// -- Backup and Restore --------------------------------------

			struct MatchContextBackup {

				/**
				 * A reference to the context this backup is based on.
				 * Backups may only be restored for the same context.
				 */
				const MatchContext& context;

				const IncrementID backup;

				explicit MatchContextBackup(const MatchContext& data)
					: context(data),
					  backup(data.match.backup())
					  {}

				void restore(MatchContext& target) {
					assert(&target == &context && "Unable to restore different context!");

					// restore match context content
					//	- path can be ignored since it is continuously maintained
					//  - recursive variables are also not required to be checked (handled automatically)

					// the variable bindings need to be re-set
					target.match.restore(backup);
				}
			};

			MatchContextBackup backup() const {
				return MatchContextBackup(*this);
			}

			void restore(const MatchContextBackup& backup) {
				backup.restore(*this);
			}
		};


		template<typename T>
		bool match(const TreePattern& pattern, MatchContext<T>& context, const typename T::value_type& tree, const std::function<bool(MatchContext<T>&)>& delayedCheck);

		template<typename T, typename iterator = typename T::value_iterator>
		bool match(const ListPattern& pattern, MatchContext<T>& context, const iterator& begin, const iterator& end, const std::function<bool(MatchContext<T>&)>& delayedCheck);

		template<typename T>
		inline bool match(const TreePatternPtr& pattern, MatchContext<T>& context, const typename T::value_type& tree, const std::function<bool(MatchContext<T>&)>& delayedCheck) {
			return match(*pattern.get(), context, tree, delayedCheck);
		}

		template<typename T, typename iterator = typename T::value_iterator>
		inline bool match(const ListPatternPtr& pattern, MatchContext<T>& context, const iterator& begin, const iterator& end, const std::function<bool(MatchContext<T>&)>& delayedCheck) {
			return match(*pattern.get(), context, begin, end, delayedCheck);
		}


		template<typename T, typename target = typename match_target_info<T>::target_type>
		boost::optional<Match<target>> match(const TreePattern& pattern, const T& tree) {
			MatchContext<target> context(tree);
			std::function<bool(MatchContext<target>&)> accept = [](MatchContext<target>& context)->bool { return true; };
			if (match(pattern, context, tree, accept)) {
				// it worked => return match result
				return context.getMatch();
			}
			return 0;
		}

		template<typename T, typename target = typename match_target_info<T>::target_type>
		boost::optional<Match<target>> match(const ListPattern& pattern, const std::vector<T>& trees) {
			MatchContext<target> context;
			std::function<bool(MatchContext<target>&)> accept = [](MatchContext<target>& context)->bool { return true; };
			if (match(pattern, context, trees.begin(), trees.end(), accept)) {
				// => it is a match (but leaf root empty)
				return context.getMatch();
			}
			return 0;
		}

		template<typename T, typename target = typename match_target_info<T>::target_type>
		boost::optional<Match<target>> match(const TreePatternPtr& pattern, const T& tree) {
			return match(*pattern.get(), tree);
		}

		template<typename T, typename target = typename match_target_info<T>::target_type>
		boost::optional<Match<target>> match(const ListPatternPtr& pattern, const std::vector<T>& trees) {
			return match(*pattern.get(), trees);
		}


		// -- Match Tree Patterns -------------------------------------------------------

		// Atom, Variable, Wildcard, Node, Negation, Alternative, Descendant, Recursion

		namespace tree {

		#define MATCH(NAME) \
			template<typename T> \
			bool match ## NAME (const pattern::tree::NAME& pattern, MatchContext<T>& context, const typename T::value_type& tree, const std::function<bool(MatchContext<T>&)>& delayedCheck)

			MATCH(Value) {
				return tree->isValue() && tree->getNodeValue() == pattern.value && delayedCheck(context);
			}

			MATCH(Constant) {
				assert(pattern.nodeAtom && "Wrong type of constant value stored within atom node!");
				return *pattern.nodeAtom == *tree && delayedCheck(context);
			}

			MATCH(Wildcard) {
				return delayedCheck(context);	// just finish delayed checks
			}

			MATCH(Variable) {

				// check whether the variable is already bound
				if(context.isTreeVarBound(pattern.name)) {
					return *context.getTreeVarBinding(pattern.name) == *tree && delayedCheck(context);
				}

				// check filter-pattern of this variable
				context.bindTreeVar(pattern.name, tree);		// speculate => bind variable
				return match(pattern.pattern, context, tree, delayedCheck);
			}

			namespace {

				bool contains_variable_free(MatchContext<ptr_target>& context, const core::NodePtr& tree, const TreePatternPtr& pattern, const std::function<bool(MatchContext<ptr_target>&)>& delayedCheck) {
					return core::visitDepthFirstOnceInterruptible(tree, [&](const core::NodePtr& cur)->bool {
						return match(pattern, context, cur, delayedCheck);
					}, pattern->mayBeType);
				}

				bool contains_variable_free(MatchContext<address_target>& context, const core::NodeAddress& tree, const TreePatternPtr& pattern, const std::function<bool(MatchContext<address_target>&)>& delayedCheck) {
					return core::visitDepthFirstOnceInterruptible(tree.as<core::NodePtr>(), [&](const core::NodePtr& cur)->bool {
						return match(pattern, context, core::NodeAddress(cur), delayedCheck);
					}, pattern->mayBeType);
				}

				bool contains_variable_free(MatchContext<tree_target>& context, const TreePtr& tree, const TreePatternPtr& pattern, const std::function<bool(MatchContext<tree_target>&)>& delayedCheck) {
					bool res = false;
					res = res || match(pattern, context, tree, delayedCheck);
					for_each(tree->getChildList(), [&](const TreePtr& cur) { // generalize this
						res = res || contains_variable_free(context, cur, pattern, delayedCheck);
					});
					return res;
				}

				bool contains_with_variables(MatchContext<ptr_target>& context, const core::NodePtr& tree, const TreePatternPtr& pattern, const std::function<bool(MatchContext<ptr_target>&)>& delayedCheck) {
					// if there are variables, the context needs to be reset
					auto backup = context.backup();
					return core::visitDepthFirstOnceInterruptible(tree, [&](const core::NodePtr& cur)->bool {
						backup.restore(context);		// restore context
						return match(pattern, context, cur, delayedCheck);
					}, true, pattern->mayBeType);
				}

				bool contains_with_variables(MatchContext<address_target>& context, const core::NodeAddress& tree, const TreePatternPtr& pattern, const std::function<bool(MatchContext<address_target>&)>& delayedCheck) {
					// if there are variables, the context needs to be reset
					auto backup = context.backup();
					// for addresses everything has to be visited (no visit once)
					return core::visitDepthFirstInterruptible(tree, [&](const core::NodeAddress& cur)->bool {
						backup.restore(context);		// restore context
						return match(pattern, context, cur, delayedCheck);
					}, true, pattern->mayBeType);
				}

				bool contains_with_variables(MatchContext<tree_target>& context, const TreePtr& tree, const TreePatternPtr& pattern, const std::function<bool(MatchContext<tree_target>&)>& delayedCheck) {
					// if there are variables, all nodes need to be checked
					bool res = false;

					// isolate context for each try
					auto backup = context.backup();
					res = res || match(pattern, context, tree, delayedCheck);
					for_each(tree->getChildList(), [&](const TreePtr& cur) { // generalize this
						if (!res) backup.restore(context);		// restore context
						res = res || contains_with_variables(context, cur, pattern, delayedCheck);
					});
					return res;
				}

			}

			template<typename T>
			bool contains(MatchContext<T>& context, const typename T::value_type& tree, const TreePatternPtr& pattern, const std::function<bool(MatchContext<T>&)>& delayedCheck) {

				// prune types
				if (!pattern->mayBeType && isTypeOrValueOrParam(tree)) return false;

				// if variable free, only non-shared nodes need to be checked
				if (pattern->isVariableFree) {
					return contains_variable_free(context, tree, pattern, delayedCheck);
				}

				// use version considering variables
				return contains_with_variables(context, tree, pattern, delayedCheck);
			}

			MATCH(Descendant) {
				// search for all patterns occurring in the sub-trees
				return ::all(pattern.subPatterns, [&](const TreePatternPtr& cur) {
					return contains(context, tree, cur, delayedCheck);
				});
			}

			MATCH(Recursion) {
				// handle terminal
				if(pattern.terminal) {
					// get pattern bound to recursive variable
					assert(context.isRecVarBound(pattern.name) && "Recursive variable unbound!");

					// save current context path
					MatchPath path = context.getCurrentPath();

					// restore recursion level of outer recursive scope
					unsigned recLevel = context.getRecVarDepth(pattern.name);
					context.getCurrentPath().prune(recLevel);

					// update number of recursion applications
					context.set(context.incRecVarCounter(pattern.name));

					// run match again
					bool res = match(context.getRecVarBinding(pattern.name), context, tree, delayedCheck);

					// restore current context path
					context.setCurrentPath(path);
					return res;
				}

				// start of recursion => bind recursive variable and handle context
				context.push();

				// safe current value of the recursive variable
				bool preBound = context.isRecVarBound(pattern.name);
				typename MatchContext<T>::RecVarInfo oldInfo;
				if (preBound) {
					assert(context.getDepth() > context.getRecVarDepth(pattern.name) && "Nested recursive variables must not be on same level!");
					oldInfo = context.getRecVarInfo(pattern.name);
					context.unbindRecVar(pattern.name);
				}

				// start by ignoring delayed checks
				std::function<bool(MatchContext<T>&)> accept = [](MatchContext<T>& context) { return true; };

				// match using new rec-var binding
				context.bindRecVar(pattern.name, pattern.pattern);
				bool res = match(pattern.pattern, context, tree, accept);

				// remove binding
				context.unbindRecVar(pattern.name);
				context.pop();

				// restore old recursive variable if necessary
				if (preBound) context.bindRecVar(pattern.name, oldInfo);

				// run remaining delayed checks
				return res && delayedCheck(context);
			}

			MATCH(Node) {
				if (pattern.type != -1 && pattern.type != tree->getNodeType()) return false;
				const auto& children = tree->getChildList();
				return match(pattern.pattern, context, children.begin(), children.end(), delayedCheck);
			}

			MATCH(Negation) {
				// backup current state - negation operates on isolated context (what shouldn't be shouldn't leaf traces)
				auto backup = context.backup();

				// ignore delayed checks while matching inner block and conduct those checks if inner one fails
				std::function<bool(MatchContext<T>&)> accept = [](MatchContext<T>& context) { return true; };
				bool fits = !match(pattern.pattern, context, tree, accept);

				// save us the effort of restoring the old context
				if (!fits) return false;

				// restore context
				backup.restore(context);

				// finish by processing delayed checks on the original context
				return delayedCheck(context);
			}

			MATCH(Conjunction) {
				// match first and delay matching of second half
				std::function<bool(MatchContext<T>&)> delayed = [&](MatchContext<T>& context) { return match(pattern.pattern2, context, tree, delayedCheck); };
				return match(pattern.pattern1, context, tree, delayed);
			}

			MATCH(Disjunction) {
				// create context backup for rollback
				auto backup = context.backup();
				if (match(pattern.pattern1, context, tree, delayedCheck)) return true;
				// restore context
				backup.restore(context);
				return match(pattern.pattern2, context, tree, delayedCheck);
			}

			// -- for test structure only --

			// a specialization for tree pointers
			inline bool matchValue(const pattern::tree::Value& pattern, MatchContext<tree_target>& context, const TreePtr& tree, const std::function<bool(MatchContext<tree_target>&)>& delayedCheck) {
				return false;
			}

			// a specialization for tree pointers
			inline bool matchConstant(const pattern::tree::Constant& pattern, MatchContext<tree_target>& context, const TreePtr& tree, const std::function<bool(MatchContext<tree_target>&)>& delayedCheck) {
				assert(pattern.treeAtom && "Wrong type of constant value stored within atom node!");
				return *pattern.treeAtom == *tree && delayedCheck(context);
			}

			// a specialization for tree pointers
			inline bool matchNode(const pattern::tree::Node& pattern, MatchContext<tree_target>& context, const TreePtr& tree, const std::function<bool(MatchContext<tree_target>&)>& delayedCheck) {
				if (pattern.id != -1 && pattern.id != tree->getId()) return false;
				auto& children = tree->getSubTrees();
				return match(pattern.pattern, context, children.begin(), children.end(), delayedCheck) && delayedCheck(context);
			}


		#undef MATCH

		} // end namespace tree

		namespace list {

		// Empty, Single, Variable, Alternative, Sequence, Repedition

		#define MATCH(NAME) \
			template<typename T, typename iterator = typename T::value_iterator> \
			bool match ## NAME (const pattern::list::NAME& pattern, MatchContext<T>& context, const iterator& begin, const iterator& end, const std::function<bool(MatchContext<T>&)>& delayedCheck)

			MATCH(Empty) {
				// only accepts empty list
				return begin == end && delayedCheck(context);
			}

			MATCH(Single) {
				// range has to be exactly one ...
				if (std::distance(begin, end) != 1) return false;
				// ... and the pattern has to match
				return match(pattern.element, context, *begin, delayedCheck);
			}

			MATCH(Variable) {

				// check whether the variable is already bound
				if(context.isListVarBound(pattern.name)) {
					const auto& value = context.getListVarBinding(pattern.name);
					return std::distance(begin, end) == std::distance(value.begin(), value.end()) &&
						   std::equal(begin, end, value.begin(), equal_target<typename T::value_type>()) &&
						   delayedCheck(context);
				}

				// check filter-pattern of this variable
				context.bindListVar(pattern.name, begin, end);	// speculate => bind variable

				// check whether the tree is a valid substitution for this variable
				return match(pattern.pattern, context, begin, end, delayedCheck);
			}

			MATCH(Sequence) {

				unsigned length = std::distance(begin, end);
				assert(length >= pattern.minLength && length <= pattern.maxLength);

				// compute sub-range to be searched
				auto min = pattern.left->minLength;
				auto max = length - pattern.right->minLength;

				// constrain min based on max length of right side
				if (length > pattern.right->maxLength) {
					min = std::max(min, length - pattern.right->maxLength);
				}

				// constrain max based on max length of left side
				if (length > pattern.left->maxLength) {
					max = std::min(max, pattern.left->maxLength);
				}

				// special case: only one split point => safe a backup
				if (min == max) {
					std::function<bool(MatchContext<T>&)> delayed = [&](MatchContext<T>& context) { return match(pattern.right, context, begin+min, end, delayedCheck); };
					return match(pattern.left, context, begin, begin+min, delayed);
				}

				// search for the split-point ...
				auto backup = context.backup();
				for(auto i = begin+min; i<=begin+max; ++i) {
					backup.restore(context);
					// check left side and delay right side
					std::function<bool(MatchContext<T>&)> delayed = [&](MatchContext<T>& context) { return match(pattern.right, context, i, end, delayedCheck); };
					if (match(pattern.left, context, begin, i, delayed)) {
						return true;
					}
				}
				return false;
			}

			MATCH(Alternative) {
				// try both alternatives using a private context
				auto backup = context.backup();
				if (match(pattern.alternative1, context, begin, end, delayedCheck)) {
					return true;
				}

				// try alternative after reseting context
				backup.restore(context);
				return match(pattern.alternative2, context, begin, end, delayedCheck);
			}

			template<typename T, typename iterator = typename T::value_iterator>
			bool matchRepetitionInternal(
					const pattern::list::Repetition& rep, MatchContext<T>& context,
					const iterator& begin, const iterator& end, unsigned repetitions,
					const std::function<bool(MatchContext<T>&)>& delayedCheck) {

				// empty is accepted (terminal case)
				if (begin == end) {
					return repetitions >= rep.minRep && delayedCheck(context);
				}

				// test special case of a single iteration
				auto backup = context.backup();
				if (rep.minRep <= 1) {
					// try whether a single repetition is sufficient
					if (match(rep.pattern, context, begin, end, delayedCheck)) {
						return true;
					}
					// undo changes
					backup.restore(context);
				}

				// compute sub-range to be searched
				unsigned length = std::distance(begin, end);
				assert(length >= rep.pattern->minLength);

				auto min = rep.pattern->minLength;
				auto max = std::min(rep.pattern->maxLength, length - min * (rep.minRep - repetitions));		// max length or length - min space required for remaining repetitions

				// try one pattern + a recursive repetition
				for (auto i=begin+min; i<=begin+max; ++i) {

					// restore context for this attempt
					backup.restore(context);

					if (!match(rep.pattern, context, begin, i, delayedCheck)) {
						// does not match ... try next!
						continue;
					}

					// increment repetition counter
					context.inc();

					if (!matchRepetitionInternal(rep, context, i, end, repetitions + 1, delayedCheck)) {
						// does not fit any more ... try next!
						continue;
					}

					return true;
				}

				// the pattern does not match!
				return false;
			}

			MATCH(Repetition) {

				// special case: repetition of a wildcard
				if (pattern.pattern->type == ListPattern::Single &&
						static_pointer_cast<insieme::transform::pattern::list::Single>(pattern.pattern)->element->type == TreePattern::Wildcard) {
					assert(std::distance(begin, end) >= pattern.minRep);
					return delayedCheck(context);
				}

				// increase nesting level of variables by one
				context.push();

				// accept everything until repetition is complete
				std::function<bool(MatchContext<T>&)> accept = [](MatchContext<T>& context) { return true; };
				bool res = matchRepetitionInternal(pattern, context, begin, end, 0, accept);

				// drop extra level
				context.pop();

				// conduct delayed checks if necessary
				return res & delayedCheck(context);
			}

		#undef MATCH

		} // end namespace list

		namespace {

			template<typename T>
			bool match_internal(const TreePattern& pattern, MatchContext<T>& context, const typename T::value_type& tree, const std::function<bool(MatchContext<T>&)>& delayedCheck) {
				switch(pattern.type) {
					#define CASE(NAME) case TreePattern::NAME : return tree::match ## NAME (static_cast<const pattern::tree::NAME&>(pattern), context, tree, delayedCheck)
						CASE(Value);
						CASE(Constant);
						CASE(Variable);
						CASE(Wildcard);
						CASE(Node);
						CASE(Negation);
						CASE(Conjunction);
						CASE(Disjunction);
						CASE(Descendant);
						CASE(Recursion);
					#undef CASE
				}
				assert(false && "Missed a pattern type!");
				return false;
			}
		}

		template<typename T>
		bool match(const TreePattern& pattern, MatchContext<T>& context, const typename T::value_type& tree, const std::function<bool(MatchContext<T>&)>& delayedCheck) {

			const bool DEBUG = false;

			if (DEBUG) std::cout << "Matching " << pattern << " against " << tree << " with context " << context << " ... \n";
			assert_decl(auto path = context.getCurrentPath());

			// quick check for wildcards (should not even be cached)
			if (pattern.type == TreePattern::Wildcard) return delayedCheck(context);

			// skip searching within types if not searching for a type
			if (!pattern.mayBeType && isTypeOrValueOrParam(tree)) return false;

			// use cache if possible
			if (pattern.isVariableFree) {
				CachedMatchResult cachRes = context.cachedMatch(pattern, tree);
				if (cachRes != Unknown) {
					bool res = (cachRes == Yes) && delayedCheck(context);
					if (DEBUG) std::cout << "Matching " << pattern << " against " << tree << " with context " << context << " ... - from cache: " << res << "\n";
					return res;
				}

				// resolve without delayed checks and save result
				std::function<bool(MatchContext<T>&)> accept = [](MatchContext<T>& context) { return true; };
				bool res = match_internal(pattern, context, tree, accept);
				context.addToCache(pattern, tree, res);

				// return result + delayed checks
				res = res && delayedCheck(context);
				if (DEBUG) std::cout << "Matching " << pattern << " against " << tree << " with context " << context << " ... - added to cache: " << res << "\n";
				return res;
			}

			// for all the rest, use non-cached inner implementation
			bool res = match_internal(pattern, context, tree, delayedCheck);
			if (DEBUG) std::cout << "Matching " << pattern << " against " << tree << " with context " << context << " ... - matched: " << res << "\n";

			// check correct handling of paths
			assert_eq(path, context.getCurrentPath());

			return res;
		}


		template<typename T, typename iterator = typename T::value_iterator>
		bool match(const ListPattern& pattern, MatchContext<T>& context, const iterator& begin, const iterator& end, const std::function<bool(MatchContext<T>&)>& delayedCheck) {

			const bool DEBUG = false;

			if (DEBUG) std::cout << "Matching " << pattern << " against " << join(", ", begin, end, print<deref<typename T::value_type>>()) << " with context " << context << " ... \n";
			assert_decl(auto path = context.getCurrentPath());

			// quick check length
			auto length = std::distance(begin, end);
			if (pattern.minLength > length || length > pattern.maxLength) {
				if (DEBUG) std::cout << "Matching " << pattern << " against " << join(", ", begin, end, print<deref<typename T::value_type>>()) << " with context " << context << " ... skipped due to length limit. \n";
				return false;		// will not match
			}

			bool res = false;
			switch(pattern.type) {
				#define CASE(NAME) case ListPattern::NAME : res = list::match ## NAME (static_cast<const pattern::list::NAME&>(pattern), context, begin, end, delayedCheck); break
					CASE(Empty);
					CASE(Single);
					CASE(Variable);
					CASE(Alternative);
					CASE(Sequence);
					CASE(Repetition);
				#undef CASE
			}

			if (DEBUG) std::cout << "Matching " << pattern << " against " << join(", ", begin, end, print<deref<typename T::value_type>>()) << " with context " << context << " ...  match: " << res << "\n";

			// check correct handling of paths
			assert_eq(path, context.getCurrentPath());

			return res;
		}

	} // end namespace details


} // end namespace pattern
} // end namespace transform
} // end namespace insieme

namespace std {

	std::ostream& operator<<(std::ostream& out, const insieme::transform::pattern::PatternPtr& pattern) {
		return (pattern)?(pattern->printTo(out)):(out << "null");
	}

	std::ostream& operator<<(std::ostream& out, const insieme::transform::pattern::TreePatternPtr& pattern) {
		return (pattern)?(pattern->printTo(out)):(out << "null");
	}

	std::ostream& operator<<(std::ostream& out, const insieme::transform::pattern::ListPatternPtr& pattern) {
		return (pattern)?(pattern->printTo(out)):(out << "null");
	}

} // end namespace std

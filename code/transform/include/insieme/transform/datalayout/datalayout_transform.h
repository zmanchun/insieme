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

#include "insieme/core/transform/node_mapper_utils.h"
#include "insieme/core/transform/node_replacer.h"

#include "insieme/core/pattern/pattern.h"

namespace insieme {

namespace analysis {
typedef std::map<core::VariableAddress, core::CompoundStmtAddress> VariableScopeMap;
}

namespace core {
namespace transform {
typedef std::function<StatementPtr(const StatementPtr&)> TypeHandler;
}
}

namespace transform {
namespace datalayout {

typedef utils::map::PointerMap<core::ExpressionAddress, core::RefTypePtr> ExprAddressRefTypeMap;
typedef utils::map::PointerMap<core::ExpressionAddress, core::ExpressionPtr> ExprAddressMap;
typedef utils::set::PointerSet<core::ExpressionAddress> ExprAddressSet; /*
typedef std::set<core::ExpressionAddress> ExprAddressSet; // */
typedef std::function<ExprAddressRefTypeMap(const core::NodeAddress& toTransform)> CandidateFinder;

ExprAddressRefTypeMap findAllSuited(const core::NodeAddress& toTransform);
ExprAddressRefTypeMap findPragma(const core::NodeAddress& toTransform);

class DatalayoutTransformer {
protected:
	core::NodeManager& mgr;
	core::NodePtr& toTransform;
	CandidateFinder candidateFinder;

	void addToReplacements( std::map<core::NodeAddress, core::NodePtr>& replacements, const core::NodeAddress& toReplace, const core::NodePtr& replacement);

	virtual ExprAddressRefTypeMap findCandidates(const core::NodeAddress& toTransform);
	void collectVariables(const std::pair<core::ExpressionAddress, core::RefTypePtr>& transformRoot,
			ExprAddressSet& toReplaceList, const core::NodeAddress& toTransform, insieme::analysis::VariableScopeMap& scopes);
	std::vector<std::pair<ExprAddressSet, core::RefTypePtr>> createCandidateLists(const core::NodeAddress& toTransform);
	std::vector<std::pair<ExprAddressSet, core::RefTypePtr>> mergeLists(std::vector<std::pair<ExprAddressSet, core::RefTypePtr>>& toReplaceLists);
	virtual core::StructTypePtr createNewType(core::StructTypePtr oldType) =0;

	virtual core::ExpressionPtr updateInit(const ExprAddressMap& varReplacements, core::ExpressionAddress init, core::NodeMap& backupReplacements,
			core::StringValuePtr fieldName = core::StringValuePtr());

	virtual core::StatementList generateNewDecl(const ExprAddressMap& varReplacements, const core::DeclarationStmtAddress& decl,
			const core::VariablePtr& newVar, const core::StructTypePtr& newStructType, const core::StructTypePtr& oldStructType,
			const core::ExpressionPtr& nElems = core::ExpressionPtr()) =0;
	void addNewDecls(const ExprAddressMap& varReplacements, const core::StructTypePtr& newStructType, const core::StructTypePtr& oldStructType,
			const core::NodeAddress& toTransform, const core::pattern::TreePattern& allocPattern, core::ExpressionMap& nElems, std::map<core::NodeAddress,
			core::NodePtr>& replacements);

	void addNewParams(const core::ExpressionMap& varReplacements, const core::NodeAddress& toTransform, std::map<core::NodeAddress,
			core::NodePtr>& replacements);

	virtual core::StatementList generateNewAssigns(const ExprAddressMap& varReplacements, const core::CallExprAddress& call,
			const core::ExpressionPtr& newVar, const core::StructTypePtr& newStructType, const core::StructTypePtr& oldStructType,
			const core::ExpressionPtr& nElems = core::ExpressionPtr()) =0;
	void replaceAssignments(const ExprAddressMap& varReplacements, const core::StructTypePtr& newStructType, const core::StructTypePtr& oldStructType,
			const core::NodeAddress& toTransform, const core::pattern::TreePattern& allocPattern, core::ExpressionMap& nElems,
			std::map<core::NodeAddress, core::NodePtr>& replacements);

	core::ExpressionPtr determineNumberOfElements(const core::ExpressionPtr& newVar,const core::ExpressionMap&  nElems);

	virtual core::StatementPtr generateMarshalling(const core::ExpressionAddress& oldVar, const core::ExpressionPtr& newVar, const core::ExpressionPtr& start,
			const core::ExpressionPtr& end, const core::StructTypePtr& structType) =0;
	std::vector<core::StatementAddress> addMarshalling(const ExprAddressMap& varReplacements,
			const core::StructTypePtr& newStructType, const core::NodeAddress& toTransform, core::ExpressionMap& nElems,
			std::map<core::NodeAddress, core::NodePtr>& replacements);

	virtual core::StatementPtr generateUnmarshalling(const core::ExpressionAddress& oldVar, const core::ExpressionPtr& newVar, const core::ExpressionPtr& start,
			const core::ExpressionPtr& end, const core::StructTypePtr& structType) =0;
	std::vector<core::StatementAddress> addUnmarshalling(const ExprAddressMap& varReplacements,
			const core::StructTypePtr& newStructType, const core::NodeAddress& toTransform, const std::vector<core::StatementAddress>& begin,
			core::ExpressionMap& nElems, std::map<core::NodeAddress, core::NodePtr>& replacements);

	void updateTuples(ExprAddressMap& varReplacements, const core::StructTypePtr& newStructType, const core::TypePtr& oldStructType,
			const core::NodeAddress& toTransform, std::map<core::NodeAddress, core::NodePtr>& replacements, core::ExpressionMap& structures);

	virtual core::ExpressionPtr generateNewAccesses(const core::ExpressionPtr& oldVar, const core::ExpressionPtr& newVar, const core::StringValuePtr& member,
			const core::ExpressionPtr& index, const core::ExpressionPtr& structAccess) =0;
	void replaceAccesses(const ExprAddressMap& varReplacements, const core::StructTypePtr& newStructType,
			const core::NodeAddress& toTransform, const std::vector<core::StatementAddress>& begin, const std::vector<core::StatementAddress>& end,
			std::map<core::NodeAddress,	core::NodePtr>& replacements, core::ExpressionMap& structures);
	virtual core::ExpressionPtr generateByValueAccesses(const core::ExpressionPtr& oldVar, const core::ExpressionPtr& newVar,
			const core::StructTypePtr& newStructType, const core::ExpressionPtr& index, const core::ExpressionPtr& oldStructAccess) =0;
	void updateScalarStructAccesses(core::NodePtr& toTransform);

	virtual core::StatementList generateDel(const core::StatementAddress& stmt, const core::ExpressionAddress& oldVar, const core::ExpressionPtr& newVar,
			const core::StructTypePtr& newStructType) =0;
	void addNewDel(const ExprAddressMap& varReplacements, const core::NodeAddress& toTransform,
			const core::StructTypePtr& newStructType, std::map<core::NodeAddress, core::NodePtr>& replacements);

	void updateCopyDeclarations(ExprAddressMap& varReplacements, const core::StructTypePtr& newStructType, const core::StructTypePtr& oldStructType,
			const core::NodeAddress& toTransform, std::map<core::NodeAddress, core::NodePtr>& replacements, core::ExpressionMap& structures);

	void doReplacements(const std::map<core::NodeAddress, core::NodePtr>& replacements, const core::transform::TypeHandler& typeOfMemAllocHandler);
public:
	DatalayoutTransformer(core::NodePtr& toTransform, CandidateFinder candidateFinder);
	virtual ~DatalayoutTransformer() {}

	virtual void transform() =0;
};

class VariableAdder0: public core::transform::CachedNodeMapping {
	core::NodeManager& mgr;
	core::ExpressionMap& varsToReplace;
	core::pattern::TreePattern typePattern;
	core::pattern::TreePattern variablePattern;
	core::pattern::TreePattern namedVariablePattern;
	core::pattern::TreePattern varWithOptionalDeref;

	std::map<int, core::ExpressionPtr> searchInArgumentList(const std::vector<core::ExpressionPtr>& args);

public:
	VariableAdder0(core::NodeManager& mgr, core::ExpressionMap& varReplacements);

	const core::NodePtr resolveElement(const core::NodePtr& element);

	core::ExpressionMap getVarsToReplace() { return varsToReplace; }
};

class VariableAdder: public core::IRVisitor<void, core::Address> {
	core::NodeManager& mgr;
	ExprAddressMap& varsToReplace;
//	std::map<core::NodeAddress, core::NodePtr>& replacements;
	core::pattern::TreePattern typePattern;
	core::pattern::TreePattern variablePattern;
	core::pattern::TreePattern namedVariablePattern;
	core::pattern::TreePattern varWithOptionalDeref;

	std::map<int, core::ExpressionPtr> searchInArgumentList(const std::vector<core::ExpressionAddress>& args);
	core::ExpressionPtr updateArgument(const core::ExpressionAddress& oldArg);

public:
	VariableAdder(core::NodeManager& mgr, ExprAddressMap& varReplacements);

	core::NodeAddress addVariablesToLambdas(core::NodePtr& src);

	void visitCallExpr(const core::CallExprAddress& call);
};

class RemoveMeAnnotation : public core::NodeAnnotation {
public:
	static const string NAME;
    static const utils::StringKey<RemoveMeAnnotation> KEY;

    const utils::AnnotationKeyPtr getKey() const { return &KEY; }
    const std::string& getAnnotationName() const { return NAME; }

    RemoveMeAnnotation() {}


    virtual bool migrate(const core::NodeAnnotationPtr& ptr, const core::NodePtr& before, const core::NodePtr& after) const {
		// always copy the annotation
		assert(&*ptr == this && "Annotation pointer should reference this annotation!");
		after->addAnnotation(ptr);
		return true;
	}
};


class NewCompoundsRemover: public core::transform::CachedNodeMapping {
	core::NodeManager& mgr;

public:
	NewCompoundsRemover(core::NodeManager& mgr) : mgr(mgr) {}

	const core::NodePtr resolveElement(const core::NodePtr& element);
};

} // datalayout
} // transform
} // insieme

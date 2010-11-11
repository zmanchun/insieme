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

#include "insieme/frontend/omp/omp_sema.h"

#include "insieme/core/transform/node_replacer.h"

namespace insieme {
namespace frontend {
namespace omp {

using namespace core;
namespace cl = lang;

bool SemaVisitor::visitStatement(const StatementAddress& stmt) {
	if(BaseAnnotationPtr anno = stmt.getAddressedNode().getAnnotation(BaseAnnotation::KEY)) {
		LOG(INFO) << "omp annotation(s) on: \n" << *stmt;
		std::for_each(anno->getAnnotationListBegin(), anno->getAnnotationListEnd(), [&](AnnotationPtr subAnn){
			LOG(INFO) << "annotation: " << *subAnn;
			if(auto par = std::dynamic_pointer_cast<Parallel>(subAnn)) {
				handleParallel(stmt, *par);
			}
		});
		return false;
	}
	return true;
}

void SemaVisitor::handleParallel(const core::StatementAddress& stmt, const Parallel& par) {
	auto parLambda = build.lambdaExpr(build.functionType(build.tupleType(), cl::TYPE_UNIT) , LambdaExpr::ParamList(), stmt.getAddressedNode());
	auto jobExp = build.jobExpr(parLambda, JobExpr::GuardedStmts(), core::JobExpr::LocalDecls());
	auto parallelCall = build.callExpr(cl::TYPE_JOB, cl::OP_PARALLEL, jobExp);
	auto mergeCall = build.callExpr(cl::OP_MERGE, parallelCall);
	replacement = dynamic_pointer_cast<const Program>(transform::replaceNode(nodeMan, stmt, mergeCall, true));
}

} // namespace omp
} // namespace frontend
} // namespace insieme
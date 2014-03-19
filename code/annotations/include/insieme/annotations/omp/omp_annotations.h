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

#include "insieme/utils/annotation.h"
#include "insieme/core/ir_expressions.h"

namespace insieme {
namespace annotations {

using namespace insieme::core;

enum Parameter { ENERGY, POWER, TIME };

typedef std::pair<core::ExpressionPtr, core::ExpressionPtr> RangeExpr;

class OmpObjectiveAnnotation : public NodeAnnotation {
    std::map<enum Parameter, core::ExpressionPtr> weights;
    std::map<enum Parameter, RangeExpr> constraints;
    unsigned regionId;

public:
	static const string NAME;
    static const utils::StringKey<OmpObjectiveAnnotation> KEY;

    const utils::AnnotationKeyPtr getKey() const { return &KEY; }
    const std::string& getAnnotationName() const { return NAME; }

    OmpObjectiveAnnotation(std::map<enum Parameter, core::ExpressionPtr>& weights, std::map<enum Parameter, RangeExpr>& constraints): weights(weights), constraints(constraints) {
        static unsigned regionCnt = 0;
        regionId = regionCnt++;
    } 

    ExpressionPtr getWeight(enum Parameter par) const { return weights.at(par); }
    RangeExpr getConstraint(enum Parameter par) const { return constraints.at(par); }
    unsigned getRegionId() const { return regionId; }

    virtual bool migrate(const core::NodeAnnotationPtr& ptr, const core::NodePtr& before, const core::NodePtr& after) const {
		// always copy the annotation
		assert(&*ptr == this && "Annotation pointer should reference this annotation!");
		after->addAnnotation(ptr);
		return true;
	}

    static void attach(const NodePtr& node, std::map<enum Parameter, core::ExpressionPtr>& weights, std::map<enum Parameter, RangeExpr>& constraints);
};

typedef std::shared_ptr<OmpObjectiveAnnotation> OmpObjectiveAnnotationPtr;

} // end namespace insieme
} // end namespace annotations

namespace std {

	std::ostream& operator<<(std::ostream& out, const insieme::annotations::OmpObjectiveAnnotation& lAnnot);

} // end namespace std

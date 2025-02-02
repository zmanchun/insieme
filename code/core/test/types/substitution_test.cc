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

#include <gtest/gtest.h>

#include "insieme/core/types/substitution.h"
#include "insieme/core/ir_builder.h"

namespace insieme {
namespace core {
namespace types {

	TEST(Substitution, Basic) {
		NodeManager manager;
		IRBuilder builder(manager);

		TypeVariablePtr varA = builder.typeVariable("A");
		TypeVariablePtr varB = builder.typeVariable("B");

		TypePtr constType = builder.genericType("constType");

		TypePtr typeA = builder.genericType("type", toVector<TypePtr>(varA));
		TypePtr typeB = builder.genericType("type", toVector<TypePtr>(varA, varB));
		TypePtr typeC = builder.genericType("type", toVector<TypePtr>(typeB, varB));


		EXPECT_EQ("'A", toString(*varA));
		EXPECT_EQ("'B", toString(*varB));

		EXPECT_EQ("constType", toString(*constType));

		EXPECT_EQ("type<'A>", toString(*typeA));
		EXPECT_EQ("type<'A,'B>", toString(*typeB));
		EXPECT_EQ("type<type<'A,'B>,'B>", toString(*typeC));

		// test empty substitution
		auto all = toVector<TypePtr>(varA, varB, typeA, typeB, typeC);
		Substitution identity;
		for_each(all, [&](const TypePtr& cur) { EXPECT_EQ(cur, identity.applyTo(manager, cur)); });

		// test one variable replacement
		Substitution substitution(varA, varB);
		EXPECT_EQ(varB, substitution.applyTo(manager, varA));
		EXPECT_EQ(varB, substitution.applyTo(manager, varB));

		EXPECT_EQ("'B", toString(*substitution.applyTo(manager, varA)));
		EXPECT_EQ("'B", toString(*substitution.applyTo(manager, varB)));
		EXPECT_EQ("constType", toString(*substitution.applyTo(manager, constType)));
		EXPECT_EQ("type<'B>", toString(*substitution.applyTo(manager, typeA)));
		EXPECT_EQ("type<'B,'B>", toString(*substitution.applyTo(manager, typeB)));
		EXPECT_EQ("type<type<'B,'B>,'B>", toString(*substitution.applyTo(manager, typeC)));

		// test one variable replacement
		substitution = Substitution(varA, constType);
		EXPECT_EQ("constType", toString(*substitution.applyTo(manager, varA)));
		EXPECT_EQ("'B", toString(*substitution.applyTo(manager, varB)));
		EXPECT_EQ("constType", toString(*substitution.applyTo(manager, constType)));
		EXPECT_EQ("type<constType>", toString(*substitution.applyTo(manager, typeA)));
		EXPECT_EQ("type<constType,'B>", toString(*substitution.applyTo(manager, typeB)));
		EXPECT_EQ("type<type<constType,'B>,'B>", toString(*substitution.applyTo(manager, typeC)));

		// add replacement for variable B
		substitution = Substitution(varA, constType);
		substitution.addMapping(varB, typeA);
		EXPECT_EQ("constType", toString(*substitution.applyTo(manager, varA)));
		EXPECT_EQ("type<'A>", toString(*substitution.applyTo(manager, varB)));
		EXPECT_EQ("constType", toString(*substitution.applyTo(manager, constType)));
		EXPECT_EQ("type<constType>", toString(*substitution.applyTo(manager, typeA)));
		EXPECT_EQ("type<constType,type<'A>>", toString(*substitution.applyTo(manager, typeB)));
		EXPECT_EQ("type<type<constType,type<'A>>,type<'A>>", toString(*substitution.applyTo(manager, typeC)));

		// override replacement for second variable
		substitution.addMapping(varB, typeB);
		EXPECT_EQ("constType", toString(*substitution.applyTo(manager, varA)));
		EXPECT_EQ("type<'A,'B>", toString(*substitution.applyTo(manager, varB)));
		EXPECT_EQ("constType", toString(*substitution.applyTo(manager, constType)));
		EXPECT_EQ("type<constType>", toString(*substitution.applyTo(manager, typeA)));
		EXPECT_EQ("type<constType,type<'A,'B>>", toString(*substitution.applyTo(manager, typeB)));
		EXPECT_EQ("type<type<constType,type<'A,'B>>,type<'A,'B>>", toString(*substitution.applyTo(manager, typeC)));


		// remove one mapping
		substitution.remMappingOf(varA);
		EXPECT_EQ("'A", toString(*substitution.applyTo(manager, varA)));
		EXPECT_EQ("type<'A,'B>", toString(*substitution.applyTo(manager, varB)));
		EXPECT_EQ("constType", toString(*substitution.applyTo(manager, constType)));
		EXPECT_EQ("type<'A>", toString(*substitution.applyTo(manager, typeA)));
		EXPECT_EQ("type<'A,type<'A,'B>>", toString(*substitution.applyTo(manager, typeB)));
		EXPECT_EQ("type<type<'A,type<'A,'B>>,type<'A,'B>>", toString(*substitution.applyTo(manager, typeC)));


		// test substitution composition
		Substitution subA(varA, typeB);

		Substitution subB(varB, constType);


		EXPECT_EQ("{AP('A)=AP(type<'A,'B>)}", toString(subA.getMapping()));
		EXPECT_EQ("{AP('B)=AP(constType)}", toString(subB.getMapping()));

		Substitution combinedAA = Substitution::compose(manager, subA, subA);
		Substitution combinedAB = Substitution::compose(manager, subA, subB);
		Substitution combinedBA = Substitution::compose(manager, subB, subA);
		Substitution combinedBB = Substitution::compose(manager, subB, subB);

		EXPECT_PRED2(containsSubString, toString(combinedAB.getMapping()), "AP('A)=AP(type<'A,constType>)");
		EXPECT_PRED2(containsSubString, toString(combinedAB.getMapping()), "AP('B)=AP(constType)");
		EXPECT_PRED2(containsSubString, toString(combinedBA.getMapping()), "AP('A)=AP(type<'A,'B>)");
		EXPECT_PRED2(containsSubString, toString(combinedBA.getMapping()), "AP('B)=AP(constType)");
		EXPECT_EQ("{AP('B)=AP(constType)}", toString(combinedBB.getMapping()));
	}

} // end namespace types
} // end namespace core
} // end namespace insieme

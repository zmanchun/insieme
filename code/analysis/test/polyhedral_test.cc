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

#include "insieme/analysis/polyhedral.h"

#include "insieme/core/program.h"
#include "insieme/core/ast_builder.h"
#include "insieme/core/statements.h"

using namespace insieme::core;
using namespace insieme::analysis;

#define CREATE_ITER_VECTOR \
	VariablePtr iter1 = Variable::get(mgr, mgr.basic.getInt4(), 1); \
	VariablePtr iter2 = Variable::get(mgr, mgr.basic.getInt4(), 2); \
	VariablePtr param = Variable::get(mgr, mgr.basic.getInt4(), 3); \
	\
	poly::IterationVector iterVec; \
	\
	iterVec.add( poly::Iterator(iter1) ); \
	EXPECT_EQ(static_cast<size_t>(2), iterVec.size()); \
	iterVec.add( poly::Parameter(param) ); \
	EXPECT_EQ(static_cast<size_t>(3), iterVec.size()); \
	iterVec.add( poly::Iterator(iter2) ); \
	EXPECT_EQ(static_cast<size_t>(4), iterVec.size()); \

TEST(IterationVector, Creation) {
	
	NodeManager mgr;
	CREATE_ITER_VECTOR;
	EXPECT_EQ(static_cast<size_t>(4), iterVec.size());
	{
		std::ostringstream ss;
		iterVec.printTo(ss);
		EXPECT_EQ("v1,v2,v3,1", ss.str());
	}
	EXPECT_TRUE( iterVec[0] == poly::Iterator(iter1) );
	EXPECT_FALSE( iterVec[0] == poly::Parameter(iter1) );

	poly::IterationVector iterVec2;

	iterVec2.add( poly::Parameter(param) );
	iterVec2.add( poly::Iterator(iter1) ); 
	iterVec2.add( poly::Iterator(iter2) );

	for (size_t it = 0; it < iterVec.size(); ++it ) {
		EXPECT_TRUE( iterVec[it] == iterVec2[it]);
	}
	{
		std::ostringstream ss;
		iterVec2.printTo(ss);
		EXPECT_EQ("v1,v2,v3,1", ss.str());
	}
}

TEST(IterationVector, Iterator) {
	
	NodeManager mgr;
	CREATE_ITER_VECTOR;

	poly::IterationVector::iterator it = iterVec.begin();
	EXPECT_EQ(*(it+1), poly::Iterator(iter2));
	EXPECT_EQ(*(it+2), poly::Parameter(param));
	EXPECT_EQ(*(it+3), poly::Constant());

}

TEST(AffineFunction, Creation) {
	NodeManager mgr;
	CREATE_ITER_VECTOR;

	poly::AffineFunction af(iterVec);
	af.setCoefficient(poly::Iterator(iter1), 0);
	af.setCoefficient(poly::Parameter(param),2);
	af.setCoefficient(poly::Iterator(iter2), 1);
	af.setConstantPart(10);

	{
		std::ostringstream ss;
		af.printTo(ss);
		EXPECT_EQ("0*v1 + 1*v2 + 2*v3 + 10*1", ss.str());
	}

	EXPECT_EQ(0, af.getCoeff(iter1));
	EXPECT_EQ(2, af.getCoeff(param));
	EXPECT_EQ(1, af.getCoeff(iter2));
	EXPECT_EQ(10, af.getConstCoeff());

	VariablePtr param2 = Variable::get(mgr, mgr.basic.getInt4(), 4); 	
	iterVec.add(poly::Parameter(param2));

	EXPECT_EQ(0, af.getCoeff(param2));
	EXPECT_EQ(0, af.getCoeff(iter1));
	EXPECT_EQ(2, af.getCoeff(param));
	EXPECT_EQ(1, af.getCoeff(iter2));
	EXPECT_EQ(10, af.getConstCoeff());

	{
		std::ostringstream ss;
		af.printTo(ss);
		EXPECT_EQ("0*v1 + 1*v2 + 2*v3 + 0*v4 + 10*1", ss.str());
	}

}

TEST(AffineFunction, CreationFromExpr) {
	NodeManager mgr;
   
   	VariablePtr iter1 = Variable::get(mgr, mgr.basic.getInt4(), 1);
	VariablePtr iter2 = Variable::get(mgr, mgr.basic.getInt4(), 2);
	VariablePtr param = Variable::get(mgr, mgr.basic.getInt4(), 3);

	CallExprPtr sum = CallExpr::get(mgr, mgr.basic.getInt4(), mgr.basic.getSignedIntAdd(), 
			toVector<ExpressionPtr>(iter1, param) 
		);
			
	poly::IterationVector iterVec; 
	iterVec.add( poly::Iterator(iter1) );

	poly::AffineFunction af(iterVec, sum);

	EXPECT_EQ(1, af.getCoeff(iter1));
	EXPECT_EQ(1, af.getCoeff(param));
	EXPECT_EQ(0, af.getConstCoeff());

	{
		std::ostringstream ss;
		af.printTo(ss);
		EXPECT_EQ("1*v1 + 1*v3 + 0*1", ss.str());
	}

	iterVec.add( poly::Iterator(iter2) );
	VariablePtr param2 = Variable::get(mgr, mgr.basic.getInt4(), 4); 
	iterVec.add(poly::Parameter(param2));

	EXPECT_EQ(1, af.getCoeff(iter1));
	EXPECT_EQ(0, af.getCoeff(iter2));
	EXPECT_EQ(1, af.getCoeff(param));
	EXPECT_EQ(0, af.getCoeff(param2));
	EXPECT_EQ(0, af.getConstCoeff());

	{
		std::ostringstream ss;
		af.printTo(ss);
		EXPECT_EQ("1*v1 + 0*v2 + 1*v3 + 0*v4 + 0*1", ss.str());
	}

}

TEST(Constraint, Creation) {
	NodeManager mgr;
	CREATE_ITER_VECTOR;

	poly::AffineFunction af(iterVec);
	af.setCoefficient(poly::Iterator(iter1), 0);
	af.setCoefficient(poly::Parameter(param),2);
	af.setCoefficient(poly::Iterator(iter2), 1);
	af.setConstantPart(10);

	poly::EqualityConstraint c(af);
	{
		std::ostringstream ss;
		c.printTo(ss);
		EXPECT_EQ("0*v1 + 1*v2 + 2*v3 + 10*1 == 0", ss.str());
	}
}

TEST(IterationDomain, Creation) {
	NodeManager mgr;
	CREATE_ITER_VECTOR;

	poly::AffineFunction af(iterVec);
	af.setCoefficient(poly::Iterator(iter1), 0);
	af.setCoefficient(poly::Parameter(param),2);
	af.setCoefficient(poly::Iterator(iter2), 1);
	af.setConstantPart(10);

	poly::AffineFunction af2(iterVec);
	af2.setCoefficient(poly::Iterator(iter1), 1);
	af2.setCoefficient(poly::Parameter(param),0);
	af2.setCoefficient(poly::Iterator(iter2), 1);
	af2.setConstantPart(7);

	poly::AffineFunction af3(iterVec);
	af3.setCoefficient(poly::Iterator(iter1), 1);
	af3.setCoefficient(poly::Parameter(param),1);
	af3.setCoefficient(poly::Iterator(iter2), 0);
	af3.setConstantPart(0);

	poly::ConstraintList cl = {
		poly::Constraint(af, poly::Constraint::LT),
		poly::Constraint(af2, poly::Constraint::LT), 
		poly::Constraint(af3, poly::Constraint::NE)
	};

	EXPECT_EQ(static_cast<size_t>(3), cl.size());

	{
		std::ostringstream ss;
		iterVec.printTo(ss);
		EXPECT_EQ("v1,v2,v3,1", ss.str());
	}

	poly::IterationDomain it(iterVec, cl);
	VariablePtr param2 = Variable::get(mgr, mgr.basic.getInt4(), 4); 
	iterVec.add(poly::Parameter(param2));
	EXPECT_EQ(static_cast<size_t>(5), iterVec.size());

	{
		std::ostringstream ss;
		iterVec.printTo(ss);
		EXPECT_EQ("v1,v2,v3,v4,1", ss.str());
	}

	{
		std::ostringstream ss;
		it.getIterationVector().printTo(ss);
		EXPECT_EQ("v1,v2,v3,1", ss.str());
	}
	// check weather these 2 affine functions are the same... even thought the
	// underlying iteration vector has been changed
	EXPECT_EQ(af, (*it.begin()).getAffineFunction());
}

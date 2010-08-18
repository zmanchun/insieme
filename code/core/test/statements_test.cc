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

#include <sstream>

#include <gtest/gtest.h>
#include "statements.h"

TEST(StatementsTest, Management) {
	TypeManager typeMan;
	StatementManager stmtMan(typeMan);
	StatementManager stmtMan2(typeMan);
	
	BreakStmtPtr bS = BreakStmt::get(stmtMan);
	NoOpStmtPtr nS = NoOpStmt::get(stmtMan);
	
	CompoundStmtPtr bSC = CompoundStmt::get(stmtMan, bS);
	CompoundStmtPtr nSC = CompoundStmt::get(stmtMan, nS);

	vector<StmtPtr> stmtVec;
	stmtVec.push_back(bS);
	stmtVec.push_back(nSC);
	stmtVec.push_back(nS);
	stmtVec.push_back(bSC);
	CompoundStmtPtr bSCVec = CompoundStmt::get(stmtMan, stmtVec);
	CompoundStmtPtr bSCVec2 = CompoundStmt::get(stmtMan2, stmtVec);

	EXPECT_EQ (5, stmtMan.size());

	DepthFirstVisitor<StmtPtr> stmt2check([&](StmtPtr cur) {
		EXPECT_TRUE(stmtMan2.contains(cur));
	});
	stmt2check.visit(bSCVec2);

	EXPECT_FALSE(stmtMan.contains(bSCVec2));
}

TEST(StatementsTest, CreationAndIdentity) {
	TypeManager typeMan;
	StatementManager stmtMan(typeMan);

	BreakStmtPtr bS = BreakStmt::get(stmtMan);
	EXPECT_EQ(bS, BreakStmt::get(stmtMan));

	NoOpStmtPtr nS = NoOpStmt::get(stmtMan);
	EXPECT_NE(*bS, *nS);
}

TEST(StatementsTest, CompoundStmt) {
	TypeManager typeMan;
	StatementManager stmtMan(typeMan);
	BreakStmtPtr bS = BreakStmt::get(stmtMan);
	ContinueStmtPtr cS = ContinueStmt::get(stmtMan);
	
	CompoundStmtPtr empty = CompoundStmt::get(stmtMan);
	CompoundStmtPtr bSC = CompoundStmt::get(stmtMan, bS);
	vector<StmtPtr> stmtVec;
	stmtVec.push_back(bS);
	CompoundStmtPtr bSCVec = CompoundStmt::get(stmtMan, stmtVec);
	EXPECT_EQ(bSC , bSCVec);
	EXPECT_EQ(*bSC , *bSCVec);
	stmtVec.push_back(cS);
	CompoundStmtPtr bScSCVec = CompoundStmt::get(stmtMan, stmtVec);
	EXPECT_NE(bSC , bScSCVec);
	EXPECT_NE(bSC->hash() , bScSCVec->hash());
	EXPECT_EQ((*bSC)[0], (*bScSCVec)[0]);
	std::stringstream ss;
	ss << bScSCVec;
	EXPECT_EQ("{\nbreak;\ncontinue;\n}\n", ss.str());
}

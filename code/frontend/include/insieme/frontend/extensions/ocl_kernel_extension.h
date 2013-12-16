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

#include "insieme/frontend/extensions/frontend_plugin.h"

namespace insieme {
namespace frontend {
namespace extensions {


// extension for OpenCl kernel files

class OclKernelPlugin : public FrontendPlugin {
public:
	OclKernelPlugin() : FrontendPlugin() {
		injectedHeaders.push_back("./ocl_device.h");
	}

private:
	core::ExpressionPtr Visit(const clang::Expr* expr, conversion::Converter& convFact);

    core::TypePtr Visit(const clang::Type* type, conversion::Converter& convFact);

    virtual insieme::core::ExpressionPtr ValueDeclPostVisit(const clang::ValueDecl* decl, core::ExpressionPtr expr, insieme::frontend::conversion::Converter& convFact);
    virtual insieme::core::TypePtr TypeDeclPostVisit(const clang::TypeDecl* decl, core::TypePtr type, insieme::frontend::conversion::Converter& convFact);
    virtual insieme::core::ExpressionPtr FuncDeclPostVisit(const clang::FunctionDecl* decl, core::ExpressionPtr expr, insieme::frontend::conversion::Converter& convFact);

    virtual core::ProgramPtr IRVisit(core::ProgramPtr& prog);
};

} //namespace plugin
} //namespace frontend
} //namespace extensions

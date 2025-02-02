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

#include "insieme/backend/addons/enum_type.h"
#include "insieme/backend/converter.h"
#include "insieme/backend/type_manager.h"
#include "insieme/backend/operator_converter.h"
#include "insieme/backend/function_manager.h"
#include "insieme/backend/c_ast/c_ast_utils.h"
#include "insieme/backend/statement_converter.h"

#include "insieme/annotations/c/include.h"
#include "insieme/annotations/c/decl_only.h"

#include "insieme/core/lang/enum.h"
#include "insieme/core/annotations/naming.h"

namespace insieme {
namespace backend {
namespace addons {

	namespace {

		const TypeInfo* EnumTypeHandler(const Converter& converter, const core::TypePtr& type) {			
			if(!core::lang::isEnumType(type)) { return NULL; }

			//check if the enum is defined in a system header
			const TypeInfo* t = NULL;
			std::function<void(std::string&, const core::TypePtr&)> ff = [](std::string& s, const core::TypePtr& type){ s = "enum" + s; };
			t = type_info_utils::headerAnnotatedTypeHandler(converter, type, ff);
			if(t) { return t; }

			const core::TagTypePtr tt = type.as<core::TagTypePtr>();
			const core::FieldPtr t1 = tt->getFields()[0];
			const core::FieldPtr t2 = tt->getFields()[1];

			//get the enum definition
			core::lang::EnumDefinition enumTy(t1->getType());
			
			//get the enum class type and convert it
			TypeManager& typeManager = converter.getTypeManager();
			const TypeInfo& enumClassTypeInfo = typeManager.getTypeInfo(t2->getType());
			//get the enum name and convert it
			const c_ast::SharedCNodeManager& cnodemgr = converter.getCNodeManager();
			c_ast::IdentifierPtr enumName = cnodemgr->create<c_ast::Identifier>(enumTy.getEnumName().as<core::GenericTypePtr>()->getName()->getValue());
			//get the enum values and convert them
			std::vector<std::pair<c_ast::IdentifierPtr, c_ast::LiteralPtr>> values;
			for(auto e : enumTy.getElements()) {
				core::lang::EnumEntry ee(e);
				c_ast::IdentifierPtr eeName = cnodemgr->create<c_ast::Identifier>(ee.getEnumEntryName().as<core::GenericTypePtr>()->getName()->getValue());
				c_ast::LiteralPtr eeVal = cnodemgr->create<c_ast::Literal>(std::to_string(ee.getEnumEntryValue()));
				values.push_back({eeName, eeVal});
			}
			//build up the enum code fragments
			auto cEnumType = cnodemgr->create<c_ast::EnumType>(enumName, values, enumClassTypeInfo.lValueType);
			auto cEnumVarTy = cnodemgr->create<c_ast::NamedType>(enumName);

			TypeInfo* retTypeInfo = new TypeInfo();
			// create definition of the enum type
			c_ast::CodeFragmentPtr definition = c_ast::CCodeFragment::createNew(converter.getFragmentManager(), cEnumType);

			retTypeInfo->declaration = definition;
			retTypeInfo->definition = definition;
			retTypeInfo->lValueType = cEnumVarTy;
			retTypeInfo->rValueType = cEnumVarTy;
			retTypeInfo->externalType = cEnumVarTy;

			// build up and return resulting type information
			return retTypeInfo;
		}


		OperatorConverterTable getEnumTypeOperatorTable(core::NodeManager& manager) {
			OperatorConverterTable res;
			const auto& ext = manager.getLangExtension<core::lang::EnumExtension>();

			#include "insieme/backend/operator_converter_begin.inc"

			// ------------------ complex specific operators ---------------
			res[ext.getEnumToInt()] = OP_CONVERTER { return CONVERT_ARG(0); };

			res[ext.getIntToEnum()] = OP_CONVERTER { return CONVERT_ARG(1); };

			#include "insieme/backend/operator_converter_end.inc"

			return res;
		}

	}

	void EnumType::installOn(Converter& converter) const {
		// registers type handler
		converter.getTypeManager().addTypeHandler(EnumTypeHandler);

		// register additional operators
		converter.getFunctionManager().getOperatorConverterTable().insertAll(getEnumTypeOperatorTable(converter.getNodeManager()));
	}

} // end namespace addons
} // end namespace backend
} // end namespace insieme

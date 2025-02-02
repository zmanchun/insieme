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

#include "insieme/frontend/utils/header_tagger.h"

#include <boost/optional.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

#include "insieme/utils/logging.h"
#include "insieme/core/ir_node.h"
#include "insieme/annotations/c/include.h"

namespace insieme {
namespace frontend {
namespace utils {

namespace fs = boost::filesystem;
namespace ba = boost::algorithm;


HeaderTagger::HeaderTagger(const vector<fs::path>& stdLibDirs, const vector<fs::path>& interceptedHeaderDirs, const vector<fs::path>& userIncludeDirs,
                           const clang::SourceManager& srcMgr)
    : stdLibDirs(::transform(stdLibDirs, [](const fs::path& cur) { return fs::canonical(cur); })),
      interceptedHeaderDirs(::transform(interceptedHeaderDirs, [](const fs::path& cur) { return fs::canonical(cur); })),
      userIncludeDirs(::transform(userIncludeDirs, [](const fs::path& cur) { return fs::canonical(cur); })), sm(srcMgr) {
	VLOG(2) << "stdLibDirs: \n\t" << this->stdLibDirs;
	VLOG(2) << "interceptedHedaerDirs: \n\t" << this->interceptedHeaderDirs;
	VLOG(2) << "userIncludeDirs: \n\t" << this->userIncludeDirs;
}

/**
 * A utility function cutting down std-lib header files.
 */
boost::optional<fs::path> HeaderTagger::toStdLibHeader(const fs::path& path) const {
	static const boost::optional<fs::path> fail;

	if(stdLibDirs.empty()) { return fail; }

	if(contains(stdLibDirs, fs::canonical(path))) { // very expensive, keep low use
		return fs::path();
	}

	if(!path.has_parent_path()) { return fail; }

	// if it is within the std-lib directory, build relative path
	auto res = toStdLibHeader(path.parent_path());
	return (res) ? (*res / path.filename()) : fail;
}

bool HeaderTagger::isStdLibHeader(const clang::SourceLocation& loc) const {
	if(!loc.isValid()) { return false; }
	auto fit = isStdCache.find(sm.getFileID(loc));
	if(fit != isStdCache.end()) { return fit->second.second; }

	std::string filename = sm.getPresumedLoc(loc).getFilename();
	bool isSys = isStdLibHeader(filename);
	isStdCache[sm.getFileID(loc)] = {filename, isSys};
	return isSys;
}

bool HeaderTagger::isStdLibHeader(const fs::path& path) const {
	return toStdLibHeader(fs::canonical(path)); // expensive, dont go crazy with this
}

bool HeaderTagger::isInterceptedLibHeader(const clang::SourceLocation& loc) const {
	if(!loc.isValid()) { return false; }
	auto fit = isInterceptedCache.find(sm.getFileID(loc));
	if(fit != isInterceptedCache.end()) { return !fit->second.second; }

	std::string filename = sm.getPresumedLoc(loc).getFilename();
	bool isIntercepted = isInterceptedLibHeader(filename);
	isInterceptedCache[sm.getFileID(loc)] = {filename, !isIntercepted};
	return isIntercepted;
}

bool HeaderTagger::isInterceptedLibHeader(const fs::path& path) const {
	return toInterceptedLibHeader(path);
}

boost::optional<fs::path> HeaderTagger::toInterceptedLibHeader(const fs::path& path) const {
	static const boost::optional<fs::path> fail;

	if(interceptedHeaderDirs.empty()) { return fail; }

	if(contains(interceptedHeaderDirs, fs::canonical(path))) { return fs::path(); }

	if(!path.has_parent_path()) { return fail; }

	// if it is within the user-added-include directory, build relative path
	auto res = toInterceptedLibHeader(path.parent_path());
	return (res) ? (*res / path.filename()) : fail;
}

bool HeaderTagger::isUserLibHeader(const clang::SourceLocation& loc) const {
	if(!loc.isValid()) { return false; }
	auto fit = isUserCache.find(sm.getFileID(loc));
	if(fit != isUserCache.end()) { return !fit->second.second; }

	std::string filename = sm.getPresumedLoc(loc).getFilename();
	bool isUser = isUserLibHeader(filename);
	isUserCache[sm.getFileID(loc)] = {filename, !isUser};
	return isUser;
}

bool HeaderTagger::isUserLibHeader(const fs::path& path) const {
	return toUserLibHeader(path);
}

boost::optional<fs::path> HeaderTagger::toUserLibHeader(const fs::path& path) const {
	static const boost::optional<fs::path> fail;

	if(userIncludeDirs.empty()) { return fail; }

	if(contains(userIncludeDirs, fs::canonical(path))) { return fs::path(); }

	if(!path.has_parent_path()) { return fail; }

	// if it is within the user-added-include directory, build relative path
	auto res = toUserLibHeader(path.parent_path());
	return (res) ? (*res / path.filename()) : fail;
}


bool HeaderTagger::isHeaderFile(const string& name) const {
	// everything ending wiht .h or .hpp or nothing (e.g. vector) => so check for not being c,cpp,...
	VLOG(2) << "isHeaderFile? " << name;
	return !name.empty()
	       && !(ba::ends_with(name, ".c") || ba::ends_with(name, ".cc") || ba::ends_with(name, ".cpp") || ba::ends_with(name, ".cxx")
	            || ba::ends_with(name, ".C"));
}

string HeaderTagger::getTopLevelInclude(const clang::SourceLocation& includeLocation) const {
	// if it is a dead end
	if(!includeLocation.isValid()) { return ""; }

	auto toFilename = [&](const clang::SourceLocation& loc) { return sm.getPresumedLoc(loc).getFilename(); };

	// get the presumed location (whatever this is, ask clang) ...
	// ~ ploc represents the file were includeLocation is located
	clang::PresumedLoc ploc = sm.getPresumedLoc(includeLocation);

	// .. and retrieve the associated include
	// ~ from where the file ploc represents was included
	clang::SourceLocation includingLocation = ploc.getIncludeLoc();

	// check whether the stack can be continued
	if(!includingLocation.isValid()) {
		return ""; // this happens when element is declared in c / cpp file => no header
	}

	// ~ pIncludeLoc represents the file were includLoc is located
	clang::PresumedLoc pIncludeLoc = sm.getPresumedLoc(includingLocation);

	if(isInjectedHeader(pIncludeLoc)) {
		// the header was injected -- has no valid filename ("<command line">)
		return "";
	}

	//*******************
	//
	// travel down until we are at the sourcefile then work up again
	// if userProvided header, we need to go further
	// if userSearchPath/systemSearch path we need to include de header
	//
	//*******************

	// descent further as long as we have a header file as presumed include includeLocation
	if(isHeaderFile(toFilename(includingLocation))) {
		// check if last include was in the search path and next is not,
		// this case is a system header included inside of a programmer include chain
		// BUT if both are still in the search path, continue cleaning the include
		if(isStdLibHeader(includeLocation) && !isStdLibHeader(includingLocation)) {
			if(!isIntrinsicHeader(toFilename(includingLocation))) { return ploc.getFilename(); }
		}

		if(isInterceptedLibHeader(includeLocation) && !isInterceptedLibHeader(includingLocation)) {
			if(!isIntrinsicHeader(toFilename(includingLocation))) { return ploc.getFilename(); }
		}

		if(isUserLibHeader(includeLocation) && !isUserLibHeader(includingLocation)) {
			if(!isIntrinsicHeader(toFilename(includingLocation))) { return ploc.getFilename(); }
		}

		return getTopLevelInclude(includingLocation);
	}

	// we already visited all the headers and we are in the .c/.cpp file
	if(isHeaderFile(ploc.getFilename())) {
		if(isIntrinsicHeader(ploc.getFilename())) { return ploc.getFilename(); }

		if(isStdLibHeader(ploc.getFilename())) {
			return ploc.getFilename(); // this happens when header file is included straight in the code
		}

		if(isInterceptedLibHeader(ploc.getFilename())) {
			return ploc.getFilename(); // this happens when header file is included straight in the code
		}

		if(isUserLibHeader(ploc.getFilename())) { return ploc.getFilename(); }

		return ""; // this happens when is declared in a header which is not system header
	}
	return "";
}

bool HeaderTagger::isIntrinsicHeader(const string& name) const {
	return toIntrinsicHeader(fs::path(name));
}
bool HeaderTagger::isInjectedHeader(const clang::PresumedLoc& ploc) const {
	// NOTE: the "-include" of clang is what we call injectedHeaders
	// injected headers are "included" from a file called "<command line>" (by clang)
	return std::strcmp("<command line>", ploc.getFilename()) == 0;
}

boost::optional<fs::path> HeaderTagger::toIntrinsicHeader(const fs::path& path) const {
	static const boost::optional<fs::path> fail;
	fs::path filename = path.filename();
	return (!filename.empty() && ba::ends_with(filename.string(), "intrin.h")) ? (filename) : fail;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 			header tagging interface

bool HeaderTagger::isDefinedInSystemHeader(const clang::Decl* decl) const {
	return isStdLibHeader(decl->getLocation());
}

void HeaderTagger::addHeaderForDecl(const core::NodePtr& node, const clang::Decl* decl, bool attachUserDefined) const {
	// check whether there is a declaration at all
	if(!decl) { return; }

	// the node was already annotated, what is the point of doint it again?
	if(insieme::annotations::c::hasIncludeAttached(node)) { return; }

	if(VLOG_IS_ON(2)) {
		std::string name("UNNAMED");
		if(const clang::NamedDecl* nmd = llvm::dyn_cast<clang::NamedDecl>(decl)) { name = nmd->getQualifiedNameAsString(); }
		VLOG(2) << "Searching header for: " << node << " of type " << node->getNodeType() << " [clang: " << name << "]";
	}

	string fileName = getTopLevelInclude(decl->getLocation());

	// file must be a header file
	if(!isHeaderFile(fileName)) {
		VLOG(2) << "'" << fileName << "' not a headerfile";
		return; // not to be attached
	}

	// do not add headers for external declarations unless those are within the std-library
	if(const clang::FunctionDecl* funDecl = llvm::dyn_cast<clang::FunctionDecl>(decl)) {
		// TODO: this is just based on integration tests - to make them work, no real foundation :(
		if(funDecl->isExternC() && !(isStdLibHeader(fileName) || isIntrinsicHeader(fileName))) { return; }
	}

	// get absolute path of header file
	fs::path header = fs::canonical(fileName);

	if(auto stdLibHeader = toStdLibHeader(header)) {
		header = *stdLibHeader;
	} else if(auto interceptedLibHeader = toInterceptedLibHeader(header)) {
		header = *interceptedLibHeader;
	} else if(auto intrinsicHeader = toIntrinsicHeader(header)) {
		header = *intrinsicHeader;
	} else if(auto userLibHeader = toUserLibHeader(header)) {
		if(attachUserDefined) {
			header = *userLibHeader;
		} else {
			return;
		}
	}

	VLOG(2) << "		header to be attached: " << header.string();

	// use resulting header
	insieme::annotations::c::attachInclude(node, header.string());
}

} // end namespace utils
} // end namespace frontend
} // end namespace utils

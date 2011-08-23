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

#include <memory>
#include <unordered_map>
#include <boost/functional/hash.hpp>
#include <boost/optional.hpp>
#include <boost/utility/typed_in_place_factory.hpp>

#include <boost/type_traits/is_base_of.hpp>
#include <boost/utility/enable_if.hpp>


#include "insieme/utils/hash_utils.h"
#include "insieme/utils/functional_utils.h"
#include "insieme/utils/string_utils.h"
#include "insieme/utils/map_utils.h"
#include "insieme/utils/printable.h"

namespace insieme {
namespace utils {

/**
 * This class constitutes the base class for all AnnotationKey types to be used to
 * attach and address Annotation s within Annotatable objects. This key type is
 * abstract, hence only sub-classes may actually be used to address Annotations.
 *
 * @see SimpleKey<T>
 * @see StringKey
 */
class AnnotationKey : public HashableImmutableData<AnnotationKey>, public utils::Printable {
protected:

	/**
	 * A simple default constructor which is protected to avoid instances of this class.
	 */
	AnnotationKey(std::size_t hashCode) : HashableImmutableData(hashCode) {};
};

/**
 * An abstract base class for any kind of annotation to be attached to a AST node or pointer.
 */
class Annotation : public utils::Printable {

public:

	/**
	 * Request a reference to the key this annotation should be associated to. The memory of the
	 * obtained key has to be managed by the annotation. As long as the annotation is valid, the
	 * key has to be valid.
	 *
	 * NOTE: best practice would be to use static variables to represent annotation keys
	 */
	virtual const AnnotationKey* getKey() const = 0;
	
	/**
	 * Requests the name of this annotation. The name should be a constant class member.
	 *
	 * @return the name of this annotation type
	 */
	virtual const std::string& getAnnotationName() const = 0;

	virtual std::ostream& printTo(std::ostream& out) const { return out << getAnnotationName(); }

	/**
	 * Checks whether this annotation is equivalent to the given annotation. The default
	 * implementation returns true if and only if the given object is the same instance
	 * (object identity).
	 *
	 * @param other the annotation to be compared with
	 * @return true if equivalent, false otherwise.
	 */
	virtual bool operator==(const Annotation& other) const {
		return this == &other;
	}

	/**
	 * Checks whether this annotation is not equivalent to the given annotation. The default
	 * implementation returns returns true whenever the given annotation is not the same
	 * instance as this annotation (object identity).
	 *
	 * @param other the annotation to be compared to
	 * @return true if not equivalent, false otherwise
	 */
	virtual bool operator!=(const Annotation& other) const {
		return this!=&other;
	}

};

/**
 * An abstract base class for compound annotations to be attached to an annotatable objects.
 * A compound annotation is used for attaching multiple annotations using the same key to
 * the same object. Useful to encode both OpenMP and OpenCL annotations.
 */
template <
	class SubAnnTy,
	class BaseAnnotation = Annotation,
	typename boost::enable_if<boost::is_base_of<Annotation, BaseAnnotation>, int>::type = 0
>
class CompoundAnnotation : public BaseAnnotation {
public:
	typedef std::shared_ptr<SubAnnTy> SubAnnotationPtr;
	typedef std::vector<SubAnnotationPtr> AnnotationList;

	CompoundAnnotation(const AnnotationList& annotationList) : annotationList(annotationList) { }

	typename AnnotationList::const_iterator getAnnotationListBegin() const { return annotationList.cbegin(); }
	typename AnnotationList::const_iterator getAnnotationListEnd() const { return annotationList.cend(); }

	const AnnotationList& getAnnotationList() {
		return annotationList;
	}

private:
	AnnotationList annotationList;
};

// Some type definitions for combined types required for handling annotations
//typedef std::shared_ptr<Annotation> AnnotationPtr;

/**
 * The base class of an annotatable object. This base class is maintaining a map of annotations
 * an offers means to manipulate the set of associated annotations.
 */
template<
	typename AnnotationType = Annotation,
	typename KeyType = AnnotationKey,
	typename boost::enable_if<boost::is_base_of<Annotation, AnnotationType>, int>::type = 0,
	typename boost::enable_if<boost::is_base_of<AnnotationKey, KeyType>, int>::type = 0
	>
class Annotatable {

public:

	/**
	 * A type definition for the pointer type used to internally maintain annotation instances.
	 */
	typedef std::shared_ptr<AnnotationType> annotation_ptr_type;

	/**
	 * A type definition for the internal data structure used for storing the annotations.
	 */
	typedef utils::map::PointerMap<const KeyType*, std::shared_ptr<AnnotationType>> annotation_map_type;

private:

	/**
	 * Defines the type of the internally used annotation map reference. The annotation map is maintained
	 * within a boost optional, which reduces the creation overhead of the annotatable objects. The
	 * internal map will only be created in case an actual annotation should be attached.
	 */
	typedef boost::optional<annotation_map_type> AnnotationMapOpt;

	/**
	 * The internal storage for annotations. It links every key to its corresponding value.
	 */
	mutable AnnotationMapOpt map;

public:

	/**
	 * The constructor of this class is initializing the referenced shared annotation register.
	 */
	Annotatable() {};

	/**
	 * The destructor is marked virtual since most likely derived classes will be used by client code.
	 */
	virtual ~Annotatable() {};

	/**
	 * Adds a new annotation referenced by the given shared pointer. In case another annotation is already
	 * registered based on the same key, the current version will be overridden by the newly provided value.
	 *
	 * @param annotation the annotation to be added. The key will be obtained from the given annotation.
	 */
	void addAnnotation(const annotation_ptr_type& annotation) const {
		// check pre-condition
		assert ( annotation && "Cannot add NULL annotation!" );

		// ensure map to be initialized
		initAnnotationMap();

		// insert new element
		auto key = annotation->getKey();
		auto value = std::make_pair(key, annotation);
		auto res = map->insert(value);

		if (!res.second) {
			// equivalent element already present => remove old and add new element
			map->erase(res.first);
			res = map->insert(value);
		}

		// check post-condition
		assert ( res.second && "Insert not successful!");
		assert ( hasAnnotation(key) && "Insert not successful!");
		assert ( &*((*map->find(key)).second)==&*annotation && "Insert not successful!");
	}

	/**
	 * Adds a new annotation of the given generic type to this class. The annotation
	 * will be constructed internally based on the given parameters.
	 *
	 * @tparam Annotation the kind of annotation to be added
	 * @tparam Params the types of the parameters required for the construction
	 * @param p the parameters to be passed to the constructor
	 */
	template<typename Annotation, typename ... Params>
	void addAnnotation(Params ... p) const {
		addAnnotation(std::make_shared<Annotation>(p...));
	}

	/**
	 * Obtains a pointer to an Annotation associated to this annotatable class.
	 *
	 * @tparam Key the type of key used to look up the requested element.
	 * @param key the key referencing the requested element.
	 * @return a pointer to the maintained element or NULL if there is no such element.
	 */
	template<typename Key>
	typename std::shared_ptr<typename Key::annotation_type> getAnnotation(const Key* key) const {

		// check whether there are annotations
		if (!hasAnnotations()) {
			return std::shared_ptr<typename Key::annotation_type>();
		}

		// search for entry
		auto pos = map->find(key);
		if (pos == map->end() ) {
			return std::shared_ptr<typename Key::annotation_type>();
		}

		// check type
		assert (dynamic_cast<typename Key::annotation_type*>(&*(*pos).second)
				&& "Annotation Type of Key does not match actual annotation!" );

		// return pointer to result
		return std::static_pointer_cast<typename Key::annotation_type>((*pos).second);
	}

	/**
	 * Obtains a pointer to an Annotation associated to this annotatable class.
	 *
	 * @tparam Key the type of key used to look up the requested element.
	 * @param key the key referencing the requested element.
	 * @return a pointer to the maintained element or NULL if there is no such element.
	 */
	template<typename Key>
	typename std::shared_ptr<typename Key::annotation_type> getAnnotation(const Key& key) const {
		return getAnnotation(&key);
	}

	/**
	 * Removes the annotation referenced by the given key from the list of maintained references.
	 *
	 * @param key a pointer to the key addressing the element to be removed.
	 */
	void remAnnotation(const KeyType* key) const {

		// check whether there are annotations at all
		if (!map) {
			return;
		}

		auto pos = map->find(key);
		if (pos != map->end()) {
			map->erase(pos);
		}
	}

	/**
	 * Removes the annotation referenced by the given key from the list of maintained references.
	 *
	 * @param key a refernec to the key addressing the element to be removed.
	 */
	void remAnnotation(const KeyType& key) const {
		remAnnotation(&key);
	}

	/**
	 * Checks whether there is an annotation referenced by the given key associated with this object.
	 *
	 * @param key a pointer to the key to be looking for
	 * @return true if found, false otherwise
	 */
	bool hasAnnotation(const KeyType* key) const {
		return map && map->find(key) != map->end();
	}

	/**
	 * Checks whether there is an annotation referenced by the given key associated with this object.
	 *
	 * @param key a reference to the key to be looking for
	 * @return true if found, false otherwise
	 */
	bool hasAnnotation(const KeyType& key) const {
		return hasAnnotation(&key);
	}

	/**
	 * Obtains an immutable reference to the internally maintained annotations.
	 */
	const annotation_map_type& getAnnotations() const {
		initAnnotationMap();
		return *map;
	}

	/**
	 * Replaces the currently assigned annotation map by the given annotations. This
	 * modifications will not be reflected to other copies of this instance. After the
	 * set, the maintained set of annotations will be disconnected from other copies
	 * of this pointer.
	 *
	 * @param annotations the annotations to be assigned
	 */
	void setAnnotations(const annotation_map_type& annotations) const {
		// replace all currently assigned annotations
		map = annotations;
	}

	/**
	 * Tests whether this annotatable object has annotations or not.
	 *
	 * @return true if it annotations are attached, false otherwise
	 */
	bool hasAnnotations() const {
		// check state of internally maintained map
		return map && !(map->empty());
	}

private:

	void initAnnotationMap() const {
		// test whether it has already been initialized
		if (map) {
			return;
		}
		// the annotation map has to be created
		map = boost::in_place<annotation_map_type>();
	}
};

/**
 * A simple implementation of a AnnotationKey which is solely represented by a single value.
 *
 * @tparam T the type of key-value to be used by this key
 * @tparam Comparator the comparator to be used to compare instances of the key type
 */
template<typename T, typename AnnotationType=Annotation, typename Comparator = std::equal_to<T>>
class SimpleKey : public AnnotationKey {
public:

	/**
	 * The type of annotation referenced by this key. Since it is a simple, general key
	 * the annotation key type is
	 */
	typedef AnnotationType annotation_type;

private:

	/**
	 * The value making this key unique.
	 */
	const T value;

	/**
	 * The comparator used to compare instances of the value represented by this key.
	 */
	Comparator comparator;

public:

	/**
	 * Creates a new instance of this key type based on the given value.
	 *
	 * @param value the value to be represented by this instance
	 */
	SimpleKey(const T& value) : AnnotationKey(boost::hash<T>()(value)), value(value) {};

	/**
	 * Compares this key with another AnnotationKey.
	 *
	 * @param other the key to be compared to
	 */
	bool equals(const AnnotationKey& other) const {
		// quick check regarding memory location
		if (this == &other) {
			return true;
		}

		// check type of other key
		if (typeid(other) != typeid(SimpleKey<T, Comparator>)) {
			return false;
		}

		// check value
		const SimpleKey<T,Comparator>& otherKey = static_cast<const SimpleKey<T, Comparator>&>(other);
		return comparator(value, otherKey.getValue());
	}

	/**
	 * Obtains the value represented by this key.
	 *
	 * @return the value represented by this key.
	 */
	const T& getValue() const {
		return value;
	}

	/**
	 * Allows simple keys to be printed to an output stream.
	 *
	 * @param out the stream to be printed to
	 * @return out a reference to the handed in output stream
	 */
	virtual std::ostream& printTo(std::ostream& out) const {
		return out << "SimpleKey(" << value << ")";
	}
};


/**
 * A simple annotation key definition which is based on a string.
 */
template<typename AnnotationType = Annotation>
class StringKey : public SimpleKey<string, AnnotationType> {
public:

	/**
	 * A default constructor for the String-Key type.
	 *
	 * @param value the value to be represented.
	 */
	StringKey(const string& value) : SimpleKey<string, AnnotationType, std::equal_to<string>>(value) {};

	/**
	 * Allows string keys to be printed to an output stream.
	 *
	 * @param out the stream to be printed to
	 * @return out a reference to the handed in output stream
	 */
	virtual std::ostream& printTo(std::ostream& out) const {
		return out << "StringKey(" << this->getValue() << ")";
	}
};


/**
 * Checks whether the given two annotatable objects are equipped with the same set of annotations.
 *
 * @param annotatableA the first of the annotatable objects to be compared
 * @param annotatableB the second of the annotatable objects to be compared
 * @return true if both have the same set of annotations, false otherwise
 */
template<typename Annotation, typename Key>
bool hasSameAnnotations(const Annotatable<Annotation, Key>& annotatableA, const Annotatable<Annotation, Key>& annotatableB) {
	typedef typename Annotatable<Annotation, Key>::annotation_ptr_type Ptr;
	typedef typename Annotatable<Annotation, Key>::annotation_map_type Map;


	// check whether both have no annotations ...
	if (!annotatableA.hasAnnotations() && !annotatableB.hasAnnotations()) {
		return true;
	}

	// check in case both have annotations ...
	if (!(annotatableA.hasAnnotations() && annotatableB.hasAnnotations())) {
		return false;
	}

	// extract maps
	const Map& mapA = annotatableA.getAnnotations();
	const Map& mapB = annotatableB.getAnnotations();

	// compare maps
	return insieme::utils::map::equal(mapA, mapB, equal_target<Ptr>());
}

} // end namespace utils
} // end namespace insieme


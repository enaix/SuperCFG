//
// Created by Flynn on 06.11.2024.
//

#ifndef SUPERCFG_ELEMENT_H
#define SUPERCFG_ELEMENT_H

#include <vector>

 /**
  * @brief Description of a tag type <tag>
  * @tparam CStr Const string class
  */
template<class CStr>
class TagType
{
public:
    CStr name; ///< Tag name
};


 /**
  * @brief Description of an attribute type
  * @tparam CStr Const string class
  */
template<class CStr>
class AttrType
{
public:
    CStr name; ///< Attribute name
};


 /**
  * @brief Link to another element (by name)
  * @tparam CStr Const string class
  */
template<class CStr>
class Link
{
public:
    CStr id; ///< Link to the element
};


 /**
  * @brief Value of an element or an attribute, either in plaintext or a link
  * @tparam CStr
  * @tparam VStr
  */
template<class CStr, class VStr>
class Value
{
public:
    VStr text; ///< Value in plaintext
    Link<CStr> link; ///< Optional link to another element
};


 /**
  * @brief Description of an attribute attr="value"
  * @tparam CStr Const string class
  * @tparam VStr Variable string class
  */
template<class CStr, class VStr>
class Attr
{
public:
    AttrType<CStr> attr; ///< Attribute name
    Value<CStr, VStr> v; ///< Attribute value
};


 /**
  * @brief Element container class
  * @tparam CStr Const string class
  * @tparam VStr Variable string class
  * @tparam AttrList Attribute container template, might be a degenerate case with 0 attributes
  * @tparam ElemList Elements container template, might be a degenerate case with 0/1 element
  */
template<class CStr, class VStr, template<class> class AttrList, template<class> class ElemList>
class Element
{
public:
    TagType<CStr> tag; ///< Tag type
    AttrList<Attr<CStr, VStr>> attrs; ///< Attributes list
    ElemList<Element<CStr, VStr, AttrList, ElemList>> elems; ///< Elements list, empty if value is present
    Value<CStr, VStr> value; ///< Plaintext link or value, empty if elements are present
};


#endif //SUPERCFG_ELEMENT_H

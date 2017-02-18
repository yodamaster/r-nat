#pragma once

#include <memory>
#include <stdio.h>
#include <iostream>
#include <cstdarg>
#include <string>
#include <sstream>
#include <stdlib.h>
#include <set>
#include <map>

#ifdef HAS_RAPIDXML_HEADERS
#include <rapidxml/rapidxml.hpp>
#include <rapidxml/rapidxml_iterators.hpp>
#include <rapidxml/rapidxml_print.hpp>
#include <rapidxml/rapidxml_utils.hpp>
#endif

#define XML_DOCUMENT_MIN_LENGTH	4

#define XML_CONFIG_BEGIN()	\
	bool FromString(std::string src,std::string node_name) \
	{ \
		if (src.size()<XML_DOCUMENT_MIN_LENGTH) return false; \
		rapidxml::xml_document<char> xml; \
		XMLSerializer ss(&xml,NULL,node_name); \
		bool ret=ss.FromString(src); \
		if (ret) __action(ss); \
		return ret;\
	} \
	bool FromFile(std::string file,std::string node_name) \
	{ \
		rapidxml::xml_document<char> xml; \
		XMLSerializer ss(&xml,NULL,node_name); \
		bool ret=ss.FromFile(file); \
		if (ret) __action(ss); \
		return ret;\
	} \
	template<class T> \
	void __action(T& ss) \
	{

/*
usage:
for plain old data type
please specialize ToString/FromString for user data type
or provide Construction and assignment for user data type
*/
#define XML_CONFIG_ITEM(name,var) ss[name].handle_item(var)

/*
usage:
chained node, for any attributes count more than 1 item should use a chained node

sample:
data:
struct repeated_t 
{
int id_;
std::string name_;
XML_CONFIG_BEGIN();
	XML_CONFIG_ITEM("id",id_);
	XML_CONFIG_ITEM("name",name_);
XML_CONFIG_END();
};
std::vector<repeated_t> id_list_;

definition:
XML_CONFIG_NODE_GROUP("id_list",id_list_);

output:
<id_list id="0" name="vector:XML_CONFIG_NODE_GROUP:0"/>
<id_list id="1" name="vector:XML_CONFIG_NODE_GROUP:1"/>
<id_list id="2" name="vector:XML_CONFIG_NODE_GROUP:2"/>
*/
#define XML_CONFIG_NODE(name,var) ss[name].handle_node(var)

/*
usage:
serialization:
containers support begin/end

deserialization:
containers support push_back
containers support insert,at current it supports std::set/set::map

sample:
data:
std::set<std::string> multi_address_set_;

1. definition:
XML_CONFIG_ITEM_GROUP("multi_address",multi_address_,"address");
output:
<multi_address address="vector:XML_CONFIG_ITEM_GROUPvector:XML_CONFIG_NODE_GROUP:0"/>

2. definition:
XML_CONFIG_ITEM_GROUP("multi_address",multi_address_);
output:
<multi_address multi_address="vector:XML_CONFIG_ITEM_GROUPvector:XML_CONFIG_NODE_GROUP:0"/>

other sample:
std::map<int,std::string> id_map_;
XML_CONFIG_ITEM_GROUP("id_map",id_map_,"id","name");
*/
#define XML_CONFIG_ITEM_GROUP(name,var,...) ss[name].handle_item_group(var,NULL,__VA_ARGS__,NULL)

/*
usage:
chained node group
*/
#define XML_CONFIG_NODE_GROUP(name,var) ss[name].handle_node_group(var)

#define XML_CONFIG_END() \
	}

#define XML_CONFIG_ROOT_BEGIN(node_name) \
	bool FromString(std::string src) \
	{ \
	return FromString(src, node_name); \
	} \
	bool FromFile(std::string file) \
	{ \
	return FromFile(file, node_name); \
	} \
	XML_CONFIG_BEGIN()

#define XML_CONFIG_ROOT_END() \
	}

// -------- implement---------------------
class XMLSerializerItem;

class XMLSerializer
{
public:
	explicit XMLSerializer(rapidxml::xml_document<char>* xml=NULL,rapidxml::xml_node<char>* root=NULL,std::string node_name="");
	~XMLSerializer();

	template <class Name>
	XMLSerializerItem operator[] (Name n);

	std::string ToString();
	bool FromString(std::string str);

	bool ToFile(std::string file);
	bool FromFile(std::string file);

protected:
	friend class XMLSerializerItem;
public:
	rapidxml::xml_document<char>* xml_;
	rapidxml::xml_node<char>* root_;
	char* node_name_;
protected:
	XMLSerializer(XMLSerializer& src);// disallow copy
};

namespace nsXMLHelper
{
	template <class Value>
	std::string ToString(Value v)
	{
		std::ostringstream os;
		os << v;
		return os.str();
	}

	// note: it can not handle string to string that contains space
	template <class Value>
	void FromString(std::string str,Value& v)
	{
		std::istringstream iss(str,std::istringstream::in);
		iss >> v;
	}
	inline void FromString(std::string str,std::string& v)
	{
		v = str;
	}
}

// represent an attribute or a node
class XMLSerializerItem
{
protected:
	XMLSerializer* impl_;
	char* name_;
public:
	explicit XMLSerializerItem(XMLSerializer* impl,std::string name):impl_(const_cast<XMLSerializer*>(impl)),name_(NULL)
	{
		if (name.size()) name_=strdup(name.c_str());
	}
	~XMLSerializerItem()
	{
		if (name_) free(name_);
	}
	XMLSerializerItem(const XMLSerializerItem& src):impl_(const_cast<XMLSerializer*>(src.impl_)),name_(NULL)
	{
		if (src.name_) name_=strdup(src.name_);
	}
	template <class Value>
	void handle_item(Value& v)
	{
		assert(impl_->root_);
		rapidxml::xml_attribute<char>* attrib = impl_->root_->first_attribute(name_);
		if (attrib)
		{
			nsXMLHelper::FromString(attrib->value(),v);
		}
	}
	template <class Value>
	void handle_node(Value& v)
	{
		assert(impl_->root_);
		rapidxml::xml_node<char>* node = impl_->root_->first_node(name_);
		if (node)
		{
			XMLSerializer ss(impl_->xml_,node);
			v.__action(ss);
		}
	}
	template <class Value>
	void handle_item_group(Value& v,void* placeholder,...)
	{
		assert(impl_->root_);

		va_list va;
		va_start(va,placeholder);
		char* a = va_arg(va,char*);
		if (a==NULL) a=name_;

		rapidxml::xml_node<char>* node = impl_->root_->first_node(name_);
		while (node!=NULL)
		{
			XMLSerializer ss(impl_->xml_,node);
			class Value::value_type vv;
			ss[a].handle_item(vv);
			v.push_back(vv); // array,vector,deque,list,forward_list
			node = node->next_sibling(name_);
		}
	}
	// for set
	template <class Value>
	void handle_item_group(std::set<Value>& v,void* placeholder,...)
	{
		assert(impl_->root_);

		va_list va;
		va_start(va,placeholder);
		char* a = va_arg(va,char*);
		if (a==NULL) a=name_;

		rapidxml::xml_node<char> * node = impl_->root_->first_node(name_);
		while (node!=NULL)
		{
			XMLSerializer ss(impl_->xml_,node);
			typename std::set<Value>::value_type vv;
			ss[a].handle_item(vv);
			v.insert(vv);
			node = node->next_sibling(name_);
		}
	}
	// for map
	template <class Name,class Value>
	void handle_item_group(std::map<Name,Value>& v,void* placeholder,...)
	{
		assert(impl_->root_);

		va_list va;
		va_start(va,placeholder);
		char* a = va_arg(va,char*),*b=NULL;
		bool need_free=false;
		if (a==NULL)
			a=name_;
		else
		{
			b = va_arg(va,char*);
		}
		if (b==NULL)
		{
			need_free = true;
			int len = strlen(a);
			b = (char*)malloc(len + 1 + 1);
			b[0]='_';
#pragma warning(push)
#pragma warning(disable:4996)
			strcpy(&b[1],a);
#pragma warning(pop)
		}

		rapidxml::xml_node<char> * node = impl_->root_->first_node(name_);
		while (node!=NULL)
		{
			XMLSerializer ss(impl_->xml_,node);
			Name nn;
			Value vv;
			ss[a].handle_item(nn);
			ss[b].handle_item(vv);
			v[nn]=vv;
			node = node->next_sibling(name_);
		}
		if (need_free)
			free(b);
	}

	template <class Value>
	void handle_node_group(Value& v)
	{
		assert(impl_->root_);
		rapidxml::xml_node<char> * node = impl_->root_->first_node(name_);
		while (node!=NULL)
		{
			XMLSerializer ss(impl_->xml_,node);
			typename Value::value_type vv;
			vv.__action(ss);
			v.push_back(vv); // array,vector,deque,list,forward_list
			node = node->next_sibling(name_);
		}
	}

	template <class Value>
	void handle_node_group(std::set<Value>& v)
	{
		assert(impl_->root_);
		rapidxml::xml_node<char> * node = impl_->root_->first_node(name_);
		while (node!=NULL)
		{
			XMLSerializer ss(impl_->xml_,node);
			typename Value::value_type vv;
			vv.__action(ss);
			v.insert(vv);// for set/hash set 
			node = node->next_sibling(name_);
		}
	}
};

inline
XMLSerializer::XMLSerializer(rapidxml::xml_document<char>* xml,
							 rapidxml::xml_node<char>* root,
							 std::string node_name):xml_(xml),root_(root),node_name_(NULL)
{
	if (node_name.size()) node_name_=strdup(node_name.c_str());
	if (root==NULL)
	{
		root_ = xml_->allocate_node(rapidxml::node_element,node_name_?xml_->allocate_string(node_name_):NULL);
	}
}

inline
XMLSerializer::~XMLSerializer()
{
	if (node_name_) free(node_name_);
}

template <class Name>
XMLSerializerItem XMLSerializer::operator[] (Name n)
{
	return XMLSerializerItem(this,nsXMLHelper::ToString(n));
}

inline
std::string XMLSerializer::ToString()
{
	std::string ret;  
	rapidxml::print(std::back_inserter(ret), *xml_, rapidxml::print_no_indenting);
	return ret;
}

inline
bool XMLSerializer::FromString(std::string str)
{
	if (str.size()<XML_DOCUMENT_MIN_LENGTH) return false;
	char* buf=xml_->allocate_string(0,str.size()+1);
	memcpy(buf,str.c_str(),str.size());
	buf[str.size()]=0;
	try
	{
		xml_->parse<0>(buf);
		root_ = xml_->first_node(node_name_);
	}
	catch (rapidxml::parse_error e)
	{
		// parse failure
		return false;
	}
	return true;
}

inline
bool XMLSerializer::FromFile(std::string file)
{
#pragma warning(push)
#pragma warning(disable:4996)
	FILE* f = fopen(file.c_str(),"rb"); 
#pragma warning(pop)
	if (f==NULL) return false; 
	fseek(f,0,SEEK_END); 
	size_t nLen=ftell(f); 
	if (nLen<XML_DOCUMENT_MIN_LENGTH)
	{
		fclose(f); 
		return false;
	}
	fseek(f,0,SEEK_SET); 
	char* buf=xml_->allocate_string(0,nLen+1);
	int ret = fread(buf,1,nLen,f); 
	ret;
	fclose(f); 
	buf[nLen]=0; 
	try
	{
		xml_->parse<0>(buf);
		root_ = xml_->first_node(node_name_);
	}
	catch (rapidxml::parse_error e)
	{
		// parse failure
		return false;
	}
	return true;
}

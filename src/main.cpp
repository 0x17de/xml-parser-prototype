#include <iostream>
#include <stdexcept>
#include <tuple>
#include <vector>
#include <map>
#include <string>
#include <cstring>
#include <sstream>
#include <pugixml.hpp>

using namespace std::literals::string_literals;


struct NodeData
{
    std::string name;
    std::string text;
    std::map<std::string, std::vector<NodeData>> subnodes;
    std::map<std::string, std::string> attributes;
};


class Required;
class copy_t {};

class NodeBase {};
template<const char* name, class... Args>
class Node;
class AttributeBase {};
template<const char* name, class... Args>
class Attribute;

inline void parse_subnodes(NodeData& data, pugi::xml_node& node);
template<class NodeDescription, class... NodeDescriptions>
inline void parse_subnodes(NodeData& data, pugi::xml_node& node, NodeDescription desc, NodeDescriptions... descs);

template<class... Args>
struct is_required;
template<>
struct is_required<> : std::false_type { };
template<class Arg, class... Args>
struct is_required<Arg, Args...> : std::conditional_t<std::is_same_v<Arg, Required>, std::true_type, is_required<Args...>> { };
template<class... Args>
constexpr bool is_required_v = is_required<Args...>::value;


template<class NodeType>
struct NodeName;
template<const char* name_, class... Args>
struct NodeName<Node<name_, Args...>>
{
    static inline constexpr const char* name = name_;
};
template<const char* name_, class... Args>
struct NodeName<Attribute<name_, Args...>>
{
    static inline constexpr const char* name = name_;
};

class Required
{
public:
    Required() { }

    inline auto subnode(pugi::xml_node node) { return node; }
    inline bool validate(pugi::xml_node node) { return true; }
    inline void parse(NodeData& data, pugi::xml_node node) { }
    template<class ParentNode>
    inline void serialize(ParentNode& parent, const NodeData& data) { }
};

template<const char* name, class... Args>
class Attribute : AttributeBase
{
public:
    inline Attribute(Args&&... args)
    { }

    inline bool validate(pugi::xml_attribute attr)
    {
        if (!attr)
        {
            if (is_required_v<Args...>) throw std::runtime_error("Expected xml attribute "s + name);
            return false;
        }
        return true;
    }
    inline auto subnode(pugi::xml_node node)
    {
        auto attr = node.attribute(name);
        return attr;
    }
    inline void parse(NodeData& data, pugi::xml_attribute attr)
    {
        data.attributes[name] = attr.as_string();
    }
    template<class ParentNode>
    inline void serialize(ParentNode& parent, const NodeData& data)
    {
        auto it = data.attributes.find(name);
        auto end = data.attributes.end();
        if (it == end) return;
        parent.append_attribute(name) = it->second.c_str();
    }
};

template<class... Args>
class Text
{
public:
    inline Text(Args... args)
    { }

    inline auto subnode(pugi::xml_node node)
    {
        std::cout << "Node: " << node.attribute("id").as_string() << std::endl;
        return node.text();
    }
    inline bool validate(pugi::xml_text text)
    {
        if (text.empty())
        {
            if (is_required_v<Args...>) throw std::runtime_error("A text node is required");
            return false;
        }
        return true;
    }
    inline void parse(NodeData& data, pugi::xml_text textNode)
    {
        data.text = textNode.as_string();
    }
    template<class ParentNode>
    inline void serialize(ParentNode& parent, const NodeData& data)
    {
        parent.text().set(data.text.c_str());
    }
};

template<const char* name, class... Args>
class Node : NodeBase
{
public:
    inline Node(copy_t copy, const Node& other)
        : args{other.args}
    { }
    inline Node(Args... args)
        : args{std::forward<Args>(args)...}
    { }

    inline bool validate(pugi::xml_node node)
    {
        if (!node)
        {
            if (is_required_v<Args...>) throw std::runtime_error("Expected an xml node of name "s + name);
            return false;
        }
        if (std::strcmp(name, node.name()))
            throw std::runtime_error("Expected "s + name + " node instead of "s + node.name());
        return true;
    }
    inline auto subnode(pugi::xml_node node)
    {
        auto subnode = node.child(name);
        return subnode;
    }
    inline void parse(NodeData& data, pugi::xml_node node)
    {
        data.name = name;
        std::apply([&](auto&... args) { parse_subnodes(data, node, args...); }, args);
    }
    template<class ParentNode>
    inline void serialize(ParentNode& parent, const NodeData& data)
    {
        pugi::xml_node node = parent.append_child(data.name.c_str());
        std::apply([&](auto&... args) { serialize_subnodes(node, data, args...); }, args);
        validate(node);
    }

    std::tuple<std::decay_t<Args>...> args;
};

template<class SubNodeType, class... Args>
class NodeList
{
public:
    inline NodeList(const SubNodeType& node, Args... args)
        : subNodeType(node)
    { }

    inline auto subnode(pugi::xml_node node)
    {
        auto children = node.children(NodeName<SubNodeType>::name);
        return children;
    }
    inline bool validate(pugi::xml_object_range<pugi::xml_named_node_iterator> children)
    {
        for (auto& child : children) if (!subNodeType.validate(child)) return false;
        return true;
    }
    inline void parse(NodeData& data, pugi::xml_object_range<pugi::xml_named_node_iterator> children)
    {
        auto& subnodes = data.subnodes[NodeName<SubNodeType>::name];
        for (auto& child : children)
        {
            subnodes.emplace_back();
            auto& subnode = subnodes.back();
            subNodeType.parse(subnode, child);
        }
    }
    template<class ParentNode>
    inline void serialize(ParentNode& parent, const NodeData& data)
    {
        auto it = data.subnodes.find(NodeName<SubNodeType>::name);
        auto end = data.subnodes.end();
        if (it == end) return;
        for (auto& child : it->second) subNodeType.serialize(parent, child);
    }

    SubNodeType subNodeType;
};

inline void serialize_subnodes(pugi::xml_node& parent, const NodeData& data)
{ }
template<class NodeDescription, class... NodeDescriptions>
inline void serialize_subnodes(pugi::xml_node& parent, const NodeData& data, NodeDescription desc, NodeDescriptions... descs)
{
    desc.serialize(parent, data);
    serialize_subnodes(parent, data, descs...);
}
inline void parse_subnodes(NodeData& data, pugi::xml_node& node)
{ }
template<class NodeDescription, class... NodeDescriptions>
inline void parse_subnodes(NodeData& data, pugi::xml_node& node, NodeDescription desc, NodeDescriptions... descs)
{
    auto subnode = desc.subnode(node);
    if (desc.validate(subnode)) desc.parse(data, subnode);
    parse_subnodes(data, node, descs...);
}

template<class NodeDescription>
inline auto parse(const std::string& s, NodeDescription desc)
{
    NodeData data;
    pugi::xml_document doc;
    doc.load_buffer(s.data(), s.size());
    desc.validate(doc.document_element());
    desc.parse(data, doc.document_element());
    return data;
}
template<class NodeDescription>
inline auto serialize(const NodeData& data, NodeDescription desc)
{
    pugi::xml_document doc;
    auto root = doc.append_child(data.name.c_str());
    std::apply([&](auto&... args) { serialize_subnodes(root, data, args...); }, desc.args);
    desc.validate(root);
    std::stringstream ss;
    doc.print(ss, "", pugi::format_raw);
    return ss.str();
}

template<const char* name>
class NodeBuilder
{
public:
    template<class... Args>
    auto operator()(Args... args) {
        return Node<name, Args...>(std::forward<Args>(args)...);
    }
};
template<const char* name>
class AttributeBuilder
{
public:
    template<class... Args>
    auto operator()(Args... args) {
        return Attribute<name, Args...>(std::forward<Args>(args)...);
    }
};

template<class CharT, CharT... chars> auto operator""_node()
{
    static const char name[] = {chars..., 0};
    return NodeBuilder<name>();
}
template<class CharT, CharT... chars> auto operator""_attr()
{
    static const char name[] = {chars..., 0};
    return AttributeBuilder<name>();
}

int main()
{
    auto xml =
        "root"_node(
            Required(),
            "key"_attr(Required()),
            "client_id"_attr(),
            NodeList(
                "data"_node(
                    "id"_attr(Required()),
                    Text(Required()))));

    auto examples = {
        "<wrong />"s,
        "<root />"s,
        "<root key=\"mykey\" />"s,
        "<root key=\"mykey\"><data id=\"1\" /></root>"s,
        "<root key=\"mykey\"><data id=\"1\">D1</data><data id=\"2\">D2</data></root>"s
    };
    for (const auto& s : examples)
    {
        try
        {
            std::cout << "== Example: " << s << std::endl;
            auto root = parse(s, xml);
            std::cout << "OK" << std::endl;

            auto dataNodeCount = root.subnodes["data"].size();
            std::cout << "KEY: " << root.attributes["key"] << '\n'
                      << "Data subnode count: " << dataNodeCount << '\n'
                      << "Data1 value: " << (dataNodeCount > 0 ? root.subnodes["data"].at(0).text : "") << '\n'
                      << "Data2 value: " << (dataNodeCount > 1 ? root.subnodes["data"].at(1).text : "") << '\n'
                      << std::flush;

            std::cout << "Serialized: " << serialize(root, xml) << std::endl;
        }
        catch(const std::exception& e)
        {
            std::cout << e.what() << std::endl;
        }
    }

    return 0;
}

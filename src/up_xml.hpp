#pragma once

#include "up_buffer.hpp"
#include "up_istring.hpp"
#include "up_optional.hpp"
#include "up_swap.hpp"
#include "up_utility.hpp"

namespace up_xml
{

    class xml final
    {
    public: // --- scope ---
        class ns;
        class qname;
        class attr;
        class element;
        using uri_loader = std::function<std::unique_ptr<up::buffer>(std::string)>;
        class document;
        class stylesheet;
        static auto null_uri_loader() -> uri_loader;
    };


    class xml::ns final
    {
    public: // --- scope ---
        using self = ns;
        class impl;
    private: // --- state ---
        std::shared_ptr<const impl> _impl;
    public: // --- life ---
        explicit ns();
        explicit ns(up::optional<std::string> uri, up::optional<std::string> prefix);
    public: // --- operations ---
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_impl, rhs._impl);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        auto uri() const -> const up::optional<std::string>&;
        auto prefix() const -> const up::optional<std::string>&;
        auto operator()(up::istring local_name) const -> qname;
    };


    class xml::qname final
    {
    public: // --- scope ---
        using self = qname;
    private: // --- state ---
        up::istring _local_name;
        std::shared_ptr<const ns::impl> _ns; // may be empty
    public: // --- life ---
        explicit qname(up::istring local_name, std::shared_ptr<const ns::impl> ns)
            : _local_name(std::move(local_name)) , _ns(std::move(ns))
        { }
    public: // --- operations ---
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_local_name, rhs._local_name);
            up::swap_noexcept(_ns, rhs._ns);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        auto local_name() const -> auto& { return _local_name; }
        auto ns_uri() const -> const up::optional<std::string>&;
        auto ns_prefix() const -> const up::optional<std::string>&;
    };


    class xml::attr final
    {
    public: // --- scope ---
        using self = attr;
    private: // --- state ---
        qname _name;
        up::istring _value;
    public: // --- life ---
        explicit attr(qname name, up::istring value)
            : _name(std::move(name)), _value(std::move(value))
        { }
    public: // --- operations ---
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_name, rhs._name);
            up::swap_noexcept(_value, rhs._value);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        auto name() const -> auto& { return _name; }
        auto value() const -> auto& { return _value; }
    };


    class xml::element final
    {
    public: // --- scope ---
        using self = element;
    private: // --- state ---
        qname _tag;
        std::vector<attr> _attrs;
        std::string _head;
        std::vector<element> _elements;
        std::string _tail;
    public: // --- life ---
        explicit element(
            qname tag,
            std::vector<attr> attrs,
            std::string head,
            std::vector<element> elements,
            std::string tail)
            : _tag(std::move(tag))
            , _attrs(std::move(attrs))
            , _head(std::move(head))
            , _elements(std::move(elements))
            , _tail(std::move(tail))
        { }
    public: // --- operations ---
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_tag, rhs._tag);
            up::swap_noexcept(_attrs, rhs._attrs);
            up::swap_noexcept(_head, rhs._head);
            up::swap_noexcept(_elements, rhs._elements);
            up::swap_noexcept(_tail, rhs._tail);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        auto tag() const -> auto& { return _tag; }
        auto attrs() const -> auto& { return _attrs; }
        auto head() const -> auto& { return _head; }
        auto elements() const -> auto& { return _elements; }
        auto tail() const -> auto& { return _tail; }
    };


    class xml::document final
    {
    public: // --- scope ---
        using self = document;
        class impl;
        enum class option : uint8_t { dtd_validation, strip_blanks, xinclude, };
        using options = up::enum_set<option>;
        friend stylesheet;
    private: // --- state ---
        std::shared_ptr<const impl> _impl;
    public: // --- life ---
        // parse document
        explicit document(
            up::chunk::from chunk,
            const up::optional<std::string>& uri,
            const up::optional<std::string>& encoding,
            const uri_loader& loader,
            options options);
        // create document from element
        explicit document(const element& root);
        // internal
        explicit document(std::shared_ptr<const impl> impl);
    public: // --- operations ---
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_impl, rhs._impl);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        auto serialize() const -> up::buffer;
        auto to_element() const -> element;
    };


    class xml::stylesheet final
    {
    public: // --- scope ---
        using self = stylesheet;
        class impl;
        friend document;
    private: // --- state ---
        std::shared_ptr<const impl> _impl;
    public: // --- life ---
        // internal
        explicit stylesheet(document document, const uri_loader& loader);
    public: // --- operations ---
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_impl, rhs._impl);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        auto operator()(const document& source, const uri_loader& loader) const -> document;
    };

}

namespace up
{

    using up_xml::xml;

}

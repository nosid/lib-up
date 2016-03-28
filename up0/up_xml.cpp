#include "up_xml.hpp"

#include <cstring>
#include <sstream>
#include <unordered_map>

#include "libxml/parser.h"
#include "libxml/tree.h"
#include "libxml/xinclude.h"
#include "libxml/xmlsave.h"

#include "libxslt/transform.h"
#include "libxslt/xsltutils.h"

#include "up_buffer_adapter.hpp"
#include "up_char_cast.hpp"
#include "up_defer.hpp"
#include "up_exception.hpp"
#include "up_hash.hpp"
#include "up_ints.hpp"
#include "up_nts.hpp"
#include "up_utility.hpp"


// TODO: XPATH support

namespace
{

    class libxml_process final
    {
    public: // --- scope ---
        using self = libxml_process;
        static auto instance() -> auto&
        {
            static self instance;
            return instance;
        }
    private:
        static auto _xml_load_uri(const char* URI, xmlCharEncoding encoding)
            -> xmlParserInputBufferPtr;
        __attribute__((__format__(__printf__, 2, 0)))
        static void _xslt_generic_error(void* cookie, const char* msg, ...);
    public: // --- life ---
        explicit libxml_process()
        {
            // libxml2
            LIBXML_TEST_VERSION;
            ::xmlInitParser();
            ::xmlParserInputBufferCreateFilenameDefault(&_xml_load_uri);
            ::xmlThrDefParserInputBufferCreateFilenameDefault(&_xml_load_uri);
            // libxslt1
            ::xsltInit(); // global initialization function
            // in contrast to libxml2, libxslt uses global functions
            ::xsltSetGenericErrorFunc(nullptr, &_xslt_generic_error);
            ::xsltSetGenericDebugFunc(nullptr, nullptr);
        }
        libxml_process(const self& rhs) = delete;
        libxml_process(self&& rhs) noexcept = delete;
        ~libxml_process() noexcept
        {
            ::xsltCleanupGlobals(); // global cleanup function
            ::xmlCleanupParser();
        }
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self& = delete;
        auto operator=(self&& rhs) & noexcept -> self& = delete;
    };


    class libxml_thread final
    {
    public: // --- scope ---
        using self = libxml_thread;
        static auto instance() -> auto&
        {
            static thread_local self instance;
            return instance;
        }
        class context final
        {
        public: // --- scope ---
            using self = context;
            friend libxml_thread;
        private: // --- state ---
            context*& _root;
            context* _next;
            const up_xml::xml::uri_loader& _loader;
        public: // --- life ---
            explicit context(const up_xml::xml::uri_loader& loader)
                : _root(instance()._context), _next(_root), _loader(loader)
            {
                _root = this;
            }
            context(const self& rhs) = delete;
            context(self&& rhs) noexcept = delete;
            ~context() noexcept
            {
                _root = _next;
            }
        public: // --- operations ---
            auto operator=(const self& rhs) & -> self& = delete;
            auto operator=(self&& rhs) & noexcept -> self& = delete;
        };
    private:
        class errors final
        {
        private: // --- state ---
            std::vector<up::shared_string> _values;
        public: // --- life ---
            explicit errors(std::vector<up::shared_string>&& values)
                : _values()
            {
                // guarantee that values will be empty afterwards
                swap(values, _values);
            }
        public: // --- operations ---
            auto to_insight() const -> up::insight
            {
                up::insights insights;
                for (auto&& value : _values) {
                    insights.emplace_back(typeid(value), value);
                }
                return up::insight(typeid(*this), "insights", std::move(insights));
            }
        };
        __attribute__((__format__(__printf__, 2, 0)))
        static void _xml_generic_error(void* cookie, const char* msg, ...)
        {
            va_list ap;
            va_start(ap, msg);
            try {
                static_cast<libxml_thread*>(cookie)->push_error(up::cformat(msg, ap));
                va_end(ap);
            } catch (...) {
                va_end(ap);
                throw;
            }
        }
        static void _xml_structured_error(void* cookie, xmlErrorPtr error)
        {
            std::ostringstream os;
            os << error->message << '(' << error->domain << '-' << error->code << ')';
            static_cast<libxml_thread*>(cookie)->push_error(up::to_string_view(os.str()));
        }
    private: // --- state ---
        std::vector<up::shared_string> _errors;
        context* _context = nullptr;
    public: // --- life ---
        explicit libxml_thread()
        {
            libxml_process::instance();
            /* The following settings are thread-specific in libxml2, even if
             * they look like global settings. */
            ::xmlSetGenericErrorFunc(this, &_xml_generic_error);
            ::xmlSetStructuredErrorFunc(this, &_xml_structured_error);
        }
    public: // --- operations ---
        auto load_uri(const char* url, xmlCharEncoding encoding) -> xmlParserInputBufferPtr
        {
            if (url == nullptr || _context == nullptr) {
                return nullptr;
            }
            try {
                auto buffer = _context->_loader(up::shared_string(url));
                if (auto result = ::xmlAllocParserInputBuffer(encoding)) {
                    result->context = buffer.get();
                    result->readcallback = [](void* context, char* buf, int len) -> int {
                        if (context && buf && len >= 0) {
                            auto buffer = static_cast<up::buffer*>(context);
                            auto n = std::min(up::ints::cast<std::size_t>(len), buffer->available());
                            std::memcpy(buf, buffer->warm(), n);
                            buffer->consume(n);
                            return up::ints::caster(n);
                        } else {
                            return -1;
                        }
                    };
                    result->closecallback = [](void* context) -> int {
                        if (context) {
                            auto buffer = static_cast<up::buffer*>(context);
                            delete buffer;
                            return 0;
                        } else {
                            return -1;
                        }
                    };
                    buffer.release();
                    return result;
                } else {
                    return nullptr;
                }
            } catch (...) {
                up::suppress_current_exception("xml-external-entity-loader");
                return nullptr;
            }
        }
        void flush()
        {
            _errors.clear();
        }
        void push_error(up::unique_string&& error)
        {
            if (!error.empty() && error.back() == '\n') {
                error.pop_back();
            }
            _errors.push_back(std::move(error));
        }
        template <typename..., typename... Args>
        [[noreturn]]
        void raise(up::source&& source, Args&&... args)
        {
            if (_errors.empty()) {
                throw up::make_exception(std::move(source))
                    .with(std::forward<Args>(args)...);
            } else {
                errors errors(std::move(_errors));
                throw up::make_exception(std::move(source))
                    .with(std::forward<Args>(args)..., std::move(errors));
            }
        }
    };


    void libxml_thread_flush()
    {
        libxml_thread::instance().flush();
    }


    auto libxml_process::_xml_load_uri(const char* URI, xmlCharEncoding encoding)
        -> xmlParserInputBufferPtr
    {
        return libxml_thread::instance().load_uri(URI, encoding);
    }

    void libxml_process::_xslt_generic_error(void* cookie __attribute__((unused)), const char* msg, ...)
    {
        va_list ap;
        va_start(ap, msg);
        try {
            libxml_thread::instance().push_error(up::cformat(msg, ap));
            va_end(ap);
        } catch (...) {
            va_end(ap);
            throw;
        }
    }


    template <typename Type, typename... Args>
    auto flush_make_shared(Args&&... args)
    {
        libxml_thread_flush();
        return std::make_shared<Type>(std::forward<Args>(args)...);
    }


    template <typename..., typename... Args>
    [[noreturn]]
    void raise(up::source&& source, Args&&... args)
    {
        libxml_thread::instance().raise(std::move(source), std::forward<Args>(args)...);
    }


    class internalizer final
    {
    private: // --- scope ---
        using xml = up_xml::xml;
        static auto _chars(const xmlChar* value)
        {
            return up::char_cast<char>(value);
        }
        class ns_hash final
        {
        public: // --- operations ---
            auto operator()(const xmlNs* ns) const -> std::size_t
            {
                return _hash(ns->href) * 37 ^ _hash(ns->prefix);
            }
        private:
            auto _hash(const xmlChar* string) const -> std::size_t
            {
                auto chars = _chars(string);
                return chars ? up::fnv1a(chars, std::strlen(chars)) : 0;
            }
        };
        class ns_equal final
        {
        public: // --- operations ---
            bool operator()(const xmlNs* lhs, const xmlNs* rhs) const
            {
                return _equals(lhs->href, rhs->href)
                    && _equals(lhs->prefix, rhs->prefix);
            }
        private:
            bool _equals(const xmlChar* lhs, const xmlChar* rhs) const
            {
                if (lhs == nullptr) {
                    return rhs == nullptr;
                } else if (rhs == nullptr) {
                    return false;
                } else {
                    return std::strcmp(_chars(lhs), _chars(rhs)) == 0;
                }
            }
        };
    private: // --- state ---
        std::unordered_map<const xmlNs*, xml::ns, ns_hash, ns_equal> _cache;
    public: // --- life ---
        explicit internalizer() = default;
    public: // --- operations ---
        auto operator()(const xmlNode* node) -> xml::element
        {
            return _element(node, {});
        }
    private:
        auto _ns(const xmlNs* ns)
        {
            using ostring = up::optional<up::shared_string>;
            return xml::ns(
                ns->href ? ostring(_chars(ns->href)) : ostring(),
                ns->prefix ? ostring(_chars(ns->prefix)) : ostring());
        }
        auto _qname(const xmlChar* name, const xmlNs* ns)
        {
            if (ns && (ns->href || ns->prefix)) {
                auto p = _cache.find(ns);
                if (p == _cache.end()) {
                    p = _cache.emplace(ns, _ns(ns)).first;
                }
                return p->second(_chars(name));
            } else {
                return xml::ns()(_chars(name));
            }
        }
        auto _attrs(const xmlAttr* attr)
        {
            std::vector<xml::attr> result;
            for (; attr; attr = attr->next) {
                up::unique_string value;
                for (auto node = attr->children; node; node = node->next) {
                    if (node->type == XML_TEXT_NODE || node->type == XML_CDATA_SECTION_NODE) {
                        value.append(_chars(node->content));
                    } else {
                        throw up::make_exception("xml-bad-attr-node-type").with(node->type);
                    }
                }
                result.emplace_back(_qname(attr->name, attr->ns), value);
            }
            return result;
        }
        auto _element(const xmlNode* element, up::shared_string&& tail) -> xml::element
        {
            up::unique_string head;
            std::vector<xml::element> elements;
            up::unique_string text;
            const xmlNode* pending = nullptr;
            for (auto node = element->children; node; node = node->next) {
                switch (node->type) {
                case XML_ELEMENT_NODE:
                    if (pending) {
                        elements.push_back(_element(pending, std::move(text)));
                        text.clear();
                    }
                    pending = node;
                    break;
                case XML_TEXT_NODE:
                case XML_CDATA_SECTION_NODE:
                    if (node->content == nullptr) {
                        // nothing
                    } else if (pending) {
                        text.append(_chars(node->content));
                    } else {
                        head.append(_chars(node->content));
                    }
                    break;
                case XML_COMMENT_NODE:
                    break; // skip
                default:
                    throw up::make_exception("xml-bad-element-node-type").with(node->type);
                }
            }
            if (pending) {
                elements.push_back(_element(pending, std::move(text)));
            }
            return xml::element(
                _qname(element->name, element->ns),
                _attrs(element->properties),
                std::move(head),
                std::move(elements),
                std::move(tail));
        }
    };


    class externalizer final
    {
    private: // --- scope ---
        using xml = up_xml::xml;
        using ostring = up::optional<up::shared_string>;
        using prefixes = std::unordered_map<ostring, std::pair<ostring, xmlNs*>>;
        static auto _chars(const char* value)
        {
            return up::char_cast<xmlChar>(value);
        }
    private: // --- state ---
        up::buffer _buffer;
    public: // --- life ---
        explicit externalizer(xmlNode* parent, const xml::element& element)
        {
            _element(prefixes(), parent, element);
        }
    private: // --- operations ---
        auto _as_c_str(const up::string_view& value) -> const xmlChar*
        {
            _buffer.consume(_buffer.available());
            auto data = value.data();
            auto size = value.size();
            using sizes = up::ints::domain<up::buffer::size_type>::or_range_error;
            _buffer.reserve(sizes::add(size, 1));
            std::memcpy(_buffer.cold(), data, size);
            _buffer.cold()[size] = '\0';
            _buffer.produce(size + 1);
            return _chars(_buffer.warm());
        }
        void _element(const prefixes& defaults, xmlNode* parent, const xml::element& element)
        {
            auto node = ::xmlNewChild(parent, nullptr, _as_c_str(element.tag().local_name()), nullptr);
            if (node == nullptr) {
                raise("xml-new-node-error");
            }
            prefixes overwrite;
            auto put_ns = [&defaults,&overwrite,node](auto&& qname) -> xmlNs* {
                auto&& prefix = qname.ns_prefix();
                auto&& uri = qname.ns_uri();
                if (prefix || uri) {
                    auto p = overwrite.find(prefix);
                    if (p == overwrite.end()) {
                        auto q = defaults.find(prefix);
                        if (q == defaults.end() || !(q->second.first == uri)) {
                            auto ns = ::xmlNewNs(
                                node,
                                uri ? _chars(up::nts(*uri)) : nullptr,
                                prefix ? _chars(up::nts(*prefix)) : nullptr);
                            if (ns == nullptr && prefix && *prefix == "xml") {
                                ns = ::xmlSearchNs(node->doc, node, _chars("xml"));
                            }
                            if (ns == nullptr) {
                                raise("xml-new-ns-error");
                            }
                            q = overwrite.emplace(prefix, std::make_pair(uri, ns)).first;
                        }
                        return q->second.second;
                    } else if (p->second.first == uri) {
                        return p->second.second;
                    } else {
                        throw up::make_exception("xml-inconsistent-namespace-error")
                            .with(prefix, uri, p->second.first);
                    }
                } else {
                    /* Explicitly unset namespace. It is necessary, because in
                     * some cases the the parent namespace will be
                     * automatically inherited. */
                    return nullptr;
                }
            };
            ::xmlSetNs(node, put_ns(element.tag()));
            for (auto&& attr : element.attrs()) {
                auto prop = ::xmlNewNsProp(node, put_ns(attr.name()), _as_c_str(attr.name().local_name()), nullptr);
                if (prop == nullptr) {
                    raise("xml-new-attr-error");
                }
                _add_text(reinterpret_cast<xmlNode*>(prop), attr.value());
            }
            auto prefixes = &defaults;
            if (!overwrite.empty() && !element.elements().empty()) {
                overwrite.insert(defaults.begin(), defaults.end());
                prefixes = &overwrite;
            }
            _add_text(node, element.head());
            for (auto&& child : element.elements()) {
                _element(*prefixes, node, child);
            }
            _add_text(parent, element.tail());
        }
        template <typename Value>
        void _add_text(xmlNode* parent, Value&& value)
        {
            auto text = ::xmlNewDocTextLen(
                parent->doc, _chars(value.data()), up::ints::caster(value.size()));
            if (text == nullptr) {
                raise("xml-new-text-error");
            }
            text->parent = parent;
            if (parent->children) {
                auto prev = parent->last;
                prev->next = text;
                text->prev = prev;
            } else {
                parent->children = text;
            }
            parent->last = text;
        }
    };

}


auto up_xml::xml::null_uri_loader() -> uri_loader
{
    return [](up::shared_string) { return std::unique_ptr<up::buffer>(); };
}


class up_xml::xml::ns::impl final
{
public: // --- scope ---
    using self = impl;
private: // --- state ---
    up::optional<up::shared_string> _uri;
    up::optional<up::shared_string> _prefix;
public: // --- life ---
    explicit impl()
        : _uri(), _prefix()
    { }
    explicit impl(up::optional<up::shared_string> uri, up::optional<up::shared_string> prefix)
        : _uri(std::move(uri)), _prefix(std::move(prefix))
    { }
public: // --- operations ---
    friend auto get_uri(const std::shared_ptr<const impl>& ns)
        -> const up::optional<up::shared_string>&
    {
        static up::optional<up::shared_string> none;
        return ns ? ns->_uri : none;
    }
    friend auto get_prefix(const std::shared_ptr<const impl>& ns)
        -> const up::optional<up::shared_string>&
    {
        static up::optional<up::shared_string> none;
        return ns ? ns->_prefix : none;
    }
};


up_xml::xml::ns::ns()
    : _impl()
{ }

up_xml::xml::ns::ns(up::optional<up::shared_string> uri, up::optional<up::shared_string> prefix)
    : _impl(std::make_shared<const impl>(std::move(uri), std::move(prefix)))
{ }

auto up_xml::xml::ns::uri() const -> const up::optional<up::shared_string>&
{
    return get_uri(_impl);
}

auto up_xml::xml::ns::prefix() const -> const up::optional<up::shared_string>&
{
    return get_prefix(_impl);
}

auto up_xml::xml::ns::operator()(up::shared_string local_name) const -> qname
{
    return qname(std::move(local_name), _impl);
}


auto up_xml::xml::qname::ns_uri() const -> const up::optional<up::shared_string>&
{
    return get_uri(_ns);
}

auto up_xml::xml::qname::ns_prefix() const -> const up::optional<up::shared_string>&
{
    return get_prefix(_ns);
}


class up_xml::xml::document::impl
{
public: // --- scope ---
    using self = impl;
    class simple;
protected: // --- state ---
    xmlDocPtr _ptr = nullptr;
public: // --- life ---
    impl(const self& rhs) = delete;
    impl(self&& rhs) noexcept = delete;
    virtual ~impl() noexcept
    {
        if (_ptr) {
            ::xmlFreeDoc(_ptr);
        }
    }
protected:
    explicit impl() noexcept
        : _ptr(nullptr)
    { }
public: // --- operations ---
    auto operator=(const self& rhs) & -> self& = delete;
    auto operator=(self&& rhs) & noexcept -> self& = delete;
    virtual auto serialize() const -> up::buffer = 0;
    auto native_handle() const { return _ptr; }
    auto to_element() const -> element
    {
        for (const xmlNode* root = _ptr->children; root; root = root->next) {
            if (root->type == XML_ELEMENT_NODE) {
                return internalizer()(root);
            }
        }
        throw up::make_exception("xml-bad-root-error");
    }
};


class up_xml::xml::document::impl::simple final : public up_xml::xml::document::impl
{
private: // --- scope ---
    static auto as_c_str(const up::optional<up::shared_string>& value) -> up::nts
    {
        return value ? up::nts(*value) : up::nts(nullptr);
    }
    static auto as_c_str(const up::optional<up::shared_string>&& value) -> const char* = delete;
public: // --- life ---
    // parsing XML
    explicit simple(
        up::chunk::from chunk,
        const up::optional<up::shared_string>& URL,
        const up::optional<up::shared_string>& encoding,
        const uri_loader& loader,
        options options)
        : impl()
    {
        libxml_thread::context context(loader);
        auto parser = ::xmlNewParserCtxt();
        if (parser == nullptr) {
            raise("xml-new-parser-error");
        }
        UP_DEFER { ::xmlFreeParserCtxt(parser); };
        // parser settings recommended by libxslt1
        parser->loadsubset = XML_DETECT_IDS | XML_COMPLETE_ATTRS;
        parser->replaceEntities = 1;
        int flags = 0
            | XML_PARSE_NOENT // substitute entities
            | XML_PARSE_DTDLOAD // load the external subset
            | XML_PARSE_DTDATTR // default DTD attributes
            | XML_PARSE_PEDANTIC // pedantic error reporting
            | XML_PARSE_NONET // Forbid network access
            | XML_PARSE_NODICT // Do not reuse the context dictionnary
            | XML_PARSE_NSCLEAN // remove redundant namespaces declarations
            | XML_PARSE_NOCDATA // merge CDATA as text nodes
            ;
        if (options.all(option::dtd_validation)) {
            flags |= XML_PARSE_DTDVALID; // validate with the DTD
        }
        if (options.all(option::strip_blanks)) {
            flags |= XML_PARSE_NOBLANKS; // remove blank nodes
        }
        if (options.all(option::xinclude)) {
            flags |= XML_PARSE_XINCLUDE; // Implement XInclude substitition
            flags |= XML_PARSE_NOXINCNODE; // do not generate XINCLUDE START/END nodes
        }
        _ptr = ::xmlCtxtReadMemory(parser, chunk.data(), chunk.size(),
            as_c_str(URL), as_c_str(encoding), flags);
        if (auto error = ::xmlCtxtGetLastError(parser)) {
            /* In some cases, a valid _ptr is returned, even if there were
             * errors. For example if a NS URIs is not absolute. */
            UP_DEFER { ::xmlCtxtResetLastError(parser); };
            raise("xml-parser-error", URL, encoding, options,
                error->domain, error->code, error->level, up::shared_string(error->message));
        } else if (_ptr == nullptr || parser->valid == 0) {
            raise("xml-parser-error", URL, encoding, options);
        }
        if (flags & XML_PARSE_XINCLUDE) {
            /* Even if the option XML_PARSE_XINCLUDE is passed to the
             * function xmlCtxtReadMemory, xinclude directives are not
             * automatically processed. */
            if (::xmlXIncludeProcessFlags(_ptr, flags) >= 0) {
                // OK
            } else if (auto error = ::xmlCtxtGetLastError(parser)) {
                UP_DEFER { ::xmlCtxtResetLastError(parser); };
                raise("xml-parser-xinclude-error", URL, encoding, options,
                    error->domain, error->code, error->level, up::shared_string(error->message));
            } else {
                raise("xml-parser-xinclude-error", URL, encoding, options);
            }
        }
        if (options.all(option::strip_blanks)) {
            _strip_blank_nodes(_ptr->children);
        }
    }
    // conversion from internal to external representation
    explicit simple(const element& root)
        : impl()
    {
        _ptr = ::xmlNewDoc(nullptr);
        if (_ptr == nullptr) {
            raise("xml-new-document-error");
        }
        externalizer(reinterpret_cast<xmlNode*>(_ptr), root);
    }
public: // --- operations ---
    auto serialize() const -> up::buffer override
    {
        const char* encoding = "UTF-8";
        up::buffer buffer;
        auto ctxt = ::xmlSaveToIO(
            [](void* cookie, const char* data, int size) {
                try {
                    up::buffer* b = static_cast<up::buffer*>(cookie);
                    std::size_t n = up::ints::caster(size);
                    b->reserve(n);
                    std::memcpy(b->cold(), data, n);
                    b->produce(n);
                    return size;
                } catch (...) {
                    up::suppress_current_exception("xml-output-write-callback");
                    return -1;
                }
            },
            nullptr, &buffer, encoding, XML_SAVE_FORMAT);
        if (ctxt == nullptr) {
            raise("xml-serialize-error");
        }
        if (::xmlSaveDoc(ctxt, _ptr) < 0) {
            ::xmlSaveFlush(ctxt);
            raise("xml-serialize-error");
        }
        if (::xmlSaveFlush(ctxt) < 0) {
            raise("xml-serialize-error");
        }
        return buffer;
    }
private:
    void _strip_blank_nodes(xmlNode* children)
    {
        /* This implementation is intentionally non-recursive, because the
         * complexity is similar and it avoids uncontrolled deep recursion. */
        std::vector<xmlNode*> pending{children};
        while (!pending.empty()) {
            auto node = pending.back();
            pending.pop_back();
            while (node) {
                auto next = node->next;
                if (node->type == XML_ELEMENT_NODE) {
                    pending.push_back(node->children);
                } else if (node->type == XML_TEXT_NODE && ::xmlIsBlankNode(node)) {
                    /* Note: CDATA nodes are not removed. */
                    ::xmlUnlinkNode(node);
                    ::xmlFreeNode(node);
                } // else: nothing
                node = next;
            }
        }
    }
};


up_xml::xml::document::document(
    up::chunk::from chunk,
    const up::optional<up::shared_string>& uri,
    const up::optional<up::shared_string>& encoding,
    const uri_loader& loader,
    options options)
    : _impl(flush_make_shared<const impl::simple>(chunk, uri, encoding, loader, options))
{ }

up_xml::xml::document::document(const element& root)
    : _impl(flush_make_shared<const impl::simple>(root))
{ }

up_xml::xml::document::document(std::shared_ptr<const impl> impl)
    : _impl(std::move(impl))
{ }

auto up_xml::xml::document::serialize() const -> up::buffer
{
    libxml_thread_flush();
    return _impl->serialize();
}

auto up_xml::xml::document::to_element() const -> element
{
    libxml_thread_flush();
    return _impl->to_element();
}


class up_xml::xml::stylesheet::impl final
{
public: // --- scope ---
    using self = impl;
    class result;
private: // --- state ---
    std::shared_ptr<const document::impl> _document;
    xsltStylesheetPtr _ptr = nullptr;
public: // --- life ---
    explicit impl(std::shared_ptr<const document::impl>&& document, const uri_loader& loader)
        : _document(std::move(document))
    {
        libxml_thread::context context(loader);
        _ptr = ::xsltParseStylesheetDoc(_document->native_handle());
        if (_ptr == nullptr) {
            raise("xslt-parser-error");
        }
    }
    impl(const impl& rhs) = delete;
    impl(impl&& rhs) noexcept = delete;
    ~impl() noexcept
    {
        if (_ptr) {
            _ptr->doc = nullptr; // document is managed by std::shared_ptr
            ::xsltFreeStylesheet(_ptr);
        }
    }
public: // --- operations ---
    auto operator=(const self& rhs) & -> self& = delete;
    auto operator=(self&& rhs) & noexcept -> self& = delete;
};


class up_xml::xml::stylesheet::impl::result final : public document::impl
{
private: // --- scope ---
    class params final
    {
    private: // --- scope ---
        static auto escape(const char* data, std::size_t size)
        {
            /* Passing string parameters to xsltApplyStylesheet is a bit
             * weird. I have found no function to escape the parameters
             * properly. xsltproc does it manually. Apparently there is no way
             * to pass arbitrary strings (e.g. strings containing both "'" and
             * "'"), and the handling of special characters is questionable
             * (e.g. '&' can't be used as an escape character. To be on the
             * safe side, the list of allowed characters is very restrictive,
             * but may be relaxed in the future. */
            up::unique_string result;
            using sizes = up::ints::domain<up::unique_string::size_type>::or_range_error;
            result.reserve(sizes::add(size, 2));
            result.push_back('"');
            for (std::size_t i = 0; i != size; ++i) {
                char c = data[i];
                if (!std::isalnum(c) && c != '-' && c != '.' && c != '/' && c != ':' && c != '_') {
                    throw up::make_exception("xslt-bad-string-parameter-character")
                        .with(c, up::shared_string(data, size));
                }
                result.push_back(c);
            }
            result.push_back('"');
            return result;
        }
    private: // --- fields ---
        std::vector<up::nts> _nts_items;
        std::vector<const char*> _raw_items;
    public: // --- life ---
        explicit params(const parameters& parameters)
        {
            auto size = parameters.size();
            if (size) {
                /* The XSLT library expects the parameters flattened and
                 * terminated with a nullptr. */
                using sizes = up::ints::domain<decltype(size)>::or_range_error;
                _nts_items.reserve(sizes::sum(size, size));
                _raw_items.reserve(sizes::sum(size, size, 1));
                for (auto&& parameter : parameters) {
                    _nts_items.emplace_back(parameter.first);
                    _raw_items.push_back(_nts_items.back());
                    _nts_items.emplace_back(escape(parameter.second.data(), parameter.second.size()));
                    _raw_items.push_back(_nts_items.back());
                }
                _raw_items.push_back(nullptr);
            }
        }
    public: // --- operations ---
        auto operator()() &&
        {
            return _raw_items.empty() ? nullptr : &_raw_items[0];
        }
    };
private: // --- fields ---
    std::shared_ptr<const stylesheet::impl> _stylesheet;
public: // --- life ---
    // XSLT transformation
    explicit result(
        std::shared_ptr<const stylesheet::impl> stylesheet,
        const document::impl& source,
        const uri_loader& loader,
        const parameters& parameters)
        : document::impl(), _stylesheet(std::move(stylesheet))
    {
        libxml_thread::context context(loader);
        _ptr = ::xsltApplyStylesheet(
            _stylesheet->_ptr, source.native_handle(), params(parameters)());
        if (_ptr == nullptr) {
            raise("xslt-transformation-error");
        }
    }
public: // --- operations ---
    auto serialize() const -> up::buffer override
    {
        libxml_thread_flush();
        up::buffer result;
        auto rv = ::xsltSaveResultToFile(up::buffer_adapter::producer(result), _ptr, _stylesheet->_ptr);
        if (rv < 0) {
            raise("xslt-serialization-error");
        }
        return result;
    }
};


up_xml::xml::stylesheet::stylesheet(document document, const uri_loader& loader)
    : _impl(flush_make_shared<const impl>(std::move(document._impl), loader))
{ }

auto up_xml::xml::stylesheet::operator()(
    const document& source,
    const uri_loader& loader,
    const parameters& parameters) const -> document
{
    return document(
        flush_make_shared<const impl::result>(_impl, *source._impl, loader, parameters));
}

#pragma once

#include "up_chunk.hpp"
#include "up_impl_ptr.hpp"
#include "up_utility.hpp"

namespace up_fs
{

    // namespace for file-system library
    class fs final
    {
    public: // --- scope ---
        enum class kind : uint8_t;
        class stats;
        class statfs;
        class directory_entry;
        class context;
        class origin;
        class path;
        class object;
        class locked_file;
        class file;
        class directory;
    };


    /**
     * Different types of unix files. Every type not explicitly listed will
     * be treated as unknown, to make the code future proof.
     */
    enum class fs::kind : uint8_t
    {
        unknown,
        block_device,
        character_device,
        directory,
        named_pipe,
        symbolic_link,
        regular_file,
        socket,
    };

    auto to_string(fs::kind value) -> std::string;


    /**
     * Encapsulates stat data structure.
     */
    class fs::stats final
    {
    public: // --- scope ---
        using self = stats;
        class impl;
    private: // --- state ---
        std::shared_ptr<const impl> _impl;
    public: // --- life ---
        stats(std::nullptr_t) = delete;
        explicit stats(std::shared_ptr<const impl> impl)
            : _impl(std::move(impl))
        { }
    public: // --- operations ---
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_impl, rhs._impl);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        auto size() const -> off_t;
        bool is_kind(kind value) const;
        bool is_block_device() const;
        bool is_character_device() const;
        bool is_directory() const;
        bool is_named_pipe() const;
        bool is_symbolic_link() const;
        bool is_regular_file() const;
        bool is_socket() const;
    };


    class fs::statfs final
    {
    public: // --- scope ---
        using self = statfs;
        class impl;
    private: // --- state ---
        std::shared_ptr<const impl> _impl;
    public: // --- life ---
        statfs(std::nullptr_t) = delete;
        explicit statfs(std::shared_ptr<const impl> impl)
            : _impl(std::move(impl))
        { }
    public: // --- operations ---
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_impl, rhs._impl);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        auto id() -> uint64_t;
        auto bytes_total() -> uint64_t;
        auto bytes_free() -> uint64_t;
        auto bytes_available() -> uint64_t;
        auto files_total() -> uint64_t;
        auto files_free() -> uint64_t;
        auto files_available() -> uint64_t;
    };


    class fs::directory_entry final
    {
    public: // --- scope ---
        using self = directory_entry;
    private: // --- state ---
        ino_t _inode;
        up::istring _name;
        kind _type;
    public: // --- life ---
        explicit directory_entry(ino_t inode, up::istring name, kind type)
            : _inode(std::move(inode))
            , _name(std::move(name))
            , _type(std::move(type))
        { }
    public: // --- operations ---
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_inode, rhs._inode);
            up::swap_noexcept(_name, rhs._name);
            up::swap_noexcept(_type, rhs._type);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        auto inode() const { return _inode; }
        auto name() const -> auto& { return _name; }
        auto type() const { return _type; }
    };


    class fs::context final
    {
    public: // --- scope ---
        using self = context;
        class impl;
    private: // --- state ---
        std::shared_ptr<const impl> _impl;
    public: // --- life ---
        explicit context(std::string name);
    public: // --- operations ---
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_impl, rhs._impl);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        auto to_fabric() const -> up::fabric;
        auto working() const -> origin;
        auto resolved(const std::string& pathname, bool follow = false) const -> origin;
        // operator for convenience - same as working()
        auto operator()() const -> origin;
        // operator for convenience - same as resolved(...)
        auto operator()(const std::string& pathname, bool follow = false) const -> origin;
    };


    class fs::origin final
    {
    public: // --- scope ---
        using self = origin;
        class impl;
    private: // --- state ---
        std::shared_ptr<const impl> _impl;
    public: // --- life ---
        origin(std::nullptr_t) = delete;
        explicit origin(std::shared_ptr<const impl> impl)
            : _impl(std::move(impl))
        { }
    public: // --- operations ---
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_impl, rhs._impl);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        auto to_fabric() const -> up::fabric;
        auto working() const -> origin;
        auto resolved(const std::string& pathname, bool follow = false) const -> origin;
        auto location() const -> std::string;
        auto operator()(std::string pathname, bool follow = false) const -> path;
    };


    class fs::path final
    {
    public: // --- scope ---
        using self = path;
        class impl;
    private: // --- state ---
        std::shared_ptr<const impl> _impl;
    public: // --- life ---
        path(std::nullptr_t) = delete;
        explicit path(std::shared_ptr<const impl> impl)
            : _impl(std::move(impl))
        { }
    public: // --- operations ---
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_impl, rhs._impl);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        auto to_fabric() const -> up::fabric;
        auto get_impl() const -> const std::shared_ptr<const impl>&
        {
            return _impl;
        }
        auto follow(bool value) const -> self;
        auto joined(const std::string& pathname) const -> self;
        auto joined(const up::istring& pathname) const -> self;
        auto resolved() const -> origin;
        auto stat() const -> stats;
        void chmod(mode_t mode) const;
        void chown(uid_t owner, gid_t group) const;
        void mkdir(mode_t mode) const;
        void rmdir() const;
        void link(const self& target) const;
        void unlink() const;
        void rename(const self& target, bool replace) const;
        void exchange(const self& target) const;
        auto readlink() const -> std::string;
        void symlink(const std::string& value) const;
        auto list() const -> std::vector<directory_entry>;
        bool list(std::function<bool(directory_entry)> visitor) const;
        /* The following two operations are a bit different because there
         * are no corresponding system calls with an dir_fd. */
        auto statvfs() const -> statfs;
        void truncate(off_t length) const;
        auto absolute() const -> std::string;
    };


    class fs::object final
    {
    public: // --- scope ---
        using self = object;
        class impl;
    private: // --- state ---
        std::shared_ptr<const impl> _impl;
    public: // --- life ---
        object(std::nullptr_t) = delete;
        explicit object(std::shared_ptr<const impl> impl)
            : _impl(std::move(impl))
        { }
        explicit object(const path& path);
        object(const self& rhs) = delete;
        object(self&& rhs) noexcept = default;
        ~object() noexcept = default;
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self& = delete;
        auto operator=(self&& rhs) & noexcept -> self& = default;
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_impl, rhs._impl);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        void chmod(mode_t mode) const;
        void chown(uid_t owner, gid_t group) const;
        auto stat() const -> stats;
        auto statvfs() const -> statfs;
        void fdatasync() const;
        void fsync() const;
    };


    class fs::file final
    {
    public: // --- scope ---
        using self = file;
        class impl;
        enum class option : uint8_t {
            read, write, append,
            create, exclusive, tmpfile, truncate,
            // permissions
            executable, group, others,
            };
        using options = up::enum_set<option>;
        class lock;
        class channel;
    private: // --- state ---
        std::shared_ptr<const impl> _impl;
    public: // --- life ---
        explicit file(const path& path, options options);
        file(const self& rhs) = delete;
        file(self&& rhs) noexcept = default;
        ~file() noexcept = default;
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self& = delete;
        auto operator=(self&& rhs) & noexcept -> self& = default;
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_impl, rhs._impl);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        explicit operator object() const;
        void chmod(mode_t mode) const;
        void chown(uid_t owner, gid_t group) const;
        auto stat() const -> stats;
        auto statvfs() const -> statfs;
        void fdatasync() const;
        void fsync() const;
        void truncate(off_t length) const;
        auto read_some(up::chunk::into chunk, off_t offset) const -> std::size_t;
        auto write_some(up::chunk::from chunk, off_t offset) const -> std::size_t;
        auto read_some(up::chunk::into_bulk_t&& chunks, off_t offset) const -> std::size_t;
        auto write_some(up::chunk::from_bulk_t&& chunks, off_t offset) const -> std::size_t;
        void write_all(up::chunk::from chunk, off_t offset) const;
        void write_all(up::chunk::from_bulk_t&& chunks, off_t offset) const;
        void posix_fallocate(off_t offset, off_t length) const;
        void posix_fadvise(off_t offset, off_t length, int advice) const;
        void linkto(const path& target) const;
        auto acquire_lock(bool exclusive, bool blocking = true) const -> lock;
        auto make_channel() const -> channel;
    };


    class fs::file::lock final
    {
    public: // --- scope ---
        using self = lock;
        class impl;
    private: // --- state ---
        std::shared_ptr<const impl> _impl;
    public: // --- life ---
        explicit lock(std::shared_ptr<const impl> impl)
            : _impl(std::move(impl))
        { }
        lock(const self& rhs) = delete;
        lock(self&& rhs) noexcept = default;
        ~lock() noexcept = default;
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self& = delete;
        auto operator=(self&& rhs) & noexcept -> self& = default;
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_impl, rhs._impl);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
    };


    class fs::file::channel final
    {
    public: // --- scope ---
        using self = channel;
        class impl;
        static void destroy(impl* ptr);
    private: // --- state ---
        up::impl_ptr<impl, self> _impl;
    public: // --- life ---
        explicit channel(up::impl_ptr<impl, self> impl)
            : _impl(std::move(impl))
        { }
        channel(const self& rhs) = delete;
        channel(self&& rhs) noexcept = default;
        ~channel() noexcept = default;
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self& = delete;
        auto operator=(self&& rhs) & noexcept -> self& = default;
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_impl, rhs._impl);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        auto fill(const file& source, std::size_t size, off_t offset) -> std::size_t;
        auto drain(std::size_t size, off_t offset) -> std::size_t;
    };


    class fs::directory final
    {
    public: // --- scope ---
        using self = directory;
        class impl;
    private: // --- state ---
        std::shared_ptr<impl> _impl;
    public: // --- life ---
        explicit directory(const path& path);
        directory(const self& rhs) = delete;
        directory(self&& rhs) noexcept = default;
        ~directory() noexcept = default;
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self& = delete;
        auto operator=(self&& rhs) & noexcept -> self& = default;
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_impl, rhs._impl);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        explicit operator object() const;
        void chmod(mode_t mode) const;
        void chown(uid_t owner, gid_t group) const;
        auto stat() const -> stats;
        auto statvfs() const -> statfs;
        void fdatasync() const;
        void fsync() const;
        auto list() const & -> std::vector<directory_entry>;
        auto list() && -> std::vector<directory_entry>;
        bool list(std::function<bool(directory_entry)> visitor) const &;
        bool list(std::function<bool(directory_entry)> visitor) &&;
    };

}

namespace up
{

    using up_fs::fs;

}

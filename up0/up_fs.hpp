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

    auto to_string(fs::kind value) -> up::shared_string;


    /**
     * Encapsulates stat data structure.
     */
    class fs::stats final
    {
    public: // --- scope ---
        using self = stats;
        class impl;
        class init;
    private: // --- state ---
        std::shared_ptr<const impl> _impl;
    public: // --- life ---
        explicit stats(init&& arg);
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
        class init;
    private: // --- state ---
        std::shared_ptr<const impl> _impl;
    public: // --- life ---
        explicit statfs(init&& arg);
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
        up::shared_string _name;
        kind _type;
    public: // --- life ---
        explicit directory_entry(ino_t inode, up::shared_string name, kind type)
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
        class accessor;
    private: // --- state ---
        std::shared_ptr<const impl> _impl;
    public: // --- life ---
        explicit context(up::shared_string name);
    public: // --- operations ---
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_impl, rhs._impl);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        auto to_insight() const -> up::insight;
    };


    class fs::origin final
    {
    public: // --- scope ---
        using self = origin;
        class impl;
        class init;
        class accessor;
    private: // --- state ---
        std::shared_ptr<const impl> _impl;
    public: // --- life ---
        explicit origin(init&& arg);
        explicit origin(context context); // working directory
        explicit origin(context context, const up::string_view& pathname, bool follow = false);
    private:
        explicit origin(std::shared_ptr<const impl>&& impl)
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
        auto to_insight() const -> up::insight;
        /* This function returns an absolute path, not a canonical path. There
         * is currently no support for getting a canonical path. */
        auto location() const -> up::unique_string;
        auto resolved(const up::string_view& pathname, bool follow = false) const -> origin;
        // convenience operator
        auto operator()(const up::string_view& pathname, bool follow = false) const -> origin
        {
            return resolved(pathname, follow);
        }
    };


    class fs::path final
    {
    public: // --- scope ---
        using self = path;
        class impl;
        class accessor;
    private: // --- state ---
        std::shared_ptr<const impl> _impl;
    public: // --- life ---
        explicit path(origin origin, up::shared_string pathname, bool follow = false);
    private:
        explicit path(std::shared_ptr<const impl>&& impl)
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
        auto to_insight() const -> up::insight;
        auto follow(bool value) const -> self;
        auto joined(const up::string_view& pathname) const -> self;
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
        auto readlink() const -> up::unique_string;
        void symlink(const up::string_view& value) const;
        auto list() const -> std::vector<directory_entry>;
        bool list(std::function<bool(directory_entry)> visitor) const;
        /* The following two operations are a bit different because there
         * are no corresponding system calls with an dir_fd. */
        auto statvfs() const -> statfs;
        void truncate(off_t length) const;
        /* This function returns an absolute path, not a canonical path. There
         * is currently no support for getting a canonical path. */
        auto absolute() const -> up::unique_string;
    };


    class fs::object final
    {
    public: // --- scope ---
        using self = object;
        class impl;
        class init;
    private: // --- state ---
        std::shared_ptr<const impl> _impl;
    public: // --- life ---
        explicit object(init&& arg);
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


    class fs::locked_file { };


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
        enum class memory_t { };
        static constexpr const memory_t memory = memory_t();
        class lock;
        class channel;
    private: // --- state ---
        std::shared_ptr<const impl> _impl;
    public: // --- life ---
        explicit file(const path& path, options options);
        explicit file(memory_t, context context, const up::string_view& name);
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
        void collapse(off_t length) const;
        void reserve(off_t capacity) const;
        void zero_range(off_t offset, off_t length) const;
        auto read_some(up::chunk::into chunk, off_t offset) const -> std::size_t;
        auto write_some(up::chunk::from chunk, off_t offset) const -> std::size_t;
        auto read_some(up::chunk::into_bulk_t&& chunks, off_t offset) const -> std::size_t;
        auto write_some(up::chunk::from_bulk_t&& chunks, off_t offset) const -> std::size_t;
        void write_all(up::chunk::from chunk, off_t offset) const;
        void write_all(up::chunk::from_bulk_t&& chunks, off_t offset) const;
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
        class init;
    private: // --- state ---
        std::shared_ptr<const impl> _impl;
    public: // --- life ---
        explicit lock(init&& arg);
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
        class init;
        static void destroy(impl* ptr);
    private: // --- state ---
        up::impl_ptr<impl, destroy> _impl;
    public: // --- life ---
        explicit channel(init&& arg);
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

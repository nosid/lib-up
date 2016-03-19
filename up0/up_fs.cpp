#include "up_fs.hpp"

#include <algorithm>
#include <cstring>

#include <dirent.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include <linux/fs.h>

#include "up_buffer.hpp"
#include "up_defer.hpp"
#include "up_exception.hpp"
#include "up_ints.hpp"
#include "up_nts.hpp"
#include "up_terminate.hpp"


namespace
{

    template <typename..., typename... Args>
    [[noreturn]]
    void fail(up::source&& source, Args&&... args)
    {
        throw up::make_exception(std::move(source))
            .with(std::forward<Args>(args)..., up::errno_info(errno));
    }

    template <typename..., typename Type, typename... Args>
    auto check(Type rv, up::source&& source, Args&&... args) -> Type
    {
        if (rv >= 0) {
            return rv;
        } else {
            fail(std::move(source), std::forward<Args>(args)...);
        }
    }


    auto getcwd()
    {
        for (std::size_t i = 8; i <= 16; ++i) {
            std::size_t size = 1 << i;
            auto buffer = std::make_unique<char[]>(size);
            char* cwd = ::getcwd(buffer.get(), size);
            if (cwd) {
                return std::string(cwd);
            } else if (errno != ERANGE) {
                fail("fs-getcwd-error");
            } // else: continue
        }
        throw up::make_exception("fs-getcwd-error").with(up::errno_info(errno));
    }


    auto syscall_renameat2(
        int olddirfd,
        const char* oldpath,
        int newdirfd,
        const char* newpath,
        unsigned int flags)
    {
        return ::syscall(SYS_renameat2, olddirfd, oldpath, newdirfd, newpath, flags);
    }


    class handle final
    {
    public: // --- scope ---
        using self = handle;
    private: // --- state ---
        int _fd;
    public: // --- life ---
        explicit handle() noexcept
            : _fd(-1)
        { }
        explicit handle(int fd) noexcept
            : _fd(fd)
        { }
        handle(const self& rhs) = delete;
        handle(self&& rhs) noexcept
            : _fd(rhs._fd)
        {
            rhs._fd = -1;
        }
        ~handle() noexcept
        {
            close();
        }
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self& = delete;
        auto operator=(self&& rhs) & noexcept -> self&
        {
            swap(rhs);
            /* The following close operation is not strictly necessary.
             * However, it would be surprising, if the old file descriptor
             * of this would still be open after an assignment, regardless
             * whether it is a move or a copy. */
            rhs.close();
            return *this;
        }
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_fd, rhs._fd);
        }
        friend void swap(self& lhs, self& rhs) noexcept __attribute__((unused))
        {
            lhs.swap(rhs);
        }
        auto to_insight() const -> up::insight
        {
            return up::insight(typeid(*this), "fs-handle",
                up::invoke_to_insight_with_fallback(_fd));
        }
        auto get() const { return _fd; }
        auto get_or(int alternate) const
        {
            return _fd == -1 ? alternate : _fd;
        }
        auto release() noexcept
        {
            return std::exchange(_fd, -1);
        }
    private:
        void close() noexcept
        {
            if (_fd != -1) {
                int temp = std::exchange(_fd, -1);
                int rv = ::close(temp);
                if (rv != 0) {
                    up::terminate("bad-close", temp, errno);
                }
            } // else: nothing
        }
    };


    /**
     * Read all data of a file, process it with the given callable, and
     * return its result.
     */
    template <typename Callable>
    auto parse_file_content(const up_fs::fs::file& file, std::size_t initial, Callable&& callable)
    {
        off_t offset = 0;
        up::buffer buffer;
        while (auto count = file.read_some(buffer.reserve(initial), offset)) {
            offset += count;
            buffer.produce(count);
        }
        return std::forward<Callable>(callable)(buffer.warm(), buffer.warm() + buffer.available());
    }


    /**
     * The proc filesystem uses text-based files. That makes it necessary
     * for the kernel, to encode pathnames with special characters. This
     * function can be used to decode such pathname.
     */
    auto unmangle_pathname_from_proc(const char* first, const char* last)
    {
        std::string result;
        result.reserve(last - first);
        for (auto p = first; p != last; ++p) {
            if (*p != '\\') {
                result.push_back(*p);
            } else if (last - p < 4) {
                throw up::make_exception("fs-unmangle-path").with(std::string(first, last));
            } else {
                ++p;
                if (*p < '0' || *p > '3') {
                    throw up::make_exception("fs-unmangle-path").with(std::string(first, last));
                }
                unsigned value = *p - '0';
                ++p;
                if (*p < '0' || *p > '7') {
                    throw up::make_exception("fs-unmangle-path").with(std::string(first, last));
                }
                value = (value << 3) + (*p - '0');
                ++p;
                if (*p < '0' || *p > '7') {
                    throw up::make_exception("fs-unmangle-path").with(std::string(first, last));
                }
                value = (value << 3) + (*p - '0');
                result.push_back(static_cast<unsigned char>(value));
            }
        }
        return result;
    }


    class mount final
    {
    public: // --- scope ---
        using self = mount;
    private: // --- state ---
        dev_t _device;
        std::string _path;
    public: // --- life ---
        explicit mount(dev_t device, std::string path)
            : _device(std::move(device)), _path(std::move(path))
        { }
    public: // --- operations ---
        auto device() const { return _device; }
        auto path() const -> auto& { return _path; }
    };


    auto parse_mountinfo(char* first, char* last)
    {
        std::vector<mount> result;
        char* p;
        while ((p = std::find(first, last, '\n')) != last) {
            *p = '\0';
            unsigned major, minor;
            int pos;
            int rv = sscanf(first, "%*d %*d %u:%u%n", &major, &minor, &pos);
            if (rv != 2 && rv != 3) {
                throw up::make_exception("fs-mountinfo-error").with(std::string(first, p));
            }
            char* r = std::find(first + pos + 1, last, ' ');
            if (r == last) {
                throw up::make_exception("fs-mountinfo-error").with(std::string(first, p));
            }
            ++r;
            char* s = std::find(r, last, ' ');
            if (s == last) {
                throw up::make_exception("fs-mountinfo-error").with(std::string(first, p));
            }
            result.emplace_back(makedev(major, minor), unmangle_pathname_from_proc(r, s));
            first = p + 1;
        }
        return result;
    }


    auto map_dirent_type_to_kind(unsigned char type)
    {
        using kind = up_fs::fs::kind;
        switch (type) {
        case DT_BLK: return kind::block_device;
        case DT_CHR: return kind::character_device;
        case DT_DIR: return kind::directory;
        case DT_FIFO: return kind::named_pipe;
        case DT_LNK: return kind::symbolic_link;
        case DT_REG: return kind::regular_file;
        case DT_SOCK: return kind::socket;
        case DT_UNKNOWN: return kind::unknown;
        default:
            throw up::make_exception("fs-bad-type-error").with(type);
        }
    }

    template <typename Visitor>
    bool scan_directory(handle&& handle, Visitor&& visitor)
    {
        /* The syscalls for reading directories (i.e. getdents) are not
         * exposed in the linux headers. That means we have to use the libc
         * functions for reading directories. It is not possible to use the
         * already existing file descriptor, because the function closedir
         * will close it. */

        int fd = handle.get();
        DIR* dirp = ::fdopendir(fd);
        if (dirp == nullptr) {
            throw up::make_exception("fs-opendir-error").with(fd, up::errno_info(errno));
        }

        UP_DEFER {
            int rv = ::closedir(dirp);
            if (rv != 0) {
                up::terminate("bad-closedir", rv, errno);
            }
        };
        handle.release();

        long name_max = ::fpathconf(fd, _PC_NAME_MAX);
        if (name_max == -1) {
            name_max = PATH_MAX;
        }
        std::size_t size = offsetof(dirent, d_name) + name_max + 1;
        auto buffer = std::make_unique<char[]>(size);

        for (;;) {
            dirent* de = nullptr;
            int rv = ::readdir_r(dirp, reinterpret_cast<dirent*>(buffer.get()), &de);
            if (rv != 0) {
                throw up::make_exception("fs-readdir-error").with(fd, up::errno_info(rv));
            } else if (de == nullptr) {
                return false;
            } else if (std::strcmp(de->d_name, ".") == 0
                || std::strcmp(de->d_name, "..") == 0) {
                // nothing
            } else {
                auto kind = map_dirent_type_to_kind(de->d_type);
                if (visitor(up_fs::fs::directory_entry(de->d_ino, de->d_name, kind))) {
                    return true;
                }
            }
        }
    }

    auto scan_directory(handle&& handle)
    {
        std::vector<up_fs::fs::directory_entry> result;
        scan_directory(std::move(handle), [&result](auto entry) {
            result.push_back(std::move(entry));
            return false;
        });
        return result;
    }


    template <typename Callable>
    void pathname_split(const char* data, std::size_t size, Callable&& callable)
    {
        auto&& invoke = [&callable](const char* first, const char* last) {
            if (first != last && (first + 1 != last || *first != '.')) {
                callable(std::string(first, last));
            }
        };
        const char* p = data;
        const char* q = data + size;
        const char* r;
        while ((r = std::find(p, q, '/')) != q) {
            invoke(p, r);
            p = r + 1;
        }
        invoke(p, q);
    }

    auto pathname_normalize(const up::string_view& pathname)
    {
        bool absolute = pathname.compare(0, 1, "/") == 0;
        bool separator = absolute;
        std::string result;
        pathname_split(pathname.data(), pathname.size(), [&](auto&& name) {
                result.append(separator ? 1 : 0, '/');
                separator = true;
                result.append(name);
            });
        result.append(result.empty() ? 1 : 0, absolute ? '/' : '.');
        return result;
    }


    constexpr mode_t ignored_mode = 0;


    template <typename Function, typename Chunk>
    auto do_io(Function&& function, int fd, Chunk&& chunk, off_t offset, up::source&& source)
        -> std::size_t
    {
        for (;;) {
            ssize_t rv = function(fd, chunk.data(), chunk.size(), offset);
            if (rv != -1) {
                return up::ints::caster(rv);
            } else if (errno == EINTR) {
                // continue
            } else {
                fail(std::move(source), fd, chunk.size(), offset);
            }
        }
    }

    template <typename Function, typename Chunks>
    auto do_iov(Function&& function, int fd, Chunks&& chunks, off_t offset, up::source&& source)
        -> std::size_t
    {
        for (;;) {
            ssize_t rv = function(fd, chunks.template as<iovec>(),
                up::ints::caster(chunks.count()), offset);
            if (rv != -1) {
                return up::ints::caster(rv);
            } else if (errno == EINTR) {
                // continue
            } else {
                fail(std::move(source), fd, chunks.count(), chunks.total(), offset);
            }
        }
    }

}


class up_fs::fs::stats::init final
{
public: // --- state ---
    std::shared_ptr<const impl> _impl;
};


class up_fs::fs::statfs::init final
{
public: // --- state ---
    std::shared_ptr<const impl> _impl;
};


class up_fs::fs::origin::init final
{
public: // --- state ---
    std::shared_ptr<const impl> _impl;
};


class up_fs::fs::path::init final
{
public: // --- state ---
    std::shared_ptr<const impl> _impl;
};


class up_fs::fs::object::init final
{
public: // --- state ---
    std::shared_ptr<const impl> _impl;
};


class up_fs::fs::file::lock::init final
{
public: // --- state ---
    std::shared_ptr<const impl> _impl;
};


class up_fs::fs::file::channel::init final
{
public: // --- state ---
    up::impl_ptr<impl, destroy> _impl;
};


auto up_fs::to_string(fs::kind value) -> std::string
{
    switch (value) {
    case fs::kind::block_device: return "block-device";
    case fs::kind::character_device: return "character-device";
    case fs::kind::directory: return "directory";
    case fs::kind::named_pipe: return "named-pipe";
    case fs::kind::symbolic_link: return "symbolic-link";
    case fs::kind::regular_file: return "regular-file";
    case fs::kind::socket: return "socket";
    case fs::kind::unknown: return "unknown";
    default:
        throw up::make_exception("fs-bad-kind-error").with(value);
    }
}


// dev_t _dev; // ID of device containing file
// ino_t _ino; // inode number
// mode_t _mode; // protection
// nlink_t _nlink; // number of hard links
// uid_t _uid; // user ID of owner
// gid_t _gid; // group ID of owner
// dev_t _rdev; // device ID (if special file)
// off_t _size; // total size, in bytes
// blksize_t _blksize; // blocksize for file system I/O
// blkcnt_t _blocks; // number of 512B blocks allocated
// time_t _atime; // time of last access
// time_t _mtime; // time of last modification
// time_t _ctime; // time of last status chang
class up_fs::fs::stats::impl final
{
public: // --- scope ---
    using self = impl;
public: // --- state ---
    struct stat _stat;
};


up_fs::fs::stats::stats(init&& arg)
    : _impl(std::move(arg._impl))
{ }

auto up_fs::fs::stats::size() const -> off_t
{
    return _impl->_stat.st_size;
}

bool up_fs::fs::stats::is_kind(kind value) const
{
    switch (value) {
    case kind::block_device: return S_ISBLK(_impl->_stat.st_mode);
    case kind::character_device: return S_ISCHR(_impl->_stat.st_mode);
    case kind::directory: return S_ISDIR(_impl->_stat.st_mode);
    case kind::named_pipe: return S_ISFIFO(_impl->_stat.st_mode);
    case kind::symbolic_link: return S_ISLNK(_impl->_stat.st_mode);
    case kind::regular_file: return S_ISREG(_impl->_stat.st_mode);
    case kind::socket: return S_ISSOCK(_impl->_stat.st_mode);
    case kind::unknown:
        break; // ... and continue
    default:
        throw up::make_exception("fs-bad-kind-error").with(value);
    }
    auto&& known = {
        kind::block_device,
        kind::character_device,
        kind::directory,
        kind::named_pipe,
        kind::symbolic_link,
        kind::regular_file,
        kind::socket,
    };
    for (auto&& k : known) {
        if (is_kind(k)) {
            return false;
        }
    }
    return true;
}

bool up_fs::fs::stats::is_block_device() const
{
    return S_ISBLK(_impl->_stat.st_mode);
}

bool up_fs::fs::stats::is_character_device() const
{
    return S_ISCHR(_impl->_stat.st_mode);
}

bool up_fs::fs::stats::is_directory() const
{
    return S_ISDIR(_impl->_stat.st_mode);
}

bool up_fs::fs::stats::is_named_pipe() const
{
    return S_ISFIFO(_impl->_stat.st_mode);
}

bool up_fs::fs::stats::is_symbolic_link() const
{
    return S_ISLNK(_impl->_stat.st_mode);
}

bool up_fs::fs::stats::is_regular_file() const
{
    return S_ISREG(_impl->_stat.st_mode);
}

bool up_fs::fs::stats::is_socket() const
{
    return S_ISSOCK(_impl->_stat.st_mode);
}


class up_fs::fs::statfs::impl final
{
public: // --- scope ---
    using self = impl;
public: // --- state ---
    struct statvfs _statvfs;
public: // --- operations ---
    template <typename Count>
    auto bytes(Count count) const -> uint64_t
    {
        return up::ints::caster(uint64_t(_statvfs.f_frsize) * count);
    }
};


up_fs::fs::statfs::statfs(init&& arg)
    : _impl(std::move(arg._impl))
{ }

auto up_fs::fs::statfs::id() -> uint64_t
{
    return up::ints::caster(_impl->_statvfs.f_fsid);
}

auto up_fs::fs::statfs::bytes_total() -> uint64_t
{
    return _impl->bytes(_impl->_statvfs.f_blocks);
}

auto up_fs::fs::statfs::bytes_free() -> uint64_t
{
    return _impl->bytes(_impl->_statvfs.f_bfree);
}

auto up_fs::fs::statfs::bytes_available() -> uint64_t
{
    return _impl->bytes(_impl->_statvfs.f_bavail);
}

auto up_fs::fs::statfs::files_total() -> uint64_t
{
    return up::ints::caster(_impl->_statvfs.f_files);
}

auto up_fs::fs::statfs::files_free() -> uint64_t
{
    return up::ints::caster(_impl->_statvfs.f_ffree);
}

auto up_fs::fs::statfs::files_available() -> uint64_t
{
    return up::ints::caster(_impl->_statvfs.f_favail);
}


class up_fs::fs::context::impl final
{
public: // --- scope ---
    using self = impl;
private: // --- state ---
    std::string _name;
    int _additional_open_flags;
    bool _avoid_access_time;
public: // --- life ---
    explicit impl(std::string name, int additional_open_flags, bool avoid_access_time)
        : _name(std::move(name))
        , _additional_open_flags(std::move(additional_open_flags))
        , _avoid_access_time(std::move(avoid_access_time))
    { }
public: // --- operations ---
    auto to_insight() const -> up::insight
    {
        return up::insight(typeid(*this), "fs-context-impl",
            up::invoke_to_insight_with_fallback(_name),
            up::invoke_to_insight_with_fallback(_additional_open_flags),
            up::invoke_to_insight_with_fallback(_avoid_access_time));
    }
    auto openat(int dir_fd, const char* pathname, int flags, mode_t mode = ignored_mode) const
        -> int
    {
        flags |= _additional_open_flags;
        if (_avoid_access_time && (flags & O_NOATIME) == 0) {
            int rv = ::openat(dir_fd, pathname, flags | O_NOATIME, mode);
            if (rv >= 0) {
                return rv;
            } else if (errno != EPERM) {
                fail("fs-open-error", dir_fd, std::string(pathname), flags | O_NOATIME);
            } // else: continue
        }
        return check(
            ::openat(dir_fd, pathname, flags, mode),
            "fs-open-error", dir_fd, pathname, flags, mode);
    }
    auto dup(int fd) const -> int
    {
        int operation = (_additional_open_flags & O_CLOEXEC) ? F_DUPFD_CLOEXEC : F_DUPFD;
        return check(::fcntl(fd, operation), "fs-dup-error", fd, operation);
    }
    void pipe(handle& read, handle& write) const
    {
        int pipefd[2] {-1, -1};
        check(::pipe2(pipefd, _additional_open_flags & O_CLOEXEC), "fs-pipe-error");
        handle(pipefd[0]).swap(read);
        handle(pipefd[1]).swap(write);
    }
};


up_fs::fs::context::context(std::string name)
    : _impl(std::make_shared<const impl>(std::move(name), O_CLOEXEC | O_NOCTTY | O_NONBLOCK, false))
{ }

auto up_fs::fs::context::to_insight() const -> up::insight
{
    return up::insight(typeid(*this), "fs-context", _impl->to_insight());
}

auto up_fs::fs::context::working() const -> origin
{
    return origin(origin::init{std::make_shared<const origin::impl>(_impl)});
}

auto up_fs::fs::context::resolved(const up::string_view& pathname, bool follow) const -> origin
{
    return origin(origin::init{std::make_shared<const origin::impl>(_impl, pathname, follow, AT_FDCWD)});
}

auto up_fs::fs::context::operator()() const -> origin
{
    return working();
}

auto up_fs::fs::context::operator()(const up::string_view& pathname, bool follow) const -> origin
{
    return resolved(pathname, follow);
}

class up_fs::fs::origin::impl final
{
public: // --- scope ---
    using self = impl;
private: // --- state ---
    std::shared_ptr<const context::impl> _context;
    handle _handle;
public: // --- life ---
    explicit impl(std::shared_ptr<const context::impl> context)
        : _context(std::move(context)), _handle(-1)
    { }
    explicit impl(std::shared_ptr<const context::impl> context, const up::string_view& pathname, bool follow, int dir_fd)
        : _context(std::move(context)), _handle(-1)
    {
        int flags = O_RDONLY | O_DIRECTORY | O_PATH;
        flags |= follow ? 0 : O_NOFOLLOW;
        _handle = handle(_context->openat(dir_fd, up::nts(pathname), flags));
    }
public: // --- operations ---
    auto to_insight() const -> up::insight
    {
        return up::insight(typeid(*this), "fs-origin-impl",
            up::invoke_to_insight_with_fallback(*_context),
            up::invoke_to_insight_with_fallback(_handle));
    }
    auto get_context() const -> const std::shared_ptr<const context::impl>&
    {
        return _context;
    }
    auto dir_fd() const -> int
    {
        return _handle.get_or(AT_FDCWD);
    }
    auto make_handle(const char* pathname, int flags, mode_t mode = ignored_mode) const
        -> handle
    {
        return handle(_context->openat(dir_fd(), pathname, flags, mode));
    }
    auto working() const -> std::shared_ptr<const self>
    {
        return std::make_shared<const self>(_context);
    }
    auto resolved(const up::string_view& pathname, bool follow) const -> std::shared_ptr<const self>
    {
        return std::make_shared<const self>(_context, pathname, follow, dir_fd());
    }
    auto location() const -> std::string
    {
        int dir_fd = _handle.get_or(AT_FDCWD);
        if (dir_fd == AT_FDCWD) {
            return getcwd();
        }
        auto&& inode = [](int dir_fd) {
            struct stat st;
            check(::fstat(dir_fd, &st), "fs-stat-error", dir_fd);
            return std::make_pair(st.st_dev, st.st_ino);
        };
        int flags = O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_PATH;
        /* Find all mount points of the device, dir_fd belongs to. */
        auto previous = inode(dir_fd);
        using root = std::pair<ino_t, std::string>;
        std::vector<root> roots;
        for (auto&& mount : find_mounts()) {
            if (mount.device() == previous.first) {
                struct stat st;
                check(
                    ::fstatat(AT_FDCWD, mount.path().c_str(), &st, AT_SYMLINK_NOFOLLOW),
                    "fs-stat-error", mount.path());
                roots.emplace_back(st.st_ino, std::move(mount.path()));
            }
        }
        std::sort(roots.begin(), roots.end(), [](auto&& lhs, auto&& rhs) {
                return std::forward_as_tuple(lhs.first, lhs.second.size(), lhs.second)
                    < std::forward_as_tuple(rhs.first, rhs.second.size(), rhs.second);
            });
        /* First check if the directory referenced by dir_fd exists as a
         * mount point. This case is not handled by the loop below. */
        auto p = std::partition_point(roots.begin(), roots.end(), [&previous](auto&& r) {
                return r.first < previous.second;
            });
        if (p != roots.end() && p->first == previous.second) {
            return pathname_normalize(p->second);
        }

        handle current(_context->openat(_handle.get_or(AT_FDCWD), "..", flags));
        auto next = inode(current.get());

        std::vector<up::istring> names;
        while (previous.first == next.first && previous.second != next.second) {
            handle parent(_context->openat(current.get(), "..", flags));
            bool found = scan_directory(std::move(current), [&](auto entry) {
                    if (entry.type() != up_fs::fs::kind::directory) {
                        return false;
                    } else if (entry.inode() != previous.second) {
                        return false;
                    } else {
                        names.push_back(std::move(entry.name()));
                        return true;
                    }
                });
            if (!found) {
                // current directory not found in parent directory
                throw up::make_exception("fs-resolve-error").with(dir_fd);
            }
            auto q = std::partition_point(roots.begin(), roots.end(), [&next](auto&& r) {
                    return r.first < next.second;
                });
            if (q != roots.end() && q->first == next.second) {
                std::string result = q->second;
                while (!names.empty()) {
                    result.append(1, '/');
                    result.append(names.back().to_string());
                    names.pop_back();
                }
                return pathname_normalize(result);
            }
            previous = inode(parent.get());
            up::swap_noexcept(current, parent);
            up::swap_noexcept(next, previous);
        }
        throw up::make_exception("fs-resolve-error").with(dir_fd);
    }
private:
    auto find_mounts() const -> std::vector<mount>
    {
        auto path = origin(origin::init{std::make_shared<impl>(_context)})("/proc/self/mountinfo");
        auto file = up_fs::fs::file(path, {up_fs::fs::file::option::read});
        return parse_file_content(file, 1 << 12, parse_mountinfo);
    }
};


up_fs::fs::origin::origin(init&& arg)
    : origin(std::move(arg._impl))
{ }

auto up_fs::fs::origin::to_insight() const -> up::insight
{
    return up::insight(typeid(*this), "fs-origin", _impl->to_insight());
}

auto up_fs::fs::origin::working() const -> origin
{
    return self(_impl->working());
}

auto up_fs::fs::origin::resolved(const up::string_view& pathname, bool follow) const -> origin
{
    return self(_impl->resolved(pathname, follow));
}

auto up_fs::fs::origin::location() const -> std::string
{
    return _impl->location();
}

auto up_fs::fs::origin::operator()(std::string pathname, bool follow) const -> path
{
    return path(path::init{std::make_shared<const path::impl>(_impl, std::move(pathname), follow)});
}


class up_fs::fs::path::impl final
{
public: // --- scope ---
    using self = impl;
private: // --- state ---
    std::shared_ptr<const origin::impl> _origin;
    std::string _pathname; // note: empty paths are not allowed
    bool _follow;
public: // --- life ---
    explicit impl(std::shared_ptr<const origin::impl> origin, std::string pathname, bool follow)
        : _origin(std::move(origin))
        , _pathname(std::move(pathname))
        , _follow(std::move(follow))
    { }
public: // --- operations ---
    auto to_insight() const -> up::insight
    {
        return up::insight(typeid(*this), "fs-path-impl",
            up::invoke_to_insight_with_fallback(*_origin),
            up::invoke_to_insight_with_fallback(_pathname),
            up::invoke_to_insight_with_fallback(_follow));
    }
    auto get_context() const -> const std::shared_ptr<const context::impl>&
    {
        return _origin->get_context();
    }
    auto make_handle(int flags, mode_t mode = ignored_mode) const -> handle
    {
        flags |= _follow ? 0 : O_NOFOLLOW;
        return _origin->make_handle(_pathname.c_str(), flags, mode);
    }
    auto follow(bool value) const -> std::shared_ptr<const self>
    {
        return std::make_shared<const self>(_origin, _pathname, value);
    }
    auto joined(const up::string_view& pathname) const -> std::shared_ptr<const self>
    {
        if (pathname.compare(0, 1, "/") == 0) {
            return std::make_shared<const self>(
                _origin->working(), std::string(pathname.data(), pathname.size()), _follow);
        } else {
            std::string s = (_pathname + '/').append(pathname.data(), pathname.size());
            return std::make_shared<const self>(_origin, pathname_normalize(s), _follow);
        }
    }
    auto resolved() const -> std::shared_ptr<const origin::impl>
    {
        return _origin->resolved(_pathname, _follow);
    }
    auto stat() const -> std::shared_ptr<const stats::impl>
    {
        auto result = std::make_shared<stats::impl>();
        auto flags = flags_follow(AT_SYMLINK_FOLLOW);
        check(::fstatat(_origin->dir_fd(), _pathname.c_str(), &result->_stat, flags),
            "fs-stat-error", *this, flags);
        return result;
    }
    void chmod(mode_t mode) const
    {
        int flags = flags_nofollow(AT_SYMLINK_NOFOLLOW);
        check(::fchmodat(_origin->dir_fd(), _pathname.c_str(), mode, flags),
            "fs-chmod-error", *this, mode, flags);
    }
    void chown(uid_t owner, gid_t group) const
    {
        int flags = flags_nofollow(AT_SYMLINK_NOFOLLOW);
        check(::fchownat(_origin->dir_fd(), _pathname.c_str(), owner, group, flags),
            "fs-chown-error", *this, owner, group, flags);
    }
    void mkdir(mode_t mode) const
    {
        check(::mkdirat(_origin->dir_fd(), _pathname.c_str(), mode),
            "fs-mkdir-error", *this, mode);
    }
    void rmdir() const
    {
        check(::unlinkat(_origin->dir_fd(), _pathname.c_str(), AT_REMOVEDIR),
            "fs-rmdir-error", *this);
    }
    void link(const self& target) const
    {
        int flags = flags_follow(AT_SYMLINK_FOLLOW);
        check(::linkat(
                _origin->dir_fd(), _pathname.c_str(),
                target._origin->dir_fd(), target._pathname.c_str(),
                flags),
            "fs-link-error", *this, target, flags);
    }
    void unlink() const
    {
        check(::unlinkat(_origin->dir_fd(), _pathname.c_str(), 0),
            "fs-unlink-error", *this);
    }
    void rename(const self& target, bool replace) const
    {
        check(::syscall_renameat2(
                _origin->dir_fd(), _pathname.c_str(),
                target._origin->dir_fd(), target._pathname.c_str(),
                replace ? 0 : RENAME_NOREPLACE),
            "fs-rename-error", *this, target, replace);
    }
    void exchange(const self& target) const
    {
        check(::syscall_renameat2(
                _origin->dir_fd(), _pathname.c_str(),
                target._origin->dir_fd(), target._pathname.c_str(),
                RENAME_EXCHANGE),
            "fs-exchange-error", *this, target);
    }
    auto readlink() const -> std::string
    {
        for (std::size_t i = 8; i <= 16; ++i) {
            std::size_t size = 1 << i;
            auto buffer = std::make_unique<char[]>(size);
            auto rv = check(
                ::readlinkat(_origin->dir_fd(), _pathname.c_str(), buffer.get(), size),
                "fs-readlink-error", *this);
            if (static_cast<std::size_t>(rv) < size) {
                return std::string(buffer.get(), static_cast<std::size_t>(rv));
            }
        }
        throw up::make_exception("fs-readlink-error").with(up::errno_info(errno));
    }
    void symlink(const up::string_view& value) const
    {
        check(::symlinkat(up::nts(value), _origin->dir_fd(), up::nts(_pathname)),
            "fs-symlink-error", *this, value);
    }
    auto list() const -> std::vector<directory_entry>
    {
        int flags = flags_nofollow(O_NOFOLLOW, O_RDONLY | O_DIRECTORY);
        return scan_directory(_origin->make_handle(_pathname.c_str(), flags));
    }
    bool list(std::function<bool(directory_entry)>&& visitor) const
    {
        int flags = flags_nofollow(O_NOFOLLOW, O_RDONLY | O_DIRECTORY);
        return scan_directory(_origin->make_handle(_pathname.c_str(), flags), std::move(visitor));
    }
    auto absolute() const -> std::string
    {
        std::string result;
        if (_pathname.compare(0, 1, "/") == 0) {
            return pathname_normalize(_pathname);
        } else {
            return pathname_normalize(_origin->location() + '/' + _pathname);
        }
    }
private:
    auto flags_follow(int additional, int base = 0) const -> int
    {
        return _follow ? base | additional : base;
    }
    auto flags_nofollow(int additional, int base = 0) const -> int
    {
        return _follow ? base : base | additional;
    }
};


class up_fs::fs::path::accessor final
{
public:
    static auto get_impl(const path& path) -> auto&
    {
        return path._impl;
    }
};


up_fs::fs::path::path(init&& arg)
    : path(std::move(arg._impl))
{ }

auto up_fs::fs::path::to_insight() const -> up::insight
{
    return up::insight(typeid(*this), "fs-path", _impl->to_insight());
}

auto up_fs::fs::path::follow(bool value) const -> self
{
    return self(_impl->follow(value));
}

auto up_fs::fs::path::joined(const up::string_view& pathname) const -> self
{
    return self(_impl->joined(pathname));
}

auto up_fs::fs::path::resolved() const -> origin
{
    return origin(origin::init{_impl->resolved()});
}

auto up_fs::fs::path::stat() const -> stats
{
    return stats(stats::init{_impl->stat()});
}

void up_fs::fs::path::chmod(mode_t mode) const
{
    _impl->chmod(mode);
}

void up_fs::fs::path::chown(uid_t owner, gid_t group) const
{
    _impl->chown(owner, group);
}

void up_fs::fs::path::mkdir(mode_t mode) const
{
    _impl->mkdir(mode);
}

void up_fs::fs::path::rmdir() const
{
    _impl->rmdir();
}

void up_fs::fs::path::link(const path& target) const
{
    _impl->link(*target._impl);
}

void up_fs::fs::path::unlink() const
{
    _impl->unlink();
}

void up_fs::fs::path::rename(const path& target, bool replace) const
{
    _impl->rename(*target._impl, replace);
}

void up_fs::fs::path::exchange(const path& target) const
{
    _impl->exchange(*target._impl);
}

auto up_fs::fs::path::readlink() const -> std::string
{
    return _impl->readlink();
}

void up_fs::fs::path::symlink(const up::string_view& value) const
{
    _impl->symlink(value);
}

auto up_fs::fs::path::list() const -> std::vector<directory_entry>
{
    return _impl->list();
}

bool up_fs::fs::path::list(std::function<bool(directory_entry)> visitor) const
{
    return _impl->list(std::move(visitor));
}

auto up_fs::fs::path::statvfs() const -> statfs
{
    auto result = std::make_shared<statfs::impl>();
    std::string pathname = absolute();
    int rv;
    do {
        rv = ::statvfs(pathname.c_str(), &result->_statvfs);
    } while (rv == -1 && errno == EINTR);
    check(rv, "fs-statvfs-error", *this);
    return statfs(statfs::init{result});
}

void up_fs::fs::path::truncate(off_t length) const
{
    check(::truncate(absolute().c_str(), length),
        "fs-truncate-error", *this, length);
}

auto up_fs::fs::path::absolute() const -> std::string
{
    return _impl->absolute();
}


class up_fs::fs::object::impl
{
public: // --- scope ---
    using self = impl;
protected: // --- state ---
    handle _handle;
public: // --- life ---
    explicit impl(handle handle)
        : _handle(std::move(handle))
    { }
public: // --- operations ---
    auto fd() const -> int
    {
        return _handle.get();
    }
    void chmod(mode_t mode) const
    {
        check(::fchmod(fd(), mode), "fs-chmod-error", fd(), mode);
    }
    void chown(uid_t owner, gid_t group) const
    {
        check(::fchown(fd(), owner, group), "fs-chown-error", fd(), owner, group);
    }
    auto stat() const -> std::shared_ptr<const stats::impl>
    {
        auto result = std::make_shared<stats::impl>();
        check(::fstat(fd(), &result->_stat), "fs-stat-error", fd());
        return result;
    }
    auto statvfs() const -> std::shared_ptr<const statfs::impl>
    {
        auto result = std::make_shared<statfs::impl>();
        int rv;
        do {
            rv = ::fstatvfs(fd(), &result->_statvfs);
        } while (rv == -1 && errno == EINTR);
        check(rv, "fs-statvfs-error", fd());
        return result;
    }
    void fdatasync() const
    {
        check(::fdatasync(fd()), "fs-fdatasync-error", fd());
    }
    void fsync() const
    {
        check(::fsync(fd()), "fs-fsync-error", fd());
    }
};


up_fs::fs::object::object(init&& arg)
    : _impl(std::move(arg._impl))
{ }

up_fs::fs::object::object(const path& path)
    : _impl(std::make_shared<const impl>(path::accessor::get_impl(path)->make_handle(O_RDONLY)))
{ }

void up_fs::fs::object::chmod(mode_t mode) const
{
    _impl->chmod(mode);
}

void up_fs::fs::object::chown(uid_t owner, gid_t group) const
{
    _impl->chown(owner, group);
}

auto up_fs::fs::object::stat() const -> stats
{
    return stats(stats::init{_impl->stat()});
}

auto up_fs::fs::object::statvfs() const -> statfs
{
    return statfs(statfs::init{_impl->statvfs()});
}

void up_fs::fs::object::fdatasync() const
{
    _impl->fdatasync();
}

void up_fs::fs::object::fsync() const
{
    _impl->fsync();
}


class up_fs::fs::file::impl final : public object::impl
{
public: // --- scope ---
    using self = impl;
private: // --- state ---
    std::shared_ptr<const context::impl> _context;
public: // --- life ---
    explicit impl(handle handle, std::shared_ptr<const context::impl> context)
        : object::impl(std::move(handle))
        , _context(std::move(context))
    { }
public: // --- operations ---
    auto dup() const -> int
    {
        return _context->dup(_handle.get());
    }
    void pipe(handle& read, handle& write) const
    {
        _context->pipe(read, write);
    }
};


up_fs::fs::file::file(const path& path, options options)
    : _impl(nullptr)
{
    int flags = 0;
    if (options.all(option::read, option::write)) {
        flags |= O_RDWR;
    } else if (options.all(option::read)) {
        flags |= O_RDONLY;
    } else if (options.all(option::write)) {
        flags |= O_WRONLY;
    } // else: ignore
    if (options.all(option::append)) {
        flags |= O_APPEND;
    }
    if (options.all(option::create)) {
        flags |= O_CREAT;
    }
    if (options.all(option::exclusive)) {
        flags |= O_EXCL;
    }
    if (options.all(option::tmpfile)) {
        flags |= O_TMPFILE;
    }
    if (options.all(option::truncate)) {
        flags |= O_TRUNC;
    }
    mode_t mode = S_IRUSR | S_IWUSR;
    if (options.all(option::executable)) {
        mode |= S_IXUSR;
    }
    if (options.all(option::group)) {
        mode |= S_IRGRP | S_IWGRP;
    }
    if (options.all(option::group, option::executable)) {
        mode |= S_IXGRP;
    }
    if (options.all(option::others)) {
        mode |= S_IROTH | S_IWOTH;
    }
    if (options.all(option::others, option::executable)) {
        mode |= S_IXOTH;
    }
    auto&& p = path::accessor::get_impl(path);
    _impl = std::make_shared<const impl>(p->make_handle(flags, mode), p->get_context());
}


up_fs::fs::file::operator object() const
{
    return object(object::init{_impl});
}

void up_fs::fs::file::chmod(mode_t mode) const
{
    _impl->chmod(mode);
}

void up_fs::fs::file::chown(uid_t owner, gid_t group) const
{
    _impl->chown(owner, group);
}

auto up_fs::fs::file::stat() const -> stats
{
    return stats(stats::init{_impl->stat()});
}

auto up_fs::fs::file::statvfs() const -> statfs
{
    return statfs(statfs::init{_impl->statvfs()});
}

void up_fs::fs::file::fdatasync() const
{
    _impl->fdatasync();
}

void up_fs::fs::file::fsync() const
{
    _impl->fsync();
}

void up_fs::fs::file::truncate(off_t length) const
{
    int rv;
    do {
        rv = ::ftruncate(_impl->fd(), length);
    } while (rv == -1 && errno == EINTR);
    check(rv, "fs-truncate-error", _impl->fd(), length);
}

auto up_fs::fs::file::read_some(up::chunk::into chunk, off_t offset) const
    -> std::size_t
{
    return do_io(::pread, _impl->fd(), std::move(chunk), offset, "fs-read-error");
}

auto up_fs::fs::file::write_some(up::chunk::from chunk, off_t offset) const
    -> std::size_t
{
    return do_io(::pwrite, _impl->fd(), std::move(chunk), offset, "fs-write-error");
}

auto up_fs::fs::file::read_some(up::chunk::into_bulk_t&& chunks, off_t offset) const -> std::size_t
{
    return do_iov(::preadv, _impl->fd(), std::move(chunks), offset, "fs-readv-error");
}

auto up_fs::fs::file::write_some(up::chunk::from_bulk_t&& chunks, off_t offset) const -> std::size_t
{
    return do_iov(::pwritev, _impl->fd(), std::move(chunks), offset, "fs-writev-error");
}

void up_fs::fs::file::write_all(up::chunk::from chunk, off_t offset) const
{
    /* A do-while loop is used, so that the function on the underlying engine
     * is called at least once. This is intentionally, so that the
     * implementation is similar to write_some. */
    do {
        auto n = do_io(::pwrite, _impl->fd(), chunk, offset, "fs-write-all-error");
        chunk.drain(n);
        offset += n;
    } while (chunk.size());
}

void up_fs::fs::file::write_all(up::chunk::from_bulk_t&& chunks, off_t offset) const
{
    /* See above regarding the use of a do-while loop. */
    do {
        auto n = do_iov(::pwritev, _impl->fd(), chunks, offset, "fs-writev-all-error");
        chunks.drain(n);
        offset += n;
    } while (chunks.total());
}

void up_fs::fs::file::posix_fallocate(off_t offset, off_t length) const
{
    while (auto rv = ::posix_fallocate(_impl->fd(), offset, length)) {
        if (rv == EINTR) {
            // continue
        } else {
            throw up::make_exception("fs-posix-fallocate-error")
                .with(_impl->fd(), offset, length, up::errno_info(rv));
        }
    }
}

void up_fs::fs::file::posix_fadvise(off_t offset, off_t length, int advice) const
{
    if (auto rv = ::posix_fadvise(_impl->fd(), offset, length, advice)) {
        throw up::make_exception("fs-posix-fadvise-error")
            .with(_impl->fd(), offset, length, advice, up::errno_info(rv));
    }
}

void up_fs::fs::file::linkto(const path& target) const
{
    auto source = "/proc/self/fd/" + std::to_string(_impl->fd());
    target.joined(source).follow(true).link(target);
}

auto up_fs::fs::file::acquire_lock(bool exclusive, bool blocking) const -> lock
{
    return lock(lock::init{std::make_shared<const lock::impl>(_impl, exclusive, blocking)});
}

auto up_fs::fs::file::make_channel() const -> channel
{
    return channel(channel::init{up::impl_make(_impl)});
}


class up_fs::fs::file::lock::impl final
{
public: // --- scope ---
    using self = impl;
private: // --- state ---
    std::shared_ptr<const file::impl> _file;
    bool _exclusive;
public: // --- life ---
    explicit impl(std::shared_ptr<const file::impl> file, bool exclusive, bool blocking)
        : _file(std::move(file)), _exclusive(std::move(exclusive))
    {
        int operation = (_exclusive ? LOCK_EX : LOCK_SH) | (blocking ? 0 : LOCK_NB);
        perform(operation);
    }
    impl(const self& rhs) = delete;
    impl(self&& rhs) noexcept = delete;
    ~impl() noexcept
    {
        perform(LOCK_UN);
    }
public: // --- operations ---
    auto operator=(const self& rhs) & -> self& = delete;
    auto operator=(self&& rhs) & noexcept -> self& = delete;
private:
    void perform(int operation)
    {
        int rv;
        do {
            rv = ::flock(_file->fd(), operation);
        } while (rv == -1 && errno == EINTR);
        if (rv == -1 && errno == EWOULDBLOCK) {
            throw up::make_exception("fs-file-already-locked", up_fs::fs::locked_file())
                .with(_file->fd(), operation);
        } else {
            check(rv, "fs-file-lock", _file->fd(), operation);
        }
    }
};


up_fs::fs::file::lock::lock(init&& arg)
    : _impl(std::move(arg._impl))
{ }


class up_fs::fs::file::channel::impl final
{
public: // --- scope ---
    using self = impl;
private: // --- state ---
    std::shared_ptr<const file::impl> _file;
    handle _read;
    handle _write;
public: // --- life ---
    explicit impl(std::shared_ptr<const file::impl> file)
        : _file(std::move(file))
    {
        _file->pipe(_read, _write);
    }
    impl(const self& rhs) = delete;
    impl(self&& rhs) noexcept = delete;
    ~impl() noexcept = default;
public: // --- operations ---
    auto operator=(const self& rhs) & -> self& = delete;
    auto operator=(self&& rhs) & noexcept -> self& = delete;
    auto fill(const file::impl& source, std::size_t size, off_t offset) -> std::size_t
    {
        ssize_t rv = ::splice(source.fd(), &offset,
            _write.get(), nullptr, size, SPLICE_F_MOVE);
        check(rv, "fs-splice-error", source.fd(), offset, size);
        return static_cast<std::size_t>(rv);
    }
    auto drain(std::size_t size, off_t offset) -> std::size_t
    {
        ssize_t rv = ::splice(_read.get(), nullptr,
            _file->fd(), &offset, size, SPLICE_F_MOVE);
        check(rv, "fs-splice-error", _file->fd(), size, offset);
        return  static_cast<std::size_t>(rv);
    }
};


void up_fs::fs::file::channel::destroy(impl* ptr)
{
    std::default_delete<impl>()(ptr);
}

up_fs::fs::file::channel::channel(init&& arg)
    : _impl(std::move(arg._impl))
{ }

auto up_fs::fs::file::channel::fill(const file& source, std::size_t size, off_t offset)
    -> std::size_t
{
    return _impl->fill(*source._impl, size, offset);
}

auto up_fs::fs::file::channel::drain(std::size_t size, off_t offset) -> std::size_t
{
    return _impl->drain(size, offset);
}


class up_fs::fs::directory::impl final : public object::impl
{
public: // --- scope ---
    using self = impl;
private: // --- state ---
    std::shared_ptr<const context::impl> _context;
public: // --- life ---
    explicit impl(handle handle, std::shared_ptr<const context::impl> context)
        : object::impl(std::move(handle))
        , _context(std::move(context))
    { }
public: // --- operations ---
    auto unique_handle(bool movable) -> handle
    {
        if (movable) {
            return std::move(_handle);
        } else {
            int flags = O_RDONLY | O_DIRECTORY | O_NOFOLLOW;
            return handle(_context->openat(_handle.get(), ".", flags));
        }
    }
};


up_fs::fs::directory::directory(const path& path)
    : _impl(nullptr)
{
    auto&& p = path::accessor::get_impl(path);
    int flags = O_RDONLY | O_DIRECTORY;
    _impl = std::make_shared<impl>(p->make_handle(flags), p->get_context());
 }

up_fs::fs::directory::operator object() const
{
    return object(object::init{_impl});
}

void up_fs::fs::directory::chmod(mode_t mode) const
{
    _impl->chmod(mode);
}

void up_fs::fs::directory::chown(uid_t owner, gid_t group) const
{
    _impl->chown(owner, group);
}

auto up_fs::fs::directory::stat() const -> stats
{
    return stats(stats::init{_impl->stat()});
}

auto up_fs::fs::directory::statvfs() const -> statfs
{
    return statfs(statfs::init{_impl->statvfs()});
}

void up_fs::fs::directory::fdatasync() const
{
    _impl->fdatasync();
}

void up_fs::fs::directory::fsync() const
{
    _impl->fsync();
}

auto up_fs::fs::directory::list() const & -> std::vector<directory_entry>
{
    return scan_directory(_impl->unique_handle(false));
}

auto up_fs::fs::directory::list() && -> std::vector<directory_entry>
{
    return scan_directory(_impl->unique_handle(_impl.unique()));
}

bool up_fs::fs::directory::list(std::function<bool(directory_entry)> visitor) const &
{
    return scan_directory(_impl->unique_handle(false), std::move(visitor));
}

bool up_fs::fs::directory::list(std::function<bool(directory_entry)> visitor) &&
{
    return scan_directory(_impl->unique_handle(_impl.unique()), std::move(visitor));
}

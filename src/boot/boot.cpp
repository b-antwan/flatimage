///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : boot
///

// Version
#ifndef VERSION
#define VERSION "unknown"
#endif

// Git commit hash
#ifndef COMMIT
#define COMMIT "unknown"
#endif

// Compilation timestamp
#ifndef TIMESTAMP
#define TIMESTAMP "unknown"
#endif

#include <elf.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <filesystem>

#include "../cpp/lib/linux.hpp"
#include "../cpp/lib/env.hpp"
#include "../cpp/lib/log.hpp"
#include "../cpp/lib/elf.hpp"

#include "config/config.hpp"
#include "parser.hpp"
#include "portal.hpp"

// Unix environment variables
extern char** environ;

namespace fs = std::filesystem;

constexpr std::array<const char*,403> const arr_busybox_applet
{
  "[","[[","acpid","add-shell","addgroup","adduser","adjtimex","arch","arp","arping","ascii","ash","awk","base32","base64",
  "basename","bc","beep","blkdiscard","blkid","blockdev","bootchartd","brctl","bunzip2","bzcat","bzip2","cal","cat","chat",
  "chattr","chgrp","chmod","chown","chpasswd","chpst","chroot","chrt","chvt","cksum","clear","cmp","comm","conspy","cp","cpio",
  "crc32","crond","crontab","cryptpw","cttyhack","cut","date","dc","dd","deallocvt","delgroup","deluser","depmod","devmem",
  "df","dhcprelay","diff","dirname","dmesg","dnsd","dnsdomainname","dos2unix","dpkg","dpkg-deb","du","dumpkmap",
  "dumpleases","echo","ed","egrep","eject","env","envdir","envuidgid","ether-wake","expand","expr","factor","fakeidentd",
  "fallocate","false","fatattr","fbset","fbsplash","fdflush","fdformat","fdisk","fgconsole","fgrep","find","findfs",
  "flock","fold","free","freeramdisk","fsck","fsck.minix","fsfreeze","fstrim","fsync","ftpd","ftpget","ftpput","fuser",
  "getfattr","getopt","getty","grep","groups","gunzip","gzip","halt","hd","hdparm","head","hexdump","hexedit","hostid",
  "hostname","httpd","hush","hwclock","i2cdetect","i2cdump","i2cget","i2cset","i2ctransfer","id","ifconfig","ifdown",
  "ifenslave","ifplugd","ifup","inetd","init","insmod","install","ionice","iostat","ip","ipaddr","ipcalc","ipcrm","ipcs",
  "iplink","ipneigh","iproute","iprule","iptunnel","kbd_mode","kill","killall","killall5","klogd","last","less","link",
  "linux32","linux64","linuxrc","ln","loadfont","loadkmap","logger","login","logname","logread","losetup","lpd","lpq",
  "lpr","ls","lsattr","lsmod","lsof","lspci","lsscsi","lsusb","lzcat","lzma","lzop","makedevs","makemime","man","md5sum",
  "mdev","mesg","microcom","mim","mkdir","mkdosfs","mke2fs","mkfifo","mkfs.ext2","mkfs.minix","mkfs.vfat","mknod",
  "mkpasswd","mkswap","mktemp","modinfo","modprobe","more","mount","mountpoint","mpstat","mt","mv","nameif","nanddump",
  "nandwrite","nbd-client","nc","netstat","nice","nl","nmeter","nohup","nologin","nproc","nsenter","nslookup","ntpd","od",
  "openvt","partprobe","passwd","paste","patch","pgrep","pidof","ping","ping6","pipe_progress","pivot_root","pkill",
  "pmap","popmaildir","poweroff","powertop","printenv","printf","ps","pscan","pstree","pwd","pwdx","raidautorun","rdate",
  "rdev","readahead","readlink","readprofile","realpath","reboot","reformime","remove-shell","renice","reset",
  "resize","resume","rev","rm","rmdir","rmmod","route","rpm","rpm2cpio","rtcwake","run-init","run-parts","runlevel",
  "runsv","runsvdir","rx","script","scriptreplay","sed","seedrng","sendmail","seq","setarch","setconsole","setfattr",
  "setfont","setkeycodes","setlogcons","setpriv","setserial","setsid","setuidgid","sh","sha1sum","sha256sum",
  "sha3sum","sha512sum","showkey","shred","shuf","slattach","sleep","smemcap","softlimit","sort","split","ssl_client",
  "start-stop-daemon","stat","strings","stty","su","sulogin","sum","sv","svc","svlogd","svok","swapoff","swapon",
  "switch_root","sync","sysctl","syslogd","tac","tail","tar","taskset","tc","tcpsvd","tee","telnet","telnetd","test","tftp",
  "tftpd","time","timeout","top","touch","tr","traceroute","traceroute6","tree","true","truncate","ts","tsort","tty",
  "ttysize","tunctl","ubiattach","ubidetach","ubimkvol","ubirename","ubirmvol","ubirsvol","ubiupdatevol","udhcpc",
  "udhcpc6","udhcpd","udpsvd","uevent","umount","uname","unexpand","uniq","unix2dos","unlink","unlzma","unshare","unxz",
  "unzip","uptime","users","usleep","uudecode","uuencode","vconfig","vi","vlock","volname","w","wall","watch","watchdog",
  "wc","wget","which","who","whoami","whois","xargs","xxd","xz","xzcat","yes","zcat","zcip",
};

// relocate() {{{
void relocate(char** argv)
{
  // This part of the code is executed to write the runner,
  // rightafter the code is replaced by the runner.
  // This is done because the current executable cannot mount itself.

  // Get path to called executable
  ethrow_if(!std::filesystem::exists("/proc/self/exe"), "Error retrieving executable path for self");
  auto path_absolute = fs::read_symlink("/proc/self/exe");

  // Create base dir
  fs::path path_dir_base = "/tmp/fim";
  ethrow_if (not fs::exists(path_dir_base) and not fs::create_directories(path_dir_base)
    , "Failed to create directory {}"_fmt(path_dir_base)
  );

  // Make the temporary directory name
  fs::path path_dir_app = path_dir_base / "app" / "{}_{}"_fmt(COMMIT, TIMESTAMP);
  ethrow_if(not fs::exists(path_dir_app) and not fs::create_directories(path_dir_app)
    , "Failed to create directory {}"_fmt(path_dir_app)
  );

  // Create bin dir
  fs::path path_dir_app_bin = path_dir_app / "bin";
  ethrow_if(not fs::exists(path_dir_app_bin) and not fs::create_directories(path_dir_app_bin),
    "Failed to create directory {}"_fmt(path_dir_app_bin)
  );

  // Create busybox dir
  fs::path path_dir_busybox = path_dir_app_bin / "busybox";
  ethrow_if(not fs::exists(path_dir_busybox) and not fs::create_directories(path_dir_busybox),
    "Failed to create directory {}"_fmt(path_dir_busybox)
  );

  // Set variables
  ns_env::set("FIM_DIR_GLOBAL", path_dir_base.c_str(), ns_env::Replace::Y);
  ns_env::set("FIM_DIR_APP", path_dir_app.c_str(), ns_env::Replace::Y);
  ns_env::set("FIM_DIR_APP_BIN", path_dir_app_bin.c_str(), ns_env::Replace::Y);
  ns_env::set("FIM_DIR_BUSYBOX", path_dir_busybox.c_str(), ns_env::Replace::Y);
  ns_env::set("FIM_FILE_BINARY", path_absolute.c_str(), ns_env::Replace::Y);

  // Create instance directory
  fs::path path_dir_instance_prefix = "{}/{}/"_fmt(path_dir_app, "instance");

  // Create temp dir to mount filesystems into
  fs::path path_dir_instance = ns_linux::mkdtemp(path_dir_instance_prefix);
  ns_env::set("FIM_DIR_INSTANCE", path_dir_instance.c_str(), ns_env::Replace::Y);

  // Path to directory with mount points
  fs::path path_dir_mount = path_dir_instance / "mount";
  ns_env::set("FIM_DIR_MOUNT", path_dir_mount.c_str(), ns_env::Replace::Y);
  ethrow_if(not fs::exists(path_dir_mount) and not fs::create_directory(path_dir_mount)
    , "Could not mount directory '{}'"_fmt(path_dir_mount)
  );

  // Path to ext mount dir is a directory called 'ext'
  fs::path path_dir_mount_ext = path_dir_mount / "ext";
  ns_env::set("FIM_DIR_MOUNT_EXT", path_dir_mount_ext.c_str(), ns_env::Replace::Y);
  ethrow_if(not fs::exists(path_dir_mount_ext) and not fs::create_directory(path_dir_mount_ext)
    , "Could not mount directory '{}'"_fmt(path_dir_mount_ext)
  );

  // Starting offsets
  uint64_t offset_beg = 0;
  uint64_t offset_end = ns_elf::skip_elf_header(path_absolute.c_str());

  // Write by binary header offset
  auto f_write_from_header = [&](fs::path path_file, uint64_t offset_end)
  {
    // Update offsets
    offset_beg = offset_end;
    offset_end = ns_elf::skip_elf_header(path_absolute.c_str(), offset_beg) + offset_beg;
    // Write binary only if it doesnt already exist
    if ( ! fs::exists(path_file) )
    {
      ns_elf::copy_binary(path_absolute.string(), path_file, {offset_beg, offset_end});
    }
    // Set permissions
    fs::permissions(path_file.c_str(), fs::perms::owner_all | fs::perms::group_all);
    // Return new values for offsets
    return std::make_pair(offset_beg, offset_end);
  };

  // Write by binary byte offset
  auto f_write_from_offset = [&](std::ifstream& file_binary, fs::path path_file, uint64_t offset_end)
  {
    uint64_t offset_beg = offset_end;
    // Set file position
    file_binary.seekg(offset_beg);
    // Read size bytes (FATAL if fails)
    uint64_t size;
    ethrow_if(not file_binary.read(reinterpret_cast<char*>(&size), sizeof(size)), "Could not read binary size");
    // Open output file and write 
    if ( lec(fs::exists, path_file) )
    {
      file_binary.seekg(offset_beg + size + sizeof(size));
    } // if
    else
    {
      // Read binary
      std::vector<char> buffer(size);
      ethrow_if(not file_binary.read(buffer.data(), size), "Could not read binary");
      // Open output binary file
      std::ofstream of{path_file, std::ios::out | std::ios::binary};
      ethrow_if(not of.is_open(), "Could not open output file '{}'"_fmt(path_file));
      // Write binary
      ethrow_if(not of.write(buffer.data(), size), "Could not write binary file '{}'"_fmt(path_file));
      // Set permissions
      lec(fs::permissions, path_file.c_str(), fs::perms::owner_all | fs::perms::group_all);
    } // if
    // Return new values for offsets
    return std::make_pair(offset_beg, file_binary.tellg());
  };

  // Write binaries
  auto start = std::chrono::high_resolution_clock::now();
  fs::path path_file_dwarfs_aio = path_dir_app_bin / "dwarfs_aio";
  std::ifstream file_binary{path_absolute, std::ios::in | std::ios::binary};
  ethrow_if(not file_binary.is_open(), "Could not open flatimage binary file");
  std::tie(offset_beg, offset_end) = f_write_from_header(path_dir_instance / "fim_boot" , 0);
  std::tie(offset_beg, offset_end) = f_write_from_offset(file_binary, path_dir_app_bin / "bash", offset_end);
  std::tie(offset_beg, offset_end) = f_write_from_offset(file_binary, path_dir_busybox / "busybox", offset_end);
  std::tie(offset_beg, offset_end) = f_write_from_offset(file_binary, path_dir_app_bin / "bwrap", offset_end);
  std::tie(offset_beg, offset_end) = f_write_from_offset(file_binary, path_dir_app_bin / "ciopfs", offset_end);
  std::tie(offset_beg, offset_end) = f_write_from_offset(file_binary, path_file_dwarfs_aio, offset_end);
  std::tie(offset_beg, offset_end) = f_write_from_offset(file_binary, path_dir_app_bin / "fim_portal", offset_end);
  std::tie(offset_beg, offset_end) = f_write_from_offset(file_binary, path_dir_app_bin / "fim_portal_daemon", offset_end);
  std::tie(offset_beg, offset_end) = f_write_from_offset(file_binary, path_dir_app_bin / "fim_bwrap_apparmor", offset_end);
  std::tie(offset_beg, offset_end) = f_write_from_offset(file_binary, path_dir_app_bin / "janitor", offset_end);
  std::tie(offset_beg, offset_end) = f_write_from_offset(file_binary, path_dir_app_bin / "lsof", offset_end);
  std::tie(offset_beg, offset_end) = f_write_from_offset(file_binary, path_dir_app_bin / "overlayfs", offset_end);
  std::tie(offset_beg, offset_end) = f_write_from_offset(file_binary, path_dir_app_bin / "unionfs", offset_end);
  std::tie(offset_beg, offset_end) = f_write_from_offset(file_binary, path_dir_app_bin / "proot", offset_end);
  file_binary.close();
  std::error_code ec;
  fs::create_symlink(path_file_dwarfs_aio, path_dir_app_bin / "dwarfs", ec);
  fs::create_symlink(path_file_dwarfs_aio, path_dir_app_bin / "mkdwarfs", ec);
  auto end = std::chrono::high_resolution_clock::now();

  // Create busybox symlinks, allow (symlinks exists) errors
  for(auto const& busybox_applet : arr_busybox_applet)
  {
    fs::create_symlink(path_dir_busybox / "busybox", path_dir_busybox / busybox_applet, ec);
  } // for

  // Filesystem starts here
  ns_env::set("FIM_OFFSET", std::to_string(offset_end).c_str(), ns_env::Replace::Y);
  ns_log::debug()("FIM_OFFSET: {}", offset_end);

  // Option to show offset and exit (to manually mount the fs with fuse2fs)
  if( getenv("FIM_MAIN_OFFSET") ){ println(offset_end); exit(0); }

  // Print copy duration
  if ( getenv("FIM_DEBUG") != nullptr )
  {
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    "Copy binaries finished in '{}' ms"_print(elapsed.count());
  } // if

  // Launch Runner
  execve("{}/fim_boot"_fmt(path_dir_instance).c_str(), argv, environ);
} // relocate() }}}

// boot() {{{
std::unique_ptr<ns_config::FlatimageConfig> boot(int argc, char** argv)
{

  // Setup environment variables
  auto config = std::make_unique<ns_config::FlatimageConfig>(ns_config::config());


  // Set log file
  ns_log::set_sink_file(config->path_dir_mount.string() + ".boot.log");

  // Start portal
  ns_portal::Portal portal = ns_portal::Portal(config->path_dir_instance / "fim_boot");

  // Refresh desktop integration
  ns_log::exception([&]{ ns_desktop::integrate(*config); });

  // Parse flatimage command if exists
  ns_parser::parse_cmds(*config, argc, argv);

  return config;
} // boot() }}}

// main() {{{
int main(int argc, char** argv)
{
  // Initialize logger level
  if ( ns_env::exists("FIM_DEBUG", "1") )
  {
    ns_log::set_level(ns_log::Level::DEBUG);
  } // if

  // Print version and exit
  if ( argc > 1 && std::string{argv[1]} == "fim-version" )
  {
    println(VERSION);
    return EXIT_SUCCESS;
  } // if
  ns_env::set("FIM_VERSION", VERSION, ns_env::Replace::Y);

  // Check if linux has the fuse module loaded
  auto expected_module_check = ns_linux::module_check("fuse");
  elog_if(not expected_module_check, expected_module_check.error());
  elog_if(expected_module_check and not *expected_module_check, "'fuse' module is not loaded");

  // Get path to self
  auto expected_path_file_self = ns_filesystem::ns_path::file_self();
  ereturn_if(not expected_path_file_self, expected_path_file_self.error(), EXIT_FAILURE);

  // If it is outside /tmp, move the binary
  fs::path path_file_self = *expected_path_file_self;
  if ( fs::file_size(path_file_self) != ns_elf::skip_elf_header(path_file_self) )
  {
    ns_log::debug()("Relocating binary");
    relocate(argv);
    // This function should not reach the return statement due to evecve
    return EXIT_FAILURE;
  } // if

  // Boot the main program
  if ( auto expected_config = ns_exception::to_expected([&]{ return boot(argc, argv); }); expected_config )
  {
    // Wait until flatimage is not busy
    if (auto error = ns_subprocess::wait_busy_file((*expected_config)->path_file_binary); error)
    {
      ns_log::error()(*error);
    } // if
  } // if
  else
  {
    println("Program exited with error: {}", expected_config.error());
    return EXIT_FAILURE;
  } // else

  return EXIT_SUCCESS;
} // main() }}}

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/

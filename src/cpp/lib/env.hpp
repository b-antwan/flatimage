///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : env
///

#pragma once

#include <filesystem>
#include <cstdlib>
#include <wordexp.h>

#include "../std/string.hpp"
#include "../common.hpp"
#include "../macro.hpp"
#include "log.hpp"
#include "subprocess.hpp"

// Environment variable handling {{{
namespace ns_env
{

namespace fs = std::filesystem;

// enum class Replace {{{
enum class Replace
{
  Y,
  N,
}; // enum class Replace }}}

// dir() {{{
// Fetches a directory path from an environment variable
// Tries to create if not exists
inline fs::path dir(const char* name)
{
  // Get environment variable
  const char * value = std::getenv(name) ;

  // Check if variable exists
  ethrow_if(not value, "Variable '{}' not set"_fmt(name));

  // Create if not exists
  ethrow_if(not fs::create_directory(value), "Could not create directory {}"_fmt(value));

  return fs::canonical(value);
} // dir() }}}

// file() {{{
// Fetches a file path from an environment variable
// Checks if file exists
inline fs::path file(const char* name)
{
  // Get environment variable
  const char * value = std::getenv(name) ;

  // Check if variable exists
  ethrow_if(not value, "Variable '{}' not set"_fmt(name));

  // Create if not exists
  ethrow_if(not fs::exists(value), "File '{}' does not exist"_fmt(value));

  return fs::canonical(value);
} // file() }}}

// set() {{{
// Sets an environment variable
template<ns_concept::StringRepresentable T, ns_concept::StringRepresentable U>
void set(T&& name, U&& value, Replace replace)
{
  // ns_log::debug()("ENV: {} -> {}", name , value);
  setenv(ns_string::to_string(name).c_str(), ns_string::to_string(value).c_str(), (replace == Replace::Y));
} // set() }}}

// prepend() {{{
// Prepends 'extra' to an environment variable 'name'
inline void prepend(const char* name, std::string const& extra)
{
  // Append to var
  if ( const char* var_curr = std::getenv(name); var_curr )
  {
    // ns_log::debug()("ENV: {} -> {}", name, extra + var_curr);
    setenv(name, std::string{extra + var_curr}.c_str(), 1);
  } // if
  else
  {
    ns_log::error()("Variable '{}' is not set"_fmt(name));
  } // else
} // prepend() }}}

// concat() {{{
// Appends 'extra' to an environment variable 'name'
inline void concat(const char* name, std::string const& extra)
{
  // Append to var
  if ( const char* var_curr = std::getenv(name); var_curr )
  {
    setenv(name, std::string{var_curr + extra}.c_str(), 1);
  } // if
  else
  {
    ns_log::error()("Variable '{}' is not set"_fmt(name));
  } // else
} // concat() }}}

// set_mutual_exclusion() {{{
// If first exists, sets first to value and unsets second
// Else sets second to value
inline void set_mutual_exclusion(const char* name1, const char* name2, const char* value)
{
  if ( std::getenv(name1) != nullptr )
  {
    setenv(name1, value, true);
    unsetenv(name2);
    return;
  } // if

  setenv(name2, value, true);
} // set_mutual_exclusion() }}}

// print() {{{
// print an env variable
inline void print(const char* name, std::ostream& os = std::cout)
{
  if ( const char* var = std::getenv(name); var )
  {
    os << var;
  } // if
} // print() }}}

// get_or_throw() {{{
// Get an env variable
template<typename T = const char*>
inline T get_or_throw(const char* name)
{
  const char* value = std::getenv(name);
  ethrow_if(not value, "Variable '{}' is undefined"_fmt(name));
  return value;
} // get_or_throw() }}}

// get_or_else() {{{
// Get an env variable
inline std::string get_or_else(std::string_view name, std::string_view alternative)
{
  const char* value = std::getenv(name.data());
  return_if_else(value != nullptr, value, alternative.data());
} // get_or_else() }}}

// get() {{{
// Get an env variable
inline const char* get(const char* name)
{
  return std::getenv(name);
} // get() }}}

// get_optional() {{{
// get_optional an env variable
template<typename T = std::string_view>
inline std::optional<T> get_optional(std::string_view name)
{
  const char* var = std::getenv(name.data());
  return (var)? std::make_optional(var) : std::nullopt;
} // get_optional() }}}

// exists() {{{
// Checks if variable exists
inline bool exists(const char* var)
{
  return get(var) != nullptr;
} // exists() }}}

// exists() {{{
// Checks if variable exists and equals value
inline bool exists(const char* var, std::string_view target)
{
  const char* value = get(var);
  qreturn_if(not value, false);
  return std::string_view{value} == target;
} // exists() }}}

// expand() {{{
inline std::expected<std::string, std::string> expand(ns_concept::StringRepresentable auto&& var)
{
  std::string expanded = ns_string::to_string(var);

  // Perform word expansion
  wordexp_t data;
  if (int ret = wordexp(expanded.c_str(), &data, 0); ret == 0)
  {
    if (data.we_wordc > 0)
    {
      expanded = data.we_wordv[0];
    } // if
    wordfree(&data);
  } // if
  else
  {
    std::string error;
    switch(ret)
    {
      case WRDE_BADCHAR: error = "WRDE_BADCHAR"; break;
      case WRDE_BADVAL: error = "WRDE_BADVAL"; break;
      case WRDE_CMDSUB: error = "WRDE_CMDSUB"; break;
      case WRDE_NOSPACE: error = "WRDE_NOSPACE"; break;
      case WRDE_SYNTAX: error = "WRDE_SYNTAX"; break;
      default: error = "unknown";
    } // switch
    return std::unexpected(error);
  } // else

  return expanded;
} // expand() }}}

} // namespace ns_env }}}

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/

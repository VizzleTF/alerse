// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/alerse_import/alerse_import_arc_data.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "crypto/aead.h"
#include "crypto/aes_cbc.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <dpapi.h>
#elif BUILDFLAG(IS_MAC)
#include <Security/Security.h>

#include "base/apple/scoped_cftyperef.h"
#endif

namespace alerse_import {

namespace {

// A tag from sql's list; the reads are an import as Firefox's are.
inline constexpr sql::Database::Tag kDatabaseTag{"FirefoxImporter"};

constexpr std::string_view kV10 = "v10";

base::FilePath ArcProfile(const base::FilePath& arc_dir) {
  return arc_dir.Append(FILE_PATH_LITERAL("User Data"))
      .Append(FILE_PATH_LITERAL("Default"));
}

// Opens a copy of `file` in `temp`, as Arc may hold the file open.
bool OpenCopy(const base::FilePath& file,
              const base::ScopedTempDir& temp,
              sql::Database& db) {
  const base::FilePath copy = temp.GetPath().Append(file.BaseName());
  return base::PathExists(file) && base::CopyFile(file, copy) &&
         db.Open(copy);
}

// Chromium's time: microseconds since 1601.
base::Time FromChromiumTime(int64_t value) {
  return base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(value));
}

std::vector<ArcPage> ReadPages(const base::FilePath& profile,
                               const base::ScopedTempDir& temp) {
  std::vector<ArcPage> pages;
  sql::Database db(kDatabaseTag);
  if (!OpenCopy(profile.Append(FILE_PATH_LITERAL("History")), temp, db)) {
    return pages;
  }
  sql::Statement s(db.GetUniqueStatement(
      "SELECT url, title, visit_count, typed_count, last_visit_time FROM urls "
      "WHERE hidden = 0 ORDER BY last_visit_time DESC LIMIT ?"));
  s.BindInt64(0, static_cast<int64_t>(kMaxArcPages));
  while (s.Step()) {
    GURL url(s.ColumnString(0));
    if (!url.is_valid() || !(url.SchemeIsHTTPOrHTTPS() || url.SchemeIsFile())) {
      continue;
    }
    ArcPage page;
    page.url = std::move(url);
    page.title = s.ColumnString16(1);
    page.visit_count = s.ColumnInt(2);
    page.typed_count = s.ColumnInt(3);
    page.last_visit = FromChromiumTime(s.ColumnInt64(4));
    pages.push_back(std::move(page));
  }
  return pages;
}

#if BUILDFLAG(IS_WIN)
// The AES-256 key from Arc's Local State, which DPAPI unwraps for this user.
std::optional<std::string> PasswordKey(const base::FilePath& arc_dir) {
  std::string json;
  if (!base::ReadFileToString(arc_dir.Append(FILE_PATH_LITERAL("User Data"))
                                  .Append(FILE_PATH_LITERAL("Local State")),
                              &json)) {
    return std::nullopt;
  }
  std::optional<base::Value> state =
      base::JSONReader::Read(json, base::JSON_PARSE_RFC);
  const std::string* encoded =
      state && state->is_dict()
          ? state->GetDict().FindStringByDottedPath("os_crypt.encrypted_key")
          : nullptr;
  std::string wrapped;
  constexpr std::string_view kDpapi = "DPAPI";
  if (!encoded || !base::Base64Decode(*encoded, &wrapped) ||
      !wrapped.starts_with(kDpapi)) {
    return std::nullopt;
  }
  wrapped.erase(0, kDpapi.size());
  DATA_BLOB in = {static_cast<DWORD>(wrapped.size()),
                  reinterpret_cast<BYTE*>(wrapped.data())};
  DATA_BLOB out = {};
  if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out)) {
    return std::nullopt;
  }
  std::string key(reinterpret_cast<const char*>(out.pbData), out.cbData);
  LocalFree(out.pbData);
  return key;
}

std::optional<std::string> Decrypt(const std::string& key,
                                   const std::string& blob) {
  if (blob.starts_with(kV10)) {
    constexpr size_t kNonce = 12;
    if (key.size() != 32 || blob.size() < kV10.size() + kNonce) {
      return std::nullopt;
    }
    const auto bytes = base::as_byte_span(blob);
    std::optional<std::vector<uint8_t>> plain = crypto::aead::Open(
        crypto::aead::AES_256_GCM, base::as_byte_span(key),
        bytes.subspan(kV10.size() + kNonce), bytes.subspan(kV10.size(), kNonce),
        {});
    if (!plain) {
      return std::nullopt;
    }
    return std::string(plain->begin(), plain->end());
  }
  // Older Chromium wrapped each password with DPAPI itself.
  DATA_BLOB in = {static_cast<DWORD>(blob.size()),
                  reinterpret_cast<BYTE*>(const_cast<char*>(blob.data()))};
  DATA_BLOB out = {};
  if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out)) {
    return std::nullopt;
  }
  std::string plain(reinterpret_cast<const char*>(out.pbData), out.cbData);
  LocalFree(out.pbData);
  return plain;
}
#elif BUILDFLAG(IS_MAC)
// The AES-128 key derived from the Keychain entry Arc keeps its secret in.
std::optional<std::string> PasswordKey(const base::FilePath&) {
  base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> query(
      CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  CFDictionarySetValue(query.get(), kSecClass, kSecClassGenericPassword);
  CFDictionarySetValue(query.get(), kSecAttrService, CFSTR("Arc Safe Storage"));
  CFDictionarySetValue(query.get(), kSecAttrAccount, CFSTR("Arc"));
  CFDictionarySetValue(query.get(), kSecReturnData, kCFBooleanTrue);
  CFDictionarySetValue(query.get(), kSecMatchLimit, kSecMatchLimitOne);
  CFTypeRef result = nullptr;
  if (SecItemCopyMatching(query.get(), &result) != errSecSuccess || !result) {
    return std::nullopt;
  }
  base::apple::ScopedCFTypeRef<CFDataRef> secret(
      static_cast<CFDataRef>(result));
  std::string key(16, '\0');
  constexpr std::string_view kSalt = "saltysalt";
  if (!PKCS5_PBKDF2_HMAC_SHA1(
          reinterpret_cast<const char*>(CFDataGetBytePtr(secret.get())),
          CFDataGetLength(secret.get()),
          reinterpret_cast<const uint8_t*>(kSalt.data()), kSalt.size(), 1003,
          key.size(), reinterpret_cast<uint8_t*>(key.data()))) {
    return std::nullopt;
  }
  return key;
}

std::optional<std::string> Decrypt(const std::string& key,
                                   const std::string& blob) {
  if (!blob.starts_with(kV10) || key.size() != 16) {
    return std::nullopt;
  }
  std::array<uint8_t, 16> iv;
  iv.fill(' ');
  std::optional<std::vector<uint8_t>> plain = crypto::aes_cbc::Decrypt(
      base::as_byte_span(key), iv,
      base::as_byte_span(blob).subspan(kV10.size()));
  if (!plain) {
    return std::nullopt;
  }
  return std::string(plain->begin(), plain->end());
}
#else
std::optional<std::string> PasswordKey(const base::FilePath&) {
  return std::nullopt;
}
std::optional<std::string> Decrypt(const std::string&, const std::string&) {
  return std::nullopt;
}
#endif

void ReadLogins(const base::FilePath& arc_dir,
                const base::ScopedTempDir& temp,
                ArcData& data) {
  sql::Database db(kDatabaseTag);
  if (!OpenCopy(ArcProfile(arc_dir).Append(FILE_PATH_LITERAL("Login Data")),
                temp, db)) {
    return;
  }
  sql::Statement s(db.GetUniqueStatement(
      "SELECT origin_url, action_url, username_element, username_value, "
      "password_element, password_value, signon_realm, date_created, "
      "blacklisted_by_user, scheme FROM logins"));
  std::optional<std::optional<std::string>> key;
  while (s.Step()) {
    ArcLogin login;
    login.url = GURL(s.ColumnString(0));
    login.action = GURL(s.ColumnString(1));
    login.username_element = s.ColumnString16(2);
    login.username = s.ColumnString16(3);
    login.password_element = s.ColumnString16(4);
    const std::string blob = s.ColumnBlobAsString(5);
    login.signon_realm = s.ColumnString(6);
    login.created = FromChromiumTime(s.ColumnInt64(7));
    login.blocked = s.ColumnBool(8);
    login.scheme = s.ColumnInt(9);
    if (!login.url.is_valid() || login.signon_realm.empty()) {
      continue;
    }
    if (!blob.empty()) {
      // The Keychain asks once, for the first password only.
      if (!key) {
        key = PasswordKey(arc_dir);
      }
      std::optional<std::string> plain =
          *key ? Decrypt(**key, blob) : std::nullopt;
      if (!plain) {
        ++data.locked;
        continue;
      }
      login.password = base::UTF8ToUTF16(*plain);
    } else if (!login.blocked) {
      continue;
    }
    data.logins.push_back(std::move(login));
  }
}

}  // namespace

ArcLogin::ArcLogin() = default;
ArcLogin::ArcLogin(ArcLogin&&) = default;
ArcLogin& ArcLogin::operator=(ArcLogin&&) = default;
ArcLogin::~ArcLogin() = default;

ArcData::ArcData() = default;
ArcData::ArcData(ArcData&&) = default;
ArcData& ArcData::operator=(ArcData&&) = default;
ArcData::~ArcData() = default;

ArcData ReadArcData(const base::FilePath& arc_dir) {
  ArcData data;
  base::ScopedTempDir temp;
  if (arc_dir.empty() || !temp.CreateUniqueTempDir()) {
    return data;
  }
  data.pages = ReadPages(ArcProfile(arc_dir), temp);
  ReadLogins(arc_dir, temp, data);
  return data;
}

}  // namespace alerse_import

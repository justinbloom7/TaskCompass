#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Sourced by operator scripts. Fixed-pair, read-only credential entry only.
# Caller supplies clientPath/platform/CREDENTIAL_DATABASE/fail and clears cache on exit.
set +x
declare -A passwords=()
readonly MAX_PASSWORD_ATTEMPTS=3
redact() {
   local line=''
   while IFS= read -r line || [[ -n "$line" ]]; do
      local secret=''
      for secret in "${accountPassword:-}" "${passwords[@]}"; do
         [[ -z "$secret" ]] || line=${line//"$secret"/<redacted>}
      done
      if [[ "$line" == 'WARNING: option --ssl-verify-server-cert is disabled, because of an insecure passwordless login.' ]]; then
         printf 'MariaDB password-option notice; TLS settings unchanged.\n' >&2
      else
         printf '%s\n' "$line" >&2
      fi
   done
}
# Retry only this read-only login/identity probe and only native 1045/28000.
# Once verified, cache in shell memory; never retry any subsequent operation.
authenticate() {
   local user=$1 attempt authOutput='' authStatus=0 line='' identity='' errors=0 unexpected=false
   case "$CREDENTIAL_DATABASE:$user" in
      taskcompass_dev:taskcompass_migrate|taskcompass_dev:taskcompass_app|taskcompass_dev:taskcompass_dev|taskcompass_phase12_test:taskcompass_phase12_test) ;;
      *) fail 'Unsupported credential database/account pair.' ;;
   esac
   [[ ! -v "passwords[$user]" ]] || return 0
   local expectedIdentity="$CREDENTIAL_DATABASE"$'\t'"$user@127.0.0.1"
   for ((attempt=1; attempt<=MAX_PASSWORD_ATTEMPTS; attempt+=1)); do
      IFS= read -r -s -p "[$platform] Password for $user (attempt $attempt/$MAX_PASSWORD_ATTEMPTS; Ctrl-D cancels): " accountPassword || fail 'Password entry cancelled.'
      printf '\n' >&2
      if [[ -z "$accountPassword" ]]; then
         printf 'Empty password; enter it again.\n' >&2
         continue
      fi
      if authOutput=$(printf '%s\n' 'SELECT DATABASE(),CURRENT_USER();' |
         LC_ALL=C MYSQL_PWD="$accountPassword" "$clientPath" --no-defaults --protocol=TCP \
            --host=127.0.0.1 --port=3306 --user="$user" --connect-timeout=5 \
            --default-character-set=utf8mb4 --batch --raw --skip-column-names \
            --database="$CREDENTIAL_DATABASE" 2>&1); then authStatus=0; else authStatus=$?; fi
      identity=''; errors=0; unexpected=false
      while IFS= read -r line || [[ -n "$line" ]]; do
         line=${line%$'\r'}
         if [[ "$line" == "$expectedIdentity" && -z "$identity" ]]; then identity=$line
         elif [[ "$line" == 'WARNING: option --ssl-verify-server-cert is disabled, because of an insecure passwordless login.' ]]; then
            printf 'MariaDB password-option notice; TLS settings unchanged.\n' >&2
         elif [[ "$line" =~ ^ERROR\ 1045\ \(28000\):\ Access\ denied\ for\ user\  ]]; then errors=$((errors+1))
         else unexpected=true; fi
      done <<< "$authOutput"
      if [[ "$authStatus" == 0 && "$identity" == "$expectedIdentity" && "$errors" == 0 ]] && ! "$unexpected"; then
         passwords[$user]=$accountPassword
         unset accountPassword authOutput
         printf 'Credential checked: %s (memory only for this run).\n' "$user"
         return 0
      fi
      # No raw error text is printed: errors may contain connection/secret data.
      if [[ "$authStatus" != 0 && "$errors" == 1 && -z "$identity" ]] && ! "$unexpected"; then
         unset accountPassword authOutput
         printf 'Authentication rejected for %s; no backup/export was attempted by this probe.\n' "$user" >&2
      else
         unset accountPassword authOutput
         fail "Login/identity check for $user failed for a reason other than a confirmed password rejection; not retried."
      fi
   done
   fail "Password entry for $user exhausted $MAX_PASSWORD_ATTEMPTS attempts."
}

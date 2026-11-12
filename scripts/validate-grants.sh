#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Sourced by dev-database.sh; conservative SHOW GRANTS validation, never SQL mutation.

# MariaDB database grant scopes use LIKE-style wildcards, not shell globs.
scopeMatchesDevelopmentDatabase() {
   local pattern=$1
   local regex='^'
   local character=''
   local escaped=false
   local index=0
   for ((index = 0; index < ${#pattern}; ++index)); do
      character=${pattern:index:1}
      if ! "$escaped"; then
         case "$character" in
            '\') escaped=true; continue ;;
            '%') regex+='.*'; continue ;;
            '_') regex+='.'; continue ;;
         esac
      fi
      case "$character" in
         '.'|'['|']'|'('|')'|'{'|'}'|'*'|'+'|'?'|'^'|'$'|'|'|'\') regex+="\\$character" ;;
         *) regex+="${character,,}" ;;
      esac
      escaped=false
   done
   if "$escaped"; then
      regex+='\\'
   fi
   regex+='$'
   # Conservative case-insensitive comparison covers Windows database-name behavior.
   [[ "${DATABASE_NAME,,}" =~ $regex ]]
}

# Decode a SHOW GRANTS identifier, not a general SQL expression.
decodeGrantIdentifier() {
   local token=$1
   local quote=${token:0:1}
   decodedIdentifier=''
   if [[ "$quote" == '`' || "$quote" == "'" ]]; then
      [[ ${#token} -ge 2 && "${token: -1}" == "$quote" ]] || fail 'Unrecognized grant identifier; administrator review required.'
      token=${token:1:${#token}-2}
      local index=0
      local character=''
      for ((index = 0; index < ${#token}; ++index)); do
         character=${token:index:1}
         if [[ "$character" == "$quote" ]]; then
            ((index += 1))
            [[ "${token:index:1}" == "$quote" ]] || fail 'Ambiguous grant identifier; administrator review required.'
         fi
         decodedIdentifier+="$character"
      done
   else
      [[ "$token" =~ ^[[:alnum:]_\$]+$ ]] || fail 'Unsupported grant identifier; administrator review required.'
      decodedIdentifier=$token
   fi
   [[ -n "$decodedIdentifier" && "$decodedIdentifier" != *$'\n'* && "$decodedIdentifier" != *$'\r'* && "$decodedIdentifier" != *$'\t'* ]] || fail 'Unsupported control characters in grant identifier.'
}

queueGrantRole() {
   local role=$1
   [[ -n "$role" && "$role" != *$'\n'* && "$role" != *$'\r'* && "$role" != *$'\t'* ]] || fail 'Unsupported role name; administrator review required.'
   [[ "$role" != *[![:print:]]* ]] || fail 'Unsupported control characters in role name.'
   if [[ ${seenGrantRoles["$role"]+present} != present ]]; then
      seenGrantRoles["$role"]=1
      grantRoleQueue+=("$role")
   fi
}

queueGrantRoleList() {
   local text=$1
   local token=''
   local quote=''
   local character=''
   local index=0
   for ((index = 0; index < ${#text}; ++index)); do
      character=${text:index:1}
      if [[ -n "$quote" ]]; then
         token+="$character"
         if [[ "$character" == "$quote" ]]; then
            if [[ "${text:index+1:1}" == "$quote" ]]; then
               ((index += 1))
               token+="$quote"
            else
               quote=''
            fi
         fi
      elif [[ "$character" == "'" || "$character" == '`' ]]; then
         quote=$character
         token+="$character"
      elif [[ "$character" == ',' ]]; then
         token=${token#"${token%%[![:space:]]*}"}
         token=${token%"${token##*[![:space:]]}"}
         decodeGrantIdentifier "$token"
         queueGrantRole "$decodedIdentifier"
         token=''
      else
         token+="$character"
      fi
   done
   [[ -z "$quote" ]] || fail 'Unclosed role identifier; Qt probe not run.'
   token=${token#"${token%%[![:space:]]*}"}
   token=${token%"${token##*[![:space:]]}"}
   decodeGrantIdentifier "$token"
   queueGrantRole "$decodedIdentifier"
}

parseGrantBatch() {
   local origin=$1
   local grants=$2
   local grant=''
   local privileges=''
   local scope=''
   local recipient=''
   local source=''
   local databaseToken=''
   local databasePattern=''
   local scopedGrantRegex='^GRANT (.+) ON (.+) TO (.+)$'
   local scopeRegex='^(\*|`([^`]|``)+`|[[:alnum:]_$%\\]+)\.(\*|`([^`]|``)+`|[[:alnum:]_$]+)$'
   local publicRegex="^(PUBLIC|public|\`PUBLIC\`|\`public\`|'PUBLIC'|'public')([[:space:]]|$)"
   local roleGrantRegex='^GRANT (.+) TO (.+)$'
   local defaultRoleRegex='^SET DEFAULT ROLE .+ FOR .+$'
   while IFS= read -r grant; do
      [[ -n "$grant" ]] || continue
      if [[ "$grant" =~ $scopedGrantRegex ]]; then
         privileges=${BASH_REMATCH[1]}
         scope=${BASH_REMATCH[2]}
         recipient=${BASH_REMATCH[3]}
         source=$origin
         if [[ "$recipient" =~ $publicRegex ]]; then
            source=PUBLIC
            ((publicGrantCount += 1))
         fi
         [[ "$scope" =~ $scopeRegex ]] || fail 'Unsupported routine/proxy/column grant syntax; cannot establish read-only access. Qt probe not run.'
         databaseToken=${BASH_REMATCH[1]}
         if [[ "$databaseToken" == '*' ]]; then
            databasePattern='%'
         else
            decodeGrantIdentifier "$databaseToken"
            databasePattern=$decodedIdentifier
         fi
         if ! scopeMatchesDevelopmentDatabase "$databasePattern"; then
            ((unrelatedGrantCount += 1))
            continue
         fi
         # On the development schema (or global scope), only SELECT/USAGE is safe.
         # Reject unrecognized future privileges and grant/admin options, too.
         [[ "${recipient^^}" != *'WITH GRANT OPTION'* && "${recipient^^}" != *'WITH ADMIN OPTION'* ]] || fail "Unexpected grant authority covering TaskCompass from $source; Qt probe not run."
         local privilege=''
         local -a privilegeList=()
         IFS=',' read -r -a privilegeList <<< "$privileges"
         for privilege in "${privilegeList[@]}"; do
            privilege=${privilege//[[:space:]]/}
            case "${privilege^^}" in
               SELECT|USAGE) ;;
               *) fail "Unexpected effective write/DDL/administrative privilege covering TaskCompass from $source; Qt probe not run." ;;
            esac
         done
      elif [[ "$grant" =~ $roleGrantRegex ]]; then
         local roleTokens=${BASH_REMATCH[1]}
         recipient=${BASH_REMATCH[2]}
         [[ "${recipient^^}" != *'WITH ADMIN OPTION'* ]] || fail 'Role ADMIN OPTION requires administrator review; Qt probe not run.'
         queueGrantRoleList "$roleTokens"
      elif [[ "$grant" =~ $defaultRoleRegex ]]; then
         # Applicable roles below are audited even when not currently enabled.
         continue
      else
         fail 'Unrecognized SHOW GRANTS syntax; cannot validate privileges. Grant/authentication text is not displayed; Qt probe not run.'
      fi
   done <<< "$grants"
}

validateGrantSources() {
   local accountGrants=$1
   local applicableRoles=''
   local enabledRoles=''
   local role=''
   local isGrantable=''
   local roleGrants=''
   local quotedRole=''
   local decodedIdentifier=''
   local publicGrantCount=0
   local unrelatedGrantCount=0
   local -a grantRoleQueue=()
   local -A seenGrantRoles=()
   parseGrantBatch 'direct account' "$accountGrants"
   applicableRoles=$(runClient dev 'SELECT ROLE_NAME,IS_GRANTABLE FROM information_schema.APPLICABLE_ROLES;')
   while IFS=$'\t' read -r role isGrantable; do
      [[ -n "$role" ]] || continue
      [[ "$isGrantable" == NO ]] || fail 'Grantable or unrecognized role authorization requires administrator review; Qt probe not run.'
      queueGrantRole "$role"
   done <<< "$applicableRoles"
   enabledRoles=$(runClient dev "SELECT ROLE_NAME FROM information_schema.ENABLED_ROLES WHERE ROLE_NAME IS NOT NULL;")
   while IFS= read -r role; do
      [[ -z "$role" ]] || queueGrantRole "$role"
   done <<< "$enabledRoles"
   local index=0
   for ((index = 0; index < ${#grantRoleQueue[@]}; ++index)); do
      role=${grantRoleQueue[index]}
      quotedRole=${role//\`/\`\`}
      roleGrants=$(runClient dev "SET SESSION sql_mode='NO_BACKSLASH_ESCAPES'; SHOW GRANTS FOR \`$quotedRole\`;")
      parseGrantBatch "role $role" "$roleGrants"
   done
   printf 'Direct account: exact development-database SELECT-only metadata validated.\n'
   printf 'Role sources audited (including nested/activatable): %s; PUBLIC grant rows: %s.\n' "${#grantRoleQueue[@]}" "$publicGrantCount"
   printf 'Nonmatching schema grants (including PUBLIC test/test_%%) ignored: %s.\n' "$unrelatedGrantCount"
   printf 'No declared write/DDL/grant authority covering TaskCompass found. No CREATE/DROP test attempted.\n'
}

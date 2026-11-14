<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Windows MariaDB administrator password recovery

Prepared for the owner-reported instance on 2026-11-14. **Proposed, not executed.**
Obtain explicit approval before stopping this service or resetting its root account.
This is administrator recovery, not a TaskCompass migration or database reset.

Confirmed by owner service output:

- Service: MariaDB, currently Running.
- Service identity: NT SERVICE\MariaDB.
- Server: C:\Program Files\MariaDB 13.0\bin\mysqld.exe.
- Configuration: C:\Program Files\MariaDB 13.0\data\my.ini.

Scope: brief service downtime, temporary local named-pipe recovery with TCP disabled,
password change only for the existing root@localhost password-authenticating
account, then normal service restart and TCP authentication check. No application
accounts, grants, databases, my.ini or service registration are changed. If other
applications depend on this service, arrange their downtime before proceeding.
Do not query or modify their data. Close TaskCompass and other SQL clients first.

MariaDB documents recovery by temporarily starting with skip-grant-tables and
running FLUSH PRIVILEGES before changing the password; see
[authentication recovery](https://mariadb.com/docs/server/security/user-account-management/authentication-from-mariadb-10-4).
The [Windows named-pipe connection](https://mariadb.com/docs/server/mariadb-quickstart-guides/mariadb-connecting-guide)
avoids exposing a passwordless recovery server over TCP. This exact Windows 13.0
procedure has not been tested by the agent. Stop on errors rather than changing
paths, permissions, authentication plugins or security policy to force it through.

## 1. Read-only configuration check, before downtime

Use **PowerShell**, not UCRT64 or a SQL prompt. Check nonsecret instance settings:

```powershell
Select-String -LiteralPath 'C:\Program Files\MariaDB 13.0\data\my.ini' `
   -Pattern '^\s*\[', '^\s*datadir\s*=', '^\s*port\s*=', '^\s*!include', '^\s*plugin[-_]load'
```

Record the intended data directory and port. Do not paste the entire configuration
or any credentials. If this reveals additional included files/service-specific
groups affecting startup, multiple instances, or audit plugins, stop for review
before using this default-instance procedure. The service's configuration-file
location alone does not prove the data directory has the same path.

## 2. Approved service stop and local recovery startup

After explicit approval, open **PowerShell as Administrator**, called window A.
Check the intended service still matches the owner-reported configuration. Then:

```powershell
Stop-Service -Name MariaDB -ErrorAction Stop
Get-Service -Name MariaDB
```

Require Stopped. Never run two servers against the same data directory. Do not
force-kill the server or initialize a new instance if it does not stop cleanly.
Start a foreground recovery process using the existing configuration:

```powershell
$recoveryOptions = @(
   '--defaults-file=C:\Program Files\MariaDB 13.0\data\my.ini'
   '--console'
   '--skip-grant-tables'
   '--skip-networking'
   '--enable-named-pipe'
   '--socket=TaskCompassRootRecovery'
   '--general-log=0'
   '--slow-query-log=0'
   '--skip-log-bin'
)
& 'C:\Program Files\MariaDB 13.0\bin\mysqld.exe' @recoveryOptions
```

Leave window A open. Require ready-for-connections output. These are temporary
command-line flags, not my.ini edits. TCP is disabled, but local recovery bypasses
authentication temporarily; use only a trusted machine with no other SQL users
active. Query/audit output must not record the new password. If startup fails,
preserve the error and stop for review; do not change directory ownership or delete
InnoDB files. If the process has fully exited, the unchanged normal service can be
restarted to restore its prior behavior (with the still-unknown old credential).

## 3. Inspect the intended root account using the local pipe

Open another **PowerShell as Administrator**, window B. Disable SQL history for
this session and remove any inherited command-line client password:

```powershell
$env:MYSQL_HISTFILE = 'NUL'
$env:MARIADB_HISTFILE = 'NUL'
Remove-Item Env:MYSQL_PWD -ErrorAction SilentlyContinue
& 'C:\Program Files\MariaDB 13.0\bin\mariadb.exe' '--no-defaults' '--protocol=PIPE' '--socket=TaskCompassRootRecovery' '--user=root'
```

No password is expected for this temporary local recovery connection. In its SQL
prompt, run only these read-only identity/plugin checks first:

```sql
SELECT VERSION(),@@datadir,@@port,@@skip_networking;
SELECT User,Host,plugin FROM mysql.user WHERE User='root';
SELECT PLUGIN_NAME,PLUGIN_STATUS FROM information_schema.PLUGINS
   WHERE PLUGIN_TYPE='AUDIT' AND PLUGIN_STATUS='ACTIVE';
```

Require intended MariaDB 13.0 instance/data directory, skip_networking=1, an existing
root@localhost with password-capable authentication, and no active audit plugin
that could record the secret. Other root-host rows do not authorize changing them.
If root@localhost is absent, auth is GSSAPI/another unexpected plugin, or an active
audit plugin exists, stop for a revised procedure. Do not query authentication
hashes, print mysql.global_priv contents or modify system tables directly.

## 4. Reset only the approved existing root account

Generate and save a new, distinct password in a password manager first. A random
32+ character letters/digits password avoids SQL escaping complications. Do not
record/screenshot this part of the session or save password-bearing SQL anywhere.
In the same history-disabled SQL prompt, replace the placeholder privately:

```sql
FLUSH PRIVILEGES;
SET PASSWORD FOR 'root'@'localhost' = PASSWORD('<new-strong-root-password>');
```

Require success for both statements. SET PASSWORD preserves the account's grants
and supported authentication method; unexpected-plugin errors need review, not an
ALTER USER rewrite or drop/recreate. See
[SET PASSWORD](https://mariadb.com/docs/server/reference/sql-statements/account-management-sql-statements/set-password).
There is no known-old-password rollback: after success, the new password is the
credential to retain. No remote root accounts or TaskCompass accounts are changed.

## 5. Shut down recovery cleanly and restore the normal service

In the connected SQL prompt:

```sql
SHUTDOWN;
```

The client may disconnect as the server shuts down. Confirm window A's foreground
server has exited completely before starting the service. If graceful shutdown
fails, stop for review; do not start a competing server or use taskkill /F.
In window B's PowerShell (leave/close the SQL client if necessary):

```powershell
Start-Service -Name MariaDB -ErrorAction Stop
Get-Service -Name MariaDB
& 'C:\Program Files\MariaDB 13.0\bin\mariadb.exe' '--no-defaults' '--protocol=TCP' '--host=127.0.0.1' '--port=3306' '--user=root' '--password'
```

Enter the new password at the hidden prompt. Then:

```sql
SELECT VERSION(),CURRENT_USER(),@@datadir,@@skip_networking,@@skip_grant_tables;
quit
```

Require the intended root account/data directory, skip_networking=0 and
skip_grant_tables=0 under the normal configuration. If TCP login still fails,
report the error; do not reset additional root-host accounts automatically.
Close these PowerShell windows afterward. No recovery flags were saved in the
service/configuration and no password file was created.

## 6. Resume TaskCompass validation

Return to UCRT64 and run scripts/phase11/02-inspect.sh using the new administrator
password. Record recovery results in docs/milestones/Phase1.1.md without secrets.
Do not mark Windows live tests passed based solely on administrator login recovery.
Any failed recovery, permissions repair, account/plugin change or broader service
modification requires its own reviewed action and approval.

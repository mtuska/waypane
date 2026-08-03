# MCP integration

Waypane ships a local stdio MCP server. Stdio limits exposure to the MCP client
that launches the process and avoids an unauthenticated localhost service.

## Tools

- `waypane_list_connections`: list redacted connection summaries.
- `waypane_get_connection`: inspect one redacted profile.
- `waypane_search_connections`: search names, hosts, users, folders, and tags.
- `waypane_build_ssh_argv`: resolve the exact SSH argument vector without secrets.
- `waypane_upsert_connection`: create or update non-secret profile data.
- `waypane_delete_connection`: delete a profile, never its external key files.
- `waypane_runtime_status`: inspect the running desktop workspace.
- `waypane_connect_ssh`: open SSH only for a saved profile.
- `waypane_browse_sftp`: open SFTP only for a saved profile.
- `waypane_open_tunnels`: start configured forwards in a visible terminal tab;
  closing the tab stops them.
- `waypane_open_local_terminal`: add a local terminal tab.
- `waypane_split_terminal`: add a right or downward local terminal pane.
- `waypane_edit_remote_file`: visibly open a remote UTF-8 file; the desktop user
  retains control of saving it.

The server accepts `WAYPANE_DATA_DIR`, `WAYPANE_CONTROL_SOCKET`, and standard XDG
paths for isolated testing. It emits protocol messages only on stdout;
diagnostics go to stderr. The live socket and its parent directory are mode 0600
and 0700 respectively.

Example client configuration from a repository checkout:

```json
{
  "mcpServers": {
    "waypane": {
      "command": "/absolute/path/to/konsole-manager/tools/waypane-mcp"
    }
  }
}
```

## Safety model

MCP never returns passwords, private-key contents, passphrases, or keyring values.
Profile mutations are explicit tools. Remote editor automation opens the visible
editor but does not save. Future headless remote command, upload, download, and
tunnel tools will be separated by capability and default to desktop approval.
Headless authorization will require an allowlist policy with target, operation,
path, and time constraints.

For remote HTTP transport, Waypane will follow MCP OAuth requirements rather
than exposing the local server directly.

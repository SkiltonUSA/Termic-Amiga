static Object *access_window, *access_enabled;
enum
{
    AC_OPEN = 2100,
    AC_SAVE,
    AC_USE,
    AC_CANCEL
};
static Object *build_access_window(void)
{
    Object
        *o = WindowObject,
        MUIA_Window_Title, "Termic: Agent access", MUIA_Window_ID, MAKE_ID('T', 'M', 'C', 'P'),
        MUIA_Window_Width, 900, MUIA_Window_Height, 480, WindowContents, VGroup,
        InnerSpacing(12, 12), MUIA_Group_Spacing, 10, Child,
        label("\33bAgent access: MCP & CLI"), Child,
        label(
            "Let MCP clients create Amiga tasks, read results and keep scratchpads.\nTermicMCP "
            "is a native 68k program; existing Amiga bridge access connects other computers."),
        Child, HGroup, Child, label("Enable Amiga MCP tools"), Child,
        access_enabled = cycle(yesno), End, Child, VGroup, GroupFrame, InnerSpacing(8, 8),
        Child, label("\33bNative connection"), Child,
        label("Command: Termic:TermicMCP --stdio\nTransport: MCP over standard input/output "
              "(one JSON-RPC message per line)\nCLI files: TermicMCP --request request.json "
              "--reply reply.json"),
        End, Child, VGroup, GroupFrame, InnerSpacing(8, 8), Child,
        label("\33bConnect from another computer"), Child,
        label("Use scripts/amiga/mcp_bridge.py with your existing DevBench bridge.\nRegister "
              "that stdio command in the MCP client's settings.\nThe bridge controls access to "
              "the Amiga; Termic opens no new network listener.\nDesktop Termic peer "
              "discovery, pairing codes and automatic CLI registration\nare not implemented by "
              "this native transport."),
        End, Child, VGroup, GroupFrame, InnerSpacing(8, 8), Child,
        label("\33bTools and access"), Child,
        label("Projects, tasks, prompts, status, results, builds, source files and "
              "scratchpads.\nMCP writes use isolated tasks it created, and refuse busy or "
              "editor-selected tasks.\nAPI keys, profile environment values and arbitrary "
              "shell commands are not exposed.\nDisable access to reject further tool calls; "
              "already-running workers continue."),
        End, Child, HVSpace, Child, HGroup, Child, button("Save", AC_SAVE), Child,
        button("Use", AC_USE), Child, button("Cancel", AC_CANCEL), End, End, End;
    if (o)
    {
        DoMethod(app, OM_ADDMEMBER, o);
        notify(o, MUIA_Window_CloseRequest, TRUE, AC_CANCEL);
    }
    return o;
}
static void access_event(int id)
{
    char *s;
    int ok = 1;
    if (id == AC_OPEN)
    {
        s = read_preference("mcp-enabled", 8);
        set(access_enabled, MUIA_Cycle_Active, s && !strcmp(s, "1"));
        free(s);
        set(access_window, MUIA_Window_Open, TRUE);
        return;
    }
    if (id == AC_SAVE || id == AC_USE)
    {
        s = value(access_enabled, MUIA_Cycle_Active) ? "1" : "0";
        mkdir_safe("ENV:Termic");
        if (id == AC_SAVE && !write_preference("Termic:Config", "mcp-enabled", s, 8))
            ok = 0;
        if (!write_preference("ENV:Termic", "mcp-enabled", s, 8))
            ok = 0;
        if (!ok)
        {
            message("Cannot save MCP access setting. Check free space/protection.");
            return;
        }
        message(*s == '1' ? "Amiga MCP tools enabled." : "Amiga MCP tools disabled.");
    }
    set(access_window, MUIA_Window_Open, FALSE);
}

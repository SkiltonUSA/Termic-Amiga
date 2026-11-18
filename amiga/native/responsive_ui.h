enum { UI_LAYOUT=6101,UI_VIEW,UI_PAGE };
static Object *workspace_body,*workspace_row,*workspace_view,*workspace_left,*workspace_files;
static Object *project_pane,*task_pane,*file_pane,*cli_pane,*workspace_center,*task_actions,*project_actions;
static Object *workspace_balance_a,*workspace_balance_b,*files_balance;
static Object *workspace_tabs,*workspace_brand,*workspace_extra,*workspace_small_actions;
static Object *agent_compose,*profile_actions,*profile_buttons,*activity_caption,*instruction_caption;
static int layout_compact=-1,layout_view=-1;
static const char *workspace_views[]={"Agent","Editor","Build log","Changes","Settings",
    "Projects","Tasks","Files","CLI","Processors",NULL};
static struct MUI_CustomClass *workspace_class;
struct WorkspaceData {struct MUI_EventHandlerNode events;};
static ULONG workspace_dispatch(__reg("a0") struct IClass *cl,__reg("a2") Object *obj,__reg("a1") Msg msg)
{
    struct WorkspaceData *data=obj?INST_DATA(cl,obj):NULL;
    switch(msg->MethodID) {
    case MUIM_Setup:
        if(!DoSuperMethodA(cl,obj,msg))return FALSE;
        memset(data,0,sizeof(*data));data->events.ehn_Object=obj;data->events.ehn_Class=cl;
        data->events.ehn_Events=IDCMP_NEWSIZE|IDCMP_RAWKEY;
        DoMethod(_win(obj),MUIM_Window_AddEventHandler,&data->events);return TRUE;
    case MUIM_Cleanup:DoMethod(_win(obj),MUIM_Window_RemEventHandler,&data->events);break;
    case MUIM_HandleEvent:
        {
            struct IntuiMessage *im=((struct MUIP_HandleEvent *)msg)->imsg;
            if(im && im->Class==IDCMP_NEWSIZE)DoMethod(app,MUIM_Application_ReturnID,UI_LAYOUT);
            else if(im && termic_key_event(im))return MUI_EventHandlerRC_Eat;
            return 0;
        }
    }
    return DoSuperMethodA(cl,obj,msg);
}
static void workspace_layout(void)
{
    struct Window *w=(struct Window *)value(win,MUIA_Window_Window);
    int view,compact;
    if(!workspace_body || !w)return;
    view=value(workspace_view,MUIA_Cycle_Active);
    compact=w->Width<800 || w->Height<500;
    if(view==layout_view && compact==layout_compact)return;
    /* Keep the same widgets, buffers, cursor and selection when changing
     * layouts. Nothing is rebuilt or saved as a consequence of a resize. */
    DoMethod(workspace_body,MUIM_Group_InitChange);
    set(workspace_view,MUIA_ShowMe,compact);
    set(workspace_brand,MUIA_ShowMe,!compact);
    set(workspace_extra,MUIA_ShowMe,!compact);
    set(workspace_small_actions,MUIA_ShowMe,compact);
    set(activity_caption,MUIA_ShowMe,!compact);
    set(instruction_caption,MUIA_ShowMe,!compact);
    set(workspace_balance_a,MUIA_ShowMe,!compact);
    set(workspace_balance_b,MUIA_ShowMe,!compact);
    set(files_balance,MUIA_ShowMe,!compact);
    set(workspace_left,MUIA_ShowMe,!compact || view==5 || view==6);
    set(project_pane,MUIA_ShowMe,!compact || view==5);
    set(task_pane,MUIA_ShowMe,!compact || view==6);
    set(workspace_files,MUIA_ShowMe,!compact || view==7 || view==8);
    set(file_pane,MUIA_ShowMe,!compact || view==7);
    set(cli_pane,MUIA_ShowMe,!compact || view==8);
    set(workspace_center,MUIA_ShowMe,!compact || view<5 || view==9);
    termic_monitor_visible(view==9);
    if(view<5 || view==9)SetAttrs(pages,MUIA_NoNotify,TRUE,MUIA_Group_ActivePage,view==9?5:view,TAG_DONE);
    DoMethod(workspace_body,MUIM_Group_ExitChange);
    layout_view=view;layout_compact=compact;
}
static void workspace_select(int view)
{
    if(!workspace_view)return;
    SetAttrs(workspace_view,MUIA_NoNotify,TRUE,MUIA_Cycle_Active,view,TAG_DONE);
    workspace_layout();
}
static int workspace_event(ULONG id)
{
    if(id==UI_PAGE){int page=value(pages,MUIA_Group_ActivePage);workspace_select(page==5?9:page);return 1;}
    if(id==UI_VIEW || id==UI_LAYOUT){workspace_layout();return 1;}
    return 0;
}

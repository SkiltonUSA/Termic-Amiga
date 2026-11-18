/* Embedded processor page. The workspace owns its widgets and window.
 * Timer/sampler cleanup must never dispose the parent window. */
#define TM_EVENT 5801
static Object *pausebutton,*monitor_page;
static int page_visible;
void termic_monitor_close(void)
{
 sampling_stop();closetimer();pending.phase=0;
 win=NULL;list=detail=summary=status=servlist=pausebutton=arminfo=zzbutton=NULL;app=NULL;
 zzvisible=FALSE;monitor_page=NULL;page_visible=0;
 if(bodyfont)CloseFont(bodyfont);if(codefont)CloseFont(codefont);
 bodyfont=codefont=NULL;displayed_rows=displayed_services=rowcount=0;
 paused=reorder_only=FALSE;
}
ULONG termic_monitor_signals(void)
{
 return timerpending && timerport ? 1UL<<timerport->mp_SigBit : 0;
}
void termic_monitor_poll(void)
{
 int p;
 if(!win || !timerpending || !CheckIO((struct IORequest *)timerio))return;
 WaitIO((struct IORequest *)timerio);timerpending=FALSE;++ticks;
 /* Pausing the display must not suspend a confirmed service restart. */
 p=pollrestart();
 if(p)statusmsg(p>0 ? "Service port returned after restart." : "Restart failed or timed out. No automatic retry.");
 if(page_visible && !paused) {
  if(zzvisible && !synthetic){armreport();SetAttrs(arminfo,MUIA_Text_Contents,armtext,TAG_DONE);}
  refresh();details();
 }
 arm_timer();
}
int termic_monitor_event(ULONG id)
{
 if(id<TM_EVENT || id>TM_EVENT+10)return 0;
 if(!win)return 1; /* queued notifications from a just-closed window */
 switch(id-TM_EVENT) {
 case 0:
  if(paused)statusmsg("Paused. Resume to collect a fresh sample.");
  else {refresh();details();}
  break;
 case 1:control(FALSE);break;
 case 2:control(TRUE);break;
 case 3:
  paused=!paused;
  if(paused)sampling_stop();else if(page_visible && !synthetic)sampling_start();
  SetAttrs(pausebutton,MUIA_Text_Contents,paused ? "Resume" : "Pause",TAG_DONE);
  statusmsg(paused ? "Display paused. Pending service restart continues." : "Sampling every second. CPU is an estimate; memory is partial.");
  break;
 case 4:break; /* Parent navigates back to Agent. */
 case 5:details();break;
 case 10:toggle_zz9000();break;
 case 6:case 7:case 8:case 9:
  sort_order=(int)(id-TM_EVENT-6);reorder_only=TRUE;refresh();details();reorder_only=FALSE;
  statusmsg(sort_order==1 ? "Sorted by sampled CPU, highest first." : sort_order==2 ?
   "Sorted by partial tracked memory, highest first." : sort_order==3 ?
   "Sorted by executable/task name." : "Sorted by task address for stable rows.");break;
 }
 return 1;
}
APTR termic_monitor_page(APTR application,int fixture_mode)
{
 Object *root=NULL,*bar,*lv,*slv,*b[5],*sortbuttons[4];int i;
 static char *labels[]={"Refresh","Stop selected","Restart service","Pause","Agent"};
 static char *sortlabels[]={"Address","CPU","Memory","Name"};
 if(monitor_page)return monitor_page;
 app=(Object *)application;synthetic=fixture_mode;sort_order=0;ticks=0;
 totalhits=idlehits=losthits=0;snapshot_clipped=FALSE;
 if(!timerstart())goto fail;
 if(synthetic) {
  strcpy(armtext,"SYNTHETIC ZZ9000: graphics and shared RAM boards\nARM CPU, memory, priority and dependencies: unavailable.\nNo ARM workload is inferred from an absent owner port.");
  nservices=1;badservices=0;memset(services,0,sizeof(services));
  strcpy(services[0].name,"Example service");strcpy(services[0].port,"EXAMPLE.SERVICE");
 } else {loadservices();}
 root=MUI_NewObject(MUIC_Group,MUIA_Font,bodyfont ? (ULONG)bodyfont : MUIV_Font_Normal,TAG_DONE);
 if(!root)goto fail;
 summary=textobject("");if(!child(root,summary))goto fail;
 bar=MUI_NewObject(MUIC_Group,MUIA_Group_Horiz,TRUE,TAG_DONE);if(!child(root,bar))goto fail;
 if(!child(bar,textobject("Sort processes:")))goto fail;
 for(i=0;i<4;++i){sortbuttons[i]=button(sortlabels[i]);if(!child(bar,sortbuttons[i]))goto fail;}
 list=process_list();
 if(!list)goto fail;
 lv=MUI_NewObject(MUIC_Listview,MUIA_Listview_List,list,TAG_DONE);
 if(!lv){MUI_DisposeObject(list);list=NULL;goto fail;}child(root,lv);
 detail=textobject("");if(!child(root,detail))goto fail;
 arminfo=zz9000_info();if(!child(root,arminfo))goto fail;
 if(!child(root,textobject("Services: opt-in definitions from services.txt beside TermicNative. Select to restart.")))goto fail;
 servlist=MUI_NewObject(MUIC_List,MUIA_Font,codefont ? (ULONG)codefont : MUIV_Font_Fixed,TAG_DONE);
 if(!servlist)goto fail;
 slv=MUI_NewObject(MUIC_Listview,MUIA_Listview_List,servlist,MUIA_Weight,15,TAG_DONE);
 if(!slv){MUI_DisposeObject(servlist);servlist=NULL;goto fail;}child(root,slv);
 if(!child(root,textobject("* Partial allocation, not RSS. OS3 uptime, output counters and automatic dependencies: unavailable.")))goto fail;
 status=textobject("Sampling every second. CPU is an estimate; memory is partial.");if(!child(root,status))goto fail;
 bar=MUI_NewObject(MUIC_Group,MUIA_Group_Horiz,TRUE,TAG_DONE);if(!child(root,bar))goto fail;
 for(i=0;i<5;++i){b[i]=button(labels[i]);if(!child(bar,b[i]))goto fail;}
 pausebutton=b[3];
 zzbutton=zz9000_button();if(!child(bar,zzbutton))goto fail;
 for(i=0;i<5;++i)DoMethod(b[i],MUIM_Notify,MUIA_Pressed,FALSE,app,2,MUIM_Application_ReturnID,TM_EVENT+i);
 DoMethod(zzbutton,MUIM_Notify,MUIA_Pressed,FALSE,app,2,MUIM_Application_ReturnID,TM_EVENT+10);
 for(i=0;i<4;++i)DoMethod(sortbuttons[i],MUIM_Notify,MUIA_Pressed,FALSE,app,2,MUIM_Application_ReturnID,TM_EVENT+6+i);
 DoMethod(list,MUIM_Notify,MUIA_List_Active,MUIV_EveryTime,app,2,MUIM_Application_ReturnID,TM_EVENT+5);
 if(synthetic){SetAttrs(b[1],MUIA_Disabled,TRUE,TAG_DONE);SetAttrs(b[2],MUIA_Disabled,TRUE,TAG_DONE);}
 if(badservices){sprintf(statustext,"Ignored %d invalid/duplicate service definitions.",badservices);statusmsg(statustext);}
 monitor_page=root;root=NULL;
 arm_timer();return monitor_page;
fail:
 if(root)MUI_DisposeObject(root);
 termic_monitor_close();return NULL;
}

void termic_monitor_attach(APTR parent) {win=(Object *)parent;}
void termic_monitor_visible(int visible)
{
 if(!monitor_page || !win || page_visible==visible)return;
 page_visible=visible;
 if(!visible){sampling_stop();return;}
 if(!paused && !synthetic)sampling_start();
 refresh();details();
}

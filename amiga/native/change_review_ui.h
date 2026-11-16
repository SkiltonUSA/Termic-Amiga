enum { CR_CLOSE=5201,CR_SELECT,CR_ACCEPT,CR_REVERT,CR_REFRESH,CR_MODE };
static Object *change_window,*change_files,*change_note,*change_mode;
static const char *change_modes[]={"Unified diff","Before / after",NULL};
static struct ChangeReview change_set;
static char change_root[PATH_SIZE];
/* Source bytes are sanitized before adding our own TextEditor colour escapes. */
static char *change_colours(const char *text,int unified)
{
 size_t n=strlen(text);const char *p=text;char *out=malloc((n+1)*12),*w=out;
 if(!out)return NULL;
 while(*p) {
  const char *colour="E6EDF3";
  if(unified) {
   if(!strncmp(p,"--- ",4) || !strncmp(p,"+++ ",4) || !strncmp(p,"@@ ",3))colour="79C8FF";
   else if(*p=='+')colour="8FE3A4";
   else if(*p=='-')colour="FF7B8B";
  }
  memcpy(w,"\33P[",3);w+=3;memcpy(w,colour,6);w+=6;*w++=']';
  while(*p && *p!='\n') {*w++=(unsigned char)*p<32 && *p!='\t' && *p!='\r'?'?':*p;++p;}
  if(*p)*w++=*p++;
 }
 *w=0;return out;
}
static void change_show(void)
{
 int n=value(change_files,MUIA_List_Active),coarse=0,unified=value(change_mode,MUIA_Cycle_Active)==0;
 char *preview=unified?review_diff_preview(change_root,n,&coarse):review_preview(change_root,n);
 if(preview) {
  char *coloured=change_colours(preview,unified);
  if(coloured){set(change_text,MUIA_TextEditor_Contents,coloured);free(coloured);}
  else set(change_text,MUIA_TextEditor_Contents,"Not enough memory for review display.");
  free(preview);
  set(change_note,MUIA_Text_Contents,coarse?
   "Large change: replacement alignment. All lines retained; green + added, red - removed.":
   unified?"Green + added; red - removed; blue headers. Accept/Revert still applies to the whole file.":
   "Original and generated contents. Accept/Revert applies to the whole file.");
 } else if(n>=0) {
  set(change_text,MUIA_TextEditor_Contents,"");
  set(change_note,MUIA_Text_Contents,"Cannot read review snapshots. Nothing has been accepted or reverted.");
 }
}
static void change_refresh(void)
{
    int i;
    DoMethod(change_files,MUIM_List_Clear);
    if(!review_load(&change_set,change_root)) {
        set(change_note,MUIA_Text_Contents,"Review journal is unreadable. Restore .review.json.bak before continuing.");return;
    }
    for(i=0;i<change_set.count;i++)DoMethod(change_files,MUIM_List_InsertSingle,change_set.entries[i].name,MUIV_List_Insert_Bottom);
    set(change_text,MUIA_TextEditor_Contents,"");
    set(change_note,MUIA_Text_Contents,change_set.count?
        "Select a file. Revert restores its original contents; later edits are protected.":"No pending edits from agent file tools in this task.");
    set(change_files,MUIA_List_Active,change_set.count?0:MUIV_List_Active_Off);
}
static void change_open(void)
{
    if(!save_current())return;
    strcpy(change_root,current);set(change_window,MUIA_Window_Open,TRUE);change_refresh();
}
static int change_build_window(void)
{
    change_window=ResponsiveWindowObject,MUIA_Window_Title,"Termic: Review agent changes",
        MUIA_Window_ID,MAKE_ID('T','C','H','G'),MUIA_Window_Width,900,MUIA_Window_Height,500,
        WindowContents,VGroup,Child,HGroup,Child,label("View"),
        Child,change_mode=CycleObject,MUIA_Cycle_Entries,change_modes,End,End,
        Child,change_note=label("Select a file"),Child,HGroup,
        Child,VGroup,MUIA_Weight,25,Child,list(&change_files),End,
        Child,VGroup,MUIA_Weight,75,Child,editor(&change_text,1),End,End,
        Child,HGroup,Child,button("Accept file",CR_ACCEPT),Child,button("Revert file",CR_REVERT),
        Child,button("Refresh",CR_REFRESH),Child,HVSpace,Child,button("Close",CR_CLOSE),End,End,End;
    if(!change_window)return 0;
    set(change_text,MUIA_TextEditor_Pen,0xffe6edf3UL);
    DoMethod(app,OM_ADDMEMBER,change_window);
    notify(change_window,MUIA_Window_CloseRequest,TRUE,CR_CLOSE);
    notify(change_files,MUIA_List_Active,MUIV_EveryTime,CR_SELECT);
    notify(change_mode,MUIA_Cycle_Active,MUIV_EveryTime,CR_MODE);return 1;
}
static void change_event(ULONG id)
{
    int n;
    switch(id) {
    case CR_OPEN:change_open();break;
    case CR_CLOSE:set(change_window,MUIA_Window_Open,FALSE);break;
    case CR_REFRESH:change_refresh();break;
    case CR_SELECT:case CR_MODE:change_show();break;
    case CR_ACCEPT:case CR_REVERT:
        n=value(change_files,MUIA_List_Active);
        if(n<0 || n>=change_set.count)break;
        if(busy(change_root) || !save_current()) {
            set(change_note,MUIA_Text_Contents,"Finish or remove queued/running work before reviewing files.");break;
        }
        if(id==CR_REVERT) {
            char *args[1];args[0]=change_set.entries[n].name;
            if(!__termic_mui_request(MUIMasterBase,app,change_window,0,"Revert agent edit","_Revert|_Cancel",
                "Restore the original contents of %s?\nThe generated version is retained in a .bak file.",args))break;
        }
        if(!review_resolve(change_root,n,id==CR_REVERT)) {
            set(change_note,MUIA_Text_Contents,"File changed since the agent edit, or storage failed. Nothing will overwrite a later edit.");break;
        }
        change_refresh();
        if(!strcmp(change_root,current)) {
            char p[PATH_SIZE];refresh_files();
            if(*opened){join(p,current,opened);content(source,p,"");}
        }
        break;
    }
}

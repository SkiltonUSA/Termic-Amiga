/* Pixel-aligned native toolbar symbols. MUI owns focus, pressed state,
 * tooltips and the button frame; no external images or fonts are required. */
enum { AI_SAVE, AI_RUN, AI_STOP, AI_BUILD, AI_SETTINGS, AI_INFO, AI_QUIT,
       AI_NEW, AI_RENAME, AI_FOLDER, AI_SEARCH, AI_REFRESH, AI_DONE, AI_TRASH,
       AI_COPY, AI_UP, AI_DOWN, AI_PROMPT, AI_KEYS, AI_INSERT, AI_CANCEL, AI_AGENT,
       AI_KANBAN, AI_QUEUE, AI_FINDNEXT, AI_SMALL=0x100 };
static struct MUI_CustomClass *action_icon_class;
static int action_icon_kind(const char *text,int id)
{
    if(*text=='/')return -1; /* Command palettes keep their command names. */
    if(id>=2000 && id<2100 && strcmp(text,"Save") && strcmp(text,"Use") && strcmp(text,"Cancel"))return -1;
    if(id==CANCEL)return AI_STOP;
    if(!strcmp(text,"Save") || !strncmp(text,"Save ",5))return AI_SAVE;
    if(!strcmp(text,"Run") || !strcmp(text,"Execute"))return AI_RUN;
    if(!strcmp(text,"Start agent"))return AI_AGENT;
    if(!strcmp(text,"Build"))return AI_BUILD;
    if(!strcmp(text,"Settings"))return AI_SETTINGS;
    if(!strcmp(text,"About"))return AI_INFO;
    if(!strcmp(text,"Quit"))return AI_QUIT;
    if(!strcmp(text,"New") || !strcmp(text,"New project") || !strcmp(text,"New task") || !strcmp(text,"Create file"))return AI_NEW;
    if(!strcmp(text,"Rename"))return AI_RENAME;
    if(!strcmp(text,"Import source drawer..."))return AI_FOLDER;
    if(!strcmp(text,"Find next"))return AI_FINDNEXT;
    if(!strcmp(text,"Find") || !strcmp(text,"Find history"))return AI_SEARCH;
    if(!strcmp(text,"Refresh files") || !strncmp(text,"Reset ",6) || !strcmp(text,"Restore missing defaults"))return AI_REFRESH;
    if(!strcmp(text,"Use") || !strcmp(text,"Mark done"))return AI_DONE;
    if(!strcmp(text,"Remove"))return AI_TRASH;
    if(!strcmp(text,"Duplicate"))return AI_COPY;
    if(!strcmp(text,"Up"))return AI_UP;
    if(!strcmp(text,"Down"))return AI_DOWN;
    if(!strcmp(text,"Prompts") || !strcmp(text,"New conversation"))return AI_PROMPT;
    if(!strcmp(text,"Shortcuts"))return AI_KEYS;
    if(!strcmp(text,"Insert + Use"))return AI_INSERT;
    if(!strcmp(text,"Cancel") || !strcmp(text,"Close"))return AI_CANCEL;
    return -1;
}
static void ai_line(struct RastPort *rp,LONG x,LONG y,int a,int b,int c,int d)
{
    Move(rp,x+a,y+b);Draw(rp,x+c,y+d);
}
static void ai_box(struct RastPort *rp,LONG x,LONG y,int a,int b,int c,int d)
{
    ai_line(rp,x,y,a,b,c,b);ai_line(rp,x,y,c,b,c,d);
    ai_line(rp,x,y,c,d,a,d);ai_line(rp,x,y,a,d,a,b);
}
static ULONG action_icon_dispatch(__reg("a0") struct IClass *cl,
                                  __reg("a2") Object *obj,__reg("a1") Msg msg)
{
    ULONG result=DoSuperMethodA(cl,obj,msg);
    if(msg->MethodID==MUIM_AskMinMax) {
        struct MUI_MinMax *mm=((struct MUIP_AskMinMax *)msg)->MinMaxInfo;
        ULONG kind=0;LONG size;
        get(obj,MUIA_UserData,&kind);size=(kind&AI_SMALL)?16:20;
        /* Area accounts for frame/insets, but has no intrinsic contents. */
        mm->MinWidth+=size;mm->DefWidth+=size;mm->MaxWidth+=size;
        mm->MinHeight+=size;mm->DefHeight+=size;mm->MaxHeight+=size;
        return 0;
    }
    if(msg->MethodID==MUIM_Draw && (((struct MUIP_Draw *)msg)->flags&MADF_DRAWOBJECT)) {
        struct RastPort *rp=_rp(obj);
        ULONG kind=0,selected=0,pen=GetAPen(rp),mode=GetDrMd(rp);
        LONG x,y;int i;
        get(obj,MUIA_UserData,&kind);get(obj,MUIA_Selected,&selected);
        x=_mleft(obj)+(_mwidth(obj)-20)/2+!!selected;
        y=_mtop(obj)+(_mheight(obj)-20)/2+!!selected;
        SetAPen(rp,_pens(obj)[MPEN_TEXT]);SetDrMd(rp,JAM1);
        if(kind&AI_SMALL) {
            /* Hand-drawn 16px glyphs keep one-pixel strokes crisp. */
            x=_mleft(obj)+(_mwidth(obj)-16)/2+!!selected;
            y=_mtop(obj)+(_mheight(obj)-16)/2+!!selected;
            switch(kind&~AI_SMALL) {
            case AI_NEW:
                ai_box(rp,x,y,2,1,13,14);
                RectFill(rp,x+4,y+7,x+11,y+8);RectFill(rp,x+7,y+4,x+8,y+11);break;
            case AI_RENAME:
                ai_line(rp,x,y,3,12,11,4);ai_line(rp,x,y,4,13,12,5);
                ai_line(rp,x,y,11,3,13,5);ai_line(rp,x,y,2,14,5,13);
                ai_line(rp,x,y,2,15,13,15);break;
            case AI_FOLDER:
                ai_box(rp,x,y,1,5,14,13);ai_box(rp,x,y,2,2,7,5);
                ai_line(rp,x,y,2,7,13,7);break;
            case AI_KANBAN:
                ai_box(rp,x,y,0,1,15,14);
                ai_line(rp,x,y,5,1,5,14);ai_line(rp,x,y,10,1,10,14);
                for(i=0;i<3;i++)ai_line(rp,x,y,2+5*i,4,3+5*i,4);
                ai_line(rp,x,y,2,7,3,7);ai_line(rp,x,y,7,7,8,7);break;
            case AI_QUEUE:
                for(i=0;i<3;i++) {
                    ai_box(rp,x,y,1,2+5*i,3,4+5*i);
                    ai_line(rp,x,y,6,3+5*i,14,3+5*i);
                }break;
            case AI_DONE:
                ai_line(rp,x,y,2,7,6,12);ai_line(rp,x,y,6,12,14,2);
                ai_line(rp,x,y,2,8,6,13);ai_line(rp,x,y,6,13,14,3);break;
            }
            SetAPen(rp,pen);SetDrMd(rp,mode);return result;
        }
        switch(kind) {
        case AI_SAVE:
            ai_box(rp,x,y,3,2,17,18);ai_box(rp,x,y,6,2,14,8);
            ai_box(rp,x,y,6,12,14,18);ai_line(rp,x,y,12,3,12,6);break;
        case AI_RUN:case AI_AGENT:
            for(i=0;i<15;i++)ai_line(rp,x,y,4,3+i,17-(i<7?7-i:i-7),3+i);
            if(kind==AI_AGENT){ai_line(rp,x,y,2,2,2,18);ai_line(rp,x,y,18,2,18,18);}break;
        case AI_STOP:RectFill(rp,x+4,y+4,x+16,y+16);break;
        case AI_BUILD:
            ai_line(rp,x,y,4,17,14,7);ai_line(rp,x,y,5,18,15,8);
            ai_line(rp,x,y,8,3,16,11);ai_line(rp,x,y,9,2,17,10);
            ai_line(rp,x,y,7,4,10,1);ai_line(rp,x,y,15,12,18,9);break;
        case AI_SETTINGS:
            DrawEllipse(rp,x+10,y+10,6,6);DrawEllipse(rp,x+10,y+10,2,2);
            for(i=0;i<3;i++) {
                ai_line(rp,x,y,9+i,1,9+i,4);ai_line(rp,x,y,9+i,16,9+i,19);
                ai_line(rp,x,y,1,9+i,4,9+i);ai_line(rp,x,y,16,9+i,19,9+i);
            }break;
        case AI_INFO:
            DrawEllipse(rp,x+10,y+10,8,8);RectFill(rp,x+9,y+8,x+10,y+15);RectFill(rp,x+9,y+5,x+10,y+6);break;
        case AI_QUIT:
            ai_box(rp,x,y,3,3,11,18);ai_line(rp,x,y,8,10,19,10);
            ai_line(rp,x,y,15,6,19,10);ai_line(rp,x,y,15,14,19,10);break;
        case AI_NEW:
            ai_box(rp,x,y,3,2,16,18);RectFill(rp,x+6,y+9,x+13,y+10);
            RectFill(rp,x+9,y+6,x+10,y+13);break;
        case AI_RENAME:
            ai_line(rp,x,y,3,16,15,4);ai_line(rp,x,y,6,17,18,5);
            ai_line(rp,x,y,3,16,6,17);ai_line(rp,x,y,15,4,18,5);
            ai_line(rp,x,y,2,19,17,19);break;
        case AI_FOLDER:
            ai_box(rp,x,y,2,6,18,17);ai_box(rp,x,y,3,3,9,6);ai_line(rp,x,y,3,9,17,9);break;
        case AI_SEARCH:
            DrawEllipse(rp,x+8,y+8,5,5);ai_line(rp,x,y,12,12,18,18);ai_line(rp,x,y,13,12,19,18);break;
        case AI_FINDNEXT:
            /* Arrow distinguishes advancing a match from starting a search. */
            ai_line(rp,x,y,10,2,10,17);ai_line(rp,x,y,11,2,11,17);
            ai_line(rp,x,y,4,11,10,17);ai_line(rp,x,y,10,17,16,11);
            ai_line(rp,x,y,4,12,10,18);ai_line(rp,x,y,10,18,16,12);break;
        case AI_REFRESH:
            ai_line(rp,x,y,4,6,8,3);ai_line(rp,x,y,8,3,14,4);ai_line(rp,x,y,14,4,17,8);
            ai_line(rp,x,y,17,8,17,13);ai_line(rp,x,y,17,13,12,17);ai_line(rp,x,y,12,17,6,16);
            ai_line(rp,x,y,6,16,3,12);ai_line(rp,x,y,3,2,3,8);ai_line(rp,x,y,3,8,9,8);break;
        case AI_DONE:
            ai_line(rp,x,y,3,10,8,16);ai_line(rp,x,y,8,16,18,4);
            ai_line(rp,x,y,3,9,8,15);ai_line(rp,x,y,8,15,18,3);break;
        case AI_TRASH:
            ai_box(rp,x,y,5,6,15,18);ai_box(rp,x,y,7,2,13,4);ai_line(rp,x,y,3,5,17,5);
            ai_line(rp,x,y,8,8,8,15);ai_line(rp,x,y,12,8,12,15);break;
        case AI_COPY:ai_box(rp,x,y,2,2,13,14);ai_box(rp,x,y,6,6,17,18);break;
        case AI_UP:case AI_DOWN:
            ai_line(rp,x,y,10,3,10,17);
            ai_line(rp,x,y,4,kind==AI_UP?9:11,10,kind==AI_UP?3:17);
            ai_line(rp,x,y,16,kind==AI_UP?9:11,10,kind==AI_UP?3:17);break;
        case AI_PROMPT:
            ai_box(rp,x,y,2,3,18,14);ai_line(rp,x,y,5,14,5,18);ai_line(rp,x,y,5,18,9,14);
            ai_line(rp,x,y,5,7,15,7);ai_line(rp,x,y,5,10,12,10);break;
        case AI_KEYS:
            ai_box(rp,x,y,1,4,19,16);
            for(i=0;i<4;i++){RectFill(rp,x+3+4*i,y+6,x+4+4*i,y+7);RectFill(rp,x+3+4*i,y+10,x+4+4*i,y+11);}
            ai_line(rp,x,y,6,14,14,14);break;
        case AI_INSERT:
            ai_box(rp,x,y,10,2,18,18);ai_line(rp,x,y,1,10,14,10);
            ai_line(rp,x,y,9,5,14,10);ai_line(rp,x,y,9,15,14,10);break;
        case AI_CANCEL:
            ai_line(rp,x,y,4,4,16,16);ai_line(rp,x,y,5,4,17,16);
            ai_line(rp,x,y,4,16,16,4);ai_line(rp,x,y,5,16,17,4);break;
        }
        SetAPen(rp,pen);SetDrMd(rp,mode);
    }
    return result;
}
static Object *action_icon_sized(int kind,const char *help,int small)
{
    if(!action_icon_class)action_icon_class=__termic_mui_class(MUIMasterBase,NULL,MUIC_Area,NULL,0,(APTR)action_icon_dispatch);
    if(!action_icon_class)return NULL;
    return NewObject(action_icon_class->mcc_Class,NULL,ButtonFrame,
        MUIA_FixWidth,small?24:32,MUIA_FixHeight,small?24:30,
        MUIA_InnerLeft,small?2:4,MUIA_InnerRight,small?2:4,
        MUIA_InnerTop,small?2:3,MUIA_InnerBottom,small?2:3,MUIA_Background,MUII_ButtonBack,
        MUIA_InputMode,MUIV_InputMode_RelVerify,MUIA_CycleChain,1,
        MUIA_UserData,kind|(small?AI_SMALL:0),MUIA_ShortHelp,help,TAG_DONE);
}
static Object *action_icon(int kind,const char *help)
{
    return action_icon_sized(kind,help,FALSE);
}

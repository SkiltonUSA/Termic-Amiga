enum {ED_CHANGED=5300,ED_DIAGNOSTIC,ED_STATUS};
/* Public SetBlock changes presentation without replacing text/undo/cursor.
 * TextEditor 15.56 accepts ffRRGGBB here, as in the Pen attribute. */
static void editor_recolour(void)
{
    char *plain,*styled,*p,*end;ULONG dirty,metadata,color,x=0,y=0,sx,sy;
    if(!colour_pending)return;
    if(colour_delay>0){colour_delay--;return;}
    if(value(source,MUIA_TextEditor_AreaMarked))return;
    colour_pending=0;
    if(!source_loaded || value(source,MUIA_TextEditor_ReadOnly))return;
    plain=(char *)DoMethod(source,MUIM_TextEditor_ExportText);
    if(!plain)return;
    styled=syntax_colours(plain,syntax_language(opened));FreeVec(plain);
    if(!styled)return;
    dirty=value(source,MUIA_TextEditor_HasChanged);
    metadata=value(source,MUIA_TextEditor_MetaDataChanged);
    set(source,MUIA_TextEditor_Quiet,TRUE);
    DoMethod(source,MUIM_TextEditor_SetBlock,MUIV_TextEditor_SetBlock_Min,MUIV_TextEditor_SetBlock_Min,
             MUIV_TextEditor_SetBlock_Max,MUIV_TextEditor_SetBlock_Max,MUIF_TextEditor_SetBlock_Color,0xffe6edf3UL);
    p=styled;color=0xffe6edf3UL;
    while(*p) {
        if((unsigned char)*p==27) {
            color=0xff000000UL|strtoul(p+3,&end,16);p=end+1;continue;
        }
        sx=x;sy=y;
        while(*p && (unsigned char)*p!=27) {if(*p++=='\n'){x=0;y++;}else x++;}
        if(color!=0xffe6edf3UL && (x!=sx || y!=sy))
            DoMethod(source,MUIM_TextEditor_SetBlock,sx,sy,x,y,MUIF_TextEditor_SetBlock_Color,color);
    }
    SetAttrs(source,MUIA_TextEditor_Pen,0xffe6edf3UL,MUIA_TextEditor_HasChanged,dirty,
             MUIA_TextEditor_MetaDataChanged,metadata,MUIA_TextEditor_Quiet,FALSE,TAG_DONE);
    free(styled);
}

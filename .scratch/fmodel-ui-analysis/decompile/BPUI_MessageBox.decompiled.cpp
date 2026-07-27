class UBPUI_MessageBox_C : public UHTUI_MessageBox
{
public:
    class UWidgetTree* WidgetTree = "WidgetTree'/Game/UI/Blueprints/MessageBox/BPUI_MessageBox.BPUI_MessageBox_C:WidgetTree'";
    bool bClassRequiresNativeTick = true;
    TArray<class UWidgetAnimation*> Animations = {
        "WidgetAnimation'/Game/UI/Blueprints/MessageBox/BPUI_MessageBox.BPUI_MessageBox_C:OpenAnim_INST'",
        "WidgetAnimation'/Game/UI/Blueprints/MessageBox/BPUI_MessageBox.BPUI_MessageBox_C:CloseAnim_INST'"
    };
    TMap<FName, struct FGuid> CookedPropertyGuids = {
        { FName("CBActionBar_Close"), FGuid(0xC754ED62, 0x7503356D, 0xB7084EC5, 0x265598F3) }, 
        { FName("Image_1"), FGuid(0x17B01630, 0x7B673F61, 0x9E29E818, 0xAB4410B5) }, 
        { FName("Image_128"), FGuid(0xE4C51B9E, 0x582F3223, 0x8CAFFFD8, 0x12B1655C) }, 
        { FName("Image_205"), FGuid(0x886AFE0A, 0x453F1CAF, 0x6C756E9F, 0xF5C6C6C8) }
    };
    FSoftObjectPath OpenSound = FSoftObjectPath("/Game/WwiseAudio/Events/UI/UI/Play_Sfx_UI_Cmn_Tip.Play_Sfx_UI_Cmn_Tip");
    FSoftObjectPath CloseSound = FSoftObjectPath("/Game/WwiseAudio/Events/UI/UI/Play_Sfx_UI_Click_Back.Play_Sfx_UI_Click_Back");
    struct FGameplayTag LayerName = {
        "TagName": FName("Layer.MessageBox")
    };
    bool bForceShowMouseDuringSequence = true;
    enum InputConfig = EHTWidgetInputMode::Menu;
    bool bIsBackHandler = true;
    bool bIsBackActionDisplayedInActionBar = true;
    bool bAutoActivate = true;
    bool bSetVisibilityOnActivated = true;
    bool bSetVisibilityOnDeactivated = true;
    bool bHasScriptImplementedTick = false;
    bool bHasScriptImplementedPaint = false;
    class UImage* Image_205;
    class UImage* Image_128;
    class UImage* Image_1;
    class UCommonBoundActionBar* CBActionBar_Close;
};